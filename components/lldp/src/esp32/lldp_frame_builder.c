/*
 * Copyright (c) 2025, Adam G. Sweeney <agsweeney@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lldp_frame_builder.h"
#include <string.h>
#include <stdint.h>

// LLDP TLV Types (IEEE 802.1AB)
#define LLDP_TLV_CHASSIS_ID        1
#define LLDP_TLV_PORT_ID           2
#define LLDP_TLV_TTL               3
#define LLDP_TLV_PORT_DESCRIPTION  4
#define LLDP_TLV_SYSTEM_NAME       5
#define LLDP_TLV_SYSTEM_DESCRIPTION 6
#define LLDP_TLV_SYSTEM_CAPABILITIES 7
#define LLDP_TLV_MANAGEMENT_ADDRESS 8
#define LLDP_TLV_END_OF_LLDPDU     0

// LLDP Chassis ID Subtypes
#define LLDP_CHASSIS_ID_SUBTYPE_MAC_ADDRESS 4

// LLDP Port ID Subtypes
#define LLDP_PORT_ID_SUBTYPE_MAC_ADDRESS 3
#define LLDP_PORT_ID_SUBTYPE_INTERFACE_NAME 5

// Default TTL: 120 seconds (standard)
#define LLDP_DEFAULT_TTL 120

/**
 * Encode TLV header (Type and Length)
 * @param buffer Output buffer
 * @param type TLV type (7 bits)
 * @param length TLV length (9 bits, 0-511)
 * @return Number of bytes written (2)
 */
static size_t lldp_encode_tlv_header(uint8_t *buffer, uint8_t type, uint16_t length) {
    // TLV format: [Type (7 bits) | Length (9 bits)] = 2 bytes
    // Type in upper 7 bits, Length in lower 9 bits
    buffer[0] = (type << 1) | ((length >> 8) & 0x01);
    buffer[1] = length & 0xFF;
    return 2;
}

