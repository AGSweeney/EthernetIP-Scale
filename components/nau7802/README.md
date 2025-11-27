# NAU7802 ESP-IDF Driver

ESP-IDF driver for the NAU7802 24-bit wheatstone bridge and load cell amplifier.

## Overview

The NAU7802 is a high-precision 24-bit ADC designed for load cell and scale applications. This driver provides a complete interface for ESP-IDF projects, supporting all device features including dual-channel operation, calibration, interrupts, and power management.

**Key Features:**
- 24-bit resolution ADC
- Dual-channel support (Channel 1 and Channel 2)
- Programmable gain: x1, x2, x4, x8, x16, x32, x64, x128
- Sample rates: 10, 20, 40, 80, 320 SPS
- Internal and external calibration modes
- Interrupt support via CRDY pin
- Low-power mode (~200nA)
- I2C interface (400kHz capable)

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Calibration](#calibration)
- [Interrupts](#interrupts)
- [Hardware Connections](#hardware-connections)
- [License](#license)

## Installation

### Using ESP-IDF Component Manager

Add this component to your project's `idf_component.yml`:

```yaml
dependencies:
  nau7802:
    path: components/nau7802
```

### Manual Installation

1. Copy the `nau7802` component directory to your project's `components` folder
2. Add `nau7802` to your component's `REQUIRES` in `CMakeLists.txt`:

```cmake
idf_component_register(
    ...
    REQUIRES nau7802
)
```

## Quick Start

### Basic Usage

```c
#include "nau7802.h"
#include "driver/i2c_master.h"

// Initialize I2C bus
i2c_master_bus_config_t i2c_bus_config = {0};
i2c_bus_config.i2c_port = 0;
i2c_bus_config.sda_io_num = 5;
i2c_bus_config.scl_io_num = 6;
i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
i2c_bus_config.flags.enable_internal_pullup = true;

i2c_master_bus_handle_t i2c_bus_handle;
i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);

// Initialize NAU7802
nau7802_t scale;
nau7802_init(&scale, i2c_bus_handle, NAU7802_I2C_ADDRESS);
nau7802_begin(&scale);

// Read weight
while (1) {
    if (nau7802_available(&scale)) {
        float weight = nau7802_get_weight(&scale, false, 1, 0);
        ESP_LOGI("MAIN", "Weight: %.2f g", weight);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

## API Reference

### Initialization Functions

#### `nau7802_init()`
Initialize the device structure and add to I2C bus.

```c
esp_err_t nau7802_init(nau7802_t *dev, i2c_master_bus_handle_t i2c_bus, uint8_t address);
```

**Parameters:**
- `dev`: Pointer to NAU7802 device structure
- `i2c_bus`: I2C master bus handle (must be initialized separately)
- `address`: I2C device address (typically `NAU7802_I2C_ADDRESS` = 0x2A)

**Returns:** `ESP_OK` on success

#### `nau7802_begin()`
Complete initialization and configure the device. This performs:
- Device reset
- Power up sequence
- LDO configuration (3.3V)
- Gain setting (128x)
- Sample rate (80 SPS)
- AFE calibration

```c
esp_err_t nau7802_begin(nau7802_t *dev);
```

**Returns:** `ESP_OK` on success

#### `nau7802_is_connected()`
Check if device responds on I2C bus.

```c
bool nau7802_is_connected(nau7802_t *dev);
```

**Returns:** `true` if device responds, `false` otherwise

### Configuration Functions

#### `nau7802_set_gain()`
Set the programmable gain amplifier (PGA) gain.

```c
esp_err_t nau7802_set_gain(nau7802_t *dev, nau7802_gain_t gain);
```

**Gain Options:**
- `NAU7802_GAIN_1` through `NAU7802_GAIN_128`

**Note:** After changing gain, recalibrate the AFE.

#### `nau7802_set_sample_rate()`
Set the sample rate (samples per second).

```c
esp_err_t nau7802_set_sample_rate(nau7802_t *dev, nau7802_sps_t rate);
```

**Sample Rate Options:**
- `NAU7802_SPS_10` - 10 samples/second
- `NAU7802_SPS_20` - 20 samples/second
- `NAU7802_SPS_40` - 40 samples/second
- `NAU7802_SPS_80` - 80 samples/second
- `NAU7802_SPS_320` - 320 samples/second

**Note:** After changing sample rate, recalibrate the AFE.

#### `nau7802_set_channel()`
Select active channel (1 or 2).

```c
esp_err_t nau7802_set_channel(nau7802_t *dev, nau7802_channel_t channel);
```

**Channel Options:**
- `NAU7802_CHANNEL_1`
- `NAU7802_CHANNEL_2`

#### `nau7802_set_ldo()`
Set the Low-Dropout Regulator voltage.

```c
esp_err_t nau7802_set_ldo(nau7802_t *dev, uint8_t ldo_value);
```

**LDO Values:**
- `0b000` = 4.5V
- `0b001` = 4.2V
- `0b010` = 3.9V
- `0b011` = 3.6V
- `0b100` = 3.3V (recommended for Qwiic)
- `0b101` = 3.0V
- `0b110` = 2.7V
- `0b111` = 2.4V

### Reading Functions

#### `nau7802_available()`
Check if a new reading is available.

```c
bool nau7802_available(nau7802_t *dev);
```

**Returns:** `true` if conversion complete (CR bit set), `false` otherwise

#### `nau7802_get_reading()`
Get a single 24-bit ADC reading.

```c
int32_t nau7802_get_reading(nau7802_t *dev);
```

**Returns:** 24-bit signed ADC value

**Note:** Only call when `nau7802_available()` returns `true`.

#### `nau7802_get_average()`
Get average of multiple readings.

```c
int32_t nau7802_get_average(nau7802_t *dev, uint8_t sample_count, uint32_t timeout_ms);
```

**Parameters:**
- `sample_count`: Number of samples to average
- `timeout_ms`: Timeout in milliseconds (0 = no timeout)

**Returns:** Average ADC reading

#### `nau7802_get_weight()`
Get calibrated weight reading.

```c
float nau7802_get_weight(nau7802_t *dev, bool allow_negative, uint8_t sample_count, uint32_t timeout_ms);
```

**Parameters:**
- `allow_negative`: If `false`, negative weights are clamped to zero
- `sample_count`: Number of samples to average (1 = single reading)
- `timeout_ms`: Timeout in milliseconds (0 = no timeout)

**Returns:** Calculated weight in units used during calibration

**Formula:** `weight = (reading - zero_offset) / calibration_factor`

### Calibration Functions

#### `nau7802_calculate_zero_offset()`
Calculate and set zero offset (tare).

```c
esp_err_t nau7802_calculate_zero_offset(nau7802_t *dev, uint8_t sample_count, uint32_t timeout_ms);
```

**Usage:** Call when scale is empty and stable.

#### `nau7802_calculate_calibration_factor()`
Calculate calibration factor using known weight.

```c
esp_err_t nau7802_calculate_calibration_factor(nau7802_t *dev, float known_weight, uint8_t sample_count, uint32_t timeout_ms);
```

**Usage:** 
1. Set zero offset first
2. Place known weight on scale
3. Call this function with the known weight value

#### `nau7802_calibrate_af()`
Perform Analog Front End (AFE) calibration.

```c
esp_err_t nau7802_calibrate_af(nau7802_t *dev);
```

**When to use:** After changing gain, sample rate, or channel.

#### `nau7802_calibrate_af_mode()`
Perform AFE calibration with specified mode.

```c
esp_err_t nau7802_calibrate_af_mode(nau7802_t *dev, nau7802_cal_mode_t mode);
```

**Calibration Modes:**
- `NAU7802_CALMOD_INTERNAL` - Internal calibration
- `NAU7802_CALMOD_OFFSET` - External offset calibration
- `NAU7802_CALMOD_GAIN` - External gain calibration

### Interrupt Functions

#### `nau7802_set_int_polarity_high()`
Set interrupt polarity to high (default).

```c
esp_err_t nau7802_set_int_polarity_high(nau7802_t *dev);
```

CRDY pin will be HIGH when data is ready.

#### `nau7802_set_int_polarity_low()`
Set interrupt polarity to low.

```c
esp_err_t nau7802_set_int_polarity_low(nau7802_t *dev);
```

CRDY pin will be LOW when data is ready.

### Power Management Functions

#### `nau7802_power_up()`
Power up digital and analog sections.

```c
esp_err_t nau7802_power_up(nau7802_t *dev);
```

#### `nau7802_power_down()`
Put device into low-power mode (~200nA).

```c
esp_err_t nau7802_power_down(nau7802_t *dev);
```

#### `nau7802_reset()`
Reset all registers to power-on defaults.

```c
esp_err_t nau7802_reset(nau7802_t *dev);
```

### Low-Level Register Access

The driver also provides low-level register access functions:

- `nau7802_get_register()` / `nau7802_set_register()` - 8-bit register access
- `nau7802_get_bit()` / `nau7802_set_bit()` / `nau7802_clear_bit()` - Bit manipulation
- `nau7802_get_24bit_register()` / `nau7802_set_24bit_register()` - 24-bit register access
- `nau7802_get_32bit_register()` / `nau7802_set_32bit_register()` - 32-bit register access

See the header file (`nau7802.h`) for complete function documentation.

## Examples

### Example 1: Basic Reading

See `examples/example_basic.c` for a simple example that reads weight values continuously.

### Example 2: Calibration

See `examples/example_calibration.c` for an example showing:
- Zero offset calculation (tare)
- Calibration factor calculation
- Saving/loading calibration from NVS

### Example 3: Interrupts

See `examples/example_interrupt.c` for an example using GPIO interrupts to detect when data is ready.

## Calibration

### Two-Point Calibration

The NAU7802 uses a linear calibration model: `weight = (reading - zero_offset) / calibration_factor`

**Step 1: Zero Offset (Tare)**
```c
// Remove all weight from scale, wait for stability
nau7802_calculate_zero_offset(&scale, 64, 2000);
```

**Step 2: Calibration Factor**
```c
// Place known weight (e.g., 100g) on scale
nau7802_calculate_calibration_factor(&scale, 100.0f, 64, 2000);
```

### External Calibration Mode

For more precise calibration, use external calibration mode which stores values in the device's internal registers:

```c
// Zero calibration
nau7802_calibrate_af_mode(&scale, NAU7802_CALMOD_OFFSET);
int32_t offset = nau7802_get_channel1_offset(&scale);

// Gain calibration (with known weight)
nau7802_calibrate_af_mode(&scale, NAU7802_CALMOD_GAIN);
uint32_t gain = nau7802_get_channel1_gain(&scale);
```

### Storing Calibration in NVS

Use the calibration storage helper functions (see `nau7802_calibration_storage.h`):

```c
// Save calibration
nau7802_calibration_data_t cal_data;
nau7802_calibration_read_from_device(&scale, &cal_data);
nau7802_calibration_save(&cal_data);

// Load calibration
nau7802_calibration_load(&cal_data);
if (cal_data.is_valid) {
    nau7802_calibration_apply(&scale, &cal_data);
}
```

## Interrupts

The NAU7802 provides a CRDY (Cycle Ready) pin that indicates when a conversion is complete.

### Configuration

```c
// Set interrupt polarity (high = default)
nau7802_set_int_polarity_high(&scale);

// Or set to low
nau7802_set_int_polarity_low(&scale);
```

### GPIO Interrupt Setup

```c
// Configure GPIO
gpio_config_t io_conf = {0};
io_conf.intr_type = GPIO_INTR_POSEDGE;  // or GPIO_INTR_NEGEDGE
io_conf.mode = GPIO_MODE_INPUT;
io_conf.pin_bit_mask = (1ULL << CRDY_GPIO);
io_conf.pull_up_en = 1;
gpio_config(&io_conf);

// Install ISR service
gpio_install_isr_service(0);
gpio_isr_handler_add(CRDY_GPIO, gpio_isr_handler, NULL);

// In ISR handler, check available and read
if (nau7802_available(&scale)) {
    float weight = nau7802_get_weight(&scale, false, 1, 0);
}
```

## Hardware Connections

### I2C Connections

| NAU7802 Pin | ESP32 Pin | Notes |
|-------------|-----------|-------|
| SDA         | GPIO 5    | I2C Data (with pullup) |
| SCL         | GPIO 6    | I2C Clock (with pullup) |
| VCC         | 3.3V      | Power supply |
| GND         | GND       | Ground |
| CRDY        | GPIO 7    | Optional: Cycle Ready interrupt |

### Load Cell Connections

Connect your load cell to the NAU7802:
- E+ / EX+ → Load cell excitation positive
- E- / EX- → Load cell excitation negative  
- A+ / IN+ → Load cell signal positive
- A- / IN- → Load cell signal negative

**Note:** The NAU7802 provides excitation voltage to the load cell.

## Troubleshooting

### Device Not Detected

- Check I2C connections (SDA, SCL)
- Verify I2C address (default 0x2A)
- Ensure pullup resistors are present (or enable internal pullups)
- Check power supply (3.3V)

### Readings Are Zero or Invalid

- Ensure device is calibrated (`nau7802_calculate_zero_offset()` and `nau7802_calculate_calibration_factor()`)
- Check load cell connections
- Verify gain setting is appropriate for your load cell
- Ensure AFE is calibrated (`nau7802_calibrate_af()`)

### Unstable Readings

- Increase averaging (`nau7802_get_average()` or use `sample_count` > 1 in `nau7802_get_weight()`)
- Lower sample rate for more stable readings
- Ensure load cell is properly mounted and stable
- Check for electrical noise/interference

### Calibration Fails

- Ensure scale is stable before calibrating
- Wait longer between steps (increase delays)
- Use more samples for averaging
- Check that known weight is accurate

## License

MIT License

Copyright (c) 2025 Adam G. Sweeney <agsweeney@gmail.com>  
Copyright (c) 2019 SparkFun Electronics

See LICENSE file for full license text.

## Credits

This driver is based on the SparkFun Qwiic Scale NAU7802 Arduino Library by Nathan Seidle @ SparkFun Electronics.

Original Arduino Library: https://github.com/sparkfun/SparkFun_Qwiic_Scale_NAU7802_Arduino_Library

## References

- [NAU7802 Datasheet](https://www.nuvoton.com/resource-files/NAU7802%20Data%20Sheet%20V1.2.pdf)
- [SparkFun Qwiic Scale Hookup Guide](https://learn.sparkfun.com/tutorials/qwiic-scale-hookup-guide)

