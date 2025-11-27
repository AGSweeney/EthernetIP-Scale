/*
 * ESP-IDF driver for NAU7802 24-bit wheatstone bridge and load cell amplifier
 * 
 * This driver is based on the SparkFun Qwiic Scale NAU7802 Arduino Library
 * Original Arduino library by Nathan Seidle @ SparkFun Electronics, March 3rd, 2019
 * 
 * ESP-IDF port and enhancements by Adam G. Sweeney <agsweeney@gmail.com>
 * 
 * Copyright (c) 2025 Adam G. Sweeney
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

#include "nau7802.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "NAU7802";

static esp_err_t nau7802_read_register(nau7802_t *dev, uint8_t reg, uint8_t *data)
{
    uint8_t write_data = reg;
    esp_err_t ret = i2c_master_transmit_receive(dev->i2c_dev, &write_data, 1, data, 1, 100);
    return ret;
}

static esp_err_t nau7802_write_register(nau7802_t *dev, uint8_t reg, uint8_t data)
{
    uint8_t write_data[2] = {reg, data};
    esp_err_t ret = i2c_master_transmit(dev->i2c_dev, write_data, 2, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write register 0x%02X=0x%02X failed: %s (0x%x)", reg, data, esp_err_to_name(ret), ret);
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
    
    uint8_t verify;
    ret = nau7802_read_register(dev, reg, &verify);
    if (ret == ESP_OK && verify != data && reg != NAU7802_REGISTER_PU_CTRL) {
        ESP_LOGW(TAG, "Register 0x%02X write verification failed: wrote 0x%02X, read 0x%02X", reg, data, verify);
    }
    
    return ESP_OK;
}

static esp_err_t nau7802_set_register_bit(nau7802_t *dev, uint8_t reg, uint8_t bit)
{
    uint8_t value;
    esp_err_t ret = nau7802_read_register(dev, reg, &value);
    if (ret != ESP_OK) return ret;
    
    value |= (1 << bit);
    return nau7802_write_register(dev, reg, value);
}

static esp_err_t nau7802_clear_register_bit(nau7802_t *dev, uint8_t reg, uint8_t bit)
{
    uint8_t value;
    esp_err_t ret = nau7802_read_register(dev, reg, &value);
    if (ret != ESP_OK) return ret;
    
    value &= ~(1 << bit);
    return nau7802_write_register(dev, reg, value);
}

/**
 * @brief Initialize the NAU7802 device structure
 * 
 * Initializes the device structure with default values and adds the device
 * to the I2C bus. After calling this function, call nau7802_begin() to
 * complete the initialization sequence.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param i2c_bus I2C master bus handle (must be initialized separately)
 * @param address I2C device address (typically NAU7802_I2C_ADDRESS)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if dev or i2c_bus is NULL
 */
esp_err_t nau7802_init(nau7802_t *dev, i2c_master_bus_handle_t i2c_bus, uint8_t address)
{
    if (dev == NULL || i2c_bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(dev, 0, sizeof(nau7802_t));
    dev->i2c_bus = i2c_bus;
    dev->address = address;
    dev->calibration_factor = 1.0f;
    dev->zero_offset = 0.0f;
    dev->ldo_ramp_delay = 250;
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t nau7802_reset(nau7802_t *dev)
{
    esp_err_t ret = nau7802_set_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_RR);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ret = nau7802_clear_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_RR);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(1));
    return ESP_OK;
}

esp_err_t nau7802_power_up(nau7802_t *dev)
{
    esp_err_t ret = nau7802_set_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_PUD);
    if (ret != ESP_OK) return ret;
    
    ret = nau7802_set_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_PUA);
    if (ret != ESP_OK) return ret;
    
    uint8_t counter = 0;
    uint8_t pwr;
    while (counter < 100) {
        ret = nau7802_read_register(dev, NAU7802_REGISTER_PU_CTRL, &pwr);
        if (ret != ESP_OK) return ret;
        if (pwr & (1 << NAU7802_PU_CTRL_PUR)) break;
        vTaskDelay(pdMS_TO_TICKS(1));
        counter++;
    }
    if (counter >= 100) {
        ESP_LOGE(TAG, "Power up timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    return nau7802_set_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_CS);
}

esp_err_t nau7802_power_down(nau7802_t *dev)
{
    esp_err_t ret = nau7802_clear_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_PUD);
    if (ret != ESP_OK) return ret;
    
    ret = nau7802_clear_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_PUA);
    if (ret != ESP_OK) return ret;
    
    ret = nau7802_clear_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_AVDDS);
    if (ret != ESP_OK) return ret;
    
    ret = nau7802_clear_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_OSCS);
    return ret;
}

