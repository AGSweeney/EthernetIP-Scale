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

#include "log_buffer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *TAG = "log_buffer";

#define DEFAULT_BUFFER_SIZE (16 * 1024) // 16KB default

static char *s_log_buffer = NULL;
static size_t s_buffer_size = 0;
static size_t s_write_pos = 0;
static size_t s_total_written = 0;
static bool s_wrapped = false;
static SemaphoreHandle_t s_buffer_mutex = NULL;
static bool s_enabled = false;
static int (*s_original_vprintf)(const char *fmt, va_list args) = NULL;

// Custom vprintf that captures logs to buffer
static int log_buffer_vprintf(const char *fmt, va_list args)
{
    // Capture to buffer first (before args is consumed)
    if (s_enabled && s_log_buffer && s_buffer_mutex) {
        if (xSemaphoreTake(s_buffer_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Format log message to temporary buffer
            va_list args_copy;
            va_copy(args_copy, args);
            char temp_buffer[512];
            int len = vsnprintf(temp_buffer, sizeof(temp_buffer), fmt, args_copy);
            va_end(args_copy);
            
            if (len > 0) {
                size_t bytes_to_write = (len < (int)sizeof(temp_buffer)) ? len : sizeof(temp_buffer) - 1;
                
                // Write to circular buffer
                if (s_write_pos + bytes_to_write <= s_buffer_size) {
                    // Simple case: fits in remaining buffer
                    memcpy(&s_log_buffer[s_write_pos], temp_buffer, bytes_to_write);
                    s_write_pos += bytes_to_write;
                } else {
                    // Wraps around
                    size_t first_part = s_buffer_size - s_write_pos;
                    memcpy(&s_log_buffer[s_write_pos], temp_buffer, first_part);
                    memcpy(s_log_buffer, &temp_buffer[first_part], bytes_to_write - first_part);
                    s_write_pos = bytes_to_write - first_part;
                    s_wrapped = true;
                }
                
                s_total_written += bytes_to_write;
            }
            
            xSemaphoreGive(s_buffer_mutex);
        }
    }
    
    // Call original vprintf (to UART) - args is still valid here
    int ret = 0;
    if (s_original_vprintf) {
        ret = s_original_vprintf(fmt, args);
    }
    
    return ret;
}

bool log_buffer_init(size_t buffer_size)
{
    if (s_enabled) {
        ESP_LOGW(TAG, "Log buffer already initialized");
        return true;
    }
    
    if (buffer_size == 0) {
        buffer_size = DEFAULT_BUFFER_SIZE;
    }
    
    // Create mutex
    s_buffer_mutex = xSemaphoreCreateMutex();
    if (s_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create log buffer mutex");
        return false;
    }
    
    // Allocate buffer
    s_log_buffer = (char *)malloc(buffer_size);
    if (s_log_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate log buffer (%zu bytes)", buffer_size);
        vSemaphoreDelete(s_buffer_mutex);
        s_buffer_mutex = NULL;
        return false;
    }
    
    s_buffer_size = buffer_size;
    s_write_pos = 0;
    s_total_written = 0;
    s_wrapped = false;
    memset(s_log_buffer, 0, buffer_size);
    
    // Install custom vprintf
    s_original_vprintf = esp_log_set_vprintf(log_buffer_vprintf);
    
    s_enabled = true;
    ESP_LOGI(TAG, "Log buffer initialized (%zu bytes)", buffer_size);
    
    return true;
}

size_t log_buffer_get(char *buffer, size_t buffer_size)
{
    if (!s_enabled || !s_log_buffer || !buffer || buffer_size == 0) {
        return 0;
    }
    
    if (s_buffer_mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_buffer_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    
    size_t bytes_to_copy = 0;
    
    if (s_wrapped) {
        // Buffer has wrapped - copy from write_pos to end, then from start to write_pos
        size_t first_part = s_buffer_size - s_write_pos;
        size_t second_part = s_write_pos;
        
        if (buffer_size >= s_buffer_size) {
            // Can copy entire buffer
            memcpy(buffer, &s_log_buffer[s_write_pos], first_part);
            memcpy(&buffer[first_part], s_log_buffer, second_part);
            bytes_to_copy = s_buffer_size;
        } else {
            // Copy what fits, starting from oldest data
            if (buffer_size <= first_part) {
                memcpy(buffer, &s_log_buffer[s_write_pos], buffer_size);
                bytes_to_copy = buffer_size;
            } else {
                memcpy(buffer, &s_log_buffer[s_write_pos], first_part);
                size_t remaining = buffer_size - first_part;
                if (remaining <= second_part) {
                    memcpy(&buffer[first_part], s_log_buffer, remaining);
                    bytes_to_copy = buffer_size;
                } else {
                    memcpy(&buffer[first_part], s_log_buffer, second_part);
                    bytes_to_copy = first_part + second_part;
                }
            }
        }
    } else {
        // Buffer hasn't wrapped - simple copy
        bytes_to_copy = (s_write_pos < buffer_size) ? s_write_pos : buffer_size;
        memcpy(buffer, s_log_buffer, bytes_to_copy);
    }
    
    // Ensure null termination
    if (bytes_to_copy < buffer_size) {
        buffer[bytes_to_copy] = '\0';
    } else {
        buffer[buffer_size - 1] = '\0';
        bytes_to_copy = buffer_size - 1;
    }
    
    xSemaphoreGive(s_buffer_mutex);
    
    return bytes_to_copy;
}

size_t log_buffer_get_size(void)
{
    if (!s_enabled || !s_log_buffer) {
        return 0;
    }
    
    if (s_buffer_mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(s_buffer_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    
    size_t size = s_wrapped ? s_buffer_size : s_write_pos;
    
    xSemaphoreGive(s_buffer_mutex);
    
    return size;
}

void log_buffer_clear(void)
{
    if (!s_enabled || !s_log_buffer || !s_buffer_mutex) {
        return;
    }
    
    if (xSemaphoreTake(s_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        s_write_pos = 0;
        s_total_written = 0;
        s_wrapped = false;
        memset(s_log_buffer, 0, s_buffer_size);
        xSemaphoreGive(s_buffer_mutex);
        ESP_LOGI(TAG, "Log buffer cleared");
    }
}

bool log_buffer_is_enabled(void)
{
    return s_enabled;
}

