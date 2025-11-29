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

#ifndef LLDP_RECEPTION_H_
#define LLDP_RECEPTION_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @file lldp_reception.h
 * @brief LLDP frame reception and processing
 * 
 * This module handles receiving and processing LLDP frames from neighbors.
 */

/**
 * Initialize LLDP reception
 * @return 0 on success, -1 on failure
 */
int lldp_reception_init(void);

/**
 * Deinitialize LLDP reception
 */
void lldp_reception_deinit(void);

/**
 * Process a single received Ethernet frame
 * @param frame Raw Ethernet frame buffer
 * @param frame_len Length of frame in bytes
 * @return 0 on success, -1 on error
 */
int lldp_reception_process_frame(const uint8_t *frame, size_t frame_len);

#endif /* LLDP_RECEPTION_H_ */

