/**
 * @file i2c_config.h
 * @brief I2C configuration management
 * 
 * Provides system-wide I2C configuration settings, including internal pull-up
 * enable/disable functionality.
 */

#ifndef I2C_CONFIG_H
#define I2C_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the system-wide I2C internal pull-up setting
 * 
 * @return true if internal pull-ups should be enabled, false to use external pull-ups
 * 
 * @note This is a system-wide setting that applies to all I2C buses.
 * @note The setting is loaded from NVS at runtime, falling back to the compile-time
 *       CONFIG_OPENER_I2C_INTERNAL_PULLUP value if not set.
 */
bool i2c_config_get_internal_pullup_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_CONFIG_H

