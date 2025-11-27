/**
 * @file example_interrupt.c
 * @brief Example showing how to use interrupts with the NAU7802
 * 
 * This example demonstrates:
 * - Configuring interrupt polarity
 * - Using GPIO interrupt to detect when data is ready
 * - Reading data when interrupt occurs
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
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nau7802.h"

static const char *TAG = "example_interrupt";

#define I2C_MASTER_SCL_IO           6
#define I2C_MASTER_SDA_IO           5
#define I2C_MASTER_NUM              0
#define CRDY_GPIO                   7

static QueueHandle_t gpio_evt_queue = NULL;  /**< Queue for GPIO interrupt events */

/**
 * @brief GPIO interrupt service routine
 * 
 * This ISR is called when the CRDY pin changes state (data ready).
 * It sends the GPIO number to a queue for processing in the task context.
 * 
 * @param arg GPIO number (cast to void*)
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/**
 * @brief GPIO task to process interrupt events
 * 
 * This task waits for GPIO interrupt events from the queue and reads
 * weight data when the CRDY interrupt occurs.
 * 
 * @param arg Pointer to NAU7802 device structure
 */
static void gpio_task(void* arg)
{
    nau7802_t *scale = (nau7802_t *)arg;
    uint32_t io_num;
    
    while (1) {
        // Wait for interrupt event from queue
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (io_num == CRDY_GPIO) {
                // CRDY interrupt occurred - check if data is ready
                if (nau7802_available(scale)) {
                    // Read the data
                    int32_t reading = nau7802_get_reading(scale);
                    float weight = nau7802_get_weight(scale, false, 1, 0);
                    ESP_LOGI(TAG, "Interrupt! Reading: %ld, Weight: %.2f", reading, weight);
                }
            }
        }
    }
}

/**
 * @brief Main application entry point
 * 
 * This example demonstrates interrupt-driven data acquisition:
 * 1. Initialize I2C and NAU7802
 * 2. Configure interrupt polarity
 * 3. Setup GPIO interrupt on CRDY pin
 * 4. Process interrupts asynchronously
 */
void app_main(void)
{
    ESP_LOGI(TAG, "NAU7802 Interrupt Example");

    // Step 1: Configure and initialize I2C bus
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.i2c_port = I2C_MASTER_NUM;
    i2c_bus_config.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_bus_config.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    // Step 2: Initialize NAU7802 device
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

    // Step 3: Configure interrupt polarity
    // Set CRDY pin to be HIGH when data is ready (default)
    ret = nau7802_set_int_polarity_high(&scale);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interrupt polarity");
        return;
    }

    // Step 4: Configure GPIO for interrupt
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // Trigger on rising edge (high)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CRDY_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;  // Enable pull-up resistor
    gpio_config(&io_conf);

    // Step 5: Create queue and task for interrupt processing
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, &scale, 10, NULL);

    // Step 6: Install GPIO ISR service and add handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CRDY_GPIO, gpio_isr_handler, (void*) CRDY_GPIO);

    ESP_LOGI(TAG, "Interrupt configured on GPIO %d", CRDY_GPIO);
    ESP_LOGI(TAG, "CRDY pin will go HIGH when data is ready");
    ESP_LOGI(TAG, "Waiting for data ready interrupts...");

    // Main loop - interrupt processing happens in gpio_task
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

