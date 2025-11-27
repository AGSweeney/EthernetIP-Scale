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

#include "webui_api.h"
#include "ota_manager.h"
#include "system_config.h"
#include "driver/i2c_master.h"
#include "modbus_tcp.h"
#include "ciptcpipinterface.h"
#include "nvtcpip.h"
#include "log_buffer.h"
#include "nau7802.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for assembly access
extern uint8_t g_assembly_data064[32];
extern uint8_t g_assembly_data096[32];
extern uint8_t g_assembly_data097[10];

// Forward declaration for assembly mutex access
extern SemaphoreHandle_t scale_application_get_assembly_mutex(void);

static const char *TAG = "webui_api";

// Cache for Modbus enabled state to avoid frequent NVS reads
static bool s_cached_modbus_enabled = false;
static bool s_modbus_enabled_cached = false;

// Cache for I2C pull-up enabled state to avoid frequent NVS reads
static bool s_cached_i2c_pullup_enabled = false;
static bool s_i2c_pullup_enabled_cached = false;

// Cache for NAU7802 enabled state to avoid frequent NVS reads
static bool s_cached_nau7802_enabled = false;
static bool s_nau7802_enabled_cached = false;
static uint8_t s_cached_nau7802_byte_offset = 0;
static bool s_nau7802_byte_offset_cached = false;
static uint8_t s_cached_nau7802_unit = 1;  // Default to lbs
static bool s_nau7802_unit_cached = false;
static uint8_t s_cached_nau7802_gain = 7;  // Default to x128
static bool s_nau7802_gain_cached = false;
static uint8_t s_cached_nau7802_sample_rate = 3;  // Default to 80 SPS
static bool s_nau7802_sample_rate_cached = false;
static uint8_t s_cached_nau7802_channel = 0;  // Default to Channel 1
static bool s_nau7802_channel_cached = false;
static uint8_t s_cached_nau7802_ldo = 4;  // Default to 3.3V
static bool s_nau7802_ldo_cached = false;
static uint8_t s_cached_nau7802_average = 1;  // Default to 1 sample (no averaging)
static bool s_nau7802_average_cached = false;

// Forward declarations for NAU7802 access functions (implemented in main.c)
extern nau7802_t* scale_application_get_nau7802_handle(void);
extern bool scale_application_is_nau7802_initialized(void);
extern SemaphoreHandle_t scale_application_get_nau7802_mutex(void);

// Mutex for protecting g_tcpip structure access (shared between OpENer task and API handlers)
static SemaphoreHandle_t s_tcpip_mutex = NULL;

// Helper function to send JSON response
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json, esp_err_t status_code)
{
    char *json_str = cJSON_Print(json);
    if (json_str == NULL) {
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status_code == ESP_OK ? "200 OK" : "400 Bad Request");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(json);
    return ESP_OK;
}

// Helper function to send JSON error response
static esp_err_t send_json_error(httpd_req_t *req, const char *message, int http_status)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "error");
    cJSON_AddStringToObject(json, "message", message);
    
    char *json_str = cJSON_Print(json);
    if (json_str == NULL) {
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    if (http_status == 400) {
        httpd_resp_set_status(req, "400 Bad Request");
    } else if (http_status == 500) {
        httpd_resp_set_status(req, "500 Internal Server Error");
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
    }
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(json);
    return ESP_OK;
}

// POST /api/reboot - Reboot the device
static esp_err_t api_reboot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Reboot requested via web UI");
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Device rebooting...");
    
    esp_err_t ret = send_json_response(req, response, ESP_OK);
    
    // Give a small delay to ensure response is sent
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Reboot the device
    esp_restart();
    
    return ret; // This will never be reached
}

