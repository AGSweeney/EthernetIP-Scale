/*******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

/** @file nvtcpip.c
 *  @brief This file implements the functions to handle TCP/IP object's NV data.
 *
 *  This is only a code skeleton. The real load and store operation is NOT
 *  implemented.
 */
#include "nvtcpip.h"

#include <string.h>
#include <inttypes.h>

#include "ciptcpipinterface.h"
#include "cipstring.h"
#include "trace.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"

#define TCPIP_NVS_NAMESPACE  "opener"   /**< NVS namespace for TCP/IP data */
#define TCPIP_NVS_KEY        "tcpip_cfg"
#define TCPIP_NV_VERSION     2U

#define TCPIP_DOMAIN_MAX_LEN   48U
#define TCPIP_HOSTNAME_MAX_LEN 64U

static const char *kTag = "NvTcpip";

typedef struct __attribute__((packed)) {
  uint32_t version;
  uint32_t config_control;
  uint32_t ip_address;
  uint32_t network_mask;
  uint32_t gateway;
  uint32_t name_server;
  uint32_t name_server2;
  uint16_t domain_length;
  uint16_t hostname_length;
  uint8_t domain[TCPIP_DOMAIN_MAX_LEN];
  uint8_t hostname[TCPIP_HOSTNAME_MAX_LEN];
  uint8_t select_acd;
} TcpipNvBlob;

typedef struct __attribute__((packed)) {
  uint32_t version;
  uint32_t config_control;
  uint32_t ip_address;
  uint32_t network_mask;
  uint32_t gateway;
  uint32_t name_server;
  uint32_t name_server2;
  uint16_t domain_length;
  uint16_t hostname_length;
  uint8_t domain[TCPIP_DOMAIN_MAX_LEN];
  uint8_t hostname[TCPIP_HOSTNAME_MAX_LEN];
} TcpipNvBlobV1;

static esp_err_t TcpipNvOpen(nvs_handle_t *handle, nvs_open_mode_t mode) {
  esp_err_t err = nvs_open(TCPIP_NVS_NAMESPACE, mode, handle);
  if (ESP_ERR_NVS_NOT_FOUND == err && mode == NVS_READWRITE) {
    /* Namespace does not exist yet - create by opening in read/write */
    err = nvs_open(TCPIP_NVS_NAMESPACE, NVS_READWRITE, handle);
  }
  if (ESP_OK != err) {
    ESP_LOGE(kTag, "nvs_open failed (%s)", esp_err_to_name(err));
  }
  return err;
}

/** @brief Load NV data of the TCP/IP object from NVS
 *
 *  @param  p_tcp_ip pointer to the TCP/IP object's data structure
 *  @return kEipStatusOk: success; kEipStatusError: failure
 */
