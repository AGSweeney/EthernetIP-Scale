/**
 * @file example_calibration.c
 * @brief Example showing how to calibrate the NAU7802 scale
 * 
 * This example demonstrates:
 * - Calculating zero offset (tare)
 * - Calculating calibration factor with a known weight
 * - Saving and loading calibration from NVS
 * 
 * Copyright (c) 2025 Adam G. Sweeney <agsweeney@gmail.com>
 * Copyright (c) 2019 SparkFun Electronics
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nau7802.h"
#include "nau7802_calibration_storage.h"

static const char *TAG = "example_calibration";

#define I2C_MASTER_SCL_IO           6
#define I2C_MASTER_SDA_IO           5
#define I2C_MASTER_NUM              0
#define CALIBRATION_SAMPLES         64
#define CALIBRATION_TIMEOUT_MS      2000
#define KNOWN_WEIGHT_GRAMS          100.0f

/**
 * @brief Perform two-point calibration of the scale
 * 
 * This function performs a complete calibration sequence:
 * 1. Zero offset calculation (tare) - scale must be empty
 * 2. Calibration factor calculation - known weight must be placed
 * 3. Save calibration data to NVS for persistence
 * 
 * @param scale Pointer to initialized NAU7802 device structure
 * @param known_weight_grams Known weight value in grams (or any unit)
 */
static void calibrate_scale(nau7802_t *scale, float known_weight_grams)
{
    ESP_LOGI(TAG, "=== Starting Scale Calibration ===");
    
    // Step 1: Zero Offset (Tare)
    // Remove all weight from the scale and wait for stability
    ESP_LOGI(TAG, "Step 1: Remove all weight from scale");
    ESP_LOGI(TAG, "Waiting 3 seconds for scale to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Calculate zero offset by averaging multiple readings
    ESP_LOGI(TAG, "Calculating zero offset (averaging %d samples)...", CALIBRATION_SAMPLES);
    esp_err_t ret = nau7802_calculate_zero_offset(scale, CALIBRATION_SAMPLES, CALIBRATION_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate zero offset: %s", esp_err_to_name(ret));
        return;
    }
    
    float zero_offset = nau7802_get_zero_offset(scale);
    ESP_LOGI(TAG, "Zero offset calculated: %.2f", zero_offset);
    
    // Step 2: Calibration Factor
    // Place a known weight on the scale and wait for stability
    ESP_LOGI(TAG, "Step 2: Place %.2f grams on the scale", known_weight_grams);
    ESP_LOGI(TAG, "Waiting 5 seconds for weight to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Calculate calibration factor using the known weight
    // Formula: calibration_factor = (reading - zero_offset) / known_weight
    ESP_LOGI(TAG, "Calculating calibration factor (averaging %d samples)...", CALIBRATION_SAMPLES);
    ret = nau7802_calculate_calibration_factor(scale, known_weight_grams, CALIBRATION_SAMPLES, CALIBRATION_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate calibration factor: %s", esp_err_to_name(ret));
        return;
    }
    
    float cal_factor = nau7802_get_calibration_factor(scale);
    ESP_LOGI(TAG, "Calibration factor calculated: %.6f", cal_factor);
    
    // Step 3: Save calibration to NVS
    // Read current calibration values from device
    nau7802_calibration_data_t cal_data;
    ret = nau7802_calibration_read_from_device(scale, &cal_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read calibration from device");
        return;
    }
    
    // Save to NVS for persistence across power cycles
    ret = nau7802_calibration_save(&cal_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save calibration to NVS: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "=== Calibration Complete ===");
    ESP_LOGI(TAG, "Zero offset: %.2f", cal_data.zero_offset);
    ESP_LOGI(TAG, "Calibration factor: %.6f", cal_data.calibration_factor);
    ESP_LOGI(TAG, "Calibration saved to NVS");
}

/**
 * @brief Main application entry point
 * 
 * This example demonstrates:
 * 1. Initializing NVS for calibration storage
 * 2. Initializing I2C and NAU7802
 * 3. Loading saved calibration from NVS (if available)
 * 4. Performing calibration if none exists
 * 5. Reading calibrated weight values
 */
void app_main(void)
{
    ESP_LOGI(TAG, "NAU7802 Calibration Example");

    // Step 1: Initialize NVS flash for calibration storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Step 2: Configure and initialize I2C bus
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.i2c_port = I2C_MASTER_NUM;
    i2c_bus_config.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_bus_config.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    // Step 3: Initialize NAU7802 device
    nau7802_t scale;
    ret = nau7802_init(&scale, i2c_bus_handle, NAU7802_I2C_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NAU7802 driver");
        return;
    }

    ret = nau7802_begin(&scale);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to begin NAU7802: %s", esp_err_to_name(ret));
        return;
    }

    // Step 4: Try to load saved calibration from NVS
    nau7802_calibration_data_t cal_data;
    ret = nau7802_calibration_load(&cal_data);
    if (ret == ESP_OK && cal_data.is_valid) {
        // Calibration found - apply it to the device
        ESP_LOGI(TAG, "Loading calibration from NVS...");
        nau7802_calibration_apply(&scale, &cal_data);
        ESP_LOGI(TAG, "Calibration loaded: factor=%.6f, offset=%.2f", 
                 cal_data.calibration_factor, cal_data.zero_offset);
    } else {
        // No valid calibration - perform calibration sequence
        ESP_LOGW(TAG, "No valid calibration found. Starting calibration...");
        calibrate_scale(&scale, KNOWN_WEIGHT_GRAMS);
    }

    // Step 5: Main loop - read calibrated weight values
    ESP_LOGI(TAG, "Starting continuous readings...");
    
    while (1) {
        if (nau7802_available(&scale)) {
            // Get weight with averaging (8 samples) for stability
            float weight = nau7802_get_weight(&scale, false, 8, 1000);
            ESP_LOGI(TAG, "Weight: %.2f g", weight);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