esp_err_t nau7802_set_ldo(nau7802_t *dev, uint8_t ldo_value)
{
    uint8_t ctrl1;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL1, &ctrl1);
    if (ret != ESP_OK) return ret;
    ctrl1 &= 0xC7;
    ctrl1 |= (ldo_value & 0x07) << 3;
    ret = nau7802_write_register(dev, NAU7802_REGISTER_CTRL1, ctrl1);
    if (ret != ESP_OK) return ret;
    return nau7802_set_register_bit(dev, NAU7802_REGISTER_PU_CTRL, NAU7802_PU_CTRL_AVDDS);
}

/**
 * @brief Complete initialization and configure the NAU7802 device
 * 
 * Performs the full initialization sequence including reset, power-up,
 * configuration, and AFE calibration. This function should be called
 * after nau7802_init().
 * 
 * @param dev Pointer to NAU7802 device structure (must be initialized with nau7802_init())
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_RESPONSE on calibration failure
 */
esp_err_t nau7802_begin(nau7802_t *dev)
{
    esp_err_t ret = nau7802_reset(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed");
        return ret;
    }
    
    ret = nau7802_power_up(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Power up failed");
        return ret;
    }
    
    ret = nau7802_set_ldo(dev, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set LDO failed");
        return ret;
    }
    
    ret = nau7802_set_gain(dev, NAU7802_GAIN_128);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set gain failed");
        return ret;
    }
    
    ret = nau7802_set_sample_rate(dev, NAU7802_SPS_80);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set sample rate failed");
        return ret;
    }
    
    uint8_t adc;
    ret = nau7802_read_register(dev, NAU7802_REGISTER_ADC, &adc);
    if (ret != ESP_OK) return ret;
    adc |= 0x30;
    ret = nau7802_write_register(dev, NAU7802_REGISTER_ADC, adc);
    if (ret != ESP_OK) return ret;
    
    ret = nau7802_set_register_bit(dev, NAU7802_REGISTER_POWER, 7);
    if (ret != ESP_OK) return ret;
    
    uint8_t pga_value;
    ret = nau7802_read_register(dev, NAU7802_REGISTER_PGA, &pga_value);
    if (ret != ESP_OK) return ret;
    pga_value &= ~(1 << 5);
    ret = nau7802_write_register(dev, NAU7802_REGISTER_PGA, pga_value);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(250));
    
    for (int i = 0; i < 10; i++) {
        uint8_t pwr;
        ret = nau7802_read_register(dev, NAU7802_REGISTER_PU_CTRL, &pwr);
        if (ret == ESP_OK && (pwr & (1 << NAU7802_PU_CTRL_CR))) {
            uint8_t adc_reg = NAU7802_REGISTER_ADC_DATA;
            uint8_t adc_data[3];
            i2c_master_transmit_receive(dev->i2c_dev, &adc_reg, 1, adc_data, 3, 100);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ret = nau7802_calibrate_af(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AFE calibration failed");
        return ret;
    }
    
    ESP_LOGI(TAG, "NAU7802 initialized successfully");
    return ESP_OK;
}

esp_err_t nau7802_set_gain(nau7802_t *dev, nau7802_gain_t gain)
{
    uint8_t ctrl1;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL1, &ctrl1);
    if (ret != ESP_OK) return ret;
    
    ctrl1 &= ~NAU7802_CTRL1_GAIN_MASK;
    ctrl1 |= (gain & NAU7802_CTRL1_GAIN_MASK);
    
    return nau7802_write_register(dev, NAU7802_REGISTER_CTRL1, ctrl1);
}

esp_err_t nau7802_set_sample_rate(nau7802_t *dev, nau7802_sps_t rate)
{
    uint8_t ctrl2;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) return ret;
    
    ctrl2 &= ~NAU7802_CTRL2_CRS_MASK;
    ctrl2 |= ((rate & 0x07) << 4);
    
    return nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
}

esp_err_t nau7802_set_channel(nau7802_t *dev, nau7802_channel_t channel)
{
    uint8_t adc;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_ADC, &adc);
    if (ret != ESP_OK) return ret;
    
    if (channel == NAU7802_CHANNEL_1) {
        adc &= ~NAU7802_ADC_CHANNEL_MASK;
    } else {
        adc |= NAU7802_ADC_CHANNEL_MASK;
    }
    
    return nau7802_write_register(dev, NAU7802_REGISTER_ADC, adc);
}