// POST /api/ota/update - Trigger OTA update (supports both URL and file upload)
static esp_err_t api_ota_update_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA update request received");
    
    // Check content type
    char content_type[256];
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Missing Content-Type header (ret=%d)", ret);
        return send_json_error(req, "Missing Content-Type", 400);
    }
    
    ESP_LOGI(TAG, "OTA update request, Content-Type: %s", content_type);
    
    // Handle file upload (multipart/form-data) - Use streaming to avoid memory issues
    if (strstr(content_type, "multipart/form-data") != NULL) {
        // Get content length - may be 0 if chunked transfer
        size_t content_len = req->content_len;
        ESP_LOGI(TAG, "Content-Length: %d", content_len);
        
        // Get OTA partition size to validate against actual partition capacity
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        size_t max_firmware_size = 0;
        if (update_partition != NULL) {
            max_firmware_size = update_partition->size;
            ESP_LOGI(TAG, "OTA partition size: %d bytes", max_firmware_size);
        } else {
            // Fallback to 1.5MB if partition lookup fails (matches partition table)
            max_firmware_size = 0x180000; // 1,572,864 bytes
            ESP_LOGW(TAG, "Could not determine partition size, using default: %d bytes", max_firmware_size);
        }
        
        // Validate size against partition capacity
        if (content_len > 0 && content_len > max_firmware_size) {
            ESP_LOGW(TAG, "Content length too large: %d bytes (max: %d bytes)", content_len, max_firmware_size);
            return send_json_error(req, "File too large for OTA partition", 400);
        }
        
        // Parse multipart boundary first
        const char *boundary_str = strstr(content_type, "boundary=");
        if (boundary_str == NULL) {
            ESP_LOGW(TAG, "No boundary found in Content-Type");
            return send_json_error(req, "Invalid multipart data: no boundary", 400);
        }
        boundary_str += 9; // Skip "boundary="
        
        // Extract boundary value
        char boundary[128];
        int boundary_len = 0;
        while (*boundary_str && *boundary_str != ';' && *boundary_str != ' ' && *boundary_str != '\r' && *boundary_str != '\n' && boundary_len < 127) {
            boundary[boundary_len++] = *boundary_str++;
        }
        boundary[boundary_len] = '\0';
        ESP_LOGI(TAG, "Multipart boundary: %s", boundary);
        
        // Use a small buffer to read multipart headers (64KB should be enough)
        const size_t header_buffer_size = 64 * 1024;
        char *header_buffer = malloc(header_buffer_size);
        if (header_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for header buffer");
            return send_json_error(req, "Failed to allocate memory", 500);
        }
        
        // Read enough to find the data separator (\r\n\r\n)
        size_t header_read = 0;
        bool found_separator = false;
        uint32_t header_timeout_count = 0;
        const uint32_t max_header_timeouts = 50; // Max 50 timeouts (~5 seconds at 100ms each)
        
        while (header_read < header_buffer_size - 1) {
            int ret = httpd_req_recv(req, header_buffer + header_read, header_buffer_size - header_read - 1);
            if (ret <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    header_timeout_count++;
                    if (header_timeout_count > max_header_timeouts) {
                        ESP_LOGE(TAG, "Too many timeouts reading multipart headers");
                        free(header_buffer);
                        return send_json_error(req, "Timeout reading request headers", 408);
                    }
                    continue;
                }
                ESP_LOGE(TAG, "Error reading headers: %d", ret);
                free(header_buffer);
                return send_json_error(req, "Failed to read request headers", 500);
            }
            header_timeout_count = 0; // Reset timeout counter on successful read
            header_read += ret;
            header_buffer[header_read] = '\0'; // Null terminate for string search
            
            // Look for data separator
            if (strstr(header_buffer, "\r\n\r\n") != NULL || strstr(header_buffer, "\n\n") != NULL) {
                found_separator = true;
                break;
            }
        }
        
        if (!found_separator) {
            ESP_LOGW(TAG, "Could not find data separator in multipart headers");
            free(header_buffer);
            return send_json_error(req, "Invalid multipart format: no data separator", 400);
        }
        
        // Find where data starts
        char *data_start = strstr(header_buffer, "\r\n\r\n");
        size_t header_len = 0;
        if (data_start != NULL) {
            header_len = (data_start - header_buffer) + 4;
        } else {
            data_start = strstr(header_buffer, "\n\n");
            if (data_start != NULL) {
                header_len = (data_start - header_buffer) + 2;
            } else {
                free(header_buffer);
                return send_json_error(req, "Invalid multipart format", 400);
            }
        }
        
        // Calculate how much data we already have in the buffer
        size_t data_in_buffer = header_read - header_len;
        
        // Start streaming OTA update
        // Estimate firmware size: Content-Length minus multipart headers (typically ~1KB)
        // This gives us a reasonable estimate for progress tracking
        size_t estimated_firmware_size = 0;
        if (content_len > 0) {
            // Subtract estimated multipart header overhead (boundary + headers ~1KB)
            estimated_firmware_size = (content_len > 1024) ? (content_len - 1024) : content_len;
        }
        esp_ota_handle_t ota_handle = ota_manager_start_streaming_update(estimated_firmware_size);
        if (ota_handle == 0) {
            ESP_LOGE(TAG, "Failed to start streaming OTA update - check serial logs for details");
            free(header_buffer);
            return send_json_error(req, "Failed to start OTA update. Check device logs for details.", 500);
        }
        
        // Prepare boundary strings for detection
        char start_boundary[256];
        char end_boundary[256];
        snprintf(start_boundary, sizeof(start_boundary), "--%s", boundary);
        snprintf(end_boundary, sizeof(end_boundary), "--%s--", boundary);
        
        bool done = false;
        
        // Write data we already have in buffer (check for boundary first)
        if (data_in_buffer > 0) {
            // Check if boundary is already in the initial data
            char *boundary_in_header = strstr((char *)(header_buffer + header_len), start_boundary);
            
            if (boundary_in_header != NULL) {
                // Boundary found in initial data - only write up to it
                size_t initial_to_write = boundary_in_header - (header_buffer + header_len);
                // Remove trailing \r\n
                while (initial_to_write > 0 && 
                       (header_buffer[header_len + initial_to_write - 1] == '\r' || 
                        header_buffer[header_len + initial_to_write - 1] == '\n')) {
                    initial_to_write--;
                }
                if (initial_to_write > 0) {
                    if (!ota_manager_write_streaming_chunk(ota_handle, (const uint8_t *)(header_buffer + header_len), initial_to_write)) {
                        ESP_LOGE(TAG, "Failed to write initial chunk");
                        free(header_buffer);
                        return send_json_error(req, "Failed to write firmware data", 500);
                    }
                    data_in_buffer = initial_to_write;
                } else {
                    data_in_buffer = 0;  // No data to write, boundary was at start
                }
                done = true;  // We're done, boundary found
            } else {
                // No boundary in initial data, write it all
                if (!ota_manager_write_streaming_chunk(ota_handle, (const uint8_t *)(header_buffer + header_len), data_in_buffer)) {
                    ESP_LOGE(TAG, "Failed to write initial chunk");
                    free(header_buffer);
                    return send_json_error(req, "Failed to write firmware data", 500);
                }
            }
        }
        
        free(header_buffer); // Free header buffer, we'll use a smaller chunk buffer now
        
        // If boundary was found in initial data, we're done
        if (done) {
            ESP_LOGI(TAG, "Streamed %d bytes to OTA partition", data_in_buffer);
            
            // Finish OTA update
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "status", "ok");
            cJSON_AddStringToObject(response, "message", "Firmware uploaded successfully. Finishing update and rebooting...");
            
            esp_err_t resp_err = send_json_response(req, response, ESP_OK);
            vTaskDelay(pdMS_TO_TICKS(100));
            
            if (!ota_manager_finish_streaming_update(ota_handle)) {
                ESP_LOGE(TAG, "Failed to finish streaming OTA update");
                return ESP_FAIL;
            }
            return resp_err;
        }
        
        // Stream remaining data in chunks (64KB chunks)
        const size_t chunk_size = 64 * 1024;
        char *chunk_buffer = malloc(chunk_size);
        if (chunk_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate chunk buffer");
            // Abort OTA update since we can't continue
            esp_ota_abort(ota_handle);
            return send_json_error(req, "Failed to allocate memory", 500);
        }
        
        size_t total_written = data_in_buffer;
        uint32_t timeout_count = 0;
        const uint32_t max_timeouts = 100; // Max 100 timeouts (~10 seconds at 100ms each)
        
        // Calculate expected firmware size for validation
        size_t expected_firmware_bytes = 0;
        if (content_len > 0) {
            // Account for multipart overhead (boundary + headers, typically ~1KB)
            expected_firmware_bytes = (content_len > 1024) ? (content_len - 1024) : content_len;
        }
        
        while (!done) {
            int ret = httpd_req_recv(req, chunk_buffer, chunk_size);
            if (ret <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    timeout_count++;
                    if (timeout_count > max_timeouts) {
                        ESP_LOGE(TAG, "Too many timeouts during upload, aborting");
                        esp_ota_abort(ota_handle);
                        free(chunk_buffer);
                        return send_json_error(req, "Upload timeout - connection too slow", 408);
                    }
                    continue;
                }
                
                // ret == 0 means connection closed by client (EOF)
                // This is normal when client finishes sending data
                // Check if we've received enough data or found the boundary
                if (ret == 0) {
                    if (done) {
                        // Already found boundary, connection close is expected
                        ESP_LOGI(TAG, "Connection closed by client after receiving all data");
                        break;
                    } else if (expected_firmware_bytes > 0 && total_written >= (expected_firmware_bytes * 95 / 100)) {
                        // Received at least 95% of expected data, likely complete
                        ESP_LOGI(TAG, "Connection closed by client, received %d bytes (expected ~%d)", 
                                 total_written, expected_firmware_bytes);
                        break;
                    } else {
                        // Connection closed but we haven't received enough data
                        ESP_LOGE(TAG, "Connection closed prematurely (ret=0): received %d bytes, expected ~%d bytes", 
                                 total_written, expected_firmware_bytes);
                        esp_ota_abort(ota_handle);
                        free(chunk_buffer);
                        return send_json_error(req, "Connection closed before upload completed", 500);
                    }
                } else {
                    // Negative ret value indicates actual error
                    ESP_LOGE(TAG, "Connection error during upload (ret=%d), aborting OTA", ret);
                    esp_ota_abort(ota_handle);
                    free(chunk_buffer);
                    return send_json_error(req, "Connection error during upload", 500);
                }
            }
            timeout_count = 0; // Reset timeout counter on successful read
            
            // Search for boundary markers in this chunk
            // Look for both start boundary (--boundary) and end boundary (--boundary--)
            char *boundary_pos = NULL;
            size_t to_write = ret;
            bool found_end = false;
            
            // First check for end boundary (--boundary--)
            char *search_start = chunk_buffer;
            while ((search_start = strstr(search_start, end_boundary)) != NULL) {
                // Check if this is actually a boundary (should be preceded by \r\n or at start)
                if (search_start == chunk_buffer || 
                    (search_start > chunk_buffer && 
                     (search_start[-1] == '\n' || (search_start[-1] == '\r' && search_start > chunk_buffer + 1 && search_start[-2] == '\n')))) {
                    boundary_pos = search_start;
                    found_end = true;
                    break;
                }
                search_start++;
            }
            
            // If not found, check for start boundary (--boundary) - this indicates next part
            if (boundary_pos == NULL) {
                search_start = chunk_buffer;
                while ((search_start = strstr(search_start, start_boundary)) != NULL) {
                    // Check if this is actually a boundary (should be preceded by \r\n or at start)
                    if (search_start == chunk_buffer || 
                        (search_start > chunk_buffer && 
                         (search_start[-1] == '\n' || (search_start[-1] == '\r' && search_start > chunk_buffer + 1 && search_start[-2] == '\n')))) {
                        // Make sure it's not the end boundary (which is longer)
                        size_t end_boundary_len = strlen(end_boundary);
                        if (strncmp(search_start, end_boundary, end_boundary_len) != 0) {
                            boundary_pos = search_start;
                            break;
                        }
                    }
                    search_start++;
                }
            }
            
            if (boundary_pos != NULL) {
                // Found boundary - only write up to it (excluding the boundary itself and leading \r\n)
                to_write = boundary_pos - chunk_buffer;
                
                // Remove any trailing \r\n before the boundary (multipart boundaries are preceded by \r\n)
                while (to_write > 0 && (chunk_buffer[to_write - 1] == '\r' || chunk_buffer[to_write - 1] == '\n')) {
                    to_write--;
                }
                
                // Also check if there's a \r\n sequence before boundary_pos that we should exclude
                if (to_write >= 2 && chunk_buffer[to_write - 2] == '\r' && chunk_buffer[to_write - 1] == '\n') {
                    to_write -= 2;
                } else if (to_write >= 1 && chunk_buffer[to_write - 1] == '\n') {
                    to_write -= 1;
                }
                
                done = true;
            }
            
            if (to_write > 0) {
                if (!ota_manager_write_streaming_chunk(ota_handle, (const uint8_t *)chunk_buffer, to_write)) {
                    ESP_LOGE(TAG, "Failed to write chunk at offset %d", total_written);
                    free(chunk_buffer);
                    return send_json_error(req, "Failed to write firmware data", 500);
                }
                total_written += to_write;
            }
            
            // If we found the end boundary, we're done
            if (found_end) {
                done = true;
            }
        }
        
        free(chunk_buffer);
        
        ESP_LOGI(TAG, "Streamed %d bytes to OTA partition", total_written);
        
        // Validate upload completeness if Content-Length was provided
        if (content_len > 0) {
            // Account for multipart overhead (boundary + headers, typically ~1KB)
            size_t expected_firmware_bytes = (content_len > 1024) ? (content_len - 1024) : content_len;
            // Allow 5% tolerance for multipart overhead variations
            size_t min_expected = (expected_firmware_bytes * 95) / 100;
            
            if (total_written < min_expected) {
                ESP_LOGE(TAG, "Upload incomplete: received %d bytes, expected at least %d bytes", 
                         total_written, min_expected);
                esp_ota_abort(ota_handle);
                return send_json_error(req, "Upload incomplete - connection may have been interrupted", 400);
            }
            
            ESP_LOGI(TAG, "Upload validation: received %d bytes, expected ~%d bytes (within tolerance)", 
                     total_written, expected_firmware_bytes);
        }
        
        // Finish OTA update (this will set boot partition and reboot)
        // Send HTTP response BEFORE finishing, as the device will reboot
        cJSON *response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "status", "ok");
        cJSON_AddStringToObject(response, "message", "Firmware uploaded successfully. Finishing update and rebooting...");
        
        // Send response first
        esp_err_t resp_err = send_json_response(req, response, ESP_OK);
        
        // Small delay to ensure response is sent
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Now finish the update (this will reboot)
        if (!ota_manager_finish_streaming_update(ota_handle)) {
            ESP_LOGE(TAG, "Failed to finish streaming OTA update");
            // Response already sent, but update failed - device will not reboot
            return ESP_FAIL;
        }
        
        // This should never be reached as ota_manager_finish_streaming_update() reboots
        return resp_err;
    }
    
    // Handle URL-based update (existing JSON method)
    if (strstr(content_type, "application/json") == NULL) {
        ESP_LOGW(TAG, "Unsupported Content-Type for OTA update: %s", content_type);
        return send_json_error(req, "Unsupported Content-Type. Use multipart/form-data for file upload or application/json for URL", 400);
    }
    
    char content[256];
    int bytes_received = httpd_req_recv(req, content, sizeof(content) - 1);
    if (bytes_received <= 0) {
        ESP_LOGE(TAG, "Failed to read request body");
        return send_json_error(req, "Failed to read request body", 500);
    }
    content[bytes_received] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        ESP_LOGW(TAG, "Invalid JSON in request");
        return send_json_error(req, "Invalid JSON", 400);
    }
    
    cJSON *item = cJSON_GetObjectItem(json, "url");
    if (item == NULL || !cJSON_IsString(item)) {
        cJSON_Delete(json);
        return send_json_error(req, "Missing or invalid URL", 400);
    }
    
    const char *url = cJSON_GetStringValue(item);
    if (url == NULL) {
        cJSON_Delete(json);
        return send_json_error(req, "Invalid URL", 400);
    }
    cJSON_Delete(json);
    
    ESP_LOGI(TAG, "Starting OTA update from URL: %s", url);
    bool success = ota_manager_start_update(url);
    
    cJSON *response = cJSON_CreateObject();
    if (success) {
        cJSON_AddStringToObject(response, "status", "ok");
        cJSON_AddStringToObject(response, "message", "OTA update started");
    } else {
        cJSON_AddStringToObject(response, "status", "error");
        cJSON_AddStringToObject(response, "message", "Failed to start OTA update");
    }
    
    return send_json_response(req, response, success ? ESP_OK : ESP_FAIL);
}

