/*******************************************************************************
 * Copyright (c) 2024, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * MODIFICATIONS:
 * - Added mutex protection for thread-safe access to last_change counter
 * - Added error handling for timer creation failures
 * - Modified by: Adam G. Sweeney <agsweeney@gmail.com>
 *
 ******************************************************************************/

#include "ciplldpmanagement.h"
#include "opener_api.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "trace.h"
#include "lldp_transmission.h"
#include "opener_user_conf.h"
#include "endianconv.h"
#include "cpf.h"
#include <stdint.h>
#include "esp32/nvlldpmanagement.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

CipLldpManagementObjectValues g_lldp_management_object_instance_values;
static TimerHandle_t s_last_change_timer = NULL;
static uint32_t s_last_change_timestamp = 0;  // Timestamp (in seconds) when last_change was reset to 0
static SemaphoreHandle_t s_last_change_mutex = NULL;

void CipLldpManagementEncodeLldpEnable(const void *const data,
                                       ENIPMessage *const outgoing_message) {
  const CipLldpManagementLldpEnable *lldp_enable = (const CipLldpManagementLldpEnable *)data;
  
  if (lldp_enable == NULL || outgoing_message == NULL) {
    return;
  }
  
  // Encode array length (UINT16)
  AddIntToMessage((EipUint16)lldp_enable->lldp_enable_array_length, outgoing_message);
  
  // Encode array elements (BYTE array) - write directly to buffer
  for (CipUint i = 0; i < lldp_enable->lldp_enable_array_length && i < OPENER_ETHLINK_INSTANCE_CNT; i++) {
    *(outgoing_message->current_message_position) = lldp_enable->lldp_enable_array[i];
    MoveMessageNOctets(1, outgoing_message);
  }
}

int CipLldpManagementDecodeLldpEnable(void *const data_to,
                                       CipMessageRouterRequest *const
                                       message_router_request,
                                       CipMessageRouterResponse *const
                                       message_router_response) {
  CipLldpManagementLldpEnable *lldp_enable = (CipLldpManagementLldpEnable *)data_to;
  
  if (lldp_enable == NULL || message_router_request == NULL || message_router_response == NULL) {
    if (message_router_response != NULL) {
      message_router_response->general_status = kCipErrorInvalidParameter;
    }
    return -1;
  }
  
  const CipOctet **cip_message = (const CipOctet **)&(message_router_request->data);
  
  // Decode array length (UINT16)
  if (message_router_request->request_data_size < 2) {
    message_router_response->general_status = kCipErrorNotEnoughData;
    return -1;
  }
  
  lldp_enable->lldp_enable_array_length = GetUintFromMessage(cip_message);
  
  // Validate array length
  if (lldp_enable->lldp_enable_array_length > OPENER_ETHLINK_INSTANCE_CNT) {
    message_router_response->general_status = kCipErrorInvalidAttributeValue;
    return -1;
  }
  
  // Decode array elements (BYTE array)
  if (message_router_request->request_data_size < (2 + lldp_enable->lldp_enable_array_length)) {
    message_router_response->general_status = kCipErrorNotEnoughData;
    return -1;
  }
  
  for (CipUint i = 0; i < lldp_enable->lldp_enable_array_length; i++) {
    lldp_enable->lldp_enable_array[i] = GetByteFromMessage(cip_message);
  }
  
  // Update LLDP transmission enabled state based on first element (for instance 1)
  if (lldp_enable->lldp_enable_array_length > 0) {
    bool enabled = (lldp_enable->lldp_enable_array[0] != 0);
    lldp_transmission_set_enabled(enabled);
  }
  
  // Reset last_change counter (configuration changed)
  LldpManagementResetLastChange();
  
  // Save updated configuration to NVS
  (void)NvLldpManagementStore(&g_lldp_management_object_instance_values);
  
  message_router_response->general_status = kCipErrorSuccess;
  return (int)(2 + lldp_enable->lldp_enable_array_length);  // Return bytes decoded
}

/**
 * Reset last_change counter to 0 (called when any change occurs)
 * Public function so it can be called when neighbors are added/removed
 */
void LldpManagementResetLastChange(void) {
  if (s_last_change_mutex != NULL) {
    if (xSemaphoreTake(s_last_change_mutex, portMAX_DELAY) == pdTRUE) {
      g_lldp_management_object_instance_values.last_change = 0;
      s_last_change_timestamp = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
      xSemaphoreGive(s_last_change_mutex);
    }
  } else {
    g_lldp_management_object_instance_values.last_change = 0;
    s_last_change_timestamp = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
  }
}

