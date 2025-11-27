/**
 * @mainpage NAU7802 ESP-IDF Driver Documentation
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
 * 
 * @section intro_sec Introduction
 * 
 * This driver provides a complete interface to the NAU7802 24-bit wheatstone bridge
 * and load cell amplifier for ESP-IDF projects. The driver supports all device features
 * including dual-channel operation, calibration, interrupts, and power management.
 * 
 * @section features_sec Features
 * 
 * - Complete API matching SparkFun Arduino library functionality
 * - 24-bit ADC resolution
 * - Dual-channel support (Channel 1 and Channel 2)
 * - Programmable gain: x1 through x128
 * - Sample rates: 10, 20, 40, 80, 320 SPS
 * - Internal and external calibration modes
 * - Interrupt support via CRDY pin
 * - Low-power mode support
 * - NVS calibration storage helpers
 * 
 * @section quick_start_sec Quick Start
 * 
 * @code{c}
 * // Initialize I2C bus
 * i2c_master_bus_config_t i2c_bus_config = {0};
 * i2c_bus_config.i2c_port = 0;
 * i2c_bus_config.sda_io_num = 5;
 * i2c_bus_config.scl_io_num = 6;
 * i2c_bus_config.flags.enable_internal_pullup = true;
 * 
 * i2c_master_bus_handle_t i2c_bus_handle;
 * i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
 * 
 * // Initialize NAU7802
 * nau7802_t scale;
 * nau7802_init(&scale, i2c_bus_handle, NAU7802_I2C_ADDRESS);
 * nau7802_begin(&scale);
 * 
 * // Read weight
 * if (nau7802_available(&scale)) {
 *     float weight = nau7802_get_weight(&scale, false, 1, 0);
 * }
 * @endcode
 * 
 * @section examples_sec Examples
 * 
 * - @ref example_basic.c - Basic reading example
 * - @ref example_calibration.c - Calibration example with NVS storage
 * - @ref example_interrupt.c - GPIO interrupt example
 * 
 * @section api_sec API Reference
 * 
 * See the @ref NAU7802_Initialization "Initialization Functions" for setup,
 * @ref NAU7802_Reading "Reading Functions" for data acquisition, and
 * @ref NAU7802_Calibration "Calibration Functions" for calibration.
 * 
 * @section license_sec License
 * 
 * MIT License - See LICENSE file for details.
 * 
 * @section credits_sec Credits
 * 
 * Based on the SparkFun Qwiic Scale NAU7802 Arduino Library by Nathan Seidle.
 * ESP-IDF port and enhancements by Adam G. Sweeney.
 */

