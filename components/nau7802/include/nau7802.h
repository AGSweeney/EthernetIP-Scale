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

/**
 * @file nau7802.h
 * @brief ESP-IDF driver for NAU7802 24-bit wheatstone bridge and load cell amplifier
 * 
 * @author Adam G. Sweeney <agsweeney@gmail.com>
 * @date 2025
 * 
 * This driver provides a complete interface to the NAU7802 ADC for use with load cells
 * and scales. It supports both channels, calibration, interrupts, and all device features.
 * 
 * @note Based on the SparkFun Qwiic Scale NAU7802 Arduino Library by Nathan Seidle
 */

#ifndef NAU7802_H
#define NAU7802_H

#include "driver/i2c_master.h"
#include "esp_err.h"

/** @defgroup NAU7802_Constants Constants
 *  @{
 */

/** Default I2C address of the NAU7802 */
#define NAU7802_I2C_ADDRESS 0x2A

/** @} */

/** @defgroup NAU7802_Registers Register Definitions
 *  @{
 */

#define NAU7802_REGISTER_PU_CTRL 0x00
#define NAU7802_REGISTER_CTRL1 0x01
#define NAU7802_REGISTER_CTRL2 0x02
#define NAU7802_REGISTER_OCAL1_BP2 0x03
#define NAU7802_REGISTER_OCAL1_BP1 0x04
#define NAU7802_REGISTER_OCAL1_BP0 0x05
#define NAU7802_REGISTER_GCAL1_BP3 0x06
#define NAU7802_REGISTER_GCAL1_BP2 0x07
#define NAU7802_REGISTER_GCAL1_BP1 0x08
#define NAU7802_REGISTER_GCAL1_BP0 0x09
#define NAU7802_REGISTER_OCAL2_BP2 0x0A
#define NAU7802_REGISTER_OCAL2_BP1 0x0B
#define NAU7802_REGISTER_OCAL2_BP0 0x0C
#define NAU7802_REGISTER_GCAL2_BP3 0x0D
#define NAU7802_REGISTER_GCAL2_BP2 0x0E
#define NAU7802_REGISTER_GCAL2_BP1 0x0F
#define NAU7802_REGISTER_GCAL2_BP0 0x10
#define NAU7802_REGISTER_I2C_CONTROL 0x11
#define NAU7802_REGISTER_PGA 0x1B
#define NAU7802_REGISTER_ADC 0x15
#define NAU7802_REGISTER_ADC_DATA 0x12
#define NAU7802_REGISTER_POWER 0x1C
#define NAU7802_REGISTER_REVISION_ID 0x1F

#define NAU7802_PU_CTRL_RR 0
#define NAU7802_PU_CTRL_PUD 1
#define NAU7802_PU_CTRL_PUA 2
#define NAU7802_PU_CTRL_PUR 3
#define NAU7802_PU_CTRL_CS 4
#define NAU7802_PU_CTRL_CR 5
#define NAU7802_PU_CTRL_OSCS 6
#define NAU7802_PU_CTRL_AVDDS 7

#define NAU7802_CTRL1_GAIN_MASK 0x07
#define NAU7802_CTRL1_VLDO_MASK 0x38
#define NAU7802_CTRL1_DRDY_SRC 0x40
#define NAU7802_CTRL1_CRP 0x80

#define NAU7802_CTRL2_CALMOD_MASK 0x03
#define NAU7802_CTRL2_CALS 0x04
#define NAU7802_CTRL2_CAL_ERROR 0x08
#define NAU7802_CTRL2_CRS_MASK 0x70

#define NAU7802_PGA_CHP_DIS 0x00
#define NAU7802_PGA_CHP_EN 0x01
#define NAU7802_PGA_INV 0x02
#define NAU7802_PGA_BYPASS_EN 0x04
#define NAU7802_PGA_OUT_EN 0x08
#define NAU7802_PGA_LDOMODE 0x10
#define NAU7802_PGA_GAIN_MASK 0xE0

#define NAU7802_POWER_PGA_CURR_MASK 0x07
#define NAU7802_POWER_ADC_CURR_MASK 0x70
#define NAU7802_POWER_INT_CURR_MASK 0x80

#define NAU7802_ADC_BITS_MASK 0x07
#define NAU7802_ADC_SAMP_MASK 0x70
#define NAU7802_ADC_CHANNEL_MASK 0x80

