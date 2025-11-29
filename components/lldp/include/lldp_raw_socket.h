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

#ifndef LLDP_RAW_SOCKET_H_
#define LLDP_RAW_SOCKET_H_

#include <stdint.h>
#include <stddef.h>
#include "lwip/netif.h"

/**
 * @file lldp_raw_socket.h
 * @brief Raw Ethernet socket interface for LLDP frame transmission/reception
 * 
 * This module provides raw Ethernet frame access using ESP-NETIF L2 TAP
 * for sending and receiving LLDP frames.
 */

/**
 * Initialize raw Ethernet socket for LLDP
 * @param netif Network interface to attach to
 * @return 0 on success, -1 on failure
 */
int lldp_raw_socket_init(struct netif *netif);

/**
 * Send raw Ethernet frame
 * @param frame Buffer containing complete Ethernet frame (MAC + EtherType + payload)
 * @param len Length of frame (must be >= 14 bytes for MAC header)
 * @return Number of bytes sent on success, -1 on failure
 */
int lldp_raw_socket_send(const uint8_t *frame, size_t len);

/**
 * Receive raw Ethernet frame (non-blocking)
 * @param frame Buffer to store received frame
 * @param max_len Maximum buffer size
 * @return Number of bytes received, 0 if no frame available, -1 on error
 */
int lldp_raw_socket_recv(uint8_t *frame, size_t max_len);

/**
 * Cleanup raw Ethernet socket
 */
void lldp_raw_socket_deinit(void);

/**
 * Get MAC address from network interface
 * @param mac Output buffer for MAC address (6 bytes)
 */
void lldp_get_mac_address(uint8_t mac[6]);

/**
 * Check if raw socket is initialized
 * @return true if initialized, false otherwise
 */
bool lldp_raw_socket_is_initialized(void);

/**
 * Get system hostname from network interface
 * @param hostname Buffer to store hostname (must be at least max_len bytes)
 * @param max_len Maximum length of hostname buffer
 * @return Length of hostname (0 if not available)
 */
size_t lldp_get_hostname(char *hostname, size_t max_len);

/**
 * Get system description (defaults to "ESP32 EtherNet/IP Adapter")
 * @param description Buffer to store description (must be at least max_len bytes)
 * @param max_len Maximum length of description buffer
 * @return Length of description string
 */
size_t lldp_get_system_description(char *description, size_t max_len);

/**
 * Get IP address from network interface (network byte order)
 * @param ip_address Output buffer for IP address (4 bytes)
 * @return 0 on success, -1 if IP address not available
 */
int lldp_get_ip_address(uint8_t ip_address[4]);

#endif /* LLDP_RAW_SOCKET_H_ */