// GET /api/ota/status - Get OTA status
static esp_err_t api_ota_status_handler(httpd_req_t *req)
{
    ota_status_info_t status_info;
    if (!ota_manager_get_status(&status_info)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    cJSON *json = cJSON_CreateObject();
    const char *status_str;
    switch (status_info.status) {
        case OTA_STATUS_IDLE:
            status_str = "idle";
            break;
        case OTA_STATUS_IN_PROGRESS:
            status_str = "in_progress";
            break;
        case OTA_STATUS_COMPLETE:
            status_str = "complete";
            break;
        case OTA_STATUS_ERROR:
            status_str = "error";
            break;
        default:
            status_str = "unknown";
            break;
    }
    
    cJSON_AddStringToObject(json, "status", status_str);
    cJSON_AddNumberToObject(json, "progress", status_info.progress);
    cJSON_AddStringToObject(json, "message", status_info.message);
    
    return send_json_response(req, json, ESP_OK);
}

// GET /api/modbus - Get Modbus enabled state
static esp_err_t api_get_modbus_handler(httpd_req_t *req)
{
    // Use cached value if available, otherwise load from NVS and cache it
    if (!s_modbus_enabled_cached) {
        s_cached_modbus_enabled = system_modbus_enabled_load();
        s_modbus_enabled_cached = true;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "enabled", s_cached_modbus_enabled);
    
    return send_json_response(req, json, ESP_OK);
}

// POST /api/modbus - Set Modbus enabled state
static esp_err_t api_post_modbus_handler(httpd_req_t *req)
{
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *item = cJSON_GetObjectItem(json, "enabled");
    if (item == NULL || !cJSON_IsBool(item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'enabled' field");
        return ESP_FAIL;
    }
    
    bool enabled = cJSON_IsTrue(item);
    cJSON_Delete(json);
    
    if (!system_modbus_enabled_save(enabled)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save Modbus state");
        return ESP_FAIL;
    }
    
    // Update cache when state changes
    s_cached_modbus_enabled = enabled;
    s_modbus_enabled_cached = true;
    
    // Apply the change immediately
    if (enabled) {
        if (!modbus_tcp_init()) {
            ESP_LOGW(TAG, "Failed to initialize ModbusTCP");
        } else {
            if (!modbus_tcp_start()) {
                ESP_LOGW(TAG, "Failed to start ModbusTCP server");
            }
        }
    } else {
        modbus_tcp_stop();
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddBoolToObject(response, "enabled", enabled);
    cJSON_AddStringToObject(response, "message", "Modbus state saved successfully");
    
    return send_json_response(req, response, ESP_OK);
}

// GET /api/assemblies/sizes - Get assembly sizes
static esp_err_t api_get_assemblies_sizes_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "input_assembly_size", sizeof(g_assembly_data064));
    cJSON_AddNumberToObject(json, "output_assembly_size", sizeof(g_assembly_data096));
    
    return send_json_response(req, json, ESP_OK);
}

// GET /api/status - Get assembly data for status pages
static esp_err_t api_get_status_handler(httpd_req_t *req)
{
    SemaphoreHandle_t mutex = scale_application_get_assembly_mutex();
    if (mutex == NULL) {
        return send_json_error(req, "Assembly mutex not available", 500);
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return send_json_error(req, "Failed to acquire assembly mutex", 500);
    }
    
    cJSON *json = cJSON_CreateObject();
    
    // Input assembly 100 (g_assembly_data064)
    cJSON *input_assembly = cJSON_CreateObject();
    cJSON *input_bytes = cJSON_CreateArray();
    for (int i = 0; i < sizeof(g_assembly_data064); i++) {
        cJSON_AddItemToArray(input_bytes, cJSON_CreateNumber(g_assembly_data064[i]));
    }
    cJSON_AddItemToObject(input_assembly, "raw_bytes", input_bytes);
    
    // Extract NAU7802 data from assembly if enabled and initialized
    if (scale_application_is_nau7802_initialized()) {
        // Use cached value for byte offset
        if (!s_nau7802_byte_offset_cached) {
            s_cached_nau7802_byte_offset = system_nau7802_byte_offset_load();
            s_nau7802_byte_offset_cached = true;
        }
        uint8_t byte_offset = s_cached_nau7802_byte_offset;
        
        // Check if we have valid NAU7802 data in assembly
        if (byte_offset <= 22) {  // Max offset for 10-byte data
            // Extract weight (int32_t, bytes 0-3)
            int32_t weight_scaled = 0;
            memcpy(&weight_scaled, &g_assembly_data064[byte_offset], sizeof(int32_t));
            
            // Extract raw reading (int32_t, bytes 4-7)
            int32_t raw_reading = 0;
            memcpy(&raw_reading, &g_assembly_data064[byte_offset + 4], sizeof(int32_t));
            
            // Extract unit code (uint8, byte 8)
            uint8_t unit_code = g_assembly_data064[byte_offset + 8];
            const char *unit_str = (unit_code == 0) ? "g" : (unit_code == 1) ? "lbs" : "kg";
            
            // Extract status flags (uint8, byte 9)
            uint8_t status_byte = g_assembly_data064[byte_offset + 9];
            bool available = (status_byte & 0x01) != 0;  // Bit 0
            bool connected = (status_byte & 0x02) != 0;  // Bit 1
            bool initialized = (status_byte & 0x04) != 0;  // Bit 2
            
            // Calculate actual weight from scaled value
            float weight_actual = (float)weight_scaled / 100.0f;
            
            // Add NAU7802 data to input assembly object
            cJSON *nau7802_data = cJSON_CreateObject();
            cJSON_AddNumberToObject(nau7802_data, "weight_scaled", weight_scaled);
            cJSON_AddNumberToObject(nau7802_data, "weight", weight_actual);
            cJSON_AddStringToObject(nau7802_data, "unit", unit_str);
            cJSON_AddNumberToObject(nau7802_data, "unit_code", unit_code);
            cJSON_AddNumberToObject(nau7802_data, "raw_reading", raw_reading);
            cJSON_AddNumberToObject(nau7802_data, "byte_offset", byte_offset);
            cJSON_AddBoolToObject(nau7802_data, "available", available);
            cJSON_AddBoolToObject(nau7802_data, "connected", connected);
            cJSON_AddBoolToObject(nau7802_data, "initialized", initialized);
            cJSON_AddNumberToObject(nau7802_data, "status_byte", status_byte);
            cJSON_AddItemToObject(input_assembly, "nau7802", nau7802_data);
        }
    }
    
    cJSON_AddItemToObject(json, "input_assembly_100", input_assembly);
    
    // Output assembly 150 (g_assembly_data096)
    cJSON *output_assembly = cJSON_CreateObject();
    cJSON *output_bytes = cJSON_CreateArray();
    for (int i = 0; i < sizeof(g_assembly_data096); i++) {
        cJSON_AddItemToArray(output_bytes, cJSON_CreateNumber(g_assembly_data096[i]));
    }
    cJSON_AddItemToObject(output_assembly, "raw_bytes", output_bytes);
    cJSON_AddItemToObject(json, "output_assembly_150", output_assembly);
    
    xSemaphoreGive(mutex);
    return send_json_response(req, json, ESP_OK);
}


// GET /api/i2c/pullup - Get I2C pull-up enabled state
static esp_err_t api_get_i2c_pullup_handler(httpd_req_t *req)
{
    // Use cached value if available, otherwise load from NVS and cache it
    if (!s_i2c_pullup_enabled_cached) {
        s_cached_i2c_pullup_enabled = system_i2c_internal_pullup_load();
        s_i2c_pullup_enabled_cached = true;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "enabled", s_cached_i2c_pullup_enabled);
    
    return send_json_response(req, json, ESP_OK);
}

// POST /api/i2c/pullup - Set I2C pull-up enabled state
static esp_err_t api_post_i2c_pullup_handler(httpd_req_t *req)
{
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *item = cJSON_GetObjectItem(json, "enabled");
    if (item == NULL || !cJSON_IsBool(item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'enabled' field");
        return ESP_FAIL;
    }
    
    bool enabled = cJSON_IsTrue(item);
    cJSON_Delete(json);
    
    if (!system_i2c_internal_pullup_save(enabled)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save I2C pull-up setting");
        return ESP_FAIL;
    }
    
    // Update cache when state changes
    s_cached_i2c_pullup_enabled = enabled;
    s_i2c_pullup_enabled_cached = true;
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddBoolToObject(response, "enabled", enabled);
    cJSON_AddStringToObject(response, "message", "I2C pull-up setting saved. Restart required for changes to take effect.");
    
    return send_json_response(req, response, ESP_OK);
}

// GET /api/logs - Get system logs
static esp_err_t api_get_logs_handler(httpd_req_t *req)
{
    if (!log_buffer_is_enabled()) {
        return send_json_error(req, "Log buffer not enabled", 503);
    }
    
    // Get log buffer size
    size_t log_size = log_buffer_get_size();
    
    // Allocate buffer for logs (limit to 32KB for API response)
    size_t buffer_size = (log_size < 32 * 1024) ? log_size + 1 : 32 * 1024;
    char *log_buffer = (char *)malloc(buffer_size);
    if (log_buffer == NULL) {
        return send_json_error(req, "Failed to allocate memory for logs", 500);
    }
    
    // Get logs
    size_t bytes_read = log_buffer_get(log_buffer, buffer_size);
    
    // Create JSON response
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "ok");
    cJSON_AddStringToObject(json, "logs", log_buffer);
    cJSON_AddNumberToObject(json, "size", bytes_read);
    cJSON_AddNumberToObject(json, "total_size", log_size);
    cJSON_AddBoolToObject(json, "truncated", bytes_read < log_size);
    
    free(log_buffer);
    
    return send_json_response(req, json, ESP_OK);
}

// Helper function to convert IP string to uint32_t (network byte order)
static uint32_t ip_string_to_uint32(const char *ip_str)
{
    if (ip_str == NULL || strlen(ip_str) == 0) {
        return 0;
    }
    struct in_addr addr;
    if (inet_aton(ip_str, &addr) == 0) {
        return 0;
    }
    return addr.s_addr;
}

// Helper function to convert uint32_t (network byte order) to IP string
static void ip_uint32_to_string(uint32_t ip, char *buf, size_t buf_size)
{
    struct in_addr addr;
    addr.s_addr = ip;
    const char *ip_str = inet_ntoa(addr);
    if (ip_str != NULL) {
        strncpy(buf, ip_str, buf_size - 1);
        buf[buf_size - 1] = '\0';
    } else {
        buf[0] = '\0';
    }
}

// GET /api/ipconfig - Get IP configuration
static esp_err_t api_get_ipconfig_handler(httpd_req_t *req)
{
    // Initialize mutex if needed (thread-safe lazy initialization)
    if (s_tcpip_mutex == NULL) {
        s_tcpip_mutex = xSemaphoreCreateMutex();
        if (s_tcpip_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create TCP/IP mutex");
            return send_json_error(req, "Internal error: mutex creation failed", 500);
        }
    }
    
    // Always read from OpENer's g_tcpip (single source of truth)
    // Protect with mutex to prevent race conditions with OpENer task
    if (xSemaphoreTake(s_tcpip_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout waiting for TCP/IP mutex");
        return send_json_error(req, "Timeout accessing IP configuration", 500);
    }
    
    bool use_dhcp = (g_tcpip.config_control & kTcpipCfgCtrlMethodMask) == kTcpipCfgCtrlDhcp;
    
    // Copy values to local variables while holding mutex
    uint32_t ip_address = g_tcpip.interface_configuration.ip_address;
    uint32_t network_mask = g_tcpip.interface_configuration.network_mask;
    uint32_t gateway = g_tcpip.interface_configuration.gateway;
    uint32_t name_server = g_tcpip.interface_configuration.name_server;
    uint32_t name_server_2 = g_tcpip.interface_configuration.name_server_2;
    
    xSemaphoreGive(s_tcpip_mutex);
    
    // Build JSON response outside of mutex (safer, no blocking)
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "use_dhcp", use_dhcp);
    
    char ip_str[16];
    ip_uint32_to_string(ip_address, ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(json, "ip_address", ip_str);
    
    ip_uint32_to_string(network_mask, ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(json, "netmask", ip_str);
    
    ip_uint32_to_string(gateway, ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(json, "gateway", ip_str);
    
    ip_uint32_to_string(name_server, ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(json, "dns1", ip_str);
    
    ip_uint32_to_string(name_server_2, ip_str, sizeof(ip_str));
    cJSON_AddStringToObject(json, "dns2", ip_str);
    
    return send_json_response(req, json, ESP_OK);
}

// POST /api/ipconfig - Set IP configuration
static esp_err_t api_post_ipconfig_handler(httpd_req_t *req)
{
    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Parse JSON first (before taking mutex)
    cJSON *item = cJSON_GetObjectItem(json, "use_dhcp");
    bool use_dhcp_requested = false;
    bool use_dhcp_set = false;
    if (item != NULL && cJSON_IsBool(item)) {
        use_dhcp_requested = cJSON_IsTrue(item);
        use_dhcp_set = true;
    }
    
    // Parse IP configuration values
    uint32_t ip_address_new = 0;
    uint32_t network_mask_new = 0;
    uint32_t gateway_new = 0;
    uint32_t name_server_new = 0;
    uint32_t name_server_2_new = 0;
    bool ip_address_set = false;
    bool network_mask_set = false;
    bool gateway_set = false;
    bool name_server_set = false;
    bool name_server_2_set = false;
    
    // Read current config_control to determine if we should parse IP settings
    bool is_static_ip = false;
    if (s_tcpip_mutex != NULL && xSemaphoreTake(s_tcpip_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        is_static_ip = ((g_tcpip.config_control & kTcpipCfgCtrlMethodMask) == kTcpipCfgCtrlStaticIp);
        xSemaphoreGive(s_tcpip_mutex);
    }
    
    if (is_static_ip || !use_dhcp_requested) {
        item = cJSON_GetObjectItem(json, "ip_address");
        if (item != NULL && cJSON_IsString(item)) {
            ip_address_new = ip_string_to_uint32(cJSON_GetStringValue(item));
            ip_address_set = true;
        }
        
        item = cJSON_GetObjectItem(json, "netmask");
        if (item != NULL && cJSON_IsString(item)) {
            network_mask_new = ip_string_to_uint32(cJSON_GetStringValue(item));
            network_mask_set = true;
        }
        
        item = cJSON_GetObjectItem(json, "gateway");
        if (item != NULL && cJSON_IsString(item)) {
            gateway_new = ip_string_to_uint32(cJSON_GetStringValue(item));
            gateway_set = true;
        }
    }
    
    item = cJSON_GetObjectItem(json, "dns1");
    if (item != NULL && cJSON_IsString(item)) {
        name_server_new = ip_string_to_uint32(cJSON_GetStringValue(item));
        name_server_set = true;
    }
    
    item = cJSON_GetObjectItem(json, "dns2");
    if (item != NULL && cJSON_IsString(item)) {
        name_server_2_new = ip_string_to_uint32(cJSON_GetStringValue(item));
        name_server_2_set = true;
    }
    
    cJSON_Delete(json);
    
    // Initialize mutex if needed
    if (s_tcpip_mutex == NULL) {
        s_tcpip_mutex = xSemaphoreCreateMutex();
        if (s_tcpip_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create TCP/IP mutex");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal error: mutex creation failed");
            return ESP_FAIL;
        }
    }
    
    // Update g_tcpip with mutex protection
    if (xSemaphoreTake(s_tcpip_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout waiting for TCP/IP mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Timeout accessing IP configuration");
        return ESP_FAIL;
    }
    
    // Update configuration control
    if (use_dhcp_set) {
        if (use_dhcp_requested) {
            g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
            g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
            g_tcpip.interface_configuration.ip_address = 0;
            g_tcpip.interface_configuration.network_mask = 0;
            g_tcpip.interface_configuration.gateway = 0;
        } else {
            g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
            g_tcpip.config_control |= kTcpipCfgCtrlStaticIp;
        }
    }
    
    // Update IP settings if provided
    if (ip_address_set) {
        g_tcpip.interface_configuration.ip_address = ip_address_new;
    }
    if (network_mask_set) {
        g_tcpip.interface_configuration.network_mask = network_mask_new;
    }
    if (gateway_set) {
        g_tcpip.interface_configuration.gateway = gateway_new;
    }
    if (name_server_set) {
        g_tcpip.interface_configuration.name_server = name_server_new;
    }
    if (name_server_2_set) {
        g_tcpip.interface_configuration.name_server_2 = name_server_2_new;
    }
    
    // Save to NVS while holding mutex
    EipStatus nvs_status = NvTcpipStore(&g_tcpip);
    xSemaphoreGive(s_tcpip_mutex);
    
    if (nvs_status != kEipStatusOk) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save IP configuration");
        return ESP_FAIL;
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "IP configuration saved successfully. Reboot required to apply changes.");
    
    return send_json_response(req, response, ESP_OK);
}

// GET /api/nau7802 - Get NAU7802 scale reading, status, and configuration
static esp_err_t api_get_nau7802_handler(httpd_req_t *req)
{
    // Use cached value if available, otherwise load from NVS and cache it
    if (!s_nau7802_enabled_cached) {
        s_cached_nau7802_enabled = system_nau7802_enabled_load();
        s_nau7802_enabled_cached = true;
    }
    if (!s_nau7802_byte_offset_cached) {
        s_cached_nau7802_byte_offset = system_nau7802_byte_offset_load();
        s_nau7802_byte_offset_cached = true;
    }
    if (!s_nau7802_unit_cached) {
        s_cached_nau7802_unit = system_nau7802_unit_load();
        s_nau7802_unit_cached = true;
    }
    if (!s_nau7802_gain_cached) {
        s_cached_nau7802_gain = system_nau7802_gain_load();
        s_nau7802_gain_cached = true;
    }
    if (!s_nau7802_sample_rate_cached) {
        s_cached_nau7802_sample_rate = system_nau7802_sample_rate_load();
        s_nau7802_sample_rate_cached = true;
    }
    if (!s_nau7802_channel_cached) {
        s_cached_nau7802_channel = system_nau7802_channel_load();
        s_nau7802_channel_cached = true;
    }
    if (!s_nau7802_ldo_cached) {
        s_cached_nau7802_ldo = system_nau7802_ldo_load();
        s_nau7802_ldo_cached = true;
    }
    if (!s_nau7802_average_cached) {
        s_cached_nau7802_average = system_nau7802_average_load();
        s_nau7802_average_cached = true;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "enabled", s_cached_nau7802_enabled);
    cJSON_AddNumberToObject(json, "byte_offset", s_cached_nau7802_byte_offset);
    cJSON_AddNumberToObject(json, "unit", s_cached_nau7802_unit);
    cJSON_AddNumberToObject(json, "gain", s_cached_nau7802_gain);
    cJSON_AddNumberToObject(json, "sample_rate", s_cached_nau7802_sample_rate);
    cJSON_AddNumberToObject(json, "channel", s_cached_nau7802_channel);
    cJSON_AddNumberToObject(json, "ldo_value", s_cached_nau7802_ldo);
    cJSON_AddNumberToObject(json, "average", s_cached_nau7802_average);
    cJSON_AddBoolToObject(json, "initialized", scale_application_is_nau7802_initialized());
    
    // Add labels for better readability
    const char *gain_labels[] = {"x1", "x2", "x4", "x8", "x16", "x32", "x64", "x128"};
    const char *sps_labels[] = {"10", "20", "40", "80", "", "", "", "320"};
    const char *unit_labels[] = {"g", "lbs", "kg"};
    const float ldo_voltages[] = {4.5f, 4.2f, 3.9f, 3.6f, 3.3f, 3.0f, 2.7f, 2.4f};
    
    if (s_cached_nau7802_gain < 8) {
        cJSON_AddStringToObject(json, "gain_label", gain_labels[s_cached_nau7802_gain]);
    }
    if (s_cached_nau7802_sample_rate < 8 && sps_labels[s_cached_nau7802_sample_rate][0] != '\0') {
        cJSON_AddStringToObject(json, "sample_rate_label", sps_labels[s_cached_nau7802_sample_rate]);
    }
    if (s_cached_nau7802_unit < 3) {
        cJSON_AddStringToObject(json, "unit_label", unit_labels[s_cached_nau7802_unit]);
    }
    if (s_cached_nau7802_channel < 2) {
        cJSON_AddStringToObject(json, "channel_label", s_cached_nau7802_channel == 0 ? "Channel 1" : "Channel 2");
    }
    if (s_cached_nau7802_ldo < 8) {
        cJSON_AddNumberToObject(json, "ldo_voltage", ldo_voltages[s_cached_nau7802_ldo]);
    }
    
    // Get scale reading if initialized
    nau7802_t *nau7802 = scale_application_get_nau7802_handle();
    if (nau7802 != NULL && scale_application_is_nau7802_initialized()) {
        // Get device mutex for thread-safe access
        SemaphoreHandle_t nau7802_mutex = scale_application_get_nau7802_mutex();
        bool connected = false;
        bool available = false;
        int32_t raw_reading = 0;
        float weight_grams = 0.0f;
        uint8_t revision_code = 0;
        int32_t ch1_offset = 0;
        uint32_t ch1_gain = 0;
        int32_t ch2_offset = 0;
        uint32_t ch2_gain = 0;
        uint8_t pu_ctrl = 0;
        uint8_t ctrl2 = 0;
        
        if (nau7802_mutex != NULL && xSemaphoreTake(nau7802_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            connected = nau7802_is_connected(nau7802);
            
            if (connected) {
                // Get current reading
                available = nau7802_available(nau7802);
                if (available) {
                    raw_reading = nau7802_get_reading(nau7802);
                }
                
                // Get calibrated weight in grams
                weight_grams = nau7802_get_weight(nau7802, false, 1, 100);
                
                // Get calibration parameters
                float cal_factor = nau7802_get_calibration_factor(nau7802);
                float zero_offset = nau7802_get_zero_offset(nau7802);
                cJSON_AddNumberToObject(json, "calibration_factor", cal_factor);
                cJSON_AddNumberToObject(json, "zero_offset", zero_offset);
                
                // Get revision code
                revision_code = nau7802_get_revision_code(nau7802);
                
                // Get channel calibration registers
                ch1_offset = nau7802_get_channel1_offset(nau7802);
                ch1_gain = nau7802_get_channel1_gain(nau7802);
                ch2_offset = nau7802_get_channel2_offset(nau7802);
                ch2_gain = nau7802_get_channel2_gain(nau7802);
                
                // Get status flags from registers
                pu_ctrl = nau7802_get_register(nau7802, NAU7802_REGISTER_PU_CTRL);
                ctrl2 = nau7802_get_register(nau7802, NAU7802_REGISTER_CTRL2);
            }
            
            xSemaphoreGive(nau7802_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire NAU7802 mutex for GET handler");
        }
        
        if (connected) {
            cJSON_AddBoolToObject(json, "connected", true);
            cJSON_AddNumberToObject(json, "raw_reading", raw_reading);
            cJSON_AddBoolToObject(json, "available", available);
            
            // Convert to selected unit for display (use cached value)
            uint8_t unit = s_cached_nau7802_unit;
            float weight_display = weight_grams;
            const char *unit_str = "g";
            if (unit == 1) {
                // Convert grams to lbs
                weight_display = weight_grams / 453.592f;
                unit_str = "lbs";
            } else if (unit == 2) {
                // Convert grams to kg
                weight_display = weight_grams / 1000.0f;
                unit_str = "kg";
            }
            
            cJSON_AddNumberToObject(json, "weight", weight_display);
            cJSON_AddStringToObject(json, "unit", unit_str);
            cJSON_AddNumberToObject(json, "unit_code", unit);
            cJSON_AddNumberToObject(json, "calibration_factor", cal_factor);
            cJSON_AddNumberToObject(json, "zero_offset", zero_offset);
            
            // Get revision code
            uint8_t revision_code = nau7802_get_revision_code(nau7802);
            cJSON_AddNumberToObject(json, "revision_code", revision_code);
            
            // Get channel calibration registers
            int32_t ch1_offset = nau7802_get_channel1_offset(nau7802);
            uint32_t ch1_gain = nau7802_get_channel1_gain(nau7802);
            int32_t ch2_offset = nau7802_get_channel2_offset(nau7802);
            uint32_t ch2_gain = nau7802_get_channel2_gain(nau7802);
            
            cJSON *ch1 = cJSON_CreateObject();
            cJSON_AddNumberToObject(ch1, "offset", ch1_offset);
            cJSON_AddNumberToObject(ch1, "gain", ch1_gain);
            cJSON_AddItemToObject(json, "channel1", ch1);
            
            cJSON *ch2 = cJSON_CreateObject();
            cJSON_AddNumberToObject(ch2, "offset", ch2_offset);
            cJSON_AddNumberToObject(ch2, "gain", ch2_gain);
            cJSON_AddItemToObject(json, "channel2", ch2);
            
            // Get status flags from registers
            uint8_t pu_ctrl = nau7802_get_register(nau7802, NAU7802_REGISTER_PU_CTRL);
            uint8_t ctrl2 = nau7802_get_register(nau7802, NAU7802_REGISTER_CTRL2);
            
            cJSON *status = cJSON_CreateObject();
            cJSON_AddBoolToObject(status, "available", available);
            cJSON_AddBoolToObject(status, "power_digital", (pu_ctrl & (1 << NAU7802_PU_CTRL_PUD)) != 0);
            cJSON_AddBoolToObject(status, "power_analog", (pu_ctrl & (1 << NAU7802_PU_CTRL_PUA)) != 0);
            cJSON_AddBoolToObject(status, "power_regulator", (pu_ctrl & (1 << NAU7802_PU_CTRL_PUR)) != 0);
            cJSON_AddBoolToObject(status, "calibration_active", (ctrl2 & NAU7802_CTRL2_CALS) != 0);
            cJSON_AddBoolToObject(status, "calibration_error", (ctrl2 & NAU7802_CTRL2_CAL_ERROR) != 0);
            cJSON_AddBoolToObject(status, "oscillator_ready", (pu_ctrl & (1 << NAU7802_PU_CTRL_OSCS)) != 0);
            cJSON_AddBoolToObject(status, "avdd_ready", (pu_ctrl & (1 << NAU7802_PU_CTRL_AVDDS)) != 0);
            cJSON_AddItemToObject(json, "status", status);
        } else {
            cJSON_AddBoolToObject(json, "connected", false);
        }
    } else {
        cJSON_AddBoolToObject(json, "connected", false);
    }
    
    return send_json_response(req, json, ESP_OK);
}

// POST /api/nau7802 - Configure NAU7802 (enable/disable, byte offset)
static esp_err_t api_post_nau7802_handler(httpd_req_t *req)
{
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    bool config_changed = false;
    
    // Handle enabled state
    cJSON *item = cJSON_GetObjectItem(json, "enabled");
    if (item != NULL && cJSON_IsBool(item)) {
        bool enabled = cJSON_IsTrue(item);
        if (system_nau7802_enabled_save(enabled)) {
            s_cached_nau7802_enabled = enabled;
            s_nau7802_enabled_cached = true;
            config_changed = true;
        }
    }
    
    // Handle byte offset
    item = cJSON_GetObjectItem(json, "byte_offset");
    if (item != NULL && cJSON_IsNumber(item)) {
        uint8_t byte_offset = (uint8_t)cJSON_GetNumberValue(item);
        const uint8_t assembly_size = 32;  // Assembly 100 is 32 bytes
        const uint8_t nau7802_data_size = 10;  // 4 bytes weight + 4 bytes raw + 1 byte unit + 1 byte status
        const uint8_t max_offset = assembly_size - nau7802_data_size;  // 32 - 10 = 22
        
        if (byte_offset > max_offset) {
            cJSON_Delete(json);
            return send_json_error(req, "Byte offset too large. Maximum is 22 (assembly size 32 - data size 10)", 400);
        }
        
        if (system_nau7802_byte_offset_save(byte_offset)) {
            s_cached_nau7802_byte_offset = byte_offset;
            s_nau7802_byte_offset_cached = true;
            config_changed = true;
        }
    }
    
    // Handle unit selection
    item = cJSON_GetObjectItem(json, "unit");
    if (item != NULL && cJSON_IsNumber(item)) {
        int unit_int = (int)cJSON_GetNumberValue(item);
        if (unit_int >= 0 && unit_int <= 2) {
            uint8_t unit = (uint8_t)unit_int;
            if (system_nau7802_unit_save(unit)) {
                s_cached_nau7802_unit = unit;
                s_nau7802_unit_cached = true;
                config_changed = true;
            }
        }
    }
    
    // Handle gain setting
    item = cJSON_GetObjectItem(json, "gain");
    if (item != NULL && cJSON_IsNumber(item)) {
        int gain_int = (int)cJSON_GetNumberValue(item);
        if (gain_int >= 0 && gain_int <= 7) {
            uint8_t gain = (uint8_t)gain_int;
            if (system_nau7802_gain_save(gain)) {
                s_cached_nau7802_gain = gain;
                s_nau7802_gain_cached = true;
                config_changed = true;
            }
        }
    }
    
    // Handle sample rate
    item = cJSON_GetObjectItem(json, "sample_rate");
    if (item != NULL && cJSON_IsNumber(item)) {
        int sample_rate_int = (int)cJSON_GetNumberValue(item);
        // Valid values: 0, 1, 2, 3, 7
        if (sample_rate_int == 0 || sample_rate_int == 1 || sample_rate_int == 2 || 
            sample_rate_int == 3 || sample_rate_int == 7) {
            uint8_t sample_rate = (uint8_t)sample_rate_int;
            if (system_nau7802_sample_rate_save(sample_rate)) {
                s_cached_nau7802_sample_rate = sample_rate;
                s_nau7802_sample_rate_cached = true;
                config_changed = true;
            }
        }
    }
    
    // Handle channel selection
    item = cJSON_GetObjectItem(json, "channel");
    if (item != NULL && cJSON_IsNumber(item)) {
        int channel_int = (int)cJSON_GetNumberValue(item);
        if (channel_int >= 0 && channel_int <= 1) {
            uint8_t channel = (uint8_t)channel_int;
            if (system_nau7802_channel_save(channel)) {
                s_cached_nau7802_channel = channel;
                s_nau7802_channel_cached = true;
                config_changed = true;
            }
        }
    }
    
    // Handle LDO voltage
    item = cJSON_GetObjectItem(json, "ldo_value");
    if (item != NULL && cJSON_IsNumber(item)) {
        int ldo_int = (int)cJSON_GetNumberValue(item);
        if (ldo_int >= 0 && ldo_int <= 7) {
            uint8_t ldo = (uint8_t)ldo_int;
            if (system_nau7802_ldo_save(ldo)) {
                s_cached_nau7802_ldo = ldo;
                s_nau7802_ldo_cached = true;
                config_changed = true;
            }
        }
    }
    
    // Handle averaging setting (for regular readings)
    item = cJSON_GetObjectItem(json, "average");
    if (item != NULL && cJSON_IsNumber(item)) {
        int avg_int = (int)cJSON_GetNumberValue(item);
        if (avg_int >= 1 && avg_int <= 50) {
            uint8_t average = (uint8_t)avg_int;
            if (system_nau7802_average_save(average)) {
                s_cached_nau7802_average = average;
                s_nau7802_average_cached = true;
                config_changed = true;
            }
        }
    }
    
    cJSON_Delete(json);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", config_changed ? "Configuration saved. Reboot required to apply gain, sample rate, channel, or LDO changes." : "No changes");
    
    return send_json_response(req, response, ESP_OK);
}

// POST /api/nau7802/calibrate - Calibrate the scale
static esp_err_t api_post_nau7802_calibrate_handler(httpd_req_t *req)
{
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    nau7802_t *nau7802 = scale_application_get_nau7802_handle();
    if (nau7802 == NULL || !scale_application_is_nau7802_initialized()) {
        cJSON_Delete(json);
        return send_json_error(req, "NAU7802 not initialized", 500);
    }
    
    cJSON *item = cJSON_GetObjectItem(json, "action");
    if (item == NULL || !cJSON_IsString(item)) {
        cJSON_Delete(json);
        return send_json_error(req, "Missing or invalid 'action' field (must be 'tare' or 'calibrate')", 400);
    }
    
    const char *action = cJSON_GetStringValue(item);
    cJSON *response = cJSON_CreateObject();
    
    if (strcmp(action, "tare") == 0) {
        // Tare (zero offset) calibration
        // Use default of 10 samples for averaging during calibration
        SemaphoreHandle_t nau7802_mutex = scale_application_get_nau7802_mutex();
        esp_err_t err = ESP_FAIL;
        
        if (nau7802_mutex != NULL && xSemaphoreTake(nau7802_mutex, portMAX_DELAY) == pdTRUE) {
            err = nau7802_calculate_zero_offset(nau7802, 10, 5000);
            if (err == ESP_OK) {
                float zero_offset = nau7802_get_zero_offset(nau7802);
                xSemaphoreGive(nau7802_mutex);
                system_nau7802_zero_offset_save(zero_offset);
                cJSON_AddStringToObject(response, "status", "ok");
                cJSON_AddStringToObject(response, "message", "Tare calibration completed");
                cJSON_AddNumberToObject(response, "zero_offset", zero_offset);
            } else {
                xSemaphoreGive(nau7802_mutex);
                ESP_LOGE(TAG, "Tare calibration failed: %s", esp_err_to_name(err));
                cJSON_AddStringToObject(response, "status", "error");
                cJSON_AddStringToObject(response, "message", "Tare calibration failed");
            }
        } else {
            ESP_LOGE(TAG, "Failed to acquire NAU7802 mutex for tare calibration");
            cJSON_AddStringToObject(response, "status", "error");
            cJSON_AddStringToObject(response, "message", "Failed to acquire device lock");
        }
    } else if (strcmp(action, "calibrate") == 0) {
        // Calibration with known weight
        item = cJSON_GetObjectItem(json, "known_weight");
        if (item == NULL || !cJSON_IsNumber(item)) {
            cJSON_Delete(json);
            cJSON_Delete(response);
            return send_json_error(req, "Missing or invalid 'known_weight' field", 400);
        }
        
        float known_weight_input = (float)cJSON_GetNumberValue(item);
        if (known_weight_input <= 0.0f) {
            cJSON_Delete(json);
            cJSON_Delete(response);
            return send_json_error(req, "Known weight must be greater than 0", 400);
        }
        
        // Get unit selection and convert to grams (NAU7802 calibration uses grams internally)
        // Use cached value if available, otherwise load from NVS
        if (!s_nau7802_unit_cached) {
            s_cached_nau7802_unit = system_nau7802_unit_load();
            s_nau7802_unit_cached = true;
        }
        uint8_t unit = s_cached_nau7802_unit;
        float known_weight_grams = known_weight_input;
        if (unit == 1) {
            // Convert lbs to grams: 1 lb = 453.592 grams
            known_weight_grams = known_weight_input * 453.592f;
        } else if (unit == 2) {
            // Convert kg to grams: 1 kg = 1000 grams
            known_weight_grams = known_weight_input * 1000.0f;
        }
        // unit == 0 means grams, no conversion needed
        
        // Use default of 10 samples for averaging during calibration
        SemaphoreHandle_t nau7802_mutex = scale_application_get_nau7802_mutex();
        esp_err_t err = ESP_FAIL;
        
        if (nau7802_mutex != NULL && xSemaphoreTake(nau7802_mutex, portMAX_DELAY) == pdTRUE) {
            err = nau7802_calculate_calibration_factor(nau7802, known_weight_grams, 10, 5000);
            if (err == ESP_OK) {
                float cal_factor = nau7802_get_calibration_factor(nau7802);
                float zero_offset = nau7802_get_zero_offset(nau7802);
                xSemaphoreGive(nau7802_mutex);
                system_nau7802_calibration_factor_save(cal_factor);
                system_nau7802_zero_offset_save(zero_offset);
                cJSON_AddStringToObject(response, "status", "ok");
                cJSON_AddStringToObject(response, "message", "Calibration completed");
                cJSON_AddNumberToObject(response, "calibration_factor", cal_factor);
                cJSON_AddNumberToObject(response, "zero_offset", zero_offset);
            } else {
                xSemaphoreGive(nau7802_mutex);
                ESP_LOGE(TAG, "Known-weight calibration failed: %s", esp_err_to_name(err));
                cJSON_AddStringToObject(response, "status", "error");
                cJSON_AddStringToObject(response, "message", "Calibration failed");
            }
        } else {
            ESP_LOGE(TAG, "Failed to acquire NAU7802 mutex for calibration");
            cJSON_AddStringToObject(response, "status", "error");
            cJSON_AddStringToObject(response, "message", "Failed to acquire device lock");
        }
    } else if (strcmp(action, "afe") == 0) {
        // AFE (Analog Front End) calibration
        ESP_LOGI(TAG, "Performing AFE calibration");
        SemaphoreHandle_t nau7802_mutex = scale_application_get_nau7802_mutex();
        esp_err_t err = ESP_FAIL;
        
        if (nau7802_mutex != NULL && xSemaphoreTake(nau7802_mutex, portMAX_DELAY) == pdTRUE) {
            err = nau7802_calibrate_af(nau7802);
            xSemaphoreGive(nau7802_mutex);
            
            if (err == ESP_OK) {
                cJSON_AddStringToObject(response, "status", "ok");
                cJSON_AddStringToObject(response, "message", "AFE calibration completed successfully");
            } else {
                ESP_LOGE(TAG, "AFE calibration failed: %s", esp_err_to_name(err));
                cJSON_AddStringToObject(response, "status", "error");
                cJSON_AddStringToObject(response, "message", "AFE calibration failed");
            }
        } else {
            ESP_LOGE(TAG, "Failed to acquire NAU7802 mutex for AFE calibration");
            cJSON_AddStringToObject(response, "status", "error");
            cJSON_AddStringToObject(response, "message", "Failed to acquire device lock");
        }
    } else {
        cJSON_Delete(json);
        cJSON_Delete(response);
        return send_json_error(req, "Invalid action (must be 'tare', 'calibrate', or 'afe')", 400);
    }
    
    cJSON_Delete(json);
    return send_json_response(req, response, ESP_OK);
}

void webui_register_api_handlers(httpd_handle_t server)
{
    if (server == NULL) {
        ESP_LOGE(TAG, "Cannot register API handlers: server handle is NULL!");
        return;
    }
    
    ESP_LOGI(TAG, "Registering API handlers...");
    
    // POST /api/ota/update
    httpd_uri_t ota_update_uri = {
        .uri       = "/api/ota/update",
        .method    = HTTP_POST,
        .handler   = api_ota_update_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &ota_update_uri);
    
    // GET /api/ota/status
    httpd_uri_t ota_status_uri = {
        .uri       = "/api/ota/status",
        .method    = HTTP_GET,
        .handler   = api_ota_status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &ota_status_uri);
    
    // POST /api/reboot
    httpd_uri_t reboot_uri = {
        .uri       = "/api/reboot",
        .method    = HTTP_POST,
        .handler   = api_reboot_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &reboot_uri);
    
    // GET /api/modbus
    httpd_uri_t get_modbus_uri = {
        .uri       = "/api/modbus",
        .method    = HTTP_GET,
        .handler   = api_get_modbus_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_modbus_uri);
    
    // POST /api/modbus
    httpd_uri_t post_modbus_uri = {
        .uri       = "/api/modbus",
        .method    = HTTP_POST,
        .handler   = api_post_modbus_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &post_modbus_uri);
    
    // GET /api/assemblies/sizes
    httpd_uri_t get_assemblies_sizes_uri = {
        .uri       = "/api/assemblies/sizes",
        .method    = HTTP_GET,
        .handler   = api_get_assemblies_sizes_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_assemblies_sizes_uri);
    
    // GET /api/status - Get assembly data for status pages
    httpd_uri_t get_status_uri = {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = api_get_status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_status_uri);
    
    // GET /api/i2c/pullup
    httpd_uri_t get_i2c_pullup_uri = {
        .uri       = "/api/i2c/pullup",
        .method    = HTTP_GET,
        .handler   = api_get_i2c_pullup_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_i2c_pullup_uri);
    
    // POST /api/i2c/pullup
    httpd_uri_t post_i2c_pullup_uri = {
        .uri       = "/api/i2c/pullup",
        .method    = HTTP_POST,
        .handler   = api_post_i2c_pullup_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &post_i2c_pullup_uri);
    
    
    // GET /api/logs - Get system logs
    httpd_uri_t get_logs_uri = {
        .uri       = "/api/logs",
        .method    = HTTP_GET,
        .handler   = api_get_logs_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_logs_uri);
    
    // GET /api/ipconfig
    httpd_uri_t get_ipconfig_uri = {
        .uri       = "/api/ipconfig",
        .method    = HTTP_GET,
        .handler   = api_get_ipconfig_handler,
        .user_ctx  = NULL
    };
    esp_err_t ret = httpd_register_uri_handler(server, &get_ipconfig_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /api/ipconfig: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Registered GET /api/ipconfig handler");
    }
    
    // POST /api/ipconfig
    httpd_uri_t post_ipconfig_uri = {
        .uri       = "/api/ipconfig",
        .method    = HTTP_POST,
        .handler   = api_post_ipconfig_handler,
        .user_ctx  = NULL
    };
    ret = httpd_register_uri_handler(server, &post_ipconfig_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/ipconfig: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/ipconfig handler");
    }
    
    // GET /api/nau7802
    httpd_uri_t get_nau7802_uri = {
        .uri       = "/api/nau7802",
        .method    = HTTP_GET,
        .handler   = api_get_nau7802_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_nau7802_uri);
    
    // POST /api/nau7802
    httpd_uri_t post_nau7802_uri = {
        .uri       = "/api/nau7802",
        .method    = HTTP_POST,
        .handler   = api_post_nau7802_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &post_nau7802_uri);
    
    // POST /api/nau7802/calibrate
    httpd_uri_t post_nau7802_calibrate_uri = {
        .uri       = "/api/nau7802/calibrate",
        .method    = HTTP_POST,
        .handler   = api_post_nau7802_calibrate_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &post_nau7802_calibrate_uri);
    
    ESP_LOGI(TAG, "API handler registration complete");
}

