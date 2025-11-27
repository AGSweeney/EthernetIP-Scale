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

#include "system_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "system_config";
static const char *NVS_NAMESPACE = "system";
static const char *NVS_KEY_IPCONFIG = "ipconfig";
static const char *NVS_KEY_MODBUS_ENABLED = "modbus_enabled";
static const char *NVS_KEY_SENSOR_ENABLED = "sensor_enabled";
static const char *NVS_KEY_SENSOR_BYTE_OFFSET = "sens_byte_off";
static const char *NVS_KEY_MCP_ENABLED = "mcp_enabled";
static const char *NVS_KEY_MCP_DEVICE_TYPE = "mcp_dev_type";  // 0 = MCP23017, 1 = MCP23008
static const char *NVS_KEY_MCP_UPDATE_RATE_MS = "mcp_upd_rate";  // Update rate in milliseconds
static const char *NVS_KEY_I2C_INTERNAL_PULLUP = "i2c_pullup";
static const char *NVS_KEY_NAU7802_ENABLED = "nau7802_enabled";
static const char *NVS_KEY_NAU7802_BYTE_OFFSET = "nau7802_off";
static const char *NVS_KEY_NAU7802_CAL_FACTOR = "nau7802_cal";
static const char *NVS_KEY_NAU7802_ZERO_OFFSET = "nau7802_zero";
static const char *NVS_KEY_NAU7802_UNIT = "nau7802_unit";  // 0=grams, 1=lbs, 2=kg
static const char *NVS_KEY_NAU7802_GAIN = "nau7802_gain";  // 0-7 (x1-x128)
static const char *NVS_KEY_NAU7802_SAMPLE_RATE = "nau7802_sps";  // 0,1,2,3,7 (10,20,40,80,320 SPS)
static const char *NVS_KEY_NAU7802_CHANNEL = "nau7802_chan";  // 0=Channel 1, 1=Channel 2
static const char *NVS_KEY_NAU7802_LDO = "nau7802_ldo";  // 0-7 (2.4V-4.5V)
static const char *NVS_KEY_NAU7802_AVERAGE = "nau7802_avg";  // 1-50 samples for regular readings

void system_ip_config_get_defaults(system_ip_config_t *config)
{
    if (config == NULL) {
        return;
    }
    
    memset(config, 0, sizeof(system_ip_config_t));
    config->use_dhcp = true;  // Default to DHCP
    // All other fields are 0 (DHCP will assign)
}

bool system_ip_config_load(system_ip_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved IP configuration found, using defaults");
            system_ip_config_get_defaults(config);
            return false;
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    size_t required_size = sizeof(system_ip_config_t);
    err = nvs_get_blob(handle, NVS_KEY_IPCONFIG, config, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved IP configuration found, using defaults");
        system_ip_config_get_defaults(config);
        return false;
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load IP configuration: %s", esp_err_to_name(err));
        return false;
    }
    
    if (required_size != sizeof(system_ip_config_t)) {
        ESP_LOGW(TAG, "IP configuration size mismatch (expected %zu, got %zu), using defaults",
                 sizeof(system_ip_config_t), required_size);
        system_ip_config_get_defaults(config);
        return false;
    }
    
    ESP_LOGI(TAG, "IP configuration loaded successfully from NVS (DHCP=%s)", 
             config->use_dhcp ? "enabled" : "disabled");
    return true;
}

bool system_ip_config_save(const system_ip_config_t *config)
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
    
    err = nvs_set_blob(handle, NVS_KEY_IPCONFIG, config, sizeof(system_ip_config_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save IP configuration: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit IP configuration: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "IP configuration saved successfully to NVS");
    return true;
}

bool system_modbus_enabled_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved Modbus enabled state found, defaulting to disabled");
            return false;  // Default to disabled
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;  // Default to disabled on error
    }
    
    uint8_t enabled = 0;  // Default to disabled
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_MODBUS_ENABLED, &enabled, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved Modbus enabled state found, defaulting to disabled");
        return false;  // Default to disabled
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load Modbus enabled state: %s", esp_err_to_name(err));
        return false;  // Default to disabled on error
    }
    
    ESP_LOGI(TAG, "Modbus enabled state loaded from NVS: %s", enabled ? "enabled" : "disabled");
    return enabled != 0;
}