/**
 * Update last_change counter (called periodically to increment it)
 */
static void LldpManagementUpdateLastChange(void) {
  if (s_last_change_mutex != NULL) {
    if (xSemaphoreTake(s_last_change_mutex, portMAX_DELAY) == pdTRUE) {
      uint32_t current_time = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
      uint32_t elapsed = current_time - s_last_change_timestamp;
      // Clamp to UDINT max (4294967295 seconds = ~136 years)
      if (elapsed > 4294967295UL) {
        g_lldp_management_object_instance_values.last_change = 4294967295UL;
      } else {
        g_lldp_management_object_instance_values.last_change = (CipUdint)elapsed;
      }
      xSemaphoreGive(s_last_change_mutex);
    }
  } else {
    // Fallback if mutex not initialized (shouldn't happen, but be safe)
    uint32_t current_time = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    uint32_t elapsed = current_time - s_last_change_timestamp;
    if (elapsed > 4294967295UL) {
      g_lldp_management_object_instance_values.last_change = 4294967295UL;
    } else {
      g_lldp_management_object_instance_values.last_change = (CipUdint)elapsed;
    }
  }
}

/**
 * Timer callback to update last_change counter every second
 */
static void LldpManagementLastChangeTimerCallback(TimerHandle_t xTimer) {
  (void)xTimer;
  LldpManagementUpdateLastChange();
}

/**
 * PostSetCallback for LLDP Management Object
 * Updates transmission timer interval when msg_tx_interval (Attr 2) is changed
 */
static EipStatus CipLldpManagementPostSetCallback(CipInstance *const instance,
                                                  CipAttributeStruct *const attribute,
                                                  CipByte service) {
  (void)service;
  
  if (attribute == NULL || instance == NULL) {
    return kEipStatusError;
  }
  
  // Don't update last_change when last_change itself is being set (attribute 5)
  if (attribute->attribute_number != 5) {
    // Reset last_change counter on any configuration change
    LldpManagementResetLastChange();
  }
  
  // Update transmission timer if msg_tx_interval (Attribute 2) was changed
  if (attribute->attribute_number == 2) {
    CipUint interval_seconds = g_lldp_management_object_instance_values.msg_tx_interval;
    // Convert seconds to milliseconds
    // CipUint is uint16_t, max value 65535 seconds = 65535000 ms (doesn't fit in uint16_t)
    // Clamp to max uint16_t milliseconds (65535 ms = 65.535 seconds)
    uint32_t interval_ms_32 = (uint32_t)interval_seconds * 1000U;
    uint16_t interval_ms;
    if (interval_ms_32 > UINT16_MAX) {
      interval_ms = UINT16_MAX;
    } else {
      interval_ms = (uint16_t)interval_ms_32;
    }
    lldp_transmission_set_interval(interval_ms);
    
    // Save updated configuration to NVS
    (void)NvLldpManagementStore(&g_lldp_management_object_instance_values);
  }
  // Also save when other persistent attributes change (attributes 1, 3, 4)
  else if (attribute->attribute_number == 1 || 
           attribute->attribute_number == 3 || 
           attribute->attribute_number == 4) {
    // Save updated configuration to NVS
    (void)NvLldpManagementStore(&g_lldp_management_object_instance_values);
  }
  
  return kEipStatusOk;
}