/**
 * Build Chassis ID TLV (mandatory)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param mac_address MAC address (6 bytes)
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_chassis_id_tlv(uint8_t *buffer, size_t buffer_len, const uint8_t mac_address[6]) {
    if (buffer_len < 9) {  // 2 bytes header + 1 byte subtype + 6 bytes MAC
        return 0;
    }
    
    size_t offset = 0;
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_CHASSIS_ID, 7);  // 1 subtype + 6 MAC
    buffer[offset++] = LLDP_CHASSIS_ID_SUBTYPE_MAC_ADDRESS;
    memcpy(buffer + offset, mac_address, 6);
    offset += 6;
    
    return offset;
}

/**
 * Build Port ID TLV (mandatory)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param mac_address MAC address (6 bytes)
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_port_id_tlv(uint8_t *buffer, size_t buffer_len, const uint8_t mac_address[6]) {
    if (buffer_len < 9) {  // 2 bytes header + 1 byte subtype + 6 bytes MAC
        return 0;
    }
    
    size_t offset = 0;
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_PORT_ID, 7);  // 1 subtype + 6 MAC
    buffer[offset++] = LLDP_PORT_ID_SUBTYPE_MAC_ADDRESS;
    memcpy(buffer + offset, mac_address, 6);
    offset += 6;
    
    return offset;
}

/**
 * Build TTL TLV (mandatory)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param ttl TTL value in seconds
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_ttl_tlv(uint8_t *buffer, size_t buffer_len, uint16_t ttl) {
    if (buffer_len < 4) {  // 2 bytes header + 2 bytes TTL
        return 0;
    }
    
    size_t offset = 0;
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_TTL, 2);  // 2 bytes TTL
    buffer[offset++] = (ttl >> 8) & 0xFF;  // Big-endian
    buffer[offset++] = ttl & 0xFF;
    
    return offset;
}

/**
 * Build End of LLDPDU TLV (mandatory)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_end_tlv(uint8_t *buffer, size_t buffer_len) {
    if (buffer_len < 2) {  // 2 bytes header only
        return 0;
    }
    
    return lldp_encode_tlv_header(buffer, LLDP_TLV_END_OF_LLDPDU, 0);
}

/**
 * Build System Name TLV (optional, Type 5)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param system_name System name string (null-terminated)
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_system_name_tlv(uint8_t *buffer, size_t buffer_len, const char *system_name) {
    if (system_name == NULL || *system_name == '\0') {
        return 0;  // Don't include empty system name
    }
    
    size_t name_len = strlen(system_name);
    if (name_len == 0 || name_len > 255) {
        return 0;  // LLDP string TLVs max length is 255
    }
    
    size_t required = 2 + name_len;  // 2 bytes header + string
    if (buffer_len < required) {
        return 0;
    }
    
    size_t offset = 0;
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_SYSTEM_NAME, name_len);
    memcpy(buffer + offset, system_name, name_len);
    offset += name_len;
    
    return offset;
}

/**
 * Build System Description TLV (optional, Type 6)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param system_description System description string (null-terminated)
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_system_description_tlv(uint8_t *buffer, size_t buffer_len, const char *system_description) {
    if (system_description == NULL || *system_description == '\0') {
        return 0;  // Don't include empty description
    }
    
    size_t desc_len = strlen(system_description);
    if (desc_len == 0 || desc_len > 255) {
        return 0;  // LLDP string TLVs max length is 255
    }
    
    size_t required = 2 + desc_len;  // 2 bytes header + string
    if (buffer_len < required) {
        return 0;
    }
    
    size_t offset = 0;
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_SYSTEM_DESCRIPTION, desc_len);
    memcpy(buffer + offset, system_description, desc_len);
    offset += desc_len;
    
    return offset;
}

/**
 * Build System Capabilities TLV (optional, Type 7)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param system_capabilities System capabilities bitmask (2 bytes)
 * @param enabled_capabilities Enabled capabilities bitmask (2 bytes)
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_system_capabilities_tlv(uint8_t *buffer, size_t buffer_len,
                                                  uint16_t system_capabilities,
                                                  uint16_t enabled_capabilities) {
    // System Capabilities TLV format (IEEE 802.1AB):
    // TLV Header (2 bytes): Type 7, Length 4
    // System Capabilities (2 bytes): Bitmask of supported capabilities
    // Enabled Capabilities (2 bytes): Bitmask of currently enabled capabilities
    const size_t required = 2 + 4;  // 2 bytes header + 4 bytes data
    
    if (buffer_len < required) {
        return 0;
    }
    
    size_t offset = 0;
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_SYSTEM_CAPABILITIES, 4);
    
    // System Capabilities (2 bytes, big-endian)
    buffer[offset++] = (system_capabilities >> 8) & 0xFF;
    buffer[offset++] = system_capabilities & 0xFF;
    
    // Enabled Capabilities (2 bytes, big-endian)
    buffer[offset++] = (enabled_capabilities >> 8) & 0xFF;
    buffer[offset++] = enabled_capabilities & 0xFF;
    
    return offset;
}

/**
 * Build Management Address TLV (optional, Type 8)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param ip_address IPv4 address (network byte order, 4 bytes)
 * @return Number of bytes written, 0 on error
 */