/** @} */

/** @defgroup NAU7802_Types Data Types
 *  @{
 */

/**
 * @brief Calibration status enumeration
 */
typedef enum {
    NAU7802_CAL_SUCCESS = 0,      /**< Calibration completed successfully */
    NAU7802_CAL_IN_PROGRESS = 1,  /**< Calibration is currently in progress */
    NAU7802_CAL_FAILURE = 2       /**< Calibration failed */
} nau7802_cal_status_t;

/**
 * @brief NAU7802 device structure
 * 
 * This structure holds the device state and configuration.
 * Initialize with nau7802_init() before use.
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;  /**< I2C master bus handle */
    i2c_master_dev_handle_t i2c_dev;  /**< I2C device handle */
    uint8_t address;                   /**< I2C device address */
    float calibration_factor;          /**< Calibration factor for weight calculation */
    float zero_offset;                 /**< Zero offset (tare value) */
    uint32_t ldo_ramp_delay;           /**< LDO ramp delay in milliseconds (default: 250) */
} nau7802_t;

/**
 * @brief Gain settings enumeration
 * 
 * Available gain values: x1, x2, x4, x8, x16, x32, x64, x128
 */
typedef enum {
    NAU7802_GAIN_1 = 0,    /**< Gain x1 */
    NAU7802_GAIN_2 = 1,    /**< Gain x2 */
    NAU7802_GAIN_4 = 2,    /**< Gain x4 */
    NAU7802_GAIN_8 = 3,    /**< Gain x8 */
    NAU7802_GAIN_16 = 4,   /**< Gain x16 */
    NAU7802_GAIN_32 = 5,   /**< Gain x32 */
    NAU7802_GAIN_64 = 6,   /**< Gain x64 */
    NAU7802_GAIN_128 = 7   /**< Gain x128 */
} nau7802_gain_t;

/**
 * @brief Sample rate settings enumeration
 * 
 * Available sample rates: 10, 20, 40, 80, 320 samples per second
 */
typedef enum {
    NAU7802_SPS_10 = 0,    /**< 10 samples per second */
    NAU7802_SPS_20 = 1,    /**< 20 samples per second */
    NAU7802_SPS_40 = 2,    /**< 40 samples per second */
    NAU7802_SPS_80 = 3,    /**< 80 samples per second */
    NAU7802_SPS_320 = 7    /**< 320 samples per second */
} nau7802_sps_t;

/**
 * @brief Channel selection enumeration
 */
typedef enum {
    NAU7802_CHANNEL_1 = 0, /**< Channel 1 */
    NAU7802_CHANNEL_2 = 1  /**< Channel 2 */
} nau7802_channel_t;

/**
 * @brief Calibration mode enumeration
 */
typedef enum {
    NAU7802_CALMOD_INTERNAL = 0, /**< Internal calibration */
    NAU7802_CALMOD_OFFSET = 2,   /**< External offset calibration */
    NAU7802_CALMOD_GAIN = 3      /**< External gain calibration */
} nau7802_cal_mode_t;

/** @} */

/** @defgroup NAU7802_Initialization Initialization Functions
 *  @{
 */

/**
 * @brief Initialize the NAU7802 device structure
 * 
 * This function initializes the device structure and adds it to the I2C bus.
 * After calling this, call nau7802_begin() to complete initialization.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param i2c_bus I2C master bus handle (must be initialized separately)
 * @param address I2C device address (typically NAU7802_I2C_ADDRESS)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if dev or i2c_bus is NULL
 * @return ESP_ERR_INVALID_RESPONSE if device doesn't respond
 */
esp_err_t nau7802_init(nau7802_t *dev, i2c_master_bus_handle_t i2c_bus, uint8_t address);

/**
 * @brief Check if the NAU7802 device is connected and responding
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return true if device responds, false otherwise
 */
bool nau7802_is_connected(nau7802_t *dev);

/**
 * @brief Complete initialization and configure the NAU7802 device
 * 
 * This function performs the full initialization sequence:
 * - Resets the device
 * - Powers up analog and digital sections
 * - Sets LDO to 3.3V
 * - Sets gain to 128x
 * - Sets sample rate to 80 SPS
 * - Configures ADC register
 * - Enables PGA cap
 * - Clears LDOMODE bit
 * - Waits for LDO stabilization
 * - Flushes initial readings
 * - Performs AFE calibration
 * 
 * @param dev Pointer to NAU7802 device structure (must be initialized with nau7802_init())
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_RESPONSE on calibration failure
 */
