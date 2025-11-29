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

#include "esp32/nvlldpmanagement.h"
#include "ciplldpmanagement.h"
#include "opener_user_conf.h"
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *kTag = "LLDP_NV";

#define LLDP_NVS_NAMESPACE "lldp"        /**< NVS namespace for LLDP Management data */
#define LLDP_NVS_KEY       "mgmt_cfg"    /**< NVS key for LLDP Management configuration */

#define LLDP_NV_VERSION 1                /**< Version for NV data structure */

/**
 * @brief Non-volatile storage blob structure for LLDP Management Object
 */
typedef struct {
    uint8_t version;                     /**< Structure version */
    uint8_t lldp_enable_array[OPENER_ETHLINK_INSTANCE_CNT];  /**< Enable flags per Ethernet Link */
    uint16_t msg_tx_interval;            /**< Transmission interval in seconds */
    uint8_t msg_tx_hold;                 /**< TX hold multiplier */
    uint16_t lldp_datastore;             /**< Data store identifier */
} LldpManagementNvBlob;

/**
 * @brief Open NVS handle for LLDP Management data
 */
static esp_err_t LldpNvOpen(nvs_handle_t *handle, nvs_open_mode_t mode) {
    esp_err_t err = nvs_open(LLDP_NVS_NAMESPACE, mode, handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Namespace doesn't exist, try creating it
        err = nvs_open(LLDP_NVS_NAMESPACE, NVS_READWRITE, handle);
    }
    return err;
}

/**
 * @brief Load LLDP Management Object configuration from NVS
 * 
 * @param values Pointer to structure to fill with loaded values
 * @return kEipStatusOk on success, kEipStatusError on failure
 */
EipStatus NvLldpManagementLoad(CipLldpManagementObjectValues *values) {
    if (values == NULL) {
        return kEipStatusError;
    }

    nvs_handle_t handle;
    esp_err_t err = LldpNvOpen(&handle, NVS_READONLY);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to open NVS namespace (using defaults): %s", esp_err_to_name(err));
        return kEipStatusError;
    }

    LldpManagementNvBlob blob = {0};
    size_t required_size = sizeof(LldpManagementNvBlob);
    err = nvs_get_blob(handle, LLDP_NVS_KEY, &blob, &required_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(kTag, "No saved LLDP Management configuration found, using defaults");
        return kEipStatusError;  // Use defaults
    }

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to load LLDP Management configuration: %s", esp_err_to_name(err));
        return kEipStatusError;
    }

    if (required_size != sizeof(LldpManagementNvBlob)) {
        ESP_LOGW(kTag, "LLDP Management configuration size mismatch (expected %zu, got %zu), using defaults",
                 sizeof(LldpManagementNvBlob), required_size);
        return kEipStatusError;  // Use defaults
    }

    if (blob.version != LLDP_NV_VERSION) {
        ESP_LOGW(kTag, "LLDP Management configuration version mismatch (expected %d, got %d), using defaults",
                 LLDP_NV_VERSION, blob.version);
        return kEipStatusError;  // Use defaults
    }

    // Validate loaded values (upper bounds enforced by type: uint16_t max=65535, uint8_t max=255)
    if (blob.msg_tx_interval == 0) {
        ESP_LOGW(kTag, "Invalid msg_tx_interval (%u), using defaults", blob.msg_tx_interval);
        return kEipStatusError;
    }

    if (blob.msg_tx_hold == 0) {
        ESP_LOGW(kTag, "Invalid msg_tx_hold (%u), using defaults", blob.msg_tx_hold);
        return kEipStatusError;
    }

    // Copy loaded values to structure
    values->lldp_enable.lldp_enable_array_length = OPENER_ETHLINK_INSTANCE_CNT;
    memcpy(values->lldp_enable.lldp_enable_array, blob.lldp_enable_array, OPENER_ETHLINK_INSTANCE_CNT);
    values->msg_tx_interval = blob.msg_tx_interval;
    values->msg_tx_hold = blob.msg_tx_hold;
    values->lldp_datastore = blob.lldp_datastore;
    // last_change is not persisted (runtime timestamp)

    ESP_LOGI(kTag, "Loaded LLDP Management configuration: interval=%u, hold=%u, enabled=%d",
             blob.msg_tx_interval, blob.msg_tx_hold, blob.lldp_enable_array[0]);

    return kEipStatusOk;
}

/**
 * @brief Store LLDP Management Object configuration to NVS
 * 
 * @param values Pointer to structure containing values to save
 * @return kEipStatusOk on success, kEipStatusError on failure
 */
EipStatus NvLldpManagementStore(const CipLldpManagementObjectValues *values) {
    if (values == NULL) {
        return kEipStatusError;
    }

    nvs_handle_t handle;
    esp_err_t err = LldpNvOpen(&handle, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return kEipStatusError;
    }

    LldpManagementNvBlob blob = {0};
    blob.version = LLDP_NV_VERSION;
    
    // Copy values to blob
    uint8_t array_len = (values->lldp_enable.lldp_enable_array_length > OPENER_ETHLINK_INSTANCE_CNT) ?
                        OPENER_ETHLINK_INSTANCE_CNT : values->lldp_enable.lldp_enable_array_length;
    memcpy(blob.lldp_enable_array, values->lldp_enable.lldp_enable_array, array_len);
    // Zero out remaining array elements if needed
    if (array_len < OPENER_ETHLINK_INSTANCE_CNT) {
        memset(blob.lldp_enable_array + array_len, 0, OPENER_ETHLINK_INSTANCE_CNT - array_len);
    }
    
    blob.msg_tx_interval = values->msg_tx_interval;
    blob.msg_tx_hold = values->msg_tx_hold;
    blob.lldp_datastore = values->lldp_datastore;

    err = nvs_set_blob(handle, LLDP_NVS_KEY, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to store LLDP Management configuration: %s", esp_err_to_name(err));
        return kEipStatusError;
    }

    ESP_LOGI(kTag, "Stored LLDP Management configuration: interval=%u, hold=%u, enabled=%d",
             blob.msg_tx_interval, blob.msg_tx_hold, blob.lldp_enable_array[0]);

    return kEipStatusOk;
}

