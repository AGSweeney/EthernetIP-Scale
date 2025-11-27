/*******************************************************************************
 * Copyright (c) 2012, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "cipstring.h"
#include "ciptypes.h"
#include "typedefs.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvtcpip.h"
#include "cipethernetlink.h"
#include "generic_networkhandler.h"
#include "sdkconfig.h"
#include "system_config.h"

struct netif;

static void ScheduleRestart(void);
static void RestoreTcpIpDefaults(void);
static void ConfigureStatusLed(void);

#define DEMO_APP_INPUT_ASSEMBLY_NUM                100
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM               150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM               151
EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];

static const gpio_num_t kStatusLedGpio = GPIO_NUM_33;
static bool restart_pending = false;
static EipUint32 s_active_io_connections = 0;
static bool s_io_activity_seen = false;

/* Mutexes for thread-safe access */
static SemaphoreHandle_t s_assembly_mutex = NULL;  // Protects assembly data arrays

// Export mutex for webui_api.c and modbus_register_map.c
SemaphoreHandle_t scale_application_get_assembly_mutex(void)
{
    return s_assembly_mutex;
}


static void IdentityEnter(CipIdentityState state,
                          CipIdentityExtendedStatus ext_status) {
  if (g_identity.state != (CipUsint)state) {
    OPENER_TRACE_INFO("Identity state -> %u\n", (unsigned)state);
    g_identity.state = (CipUsint)state;
  }
  CipIdentitySetExtendedDeviceStatus(ext_status);
}

static void IdentityFlagFault(bool fatal) {
  CipWord flag = fatal ? kMajorUnrecoverableFault : kMajorRecoverableFault;
  CipIdentitySetStatusFlags(flag);
  IdentityEnter(fatal ? kStateMajorUnrecoverableFault
                      : kStateMajorRecoverableFault,
                kMajorFault);
}

static void IdentityNoteIoActivity(void) {
  if (s_active_io_connections > 0) {
    s_io_activity_seen = true;
    IdentityEnter(kStateOperational, kAtLeastOneIoConnectionInRunMode);
  }
}

void ScaleApplicationNotifyLinkUp(void) {
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateEnabled);
  IdentityEnter(kStateStandby,
                s_active_io_connections > 0 ?
                kAtLeastOneIoConnectionEstablishedAllInIdleMode :
                kNoIoConnectionsEstablished);
}

void ScaleApplicationNotifyLinkDown(void) {
  s_active_io_connections = 0;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  s_io_activity_seen = false;
  IdentityFlagFault(false);
}

void ScaleApplicationSetActiveNetif(struct netif *netif) {
  (void)netif;
}

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
EipStatus EthLnkPreGetCallback(CipInstance *instance,
                               CipAttributeStruct *attribute,
                               CipByte service);
EipStatus EthLnkPostGetCallback(CipInstance *instance,
                                CipAttributeStruct *attribute,
                                CipByte service);
#endif

static void RestartTask(void *param) {
  (void)param;
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_restart();
}