esp_err_t nau7802_calibrate_af(nau7802_t *dev)
{
    return nau7802_calibrate_af_mode(dev, NAU7802_CALMOD_INTERNAL);
}

esp_err_t nau7802_calibrate_af_mode(nau7802_t *dev, nau7802_cal_mode_t mode)
{
    uint8_t ctrl2;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CTRL2 before calibration");
        return ret;
    }
    
    ctrl2 &= ~NAU7802_CTRL2_CALMOD_MASK;
    ctrl2 |= (mode & NAU7802_CTRL2_CALMOD_MASK);
    ret = nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set calibration mode");
        return ret;
    }
    
    ESP_LOGI(TAG, "Starting AF calibration mode %d, CTRL2=0x%02X", mode, ctrl2);
    ctrl2 |= NAU7802_CTRL2_CALS;
    ret = nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write CALS bit");
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) return ret;
    
    if ((ctrl2 & NAU7802_CTRL2_CALS) == 0) {
        ESP_LOGW(TAG, "CALS bit not set after write, CTRL2=0x%02X", ctrl2);
    }
    
    for (int i = 0; i < 100; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
        if (ret != ESP_OK) return ret;
        
        if ((ctrl2 & NAU7802_CTRL2_CALS) == 0) {
            if (ctrl2 & NAU7802_CTRL2_CAL_ERROR) {
                ESP_LOGE(TAG, "Calibration error, CTRL2=0x%02X", ctrl2);
                ctrl2 &= ~NAU7802_CTRL2_CALS;
                nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
                return ESP_ERR_INVALID_RESPONSE;
            }
            ESP_LOGI(TAG, "Calibration completed successfully");
            return ESP_OK;
        }
        
        if (i % 10 == 0) {
            ESP_LOGI(TAG, "Calibration in progress, CTRL2=0x%02X (iteration %d)", ctrl2, i);
        }
    }
    
    ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    ESP_LOGW(TAG, "Calibration timeout, clearing CALS bit. Final CTRL2=0x%02X", ctrl2);
    ctrl2 &= ~NAU7802_CTRL2_CALS;
    nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
    return ESP_ERR_TIMEOUT;
}

esp_err_t nau7802_calibrate_system_offset(nau7802_t *dev, nau7802_channel_t channel)
{
    esp_err_t ret = nau7802_set_channel(dev, channel);
    if (ret != ESP_OK) return ret;
    
    uint8_t ctrl2;
    ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) return ret;
    
    ctrl2 &= ~NAU7802_CTRL2_CALMOD_MASK;
    ret = nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
    if (ret != ESP_OK) return ret;
    
    return nau7802_calibrate_af(dev);
}

esp_err_t nau7802_calibrate_system_gain(nau7802_t *dev, nau7802_channel_t channel)
{
    esp_err_t ret = nau7802_set_channel(dev, channel);
    if (ret != ESP_OK) return ret;
    
    uint8_t ctrl2;
    ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) return ret;
    
    ctrl2 |= NAU7802_CTRL2_CALMOD_MASK;
    ret = nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
    if (ret != ESP_OK) return ret;
    
    return nau7802_calibrate_af(dev);
}

bool nau7802_available(nau7802_t *dev)
{
    uint8_t pwr;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_PU_CTRL, &pwr);
    if (ret != ESP_OK) {
        return false;
    }
    return (pwr & (1 << NAU7802_PU_CTRL_CR)) != 0;
}

int32_t nau7802_get_reading(nau7802_t *dev)
{
    uint8_t data[3];
    uint8_t reg = NAU7802_REGISTER_ADC_DATA;
    
    esp_err_t ret = i2c_master_transmit_receive(dev->i2c_dev, &reg, 1, data, 3, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC data: %d", ret);
        return 0;
    }
    
    int32_t value = ((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2];
    
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }
    
    return value;
}

int32_t nau7802_get_channel1_offset(nau7802_t *dev)
{
    uint8_t data[3];
    uint8_t reg = NAU7802_REGISTER_OCAL1_BP2;
    
    esp_err_t ret = i2c_master_transmit_receive(dev->i2c_dev, &reg, 1, data, 3, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read channel 1 offset: %d", ret);
        return 0;
    }
    
    int32_t value = ((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2];
    
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }
    
    return value;
}