static size_t lldp_build_management_address_tlv(uint8_t *buffer, size_t buffer_len, const uint8_t ip_address[4]) {
    if (ip_address == NULL || (ip_address[0] == 0 && ip_address[1] == 0 && ip_address[2] == 0 && ip_address[3] == 0)) {
        return 0;  // Don't include zero IP address
    }
    
    // Management Address TLV format (IEEE 802.1AB):
    // TLV Header (2 bytes)
    // Management Address String:
    //   - Management Address Length (1 byte): Length of Address Subtype + Address ONLY
    //     For IPv4: 1 (subtype) + 4 (address) = 5 bytes
    //     This field does NOT include Interface Subtype, Interface Number, or OID fields!
    //   - Address Subtype (1 byte): 1 = IPv4, 2 = IPv6
    //   - Address (4 bytes for IPv4, 16 for IPv6): IP address
    //   - Interface Subtype (1 byte): 1 = unknown, 2 = ifIndex, 3 = system port number
    //   - Interface Number (variable): Interface identifier
    //      * For subtype 1 (unknown): 0 bytes
    //      * For subtype 2 (ifIndex): 4 bytes
    //      * For subtype 3 (system port): 4 bytes
    //   - OID String Length (1 byte): Length of OID string (0 if not present)
    //   - OID String (variable): OID string (optional, omitted if length is 0)
    //
    // Note: The Management Address Length field only counts Address Subtype + Address.
    // Interface and OID fields are separate and follow after the address.
    
    // For IPv4 with unknown interface (subtype 1), including OID String Length (0):
    // Management Address Length = 1 (addr subtype) + 4 (IPv4 addr) = 5 bytes (ONLY these!)
    // Additional fields: Interface Subtype (1) + Interface Number (4 bytes, even for unknown!) + OID String Length (1) = 6 bytes
    // Total TLV value length: 1 (Management Address Length byte) + 5 (address) + 6 (interface/OID) = 12 bytes
    // Note: Even for Interface Subtype = 1 (unknown), we must include 4 bytes for Interface Number (set to 0)
    const uint8_t addr_subtype = 1;  // IPv4
    const uint8_t addr_len = 4;      // IPv4 address length
    const uint8_t if_subtype = 1;    // Unknown interface
    const uint8_t if_number_len = 4; // Interface Number is ALWAYS 4 bytes, even for unknown subtype
    const uint8_t oid_string_len = 0; // No OID string
    
    // Management Address Length: ONLY Address Subtype + Address (NOT interface/OID fields)
    const uint8_t mgmt_addr_len = addr_subtype + addr_len;  // 1 + 4 = 5 bytes
    // Interface/OID fields length: Interface Subtype + Interface Number (always 4 bytes) + OID Length
    const uint8_t interface_oid_len = 1 + if_number_len + 1;  // Interface Subtype (1) + Interface Number (4) + OID Length (1) = 6 bytes
    // Total TLV value length: Management Address Length byte + address fields + interface/OID fields
    const size_t tlv_value_len = 1 + mgmt_addr_len + interface_oid_len;  // 1 + 5 + 6 = 12 bytes
    const size_t total_tlv_len = 2 + tlv_value_len;  // 2 bytes (TLV header) + value = 14 bytes
    
    if (buffer_len < total_tlv_len) {
        return 0;
    }
    
    size_t offset = 0;
    
    // TLV Header: Type 8, Length = tlv_value_len (8)
    offset += lldp_encode_tlv_header(buffer + offset, LLDP_TLV_MANAGEMENT_ADDRESS, tlv_value_len);
    
    // Management Address Length (1 byte): ONLY Address Subtype + Address = 5 bytes
    buffer[offset++] = mgmt_addr_len;
    
    // Address Subtype: 1 = IPv4
    buffer[offset++] = addr_subtype;
    
    // IPv4 address (4 bytes, network byte order)
    memcpy(buffer + offset, ip_address, 4);
    offset += 4;
    
    // Interface Subtype: 1 = unknown
    buffer[offset++] = if_subtype;
    
    // Interface Number: ALWAYS 4 bytes, even for unknown interface (subtype 1)
    // For unknown interface, set Interface Number to 0 (4 bytes)
    memset(buffer + offset, 0, 4);
    offset += 4;
    
    // OID String Length: 0 (no OID string)
    buffer[offset++] = oid_string_len;
    
    return offset;
}

/**
 * Build complete Ethernet frame for LLDP
 * @param buffer Output buffer (must be at least ETH_HEADER_SIZE + lldpdu_len bytes)
 * @param buffer_len Total buffer size
 * @param src_mac Source MAC address (6 bytes)
 * @param lldpdu LLDP Data Unit payload
 * @param lldpdu_len Length of LLDPDU
 * @return Total frame length (ETH_HEADER_SIZE + lldpdu_len) on success, 0 on error
 */
size_t lldp_build_ethernet_frame(uint8_t *buffer, size_t buffer_len,
                                  const uint8_t src_mac[6],
                                  const uint8_t *lldpdu, size_t lldpdu_len) {
    // Use standard LLDP multicast MAC
    const uint8_t dst_mac[6] = LLDP_MULTICAST_MAC_ADDR;
    return lldp_build_ethernet_frame_custom(buffer, buffer_len,
                                             dst_mac, src_mac,
                                             lldpdu, lldpdu_len);
}

/**
 * Build complete Ethernet frame for LLDP with custom destination MAC
 * @param buffer Output buffer (must be at least ETH_HEADER_SIZE + lldpdu_len bytes)
 * @param buffer_len Total buffer size
 * @param dst_mac Destination MAC address (6 bytes)
 * @param src_mac Source MAC address (6 bytes)
 * @param lldpdu LLDP Data Unit payload
 * @param lldpdu_len Length of LLDPDU
 * @return Total frame length (ETH_HEADER_SIZE + lldpdu_len) on success, 0 on error
 */