bool system_modbus_enabled_save(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    uint8_t enabled_val = enabled ? 1 : 0;
    err = nvs_set_blob(handle, NVS_KEY_MODBUS_ENABLED, &enabled_val, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save Modbus enabled state: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit Modbus enabled state: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Modbus enabled state saved successfully to NVS: %s", enabled ? "enabled" : "disabled");
    return true;
}

bool system_sensor_enabled_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved sensor enabled state found, defaulting to disabled");
            return false;  // Default to disabled
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;  // Default to disabled on error
    }
    
    uint8_t enabled = 0;  // Default to disabled
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_SENSOR_ENABLED, &enabled, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved sensor enabled state found, defaulting to disabled");
        return false;  // Default to disabled
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load sensor enabled state: %s", esp_err_to_name(err));
        return false;  // Default to disabled on error
    }
    
    ESP_LOGI(TAG, "Sensor enabled state loaded from NVS: %s", enabled ? "enabled" : "disabled");
    return enabled != 0;
}

bool system_sensor_enabled_save(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    uint8_t enabled_val = enabled ? 1 : 0;
    err = nvs_set_blob(handle, NVS_KEY_SENSOR_ENABLED, &enabled_val, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save sensor enabled state: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit sensor enabled state: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Sensor enabled state saved successfully to NVS: %s", enabled ? "enabled" : "disabled");
    return true;
}

uint8_t system_sensor_byte_offset_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved sensor byte offset found, defaulting to 0");
            return 0;  // Default to 0
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 0;  // Default to 0 on error
    }
    
    uint8_t start_byte = 0;  // Default to 0
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_SENSOR_BYTE_OFFSET, &start_byte, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved sensor byte offset found, defaulting to 0");
        return 0;  // Default to 0
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load sensor byte offset: %s", esp_err_to_name(err));
        return 0;  // Default to 0 on error
    }
    
    // Validate: must be 0, 9, or 18
    if (start_byte != 0 && start_byte != 9 && start_byte != 18) {
        ESP_LOGW(TAG, "Invalid sensor byte offset %d found in NVS, defaulting to 0", start_byte);
        return 0;
    }
    
    ESP_LOGI(TAG, "Sensor byte offset loaded from NVS: %d (bytes %d-%d)", start_byte, start_byte, start_byte + 8);
    return start_byte;
}

bool system_sensor_byte_offset_save(uint8_t start_byte)
{
    // Validate: must be 0, 9, or 18
    if (start_byte != 0 && start_byte != 9 && start_byte != 18) {
        ESP_LOGE(TAG, "Invalid sensor byte offset: %d (must be 0, 9, or 18)", start_byte);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_SENSOR_BYTE_OFFSET, &start_byte, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save sensor byte offset: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit sensor byte offset: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Sensor byte offset saved successfully to NVS: %d (bytes %d-%d)", start_byte, start_byte, start_byte + 8);
    return true;
}

bool system_mcp_enabled_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved MCP enabled state found, defaulting to disabled");
            return false;
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }

    uint8_t enabled = 0;
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_MCP_ENABLED, &enabled, &required_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved MCP enabled state found, defaulting to disabled");
        return false;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load MCP enabled state: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "MCP enabled state loaded from NVS: %s", enabled ? "enabled" : "disabled");
    return enabled != 0;
}

bool system_mcp_enabled_save(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }

    uint8_t enabled_val = enabled ? 1 : 0;
    err = nvs_set_blob(handle, NVS_KEY_MCP_ENABLED, &enabled_val, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save MCP enabled state: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit MCP enabled state: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "MCP enabled state saved successfully to NVS: %s", enabled ? "enabled" : "disabled");
    return true;
}

uint8_t system_mcp_device_type_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved MCP device type found, defaulting to MCP23008");
            return 1;  // Default to MCP23008 (0 = MCP23017, 1 = MCP23008)
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 1;  // Default to MCP23008 on error
    }
    
    uint8_t device_type = 1;  // Default to MCP23008
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_MCP_DEVICE_TYPE, &device_type, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved MCP device type found, defaulting to MCP23008");
        return 1;  // Default to MCP23008
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load MCP device type: %s", esp_err_to_name(err));
        return 1;  // Default to MCP23008 on error
    }
    
    // Validate: must be 0 (MCP23017) or 1 (MCP23008)
    if (device_type > 1) {
        ESP_LOGW(TAG, "Invalid MCP device type %d found in NVS, defaulting to MCP23008", device_type);
        return 1;
    }
    
    ESP_LOGI(TAG, "MCP device type loaded from NVS: %s", device_type == 0 ? "MCP23017" : "MCP23008");
    return device_type;
}

