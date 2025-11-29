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

#include "lldp_neighbor_db.h"
#include "ciplldpdatatable_instance.h"
#include "trace.h"
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ciplldpmanagement.h"  // For LldpManagementResetLastChange

static lldp_neighbor_entry_t *s_neighbor_list = NULL;
static SemaphoreHandle_t s_db_mutex = NULL;
static int s_neighbor_count = 0;

/**
 * Compare two chassis/port ID combinations
 */
static bool lldp_compare_ids(uint8_t subtype1, const uint8_t *id1, size_t len1,
                             uint8_t subtype2, const uint8_t *id2, size_t len2) {
    if (subtype1 != subtype2 || len1 != len2) {
        return false;
    }
    return (memcmp(id1, id2, len1) == 0);
}

/**
 * Initialize neighbor database
 */
int lldp_neighbor_db_init(void) {
    if (s_db_mutex != NULL) {
        return 0;  // Already initialized
    }
    
    s_db_mutex = xSemaphoreCreateMutex();
    if (s_db_mutex == NULL) {
        OPENER_TRACE_ERR("Failed to create neighbor DB mutex\n");
        return -1;
    }
    
    s_neighbor_list = NULL;
    s_neighbor_count = 0;
    
    return 0;
}

/**
 * Cleanup neighbor database
 */
void lldp_neighbor_db_deinit(void) {
    if (s_db_mutex == NULL) {
        return;
    }
    
    if (xSemaphoreTake(s_db_mutex, portMAX_DELAY) == pdTRUE) {
        lldp_neighbor_entry_t *entry = s_neighbor_list;
        while (entry != NULL) {
            lldp_neighbor_entry_t *next = entry->next;
            free(entry);
            entry = next;
        }
        s_neighbor_list = NULL;
        s_neighbor_count = 0;
        xSemaphoreGive(s_db_mutex);
    }
    
    vSemaphoreDelete(s_db_mutex);
    s_db_mutex = NULL;
}

/**
 * Add or update a neighbor entry
 */