size_t lldp_build_ethernet_frame_custom(uint8_t *buffer, size_t buffer_len,
                                         const uint8_t dst_mac[6],
                                         const uint8_t src_mac[6],
                                         const uint8_t *lldpdu, size_t lldpdu_len) {
    if (buffer == NULL || dst_mac == NULL || src_mac == NULL) {
        return 0;
    }
    
    if (lldpdu == NULL && lldpdu_len > 0) {
        return 0;
    }
    
    size_t required_len = ETH_HEADER_SIZE + lldpdu_len;
    if (buffer_len < required_len) {
        return 0;  // Buffer too small
    }
    
    // Destination MAC (6 bytes)
    memcpy(buffer, dst_mac, 6);
    
    // Source MAC (6 bytes)
    memcpy(buffer + 6, src_mac, 6);
    
    // EtherType (2 bytes, network byte order / big-endian)
    buffer[12] = (LLDP_ETHERTYPE >> 8) & 0xFF;
    buffer[13] = LLDP_ETHERTYPE & 0xFF;
    
    // LLDPDU payload
    if (lldpdu != NULL && lldpdu_len > 0) {
        // Copy exactly lldpdu_len bytes - no more, no less
        memcpy(buffer + ETH_HEADER_SIZE, lldpdu, lldpdu_len);
    }
    
    return required_len;  // Ethernet header + payload
}

/**
 * Build LLDP Data Unit (LLDPDU) with mandatory and optional TLVs
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param mac_address MAC address for Chassis ID and Port ID (6 bytes)
 * @param ttl TTL value in seconds (default: 120)
 * @param system_name Optional system name (NULL to omit)
 * @param system_description Optional system description (NULL to omit)
 * @param ip_address Optional management IP address (NULL or all zeros to omit)
 * @return Length of LLDPDU on success, 0 on error
 */
size_t lldp_build_lldpdu(uint8_t *buffer, size_t buffer_len, 
                         const uint8_t mac_address[6], uint16_t ttl,
                         const char *system_name,
                         const char *system_description,
                         const uint8_t ip_address[4]) {
    if (buffer == NULL || mac_address == NULL) {
        return 0;
    }
    
    size_t offset = 0;
    size_t remaining = buffer_len;
    
    // Chassis ID TLV (mandatory)
    size_t written = lldp_build_chassis_id_tlv(buffer + offset, remaining, mac_address);
    if (written == 0) {
        return 0;
    }
    offset += written;
    remaining -= written;
    
    // Port ID TLV (mandatory)
    written = lldp_build_port_id_tlv(buffer + offset, remaining, mac_address);
    if (written == 0) {
        return 0;
    }
    offset += written;
    remaining -= written;
    
    // TTL TLV (mandatory)
    written = lldp_build_ttl_tlv(buffer + offset, remaining, ttl);
    if (written == 0) {
        return 0;
    }
    offset += written;
    remaining -= written;
    
    // Optional TLVs (inserted before End TLV)
    
    // System Name TLV (optional, Type 5)
    if (system_name != NULL && *system_name != '\0') {
        written = lldp_build_system_name_tlv(buffer + offset, remaining, system_name);
        if (written > 0) {
            offset += written;
            remaining -= written;
        }
        // Continue even if this optional TLV fails (non-fatal)
    }
    
    // System Description TLV (optional, Type 6)
    if (system_description != NULL && *system_description != '\0') {
        written = lldp_build_system_description_tlv(buffer + offset, remaining, system_description);
        if (written > 0) {
            offset += written;
            remaining -= written;
        }
        // Continue even if this optional TLV fails (non-fatal)
    }
    
    // System Capabilities TLV (optional, Type 7)
    // For EtherNet/IP adapter: Station (end device) - bit 7 = 0x80
    // Station capability indicates this is an end device, not a bridge or router
    const uint16_t system_caps = 0x0080;  // Bit 7 = Station (end device)
    const uint16_t enabled_caps = 0x0080; // Station capability is enabled
    written = lldp_build_system_capabilities_tlv(buffer + offset, remaining, system_caps, enabled_caps);
    if (written > 0) {
        offset += written;
        remaining -= written;
    }
    // Continue even if this optional TLV fails (non-fatal)
    
    // Management Address TLV (optional, Type 8)
    if (ip_address != NULL && 
        (ip_address[0] != 0 || ip_address[1] != 0 || ip_address[2] != 0 || ip_address[3] != 0)) {
        written = lldp_build_management_address_tlv(buffer + offset, remaining, ip_address);
        if (written > 0) {
            offset += written;
            remaining -= written;
        }
        // Continue even if this optional TLV fails (non-fatal)
    }
    
    // End of LLDPDU TLV (mandatory - must be last)
    written = lldp_build_end_tlv(buffer + offset, remaining);
    if (written == 0) {
        return 0;
    }
    offset += written;
    
    return offset;
}