bool system_mcp_device_type_save(uint8_t device_type)
{
    // Validate: must be 0 (MCP23017) or 1 (MCP23008)
    if (device_type > 1) {
        ESP_LOGE(TAG, "Invalid MCP device type: %d (must be 0=MCP23017 or 1=MCP23008)", device_type);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_MCP_DEVICE_TYPE, &device_type, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save MCP device type: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit MCP device type: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "MCP device type saved successfully to NVS: %s", device_type == 0 ? "MCP23017" : "MCP23008");
    return true;
}

uint16_t system_mcp_update_rate_ms_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved MCP update rate found, defaulting to 20ms");
            return 20;  // Default to 20ms (50 Hz)
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 20;  // Default to 20ms on error
    }
    
    uint16_t update_rate_ms = 20;  // Default to 20ms
    size_t required_size = sizeof(uint16_t);
    err = nvs_get_blob(handle, NVS_KEY_MCP_UPDATE_RATE_MS, &update_rate_ms, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved MCP update rate found, defaulting to 20ms");
        return 20;  // Default to 20ms
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load MCP update rate: %s", esp_err_to_name(err));
        return 20;  // Default to 20ms on error
    }
    
    // Validate: reasonable range 10ms to 1000ms
    if (update_rate_ms < 10 || update_rate_ms > 1000) {
        ESP_LOGW(TAG, "Invalid MCP update rate %d ms found in NVS, defaulting to 20ms", update_rate_ms);
        return 20;
    }
    
    ESP_LOGI(TAG, "MCP update rate loaded from NVS: %d ms (%.1f Hz)", update_rate_ms, 1000.0f / update_rate_ms);
    return update_rate_ms;
}

bool system_mcp_update_rate_ms_save(uint16_t update_rate_ms)
{
    // Validate: reasonable range 10ms to 1000ms
    if (update_rate_ms < 10 || update_rate_ms > 1000) {
        ESP_LOGE(TAG, "Invalid MCP update rate %d ms (must be 10-1000ms)", update_rate_ms);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_MCP_UPDATE_RATE_MS, &update_rate_ms, sizeof(uint16_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save MCP update rate: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit MCP update rate: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "MCP update rate saved successfully to NVS: %d ms (%.1f Hz)", update_rate_ms, 1000.0f / update_rate_ms);
    return true;
}

bool system_i2c_internal_pullup_load(void)
{
    // Get compile-time default (Kconfig option)
    #ifdef CONFIG_OPENER_I2C_INTERNAL_PULLUP
    bool default_enabled = CONFIG_OPENER_I2C_INTERNAL_PULLUP;
    #else
    bool default_enabled = false;  // Default to disabled if not defined
    #endif
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved I2C pull-up setting found, using compile-time default");
            return default_enabled;
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return default_enabled;
    }
    
    uint8_t enabled = default_enabled ? 1 : 0;  // Default to Kconfig value
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_I2C_INTERNAL_PULLUP, &enabled, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved I2C pull-up setting found, using compile-time default");
        return default_enabled;
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load I2C pull-up setting: %s", esp_err_to_name(err));
        return default_enabled;
    }
    
    ESP_LOGI(TAG, "I2C internal pull-up setting loaded from NVS: %s", enabled ? "enabled" : "disabled");
    return enabled != 0;
}

bool system_i2c_internal_pullup_save(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    uint8_t value = enabled ? 1 : 0;
    err = nvs_set_blob(handle, NVS_KEY_I2C_INTERNAL_PULLUP, &value, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save I2C pull-up setting: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit I2C pull-up setting: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "I2C internal pull-up setting saved to NVS: %s", enabled ? "enabled" : "disabled");
    return true;
}

