/*******************************************************************************
 * Copyright (c) 2025, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

/** @file eth_media_counters.h
 *  @brief ESP32 Ethernet Media Counter Support
 *
 *  This module provides hardware-level media counter collection for the
 *  EtherNet/IP Ethernet Link object (Attribute #5). It supports IP101 PHY
 *  with automatic detection and graceful fallback for other PHY types.
 */

#ifndef ETH_MEDIA_COUNTERS_H
#define ETH_MEDIA_COUNTERS_H

#include "cipethernetlink.h"
#include "esp_eth_mac.h"

/**
 * @brief Initialize media counter support
 * 
 * Detects the PHY type and stores MAC pointer if IP101 is detected.
 * Should be called after Ethernet is initialized and link is up.
 * 
 * @param mac Pointer to ESP32 MAC driver instance
 * @param phy_addr PHY address (typically CONFIG_OPENER_ETH_PHY_ADDR)
 * @return true if IP101 PHY detected and media counters enabled, false otherwise
 */
bool EthMediaCountersInit(esp_eth_mac_t *mac, uint8_t phy_addr);

/**
 * @brief Check if media counters are supported
 * 
 * @return true if IP101 PHY detected and counters are available, false otherwise
 */
bool EthMediaCountersSupported(void);

/**
 * @brief Collect media counters from hardware
 * 
 * Reads counters from ESP32 EMAC and IP101 PHY (if detected).
 * If IP101 is not detected, PHY-specific counters are set to zero.
 * ESP32 EMAC counters are always available regardless of PHY type.
 * 
 * @param counters Pointer to media counters structure to fill
 */
void EthMediaCountersCollect(CipEthernetLinkMediaCounters *counters);

/**
 * @brief Reset media counter baselines
 * 
 * This should be called when GetAndClear service is invoked.
 * It captures the current hardware counter values as the new baseline,
 * so subsequent reads will show deltas from this point.
 */
void EthMediaCountersResetBaseline(void);

#endif /* ETH_MEDIA_COUNTERS_H */

