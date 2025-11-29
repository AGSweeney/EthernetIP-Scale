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

#ifndef LLDP_TLV_DECODER_H_
#define LLDP_TLV_DECODER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file lldp_tlv_decoder.h
 * @brief TLV decoding functions for received LLDP frames
 * 
 * This module provides functions to decode TLVs from received LLDPDUs.
 */

/**
 * Decoded TLV header structure
 */
typedef struct {
    uint8_t type;      // TLV type (7 bits)
    uint16_t length;   // TLV length (9 bits, 0-511)
} lldp_tlv_header_t;

/**
 * Decoded Chassis ID TLV
 */
typedef struct {
    uint8_t subtype;   // Chassis ID subtype
    uint8_t id[256];   // Chassis ID value (max 255 bytes + null terminator)
    size_t id_len;     // Actual length of ID
} lldp_chassis_id_tlv_t;

/**
 * Decoded Port ID TLV
 */
typedef struct {
    uint8_t subtype;   // Port ID subtype
    uint8_t id[256];   // Port ID value (max 255 bytes + null terminator)
    size_t id_len;     // Actual length of ID
} lldp_port_id_tlv_t;

/**
 * Decode TLV header from buffer
 * @param buffer Input buffer containing TLV header
 * @param header Output structure for decoded header
 * @return Number of bytes consumed (2 on success, 0 on error)
 */
size_t lldp_decode_tlv_header(const uint8_t *buffer, lldp_tlv_header_t *header);

/**
 * Decode Chassis ID TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value
 * @param chassis_id Output structure for decoded Chassis ID
 * @return Number of bytes consumed, 0 on error
 */
size_t lldp_decode_chassis_id_tlv(const uint8_t *buffer, size_t length, lldp_chassis_id_tlv_t *chassis_id);

/**
 * Decode Port ID TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value
 * @param port_id Output structure for decoded Port ID
 * @return Number of bytes consumed, 0 on error
 */
size_t lldp_decode_port_id_tlv(const uint8_t *buffer, size_t length, lldp_port_id_tlv_t *port_id);

/**
 * Decode TTL TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value (must be 2)
 * @param ttl Output TTL value in seconds
 * @return Number of bytes consumed (2 on success, 0 on error)
 */
size_t lldp_decode_ttl_tlv(const uint8_t *buffer, size_t length, uint16_t *ttl);

/**
 * Decode System Name TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value
 * @param system_name Output buffer for system name (null-terminated)
 * @param max_len Maximum length of output buffer
 * @return Number of bytes consumed, 0 on error
 */
size_t lldp_decode_system_name_tlv(const uint8_t *buffer, size_t length, char *system_name, size_t max_len);

/**
 * Decode System Description TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value
 * @param system_description Output buffer for system description (null-terminated)
 * @param max_len Maximum length of output buffer
 * @return Number of bytes consumed, 0 on error
 */
size_t lldp_decode_system_description_tlv(const uint8_t *buffer, size_t length, char *system_description, size_t max_len);

/**
 * Decode System Capabilities TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value (must be 4)
 * @param system_capabilities Output system capabilities bitmask
 * @param enabled_capabilities Output enabled capabilities bitmask
 * @return Number of bytes consumed (4 on success, 0 on error)
 */
size_t lldp_decode_system_capabilities_tlv(const uint8_t *buffer, size_t length, 
                                           uint16_t *system_capabilities, uint16_t *enabled_capabilities);

/**
 * Decode Management Address TLV
 * @param buffer Input buffer pointing to TLV (after header)
 * @param length Length of TLV value
 * @param ip_address Output IPv4 address (4 bytes, network byte order), NULL if not IPv4 or unavailable
 * @return Number of bytes consumed, 0 on error
 */
size_t lldp_decode_management_address_tlv(const uint8_t *buffer, size_t length, uint8_t ip_address[4]);

#endif /* LLDP_TLV_DECODER_H_ */