esp_err_t nau7802_begin(nau7802_t *dev);

/** @} */

/** @defgroup NAU7802_Configuration Configuration Functions
 *  @{
 */
/**
 * @brief Set the PGA gain
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param gain Gain value (NAU7802_GAIN_1 through NAU7802_GAIN_128)
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_gain(nau7802_t *dev, nau7802_gain_t gain);

/**
 * @brief Set the sample rate
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param rate Sample rate (NAU7802_SPS_10 through NAU7802_SPS_320)
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_sample_rate(nau7802_t *dev, nau7802_sps_t rate);

/**
 * @brief Select the active channel
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param channel Channel to select (NAU7802_CHANNEL_1 or NAU7802_CHANNEL_2)
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_channel(nau7802_t *dev, nau7802_channel_t channel);

/**
 * @brief Set the Low-Dropout Regulator (LDO) voltage
 * 
 * Available voltages: 2.4V, 2.7V, 3.0V, 3.3V, 3.6V, 3.9V, 4.2V, 4.5V
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param ldo_value LDO value (0b000=4.5V, 0b001=4.2V, ..., 0b111=2.4V)
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_ldo(nau7802_t *dev, uint8_t ldo_value);

/**
 * @brief Set the LDO ramp delay
 * 
 * The LDO takes approximately 200ms to ramp up. This delay is used during
 * initialization before performing AFE calibration.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param delay_ms Delay in milliseconds (default: 250)
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_ldo_ramp_delay(nau7802_t *dev, uint32_t delay_ms);

/**
 * @brief Get the current LDO ramp delay
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return LDO ramp delay in milliseconds
 */
uint32_t nau7802_get_ldo_ramp_delay(nau7802_t *dev);

/** @} */

/** @defgroup NAU7802_Calibration Calibration Functions
 *  @{
 */
/**
 * @brief Perform synchronous AFE (Analog Front End) calibration
 * 
 * This function performs internal calibration and waits for completion.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if calibration times out
 * @return ESP_ERR_INVALID_RESPONSE if calibration fails
 */
esp_err_t nau7802_calibrate_af(nau7802_t *dev);

/**
 * @brief Perform synchronous AFE calibration with specified mode
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param mode Calibration mode (NAU7802_CALMOD_INTERNAL, NAU7802_CALMOD_OFFSET, or NAU7802_CALMOD_GAIN)
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if calibration times out
 * @return ESP_ERR_INVALID_RESPONSE if calibration fails
 */
esp_err_t nau7802_calibrate_af_mode(nau7802_t *dev, nau7802_cal_mode_t mode);

/**
 * @brief Begin asynchronous AFE calibration
 * 
 * Starts calibration and returns immediately. Use nau7802_cal_af_status() to check
 * progress or nau7802_wait_for_calibrate_af() to wait for completion.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param mode Calibration mode
 * @return ESP_OK if calibration started successfully
 */
esp_err_t nau7802_begin_calibrate_af(nau7802_t *dev, nau7802_cal_mode_t mode);

/**
 * @brief Wait for asynchronous AFE calibration to complete
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param timeout_ms Timeout in milliseconds (0 = wait indefinitely)
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if timeout occurs
 * @return ESP_ERR_INVALID_RESPONSE if calibration fails
 */
esp_err_t nau7802_wait_for_calibrate_af(nau7802_t *dev, uint32_t timeout_ms);

/**
 * @brief Check the status of asynchronous AFE calibration
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return NAU7802_CAL_SUCCESS if complete
 * @return NAU7802_CAL_IN_PROGRESS if still calibrating
 * @return NAU7802_CAL_FAILURE if calibration failed
 */
nau7802_cal_status_t nau7802_cal_af_status(nau7802_t *dev);

/**
 * @brief Perform system offset calibration for a channel
 * 
 * This performs external offset calibration and stores the result in the
 * device's offset register for the specified channel.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param channel Channel to calibrate (NAU7802_CHANNEL_1 or NAU7802_CHANNEL_2)
 * @return ESP_OK on success
 */
esp_err_t nau7802_calibrate_system_offset(nau7802_t *dev, nau7802_channel_t channel);