EipStatus kCipLldpManagementInit(void) {
  CipClass *lldp_management_class = NULL;

  if ( ( lldp_management_class = CreateCipClass(
           kCipLldpManagementClassCode, 7, /* # class attributes */
           7,                    /* # highest class attribute number */
           2,                    /* # class services */
           5,                    /* # instance attributes */
           5,                    /* # highest instance attribute number */
           3,                    /* # instance services */
           1,                    /* # instances */
           "LLDP Management", 1, /* # class revision */
           NULL                  /* # function pointer for initialization */
           ) ) == 0 ) {
    return kEipStatusError;
  }
  
  // Register PostSetCallback to handle attribute changes
  lldp_management_class->PostSetCallback = CipLldpManagementPostSetCallback;

  CipInstance *instance = GetCipInstance(lldp_management_class, 1);   /* bind attributes to the instance #1 that was created above */

  // Initialize default values
  g_lldp_management_object_instance_values.lldp_enable.lldp_enable_array_length = OPENER_ETHLINK_INSTANCE_CNT;
  memset(g_lldp_management_object_instance_values.lldp_enable.lldp_enable_array, 1, OPENER_ETHLINK_INSTANCE_CNT);  // Default: enabled
  g_lldp_management_object_instance_values.msg_tx_interval = OPENER_LLDP_TX_INTERVAL_MS / 1000;  // Convert ms to seconds
  g_lldp_management_object_instance_values.msg_tx_hold = 4;  // Standard: 4
  g_lldp_management_object_instance_values.lldp_datastore = 0;
  g_lldp_management_object_instance_values.last_change = 0;
  s_last_change_timestamp = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);  // Initialize timestamp

  // Try to load saved configuration from NVS
  EipStatus nv_status = NvLldpManagementLoad(&g_lldp_management_object_instance_values);
  if (nv_status == kEipStatusOk) {
    // Successfully loaded saved configuration
    // Apply loaded enable state to transmission
    if (g_lldp_management_object_instance_values.lldp_enable.lldp_enable_array_length > 0) {
      bool enabled = (g_lldp_management_object_instance_values.lldp_enable.lldp_enable_array[0] != 0);
      lldp_transmission_set_enabled(enabled);
    }
    // Apply loaded interval to transmission timer (will be applied when transmission is initialized)
  } else {
    // No saved configuration, using defaults
    // Defaults already set above
  }

  InsertAttribute(instance, 1, kCipAny, CipLldpManagementEncodeLldpEnable,
                  CipLldpManagementDecodeLldpEnable,
                  &g_lldp_management_object_instance_values.lldp_enable,
                  kSetAndGetAble);
  InsertAttribute(instance, 2, kCipUint, EncodeCipUint, 
                  (CipAttributeDecodeFromMessage)DecodeCipUint,
                  &g_lldp_management_object_instance_values.msg_tx_interval,
                  kSetAndGetAble | kPostSetFunc);
  InsertAttribute(instance, 3, kCipUsint, EncodeCipUsint, 
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_lldp_management_object_instance_values.msg_tx_hold,
                  kSetAndGetAble);
  InsertAttribute(instance, 4, kCipWord, EncodeCipWord, 
                  (CipAttributeDecodeFromMessage)DecodeCipWord,
                  &g_lldp_management_object_instance_values.lldp_datastore,
                  kSetAndGetAble);
  InsertAttribute(instance, 5, kCipUdint, EncodeCipUdint, 
                  (CipAttributeDecodeFromMessage)DecodeCipUdint,
                  &g_lldp_management_object_instance_values.last_change,
                  kSetAndGetAble);
  
  // Register instance services (required for attribute access)
  InsertService(lldp_management_class,
                kGetAttributeSingle,
                &GetAttributeSingle,
                "GetAttributeSingle");
  InsertService(lldp_management_class,
                kGetAttributeAll,
                &GetAttributeAll,
                "GetAttributeAll");
  InsertService(lldp_management_class,
                kSetAttributeSingle,
                &SetAttributeSingle,
                "SetAttributeSingle");
  
  // Register PostSetCallback to handle attribute changes
  InsertGetSetCallback(lldp_management_class, CipLldpManagementPostSetCallback, kPostSetFunc);

  // Create timer to update last_change counter every second
  s_last_change_timer = xTimerCreate("LLDP_LastChange",
                                     pdMS_TO_TICKS(1000),  // 1 second
                                     pdTRUE,  // Auto-reload
                                     NULL,
                                     LldpManagementLastChangeTimerCallback);
  if (s_last_change_timer == NULL) {
    OPENER_TRACE_ERR("Failed to create last_change timer\n");
  } else {
    if (xTimerStart(s_last_change_timer, 0) != pdPASS) {
      OPENER_TRACE_ERR("Failed to start last_change timer, cleaning up\n");
      xTimerDelete(s_last_change_timer, portMAX_DELAY);
      s_last_change_timer = NULL;
    }
  }

  return kEipStatusOk;
}

/**
 * @brief Deinitialize LLDP Management Object and cleanup timers
 */
void kCipLldpManagementDeinit(void) {
  if (s_last_change_timer != NULL) {
    xTimerStop(s_last_change_timer, portMAX_DELAY);
    xTimerDelete(s_last_change_timer, portMAX_DELAY);
    s_last_change_timer = NULL;
  }
  
  if (s_last_change_mutex != NULL) {
    vSemaphoreDelete(s_last_change_mutex);
    s_last_change_mutex = NULL;
  }
}