lldp_neighbor_entry_t *lldp_neighbor_db_add_or_update(
    const uint8_t source_mac[6],
    uint8_t chassis_id_subtype,
    const uint8_t *chassis_id,
    size_t chassis_id_len,
    uint8_t port_id_subtype,
    const uint8_t *port_id,
    size_t port_id_len,
    uint16_t ttl_seconds) {
    
    if (s_db_mutex == NULL) {
        return NULL;
    }
    
    if (xSemaphoreTake(s_db_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }
    
    // Search for existing entry
    lldp_neighbor_entry_t *entry = s_neighbor_list;
    lldp_neighbor_entry_t *prev = NULL;
    
    while (entry != NULL) {
        if (lldp_compare_ids(chassis_id_subtype, chassis_id, chassis_id_len,
                            entry->chassis_id_subtype, entry->chassis_id, entry->chassis_id_len) &&
            lldp_compare_ids(port_id_subtype, port_id, port_id_len,
                            entry->port_id_subtype, entry->port_id, entry->port_id_len)) {
            // Found existing entry - update it
            break;
        }
        prev = entry;
        entry = entry->next;
    }
    
    uint32_t current_time = lldp_get_timestamp_seconds();
    bool is_new_entry = (entry == NULL);
    
    if (entry == NULL) {
        // Create new entry
        entry = (lldp_neighbor_entry_t *)calloc(1, sizeof(lldp_neighbor_entry_t));
        if (entry == NULL) {
            xSemaphoreGive(s_db_mutex);
            OPENER_TRACE_ERR("Failed to allocate neighbor entry\n");
            return NULL;
        }
        
        // Add to list
        if (prev == NULL) {
            entry->next = s_neighbor_list;
            s_neighbor_list = entry;
        } else {
            entry->next = prev->next;
            prev->next = entry;
        }
        
        s_neighbor_count++;
    }
    
    // Update mandatory fields
    memcpy(entry->source_mac, source_mac, 6);
    entry->chassis_id_subtype = chassis_id_subtype;
    if (chassis_id_len > 255) chassis_id_len = 255;
    memcpy(entry->chassis_id, chassis_id, chassis_id_len);
    entry->chassis_id_len = chassis_id_len;
    
    entry->port_id_subtype = port_id_subtype;
    if (port_id_len > 255) port_id_len = 255;
    memcpy(entry->port_id, port_id, port_id_len);
    entry->port_id_len = port_id_len;
    
    entry->ttl_seconds = ttl_seconds;
    entry->expire_timestamp = current_time + ttl_seconds;
    entry->last_update = current_time;
    entry->valid = true;
    
    xSemaphoreGive(s_db_mutex);
    
    // Create or update CIP object instance
    if (is_new_entry) {
        // New entry - create instance
        lldp_datatable_create_instance(entry, 0);  // Auto-assign instance number
        
        // Reset last_change counter (neighbor added to database)
        LldpManagementResetLastChange();
    } else {
        // Existing entry - update instance
        lldp_datatable_update_instance(entry);
        // Note: Updating existing neighbor doesn't reset last_change
        // Only additions/removals reset it
    }
    
    return entry;
}

/**
 * Find neighbor by Chassis ID and Port ID
 */
lldp_neighbor_entry_t *lldp_neighbor_db_find(
    uint8_t chassis_id_subtype,
    const uint8_t *chassis_id,
    size_t chassis_id_len,
    uint8_t port_id_subtype,
    const uint8_t *port_id,
    size_t port_id_len) {
    
    if (s_db_mutex == NULL) {
        return NULL;
    }
    
    if (xSemaphoreTake(s_db_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }
    
    lldp_neighbor_entry_t *entry = s_neighbor_list;
    while (entry != NULL) {
        if (entry->valid &&
            lldp_compare_ids(chassis_id_subtype, chassis_id, chassis_id_len,
                            entry->chassis_id_subtype, entry->chassis_id, entry->chassis_id_len) &&
            lldp_compare_ids(port_id_subtype, port_id, port_id_len,
                            entry->port_id_subtype, entry->port_id, entry->port_id_len)) {
            xSemaphoreGive(s_db_mutex);
            return entry;
        }
        entry = entry->next;
    }
    
    xSemaphoreGive(s_db_mutex);
    return NULL;
}

/**
 * Remove expired neighbors
 */
int lldp_neighbor_db_cleanup_expired(uint32_t current_time) {
    if (s_db_mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_db_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    
    int removed = 0;
    lldp_neighbor_entry_t *entry = s_neighbor_list;
    lldp_neighbor_entry_t *prev = NULL;
    
    while (entry != NULL) {
        if (!entry->valid || current_time >= entry->expire_timestamp) {
            // Remove this entry
            lldp_neighbor_entry_t *next = entry->next;
            
            // Debug: Log neighbor expiration
            OPENER_TRACE_WARN("LLDP: Neighbor expired - MAC: %02x:%02x:%02x:%02x:%02x:%02x, Instance: %u\n",
                              entry->source_mac[0], entry->source_mac[1], entry->source_mac[2],
                              entry->source_mac[3], entry->source_mac[4], entry->source_mac[5],
                              entry->instance_number);
            
            // Delete CIP object instance before freeing neighbor entry
            lldp_datatable_delete_instance(entry);
            
            // Reset last_change counter (neighbor removed from database)
            LldpManagementResetLastChange();
            
            if (prev == NULL) {
                s_neighbor_list = next;
            } else {
                prev->next = next;
            }
            
            free(entry);
            entry = next;
            s_neighbor_count--;
            removed++;
        } else {
            prev = entry;
            entry = entry->next;
        }
    }
    
    xSemaphoreGive(s_db_mutex);
    
    // Debug: Log cleanup summary
    if (removed > 0) {
        int remaining = lldp_neighbor_db_get_count();
        OPENER_TRACE_WARN("LLDP: Cleanup removed %d neighbor(s), %d remaining\n", removed, remaining);
    }
    
    return removed;
}

/**
 * Get number of valid neighbors
 */
int lldp_neighbor_db_get_count(void) {
    if (s_db_mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_db_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    
    int count = 0;
    lldp_neighbor_entry_t *entry = s_neighbor_list;
    while (entry != NULL) {
        if (entry->valid) {
            count++;
        }
        entry = entry->next;
    }
    
    xSemaphoreGive(s_db_mutex);
    return count;
}

/**
 * Update optional TLV fields
 * MODIFICATION: Added validity check to prevent use-after-free
 * Added by: Adam G. Sweeney <agsweeney@gmail.com>
 */
void lldp_neighbor_db_update_system_name(lldp_neighbor_entry_t *entry, const char *system_name) {
    if (entry == NULL || system_name == NULL) {
        return;
    }
    
    // Verify entry is still valid (not freed)
    if (!entry->valid) {
        return;
    }
    
    size_t len = strlen(system_name);
    if (len >= LLDP_MAX_STRING_LEN) {
        len = LLDP_MAX_STRING_LEN - 1;
    }
    
    memcpy(entry->system_name, system_name, len);
    entry->system_name[len] = '\0';
    entry->has_system_name = true;
}

void lldp_neighbor_db_update_system_description(lldp_neighbor_entry_t *entry, const char *system_description) {
    if (entry == NULL || system_description == NULL) {
        return;
    }
    
    // Verify entry is still valid (not freed)
    if (!entry->valid) {
        return;
    }
    
    size_t len = strlen(system_description);
    if (len >= LLDP_MAX_STRING_LEN) {
        len = LLDP_MAX_STRING_LEN - 1;
    }
    
    memcpy(entry->system_description, system_description, len);
    entry->system_description[len] = '\0';
    entry->has_system_description = true;
}

void lldp_neighbor_db_update_system_capabilities(lldp_neighbor_entry_t *entry, 
                                                 uint16_t system_capabilities, 
                                                 uint16_t enabled_capabilities) {
    if (entry == NULL) {
        return;
    }
    
    // Verify entry is still valid (not freed)
    if (!entry->valid) {
        return;
    }
    
    entry->system_capabilities = system_capabilities;
    entry->enabled_capabilities = enabled_capabilities;
    entry->has_system_capabilities = true;
}

void lldp_neighbor_db_update_management_ip(lldp_neighbor_entry_t *entry, const uint8_t ip[4]) {
    if (entry == NULL || ip == NULL) {
        return;
    }
    
    // Verify entry is still valid (not freed)
    if (!entry->valid) {
        return;
    }
    
    memcpy(entry->management_ip, ip, 4);
    entry->has_management_ip = true;
}

/**
 * Get current timestamp in seconds
 */
uint32_t lldp_get_timestamp_seconds(void) {
    return (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
}

