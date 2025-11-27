#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "lwip/ip4_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IP configuration structure
 */
typedef struct {
    bool use_dhcp;              // true for DHCP, false for static
    uint32_t ip_address;        // IP address in network byte order
    uint32_t netmask;           // Network mask in network byte order
    uint32_t gateway;           // Gateway in network byte order
    uint32_t dns1;              // Primary DNS in network byte order
    uint32_t dns2;              // Secondary DNS in network byte order
} system_ip_config_t;

/**
 * @brief Get default IP configuration (DHCP enabled)
 */
void system_ip_config_get_defaults(system_ip_config_t *config);

/**
 * @brief Load IP configuration from NVS
 * @param config Pointer to config structure to fill
 * @return true if loaded successfully, false if using defaults
 */
bool system_ip_config_load(system_ip_config_t *config);

/**
 * @brief Save IP configuration to NVS
 * @param config Pointer to config structure to save
 * @return true on success, false on error
 */
bool system_ip_config_save(const system_ip_config_t *config);

/**
 * @brief Load Modbus enabled state from NVS
 * @return true if Modbus is enabled, false if disabled or not set
 */
bool system_modbus_enabled_load(void);

/**
 * @brief Save Modbus enabled state to NVS
 * @param enabled true to enable Modbus, false to disable
 * @return true on success, false on error
 */
bool system_modbus_enabled_save(bool enabled);

/**
 * @brief Load VL53L1x sensor enabled state from NVS
 * @return true if sensor is enabled, false if disabled or not set
 */
bool system_sensor_enabled_load(void);

/**
 * @brief Save VL53L1x sensor enabled state to NVS
 * @param enabled true to enable sensor, false to disable
 * @return true on success, false on error
 */
bool system_sensor_enabled_save(bool enabled);

/**
 * @brief Load VL53L1x sensor data start byte offset from NVS
 * @return Start byte offset (0, 9, or 18). Defaults to 0 if not set or invalid.
 */
uint8_t system_sensor_byte_offset_load(void);

/**
 * @brief Save VL53L1x sensor data start byte offset to NVS
 * @param start_byte Start byte offset (must be 0, 9, or 18)
 * @return true on success, false on error or invalid value
 */
bool system_sensor_byte_offset_save(uint8_t start_byte);

/**
 * @brief Load MCP enabled state from NVS
 * @return true if MCP is enabled, false if disabled or not set
 */
bool system_mcp_enabled_load(void);

/**
 * @brief Save MCP enabled state to NVS
 * @param enabled true to enable MCP, false to disable
 * @return true on success, false on error
 */
bool system_mcp_enabled_save(bool enabled);

/**
 * @brief Load MCP device type preference from NVS
 * @return 0 for MCP23017, 1 for MCP23008. Defaults to 1 (MCP23008) if not set or invalid.
 */
uint8_t system_mcp_device_type_load(void);

/**
 * @brief Save MCP device type preference to NVS
 * @param device_type 0 for MCP23017, 1 for MCP23008
 * @return true on success, false on error or invalid value
 */
bool system_mcp_device_type_save(uint8_t device_type);

/**
 * @brief Load MCP I/O task update rate from NVS
 * @return Update rate in milliseconds. Defaults to 20ms (50 Hz) if not set or invalid.
 */
uint16_t system_mcp_update_rate_ms_load(void);

/**
 * @brief Save MCP I/O task update rate to NVS
 * @param update_rate_ms Update rate in milliseconds (10-1000ms)
 * @return true on success, false on error or invalid value
 */
bool system_mcp_update_rate_ms_save(uint16_t update_rate_ms);

/**
 * @brief Load I2C internal pull-up setting from NVS
 * @return true if internal pull-ups are enabled, false if disabled. Falls back to CONFIG_OPENER_I2C_INTERNAL_PULLUP if not set.
 */
bool system_i2c_internal_pullup_load(void);

/**
 * @brief Save I2C internal pull-up setting to NVS
 * @param enabled true to enable internal pull-ups, false to disable (use external)
 * @return true on success, false on error
 * @note Changes take effect on next boot (I2C buses are initialized at boot time)
 */
bool system_i2c_internal_pullup_save(bool enabled);

/**
 * @brief Load NAU7802 enabled state from NVS
 * @return true if NAU7802 is enabled, false if disabled or not set
 */
bool system_nau7802_enabled_load(void);