bool system_nau7802_enabled_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return false;  // Default to disabled
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    uint8_t enabled = 0;
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_ENABLED, &enabled, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;  // Default to disabled
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load NAU7802 enabled state: %s", esp_err_to_name(err));
        return false;
    }
    
    return (enabled != 0);
}

bool system_nau7802_enabled_save(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    uint8_t value = enabled ? 1 : 0;
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_ENABLED, &value, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 enabled state: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 enabled state: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 enabled state saved to NVS: %s", enabled ? "enabled" : "disabled");
    return true;
}

uint8_t system_nau7802_byte_offset_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return 0;  // Default to byte 0
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 0;
    }
    
    uint8_t offset = 0;
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_BYTE_OFFSET, &offset, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 0;  // Default to byte 0
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load NAU7802 byte offset: %s", esp_err_to_name(err));
        return 0;
    }
    
    // Validate offset: Assembly 100 is 32 bytes, NAU7802 data is 10 bytes (4 weight + 4 raw + 1 unit + 1 status)
    // Maximum valid offset: 32 - 10 = 22
    const uint8_t assembly_size = 32;
    const uint8_t nau7802_data_size = 10;
    const uint8_t max_offset = assembly_size - nau7802_data_size;
    if (offset > max_offset) {
        ESP_LOGW(TAG, "Invalid NAU7802 byte offset %d (max %d), using default 0", offset, max_offset);
        return 0;
    }
    
    return offset;
}

bool system_nau7802_byte_offset_save(uint8_t start_byte)
{
    // Validate byte offset: Assembly 100 is 32 bytes, NAU7802 data is 10 bytes (4 weight + 4 raw + 1 unit + 1 status)
    // Maximum valid offset: 32 - 10 = 22
    const uint8_t assembly_size = 32;
    const uint8_t nau7802_data_size = 10;
    const uint8_t max_offset = assembly_size - nau7802_data_size;
    if (start_byte > max_offset) {
        ESP_LOGE(TAG, "Invalid NAU7802 byte offset %d (must be 0-%d, assembly size %d - data size %d)", 
                 start_byte, max_offset, assembly_size, nau7802_data_size);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_BYTE_OFFSET, &start_byte, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 byte offset: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 byte offset: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 byte offset saved to NVS: %d", start_byte);
    return true;
}

float system_nau7802_calibration_factor_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return 0.0f;  // Default to 0.0
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 0.0f;
    }
    
    float factor = 0.0f;
    size_t required_size = sizeof(float);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_CAL_FACTOR, &factor, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 0.0f;  // Default to 0.0
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load NAU7802 calibration factor: %s", esp_err_to_name(err));
        return 0.0f;
    }
    
    return factor;
}

bool system_nau7802_calibration_factor_save(float factor)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_CAL_FACTOR, &factor, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 calibration factor: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 calibration factor: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 calibration factor saved to NVS: %.6f", factor);
    return true;
}

float system_nau7802_zero_offset_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return 0.0f;  // Default to 0.0
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 0.0f;
    }
    
    float offset = 0.0f;
    size_t required_size = sizeof(float);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_ZERO_OFFSET, &offset, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 0.0f;  // Default to 0.0
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load NAU7802 zero offset: %s", esp_err_to_name(err));
        return 0.0f;
    }
    
    return offset;
}

bool system_nau7802_zero_offset_save(float offset)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_ZERO_OFFSET, &offset, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 zero offset: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 zero offset: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 zero offset saved to NVS: %.6f", offset);
    return true;
}

uint8_t system_nau7802_unit_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return 1;  // Default to lbs
        }
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return 1;  // Default to lbs on error
    }
    
    uint8_t unit = 1;  // Default to lbs
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_UNIT, &unit, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 1;  // Default to lbs
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load NAU7802 unit: %s", esp_err_to_name(err));
        return 1;  // Default to lbs on error
    }
    
    // Validate unit value (0=grams, 1=lbs, 2=kg)
    if (unit > 2) {
        ESP_LOGW(TAG, "Invalid NAU7802 unit value %d, defaulting to lbs", unit);
        return 1;
    }
    
    return unit;
}

