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

#ifndef NAU7802_CALIBRATION_STORAGE_H
#define NAU7802_CALIBRATION_STORAGE_H

#include "nau7802.h"
#include "esp_err.h"

#define NVS_NAMESPACE "nau7802_cal"

typedef struct {
    float calibration_factor;
    float zero_offset;
    int32_t channel1_offset;
    bool is_valid;
} nau7802_calibration_data_t;

esp_err_t nau7802_calibration_load(nau7802_calibration_data_t *cal_data);
esp_err_t nau7802_calibration_save(const nau7802_calibration_data_t *cal_data);
esp_err_t nau7802_calibration_apply(nau7802_t *scale, const nau7802_calibration_data_t *cal_data);
esp_err_t nau7802_calibration_read_from_device(nau7802_t *scale, nau7802_calibration_data_t *cal_data);
esp_err_t nau7802_calibration_erase(void);

#endif

