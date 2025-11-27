/*******************************************************************************
 * Copyright (c) 2018, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cipstring.h"
#include "networkconfig.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "trace.h"
#include "opener_api.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"


EipStatus IfaceGetMacAddress(TcpIpInterface *iface,
                             uint8_t *const physical_address) {
  memcpy(physical_address, iface->hwaddr, NETIF_MAX_HWADDR_LEN);

  return kEipStatusOk;
}

static EipStatus GetIpAndNetmaskFromInterface(
    TcpIpInterface *iface, CipTcpIpInterfaceConfiguration *iface_cfg) {
  iface_cfg->ip_address = ip4_addr_get_u32(ip_2_ip4(&iface->ip_addr));
  iface_cfg->network_mask = ip4_addr_get_u32(ip_2_ip4(&iface->netmask));

  return kEipStatusOk;
}

static EipStatus GetGatewayFromRoute(TcpIpInterface *iface,
                                     CipTcpIpInterfaceConfiguration *iface_cfg) {
  iface_cfg->gateway = ip4_addr_get_u32(ip_2_ip4(&iface->gw));

  return kEipStatusOk;
}

EipStatus IfaceGetConfiguration(TcpIpInterface *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  CipTcpIpInterfaceConfiguration local_cfg;
  EipStatus status;

  memset(&local_cfg, 0x00, sizeof local_cfg);

  status = GetIpAndNetmaskFromInterface(iface, &local_cfg);
  if (kEipStatusOk == status) {
    status = GetGatewayFromRoute(iface, &local_cfg);
  }
  if (kEipStatusOk == status) {
    ClearCipString(&iface_cfg->domain_name);
    *iface_cfg = local_cfg;
  }
  return status;
}

void GetHostName(TcpIpInterface *iface,
                 CipString *hostname) {
  SetCipStringByCstr(hostname, netif_get_hostname(iface));
}

