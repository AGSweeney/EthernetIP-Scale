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

#include "ota_manager.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota_manager";
static ota_status_info_t s_ota_status = {0};
static SemaphoreHandle_t s_ota_mutex = NULL;
static TaskHandle_t s_ota_task_handle = NULL;
static const esp_partition_t *s_update_partition = NULL; // Store partition being updated
static size_t s_streaming_total_bytes = 0; // Total bytes written during streaming update
static size_t s_streaming_expected_size = 0; // Expected total size for streaming update

static void ota_task(void *pvParameters)
{
    const char *url = (const char *)pvParameters;
    
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "OTA begin failed: %s", esp_err_to_name(err));
        s_ota_status.progress = 0;
        s_ota_task_handle = NULL;
        xSemaphoreGive(s_ota_mutex);
        free((void *)url); // Free the URL copy
        vTaskDelete(NULL);
        return;
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_ota_status.status = OTA_STATUS_IN_PROGRESS;
    s_ota_status.progress = 0;
    strcpy(s_ota_status.message, "Downloading firmware...");
    xSemaphoreGive(s_ota_mutex);
    
    esp_err_t ota_finish_err = ESP_OK;
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Update progress (simplified - just indicate we're downloading)
        esp_app_desc_t new_app_info;
        if (esp_https_ota_get_img_desc(https_ota_handle, &new_app_info) == ESP_OK) {
            // Image header received, update progress indicator
            xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
            if (s_ota_status.progress < 90) {
                s_ota_status.progress += 1; // Increment progress gradually
            }
            snprintf(s_ota_status.message, sizeof(s_ota_status.message), 
                    "Downloading firmware...");
            xSemaphoreGive(s_ota_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (err == ESP_OK) {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if (ota_finish_err == ESP_OK) {
            ESP_LOGI(TAG, "OTA update successful, rebooting...");
            xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
            s_ota_status.status = OTA_STATUS_COMPLETE;
            s_ota_status.progress = 100;
            strcpy(s_ota_status.message, "Update complete, rebooting...");
            xSemaphoreGive(s_ota_mutex);
            
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(ota_finish_err));
            xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
            s_ota_status.status = OTA_STATUS_ERROR;
            snprintf(s_ota_status.message, sizeof(s_ota_status.message), 
                    "OTA finish failed: %s", esp_err_to_name(ota_finish_err));
            xSemaphoreGive(s_ota_mutex);
        }
    } else {
        ESP_LOGE(TAG, "OTA perform failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), 
                "OTA perform failed: %s", esp_err_to_name(err));
        xSemaphoreGive(s_ota_mutex);
    }
    
    esp_https_ota_abort(https_ota_handle);
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_ota_task_handle = NULL;
    xSemaphoreGive(s_ota_mutex);
    free((void *)url); // Free the URL copy
    vTaskDelete(NULL);
}

bool ota_manager_init(void)
{
    if (s_ota_mutex == NULL) {
        s_ota_mutex = xSemaphoreCreateMutex();
        if (s_ota_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create OTA mutex");
            return false;
        }
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_ota_status.status = OTA_STATUS_IDLE;
    s_ota_status.progress = 0;
    strcpy(s_ota_status.message, "Ready");
    xSemaphoreGive(s_ota_mutex);
    
    ESP_LOGI(TAG, "OTA manager initialized");
    return true;
}

bool ota_manager_start_update(const char *url)
{
    if (url == NULL || strlen(url) == 0) {
        ESP_LOGE(TAG, "Invalid URL");
        return false;
    }
    
    if (s_ota_mutex == NULL) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return false;
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    if (s_ota_task_handle != NULL) {
        xSemaphoreGive(s_ota_mutex);
        ESP_LOGW(TAG, "OTA update already in progress");
        return false;
    }
    
    // Create task for OTA update
    char *url_copy = strdup(url);
    if (url_copy == NULL) {
        xSemaphoreGive(s_ota_mutex);
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return false;
    }
    
    BaseType_t result = xTaskCreate(ota_task, "ota_task", 8192, url_copy, 5, &s_ota_task_handle);
    if (result != pdPASS) {
        xSemaphoreGive(s_ota_mutex);
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(url_copy);
        return false;
    }
    
    xSemaphoreGive(s_ota_mutex);
    ESP_LOGI(TAG, "OTA update started from URL: %s", url);
    return true;
}

static void ota_update_from_data_task(void *pvParameters)
{
    typedef struct {
        uint8_t *data;
        size_t len;
    } ota_data_t;
    
    ota_data_t *ota_data = (ota_data_t *)pvParameters;
    
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        strcpy(s_ota_status.message, "No OTA partition found");
        s_ota_status.progress = 0;
        s_ota_task_handle = NULL;
        xSemaphoreGive(s_ota_mutex);
        free(ota_data->data);
        free(ota_data);
        vTaskDelete(NULL);
        return;
    }
    
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, ota_data->len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "OTA begin failed: %s", esp_err_to_name(err));
        s_ota_status.progress = 0;
        s_ota_task_handle = NULL;
        xSemaphoreGive(s_ota_mutex);
        free(ota_data->data);
        free(ota_data);
        vTaskDelete(NULL);
        return;
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_ota_status.status = OTA_STATUS_IN_PROGRESS;
    s_ota_status.progress = 0;
    strcpy(s_ota_status.message, "Writing firmware...");
    xSemaphoreGive(s_ota_mutex);
    
    // Write firmware in chunks
    const size_t chunk_size = 4096;
    size_t written = 0;
    for (size_t offset = 0; offset < ota_data->len; offset += chunk_size) {
        size_t to_write = (offset + chunk_size > ota_data->len) ? (ota_data->len - offset) : chunk_size;
        err = esp_ota_write(ota_handle, ota_data->data + offset, to_write);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed at offset %d: %s", offset, esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
            s_ota_status.status = OTA_STATUS_ERROR;
            snprintf(s_ota_status.message, sizeof(s_ota_status.message), "Write failed: %s", esp_err_to_name(err));
            s_ota_task_handle = NULL;
            xSemaphoreGive(s_ota_mutex);
            free(ota_data->data);
            free(ota_data);
            vTaskDelete(NULL);
            return;
        }
        
        written += to_write;
        uint8_t progress = (uint8_t)((written * 100) / ota_data->len);
        
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.progress = progress;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "Writing: %d%%", progress);
        xSemaphoreGive(s_ota_mutex);
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to allow status updates
    }
    
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "OTA end failed: %s", esp_err_to_name(err));
        s_ota_task_handle = NULL;
        xSemaphoreGive(s_ota_mutex);
        free(ota_data->data);
        free(ota_data);
        vTaskDelete(NULL);
        return;
    }
    
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "Set boot partition failed: %s", esp_err_to_name(err));
        s_ota_task_handle = NULL;
        xSemaphoreGive(s_ota_mutex);
        free(ota_data->data);
        free(ota_data);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "OTA update successful, rebooting...");
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_ota_status.status = OTA_STATUS_COMPLETE;
    s_ota_status.progress = 100;
    strcpy(s_ota_status.message, "Update complete, rebooting...");
    xSemaphoreGive(s_ota_mutex);
    
    free(ota_data->data);
    free(ota_data);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

