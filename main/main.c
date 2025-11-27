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

/**
 * @file main.c
 * @brief Main application entry point for ESP32-P4 EtherNet/IP device
 *
 * ADDRESS CONFLICT DETECTION (ACD) IMPLEMENTATION
 * ===============================================
 *
 * This file implements RFC 5227 compliant Address Conflict Detection (ACD) for
 * static IP addresses. ACD ensures that IP addresses are not assigned until
 * confirmed safe to use, preventing network conflicts.
 *
 * Architecture:
 * ------------
 * - Static IP: RFC 5227 compliant behavior (implemented in application layer)
 *   * Probe phase: 3 ARP probes from 0.0.0.0 with configurable intervals (default: 200ms)
 *   * Announce phase: 4 ARP announcements after successful probe (default: 2000ms intervals)
 *   * Ongoing defense: Periodic ARP probes every ~90 seconds (configurable)
 *   * Total time: ~6-10 seconds for initial IP assignment
 *   * ACD probe sequence runs BEFORE IP assignment
 *   * IP assigned only after ACD confirms no conflict (ACD_IP_OK callback)
 *
 * - DHCP: Simplified ACD (not fully RFC 5227 compliant)
 *   * ACD check performed by lwIP DHCP client before accepting IP
 *   * Handled internally by lwIP DHCP client
 *
 * Implementation:
 * --------------
 * The ACD implementation is in the application layer (this file) and coordinates
 * with the lwIP ACD module. The implementation follows RFC 5227 behavior:
 * - ACD probe sequence completes before IP assignment
 * - Uses tcpip_perform_acd() to coordinate probe sequence
 * - IP assignment deferred until ACD_IP_OK callback received
 * - Natural state machine flow: PROBE_WAIT → PROBING → ANNOUNCE_WAIT → ANNOUNCING → ONGOING
 *
 * Features:
 * --------
 * 1. Retry Logic (CONFIG_OPENER_ACD_RETRY_ENABLED):
 *    - On conflict, removes IP and schedules retry after delay
 *    - Configurable max attempts and retry delay
 *    - Prevents infinite retry loops
 *
 * 2. User LED Indication:
 *    - GPIO27 blinks during normal operation
 *    - Goes solid on ACD conflict detection
 *    - Visual feedback for network issues
 *
 * 3. Callback Tracking:
 *    - Distinguishes between callback events and timeout conditions
 *    - Prevents false positive conflict detection when probe sequence is still running
 *    - IP assignment occurs in callback when ACD_IP_OK fires
 *
 * Thread Safety:
 * -------------
 * - ACD operations use tcpip_callback_with_block() to ensure execution on tcpip thread
 * - Context structures allocated on heap to prevent stack corruption
 * - Semaphores coordinate async callback execution
 *
 * Configuration:
 * --------------
 * - CONFIG_OPENER_ACD_PROBE_NUM: Number of probes (default: 3)
 * - CONFIG_OPENER_ACD_PROBE_WAIT_MS: Initial delay before probing (default: 200ms)
 * - CONFIG_OPENER_ACD_PROBE_MIN_MS: Minimum delay between probes (default: 200ms)
 * - CONFIG_OPENER_ACD_PROBE_MAX_MS: Maximum delay between probes (default: 200ms)
 * - CONFIG_OPENER_ACD_ANNOUNCE_NUM: Number of announcements (default: 4)
 * - CONFIG_OPENER_ACD_ANNOUNCE_INTERVAL_MS: Time between announcements (default: 2000ms)
 * - CONFIG_OPENER_ACD_ANNOUNCE_WAIT_MS: Delay before announcing (default: 2000ms)
 * - CONFIG_OPENER_ACD_PERIODIC_DEFEND_INTERVAL_MS: Defensive ARP interval (default: 90000ms)
 * - CONFIG_OPENER_ACD_RETRY_ENABLED: Enable retry on conflict
 * - CONFIG_OPENER_ACD_RETRY_DELAY_MS: Delay before retry (default: 10000ms)
 * - CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS: Max retry attempts (default: 5)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lwip/netif.h"
#include "lwip/err.h"
#include "lwip/tcpip.h"
#include "lwip/ip4_addr.h"
#include "lwip/acd.h"
#include "lwip/netifapi.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "opener.h"
#include "nvtcpip.h"
#include "ciptcpipinterface.h"
#include "sdkconfig.h"
#include "esp_netif_net_stack.h"
#include "webui.h"
#include "modbus_tcp.h"
#include "ota_manager.h"
#include "system_config.h"
#include "log_buffer.h"
#include "nau7802.h"
#include "driver/i2c_master.h"

// Forward declaration - function is in opener component
SemaphoreHandle_t scale_application_get_assembly_mutex(void);

void ScaleApplicationSetActiveNetif(struct netif *netif);
void ScaleApplicationNotifyLinkUp(void);
void ScaleApplicationNotifyLinkDown(void);

// External assembly data arrays (defined in opener component)
extern uint8_t g_assembly_data064[32];  // Input Assembly 100
extern uint8_t g_assembly_data096[32];  // Output Assembly 150

static const char *TAG = "opener_main";
static struct netif *s_netif = NULL;
static SemaphoreHandle_t s_netif_mutex = NULL;
static bool s_services_initialized = false;

// NAU7802 scale device
static nau7802_t s_nau7802_device;
static i2c_master_bus_handle_t s_i2c_bus_handle = NULL;
static bool s_nau7802_initialized = false;
static TaskHandle_t s_nau7802_task_handle = NULL;
static SemaphoreHandle_t s_nau7802_mutex = NULL;  // Protects device I2C operations
static SemaphoreHandle_t s_nau7802_state_mutex = NULL;  // Protects initialization flag


// User LED state (GPIO27)
#define USER_LED_GPIO 27
static bool s_user_led_initialized = false;
static bool s_user_led_flash_enabled = false;
static TaskHandle_t s_user_led_task_handle = NULL;

#if LWIP_IPV4 && LWIP_ACD
static struct acd s_static_ip_acd;
static bool s_acd_registered = false;
static SemaphoreHandle_t s_acd_sem = NULL;
static SemaphoreHandle_t s_acd_registration_sem = NULL;  // Semaphore to wait for ACD registration
static acd_callback_enum_t s_acd_last_state = ACD_IP_OK;  // Will be set by callback when ACD completes
static bool s_acd_callback_received = false;  // Track if callback was actually received
static bool s_acd_probe_pending = false;
static esp_netif_ip_info_t s_pending_static_ip_cfg = {0};
#if CONFIG_OPENER_ACD_RETRY_ENABLED
static TimerHandle_t s_acd_retry_timer = NULL;
static int s_acd_retry_count = 0;
static esp_netif_t *s_acd_retry_netif = NULL;
static struct netif *s_acd_retry_lwip_netif = NULL;
#endif
#endif
static bool s_opener_initialized;

static bool tcpip_config_uses_dhcp(void);
static void configure_hostname(esp_netif_t *netif);
static void opener_configure_dns(esp_netif_t *netif);

static bool ip_info_has_static_address(const esp_netif_ip_info_t *ip_info) {
    if (ip_info == NULL) {
        return false;
    }
    if (ip_info->ip.addr == 0 || ip_info->netmask.addr == 0) {
        return false;
    }
    return true;
}

static bool tcpip_config_uses_dhcp(void) {
    return (g_tcpip.config_control & kTcpipCfgCtrlMethodMask) == kTcpipCfgCtrlDhcp;
}

static bool tcpip_static_config_valid(void) {
    if ((g_tcpip.config_control & kTcpipCfgCtrlMethodMask) != kTcpipCfgCtrlStaticIp) {
        return true;
    }
    return CipTcpIpIsValidNetworkConfig(&g_tcpip.interface_configuration);
}

static void configure_hostname(esp_netif_t *netif) {
    if (g_tcpip.hostname.length > 0 && g_tcpip.hostname.string != NULL) {
        size_t length = g_tcpip.hostname.length;
        if (length > 63) {
            length = 63;
        }
        char host[64];
        memcpy(host, g_tcpip.hostname.string, length);
        host[length] = '\0';
        esp_netif_set_hostname(netif, host);
    }
}

static void opener_configure_dns(esp_netif_t *netif) {
    esp_netif_dns_info_t dns_info = {
        .ip.type = IPADDR_TYPE_V4,
        .ip.u_addr.ip4.addr = g_tcpip.interface_configuration.name_server
    };
    if (dns_info.ip.u_addr.ip4.addr != 0) {
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info));
    }

    dns_info.ip.u_addr.ip4.addr = g_tcpip.interface_configuration.name_server_2;
    if (dns_info.ip.u_addr.ip4.addr != 0) {
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info));
    }
}

#if LWIP_IPV4 && LWIP_ACD
typedef struct {
    struct netif *netif;
    ip4_addr_t ip;
    err_t err;
} AcdStartContext;

typedef struct {
    struct netif *netif;
    ip4_addr_t ip;
    err_t err;
} AcdStartProbeContext;

/**
 * @brief Try to start pending ACD probe sequence
 * 
 * Attempts to start the ACD probe sequence if conditions are met (link up,
 * MAC address available). Called when ACD was deferred due to missing conditions.
 * 
 * @param netif ESP-NETIF handle
 * @param lwip_netif lwIP netif structure
 */
