/**
 * @file example_basic.c
 * @brief Basic example showing how to use the NAU7802 driver
 * 
 * This example demonstrates:
 * - Initializing the I2C bus
 * - Initializing the NAU7802 device
 * - Reading weight values continuously
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
#include "nau7802.h"

static const char *TAG = "example_basic";

#define I2C_MASTER_SCL_IO           6
#define I2C_MASTER_SDA_IO           5
#define I2C_MASTER_NUM              0
#define READING_SAMPLE_COUNT        8      // Number of samples to average for weight readings
#define READING_TIMEOUT_MS          1000   // Timeout for reading samples

/**
 * @brief Main application entry point
 * 
 * This example demonstrates the basic usage of the NAU7802 driver:
 * 1. Initialize I2C bus
 * 2. Initialize NAU7802 device
 * 3. Check device connection
 * 4. Complete device initialization
 * 5. Continuously read weight values
 */
void app_main(void)
{
    ESP_LOGI(TAG, "NAU7802 Basic Example");

    // Step 1: Configure I2C bus
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.i2c_port = I2C_MASTER_NUM;
    i2c_bus_config.sda_io_num = I2C_MASTER_SDA_IO;      // GPIO 5 for SDA
    i2c_bus_config.scl_io_num = I2C_MASTER_SCL_IO;      // GPIO 6 for SCL
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true; // Enable internal pullups

    // Step 2: Initialize I2C master bus
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    // Step 3: Initialize NAU7802 device structure
    nau7802_t scale;
    ret = nau7802_init(&scale, i2c_bus_handle, NAU7802_I2C_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NAU7802 driver");
        return;
    }

    // Step 4: Verify device is connected and responding
    if (!nau7802_is_connected(&scale)) {
        ESP_LOGE(TAG, "NAU7802 not detected");
        return;
    }

    // Step 5: Complete initialization sequence (reset, power-up, calibration)
    ret = nau7802_begin(&scale);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to begin NAU7802: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Reading weight values...");
    ESP_LOGI(TAG, "Note: Weight values will be 0 until calibration is performed");
    ESP_LOGI(TAG, "Using %d samples per reading for averaging", READING_SAMPLE_COUNT);
    
    // Step 6: Main loop - continuously read weight values
    while (1) {
        // Check if a new reading is available
        if (nau7802_available(&scale)) {
            // Get raw ADC reading (24-bit signed integer)
            int32_t reading = nau7802_get_reading(&scale);
            
            // Get calibrated weight with averaging (multiple samples) for stability
            // Parameters: allow_negative=false, sample_count=READING_SAMPLE_COUNT, timeout=READING_TIMEOUT_MS
            float weight = nau7802_get_weight(&scale, false, READING_SAMPLE_COUNT, READING_TIMEOUT_MS);
            
            ESP_LOGI(TAG, "Reading: %ld, Weight: %.2f", reading, weight);
        }
        
        // Small delay to avoid busy-waiting
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

