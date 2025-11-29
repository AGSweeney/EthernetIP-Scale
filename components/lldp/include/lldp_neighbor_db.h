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

#ifndef LLDP_NEIGHBOR_DB_H_
#define LLDP_NEIGHBOR_DB_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Forward declaration to avoid include dependency - CipInstanceNum is CipUint which is uint16_t
#ifndef CipInstanceNum
typedef uint16_t CipInstanceNum;
#endif

/**
 * @file lldp_neighbor_db.h
 * @brief Neighbor database for storing discovered LLDP neighbors
 */

/**
 * Maximum number of neighbors per interface
 */
#define LLDP_MAX_NEIGHBORS_PER_INTERFACE 16

/**
 * Maximum length of system name/description strings
 */
#define LLDP_MAX_STRING_LEN 256

/**
 * LLDP neighbor entry structure
 */
typedef struct lldp_neighbor_entry {
    // Mandatory TLVs
    uint8_t chassis_id_subtype;
    uint8_t chassis_id[256];  // Variable length, max 255 bytes
    size_t chassis_id_len;
    
    uint8_t port_id_subtype;
    uint8_t port_id[256];  // Variable length, max 255 bytes
    size_t port_id_len;
    
    uint16_t ttl_seconds;
    uint32_t expire_timestamp;  // Seconds since boot when entry expires
    
    // Optional TLVs
    char system_name[LLDP_MAX_STRING_LEN];
    bool has_system_name;
    
    char system_description[LLDP_MAX_STRING_LEN];
    bool has_system_description;
    
    uint16_t system_capabilities;
    uint16_t enabled_capabilities;
    bool has_system_capabilities;
    
    uint8_t management_ip[4];  // IPv4 address (network byte order)
    bool has_management_ip;
    
    // Metadata
    uint8_t source_mac[6];  // MAC address of the neighbor device
    uint32_t last_update;   // Timestamp when entry was last updated
    bool valid;             // Entry is valid (not expired)
    
    // CIP instance reference (for LLDP Data Table Object)
    void *cip_instance_ptr;  // Pointer to CipInstance (opaque to avoid circular dependency)
    CipInstanceNum instance_number;  // CIP instance number
    
    // Linked list
    struct lldp_neighbor_entry *next;
} lldp_neighbor_entry_t;

/**
 * Initialize neighbor database
 * @return 0 on success, -1 on failure
 */
int lldp_neighbor_db_init(void);

/**
 * Cleanup neighbor database (remove all entries)
 */
void lldp_neighbor_db_deinit(void);

/**
 * Add or update a neighbor entry
 * @param source_mac Source MAC address (6 bytes)
 * @param chassis_id_subtype Chassis ID subtype
 * @param chassis_id Chassis ID value
 * @param chassis_id_len Length of Chassis ID
 * @param port_id_subtype Port ID subtype
 * @param port_id Port ID value
 * @param port_id_len Length of Port ID
 * @param ttl_seconds TTL in seconds
 * @return Pointer to neighbor entry, NULL on error
 */
lldp_neighbor_entry_t *lldp_neighbor_db_add_or_update(
    const uint8_t source_mac[6],
    uint8_t chassis_id_subtype,
    const uint8_t *chassis_id,
    size_t chassis_id_len,
    uint8_t port_id_subtype,
    const uint8_t *port_id,
    size_t port_id_len,
    uint16_t ttl_seconds);

/**
 * Find neighbor by Chassis ID and Port ID
 * @param chassis_id_subtype Chassis ID subtype
 * @param chassis_id Chassis ID value
 * @param chassis_id_len Length of Chassis ID
 * @param port_id_subtype Port ID subtype
 * @param port_id Port ID value
 * @param port_id_len Length of Port ID
 * @return Pointer to neighbor entry, NULL if not found
 */
lldp_neighbor_entry_t *lldp_neighbor_db_find(
    uint8_t chassis_id_subtype,
    const uint8_t *chassis_id,
    size_t chassis_id_len,
    uint8_t port_id_subtype,
    const uint8_t *port_id,
    size_t port_id_len);

/**
 * Remove expired neighbors from database
 * @param current_time Current timestamp in seconds since boot
 * @return Number of neighbors removed
 */
int lldp_neighbor_db_cleanup_expired(uint32_t current_time);

/**
 * Get number of valid neighbors
 * @return Number of valid (non-expired) neighbors
 */
int lldp_neighbor_db_get_count(void);

/**
 * Update optional TLV fields in neighbor entry
 */
void lldp_neighbor_db_update_system_name(lldp_neighbor_entry_t *entry, const char *system_name);
void lldp_neighbor_db_update_system_description(lldp_neighbor_entry_t *entry, const char *system_description);
void lldp_neighbor_db_update_system_capabilities(lldp_neighbor_entry_t *entry, 
                                                 uint16_t system_capabilities, 
                                                 uint16_t enabled_capabilities);
void lldp_neighbor_db_update_management_ip(lldp_neighbor_entry_t *entry, const uint8_t ip[4]);

/**
 * Get current timestamp in seconds (since boot)
 * @return Timestamp in seconds
 */
uint32_t lldp_get_timestamp_seconds(void);

#endif /* LLDP_NEIGHBOR_DB_H_ */