static void ScheduleRestart(void) {
  if (restart_pending) {
    return;
  }
  restart_pending = true;
  xTaskCreate(RestartTask, "restart", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
}

static void RestoreTcpIpDefaults(void) {
  g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
  g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
  g_tcpip.interface_configuration.ip_address = 0;
  g_tcpip.interface_configuration.network_mask = 0;
  g_tcpip.interface_configuration.gateway = 0;
  g_tcpip.interface_configuration.name_server = 0;
  g_tcpip.interface_configuration.name_server_2 = 0;
  ClearCipString(&g_tcpip.interface_configuration.domain_name);
  ClearCipString(&g_tcpip.hostname);
  g_tcpip.status |= kTcpipStatusIfaceCfgPend;
  (void)NvTcpipStore(&g_tcpip);
}

static void ConfigureStatusLed(void) {
  gpio_config_t led_config = {
    .pin_bit_mask = 1ULL << kStatusLedGpio,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&led_config);
  gpio_set_level(kStatusLedGpio, 0);
}


EipStatus ApplicationInitialization(void) {
  CreateAssemblyObject( DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data096,
                       sizeof(g_assembly_data096));

  CreateAssemblyObject( DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data064,
                       sizeof(g_assembly_data064));

  CreateAssemblyObject( DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data097,
                       sizeof(g_assembly_data097));

  ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
  DEMO_APP_INPUT_ASSEMBLY_NUM,
                                         DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureInputOnlyConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                    DEMO_APP_INPUT_ASSEMBLY_NUM,
                                    DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureListenOnlyConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                     DEMO_APP_INPUT_ASSEMBLY_NUM,
                                     DEMO_APP_CONFIG_ASSEMBLY_NUM);
  CipRunIdleHeaderSetO2T(false);
  CipRunIdleHeaderSetT2O(false);
  ConfigureStatusLed();

  /* Initialize mutexes */
  s_assembly_mutex = xSemaphoreCreateMutex();
  if (s_assembly_mutex == NULL) {
    OPENER_TRACE_ERR("Failed to create assembly mutex\n");
  }

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  {
    CipClass *p_eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPreGetCallback,
                         kPreGetFunc);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPostGetCallback,
                         kPostGetFunc);
    for (int idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx)
    {
      CipAttributeStruct *p_eth_link_attr;
      CipInstance *p_eth_link_inst =
        GetCipInstance(p_eth_link_class, idx + 1);
      OPENER_ASSERT(p_eth_link_inst);

      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 4);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 5);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
    }
  }
#endif

  s_active_io_connections = 0;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);

  return kEipStatusOk;
}

void HandleApplication(void) {
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {

  (void) output_assembly_id;
  (void) input_assembly_id;

  switch (io_connection_event) {
    case kIoConnectionEventOpened:
      if (s_active_io_connections++ == 0) {
        IdentityEnter(kStateStandby,
                      kAtLeastOneIoConnectionEstablishedAllInIdleMode);
      }
      break;
    case kIoConnectionEventTimedOut:
    case kIoConnectionEventClosed:
      if (s_active_io_connections > 0) {
        s_active_io_connections--;
      }
      if (s_active_io_connections == 0) {
        s_io_activity_seen = false;
        IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
      }
      break;
    default:
      break;
  }
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  EipStatus status = kEipStatusOk;

  switch (instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      /* Process output assembly data (LED control only) */
      gpio_set_level(kStatusLedGpio,
                     (g_assembly_data096[0] & 0x01) ? 1 : 0);
      IdentityNoteIoActivity();
      break;
    case DEMO_APP_CONFIG_ASSEMBLY_NUM:
      status = kEipStatusOk;
      break;
    default:
      OPENER_TRACE_INFO(
          "Unknown assembly instance ind AfterAssemblyDataReceived");
      break;
  }
  return status;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *instance) {
  (void) instance;
  IdentityNoteIoActivity();
  return true;
}

EipStatus ResetDevice(void) {
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  s_active_io_connections = 0;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateSelfTesting, kSelftestingUnknown);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  ScheduleRestart();
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  RestoreTcpIpDefaults();
  s_active_io_connections = 0;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateSelfTesting, kSelftestingUnknown);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  ScheduleRestart();
  return kEipStatusOk;
}

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
static void ZeroInterfaceCounters(CipEthernetLinkInterfaceCounters *counters) {
  memset(counters->cntr32, 0, sizeof(counters->cntr32));
}

static void ZeroMediaCounters(CipEthernetLinkMediaCounters *counters) {
  memset(counters->cntr32, 0, sizeof(counters->cntr32));
}

