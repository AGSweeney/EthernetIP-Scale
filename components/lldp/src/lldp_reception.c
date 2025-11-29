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

#include "lldp_reception.h"
#include "lldp_raw_socket.h"
#include "lldp_tlv_decoder.h"
#include "lldp_neighbor_db.h"
#include "lldp_frame_builder.h"
#include "ciplldpdatatable_instance.h"
#include "trace.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ETH_HEADER_SIZE 14
#define LLDP_ETHERTYPE 0x88CC

static bool s_reception_enabled = false;

/**
 * Process a single received Ethernet frame
 */
int lldp_reception_process_frame(const uint8_t *frame, size_t frame_len) {
    if (frame == NULL || frame_len < ETH_HEADER_SIZE) {
        OPENER_TRACE_WARN("LLDP: Frame too short or NULL (len=%d)\n", (int)frame_len);
        return -1;
    }
    
    // Validate Ethernet header
    uint16_t ethertype = ((uint16_t)frame[12] << 8) | frame[13];
    if (ethertype != LLDP_ETHERTYPE) {
        OPENER_TRACE_WARN("LLDP: Wrong EtherType 0x%04x (expected 0x%04x)\n", ethertype, LLDP_ETHERTYPE);
        return -1;  // Not an LLDP frame
    }
    
    // Valid LLDP frame - no logging needed for normal operation
    
    // Extract source MAC address
    uint8_t source_mac[6];
    memcpy(source_mac, frame + 6, 6);
    
    // Skip Ethernet header to get LLDPDU
    const uint8_t *lldpdu = frame + ETH_HEADER_SIZE;
    size_t lldpdu_len = frame_len - ETH_HEADER_SIZE;
    
    if (lldpdu_len < 9) {  // Minimum: Chassis ID (9) + Port ID (9) + TTL (4) + End (2) = 24, but check for at least first TLV
        OPENER_TRACE_WARN("LLDP: LLDPDU too short (%d bytes, minimum 9)\n", (int)lldpdu_len);
        return -1;  // Frame too short
    }
    
    // Starting decode - no logging needed for normal operation
    
    // Decode TLVs
    size_t offset = 0;
    lldp_tlv_header_t header;
    lldp_chassis_id_tlv_t chassis_id = {0};
    lldp_port_id_tlv_t port_id = {0};
    uint16_t ttl = 0;
    bool got_chassis_id = false;
    bool got_port_id = false;
    bool got_ttl = false;
    
    char system_name[LLDP_MAX_STRING_LEN] = {0};
    char system_description[LLDP_MAX_STRING_LEN] = {0};
    uint16_t system_capabilities = 0;
    uint16_t enabled_capabilities = 0;
    uint8_t management_ip[4] = {0};
    bool has_management_ip = false;
    
    // Decode TLVs until End TLV
    while (offset < lldpdu_len) {
        if (offset + 2 > lldpdu_len) {
            return -1;  // Not enough data for TLV header
        }
        
        size_t consumed = lldp_decode_tlv_header(lldpdu + offset, &header);
        if (consumed == 0) {
            return -1;  // Invalid TLV header
        }
        offset += consumed;
        
        if (header.type == 0) {
            // End of LLDPDU TLV
            break;
        }
        
        if (offset + header.length > lldpdu_len) {
            return -1;  // TLV extends beyond frame
        }
        
        // Decode based on TLV type
        switch (header.type) {
            case 1: {  // Chassis ID
                consumed = lldp_decode_chassis_id_tlv(lldpdu + offset, header.length, &chassis_id);
                if (consumed > 0) {
                    got_chassis_id = true;
                }
                break;
            }
            case 2: {  // Port ID
                consumed = lldp_decode_port_id_tlv(lldpdu + offset, header.length, &port_id);
                if (consumed > 0) {
                    got_port_id = true;
                }
                break;
            }
            case 3: {  // TTL
                consumed = lldp_decode_ttl_tlv(lldpdu + offset, header.length, &ttl);
                if (consumed > 0) {
                    got_ttl = true;
                }
                break;
            }
            case 5: {  // System Name
                lldp_decode_system_name_tlv(lldpdu + offset, header.length, system_name, sizeof(system_name));
                break;
            }
            case 6: {  // System Description
                lldp_decode_system_description_tlv(lldpdu + offset, header.length, system_description, sizeof(system_description));
                break;
            }
            case 7: {  // System Capabilities
                lldp_decode_system_capabilities_tlv(lldpdu + offset, header.length, &system_capabilities, &enabled_capabilities);
                break;
            }
            case 8: {  // Management Address
                if (lldp_decode_management_address_tlv(lldpdu + offset, header.length, management_ip) > 0) {
                    has_management_ip = true;
                }
                break;
            }
            default:
                // Unknown TLV - skip it
                break;
        }
        
        offset += header.length;
    }
    
    // Validate mandatory TLVs were received
    if (!got_chassis_id || !got_port_id || !got_ttl) {
        OPENER_TRACE_WARN("LLDP: Invalid frame - missing mandatory TLVs (ChassisID=%d, PortID=%d, TTL=%d)\n",
                          got_chassis_id, got_port_id, got_ttl);
        return -1;  // Missing mandatory TLVs
    }
    
    // Add or update neighbor in database
    lldp_neighbor_entry_t *entry = lldp_neighbor_db_add_or_update(
        source_mac,
        chassis_id.subtype,
        chassis_id.id,
        chassis_id.id_len,
        port_id.subtype,
        port_id.id,
        port_id.id_len,
        ttl);
    
    if (entry == NULL) {
        return -1;
    }
    
    // Update optional TLVs
    if (system_name[0] != '\0') {
        lldp_neighbor_db_update_system_name(entry, system_name);
    }
    if (system_description[0] != '\0') {
        lldp_neighbor_db_update_system_description(entry, system_description);
    }
    if (system_capabilities != 0 || enabled_capabilities != 0) {
        lldp_neighbor_db_update_system_capabilities(entry, system_capabilities, enabled_capabilities);
    }
    if (has_management_ip) {
        lldp_neighbor_db_update_management_ip(entry, management_ip);
    }
    
    // Update CIP object instance with new data
    lldp_datatable_update_instance(entry);
    
    // Neighbor discovered/updated - no logging (removed for production)
    
    return 0;
}