/**
 * @brief Perform system gain calibration for a channel
 * 
 * This performs external gain calibration and stores the result in the
 * device's gain register for the specified channel.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param channel Channel to calibrate (NAU7802_CHANNEL_1 or NAU7802_CHANNEL_2)
 * @return ESP_OK on success
 */
esp_err_t nau7802_calibrate_system_gain(nau7802_t *dev, nau7802_channel_t channel);

/** @} */

/** @defgroup NAU7802_Reading Reading Functions
 *  @{
 */
/**
 * @brief Check if a new reading is available
 * 
 * Checks the Cycle Ready (CR) bit to see if a conversion is complete.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return true if data is ready, false otherwise
 */
bool nau7802_available(nau7802_t *dev);

/**
 * @brief Get a single 24-bit ADC reading
 * 
 * Returns the raw ADC value as a signed 24-bit integer.
 * Should only be called when nau7802_available() returns true.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return 24-bit signed ADC reading
 */
int32_t nau7802_get_reading(nau7802_t *dev);

/**
 * @brief Get the average of multiple readings
 * 
 * Collects multiple samples and returns the average. Useful for
 * reducing noise and getting stable readings.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param sample_count Number of samples to average
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Average ADC reading
 */
int32_t nau7802_get_average(nau7802_t *dev, uint8_t sample_count, uint32_t timeout_ms);

/**
 * @brief Get calibrated weight reading
 * 
 * Calculates weight using the formula: weight = (reading - zero_offset) / calibration_factor
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param allow_negative If false, negative weights are clamped to zero
 * @param sample_count Number of samples to average (1 = single reading)
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Calculated weight in the units used during calibration
 */
float nau7802_get_weight(nau7802_t *dev, bool allow_negative, uint8_t sample_count, uint32_t timeout_ms);

/** @} */

/** @defgroup NAU7802_Calibration_Helpers Calibration Helper Functions
 *  @{
 */
/**
 * @brief Calculate and set the zero offset (tare)
 * 
 * Averages multiple readings with nothing on the scale and sets this as
 * the zero offset. This should be called when the scale is empty and stable.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param sample_count Number of samples to average
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t nau7802_calculate_zero_offset(nau7802_t *dev, uint8_t sample_count, uint32_t timeout_ms);

/**
 * @brief Calculate and set the calibration factor
 * 
 * Places a known weight on the scale, averages readings, and calculates
 * the calibration factor. The zero offset must be set first.
 * 
 * Formula: calibration_factor = (reading - zero_offset) / known_weight
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param known_weight Known weight in any units (grams, pounds, etc.)
 * @param sample_count Number of samples to average
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t nau7802_calculate_calibration_factor(nau7802_t *dev, float known_weight, uint8_t sample_count, uint32_t timeout_ms);

/**
 * @brief Set the zero offset manually
 * 
 * Useful when loading calibration data from NVS or other storage.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param zero_offset Zero offset value
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_zero_offset(nau7802_t *dev, float zero_offset);

/**
 * @brief Get the current zero offset
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return Current zero offset value
 */
float nau7802_get_zero_offset(nau7802_t *dev);

/**
 * @brief Set the calibration factor manually
 * 
 * Useful when loading calibration data from NVS or other storage.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param calibration_factor Calibration factor value
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_calibration_factor(nau7802_t *dev, float calibration_factor);

/**
 * @brief Get the current calibration factor
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return Current calibration factor value
 */
float nau7802_get_calibration_factor(nau7802_t *dev);

/** @} */

/** @defgroup NAU7802_Interrupt Interrupt Functions
 *  @{
 */
/**
 * @brief Set interrupt polarity to high (default)
 * 
 * The CRDY pin will be high when data is ready.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_int_polarity_high(nau7802_t *dev);

/**
 * @brief Set interrupt polarity to low
 * 
 * The CRDY pin will be low when data is ready.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_int_polarity_low(nau7802_t *dev);

/** @} */

/** @defgroup NAU7802_Device_Info Device Information Functions
 *  @{
 */

/**
 * @brief Get the device revision code
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return Revision code (typically 0x0F)
 */
uint8_t nau7802_get_revision_code(nau7802_t *dev);

/** @} */

/** @defgroup NAU7802_Channel_Registers Channel Register Functions
 *  @{
 */
/**
 * @brief Get channel 1 offset register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return 24-bit signed offset value
 */
