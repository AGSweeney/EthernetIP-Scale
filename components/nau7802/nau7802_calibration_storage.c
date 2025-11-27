/*
 * ESP-IDF calibration storage helper for NAU7802 driver
 * 
 * Copyright (c) 2025 Adam G. Sweeney <agsweeney@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nau7802_calibration_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "cal_storage";

#define NVS_KEY_CAL_FACTOR "cal_factor"
#define NVS_KEY_ZERO_OFFSET "zero_offset"
#define NVS_KEY_CH1_OFFSET "ch1_offset"
#define NVS_KEY_IS_VALID "is_valid"

esp_err_t nau7802_calibration_load(nau7802_calibration_data_t *cal_data)
{
    if (cal_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        cal_data->is_valid = false;
        return ret;
    }

    size_t required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_CAL_FACTOR, &cal_data->calibration_factor, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration factor not found in NVS");
        nvs_close(nvs_handle);
        cal_data->is_valid = false;
        return ret;
    }

    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_ZERO_OFFSET, &cal_data->zero_offset, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Zero offset not found in NVS");
        nvs_close(nvs_handle);
        cal_data->is_valid = false;
        return ret;
    }

    required_size = sizeof(int32_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_CH1_OFFSET, &cal_data->channel1_offset, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Channel 1 offset not found in NVS, defaulting to 0");
        cal_data->channel1_offset = 0;
    }

    uint8_t is_valid = 0;
    required_size = sizeof(uint8_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_IS_VALID, &is_valid, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "is_valid flag not found in NVS");
        cal_data->is_valid = false;
    } else {
        cal_data->is_valid = (is_valid != 0);
    }

    nvs_close(nvs_handle);

    if (cal_data->calibration_factor == 1.0f && cal_data->zero_offset == 0.0f) {
        ESP_LOGW(TAG, "Default calibration values detected, marking as invalid");
        cal_data->is_valid = false;
    }

    ESP_LOGI(TAG, "Loaded calibration: factor=%.6f, offset=%.2f, ch1_offset=%ld, valid=%d",
             cal_data->calibration_factor, cal_data->zero_offset, 
             cal_data->channel1_offset, cal_data->is_valid);

    return ESP_OK;
}

esp_err_t nau7802_calibration_save(const nau7802_calibration_data_t *cal_data)
{
    if (cal_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, NVS_KEY_CAL_FACTOR, &cal_data->calibration_factor, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save calibration factor: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, NVS_KEY_ZERO_OFFSET, &cal_data->zero_offset, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save zero offset: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, NVS_KEY_CH1_OFFSET, &cal_data->channel1_offset, sizeof(int32_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save channel 1 offset: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    uint8_t is_valid = cal_data->is_valid ? 1 : 0;
    ret = nvs_set_blob(nvs_handle, NVS_KEY_IS_VALID, &is_valid, sizeof(uint8_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save is_valid flag: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Calibration saved successfully");
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t nau7802_calibration_apply(nau7802_t *scale, const nau7802_calibration_data_t *cal_data)
{
    if (scale == NULL || cal_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nau7802_set_zero_offset(scale, cal_data->zero_offset);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set zero offset");
        return ret;
    }

    ret = nau7802_set_calibration_factor(scale, cal_data->calibration_factor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set calibration factor");
        return ret;
    }

    if (cal_data->channel1_offset != 0) {
        ret = nau7802_set_channel1_offset(scale, cal_data->channel1_offset);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set channel 1 offset (non-critical)");
        }
    }

    ESP_LOGI(TAG, "Calibration applied: factor=%.6f, offset=%.2f, ch1_offset=%ld",
             cal_data->calibration_factor, cal_data->zero_offset, cal_data->channel1_offset);

    return ESP_OK;
}

esp_err_t nau7802_calibration_read_from_device(nau7802_t *scale, nau7802_calibration_data_t *cal_data)
{
    if (scale == NULL || cal_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cal_data->calibration_factor = nau7802_get_calibration_factor(scale);
    cal_data->zero_offset = nau7802_get_zero_offset(scale);
    cal_data->channel1_offset = nau7802_get_channel1_offset(scale);
    cal_data->is_valid = true;

    ESP_LOGI(TAG, "Read calibration from device: factor=%.6f, offset=%.2f, ch1_offset=%ld",
             cal_data->calibration_factor, cal_data->zero_offset, cal_data->channel1_offset);

    return ESP_OK;
}

esp_err_t nau7802_calibration_erase(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    nvs_erase_key(nvs_handle, NVS_KEY_CAL_FACTOR);
    nvs_erase_key(nvs_handle, NVS_KEY_ZERO_OFFSET);
    nvs_erase_key(nvs_handle, NVS_KEY_CH1_OFFSET);
    nvs_erase_key(nvs_handle, NVS_KEY_IS_VALID);

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration data erased from NVS");
    }

    return ret;
}