bool system_nau7802_unit_save(uint8_t unit)
{
    // Validate unit value (0=grams, 1=lbs, 2=kg)
    if (unit > 2) {
        ESP_LOGE(TAG, "Invalid NAU7802 unit %d (must be 0=grams, 1=lbs, or 2=kg)", unit);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_UNIT, &unit, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 unit: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 unit: %s", esp_err_to_name(err));
        return false;
    }
    
    const char *unit_str = (unit == 0) ? "grams" : (unit == 1) ? "lbs" : "kg";
    ESP_LOGI(TAG, "NAU7802 unit saved to NVS: %s", unit_str);
    return true;
}

// NAU7802 Gain (0-7: x1-x128)
uint8_t system_nau7802_gain_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS not available, using default gain (128x)");
        return 7;  // Default: NAU7802_GAIN_128
    }
    
    uint8_t gain = 7;  // Default: NAU7802_GAIN_128
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_GAIN, &gain, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "NAU7802 gain not set, using default (128x)");
        return 7;
    }
    
    if (err != ESP_OK || required_size != sizeof(uint8_t)) {
        ESP_LOGW(TAG, "Failed to load NAU7802 gain, using default (128x)");
        return 7;
    }
    
    if (gain > 7) {
        ESP_LOGW(TAG, "Invalid NAU7802 gain value (%d), using default (128x)", gain);
        return 7;
    }
    
    ESP_LOGI(TAG, "NAU7802 gain loaded from NVS: %d (x%d)", gain, 1 << gain);
    return gain;
}

bool system_nau7802_gain_save(uint8_t gain)
{
    if (gain > 7) {
        ESP_LOGE(TAG, "Invalid NAU7802 gain value: %d (must be 0-7)", gain);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_GAIN, &gain, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 gain: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 gain: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 gain saved to NVS: %d (x%d)", gain, 1 << gain);
    return true;
}

// NAU7802 Sample Rate (0=10, 1=20, 2=40, 3=80, 7=320 SPS)
uint8_t system_nau7802_sample_rate_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS not available, using default sample rate (80 SPS)");
        return 3;  // Default: NAU7802_SPS_80
    }
    
    uint8_t sample_rate = 3;  // Default: NAU7802_SPS_80
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_SAMPLE_RATE, &sample_rate, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "NAU7802 sample rate not set, using default (80 SPS)");
        return 3;
    }
    
    if (err != ESP_OK || required_size != sizeof(uint8_t)) {
        ESP_LOGW(TAG, "Failed to load NAU7802 sample rate, using default (80 SPS)");
        return 3;
    }
    
    // Valid values: 0, 1, 2, 3, 7
    if (sample_rate != 0 && sample_rate != 1 && sample_rate != 2 && sample_rate != 3 && sample_rate != 7) {
        ESP_LOGW(TAG, "Invalid NAU7802 sample rate value (%d), using default (80 SPS)", sample_rate);
        return 3;
    }
    
    const char *sps_str = (sample_rate == 0) ? "10" : (sample_rate == 1) ? "20" : 
                          (sample_rate == 2) ? "40" : (sample_rate == 3) ? "80" : "320";
    ESP_LOGI(TAG, "NAU7802 sample rate loaded from NVS: %d (%s SPS)", sample_rate, sps_str);
    return sample_rate;
}

bool system_nau7802_sample_rate_save(uint8_t sample_rate)
{
    // Valid values: 0, 1, 2, 3, 7
    if (sample_rate != 0 && sample_rate != 1 && sample_rate != 2 && sample_rate != 3 && sample_rate != 7) {
        ESP_LOGE(TAG, "Invalid NAU7802 sample rate value: %d (must be 0, 1, 2, 3, or 7)", sample_rate);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_SAMPLE_RATE, &sample_rate, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 sample rate: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 sample rate: %s", esp_err_to_name(err));
        return false;
    }
    
    const char *sps_str = (sample_rate == 0) ? "10" : (sample_rate == 1) ? "20" : 
                          (sample_rate == 2) ? "40" : (sample_rate == 3) ? "80" : "320";
    ESP_LOGI(TAG, "NAU7802 sample rate saved to NVS: %d (%s SPS)", sample_rate, sps_str);
    return true;
}

