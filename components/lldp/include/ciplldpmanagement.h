/*******************************************************************************
 * Copyright (c) 2024, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#pragma once

/** @file ciplldpmanagement.h
 *  @brief Public interface of the LLDP Management Object
 *
 */

#include "ciptypes.h"
#include "typedefs.h"

/** @brief LLDP Management Object class code */
static const CipUint kCipLldpManagementClassCode = 0x109U;

typedef struct cip_lldp_management_lldp_enable {
  CipUint lldp_enable_array_length;
  CipByte lldp_enable_array[OPENER_ETHLINK_INSTANCE_CNT];
} CipLldpManagementLldpEnable;

typedef struct cip_lldp_management_object_values {
  CipLldpManagementLldpEnable lldp_enable;
  CipUint msg_tx_interval;
  CipUsint msg_tx_hold;
  CipWord lldp_datastore;
  CipUdint last_change;
} CipLldpManagementObjectValues;

// External reference to instance values (for NVS persistence and component initialization)
extern CipLldpManagementObjectValues g_lldp_management_object_instance_values;

EipStatus kCipLldpManagementInit(void);
void kCipLldpManagementDeinit(void);

/**
 * @brief Reset last_change counter (call when LLDP database changes)
 * 
 * This should be called when:
 * - Any LLDP Management Object attribute is changed (handled automatically)
 * - Neighbors are added/removed from the database
 */
void LldpManagementResetLastChange(void);