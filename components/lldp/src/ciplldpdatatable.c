/*******************************************************************************
 * Copyright (c) 2024, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "ciplldpdatatable.h"
#include "opener_api.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "trace.h"

EipStatus kCipLldpDataTableInit() {
  CipClass *lldp_data_table_class = NULL;

  if ( ( lldp_data_table_class = CreateCipClass(
           kCipLldpDataTableCode, 7, /* # class attributes */
           7,                    /* # highest class attribute number */
           3,                    /* # class services */
           11,                    /* # instance attributes */
           11,                    /* # highest instance attribute number */
           3,                    /* # instance services */
           0,                    /* # instances - 0 to supress creation */
           "LLDP Data Table", 1, /* # class revision */
           NULL                  /* # function pointer for initialization */
           ) ) == 0 ) {
    OPENER_TRACE_ERR("Failed to create LLDP Data Table class\n");
    return kEipStatusError;
  }
  
  // Register services for instances (GetAttributeSingle, GetAttributeAll, etc.)
  // Services are already registered by CreateCipClass for standard services
  // but we should verify they exist
  
  // Register instance services (required for attribute access)
  InsertService(lldp_data_table_class,
                kGetAttributeSingle,
                &GetAttributeSingle,
                "GetAttributeSingle");
  InsertService(lldp_data_table_class,
                kGetAttributeAll,
                &GetAttributeAll,
                "GetAttributeAll");
  
  OPENER_TRACE_WARN("LLDP Data Table class 0x%x initialized (instances will be created dynamically)\n", 
                    (unsigned)kCipLldpDataTableCode);
  
  return kEipStatusOk;
}