static void tcpip_try_pending_acd(esp_netif_t *netif, struct netif *lwip_netif);

/**
 * @brief Retry ACD after deferred delay
 * 
 * Callback to retry ACD after a short delay when it was deferred.
 * 
 * @param arg ESP-NETIF handle (cast from void*)
 */
static void tcpip_retry_acd_deferred(void *arg);

#if CONFIG_OPENER_ACD_RETRY_ENABLED
/**
 * @brief ACD retry timer callback
 * 
 * Called when ACD retry timer expires. Restarts ACD probe sequence.
 * 
 * @param xTimer Timer handle
 */
static void tcpip_acd_retry_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Start ACD retry sequence
 * 
 * Initiates a retry of the ACD probe sequence after a conflict was detected.
 * 
 * @param netif ESP-NETIF handle
 * @param lwip_netif lwIP netif structure
 */
static void tcpip_acd_start_retry(esp_netif_t *netif, struct netif *lwip_netif);

/**
 * @brief Callback to start ACD probe on tcpip thread
 * 
 * Used when direct acd_start() fails. Executes on tcpip thread with proper
 * context and stack space.
 * 
 * @param arg AcdStartProbeContext pointer (heap-allocated, freed in this function)
 */
static void acd_start_probe_cb(void *arg) {
    AcdStartProbeContext *ctx = (AcdStartProbeContext *)arg;
    if (ctx == NULL || ctx->netif == NULL) {
        ESP_LOGE(TAG, "acd_start_probe_cb: Invalid context");
        if (ctx) free(ctx);
        return;
    }
    ESP_LOGI(TAG, "acd_start_probe_cb: Calling acd_start() for IP " IPSTR " on netif %p", 
             IP2STR(&ctx->ip), ctx->netif);
    ctx->err = acd_start(ctx->netif, &s_static_ip_acd, ctx->ip);
    ESP_LOGI(TAG, "acd_start_probe_cb: acd_start() returned err=%d", (int)ctx->err);
    free(ctx);  // Free heap-allocated context
}

/**
 * @brief Callback for retry timer
 * 
 * Executes retry on tcpip thread (has more stack space). Called when
 * ACD retry timer expires.
 * 
 * @param arg Unused (cast to void*)
 */
static void retry_callback(void *arg) {
    (void)arg;
    if (s_acd_retry_netif != NULL && s_acd_retry_lwip_netif != NULL) {
        ESP_LOGI(TAG, "ACD retry timer expired - restarting ACD probe sequence (attempt %d)",
                 s_acd_retry_count + 1);
        tcpip_try_pending_acd(s_acd_retry_netif, s_acd_retry_lwip_netif);
    }
}
#endif
#endif

/**
 * @brief Check if netif has a valid hardware address
 * 
 * Verifies that the netif structure has a non-zero MAC address.
 * 
 * @param netif lwIP netif structure
 * @return true if netif has valid MAC address, false otherwise
 */
static bool netif_has_valid_hwaddr(struct netif *netif) {
    if (netif == NULL) {
        return false;
    }
    if (netif->hwaddr_len != ETH_HWADDR_LEN) {
        return false;
    }
    for (int i = 0; i < ETH_HWADDR_LEN; ++i) {
        if (netif->hwaddr[i] != 0) {
            return true;
        }
    }
    return false;
}

// User LED control functions
static void user_led_init(void);
static void user_led_set(bool on);
static void user_led_flash_task(void *pvParameters);
static void user_led_start_flash(void);
static void user_led_stop_flash(void);

// NAU7802 scale reading task
static void nau7802_scale_task(void *pvParameters);

/**
 * @brief ACD conflict detection callback
 * 
 * Called by lwIP ACD module when ACD state changes. Handles IP assignment,
 * conflict detection, retry logic, and LED indication.
 * 
 * @param netif lwIP netif structure
 * @param state ACD callback state:
 *              - ACD_IP_OK: Probe successful, IP can be assigned
 *              - ACD_RESTART_CLIENT: Conflict detected, restart client
 *              - ACD_DECLINE: Conflict detected, decline IP
 */
static void tcpip_acd_conflict_callback(struct netif *netif, acd_callback_enum_t state) {
    ESP_LOGI(TAG, "ACD callback received: state=%d (0=IP_OK, 1=RESTART_CLIENT, 2=DECLINE)", (int)state);
    s_acd_last_state = state;
    s_acd_callback_received = true;  // Mark that callback was actually received
    switch (state) {
        case ACD_IP_OK:
            g_tcpip.status &= ~(kTcpipStatusAcdStatus | kTcpipStatusAcdFault);
            // ACD_IP_OK means probe phase completed successfully and IP is assigned.
            // ACD now enters ONGOING state for periodic defense, so set activity = 1.
            CipTcpIpSetLastAcdActivity(1);
            // Resume LED blinking when IP is OK (no conflict)
            user_led_start_flash();
            ESP_LOGI(TAG, "ACD: IP OK - no conflict detected, entering ongoing defense phase");
#if CONFIG_OPENER_ACD_RETRY_ENABLED
            // Reset retry count on successful IP assignment
            s_acd_retry_count = 0;
            // Stop retry timer if running
            if (s_acd_retry_timer != NULL) {
                xTimerStop(s_acd_retry_timer, portMAX_DELAY);
            }
#endif
            /* Legacy mode: Assign IP if it hasn't been assigned yet (callback fired after timeout) */
            if (s_acd_probe_pending && netif != NULL) {
                esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
                if (esp_netif != NULL && s_pending_static_ip_cfg.ip.addr != 0) {
                    ESP_LOGI(TAG, "Legacy ACD: Assigning IP " IPSTR " after callback confirmation", IP2STR(&s_pending_static_ip_cfg.ip));
                    esp_netif_set_ip_info(esp_netif, &s_pending_static_ip_cfg);
                    opener_configure_dns(esp_netif);
                    s_acd_probe_pending = false;
                }
            }
            break;
        case ACD_DECLINE:
        case ACD_RESTART_CLIENT:
            g_tcpip.status |= kTcpipStatusAcdStatus;
            g_tcpip.status |= kTcpipStatusAcdFault;
            CipTcpIpSetLastAcdActivity(3);
            // Stop LED blinking and turn solid on ACD conflict
            user_led_stop_flash();
            user_led_set(true);  // Turn LED on solid
            ESP_LOGW(TAG, "ACD: Conflict detected (state=%d) - LED set to solid", (int)state);
#if CONFIG_OPENER_ACD_RETRY_ENABLED
            // Retry logic: On conflict, remove IP and schedule retry after delay
            if (netif != NULL) {
                esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
                if (esp_netif != NULL) {
                    // Check if we should retry
                    if (CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS == 0 || 
                        s_acd_retry_count < CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS) {
                        ESP_LOGW(TAG, "ACD: Scheduling retry (attempt %d/%d) after %dms",
                                 s_acd_retry_count + 1,
                                 CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS == 0 ? 999 : CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS,
                                 CONFIG_OPENER_ACD_RETRY_DELAY_MS);
                        tcpip_acd_start_retry(esp_netif, netif);
                    } else {
                        ESP_LOGE(TAG, "ACD: Max retry attempts (%d) reached - giving up",
                                 CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS);
                    }
                }
            }
#endif
            break;
        default:
            g_tcpip.status |= kTcpipStatusAcdStatus;
            g_tcpip.status |= kTcpipStatusAcdFault;
            break;
    }
    if (s_acd_sem != NULL) {
        xSemaphoreGive(s_acd_sem);
    }
}

/**
 * @brief ACD start callback (executes on tcpip thread)
 * 
 * Registers ACD client with lwIP ACD module. Called via tcpip_callback_with_block()
 * to ensure thread-safe execution on the tcpip thread.
 * 
 * @param arg AcdStartContext pointer (heap-allocated, freed in this function)
 */