esp_err_t nau7802_set_channel1_offset(nau7802_t *dev, int32_t offset)
{
    uint8_t data[4];
    uint8_t reg = NAU7802_REGISTER_OCAL1_BP2;
    
    data[0] = reg;
    data[1] = (uint8_t)((offset >> 16) & 0xFF);
    data[2] = (uint8_t)((offset >> 8) & 0xFF);
    data[3] = (uint8_t)(offset & 0xFF);
    
    esp_err_t ret = i2c_master_transmit(dev->i2c_dev, data, 4, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write channel 1 offset: %d", ret);
        return ret;
    }
    
    return ESP_OK;
}

float nau7802_get_weight(nau7802_t *dev, bool allow_negative, uint8_t sample_count, uint32_t timeout_ms)
{
    int32_t reading;
    
    if (sample_count > 1) {
        reading = nau7802_get_average(dev, sample_count, timeout_ms);
    } else {
        reading = nau7802_get_reading(dev);
    }
    
    float weight = ((float)reading - dev->zero_offset) / dev->calibration_factor;
    
    if (!allow_negative && weight < 0) {
        weight = 0;
    }
    
    return weight;
}

/**
 * @brief Get the average of multiple ADC readings
 * 
 * Collects the specified number of samples and returns the average.
 * This function waits for each sample to become available before reading.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param sample_count Number of samples to average
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Average ADC reading, or 0 on error/timeout
 */
