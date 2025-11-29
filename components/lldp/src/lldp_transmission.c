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

#include "lldp_transmission.h"
#include "lldp_frame_builder.h"
#include "lldp_raw_socket.h"
#include "trace.h"
#include "opener_user_conf.h"
#include "freertos/timers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default TTL: 120 seconds (IEEE 802.1AB standard)
#define LLDP_DEFAULT_TTL_SECONDS 120

// Maximum LLDP frame size (Ethernet header + LLDPDU)
#define LLDP_MAX_FRAME_SIZE 512

static TimerHandle_t s_lldp_timer = NULL;
static bool s_lldp_enabled = true;
static uint16_t s_tx_interval_ms = OPENER_LLDP_TX_INTERVAL_MS;
static portMUX_TYPE s_tx_mutex = portMUX_INITIALIZER_UNLOCKED;

/**
 * Timer callback for periodic LLDP frame transmission
 */
static void lldp_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    
    // Minimal logging in timer callback to avoid stack overflow
    // Timer service task has limited stack (2048 bytes)
    
    // MODIFICATION: Use critical section to safely read s_lldp_enabled
    // Added by: Adam G. Sweeney <agsweeney@gmail.com>
    taskENTER_CRITICAL(&s_tx_mutex);
    bool enabled = s_lldp_enabled;
    taskEXIT_CRITICAL(&s_tx_mutex);
    
    if (!enabled) {
        return;  // LLDP disabled, don't transmit
    }
    
    if (!lldp_raw_socket_is_initialized()) {
        return;
    }
    
    // Get MAC address
    uint8_t mac_address[6];
    lldp_get_mac_address(mac_address);
    
    // Get optional TLV data
    char system_name[65];  // Max hostname length + 1
    uint8_t ip_address[4];
    
    size_t name_len = lldp_get_hostname(system_name, sizeof(system_name));
    const char *system_name_ptr = (name_len > 0) ? system_name : NULL;
    
    // Temporarily disable System Description to test if it's causing malformation
    // const char *system_description_ptr = NULL;
    char system_description[65];
    size_t desc_len = lldp_get_system_description(system_description, sizeof(system_description));
    const char *system_description_ptr = (desc_len > 0) ? system_description : NULL;
    
    int ip_result = lldp_get_ip_address(ip_address);
    const uint8_t *ip_address_ptr = (ip_result == 0) ? ip_address : NULL;
    
    // Build LLDPDU with optional TLVs
    uint8_t lldpdu[512];
    memset(lldpdu, 0, sizeof(lldpdu));  // Fully zero-initialize buffer
    size_t lldpdu_len = lldp_build_lldpdu(lldpdu, sizeof(lldpdu), 
                                          mac_address, 
                                          LLDP_DEFAULT_TTL_SECONDS,
                                          system_name_ptr,
                                          system_description_ptr,
                                          ip_address_ptr);
    if (lldpdu_len == 0) {
        return;
    }
    
    // Build complete Ethernet frame
    uint8_t frame[LLDP_MAX_FRAME_SIZE];
    memset(frame, 0, sizeof(frame));  // Fully zero-initialize buffer
    size_t frame_len = lldp_build_ethernet_frame(frame, sizeof(frame), mac_address, lldpdu, lldpdu_len);
    if (frame_len == 0) {
        return;
    }
    
    // Send frame (logging happens inside send function which runs in different context)
    lldp_raw_socket_send(frame, frame_len);
}

/**
 * Initialize LLDP periodic transmission timer
 * @param interval_ms Transmission interval in milliseconds
 * @return 0 on success, -1 on failure
 */