static void tcpip_acd_start_cb(void *arg) {
    ESP_LOGI(TAG, "tcpip_acd_start_cb: CALLBACK EXECUTING - arg=%p", arg);
    AcdStartContext *ctx = (AcdStartContext *)arg;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "tcpip_acd_start_cb: NULL context");
        // Signal semaphore even on error so caller doesn't hang
        if (s_acd_registration_sem != NULL) {
            xSemaphoreGive(s_acd_registration_sem);
        }
        return;
    }
    ESP_LOGI(TAG, "tcpip_acd_start_cb: Context valid - netif=%p, ip=" IPSTR, 
             ctx->netif, IP2STR(&ctx->ip));
    ctx->err = ERR_OK;
    
    // NULL check: netif may be invalidated between context creation and callback execution
    if (ctx->netif == NULL) {
        ESP_LOGD(TAG, "tcpip_acd_start_cb: NULL netif - ACD probe cancelled");
        ctx->err = ERR_IF;
        free(ctx);
        return;
    }
    
    // If probe phase is complete, still register ACD for ongoing conflict detection
    bool probe_was_pending = s_acd_probe_pending;
    
    if (!s_acd_registered) {
        ctx->netif->acd_list = NULL;
        memset(&s_static_ip_acd, 0, sizeof(s_static_ip_acd));
        err_t add_err = acd_add(ctx->netif, &s_static_ip_acd, tcpip_acd_conflict_callback);
        if (add_err == ERR_OK) {
            s_acd_registered = true;
            ESP_LOGD(TAG, "tcpip_acd_start_cb: ACD client registered");
        } else {
            ESP_LOGE(TAG, "tcpip_acd_start_cb: acd_add() failed with err=%d", (int)add_err);
            ctx->err = ERR_IF;
            // Signal registration semaphore even on failure so caller doesn't hang
            if (s_acd_registration_sem != NULL) {
                xSemaphoreGive(s_acd_registration_sem);
            }
            free(ctx);
            return;
        }
    }
    
    // Signal registration semaphore to allow tcpip_perform_acd to wait for completion
    if (s_acd_registration_sem != NULL) {
        xSemaphoreGive(s_acd_registration_sem);
    }
    
    // If probe phase was skipped (IP already assigned), manually transition to ONGOING state
    // Otherwise, ACD will naturally transition: PROBING -> ANNOUNCING -> ONGOING
    if (!probe_was_pending) {
        // IP already assigned - manually transition to ONGOING state for periodic defensive ARPs
        acd_stop(&s_static_ip_acd);  // Stop current state first
        s_static_ip_acd.state = ACD_STATE_ONGOING;
        s_static_ip_acd.ipaddr = ctx->ip;
        s_static_ip_acd.sent_num = 0;
        s_static_ip_acd.lastconflict = 0;
        s_static_ip_acd.num_conflicts = 0;
        
        // Re-add to netif's acd_list so timer processes it
        acd_add(ctx->netif, &s_static_ip_acd, tcpip_acd_conflict_callback);
        
        // Set activity = 1 (OngoingDetection) since we're entering ONGOING state
        CipTcpIpSetLastAcdActivity(1);
        
        // Set ttw to defense interval so timer counts down before first probe
#ifdef CONFIG_OPENER_ACD_PERIODIC_DEFEND_INTERVAL_MS
        if (CONFIG_OPENER_ACD_PERIODIC_DEFEND_INTERVAL_MS > 0) {
            const uint16_t timer_interval_ms = 100;
            s_static_ip_acd.ttw = (uint16_t)((CONFIG_OPENER_ACD_PERIODIC_DEFEND_INTERVAL_MS + timer_interval_ms - 1) / timer_interval_ms);
        } else {
            s_static_ip_acd.ttw = 0;
        }
#else
        s_static_ip_acd.ttw = 100;  // Default 10 seconds
#endif
    }
    // If probe_was_pending, ACD is already running via acd_start() - don't stop it
    // It will naturally transition: PROBING -> ANNOUNCING -> ONGOING
    ctx->err = ERR_OK;
    
    // Free heap-allocated context (allocated on heap to prevent stack corruption)
    free(ctx);
}

/**
 * @brief ACD stop callback (executes on tcpip thread)
 * 
 * Stops ACD client. Called via tcpip_callback_with_block() to ensure
 * thread-safe execution on the tcpip thread.
 * 
 * @param arg Unused (cast to void*)
 */
static void tcpip_acd_stop_cb(void *arg) {
    (void)arg;
    acd_stop(&s_static_ip_acd);
}

/**
 * @brief Perform Address Conflict Detection (ACD) for static IP
 * 
 * Implements RFC 5227 compliant ACD for static IP addresses. Coordinates
 * the ACD probe sequence, registration, and callback handling.
 * 
 * This function:
 * - Registers ACD client with lwIP ACD module
 * - Starts ACD probe sequence (3 probes with configurable intervals)
 * - Waits for ACD completion (probe phase ~600-800ms)
 * - Returns true if IP is safe to use, false if conflict detected
 * 
 * @param netif lwIP netif structure (must be valid and have MAC address)
 * @param ip IP address to check for conflicts
 * @return true if IP is safe to use (no conflict), false if conflict detected
 * 
 * @note This function blocks for up to 2 seconds waiting for ACD completion
 * @note ACD probe sequence: PROBE_WAIT → PROBING → ANNOUNCE_WAIT → ANNOUNCING → ONGOING
 * @note Uses semaphores and callbacks for thread-safe operation
 */
