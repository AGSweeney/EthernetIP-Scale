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

#include "lldp.h"
#include "trace.h"
#include "lwip/netif.h"
#include "lldp_transmission.h"
#include "opener_user_conf.h"
#include "ciplldpmanagement.h"
#include "esp32/nvlldpmanagement.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LLDP component
 */
EipStatus LldpComponentInit(void *netif_ptr) {
    EipStatus status = kEipStatusOk;
    struct netif *netif = (struct netif *)netif_ptr;
    
    // Initialize LLDP Management CIP Object (Class 0x109)
    status = kCipLldpManagementInit();
    if (status != kEipStatusOk) {
        OPENER_TRACE_ERR("Failed to initialize LLDP Management Object\n");
        return status;
    }
    
    // Initialize LLDP Data Table CIP Object (Class 0x10A)
    status = kCipLldpDataTableInit();
    if (status != kEipStatusOk) {
        OPENER_TRACE_ERR("Failed to initialize LLDP Data Table Object\n");
        return status;
    }
    
    // Initialize ESP32 raw socket layer (if netif provided)
    if (netif != NULL) {
        OPENER_TRACE_WARN("LLDP: Initializing raw socket with netif=%p\n", netif);
        int socket_result = lldp_raw_socket_init(netif);
        if (socket_result != 0) {
            OPENER_TRACE_ERR("LLDP: CRITICAL - Failed to initialize raw socket (result=%d)\n", socket_result);
            OPENER_TRACE_ERR("LLDP: Transmission and reception will NOT work!\n");
        } else {
            OPENER_TRACE_WARN("LLDP: Raw socket initialized successfully, starting transmission/reception\n");
            // Initialize periodic transmission timer
            // Use saved interval if available, otherwise use default
            uint16_t tx_interval_ms = OPENER_LLDP_TX_INTERVAL_MS;
            if (g_lldp_management_object_instance_values.msg_tx_interval > 0) {
                uint32_t interval_ms_32 = (uint32_t)g_lldp_management_object_instance_values.msg_tx_interval * 1000U;
                if (interval_ms_32 <= 65535) {
                    tx_interval_ms = (uint16_t)interval_ms_32;
                }
            }
            if (lldp_transmission_init(tx_interval_ms) != 0) {
                OPENER_TRACE_ERR("LLDP: Failed to initialize transmission timer\n");
            } else {
                OPENER_TRACE_WARN("LLDP: Transmission timer initialized (interval=%u ms)\n", tx_interval_ms);
            }
            
            // Initialize LLDP reception (neighbor discovery)
            if (lldp_reception_init() != 0) {
                OPENER_TRACE_ERR("LLDP: Failed to initialize reception\n");
            } else {
                OPENER_TRACE_WARN("LLDP: Reception initialized successfully\n");
            }
        }
    } else {
        OPENER_TRACE_ERR("LLDP: CRITICAL - netif is NULL, cannot initialize raw socket!\n");
    }
    
    // LLDP component initialized successfully (logging removed for production)
    return kEipStatusOk;
}

/**
 * @brief Deinitialize LLDP component
 */
void LldpComponentDeinit(void) {
    // Stop reception
    lldp_reception_deinit();
    
    // Stop transmission timer
    lldp_transmission_deinit();
    
    // Deinitialize LLDP Management Object (cleanup timers)
    kCipLldpManagementDeinit();
    
    // Cleanup raw socket
    lldp_raw_socket_deinit();
    
    // LLDP component deinitialized (logging removed for production)
}

#ifdef __cplusplus
}
#endif

