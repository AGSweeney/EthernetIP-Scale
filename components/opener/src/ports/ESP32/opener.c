/*******************************************************************************
 * Copyright (c) 2023, Peter Christen
 * All rights reserved.
 *
 ******************************************************************************/
#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipcommon.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"
#include "nvdata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portable.h"
#include "esp_random.h"
#include "opener_user_conf.h"
#include "lldp.h"

#define OPENER_THREAD_PRIO			5
#define OPENER_STACK_SIZE			  8192  // Increased from 2000 to prevent stack overflow

static void opener_thread(void *argument);
static SemaphoreHandle_t opener_init_mutex = NULL;
static bool opener_initialized = false;
TaskHandle_t opener_task_handle = NULL;
volatile int g_end_stack = 0;

void opener_init(struct netif *netif) {
  // Create mutex on first call if needed
  if (opener_init_mutex == NULL) {
    opener_init_mutex = xSemaphoreCreateMutex();
    if (opener_init_mutex == NULL) {
      OPENER_TRACE_ERR("Failed to create opener init mutex\n");
      return;
    }
  }

  // Take mutex to prevent double initialization
  if (xSemaphoreTake(opener_init_mutex, portMAX_DELAY) != pdTRUE) {
    OPENER_TRACE_ERR("Failed to take opener init mutex\n");
    return;
  }

  // Check if already initialized
  if (opener_initialized) {
    OPENER_TRACE_WARN("Opener already initialized, skipping\n");
    xSemaphoreGive(opener_init_mutex);
    return;
  }

  EipStatus eip_status = 0;

  if (IfaceLinkIsUp(netif)) {
    DoublyLinkedListInitialize(&connection_list,
                               CipConnectionObjectListArrayAllocator,
                               CipConnectionObjectListArrayFree);

    uint8_t iface_mac[6];
    IfaceGetMacAddress(netif, iface_mac);

    SetDeviceSerialNumber(123456789);

    // Use hardware random number generator instead of rand()
    EipUint16 unique_connection_id = (EipUint16)(esp_random() & 0xFFFF);

    EipStatus eip_status = CipStackInit(unique_connection_id);

    CipClass *tcp_ip_class = GetCipClass(kCipTcpIpInterfaceClassCode);
    if (NULL != tcp_ip_class) {
      InsertGetSetCallback(tcp_ip_class, NvTcpipSetCallback, kNvDataFunc);
    }

    CipEthernetLinkSetMac(iface_mac);

    GetHostName(netif, &g_tcpip.hostname);

    g_end_stack = 0;

    eip_status = IfaceGetConfiguration(netif, &g_tcpip.interface_configuration);
    if (eip_status < 0) {
      OPENER_TRACE_WARN("Problems getting interface configuration\n");
    }

    eip_status = NetworkHandlerInitialize();
    if (eip_status != kEipStatusOk) {
      OPENER_TRACE_ERR("NetworkHandlerInitialize failed with status %d\n", eip_status);
    }
    
    // Initialize LLDP component (if enabled)
    #if OPENER_LLDP_ENABLED
    OPENER_TRACE_INFO("Initializing LLDP component...\n");
    EipStatus lldp_status = LldpComponentInit(netif);
    if (lldp_status != kEipStatusOk) {
      OPENER_TRACE_WARN("LLDP component initialization failed with status %d\n", lldp_status);
    } else {
      OPENER_TRACE_INFO("LLDP component initialization succeeded\n");
    }
    #endif
  }
  else {
    OPENER_TRACE_WARN("Network link is down, OpENer not started\n");
    g_end_stack = 1;
  }
  OPENER_TRACE_INFO("Opener init: g_end_stack=%d, eip_status=%d\n", g_end_stack, eip_status);
  if ((g_end_stack == 0) && (eip_status == kEipStatusOk)) {
    // Pin OpENer task to Core 0 (same as LWIP TCP/IP task)
    OPENER_TRACE_INFO("Creating OpENer task...\n");
    BaseType_t result = xTaskCreatePinnedToCore(opener_thread,
                                                 "OpENer",
                                                 OPENER_STACK_SIZE,
                                                 netif,
                                                 OPENER_THREAD_PRIO,
                                                 &opener_task_handle,
                                                 0);  // Core 0
    if (result == pdPASS) {
      opener_initialized = true;
      OPENER_TRACE_INFO("OpENer: opener_thread started on Core 0, free heap size: %d\n",
             xPortGetFreeHeapSize());
    } else {
      OPENER_TRACE_ERR("Failed to create OpENer task (result=%d)\n", result);
    }
  } else {
    OPENER_TRACE_ERR("Opener init failed: g_end_stack=%d, eip_status=%d\n", g_end_stack, eip_status);
  }

  xSemaphoreGive(opener_init_mutex);
}

static void opener_thread(void *argument) {
  struct netif *netif = (struct netif*) argument;
  while (!g_end_stack) {
    if (kEipStatusOk != NetworkHandlerProcessCyclic()) {
      OPENER_TRACE_ERR("Error in NetworkHandler loop! Exiting OpENer!\n");
      g_end_stack = 1;
    }
    if (!IfaceLinkIsUp(netif)) {
      OPENER_TRACE_INFO("Network link is down, exiting OpENer\n");
      g_end_stack = 1;
    }
  }
  NetworkHandlerFinish();
  ShutdownCipStack();
  
  // Cleanup LLDP component
  #if OPENER_LLDP_ENABLED
  LldpComponentDeinit();
  #endif
  
  // Mark as not initialized and clear task handle atomically
  if (opener_init_mutex != NULL) {
    if (xSemaphoreTake(opener_init_mutex, portMAX_DELAY) == pdTRUE) {
      opener_initialized = false;
      opener_task_handle = NULL;
      xSemaphoreGive(opener_init_mutex);
    }
  } else {
    opener_task_handle = NULL;
  }
  
  vTaskDelete(NULL);
}