bool ota_manager_start_update_from_data(const uint8_t *data, size_t data_len)
{
    if (data == NULL || data_len == 0) {
        ESP_LOGE(TAG, "Invalid firmware data");
        return false;
    }
    
    if (s_ota_mutex == NULL) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return false;
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    if (s_ota_task_handle != NULL) {
        xSemaphoreGive(s_ota_mutex);
        ESP_LOGW(TAG, "OTA update already in progress");
        return false;
    }
    
    // Get next OTA partition to check size
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        return false;
    }
    
    // Check if firmware fits (with some margin for headers)
    size_t partition_size = update_partition->size;
    if (data_len > partition_size) {
        ESP_LOGE(TAG, "Firmware too large: %d bytes (max: %d)", data_len, partition_size);
        return false;
    }
    
    // Create task for OTA update
    typedef struct {
        uint8_t *data;
        size_t len;
    } ota_data_t;
    
    ota_data_t *ota_data = malloc(sizeof(ota_data_t));
    if (ota_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for OTA data");
        return false;
    }
    
    ota_data->data = malloc(data_len);
    if (ota_data->data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware data");
        free(ota_data);
        return false;
    }
    
    memcpy(ota_data->data, data, data_len);
    ota_data->len = data_len;
    
    BaseType_t result = xTaskCreate(ota_update_from_data_task, "ota_task", 8192, ota_data, 5, &s_ota_task_handle);
    if (result != pdPASS) {
        xSemaphoreGive(s_ota_mutex);
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(ota_data->data);
        free(ota_data);
        return false;
    }
    
    xSemaphoreGive(s_ota_mutex);
    ESP_LOGI(TAG, "OTA update started from uploaded data (%d bytes)", data_len);
    return true;
}