static bool tcpip_perform_acd(struct netif *netif, const ip4_addr_t *ip) {
    if (!g_tcpip.select_acd) {
        g_tcpip.status &= ~(kTcpipStatusAcdStatus | kTcpipStatusAcdFault);
        CipTcpIpSetLastAcdActivity(0);
        return true;
    }

    if (netif == NULL) {
        ESP_LOGW(TAG, "ACD requested but no netif available");
        g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
        CipTcpIpSetLastAcdActivity(3);
        return false;
    }

    if (s_acd_sem == NULL) {
        s_acd_sem = xSemaphoreCreateBinary();
        if (s_acd_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create ACD semaphore");
            g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
            CipTcpIpSetLastAcdActivity(3);
            return false;
        }
    }

    while (xSemaphoreTake(s_acd_sem, 0) == pdTRUE) {
        /* flush any stale signals */
    }

    // Check if probe is still pending before creating context (prevents invalid context if cancelled)
    if (!s_acd_probe_pending) {
        ESP_LOGD(TAG, "tcpip_perform_acd: ACD probe no longer pending - skipping");
        return true;  // Return true to allow IP assignment (ACD was cancelled or completed)
    }

    // Initialize callback tracking: timeout without callback means probe sequence hasn't completed yet
    // Only explicit callback (ACD_IP_OK, ACD_RESTART_CLIENT, or ACD_DECLINE) indicates completion
    s_acd_callback_received = false;
    s_acd_last_state = ACD_IP_OK;  // Default assumption, but won't be used unless callback_received is true
    CipTcpIpSetLastAcdActivity(2);

    // Verify netif is still valid (may have been invalidated)
    if (netif == NULL) {
        ESP_LOGW(TAG, "tcpip_perform_acd: netif became NULL - ACD cancelled");
        return true;  // Return true to allow IP assignment (can't perform ACD without netif)
    }

    // Allocate context on heap: tcpip_callback_with_block executes asynchronously,
    // so stack-allocated context would be corrupted
    AcdStartContext *ctx = (AcdStartContext *)malloc(sizeof(AcdStartContext));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "tcpip_perform_acd: Failed to allocate ACD context");
        g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
        CipTcpIpSetLastAcdActivity(3);
        return false;
    }
    
    ctx->netif = netif;
    ctx->ip = *ip;
    ctx->err = ERR_OK;

    ESP_LOGD(TAG, "tcpip_perform_acd: Registering ACD client for IP " IPSTR, IP2STR(ip));
    
    // Create registration semaphore to wait for callback to complete registration
    if (s_acd_registration_sem == NULL) {
        s_acd_registration_sem = xSemaphoreCreateBinary();
        if (s_acd_registration_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create ACD registration semaphore");
            free(ctx);
            g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
            CipTcpIpSetLastAcdActivity(3);
            return false;
        }
    }
    
    // Clear any stale signals
    while (xSemaphoreTake(s_acd_registration_sem, 0) == pdTRUE) {
        /* flush any stale signals */
    }
    
    // Try direct registration first (faster), fallback to callback if needed
    if (!s_acd_registered) {
        ESP_LOGD(TAG, "tcpip_perform_acd: Attempting direct ACD registration");
        netif->acd_list = NULL;
        memset(&s_static_ip_acd, 0, sizeof(s_static_ip_acd));
        err_t add_err = acd_add(netif, &s_static_ip_acd, tcpip_acd_conflict_callback);
        if (add_err == ERR_OK) {
            s_acd_registered = true;
            ESP_LOGD(TAG, "tcpip_perform_acd: Direct ACD registration succeeded");
            free(ctx);  // Free context since we didn't use callback
        } else {
            ESP_LOGW(TAG, "tcpip_perform_acd: Direct registration failed (err=%d), trying callback", (int)add_err);
            // Fall through to callback method
        }
    }
    
    // If direct registration failed, try via callback (ensures thread safety)
    if (!s_acd_registered) {
        ESP_LOGD(TAG, "tcpip_perform_acd: Registering ACD client via callback");
        err_t callback_err = tcpip_callback_with_block(tcpip_acd_start_cb, ctx, 1);
        // ctx is now freed by the callback
        
        if (callback_err != ERR_OK) {
            ESP_LOGE(TAG, "Failed to register ACD client (callback_err=%d)", (int)callback_err);
            g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
            CipTcpIpSetLastAcdActivity(3);
            return false;
        }
        
        // Wait for registration callback to complete (ensures s_acd_registered is set)
        TickType_t registration_timeout = pdMS_TO_TICKS(500);  // 500ms timeout
        if (xSemaphoreTake(s_acd_registration_sem, registration_timeout) != pdTRUE) {
            ESP_LOGW(TAG, "ACD registration callback timed out - trying direct registration as fallback");
            // Last resort: try direct registration again
            if (!s_acd_registered) {
                netif->acd_list = NULL;
                memset(&s_static_ip_acd, 0, sizeof(s_static_ip_acd));
                err_t add_err = acd_add(netif, &s_static_ip_acd, tcpip_acd_conflict_callback);
                if (add_err == ERR_OK) {
                    s_acd_registered = true;
                    ESP_LOGI(TAG, "tcpip_perform_acd: Fallback direct registration succeeded");
                } else {
                    ESP_LOGE(TAG, "ACD registration failed via both callback and direct methods");
                    g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
                    CipTcpIpSetLastAcdActivity(3);
                    return false;
                }
            }
        }
        
        if (!s_acd_registered) {
            ESP_LOGE(TAG, "ACD registration callback completed but registration failed");
            g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
            CipTcpIpSetLastAcdActivity(3);
            return false;
        }
    }
    
    // Start ACD probe directly (we're on tcpip thread or direct registration succeeded)
    if (s_acd_probe_pending && s_acd_registered) {
        ESP_LOGD(TAG, "tcpip_perform_acd: Starting ACD probe for IP " IPSTR, IP2STR(ip));
        err_t acd_start_err = acd_start(netif, &s_static_ip_acd, *ip);
        if (acd_start_err == ERR_OK) {
            ESP_LOGD(TAG, "tcpip_perform_acd: ACD probe started");
        } else {
            ESP_LOGE(TAG, "tcpip_perform_acd: acd_start() failed with err=%d", (int)acd_start_err);
            // Try via callback as fallback
            AcdStartProbeContext *probe_ctx = (AcdStartProbeContext *)malloc(sizeof(AcdStartProbeContext));
            if (probe_ctx == NULL) {
                ESP_LOGE(TAG, "Failed to allocate probe context");
                g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
                CipTcpIpSetLastAcdActivity(3);
                return false;
            }
            
            probe_ctx->netif = netif;
            probe_ctx->ip = *ip;
            probe_ctx->err = ERR_OK;
            
            err_t callback_err = tcpip_callback_with_block(acd_start_probe_cb, probe_ctx, 1);
            if (callback_err != ERR_OK) {
                ESP_LOGE(TAG, "tcpip_perform_acd: acd_start() callback failed (callback_err=%d)", 
                         (int)callback_err);
                free(probe_ctx);
                g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
                CipTcpIpSetLastAcdActivity(3);
                return false;
            }
            ESP_LOGI(TAG, "tcpip_perform_acd: ACD probe started via callback");
        }
    } else {
        ESP_LOGW(TAG, "tcpip_perform_acd: Cannot start ACD probe - probe_pending=%d, registered=%d", 
                 s_acd_probe_pending, s_acd_registered);
    }

    // Wait for ACD to complete - probe phase takes ~600-800ms (3 probes × 200ms + wait times)
    // Announce phase takes ~8s (4 announcements × 2s), but we can assign IP after probes complete
    // Total probe phase: PROBE_WAIT (0-200ms) + 3 probes (200ms each) + ANNOUNCE_WAIT (2000ms) = ~2.8-3s max
    // But we can assign after probe phase completes, so wait ~1.5s for probes + initial announce
    TickType_t wait_ticks = pdMS_TO_TICKS(2000);  // Increased from 500ms to allow full probe sequence

    ESP_LOGD(TAG, "Waiting for ACD probe sequence to complete (timeout: 2000ms)...");
    if (xSemaphoreTake(s_acd_sem, wait_ticks) == pdTRUE) {
        ESP_LOGI(TAG, "ACD completed with state=%d", (int)s_acd_last_state);
        if (s_acd_last_state == ACD_IP_OK) {
            CipTcpIpSetLastAcdActivity(0);
            return true;
        }
        // If we got a callback but not ACD_IP_OK, it might be a conflict
        if (s_acd_last_state == ACD_DECLINE || s_acd_last_state == ACD_RESTART_CLIENT) {
            ESP_LOGE(TAG, "ACD detected conflict (state=%d) - IP should not be assigned", (int)s_acd_last_state);
            CipTcpIpSetLastAcdActivity(3);
            return false;
        }
    } else if (s_acd_callback_received && s_acd_last_state == ACD_IP_OK) {
        // MODIFICATION: ACD callback was received but semaphore wait timed out
        // Added by: Adam G. Sweeney <agsweeney@gmail.com>
        // This is OK - callback set state to IP_OK, so we can safely continue
        // The timeout occurred because callback came after semaphore wait started,
        // but the state change confirms ACD completed successfully
        ESP_LOGI(TAG, "ACD callback received (state=IP_OK) - semaphore timeout was harmless, continuing with IP assignment");
        CipTcpIpSetLastAcdActivity(0);
        return true;
    }

    // Timeout - check if callback set conflict state during wait
    // Only explicit ACD_DECLINE/ACD_RESTART_CLIENT from callback indicates conflict
    // Timeout without callback means no conflict (s_acd_last_state remains ACD_IP_OK)
    if (s_acd_last_state == ACD_RESTART_CLIENT || s_acd_last_state == ACD_DECLINE) {
        // This state only occurs if callback was received, so it's a real conflict
        ESP_LOGE(TAG, "ACD conflict detected during probe phase (state=%d) - IP should not be assigned", (int)s_acd_last_state);
        CipTcpIpSetLastAcdActivity(3);
        tcpip_callback_with_block(tcpip_acd_stop_cb, NULL, 1);
        return false;
    }
    
    // Timeout without callback - ACD probe sequence is still in progress
    // Don't assign IP until callback confirms completion (ACD_IP_OK)
    // The callback will fire when announce phase completes (~6-10 seconds total)
    // Return true here to indicate "no conflict detected yet, waiting for callback"
    // This is different from returning false (which indicates actual conflict)
    ESP_LOGI(TAG, "ACD probe wait timed out (state=%d) - callback not received yet (probe sequence still running)", (int)s_acd_last_state);
    ESP_LOGI(TAG, "Note: ACD probe sequence can take 6-10 seconds (probes + announcements). Waiting for callback...");
    ESP_LOGI(TAG, "IP assignment will occur when ACD_IP_OK callback is received.");
    // Return true to indicate "no conflict, but waiting for callback to assign IP"
    // The callback will trigger IP assignment when it fires (see tcpip_acd_conflict_callback)
    return true;
}