/**
 * Reception task to poll for incoming LLDP frames
 */
static void lldp_reception_task(void *pvParameters) {
    (void)pvParameters;
    
    uint8_t frame_buffer[1518];  // Maximum Ethernet frame size
    int frame_count = 0;
    int poll_count = 0;
    int no_socket_count = 0;
    int error_count = 0;
    
    OPENER_TRACE_WARN("LLDP: Reception task started (task=%p)\n", xTaskGetCurrentTaskHandle());
    
    while (s_reception_enabled) {
        if (!lldp_raw_socket_is_initialized()) {
            no_socket_count++;
            if ((no_socket_count % 100) == 0) {  // Log every 100 polls (~1 second)
                OPENER_TRACE_WARN("LLDP: Waiting for raw socket initialization... (count=%d)\n", no_socket_count);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            poll_count++;
            continue;
        }
        
        // Socket is initialized - log once
        if (no_socket_count > 0) {
            OPENER_TRACE_WARN("LLDP: Raw socket now initialized, starting to poll for frames\n");
            no_socket_count = 0;
        }
        
        int received = lldp_raw_socket_recv(frame_buffer, sizeof(frame_buffer));
        if (received > 0) {
            frame_count++;
            error_count = 0;  // Reset error count on success
            // Frame received - no logging needed for normal operation
            int result = lldp_reception_process_frame(frame_buffer, (size_t)received);
            if (result != 0 && (frame_count <= 3 || (frame_count % 10) == 0)) {
                OPENER_TRACE_WARN("LLDP: Frame #%d processing failed (returned %d)\n", frame_count, result);
            }
        } else if (received < 0) {
            // Error occurred
            error_count++;
            if ((error_count % 100) == 0) {  // Log every 100 errors
                OPENER_TRACE_WARN("LLDP: Reception error (count=%d), continuing...\n", error_count);
            }
            vTaskDelay(pdMS_TO_TICKS(100));  // Delay before retry
        } else {
            // No frame available (received == 0) - this is normal
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        poll_count++;
    }
    
    OPENER_TRACE_WARN("LLDP: Reception task exiting (total_polls=%d, frames_received=%d)\n", poll_count, frame_count);
    vTaskDelete(NULL);
}

static TaskHandle_t s_reception_task_handle = NULL;
static TaskHandle_t s_cleanup_task_handle = NULL;

/**
 * Cleanup task to remove expired neighbors
 */
static void lldp_cleanup_task(void *pvParameters) {
    (void)pvParameters;
    
    while (s_reception_enabled) {
        uint32_t current_time = lldp_get_timestamp_seconds();
        (void)lldp_neighbor_db_cleanup_expired(current_time);  // Ignore return value
        
        // Run cleanup every 10 seconds
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    
    vTaskDelete(NULL);
}

/**
 * Initialize LLDP reception
 */
int lldp_reception_init(void) {
    if (s_reception_enabled) {
        return 0;  // Already initialized
    }
    
    // Initialize neighbor database
    if (lldp_neighbor_db_init() != 0) {
        OPENER_TRACE_ERR("Failed to initialize neighbor database\n");
        return -1;
    }
    
    OPENER_TRACE_WARN("LLDP: Reception initialized - listening for neighbor frames\n");
    
    s_reception_enabled = true;
    
    // Create reception task with larger stack to handle printf/logging
    // MODIFICATION: Increased stack from 4096 to 8192 to prevent stack overflow
    // Added by: Adam G. Sweeney <agsweeney@gmail.com>
    // printf/vfprintf uses significant stack space, especially with format strings
    if (xTaskCreate(lldp_reception_task, "LLDP_RX", 8192, NULL, 5, &s_reception_task_handle) != pdPASS) {
        OPENER_TRACE_ERR("Failed to create LLDP reception task\n");
        s_reception_enabled = false;
        lldp_neighbor_db_deinit();
        return -1;
    }
    
    // Create cleanup task
    if (xTaskCreate(lldp_cleanup_task, "LLDP_CLEAN", 2048, NULL, 3, &s_cleanup_task_handle) != pdPASS) {
        OPENER_TRACE_ERR("Failed to create LLDP cleanup task\n");
        s_reception_enabled = false;
        if (s_reception_task_handle != NULL) {
            vTaskDelete(s_reception_task_handle);
            s_reception_task_handle = NULL;
        }
        lldp_neighbor_db_deinit();
        return -1;
    }
    
    return 0;
}

/**
 * Deinitialize LLDP reception
 */
void lldp_reception_deinit(void) {
    s_reception_enabled = false;
    
    // Wait for tasks to finish and delete them properly
    // MODIFICATION: Fixed race condition - properly wait for and delete tasks
    // Added by: Adam G. Sweeney <agsweeney@gmail.com>
    if (s_reception_task_handle != NULL) {
        // Wait up to 1 second for task to exit its loop
        int timeout = 10;
        TaskHandle_t handle = s_reception_task_handle;
        while (timeout-- > 0 && eTaskGetState(handle) != eDeleted && eTaskGetState(handle) != eInvalid) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        // Force delete if still running
        if (eTaskGetState(handle) != eDeleted && eTaskGetState(handle) != eInvalid) {
            vTaskDelete(handle);
        }
        s_reception_task_handle = NULL;
    }
    
    if (s_cleanup_task_handle != NULL) {
        // Wait up to 1 second for task to exit its loop
        int timeout = 10;
        TaskHandle_t handle = s_cleanup_task_handle;
        while (timeout-- > 0 && eTaskGetState(handle) != eDeleted && eTaskGetState(handle) != eInvalid) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        // Force delete if still running
        if (eTaskGetState(handle) != eDeleted && eTaskGetState(handle) != eInvalid) {
            vTaskDelete(handle);
        }
        s_cleanup_task_handle = NULL;
    }
    
    // Cleanup neighbor database
    lldp_neighbor_db_deinit();
}

