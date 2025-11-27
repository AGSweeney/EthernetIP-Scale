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

#include "mcp_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mcp_config";
static const char *NVS_NAMESPACE = "mcp_config";
static const char *NVS_KEY_DEVICES = "devices";
static const char *NVS_KEY_COUNT = "count";

// Global storage for detected devices (from boot-time scan)
static mcp_detected_devices_t s_detected_devices = {0};

void mcp_config_get_defaults(mcp_device_config_t *config, uint8_t i2c_address, uint8_t device_type)
{
    if (config == NULL) {
        return;
    }
    
    memset(config, 0, sizeof(mcp_device_config_t));
    config->i2c_address = i2c_address;
    config->device_type = device_type;
    config->enabled = true;
    
           // Default: all pins as outputs (0x0000 for MCP23017, 0x00 for MCP23008)
           config->pin_directions = 0;
           config->input_byte_start = 0;
           config->output_byte_start = 0;
           config->output_logic_inverted = false;  // Default: standard logic (active high)
    
    // Initialize pin mappings to disabled
    for (int i = 0; i < MCP_MAX_PINS_MCP23017; i++) {
        config->pin_mappings[i].enabled = false;
        config->pin_mappings[i].assembly_type = 0;
        config->pin_mappings[i].byte_offset = 0;
        config->pin_mappings[i].bit_offset = 0;
    }
}

bool mcp_config_load_all(mcp_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved MCP configurations found, using defaults");
            config->device_count = 0;
            return false;
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    // Load device count
    uint8_t device_count = 0;
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_COUNT, &device_count, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No device count found, assuming no devices");
        config->device_count = 0;
        nvs_close(handle);
        return false;
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load device count: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    if (device_count > MCP_MAX_DEVICES) {
        ESP_LOGW(TAG, "Device count %d exceeds maximum %d, clamping", device_count, MCP_MAX_DEVICES);
        device_count = MCP_MAX_DEVICES;
    }
    
    config->device_count = device_count;
    
    // Load device configurations
    required_size = sizeof(mcp_device_config_t) * device_count;
    err = nvs_get_blob(handle, NVS_KEY_DEVICES, config->devices, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No device configurations found");
        config->device_count = 0;
        return false;
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load device configurations: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Loaded %d MCP device configuration(s) from NVS", device_count);
    return true;
}

bool mcp_config_save_all(const mcp_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    // Save device count
    err = nvs_set_blob(handle, NVS_KEY_COUNT, &config->device_count, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save device count: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    // Save device configurations
    if (config->device_count > 0) {
        err = nvs_set_blob(handle, NVS_KEY_DEVICES, config->devices, 
                          sizeof(mcp_device_config_t) * config->device_count);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save device configurations: %s", esp_err_to_name(err));
            nvs_close(handle);
            return false;
        }
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit MCP configurations: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Saved %d MCP device configuration(s) to NVS", config->device_count);
    return true;
}

bool mcp_config_load_device(uint8_t i2c_address, mcp_device_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    mcp_config_t all_config;
    if (!mcp_config_load_all(&all_config)) {
        return false;
    }
    
    mcp_device_config_t *found = mcp_config_find_device(&all_config, i2c_address);
    if (found == NULL) {
        return false;
    }
    
    memcpy(config, found, sizeof(mcp_device_config_t));
    return true;
}

bool mcp_config_save_device(const mcp_device_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    mcp_config_t all_config;
    mcp_config_load_all(&all_config);  // Load existing configs (may fail, that's OK)
    
    // Find existing device or add new one
    mcp_device_config_t *existing = mcp_config_find_device(&all_config, config->i2c_address);
    if (existing != NULL) {
        // Update existing device
        memcpy(existing, config, sizeof(mcp_device_config_t));
    } else {
        // Add new device
        if (all_config.device_count >= MCP_MAX_DEVICES) {
            ESP_LOGE(TAG, "Maximum number of devices (%d) reached", MCP_MAX_DEVICES);
            return false;
        }
        memcpy(&all_config.devices[all_config.device_count], config, sizeof(mcp_device_config_t));
        all_config.device_count++;
    }
    
    return mcp_config_save_all(&all_config);
}

bool mcp_config_delete_device(uint8_t i2c_address)
{
    mcp_config_t all_config;
    if (!mcp_config_load_all(&all_config)) {
        return false;
    }
    
    // Find and remove device
    for (int i = 0; i < all_config.device_count; i++) {
        if (all_config.devices[i].i2c_address == i2c_address) {
            // Shift remaining devices down
            for (int j = i; j < all_config.device_count - 1; j++) {
                memcpy(&all_config.devices[j], &all_config.devices[j + 1], sizeof(mcp_device_config_t));
            }
            all_config.device_count--;
            return mcp_config_save_all(&all_config);
        }
    }
    
    return false;
}

mcp_device_config_t* mcp_config_find_device(mcp_config_t *config, uint8_t i2c_address)
{
    if (config == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < config->device_count; i++) {
        if (config->devices[i].i2c_address == i2c_address) {
            return &config->devices[i];
        }
    }
    
    return NULL;
}

mcp_detected_devices_t* mcp_config_get_detected_devices(void)
{
    return &s_detected_devices;
}

void mcp_config_set_detected_devices(const mcp_detected_devices_t *detected)
{
    if (detected != NULL) {
        memcpy(&s_detected_devices, detected, sizeof(mcp_detected_devices_t));
        ESP_LOGI(TAG, "Set %d detected MCP device(s)", s_detected_devices.device_count);
    } else {
        memset(&s_detected_devices, 0, sizeof(mcp_detected_devices_t));
    }
}