EipStatus NvTcpipLoad(CipTcpIpObject *p_tcp_ip) {
  nvs_handle_t handle;
  esp_err_t err = TcpipNvOpen(&handle, NVS_READONLY);
  if (ESP_ERR_NVS_NOT_FOUND == err) {
    ESP_LOGI(kTag, "No stored TCP/IP configuration found, using defaults");
    return kEipStatusError;
  } else if (ESP_OK != err) {
    return kEipStatusError;
  }

  uint8_t raw_blob[sizeof(TcpipNvBlob)] = {0};
  size_t length = sizeof(raw_blob);
  err = nvs_get_blob(handle, TCPIP_NVS_KEY, raw_blob, &length);
  nvs_close(handle);
  if (ESP_ERR_NVS_NOT_FOUND == err) {
    ESP_LOGI(kTag, "No stored TCP/IP configuration found, using defaults");
    return kEipStatusError;
  }
  if (ESP_OK != err) {
    ESP_LOGE(kTag, "Failed to load TCP/IP configuration (%s)", esp_err_to_name(err));
    return kEipStatusError;
  }

  const TcpipNvBlob *blob_v2 = NULL;
  TcpipNvBlobV1 blob_v1 = {0};

  if (length == sizeof(TcpipNvBlob)) {
    blob_v2 = (const TcpipNvBlob *)raw_blob;
    if (blob_v2->version != TCPIP_NV_VERSION) {
      ESP_LOGW(kTag, "Stored TCP/IP configuration has unexpected version %" PRIu32,
               blob_v2->version);
      return kEipStatusError;
    }
  } else if (length == sizeof(TcpipNvBlobV1)) {
    memcpy(&blob_v1, raw_blob, sizeof(blob_v1));
    if (blob_v1.version != 1U) {
      ESP_LOGW(kTag, "Stored TCP/IP configuration has incompatible format");
      return kEipStatusError;
    }
  } else {
    ESP_LOGW(kTag, "Stored TCP/IP configuration has incompatible format");
    return kEipStatusError;
  }

  if (blob_v2 != NULL) {
    p_tcp_ip->config_control = blob_v2->config_control;
    p_tcp_ip->interface_configuration.ip_address = blob_v2->ip_address;
    p_tcp_ip->interface_configuration.network_mask = blob_v2->network_mask;
    p_tcp_ip->interface_configuration.gateway = blob_v2->gateway;
    p_tcp_ip->interface_configuration.name_server = blob_v2->name_server;
    p_tcp_ip->interface_configuration.name_server_2 = blob_v2->name_server2;
    p_tcp_ip->select_acd = (blob_v2->select_acd != 0u);
  } else {
    p_tcp_ip->config_control = blob_v1.config_control;
    p_tcp_ip->interface_configuration.ip_address = blob_v1.ip_address;
    p_tcp_ip->interface_configuration.network_mask = blob_v1.network_mask;
    p_tcp_ip->interface_configuration.gateway = blob_v1.gateway;
    p_tcp_ip->interface_configuration.name_server = blob_v1.name_server;
    p_tcp_ip->interface_configuration.name_server_2 = blob_v1.name_server2;
    p_tcp_ip->select_acd = false;
  }

  ip4_addr_t nv_ip = { .addr = p_tcp_ip->interface_configuration.ip_address };
  ip4_addr_t nv_mask = { .addr = p_tcp_ip->interface_configuration.network_mask };
  ip4_addr_t nv_gw = { .addr = p_tcp_ip->interface_configuration.gateway };
  ip4_addr_t nv_dns1 = { .addr = p_tcp_ip->interface_configuration.name_server };
  ip4_addr_t nv_dns2 = { .addr = p_tcp_ip->interface_configuration.name_server_2 };

  char ip_str[16];
  char mask_str[16];
  char gw_str[16];
  char dns1_str[16];
  char dns2_str[16];

  const char *dns1_print = ip4_addr_isany_val(nv_dns1) ?
                           "none" :
                           ip4addr_ntoa_r(&nv_dns1, dns1_str, sizeof dns1_str);
  const char *dns2_print = ip4_addr_isany_val(nv_dns2) ?
                           "none" :
                           ip4addr_ntoa_r(&nv_dns2, dns2_str, sizeof dns2_str);

  ESP_LOGI(kTag,
           "NV config ctrl=0x%08lx ip=%s mask=%s gw=%s dns=%s/%s",
           (unsigned long)p_tcp_ip->config_control,
           ip4addr_ntoa_r(&nv_ip, ip_str, sizeof ip_str),
           ip4addr_ntoa_r(&nv_mask, mask_str, sizeof mask_str),
           ip4addr_ntoa_r(&nv_gw, gw_str, sizeof gw_str),
           dns1_print,
           dns2_print);

  ClearCipString(&p_tcp_ip->interface_configuration.domain_name);
  uint16_t domain_length = blob_v2 ? blob_v2->domain_length : blob_v1.domain_length;
  if (domain_length > TCPIP_DOMAIN_MAX_LEN) {
    domain_length = TCPIP_DOMAIN_MAX_LEN;
  }
  if (domain_length > 0u) {
    const uint8_t *domain_src = blob_v2 ? blob_v2->domain : blob_v1.domain;
    if (NULL == SetCipStringByData(&p_tcp_ip->interface_configuration.domain_name,
                                   domain_length,
                                   domain_src)) {
      ESP_LOGE(kTag, "Failed to restore domain name");
      return kEipStatusError;
    }
  }

  ClearCipString(&p_tcp_ip->hostname);
  uint16_t hostname_length = blob_v2 ? blob_v2->hostname_length : blob_v1.hostname_length;
  if (hostname_length > TCPIP_HOSTNAME_MAX_LEN) {
    hostname_length = TCPIP_HOSTNAME_MAX_LEN;
  }
  if (hostname_length > 0u) {
    const uint8_t *hostname_src = blob_v2 ? blob_v2->hostname : blob_v1.hostname;
    if (NULL == SetCipStringByData(&p_tcp_ip->hostname,
                                   hostname_length,
                                   hostname_src)) {
      ESP_LOGE(kTag, "Failed to restore host name");
      return kEipStatusError;
    }
  }

  /* Mark configuration as applied */
  p_tcp_ip->status |= 0x01;
  p_tcp_ip->status &= ~kTcpipStatusIfaceCfgPend;

  if ((p_tcp_ip->config_control & kTcpipCfgCtrlMethodMask) == kTcpipCfgCtrlStaticIp) {
    if (!CipTcpIpIsValidNetworkConfig(&p_tcp_ip->interface_configuration)) {
      ESP_LOGW(kTag, "Stored static configuration invalid, switching to DHCP");
      p_tcp_ip->config_control &= ~kTcpipCfgCtrlMethodMask;
      p_tcp_ip->config_control |= kTcpipCfgCtrlDhcp;
      p_tcp_ip->interface_configuration.ip_address = 0u;
      p_tcp_ip->interface_configuration.network_mask = 0u;
      p_tcp_ip->interface_configuration.gateway = 0u;
      p_tcp_ip->interface_configuration.name_server = 0u;
      p_tcp_ip->interface_configuration.name_server_2 = 0u;
      p_tcp_ip->status &= ~(kTcpipStatusAcdStatus | kTcpipStatusAcdFault);
      (void)NvTcpipStore(p_tcp_ip);
    }
  }

  if (blob_v2 == NULL) {
    /* Upgrade legacy blob to latest format */
    (void)NvTcpipStore(p_tcp_ip);
  }

  ESP_LOGI(kTag, "Restored TCP/IP configuration (method=%s)",
           ( (p_tcp_ip->config_control & kTcpipCfgCtrlMethodMask) == kTcpipCfgCtrlDhcp) ?
           "DHCP" : "Static");

  return kEipStatusOk;
}

