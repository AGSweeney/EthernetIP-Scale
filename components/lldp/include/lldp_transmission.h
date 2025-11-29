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

#ifndef LLDP_TRANSMISSION_H_
#define LLDP_TRANSMISSION_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @file lldp_transmission.h
 * @brief LLDP periodic frame transmission management
 * 
 * This module handles periodic transmission of LLDP frames using FreeRTOS timers.
 */

/**
 * Initialize LLDP periodic transmission timer
 * @param interval_ms Transmission interval in milliseconds
 * @return 0 on success, -1 on failure
 */
int lldp_transmission_init(uint16_t interval_ms);

/**
 * Deinitialize LLDP periodic transmission timer
 */
void lldp_transmission_deinit(void);

/**
 * Enable or disable LLDP transmission
 * @param enabled true to enable, false to disable
 */
void lldp_transmission_set_enabled(bool enabled);

/**
 * Get LLDP transmission enabled state
 * @return true if enabled, false if disabled
 */
bool lldp_transmission_is_enabled(void);

/**
 * Update transmission interval
 * @param interval_ms New interval in milliseconds
 * @return 0 on success, -1 on failure
 */
int lldp_transmission_set_interval(uint16_t interval_ms);

/**
 * Get current transmission interval
 * @return Interval in milliseconds
 */
uint16_t lldp_transmission_get_interval(void);

/**
 * Manually trigger LLDP frame transmission (for testing)
 * @return 0 on success, -1 on failure
 */
int lldp_transmission_send_now(void);

#endif /* LLDP_TRANSMISSION_H_ */