bool ota_manager_get_status(ota_status_info_t *status_info)
{
    if (status_info == NULL) {
        return false;
    }
    
    if (s_ota_mutex == NULL) {
        return false;
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    memcpy(status_info, &s_ota_status, sizeof(ota_status_info_t));
    xSemaphoreGive(s_ota_mutex);
    
    return true;
}

// Streaming OTA update functions (no double buffering)
esp_ota_handle_t ota_manager_start_streaming_update(size_t expected_size)
{
    ESP_LOGI(TAG, "Starting streaming OTA update, expected_size: %d", expected_size);
    
    if (s_ota_mutex == NULL) {
        ESP_LOGE(TAG, "OTA manager not initialized - call ota_manager_init() first");
        return 0;
    }
    
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    if (s_ota_task_handle != NULL) {
        xSemaphoreGive(s_ota_mutex);
        ESP_LOGW(TAG, "OTA update already in progress (task handle: %p)", s_ota_task_handle);
        return 0;
    }
    
    // Get running partition for diagnostics
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition != NULL) {
        ESP_LOGI(TAG, "Running partition: label=%s, type=%d, subtype=%d, address=0x%x, size=%d",
                 running_partition->label, running_partition->type, running_partition->subtype,
                 running_partition->address, running_partition->size);
    } else {
        ESP_LOGW(TAG, "Could not determine running partition");
    }
    
    // Get next OTA partition
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    
    // If esp_ota_get_next_update_partition fails, try manual selection
    if (update_partition == NULL) {
        ESP_LOGW(TAG, "esp_ota_get_next_update_partition returned NULL, trying manual selection");
        
        // Try to find OTA partitions manually
        const esp_partition_t *ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        const esp_partition_t *ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
        const esp_partition_t *otadata = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
        
        if (ota0 != NULL) {
            ESP_LOGI(TAG, "Found ota_0 partition: label=%s, address=0x%x, size=%d", ota0->label, ota0->address, ota0->size);
        } else {
            ESP_LOGW(TAG, "ota_0 partition not found");
        }
        if (ota1 != NULL) {
            ESP_LOGI(TAG, "Found ota_1 partition: label=%s, address=0x%x, size=%d", ota1->label, ota1->address, ota1->size);
        } else {
            ESP_LOGW(TAG, "ota_1 partition not found");
        }
        if (otadata != NULL) {
            ESP_LOGI(TAG, "Found otadata partition: label=%s, address=0x%x, size=%d", otadata->label, otadata->address, otadata->size);
        } else {
            ESP_LOGW(TAG, "otadata partition not found");
        }
        
        // Determine which partition to update based on running partition
        if (running_partition != NULL) {
            if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 && ota1 != NULL) {
                update_partition = ota1;
                ESP_LOGI(TAG, "Running on ota_0, will update ota_1");
            } else if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1 && ota0 != NULL) {
                update_partition = ota0;
                ESP_LOGI(TAG, "Running on ota_1, will update ota_0");
            } else if (ota0 != NULL) {
                // Default to ota_0 if we can't determine
                update_partition = ota0;
                ESP_LOGI(TAG, "Defaulting to ota_0 partition");
            } else if (ota1 != NULL) {
                update_partition = ota1;
                ESP_LOGI(TAG, "Defaulting to ota_1 partition");
            }
        } else {
            // No running partition info, try ota_0 first
            if (ota0 != NULL) {
                update_partition = ota0;
                ESP_LOGI(TAG, "No running partition info, using ota_0");
            } else if (ota1 != NULL) {
                update_partition = ota1;
                ESP_LOGI(TAG, "No running partition info, using ota_1");
            }
        }
        
        if (update_partition == NULL) {
            ESP_LOGE(TAG, "No OTA partition found - check partition table");
            xSemaphoreGive(s_ota_mutex);
            return 0;
        }
    }
    
    ESP_LOGI(TAG, "Found OTA update partition: label=%s, type=%d, subtype=%d, address=0x%x, size=%d", 
             update_partition->label, update_partition->type, update_partition->subtype, 
             update_partition->address, update_partition->size);
    
    // Use partition size as max (esp_ota_begin will validate)
    size_t partition_size = update_partition->size;
    size_t ota_size = (expected_size > 0 && expected_size < partition_size) ? expected_size : partition_size;
    
    ESP_LOGI(TAG, "Starting OTA with size: %d (partition size: %d)", ota_size, partition_size);
    
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, ota_size, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s (0x%x)", esp_err_to_name(err), err);
        xSemaphoreGive(s_ota_mutex);
        return 0;
    }
    
    ESP_LOGI(TAG, "esp_ota_begin successful, handle: %d", ota_handle);
    
    // Store the partition pointer for use in finish
    s_update_partition = update_partition;
    
    // Initialize streaming progress tracking
    s_streaming_total_bytes = 0;
    s_streaming_expected_size = ota_size; // Use partition size as expected size if not provided
    
    // Update status
    s_ota_status.status = OTA_STATUS_IN_PROGRESS;
    s_ota_status.progress = 0;
    strcpy(s_ota_status.message, "Uploading firmware...");
    xSemaphoreGive(s_ota_mutex);
    
    ESP_LOGI(TAG, "Streaming OTA update started successfully");
    return ota_handle;
}