int32_t nau7802_get_average(nau7802_t *dev, uint8_t sample_count, uint32_t timeout_ms)
{
    int32_t total = 0;
    uint8_t samples_acquired = 0;
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while (samples_acquired < sample_count) {
        if (nau7802_available(dev)) {
            int32_t reading = nau7802_get_reading(dev);
            total += reading;
            samples_acquired++;
        }
        
        uint32_t elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - start_time;
        if (timeout_ms > 0 && elapsed > timeout_ms) {
            ESP_LOGW(TAG, "get_average timeout: got %d/%d samples", samples_acquired, sample_count);
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (samples_acquired == 0) {
        return 0;
    }
    
    return total / samples_acquired;
}

/**
 * @brief Calculate and set the zero offset (tare)
 * 
 * Averages multiple readings with nothing on the scale and sets this
 * as the zero offset. This should be called when the scale is empty
 * and stable.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param sample_count Number of samples to average
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t nau7802_calculate_zero_offset(nau7802_t *dev, uint8_t sample_count, uint32_t timeout_ms)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int32_t avg = nau7802_get_average(dev, sample_count, timeout_ms);
    dev->zero_offset = (float)avg;
    ESP_LOGI(TAG, "Zero offset calculated: %ld (%.2f)", avg, dev->zero_offset);
    return ESP_OK;
}

/**
 * @brief Calculate calibration factor using a known weight
 * 
 * Places a known weight on the scale, averages readings, and calculates
 * the calibration factor. The zero offset must be set first using
 * nau7802_calculate_zero_offset().
 * 
 * Formula: calibration_factor = (reading - zero_offset) / known_weight
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param known_weight Known weight in any units (grams, pounds, etc.)
 * @param sample_count Number of samples to average
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t nau7802_calculate_calibration_factor(nau7802_t *dev, float known_weight, uint8_t sample_count, uint32_t timeout_ms)
{
    if (dev == NULL || known_weight == 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int32_t avg = nau7802_get_average(dev, sample_count, timeout_ms);
    float new_factor = ((float)avg - dev->zero_offset) / known_weight;
    dev->calibration_factor = new_factor;
    ESP_LOGI(TAG, "Calibration factor calculated: %.6f (reading=%ld, offset=%.2f, weight=%.2f)", 
             new_factor, avg, dev->zero_offset, known_weight);
    return ESP_OK;
}

esp_err_t nau7802_set_zero_offset(nau7802_t *dev, float zero_offset)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    dev->zero_offset = zero_offset;
    return ESP_OK;
}

float nau7802_get_zero_offset(nau7802_t *dev)
{
    if (dev == NULL) {
        return 0.0f;
    }
    return dev->zero_offset;
}

esp_err_t nau7802_set_calibration_factor(nau7802_t *dev, float calibration_factor)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    dev->calibration_factor = calibration_factor;
    return ESP_OK;
}

float nau7802_get_calibration_factor(nau7802_t *dev)
{
    if (dev == NULL) {
        return 1.0f;
    }
    return dev->calibration_factor;
}

esp_err_t nau7802_start_conversion(nau7802_t *dev)
{
    uint8_t pwr;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_PU_CTRL, &pwr);
    if (ret != ESP_OK) return ret;
    
    ESP_LOGD(TAG, "Before conversion: PU_CTRL=0x%02X", pwr);
    
    pwr |= (1 << NAU7802_PU_CTRL_CS);
    ret = nau7802_write_register(dev, NAU7802_REGISTER_PU_CTRL, pwr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set CS bit");
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
    
    pwr &= ~(1 << NAU7802_PU_CTRL_CS);
    ret = nau7802_write_register(dev, NAU7802_REGISTER_PU_CTRL, pwr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear CS bit");
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ret = nau7802_read_register(dev, NAU7802_REGISTER_PU_CTRL, &pwr);
    if (ret != ESP_OK) return ret;
    ESP_LOGD(TAG, "After conversion start: PU_CTRL=0x%02X", pwr);
    
    return ESP_OK;
}

bool nau7802_is_connected(nau7802_t *dev)
{
    if (dev == NULL || dev->i2c_dev == NULL) {
        return false;
    }
    
    uint8_t dummy;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_PU_CTRL, &dummy);
    return (ret == ESP_OK);
}

esp_err_t nau7802_set_ldo_ramp_delay(nau7802_t *dev, uint32_t delay_ms)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    dev->ldo_ramp_delay = delay_ms;
    return ESP_OK;
}

uint32_t nau7802_get_ldo_ramp_delay(nau7802_t *dev)
{
    if (dev == NULL) {
        return 250;
    }
    return dev->ldo_ramp_delay;
}

esp_err_t nau7802_begin_calibrate_af(nau7802_t *dev, nau7802_cal_mode_t mode)
{
    uint8_t ctrl2;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) return ret;
    
    ctrl2 &= ~NAU7802_CTRL2_CALMOD_MASK;
    ctrl2 |= (mode & NAU7802_CTRL2_CALMOD_MASK);
    ret = nau7802_write_register(dev, NAU7802_REGISTER_CTRL2, ctrl2);
    if (ret != ESP_OK) return ret;
    
    return nau7802_set_register_bit(dev, NAU7802_REGISTER_CTRL2, 2);
}

nau7802_cal_status_t nau7802_cal_af_status(nau7802_t *dev)
{
    uint8_t ctrl2;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_CTRL2, &ctrl2);
    if (ret != ESP_OK) {
        return NAU7802_CAL_FAILURE;
    }
    
    if (ctrl2 & NAU7802_CTRL2_CALS) {
        return NAU7802_CAL_IN_PROGRESS;
    }
    
    if (ctrl2 & NAU7802_CTRL2_CAL_ERROR) {
        return NAU7802_CAL_FAILURE;
    }
    
    return NAU7802_CAL_SUCCESS;
}

esp_err_t nau7802_wait_for_calibrate_af(nau7802_t *dev, uint32_t timeout_ms)
{
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    nau7802_cal_status_t status;
    
    while (1) {
        status = nau7802_cal_af_status(dev);
        
        if (status == NAU7802_CAL_SUCCESS) {
            return ESP_OK;
        }
        
        if (status == NAU7802_CAL_FAILURE) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        
        if (timeout_ms > 0) {
            uint32_t elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - start_time;
            if (elapsed >= timeout_ms) {
                return ESP_ERR_TIMEOUT;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t nau7802_set_int_polarity_high(nau7802_t *dev)
{
    return nau7802_clear_register_bit(dev, NAU7802_REGISTER_CTRL1, 7);
}

esp_err_t nau7802_set_int_polarity_low(nau7802_t *dev)
{
    return nau7802_set_register_bit(dev, NAU7802_REGISTER_CTRL1, 7);
}

uint8_t nau7802_get_revision_code(nau7802_t *dev)
{
    uint8_t revision;
    esp_err_t ret = nau7802_read_register(dev, NAU7802_REGISTER_REVISION_ID, &revision);
    if (ret != ESP_OK) {
        return 0;
    }
    return revision & 0x0F;
}

uint32_t nau7802_get_channel1_gain(nau7802_t *dev)
{
    return nau7802_get_32bit_register(dev, NAU7802_REGISTER_GCAL1_BP3);
}

esp_err_t nau7802_set_channel1_gain(nau7802_t *dev, uint32_t gain)
{
    return nau7802_set_32bit_register(dev, NAU7802_REGISTER_GCAL1_BP3, gain);
}

int32_t nau7802_get_channel2_offset(nau7802_t *dev)
{
    uint8_t data[3];
    uint8_t reg = NAU7802_REGISTER_OCAL2_BP2;
    
    esp_err_t ret = i2c_master_transmit_receive(dev->i2c_dev, &reg, 1, data, 3, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read channel 2 offset: %d", ret);
        return 0;
    }
    
    int32_t value = ((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2];
    
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }
    
    return value;
}

esp_err_t nau7802_set_channel2_offset(nau7802_t *dev, int32_t offset)
{
    uint8_t data[4];
    uint8_t reg = NAU7802_REGISTER_OCAL2_BP2;
    
    data[0] = reg;
    data[1] = (uint8_t)((offset >> 16) & 0xFF);
    data[2] = (uint8_t)((offset >> 8) & 0xFF);
    data[3] = (uint8_t)(offset & 0xFF);
    
    esp_err_t ret = i2c_master_transmit(dev->i2c_dev, data, 4, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write channel 2 offset: %d", ret);
        return ret;
    }
    
    return ESP_OK;
}

uint32_t nau7802_get_channel2_gain(nau7802_t *dev)
{
    return nau7802_get_32bit_register(dev, NAU7802_REGISTER_GCAL2_BP3);
}

esp_err_t nau7802_set_channel2_gain(nau7802_t *dev, uint32_t gain)
{
    return nau7802_set_32bit_register(dev, NAU7802_REGISTER_GCAL2_BP3, gain);
}

uint8_t nau7802_get_register(nau7802_t *dev, uint8_t reg)
{
    uint8_t value;
    esp_err_t ret = nau7802_read_register(dev, reg, &value);
    if (ret != ESP_OK) {
        return 0xFF;
    }
    return value;
}

esp_err_t nau7802_set_register(nau7802_t *dev, uint8_t reg, uint8_t value)
{
    return nau7802_write_register(dev, reg, value);
}

bool nau7802_get_bit(nau7802_t *dev, uint8_t reg, uint8_t bit)
{
    uint8_t value;
    esp_err_t ret = nau7802_read_register(dev, reg, &value);
    if (ret != ESP_OK) {
        return false;
    }
    return (value & (1 << bit)) != 0;
}

esp_err_t nau7802_set_bit(nau7802_t *dev, uint8_t reg, uint8_t bit)
{
    return nau7802_set_register_bit(dev, reg, bit);
}

esp_err_t nau7802_clear_bit(nau7802_t *dev, uint8_t reg, uint8_t bit)
{
    return nau7802_clear_register_bit(dev, reg, bit);
}

int32_t nau7802_get_24bit_register(nau7802_t *dev, uint8_t reg)
{
    uint8_t data[3];
    esp_err_t ret = i2c_master_transmit_receive(dev->i2c_dev, &reg, 1, data, 3, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read 24-bit register 0x%02X: %d", reg, ret);
        return 0;
    }
    
    int32_t value = ((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2];
    
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }
    
    return value;
}

esp_err_t nau7802_set_24bit_register(nau7802_t *dev, uint8_t reg, int32_t value)
{
    uint8_t data[4];
    
    data[0] = reg;
    data[1] = (uint8_t)((value >> 16) & 0xFF);
    data[2] = (uint8_t)((value >> 8) & 0xFF);
    data[3] = (uint8_t)(value & 0xFF);
    
    esp_err_t ret = i2c_master_transmit(dev->i2c_dev, data, 4, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write 24-bit register 0x%02X: %d", reg, ret);
        return ret;
    }
    
    return ESP_OK;
}

uint32_t nau7802_get_32bit_register(nau7802_t *dev, uint8_t reg)
{
    uint8_t data[4];
    esp_err_t ret = i2c_master_transmit_receive(dev->i2c_dev, &reg, 1, data, 4, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read 32-bit register 0x%02X: %d", reg, ret);
        return 0;
    }
    
    uint32_t value = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | 
                     ((uint32_t)data[2] << 8) | data[3];
    
    return value;
}

esp_err_t nau7802_set_32bit_register(nau7802_t *dev, uint8_t reg, uint32_t value)
{
    uint8_t data[5];
    
    data[0] = reg;
    data[1] = (uint8_t)((value >> 24) & 0xFF);
    data[2] = (uint8_t)((value >> 16) & 0xFF);
    data[3] = (uint8_t)((value >> 8) & 0xFF);
    data[4] = (uint8_t)(value & 0xFF);
    
    esp_err_t ret = i2c_master_transmit(dev->i2c_dev, data, 5, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write 32-bit register 0x%02X: %d", reg, ret);
        return ret;
    }
    
    return ESP_OK;
}

