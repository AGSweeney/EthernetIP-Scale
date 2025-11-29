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

#ifndef LLDP_FRAME_BUILDER_H_
#define LLDP_FRAME_BUILDER_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @file lldp_frame_builder.h
 * @brief Ethernet frame building helper functions for LLDP
 * 
 * This module provides functions to construct complete Ethernet frames
 * for LLDP transmission.
 */

/**
 * LLDP multicast destination MAC address
 */
#define LLDP_MULTICAST_MAC_ADDR {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E}

/**
 * LLDP EtherType (IEEE 802.1AB)
 */
#define LLDP_ETHERTYPE 0x88CC

/**
 * Ethernet header size (destination MAC + source MAC + EtherType)
 */
#define ETH_HEADER_SIZE 14

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
                                  const uint8_t *lldpdu, size_t lldpdu_len);

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
                                         const uint8_t *lldpdu, size_t lldpdu_len);

/**
 * Build LLDP Data Unit (LLDPDU) with mandatory and optional TLVs
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @param mac_address MAC address for Chassis ID and Port ID (6 bytes)
 * @param ttl TTL value in seconds (default: 120)
 * @param system_name Optional system name (NULL to omit System Name TLV)
 * @param system_description Optional system description (NULL to omit System Description TLV)
 * @param ip_address Optional management IP address in network byte order (NULL or all zeros to omit Management Address TLV)
 * @return Length of LLDPDU on success, 0 on error
 */
size_t lldp_build_lldpdu(uint8_t *buffer, size_t buffer_len, 
                         const uint8_t mac_address[6], uint16_t ttl,
                         const char *system_name,
                         const char *system_description,
                         const uint8_t ip_address[4]);

#endif /* LLDP_FRAME_BUILDER_H_ */