bool ota_manager_write_streaming_chunk(esp_ota_handle_t ota_handle, const uint8_t *data, size_t len)
{
    if (ota_handle == 0 || data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for write_streaming_chunk");
        return false;
    }
    
    esp_ota_handle_t handle = ota_handle;
    esp_err_t err = esp_ota_write(handle, data, len);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        esp_ota_abort(handle);
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "Write failed: %s", esp_err_to_name(err));
        s_streaming_total_bytes = 0; // Reset streaming counters on error
        s_streaming_expected_size = 0;
        xSemaphoreGive(s_ota_mutex);
        return false;
    }
    
    // Update progress tracking
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_streaming_total_bytes += len;
    
    // Calculate progress percentage (0-100)
    if (s_streaming_expected_size > 0) {
        uint8_t progress = (uint8_t)((s_streaming_total_bytes * 100) / s_streaming_expected_size);
        if (progress > 100) {
            progress = 100; // Cap at 100%
        }
        s_ota_status.progress = progress;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), 
                "Uploading firmware... %d%% (%d/%d bytes)", 
                progress, s_streaming_total_bytes, s_streaming_expected_size);
    } else {
        // If we don't know expected size, just show bytes written
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), 
                "Uploading firmware... %d bytes written", s_streaming_total_bytes);
    }
    xSemaphoreGive(s_ota_mutex);
    
    return true;
}

bool ota_manager_finish_streaming_update(esp_ota_handle_t ota_handle)
{
    if (ota_handle == 0) {
        ESP_LOGE(TAG, "Invalid OTA handle");
        return false;
    }
    
    esp_ota_handle_t handle = ota_handle;
    
    esp_err_t err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "OTA end failed: %s", esp_err_to_name(err));
        s_ota_task_handle = NULL;
        s_update_partition = NULL;
        s_streaming_total_bytes = 0; // Reset streaming counters on error
        s_streaming_expected_size = 0;
        xSemaphoreGive(s_ota_mutex);
        return false;
    }
    
    // Use the partition we stored when starting the update
    if (s_update_partition == NULL) {
        ESP_LOGE(TAG, "Update partition not found - internal error");
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        strcpy(s_ota_status.message, "Update partition not found");
        s_ota_task_handle = NULL;
        s_streaming_total_bytes = 0; // Reset streaming counters on error
        s_streaming_expected_size = 0;
        xSemaphoreGive(s_ota_mutex);
        return false;
    }
    
    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
        s_ota_status.status = OTA_STATUS_ERROR;
        snprintf(s_ota_status.message, sizeof(s_ota_status.message), "Set boot partition failed: %s", esp_err_to_name(err));
        s_ota_task_handle = NULL;
        s_update_partition = NULL;
        s_streaming_total_bytes = 0; // Reset streaming counters on error
        s_streaming_expected_size = 0;
        xSemaphoreGive(s_ota_mutex);
        return false;
    }
    
    ESP_LOGI(TAG, "Streaming OTA update successful, rebooting...");
    xSemaphoreTake(s_ota_mutex, portMAX_DELAY);
    s_ota_status.status = OTA_STATUS_COMPLETE;
    s_ota_status.progress = 100;
    strcpy(s_ota_status.message, "Update complete, rebooting...");
    s_ota_task_handle = NULL;
    s_update_partition = NULL; // Clear partition pointer
    s_streaming_total_bytes = 0; // Reset streaming counters
    s_streaming_expected_size = 0;
    xSemaphoreGive(s_ota_mutex);
    
    // Delay 3 seconds to allow web UI to poll and display completion status
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return true;
}