static void tcpip_try_pending_acd(esp_netif_t *netif, struct netif *lwip_netif) {
    ESP_LOGI(TAG, "tcpip_try_pending_acd: called - probe_pending=%d, netif=%p, lwip_netif=%p", 
             s_acd_probe_pending, netif, lwip_netif);
    if (!s_acd_probe_pending || netif == NULL || lwip_netif == NULL) {
        ESP_LOGW(TAG, "tcpip_try_pending_acd: Skipping - probe_pending=%d, netif=%p, lwip_netif=%p", 
                 s_acd_probe_pending, netif, lwip_netif);
        return;
    }
    if (!netif_has_valid_hwaddr(lwip_netif)) {
        ESP_LOGI(TAG, "ACD deferred until MAC address is available");
        return;
    }
    // Check if link is actually up - sometimes netif_is_link_up() can be delayed
    // Use a small delay to allow netif to fully initialize after ETHERNET_EVENT_CONNECTED
    if (!netif_is_link_up(lwip_netif)) {
        ESP_LOGI(TAG, "ACD deferred until link is up (link status: %d) - will retry", netif_is_link_up(lwip_netif));
        // Note: "invalid static ip" error from esp_netif_handlers is expected and harmless.
        // IP hasn't been assigned yet (waiting for ACD). Error disappears once IP is assigned.
        // Retry after short delay - link should be up soon after ETHERNET_EVENT_CONNECTED
        sys_timeout(100, tcpip_retry_acd_deferred, netif);
        return;
    }
    ESP_LOGI(TAG, "tcpip_try_pending_acd: All conditions met, starting ACD...");

    /* Legacy ACD flow: Perform ACD BEFORE setting IP (better conflict detection) */
    ESP_LOGI(TAG, "Using legacy ACD mode - ACD runs before IP assignment");
    ip4_addr_t desired_ip = { .addr = s_pending_static_ip_cfg.ip.addr };
    CipTcpIpSetLastAcdActivity(2);
    ESP_LOGD(TAG, "Legacy ACD: Starting probe sequence for IP " IPSTR " BEFORE IP assignment", IP2STR(&desired_ip));
    
    // Run ACD BEFORE assigning IP to ensure conflicts are detected first
    (void)tcpip_perform_acd(lwip_netif, &desired_ip);
    
    // Check if callback was received and indicates conflict
    if (s_acd_callback_received && (s_acd_last_state == ACD_DECLINE || s_acd_last_state == ACD_RESTART_CLIENT)) {
        ESP_LOGE(TAG, "ACD conflict detected for " IPSTR " - NOT assigning IP", IP2STR(&desired_ip));
        ESP_LOGW(TAG, "IP assignment cancelled due to ACD conflict");
        g_tcpip.status |= kTcpipStatusAcdStatus | kTcpipStatusAcdFault;
        CipTcpIpSetLastAcdActivity(3);
        s_acd_probe_pending = false;
        // Stop ACD and cancel any pending retry timers
        tcpip_callback_with_block(tcpip_acd_stop_cb, NULL, 1);
        // Don't assign IP if conflict detected
        return;
    }
    
    // If callback was received with ACD_IP_OK, assign IP now
    // Otherwise (timeout without callback), IP will be assigned when callback fires
    if (s_acd_callback_received && s_acd_last_state == ACD_IP_OK) {
        ESP_LOGI(TAG, "Legacy ACD: No conflict detected - assigning IP " IPSTR, IP2STR(&desired_ip));
        ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &s_pending_static_ip_cfg));
        opener_configure_dns(netif);
        s_acd_probe_pending = false;
    } else {
        // Timeout without callback - probe sequence still running
        // IP will be assigned when ACD_IP_OK callback fires (see tcpip_acd_conflict_callback)
        ESP_LOGI(TAG, "Legacy ACD: Probe sequence in progress - IP will be assigned when callback fires");
    }
    
    // ACD_IP_OK callback fires AFTER announce phase completes, which means ACD is already in ONGOING state.
    // The ACD timer will naturally transition: PROBE_WAIT → PROBING → ANNOUNCE_WAIT → ANNOUNCING → ONGOING
    // So we don't need to manually transition - ACD is already in ONGOING state and will send periodic defensive ARPs.
    // Just set activity = 1 to indicate ongoing defense phase.
    CipTcpIpSetLastAcdActivity(1);
    ESP_LOGD(TAG, "Legacy ACD: ACD is in ONGOING state (callback fired after announce phase), periodic defense active");
    
    // Cancel any pending retry timers (retry handler checks s_acd_probe_pending and skips gracefully)
}

static void tcpip_retry_acd_deferred(void *arg) {
    esp_netif_t *netif = (esp_netif_t *)arg;
    if (netif == NULL) {
        ESP_LOGW(TAG, "tcpip_retry_acd_deferred: NULL netif - retry timer fired after cleanup");
        return;
    }
    
    // Check if probe is still pending (prevents retry after IP assignment or ACD completion)
    if (!s_acd_probe_pending) {
        ESP_LOGD(TAG, "tcpip_retry_acd_deferred: ACD probe no longer pending (IP likely assigned) - skipping retry");
        return;
    }
    
    // Check if ACD is already running - don't retry if it's already started
    if (s_acd_registered) {
        ESP_LOGD(TAG, "tcpip_retry_acd_deferred: ACD already running (registered=%d) - skipping retry", s_acd_registered);
        return;
    }
    
    struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(netif);
    if (lwip_netif != NULL) {
        ESP_LOGI(TAG, "tcpip_retry_acd_deferred: Retrying ACD start");
        tcpip_try_pending_acd(netif, lwip_netif);
    } else {
        ESP_LOGW(TAG, "tcpip_retry_acd_deferred: NULL lwip_netif - netif may not be fully initialized yet");
    }
}

#if CONFIG_OPENER_ACD_RETRY_ENABLED
/**
 * ACD Retry Logic
 * 
 * When a conflict is detected, removes IP address and schedules retry after delay.
 * Retry restarts the ACD probe sequence. Configurable max attempts and delay.
 */
static void tcpip_acd_retry_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    
    // Minimize stack usage: timer callbacks run in timer service task with limited stack
    // Set flag and let retry happen via tcpip callback (has more stack space)
    if (s_acd_retry_netif == NULL || s_acd_retry_lwip_netif == NULL) {
        return;  // Don't log - timer task has limited stack
    }
    
    // Reset probe pending flag to allow retry
    s_acd_probe_pending = true;
    
    // Execute retry on tcpip thread (has more stack space)
    err_t err = tcpip_callback_with_block(retry_callback, NULL, 0);
    if (err != ERR_OK) {
        // If callback fails, try direct call (may fail if not on tcpip thread, but won't crash)
        // Don't log here - timer task has limited stack
        tcpip_try_pending_acd(s_acd_retry_netif, s_acd_retry_lwip_netif);
    }
}

static void tcpip_acd_start_retry(esp_netif_t *netif, struct netif *lwip_netif) {
    if (netif == NULL || lwip_netif == NULL) {
        ESP_LOGE(TAG, "ACD retry: Invalid netif pointers");
        return;
    }
    
    // Increment retry count
    s_acd_retry_count++;
    
    // Store netif pointers for retry timer callback
    s_acd_retry_netif = netif;
    s_acd_retry_lwip_netif = lwip_netif;
    
    // Remove IP address (set to 0.0.0.0)
    esp_netif_ip_info_t zero_ip = {0};
    esp_err_t err = esp_netif_set_ip_info(netif, &zero_ip);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ACD retry: Failed to remove IP address: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "ACD retry: IP address removed (set to 0.0.0.0)");
    }
    
    // Stop ACD monitoring
    if (s_acd_registered) {
        acd_stop(&s_static_ip_acd);
        s_acd_registered = false;
    }
    
    // Create retry timer if it doesn't exist
    if (s_acd_retry_timer == NULL) {
        s_acd_retry_timer = xTimerCreate(
            "acd_retry",
            pdMS_TO_TICKS(CONFIG_OPENER_ACD_RETRY_DELAY_MS),
            pdFALSE,  // One-shot timer
            NULL,
            tcpip_acd_retry_timer_callback
        );
        
        if (s_acd_retry_timer == NULL) {
            ESP_LOGE(TAG, "ACD retry: Failed to create retry timer");
            return;
        }
    }
    
    // Reset timer to delay value and start it
    xTimerChangePeriod(s_acd_retry_timer, 
                       pdMS_TO_TICKS(CONFIG_OPENER_ACD_RETRY_DELAY_MS),
                       portMAX_DELAY);
    xTimerStart(s_acd_retry_timer, portMAX_DELAY);
    
    ESP_LOGI(TAG, "ACD retry: Timer started - will retry in %dms", CONFIG_OPENER_ACD_RETRY_DELAY_MS);
}
#endif /* CONFIG_OPENER_ACD_RETRY_ENABLED */

#if !LWIP_IPV4 || !LWIP_ACD
/**
 * @brief Stub ACD implementation when ACD is not available
 * 
 * Returns false if ACD was requested but not supported by lwIP configuration.
 * 
 * @param netif Unused (cast to void)
 * @param ip Unused (cast to void)
 * @return false if ACD was requested, true otherwise
 */
static bool tcpip_perform_acd(struct netif *netif, const ip4_addr_t *ip) {
    (void)netif;
    (void)ip;
    if (g_tcpip.select_acd) {
        ESP_LOGW(TAG, "ACD requested but not supported by lwIP configuration");
    }
    g_tcpip.status &= ~(kTcpipStatusAcdStatus | kTcpipStatusAcdFault);
    return true;
}
#endif /* !LWIP_IPV4 || !LWIP_ACD */

