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

#ifndef CIPLLDPDATATABLE_INSTANCE_H_
#define CIPLLDPDATATABLE_INSTANCE_H_

#include "ciptypes.h"
#include "typedefs.h"
#include "lldp_neighbor_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ciplldpdatatable_instance.h
 * @brief LLDP Data Table Object instance management
 * 
 * Functions to create, update, and delete CIP object instances for discovered neighbors.
 */

/**
 * Create a CIP object instance for a neighbor entry
 * @param neighbor_entry Neighbor database entry
 * @param instance_number Instance number to assign (0 = auto-assign next available)
 * @return 0 on success, -1 on failure
 */
int lldp_datatable_create_instance(lldp_neighbor_entry_t *neighbor_entry, CipInstanceNum instance_number);

/**
 * Update a CIP object instance with current neighbor data
 * @param neighbor_entry Neighbor database entry
 * @return 0 on success, -1 on failure
 */
int lldp_datatable_update_instance(lldp_neighbor_entry_t *neighbor_entry);

/**
 * Delete a CIP object instance for a neighbor entry
 * @param neighbor_entry Neighbor database entry
 * @return 0 on success, -1 on failure
 */
int lldp_datatable_delete_instance(lldp_neighbor_entry_t *neighbor_entry);

/**
 * Get next available instance number
 * @return Next instance number, or 0 on error
 */
CipInstanceNum lldp_datatable_get_next_instance_number(void);

#ifdef __cplusplus
}
#endif

#endif /* CIPLLDPDATATABLE_INSTANCE_H_ */