int32_t nau7802_get_channel1_offset(nau7802_t *dev);

/**
 * @brief Set channel 1 offset register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param offset 24-bit signed offset value
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_channel1_offset(nau7802_t *dev, int32_t offset);

/**
 * @brief Get channel 1 gain register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return 32-bit unsigned gain value
 */
uint32_t nau7802_get_channel1_gain(nau7802_t *dev);

/**
 * @brief Set channel 1 gain register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param gain 32-bit unsigned gain value
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_channel1_gain(nau7802_t *dev, uint32_t gain);

/**
 * @brief Get channel 2 offset register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return 24-bit signed offset value
 */
int32_t nau7802_get_channel2_offset(nau7802_t *dev);

/**
 * @brief Set channel 2 offset register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param offset 24-bit signed offset value
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_channel2_offset(nau7802_t *dev, int32_t offset);

/**
 * @brief Get channel 2 gain register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return 32-bit unsigned gain value
 */
uint32_t nau7802_get_channel2_gain(nau7802_t *dev);

/**
 * @brief Set channel 2 gain register value
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param gain 32-bit unsigned gain value
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_channel2_gain(nau7802_t *dev, uint32_t gain);

/** @} */

/** @defgroup NAU7802_Low_Level Low-Level Register Access Functions
 *  @{
 */
/**
 * @brief Read an 8-bit register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address
 * @return Register value (0xFF on error)
 */
uint8_t nau7802_get_register(nau7802_t *dev, uint8_t reg);

/**
 * @brief Write an 8-bit register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address
 * @param value Value to write
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_register(nau7802_t *dev, uint8_t reg, uint8_t value);

/**
 * @brief Read a specific bit from a register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address
 * @param bit Bit number (0-7)
 * @return true if bit is set, false otherwise
 */
bool nau7802_get_bit(nau7802_t *dev, uint8_t reg, uint8_t bit);

/**
 * @brief Set a specific bit in a register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address
 * @param bit Bit number (0-7)
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_bit(nau7802_t *dev, uint8_t reg, uint8_t bit);

/**
 * @brief Clear a specific bit in a register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address
 * @param bit Bit number (0-7)
 * @return ESP_OK on success
 */
esp_err_t nau7802_clear_bit(nau7802_t *dev, uint8_t reg, uint8_t bit);

/**
 * @brief Read a 24-bit signed register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address (base address, reads 3 bytes)
 * @return 24-bit signed value
 */
int32_t nau7802_get_24bit_register(nau7802_t *dev, uint8_t reg);

/**
 * @brief Write a 24-bit signed register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address (base address, writes 3 bytes)
 * @param value 24-bit signed value to write
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_24bit_register(nau7802_t *dev, uint8_t reg, int32_t value);

/**
 * @brief Read a 32-bit unsigned register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address (base address, reads 4 bytes)
 * @return 32-bit unsigned value
 */
uint32_t nau7802_get_32bit_register(nau7802_t *dev, uint8_t reg);

/**
 * @brief Write a 32-bit unsigned register
 * 
 * @param dev Pointer to NAU7802 device structure
 * @param reg Register address (base address, writes 4 bytes)
 * @param value 32-bit unsigned value to write
 * @return ESP_OK on success
 */
esp_err_t nau7802_set_32bit_register(nau7802_t *dev, uint8_t reg, uint32_t value);

/** @} */

/** @defgroup NAU7802_Power Power Management Functions
 *  @{
 */

/**
 * @brief Power up the device
 * 
 * Powers up digital and analog sections. Waits for power-up ready bit.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if power-up times out
 */
esp_err_t nau7802_power_up(nau7802_t *dev);

/**
 * @brief Power down the device
 * 
 * Puts the device into low-power mode (~200nA).
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 */
esp_err_t nau7802_power_down(nau7802_t *dev);

/**
 * @brief Reset the device
 * 
 * Resets all registers to power-on defaults.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 */
esp_err_t nau7802_reset(nau7802_t *dev);

/**
 * @brief Start a conversion cycle
 * 
 * Toggles the Cycle Start (CS) bit to trigger a new conversion.
 * 
 * @param dev Pointer to NAU7802 device structure
 * @return ESP_OK on success
 */
esp_err_t nau7802_start_conversion(nau7802_t *dev);

/** @} */

#endif