/** @brief Store NV data of the TCP/IP object to NVS
 *
 *  @param  p_tcp_ip pointer to the TCP/IP object's data structure
 *  @return kEipStatusOk: success; kEipStatusError: failure
 */
EipStatus NvTcpipStore(const CipTcpIpObject *p_tcp_ip) {
  nvs_handle_t handle;
  esp_err_t err = TcpipNvOpen(&handle, NVS_READWRITE);
  if (ESP_OK != err) {
    return kEipStatusError;
  }

  TcpipNvBlob blob = {0};
  blob.version = TCPIP_NV_VERSION;
  blob.config_control = p_tcp_ip->config_control;
  blob.ip_address = p_tcp_ip->interface_configuration.ip_address;
  blob.network_mask = p_tcp_ip->interface_configuration.network_mask;
  blob.gateway = p_tcp_ip->interface_configuration.gateway;
  blob.name_server = p_tcp_ip->interface_configuration.name_server;
  blob.name_server2 = p_tcp_ip->interface_configuration.name_server_2;

  if (p_tcp_ip->interface_configuration.domain_name.length > TCPIP_DOMAIN_MAX_LEN) {
    blob.domain_length = TCPIP_DOMAIN_MAX_LEN;
  } else {
    blob.domain_length = p_tcp_ip->interface_configuration.domain_name.length;
  }
  if (blob.domain_length > 0u && NULL != p_tcp_ip->interface_configuration.domain_name.string) {
    memcpy(blob.domain,
           p_tcp_ip->interface_configuration.domain_name.string,
           blob.domain_length);
  }

  if (p_tcp_ip->hostname.length > TCPIP_HOSTNAME_MAX_LEN) {
    blob.hostname_length = TCPIP_HOSTNAME_MAX_LEN;
  } else {
    blob.hostname_length = p_tcp_ip->hostname.length;
  }
  if (blob.hostname_length > 0u && NULL != p_tcp_ip->hostname.string) {
    memcpy(blob.hostname, p_tcp_ip->hostname.string, blob.hostname_length);
  }

  blob.select_acd = p_tcp_ip->select_acd ? 1u : 0u;

  err = nvs_set_blob(handle, TCPIP_NVS_KEY, &blob, sizeof(blob));
  if (ESP_OK == err) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);

  if (ESP_OK != err) {
    ESP_LOGE(kTag, "Failed to store TCP/IP configuration (%s)", esp_err_to_name(err));
    return kEipStatusError;
  }

  ESP_LOGI(kTag, "Stored TCP/IP configuration (method=%s)",
           ((p_tcp_ip->config_control & kTcpipCfgCtrlMethodMask) == kTcpipCfgCtrlDhcp) ?
           "DHCP" : "Static");

  return kEipStatusOk;
}