EipStatus EthLnkPreGetCallback(CipInstance *instance,
                               CipAttributeStruct *attribute,
                               CipByte service) {
  (void)service;
  if (instance == NULL || attribute == NULL) {
    return kEipStatusOk;
  }

  if (instance->instance_number == 0 ||
      instance->instance_number > OPENER_ETHLINK_INSTANCE_CNT) {
    return kEipStatusOk;
  }

  size_t idx = instance->instance_number - 1U;
  switch (attribute->attribute_number) {
    case 4: {
      const NetworkInterfaceCounters *src = NetworkGetInterfaceCounters();
      CipEthernetLinkInterfaceCounters *dst = &g_ethernet_link[idx].interface_cntrs;
      dst->ul.in_octets         = src->in_octets;
      dst->ul.in_ucast          = src->in_ucast_packets;
      dst->ul.in_nucast         = src->in_nucast_packets;
      dst->ul.in_discards       = src->in_discards;
      dst->ul.in_errors         = src->in_errors;
      dst->ul.in_unknown_protos = src->in_unknown_protos;
      dst->ul.out_octets        = src->out_octets;
      dst->ul.out_ucast         = src->out_ucast_packets;
      dst->ul.out_nucast        = src->out_nucast_packets;
      dst->ul.out_discards      = src->out_discards;
      dst->ul.out_errors        = src->out_errors;
      OPENER_TRACE_INFO("EthCntr Pre: inst=%u in_oct=%" PRIu32 " in_ucast=%" PRIu32 " out_ucast=%" PRIu32 " out_oct=%" PRIu32 "\n",
                        (unsigned)instance->instance_number,
                        src->in_octets,
                        src->in_ucast_packets,
                        src->out_ucast_packets,
                        src->out_octets);
      break;
    }
    case 5: {
      CipEthernetLinkMediaCounters *dst = &g_ethernet_link[idx].media_cntrs;
      ZeroMediaCounters(dst);
      break;
    }
    default:
      break;
  }

  return kEipStatusOk;
}

EipStatus EthLnkPostGetCallback(CipInstance *instance,
                                CipAttributeStruct *attribute,
                                CipByte service) {
  if (instance == NULL || attribute == NULL) {
    return kEipStatusOk;
  }

  if ((service & 0x7FU) != kEthLinkGetAndClear) {
    return kEipStatusOk;
  }

  if (instance->instance_number == 0 ||
      instance->instance_number > OPENER_ETHLINK_INSTANCE_CNT) {
    return kEipStatusOk;
  }

  size_t idx = instance->instance_number - 1U;
  switch (attribute->attribute_number) {
    case 4:
      ZeroInterfaceCounters(&g_ethernet_link[idx].interface_cntrs);
      NetworkResetInterfaceCounters();
      break;
    case 5:
      ZeroMediaCounters(&g_ethernet_link[idx].media_cntrs);
      break;
    default:
      break;
  }

  return kEipStatusOk;
}
#else
EipStatus EthLnkPreGetCallback(CipInstance *instance,
                               CipAttributeStruct *attribute,
                               CipByte service) {
  (void)instance;
  (void)attribute;
  (void)service;
  return kEipStatusOk;
}

EipStatus EthLnkPostGetCallback(CipInstance *instance,
                                CipAttributeStruct *attribute,
                                CipByte service) {
  (void)instance;
  (void)attribute;
  (void)service;
  return kEipStatusOk;
}
#endif /* OPENER_ETHLINK_CNTRS_ENABLE */

void*
CipCalloc(size_t number_of_elements,
          size_t size_of_element) {
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  free(data);
}

void RunIdleChanged(EipUint32 run_idle_value) {
  OPENER_TRACE_INFO("Run/Idle handler triggered\n");
  if ((0x0001 & run_idle_value) == 1) {
    IdentityNoteIoActivity();
  } else if (s_active_io_connections == 0) {
    IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
  } else if (!s_io_activity_seen) {
    IdentityEnter(kStateStandby,
                  kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
  (void) run_idle_value;
}