static void configure_netif_from_tcpip(esp_netif_t *netif) {
    if (netif == NULL) {
        return;
    }

    struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(netif);

    if (tcpip_config_uses_dhcp()) {
        esp_netif_dhcpc_stop(netif);
        esp_netif_dhcpc_start(netif);
    } else {
        esp_netif_ip_info_t ip_info = {0};
        ip_info.ip.addr = g_tcpip.interface_configuration.ip_address;
        ip_info.netmask.addr = g_tcpip.interface_configuration.network_mask;
        ip_info.gw.addr = g_tcpip.interface_configuration.gateway;
        esp_netif_dhcpc_stop(netif);

        if (ip_info_has_static_address(&ip_info)) {
            /* Legacy ACD mode: Check if ACD is enabled BEFORE setting IP */
            if (g_tcpip.select_acd) {
                /* ACD enabled - defer IP assignment until ACD completes */
                /* IP will be set after ACD probe completes (see tcpip_try_pending_acd) */
                ESP_LOGI(TAG, "Legacy ACD enabled - IP assignment deferred until ACD completes");
            } else {
                /* ACD disabled - set IP immediately */
                ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
                opener_configure_dns(netif);
            }
        } else {
            ESP_LOGW(TAG, "Static configuration missing IP/mask; attempting AutoIP fallback");
#if CONFIG_LWIP_AUTOIP
            if (lwip_netif != NULL && netifapi_autoip_start(lwip_netif) == ERR_OK) {
                ESP_LOGI(TAG, "AutoIP started successfully");
                g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
                g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
                g_tcpip.interface_configuration.ip_address = 0;
                g_tcpip.interface_configuration.network_mask = 0;
                g_tcpip.interface_configuration.gateway = 0;
                g_tcpip.interface_configuration.name_server = 0;
                g_tcpip.interface_configuration.name_server_2 = 0;
                NvTcpipStore(&g_tcpip);
                return;
            }
            ESP_LOGE(TAG, "AutoIP start failed; falling back to DHCP");
#endif
            ESP_LOGW(TAG, "Switching interface to DHCP due to invalid static configuration");
            g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
            g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
            NvTcpipStore(&g_tcpip);
            ESP_ERROR_CHECK(esp_netif_dhcpc_start(netif));
            return;
        }

#if LWIP_IPV4 && LWIP_ACD
        if (g_tcpip.select_acd) {
            /* ACD enabled - use deferred assignment */
            s_pending_static_ip_cfg = ip_info;
            s_acd_probe_pending = true;
            CipTcpIpSetLastAcdActivity(1);
            ESP_LOGI(TAG, "ACD path: select_acd=%d, lwip_netif=%p", 
                     g_tcpip.select_acd ? 1 : 0,
                     (void *)lwip_netif);
            if (lwip_netif != NULL) {
                ESP_LOGI(TAG, "Using legacy ACD for static IP");
                tcpip_try_pending_acd(netif, lwip_netif);
            }
        } else {
            /* ACD disabled - set IP immediately */
            CipTcpIpSetLastAcdActivity(0);
            s_acd_probe_pending = false;
            ESP_LOGI(TAG, "ACD disabled - setting static IP immediately");
            ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
            opener_configure_dns(netif);
        }
#else
        ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
        opener_configure_dns(netif);
#endif
    }

    configure_hostname(netif);
    g_tcpip.status |= 0x01;
    g_tcpip.status &= ~kTcpipStatusIfaceCfgPend;
}

static void ethernet_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    esp_netif_t *eth_netif = (esp_netif_t *)arg;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
               mac_addr[0], mac_addr[1], mac_addr[2],
               mac_addr[3], mac_addr[4], mac_addr[5]);
        ESP_ERROR_CHECK(esp_netif_set_mac(eth_netif, mac_addr));
        #if LWIP_IPV4 && LWIP_ACD
        if (!tcpip_config_uses_dhcp()) {
            struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(eth_netif);
            tcpip_try_pending_acd(eth_netif, lwip_netif);
            // Note: Retry timer is only scheduled inside tcpip_try_pending_acd if ACD is deferred
        }
        #endif
        ScaleApplicationNotifyLinkUp();
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        #if LWIP_IPV4 && LWIP_ACD
        tcpip_callback_with_block(tcpip_acd_stop_cb, NULL, 1);
        #endif
        s_opener_initialized = false;
        s_services_initialized = false;  // Allow re-initialization when link comes back up
        ScaleApplicationNotifyLinkDown();
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    
    // Create mutex on first call if needed
    if (s_netif_mutex == NULL) {
        s_netif_mutex = xSemaphoreCreateMutex();
        if (s_netif_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create netif mutex");
            return;
        }
    }
    
    // Take mutex to protect s_netif access
    if (xSemaphoreTake(s_netif_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take netif mutex");
        return;
    }
    
    if (s_netif == NULL) {
        for (struct netif *netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif)) {
                s_netif = netif;
                break;
            }
        }
    }
    
    struct netif *netif_to_use = s_netif;
    xSemaphoreGive(s_netif_mutex);
    
    // Create NAU7802 mutexes if not already created (only once)
    if (s_nau7802_mutex == NULL) {
        s_nau7802_mutex = xSemaphoreCreateMutex();
        if (s_nau7802_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create NAU7802 device mutex");
        } else {
            ESP_LOGI(TAG, "NAU7802 device mutex created");
        }
    }
    if (s_nau7802_state_mutex == NULL) {
        s_nau7802_state_mutex = xSemaphoreCreateMutex();
        if (s_nau7802_state_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create NAU7802 state mutex");
        } else {
            ESP_LOGI(TAG, "NAU7802 state mutex created");
        }
    }
    
    if (netif_to_use != NULL) {
        ScaleApplicationSetActiveNetif(netif_to_use);
        
        // Initialize services only once (IP_EVENT_ETH_GOT_IP can fire multiple times)
        if (!s_services_initialized) {
            opener_init(netif_to_use);
            s_opener_initialized = true;
            ScaleApplicationNotifyLinkUp();
            
            // Initialize OTA manager
            if (!ota_manager_init()) {
                ESP_LOGW(TAG, "Failed to initialize OTA manager");
            }
            
            // Initialize and start Web UI
            if (!webui_init()) {
                ESP_LOGW(TAG, "Failed to initialize Web UI");
            }
            
            // ModbusTCP is always enabled
            if (!modbus_tcp_init()) {
                ESP_LOGW(TAG, "Failed to initialize ModbusTCP");
            } else {
                if (!modbus_tcp_start()) {
                    ESP_LOGW(TAG, "Failed to start ModbusTCP server");
                } else {
                    ESP_LOGI(TAG, "ModbusTCP server started");
                }
            }
            
            // Start NAU7802 scale reading task if initialized
            // Delete old task if it exists (e.g., on reinitialization)
            if (s_nau7802_task_handle != NULL) {
                vTaskDelete(s_nau7802_task_handle);
                s_nau7802_task_handle = NULL;
                ESP_LOGI(TAG, "Deleted old NAU7802 task");
            }
            
            if (s_nau7802_initialized) {
                xTaskCreate(nau7802_scale_task, "nau7802_task", 4096, NULL, 5, &s_nau7802_task_handle);
                if (s_nau7802_task_handle == NULL) {
                    ESP_LOGW(TAG, "Failed to create NAU7802 task");
                } else {
                    ESP_LOGI(TAG, "NAU7802 scale reading task started");
                }
            }
            
            s_services_initialized = true;
            ESP_LOGI(TAG, "All services initialized");
        } else {
            ESP_LOGD(TAG, "Services already initialized, skipping re-initialization");
        }
    } else {
        ESP_LOGE(TAG, "Failed to find netif");
    }
}