int lldp_transmission_init(uint16_t interval_ms) {
    if (s_lldp_timer != NULL) {
        return 0;  // Already initialized
    }
    
    // MODIFICATION: Added critical section protection
    // Added by: Adam G. Sweeney <agsweeney@gmail.com>
    taskENTER_CRITICAL(&s_tx_mutex);
    s_tx_interval_ms = interval_ms;
    taskEXIT_CRITICAL(&s_tx_mutex);
    
    // Create FreeRTOS software timer
    s_lldp_timer = xTimerCreate("LLDP_TX",
                                 pdMS_TO_TICKS(interval_ms),
                                 pdTRUE,  // Auto-reload
                                 NULL,
                                 lldp_timer_callback);
    
    if (s_lldp_timer == NULL) {
        OPENER_TRACE_ERR("Failed to create LLDP transmission timer\n");
        return -1;
    }
    
    // Start timer - use portMAX_DELAY to ensure we're in a task context
    // FreeRTOS timers must be started from a task, not during initialization
    BaseType_t result = xTimerStart(s_lldp_timer, portMAX_DELAY);
    if (result != pdPASS) {
        OPENER_TRACE_ERR("Failed to start LLDP transmission timer (result=%d)\n", result);
        xTimerDelete(s_lldp_timer, portMAX_DELAY);
        s_lldp_timer = NULL;
        return -1;
    }
    
    // Timer started successfully (logging removed for production)
    
    // Don't send immediate test frame - wait for first timer expiration
    // This gives the Ethernet driver time to be fully ready for transmission
    // Immediate frames might be dropped if sent too soon after initialization
    
    return 0;
}

/**
 * Deinitialize LLDP periodic transmission timer
 */
void lldp_transmission_deinit(void) {
    if (s_lldp_timer != NULL) {
        xTimerStop(s_lldp_timer, portMAX_DELAY);
        xTimerDelete(s_lldp_timer, portMAX_DELAY);
        s_lldp_timer = NULL;
    }
}

/**
 * Enable or disable LLDP transmission
 * @param enabled true to enable, false to disable
 * MODIFICATION: Added critical section protection to prevent race conditions
 * Added by: Adam G. Sweeney <agsweeney@gmail.com>
 */
void lldp_transmission_set_enabled(bool enabled) {
    taskENTER_CRITICAL(&s_tx_mutex);
    s_lldp_enabled = enabled;
    taskEXIT_CRITICAL(&s_tx_mutex);
    // LLDP transmission state changed (logging removed for production)
}

/**
 * Get LLDP transmission enabled state
 * @return true if enabled, false if disabled
 * MODIFICATION: Added critical section protection to prevent race conditions
 * Added by: Adam G. Sweeney <agsweeney@gmail.com>
 */
bool lldp_transmission_is_enabled(void) {
    taskENTER_CRITICAL(&s_tx_mutex);
    bool enabled = s_lldp_enabled;
    taskEXIT_CRITICAL(&s_tx_mutex);
    return enabled;
}

/**
 * Update transmission interval
 * @param interval_ms New interval in milliseconds
 * @return 0 on success, -1 on failure
 */
int lldp_transmission_set_interval(uint16_t interval_ms) {
    if (s_lldp_timer == NULL) {
        // MODIFICATION: Added critical section protection
        // Added by: Adam G. Sweeney <agsweeney@gmail.com>
        taskENTER_CRITICAL(&s_tx_mutex);
        s_tx_interval_ms = interval_ms;
        taskEXIT_CRITICAL(&s_tx_mutex);
        return 0;  // Will be used when timer is created
    }
    
    // Update timer period
    if (xTimerChangePeriod(s_lldp_timer, pdMS_TO_TICKS(interval_ms), portMAX_DELAY) != pdPASS) {
        OPENER_TRACE_ERR("Failed to update LLDP transmission interval\n");
        return -1;
    }
    
    // MODIFICATION: Added critical section protection
    // Added by: Adam G. Sweeney <agsweeney@gmail.com>
    taskENTER_CRITICAL(&s_tx_mutex);
    s_tx_interval_ms = interval_ms;
    taskEXIT_CRITICAL(&s_tx_mutex);
    // LLDP transmission interval updated (logging removed for production)
    return 0;
}

/**
 * Get current transmission interval
 * @return Interval in milliseconds
 * MODIFICATION: Added critical section protection to prevent race conditions
 * Added by: Adam G. Sweeney <agsweeney@gmail.com>
 */
uint16_t lldp_transmission_get_interval(void) {
    taskENTER_CRITICAL(&s_tx_mutex);
    uint16_t interval = s_tx_interval_ms;
    taskEXIT_CRITICAL(&s_tx_mutex);
    return interval;
}

/**
 * Manually trigger LLDP frame transmission (for testing)
 * @return 0 on success, -1 on failure
 */
int lldp_transmission_send_now(void) {
    if (!s_lldp_enabled) {
        return -1;
    }
    
    if (!lldp_raw_socket_is_initialized()) {
        return -1;
    }
    
    // Call timer callback directly
    lldp_timer_callback(NULL);
    return 0;
}

#ifdef __cplusplus
}
#endif