// NAU7802 Channel (0=Channel 1, 1=Channel 2)
uint8_t system_nau7802_channel_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS not available, using default channel (Channel 1)");
        return 0;  // Default: NAU7802_CHANNEL_1
    }
    
    uint8_t channel = 0;  // Default: NAU7802_CHANNEL_1
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_CHANNEL, &channel, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "NAU7802 channel not set, using default (Channel 1)");
        return 0;
    }
    
    if (err != ESP_OK || required_size != sizeof(uint8_t)) {
        ESP_LOGW(TAG, "Failed to load NAU7802 channel, using default (Channel 1)");
        return 0;
    }
    
    if (channel > 1) {
        ESP_LOGW(TAG, "Invalid NAU7802 channel value (%d), using default (Channel 1)", channel);
        return 0;
    }
    
    ESP_LOGI(TAG, "NAU7802 channel loaded from NVS: %d (Channel %d)", channel, channel + 1);
    return channel;
}

bool system_nau7802_channel_save(uint8_t channel)
{
    if (channel > 1) {
        ESP_LOGE(TAG, "Invalid NAU7802 channel value: %d (must be 0 or 1)", channel);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_CHANNEL, &channel, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 channel: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 channel: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 channel saved to NVS: %d (Channel %d)", channel, channel + 1);
    return true;
}

// NAU7802 LDO Voltage (0-7: 2.4V-4.5V)
uint8_t system_nau7802_ldo_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS not available, using default LDO (3.3V)");
        return 4;  // Default: 3.3V
    }
    
    uint8_t ldo = 4;  // Default: 3.3V
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_LDO, &ldo, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "NAU7802 LDO not set, using default (3.3V)");
        return 4;
    }
    
    if (err != ESP_OK || required_size != sizeof(uint8_t)) {
        ESP_LOGW(TAG, "Failed to load NAU7802 LDO, using default (3.3V)");
        return 4;
    }
    
    if (ldo > 7) {
        ESP_LOGW(TAG, "Invalid NAU7802 LDO value (%d), using default (3.3V)", ldo);
        return 4;
    }
    
    const float voltages[] = {4.5f, 4.2f, 3.9f, 3.6f, 3.3f, 3.0f, 2.7f, 2.4f};
    ESP_LOGI(TAG, "NAU7802 LDO loaded from NVS: %d (%.1fV)", ldo, voltages[ldo]);
    return ldo;
}

bool system_nau7802_ldo_save(uint8_t ldo)
{
    if (ldo > 7) {
        ESP_LOGE(TAG, "Invalid NAU7802 LDO value: %d (must be 0-7)", ldo);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_LDO, &ldo, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 LDO: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NAU7802 LDO: %s", esp_err_to_name(err));
        return false;
    }
    
    const float voltages[] = {4.5f, 4.2f, 3.9f, 3.6f, 3.3f, 3.0f, 2.7f, 2.4f};
    ESP_LOGI(TAG, "NAU7802 LDO saved to NVS: %d (%.1fV)", ldo, voltages[ldo]);
    return true;
}

uint8_t system_nau7802_average_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS not available, using default average (1 sample)");
        return 1;  // Default: no averaging (single reading)
    }
    
    uint8_t average = 1;  // Default: no averaging
    size_t required_size = sizeof(uint8_t);
    err = nvs_get_blob(handle, NVS_KEY_NAU7802_AVERAGE, &average, &required_size);
    nvs_close(handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "NAU7802 average not set, using default (1 sample)");
        return 1;
    }
    
    if (err != ESP_OK || required_size != sizeof(uint8_t)) {
        ESP_LOGW(TAG, "Failed to load NAU7802 average, using default (1 sample)");
        return 1;
    }
    
    // Validate range (1-50)
    if (average < 1) {
        ESP_LOGW(TAG, "Invalid NAU7802 average value: %d (must be 1-50), using default (1)", average);
        return 1;
    }
    if (average > 50) {
        ESP_LOGW(TAG, "Invalid NAU7802 average value: %d (must be 1-50), clamping to 50", average);
        return 50;
    }
    
    return average;
}

bool system_nau7802_average_save(uint8_t average)
{
    if (average < 1 || average > 50) {
        ESP_LOGE(TAG, "Invalid NAU7802 average value: %d (must be 1-50)", average);
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_NAU7802_AVERAGE, &average, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save NAU7802 average: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes for NAU7802 average: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NAU7802 average saved: %d samples", average);
    return true;
}