void app_main(void)
{
    // Initialize user LED early at boot
    user_led_init();
    
    // Initialize log buffer early to capture boot logs
    // Use 32KB buffer to capture boot sequence and recent runtime logs
    if (!log_buffer_init(32 * 1024)) {
        ESP_LOGW(TAG, "Failed to initialize log buffer");
    }
    
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);
    
    // Mark the current running app as valid to allow OTA updates
    // This must be done after NVS init and before any OTA operations
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL) {
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
                ESP_LOGI(TAG, "Marking OTA image as valid");
                esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to mark app as valid: %s", esp_err_to_name(ret));
                }
            }
        }
    }
    
    (void)NvTcpipLoad(&g_tcpip);
    ESP_LOGI(TAG, "After NV load select_acd=%d", g_tcpip.select_acd);
    
    // Ensure ACD is enabled for static IP configuration
    if (!tcpip_config_uses_dhcp() && !g_tcpip.select_acd) {
        ESP_LOGW(TAG, "ACD not enabled for static IP - enabling ACD for conflict detection");
        g_tcpip.select_acd = true;
        NvTcpipStore(&g_tcpip);
        ESP_LOGI(TAG, "ACD enabled successfully");
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Ensure default configuration uses DHCP when nothing stored */
    if ((g_tcpip.config_control & kTcpipCfgCtrlMethodMask) != kTcpipCfgCtrlStaticIp &&
        (g_tcpip.config_control & kTcpipCfgCtrlMethodMask) != kTcpipCfgCtrlDhcp) {
        g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
        g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
    }
    if (!tcpip_static_config_valid()) {
        ESP_LOGW(TAG, "Invalid static configuration detected, switching to DHCP");
        g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
        g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
        g_tcpip.interface_configuration.ip_address = 0;
        g_tcpip.interface_configuration.network_mask = 0;
        g_tcpip.interface_configuration.gateway = 0;
        g_tcpip.interface_configuration.name_server = 0;
        g_tcpip.interface_configuration.name_server_2 = 0;
        g_tcpip.status &= ~(kTcpipStatusAcdStatus | kTcpipStatusAcdFault);
        NvTcpipStore(&g_tcpip);
    }
    if (tcpip_config_uses_dhcp()) {
        g_tcpip.interface_configuration.ip_address = 0;
        g_tcpip.interface_configuration.network_mask = 0;
        g_tcpip.interface_configuration.gateway = 0;
        g_tcpip.interface_configuration.name_server = 0;
        g_tcpip.interface_configuration.name_server_2 = 0;
    }

    g_tcpip.status |= 0x01;
    g_tcpip.status &= ~kTcpipStatusIfaceCfgPend;

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    ESP_ERROR_CHECK(esp_netif_set_default_netif(eth_netif));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, 
                                               &ethernet_event_handler, eth_netif));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, 
                                               &got_ip_event_handler, eth_netif));

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    
    phy_config.phy_addr = CONFIG_OPENER_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_OPENER_ETH_PHY_RST_GPIO;

    esp32_emac_config.smi_gpio.mdc_num = CONFIG_OPENER_ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_OPENER_ETH_MDIO_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    configure_netif_from_tcpip(eth_netif);
    
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    
    // Initialize I2C bus for NAU7802
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_OPENER_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_OPENER_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = system_i2c_internal_pullup_load(),
        },
    };
    esp_err_t i2c_err = i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus_handle);
    if (i2c_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(i2c_err));
        s_i2c_bus_handle = NULL;
    } else {
        ESP_LOGI(TAG, "I2C bus initialized successfully (SCL: GPIO%d, SDA: GPIO%d)", 
                 CONFIG_OPENER_I2C_SCL_GPIO, CONFIG_OPENER_I2C_SDA_GPIO);
        
        // Initialize NAU7802 if enabled
        if (system_nau7802_enabled_load()) {
            esp_err_t nau_err = nau7802_init(&s_nau7802_device, s_i2c_bus_handle, NAU7802_I2C_ADDRESS);
            if (nau_err == ESP_OK) {
                if (nau7802_is_connected(&s_nau7802_device)) {
                    // Load configuration from NVS
                    uint8_t ldo_value = system_nau7802_ldo_load();
                    uint8_t gain = system_nau7802_gain_load();
                    uint8_t sample_rate = system_nau7802_sample_rate_load();
                    uint8_t channel = system_nau7802_channel_load();
                    
                    // Initialize with defaults (nau7802_begin sets LDO=4, gain=128, sample_rate=80)
                    nau_err = nau7802_begin(&s_nau7802_device);
                    if (nau_err == ESP_OK) {
                        bool need_recal = false;
                        
                        // Apply LDO if different from default (4 = 3.3V)
                        if (ldo_value != 4) {
                            nau_err = nau7802_set_ldo(&s_nau7802_device, ldo_value);
                            if (nau_err == ESP_OK) {
                                ESP_LOGI(TAG, "NAU7802 LDO set to %d", ldo_value);
                                vTaskDelay(pdMS_TO_TICKS(250));  // Wait for LDO to stabilize
                            } else {
                                ESP_LOGW(TAG, "Failed to set NAU7802 LDO: %s", esp_err_to_name(nau_err));
                            }
                        }
                        
                        // Apply gain if different from default (7 = x128)
                        if (gain != 7) {
                            nau_err = nau7802_set_gain(&s_nau7802_device, (nau7802_gain_t)gain);
                            if (nau_err == ESP_OK) {
                                ESP_LOGI(TAG, "NAU7802 gain set to %d (x%d)", gain, 1 << gain);
                                need_recal = true;
                            } else {
                                ESP_LOGW(TAG, "Failed to set NAU7802 gain: %s", esp_err_to_name(nau_err));
                            }
                        }
                        
                        // Apply sample rate if different from default (3 = 80 SPS)
                        if (sample_rate != 3) {
                            nau_err = nau7802_set_sample_rate(&s_nau7802_device, (nau7802_sps_t)sample_rate);
                            if (nau_err == ESP_OK) {
                                const char *sps_str = (sample_rate == 0) ? "10" : (sample_rate == 1) ? "20" : 
                                                      (sample_rate == 2) ? "40" : (sample_rate == 3) ? "80" : "320";
                                ESP_LOGI(TAG, "NAU7802 sample rate set to %d (%s SPS)", sample_rate, sps_str);
                                need_recal = true;
                            } else {
                                ESP_LOGW(TAG, "Failed to set NAU7802 sample rate: %s", esp_err_to_name(nau_err));
                            }
                        }
                        
                        // Apply channel if different from default (0 = Channel 1)
                        if (channel != 0) {
                            nau_err = nau7802_set_channel(&s_nau7802_device, (nau7802_channel_t)channel);
                            if (nau_err == ESP_OK) {
                                ESP_LOGI(TAG, "NAU7802 channel set to %d (Channel %d)", channel, channel + 1);
                            } else {
                                ESP_LOGW(TAG, "Failed to set NAU7802 channel: %s", esp_err_to_name(nau_err));
                            }
                        }
                        
                        // Recalibrate AFE if gain or sample rate changed
                        if (need_recal) {
                            ESP_LOGI(TAG, "Recalibrating NAU7802 AFE due to gain/sample rate change");
                            nau_err = nau7802_calibrate_af(&s_nau7802_device);
                            if (nau_err != ESP_OK) {
                                ESP_LOGW(TAG, "NAU7802 AFE recalibration failed: %s", esp_err_to_name(nau_err));
                            }
                        }
                        
                        // Load calibration data from NVS
                        float cal_factor = system_nau7802_calibration_factor_load();
                        float zero_offset = system_nau7802_zero_offset_load();
                        if (cal_factor > 0.0f) {
                            nau7802_set_calibration_factor(&s_nau7802_device, cal_factor);
                        }
                        if (zero_offset != 0.0f) {
                            nau7802_set_zero_offset(&s_nau7802_device, zero_offset);
                        }
                        
                        // Set initialization flag with mutex protection
                        if (s_nau7802_state_mutex != NULL) {
                            xSemaphoreTake(s_nau7802_state_mutex, portMAX_DELAY);
                            s_nau7802_initialized = true;
                            xSemaphoreGive(s_nau7802_state_mutex);
                        } else {
                            s_nau7802_initialized = true;
                        }
                        ESP_LOGI(TAG, "NAU7802 initialized successfully");
                    } else {
                        ESP_LOGE(TAG, "NAU7802 begin() failed: %s", esp_err_to_name(nau_err));
                    }
                } else {
                    ESP_LOGW(TAG, "NAU7802 not connected on I2C bus");
                }
            } else {
                ESP_LOGE(TAG, "NAU7802 init() failed: %s", esp_err_to_name(nau_err));
            }
        } else {
            ESP_LOGI(TAG, "NAU7802 is disabled in configuration");
        }
    }
}

// NAU7802 access functions for web API
nau7802_t* scale_application_get_nau7802_handle(void)
{
    bool initialized = false;
    if (s_nau7802_state_mutex != NULL) {
        xSemaphoreTake(s_nau7802_state_mutex, portMAX_DELAY);
        initialized = s_nau7802_initialized;
        xSemaphoreGive(s_nau7802_state_mutex);
    } else {
        initialized = s_nau7802_initialized;
    }
    return initialized ? &s_nau7802_device : NULL;
}

bool scale_application_is_nau7802_initialized(void)
{
    bool initialized = false;
    if (s_nau7802_state_mutex != NULL) {
        xSemaphoreTake(s_nau7802_state_mutex, portMAX_DELAY);
        initialized = s_nau7802_initialized;
        xSemaphoreGive(s_nau7802_state_mutex);
    } else {
        initialized = s_nau7802_initialized;
    }
    return initialized;
}