/**
 * @brief Save NAU7802 enabled state to NVS
 * @param enabled true to enable NAU7802, false to disable
 * @return true on success, false on error
 */
bool system_nau7802_enabled_save(bool enabled);

/**
 * @brief Load NAU7802 data start byte offset from NVS
 * @return Start byte offset (0-31). Defaults to 0 if not set or invalid.
 */
uint8_t system_nau7802_byte_offset_load(void);

/**
 * @brief Save NAU7802 data start byte offset to NVS
 * @param start_byte Start byte offset (0-31)
 * @return true on success, false on error or invalid value
 */
bool system_nau7802_byte_offset_save(uint8_t start_byte);

/**
 * @brief Load NAU7802 calibration factor from NVS
 * @return Calibration factor (defaults to 0.0 if not set)
 */
float system_nau7802_calibration_factor_load(void);

/**
 * @brief Save NAU7802 calibration factor to NVS
 * @param factor Calibration factor value
 * @return true on success, false on error
 */
bool system_nau7802_calibration_factor_save(float factor);

/**
 * @brief Load NAU7802 zero offset from NVS
 * @return Zero offset (defaults to 0.0 if not set)
 */
float system_nau7802_zero_offset_load(void);

/**
 * @brief Save NAU7802 zero offset to NVS
 * @param offset Zero offset value
 * @return true on success, false on error
 */
bool system_nau7802_zero_offset_save(float offset);

/**
 * @brief Load NAU7802 weight unit from NVS
 * @return Unit: 0=grams, 1=lbs, 2=kg (defaults to 1=lbs if not set)
 */
uint8_t system_nau7802_unit_load(void);

/**
 * @brief Save NAU7802 weight unit to NVS
 * @param unit Unit: 0=grams, 1=lbs, 2=kg
 * @return true on success, false on error
 */
bool system_nau7802_unit_save(uint8_t unit);

/**
 * @brief Load NAU7802 gain setting from NVS
 * @return Gain value (0-7: x1-x128). Defaults to 7 (x128) if not set or invalid.
 */
uint8_t system_nau7802_gain_load(void);

/**
 * @brief Save NAU7802 gain setting to NVS
 * @param gain Gain value (0-7: x1-x128)
 * @return true on success, false on error or invalid value
 */
bool system_nau7802_gain_save(uint8_t gain);

/**
 * @brief Load NAU7802 sample rate from NVS
 * @return Sample rate (0=10, 1=20, 2=40, 3=80, 7=320 SPS). Defaults to 3 (80 SPS) if not set or invalid.
 */
uint8_t system_nau7802_sample_rate_load(void);

/**
 * @brief Save NAU7802 sample rate to NVS
 * @param sample_rate Sample rate (0=10, 1=20, 2=40, 3=80, 7=320 SPS)
 * @return true on success, false on error or invalid value
 */
bool system_nau7802_sample_rate_save(uint8_t sample_rate);

/**
 * @brief Load NAU7802 channel selection from NVS
 * @return Channel (0=Channel 1, 1=Channel 2). Defaults to 0 (Channel 1) if not set or invalid.
 */
uint8_t system_nau7802_channel_load(void);

/**
 * @brief Save NAU7802 channel selection to NVS
 * @param channel Channel (0=Channel 1, 1=Channel 2)
 * @return true on success, false on error or invalid value
 */
bool system_nau7802_channel_save(uint8_t channel);

/**
 * @brief Load NAU7802 LDO voltage setting from NVS
 * @return LDO value (0-7: 2.4V-4.5V). Defaults to 4 (3.3V) if not set or invalid.
 */
uint8_t system_nau7802_ldo_load(void);

/**
 * @brief Save NAU7802 LDO voltage setting to NVS
 * @param ldo LDO value (0-7: 2.4V-4.5V)
 * @return true on success, false on error or invalid value
 */
bool system_nau7802_ldo_save(uint8_t ldo);

/**
 * @brief Load NAU7802 reading average (samples) from NVS
 * @return Number of samples to average for regular readings (1-50). Default: 1 (no averaging)
 */
uint8_t system_nau7802_average_load(void);

/**
 * @brief Save NAU7802 reading average (samples) to NVS
 * @param average Number of samples to average (1-50)
 * @return true on success, false on error or invalid value
 */
bool system_nau7802_average_save(uint8_t average);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_CONFIG_H

