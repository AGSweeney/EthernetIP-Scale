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

#include "lldp_tlv_decoder.h"
#include <string.h>

/**
 * Decode TLV header from buffer
 */
size_t lldp_decode_tlv_header(const uint8_t *buffer, lldp_tlv_header_t *header) {
    if (buffer == NULL || header == NULL) {
        return 0;
    }
    
    // TLV format: [Type (7 bits) | Length (9 bits)] = 2 bytes
    // Type in upper 7 bits, Length in lower 9 bits
    uint8_t byte0 = buffer[0];
    uint8_t byte1 = buffer[1];
    
    header->type = (byte0 >> 1) & 0x7F;  // Upper 7 bits
    header->length = ((byte0 & 0x01) << 8) | byte1;  // Lower 9 bits
    
    return 2;
}

/**
 * Decode Chassis ID TLV
 */
size_t lldp_decode_chassis_id_tlv(const uint8_t *buffer, size_t length, lldp_chassis_id_tlv_t *chassis_id) {
    if (buffer == NULL || chassis_id == NULL || length == 0) {
        return 0;
    }
    
    if (length > 255) {
        return 0;  // Invalid length
    }
    
    chassis_id->subtype = buffer[0];
    chassis_id->id_len = length - 1;  // Subtract subtype byte
    
    if (chassis_id->id_len > 255) {
        chassis_id->id_len = 255;  // Clamp to max
    }
    
    // Copy ID value
    if (chassis_id->id_len > 0) {
        memcpy(chassis_id->id, buffer + 1, chassis_id->id_len);
    }
    chassis_id->id[chassis_id->id_len] = '\0';  // Null terminate
    
    return length;
}

/**
 * Decode Port ID TLV
 */
size_t lldp_decode_port_id_tlv(const uint8_t *buffer, size_t length, lldp_port_id_tlv_t *port_id) {
    if (buffer == NULL || port_id == NULL || length == 0) {
        return 0;
    }
    
    if (length > 255) {
        return 0;  // Invalid length
    }
    
    port_id->subtype = buffer[0];
    port_id->id_len = length - 1;  // Subtract subtype byte
    
    if (port_id->id_len > 255) {
        port_id->id_len = 255;  // Clamp to max
    }
    
    // Copy ID value
    if (port_id->id_len > 0) {
        memcpy(port_id->id, buffer + 1, port_id->id_len);
    }
    port_id->id[port_id->id_len] = '\0';  // Null terminate
    
    return length;
}

/**
 * Decode TTL TLV
 */
size_t lldp_decode_ttl_tlv(const uint8_t *buffer, size_t length, uint16_t *ttl) {
    if (buffer == NULL || ttl == NULL || length != 2) {
        return 0;
    }
    
    // TTL is 2 bytes in big-endian format
    *ttl = ((uint16_t)buffer[0] << 8) | buffer[1];
    
    return 2;
}

/**
 * Decode System Name TLV
 */
size_t lldp_decode_system_name_tlv(const uint8_t *buffer, size_t length, char *system_name, size_t max_len) {
    if (buffer == NULL || system_name == NULL || length == 0 || max_len == 0) {
        return 0;
    }
    
    if (length > 255) {
        return 0;  // Invalid length
    }
    
    size_t copy_len = length;
    if (copy_len >= max_len) {
        copy_len = max_len - 1;  // Reserve space for null terminator
    }
    
    memcpy(system_name, buffer, copy_len);
    system_name[copy_len] = '\0';
    
    return length;
}

/**
 * Decode System Description TLV
 */
size_t lldp_decode_system_description_tlv(const uint8_t *buffer, size_t length, char *system_description, size_t max_len) {
    if (buffer == NULL || system_description == NULL || length == 0 || max_len == 0) {
        return 0;
    }
    
    if (length > 255) {
        return 0;  // Invalid length
    }
    
    size_t copy_len = length;
    if (copy_len >= max_len) {
        copy_len = max_len - 1;  // Reserve space for null terminator
    }
    
    memcpy(system_description, buffer, copy_len);
    system_description[copy_len] = '\0';
    
    return length;
}

/**
 * Decode System Capabilities TLV
 */
size_t lldp_decode_system_capabilities_tlv(const uint8_t *buffer, size_t length, 
                                           uint16_t *system_capabilities, uint16_t *enabled_capabilities) {
    if (buffer == NULL || system_capabilities == NULL || enabled_capabilities == NULL || length != 4) {
        return 0;
    }
    
    // System Capabilities: 2 bytes (big-endian)
    *system_capabilities = ((uint16_t)buffer[0] << 8) | buffer[1];
    
    // Enabled Capabilities: 2 bytes (big-endian)
    *enabled_capabilities = ((uint16_t)buffer[2] << 8) | buffer[3];
    
    return 4;
}

/**
 * Decode Management Address TLV
 */
size_t lldp_decode_management_address_tlv(const uint8_t *buffer, size_t length, uint8_t ip_address[4]) {
    if (buffer == NULL || ip_address == NULL || length < 5) {
        return 0;  // Minimum: 1 byte length + 1 byte subtype + 4 bytes IPv4
    }
    
    // Management Address TLV format:
    // - Management Address Length (1 byte): Length of address + interface info + OID
    // - Address Subtype (1 byte): 1 = IPv4, 2 = IPv6
    // - Address (variable): IP address
    
    uint8_t mgmt_addr_len = buffer[0];
    if (mgmt_addr_len < 5 || mgmt_addr_len > length) {
        return 0;  // Invalid length
    }
    
    uint8_t addr_subtype = buffer[1];
    
    // Only decode IPv4 addresses for now
    if (addr_subtype != 1) {
        // Not IPv4, skip this TLV
        return length;
    }
    
    // IPv4 address is 4 bytes
    if (mgmt_addr_len < 5) {
        return 0;  // Not enough bytes for IPv4
    }
    
    // Extract IPv4 address (network byte order)
    memcpy(ip_address, buffer + 2, 4);
    
    // Return full TLV length (we decoded what we need, skip interface/OID fields)
    return length;
}

