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

#ifndef LLDP_H_
#define LLDP_H_

/**
 * @file lldp.h
 * @brief LLDP Component Public Interface
 * 
 * This component provides LLDP (Link Layer Discovery Protocol) support for
 * EtherNet/IP adapter devices to satisfy ODVA compliance requirements.
 * 
 * The component includes:
 * - LLDP Management Object (CIP Class 0x109)
 * - LLDP Data Table Object (CIP Class 0x10A)
 * - ESP32 platform-specific raw Ethernet socket implementation
 * - Ethernet frame building utilities
 */

#include "ciplldpmanagement.h"
#include "ciplldpdatatable.h"
#include "lldp_raw_socket.h"
#include "lldp_frame_builder.h"
#include "lldp_transmission.h"
#include "lldp_reception.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LLDP component
 * 
 * This function initializes:
 * - LLDP Management CIP Object
 * - LLDP Data Table CIP Object
 * - ESP32 raw socket layer (if enabled)
 * 
 * @param netif Network interface to use for LLDP (for ESP32 raw socket)
 * @return EipStatus kEipStatusOk on success, error code otherwise
 */
EipStatus LldpComponentInit(void *netif);

/**
 * @brief Deinitialize LLDP component
 */
void LldpComponentDeinit(void);

#ifdef __cplusplus
}
#endif

#endif /* LLDP_H_ */