// Get NAU7802 device mutex for API handlers
SemaphoreHandle_t scale_application_get_nau7802_mutex(void)
{
    return s_nau7802_mutex;
}

// NAU7802 scale reading task - updates Assembly 100 with scale data
static void nau7802_scale_task(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t update_interval = pdMS_TO_TICKS(100);  // 100ms = 10 Hz update rate
    const TickType_t config_reload_interval = pdMS_TO_TICKS(5000);  // Reload config every 5s
    TickType_t last_config_reload = xTaskGetTickCount();
    
    uint8_t byte_offset = system_nau7802_byte_offset_load();
    uint8_t average_samples = system_nau7802_average_load();
    
    ESP_LOGI(TAG, "NAU7802 scale task started, byte offset: %d, average samples: %d", byte_offset, average_samples);
    
    while (1) {
        // Reload configuration periodically to pick up API changes
        TickType_t now = xTaskGetTickCount();
        if (now - last_config_reload >= config_reload_interval) {
            byte_offset = system_nau7802_byte_offset_load();
            average_samples = system_nau7802_average_load();
            last_config_reload = now;
            ESP_LOGD(TAG, "NAU7802 config reloaded: offset=%d, average=%d", byte_offset, average_samples);
        }
        
        // Check if initialized (with mutex protection)
        bool initialized = false;
        if (s_nau7802_state_mutex != NULL) {
            xSemaphoreTake(s_nau7802_state_mutex, portMAX_DELAY);
            initialized = s_nau7802_initialized;
            xSemaphoreGive(s_nau7802_state_mutex);
        } else {
            initialized = s_nau7802_initialized;
        }
        
        if (initialized) {
            // Check connection and get readings with device mutex protection
            bool connected = false;
            if (s_nau7802_mutex != NULL && xSemaphoreTake(s_nau7802_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                connected = nau7802_is_connected(&s_nau7802_device);
                xSemaphoreGive(s_nau7802_mutex);
            }
            
            if (connected) {
                // Get mutex for assembly access
                SemaphoreHandle_t assembly_mutex = scale_application_get_assembly_mutex();
                if (assembly_mutex != NULL && xSemaphoreTake(assembly_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    // Check if we have space in assembly (need 10 bytes: weight (4), raw (4), unit (1), status (1))
                    if (byte_offset <= 22) {  // Need 10 bytes, so max offset is 22
                        // Get device readings with mutex protection
                        bool available = false;
                        int32_t raw_reading = 0;
                        float weight_grams = 0.0f;
                        
                        if (s_nau7802_mutex != NULL && xSemaphoreTake(s_nau7802_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            available = nau7802_available(&s_nau7802_device);
                            
                            // Get scale reading (use averaging if configured)
                            if (available) {
                                if (average_samples > 1) {
                                    raw_reading = nau7802_get_average(&s_nau7802_device, average_samples, 1000);
                                } else {
                                    raw_reading = nau7802_get_reading(&s_nau7802_device);
                                }
                            }
                            
                            // Get calibrated weight in grams (use averaging if configured)
                            weight_grams = nau7802_get_weight(&s_nau7802_device, false, average_samples, 1000);
                            
                            xSemaphoreGive(s_nau7802_mutex);
                        } else {
                            ESP_LOGW(TAG, "Failed to acquire NAU7802 device mutex");
                        }
                        
                        // Convert to selected unit and scale by 100
                        uint8_t unit = system_nau7802_unit_load();
                        float weight_converted = weight_grams;
                        if (unit == 1) {
                            // Convert grams to lbs: 1 lb = 453.592 grams
                            weight_converted = weight_grams / 453.592f;
                        } else if (unit == 2) {
                            // Convert grams to kg: 1 kg = 1000 grams
                            weight_converted = weight_grams / 1000.0f;
                        }
                        // unit == 0 means grams, no conversion needed
                        
                        // Clamp weight to prevent integer overflow (int32_t range: -2147483648 to 2147483647)
                        // Scaled range: -21474836.48 to 21474836.47
                        const float max_weight = 21474836.47f;
                        const float min_weight = -21474836.48f;
                        if (weight_converted > max_weight) {
                            weight_converted = max_weight;
                            ESP_LOGW(TAG, "Weight clamped to maximum (overflow protection)");
                        } else if (weight_converted < min_weight) {
                            weight_converted = min_weight;
                            ESP_LOGW(TAG, "Weight clamped to minimum (overflow protection)");
                        }
                        
                        // Scale by 100 and convert to int32_t (e.g., 100.24 lbs = 10024)
                        int32_t weight_scaled = (int32_t)(weight_converted * 100.0f + 0.5f);  // Round to nearest
                        
                        // Pack status flags into byte: bit 0=available, bit 1=connected, bit 2=initialized
                        uint8_t status_byte = 0;
                        if (available) status_byte |= 0x01;  // Bit 0: available
                        if (connected) status_byte |= 0x02;   // Bit 1: connected
                        
                        // Check initialized flag with mutex protection
                        bool is_initialized = false;
                        if (s_nau7802_state_mutex != NULL) {
                            xSemaphoreTake(s_nau7802_state_mutex, portMAX_DELAY);
                            is_initialized = s_nau7802_initialized;
                            xSemaphoreGive(s_nau7802_state_mutex);
                        } else {
                            is_initialized = s_nau7802_initialized;
                        }
                        if (is_initialized) status_byte |= 0x04;  // Bit 2: initialized
                        // Bits 3-7: reserved
                        
                        // Write to assembly (little-endian)
                        // Bytes 0-3: Weight (int32_t, scaled by 100) in selected unit
                        // Bytes 4-7: Raw reading (int32_t)
                        // Byte 8: Unit code (0=grams, 1=lbs, 2=kg)
                        // Byte 9: Status flags (bit 0=available, bit 1=connected, bit 2=initialized)
                        memcpy(&g_assembly_data064[byte_offset], &weight_scaled, sizeof(int32_t));
                        memcpy(&g_assembly_data064[byte_offset + 4], &raw_reading, sizeof(int32_t));
                        g_assembly_data064[byte_offset + 8] = unit;
                        g_assembly_data064[byte_offset + 9] = status_byte;
                    } else {
                        ESP_LOGW(TAG, "NAU7802 byte offset %d too large for 10-byte data (max 22)", byte_offset);
                    }
                    
                    xSemaphoreGive(assembly_mutex);
                }
            }
        }
        
        vTaskDelay(update_interval);
    }
}


// User LED control functions
static void user_led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << USER_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        s_user_led_initialized = true;
        // Start blinking by default at boot
        user_led_start_flash();
        ESP_LOGI(TAG, "User LED initialized on GPIO%d (blinking by default)", USER_LED_GPIO);
    } else {
        ESP_LOGE(TAG, "Failed to initialize user LED on GPIO%d: %s", USER_LED_GPIO, esp_err_to_name(ret));
    }
}

static void user_led_set(bool on) {
    if (s_user_led_initialized) {
        gpio_set_level(USER_LED_GPIO, on ? 1 : 0);
    }
}

static void user_led_flash_task(void *pvParameters) {
    (void)pvParameters;
    const TickType_t flash_interval = pdMS_TO_TICKS(500);  // 500ms on/off
    
    while (1) {
        if (s_user_led_flash_enabled) {
            user_led_set(true);
            vTaskDelay(flash_interval);
            user_led_set(false);
            vTaskDelay(flash_interval);
        } else {
            // If flashing disabled, keep LED on and exit task
            user_led_set(true);
            vTaskDelete(NULL);
            return;
        }
    }
}

static void user_led_start_flash(void) {
    if (!s_user_led_initialized) {
        return;
    }
    
    if (s_user_led_task_handle == NULL) {
        s_user_led_flash_enabled = true;
        BaseType_t ret = xTaskCreate(
            user_led_flash_task,
            "user_led_flash",
            2048,
            NULL,
            1,  // Low priority
            &s_user_led_task_handle
        );
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "User LED: Started blinking (normal operation)");
        } else {
            ESP_LOGE(TAG, "Failed to create user LED flash task");
            s_user_led_flash_enabled = false;
        }
    }
}

static void user_led_stop_flash(void) {
    if (s_user_led_task_handle != NULL) {
        s_user_led_flash_enabled = false;
        // Wait a bit for the task to exit cleanly
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_user_led_task_handle != NULL) {
            s_user_led_task_handle = NULL;
            ESP_LOGI(TAG, "User LED: Stopped blinking (going solid for ACD conflict)");
        }
    }
}


