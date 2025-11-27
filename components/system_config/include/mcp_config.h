/**
 * @file mcp_config.h
 * @brief MCP23008/MCP23017 I/O expander configuration management
 * 
 * Provides configuration management for MCP23008 and MCP23017 I2C I/O expander
 * devices, including EtherNet/IP assembly mapping and NVS storage.
 */

#ifndef MCP_CONFIG_H
#define MCP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of MCP devices supported */
#define MCP_MAX_DEVICES 8
/** @brief Maximum pins for MCP23008 */
#define MCP_MAX_PINS_MCP23008 8
/** @brief Maximum pins for MCP23017 */
#define MCP_MAX_PINS_MCP23017 16

/**
 * @brief EtherNet/IP pin mapping structure
 */
typedef struct {
    uint8_t assembly_type;  /**< 0 = Input Assembly 100, 1 = Output Assembly 150 */
    uint8_t byte_offset;     /**< Byte offset in assembly (0-31) */
    uint8_t bit_offset;      /**< Bit offset within byte (0-7) */
    bool enabled;            /**< Whether this mapping is active */
} mcp_pin_mapping_t;

/**
 * @brief MCP device configuration structure
 */
typedef struct {
    uint8_t i2c_address;           /**< I2C address (0x20-0x27) */
    uint8_t device_type;           /**< 0 = MCP23017, 1 = MCP23008 */
    bool enabled;                  /**< Whether device is enabled */
    uint16_t pin_directions;       /**< Bitmask: 0=output, 1=input (16 bits for MCP23017, 8 bits for MCP23008) */
    uint8_t input_byte_start;      /**< Start byte in Input Assembly 100 (0-31, 32 bytes total) */
    uint8_t output_byte_start;     /**< Start byte in Output Assembly 150 (0-31, 32 bytes total) */
    bool output_logic_inverted;    /**< true = inverted logic (active low), false = standard logic (active high) */
    mcp_pin_mapping_t pin_mappings[MCP_MAX_PINS_MCP23017];  /**< Mapping for each pin (legacy, kept for compatibility) */
} mcp_device_config_t;

/**
 * @brief Array of device configurations
 */
typedef struct {
    mcp_device_config_t devices[MCP_MAX_DEVICES];  /**< Array of device configurations */
    uint8_t device_count;                          /**< Number of configured devices */
} mcp_config_t;

/**
 * @brief Function prototypes
 * @{
 */

/**
 * @brief Get default configuration for an MCP device
 * 
 * @param config Pointer to configuration structure to populate
 * @param i2c_address I2C address (0x20-0x27)
 * @param device_type Device type (0 = MCP23017, 1 = MCP23008)
 */
void mcp_config_get_defaults(mcp_device_config_t *config, uint8_t i2c_address, uint8_t device_type);

/**
 * @brief Load all MCP device configurations from NVS
 * 
 * @param config Pointer to configuration structure to populate
 * @return true on success, false on error
 */
bool mcp_config_load_all(mcp_config_t *config);

/**
 * @brief Save all MCP device configurations to NVS
 * 
 * @param config Pointer to configuration structure to save
 * @return true on success, false on error
 */
bool mcp_config_save_all(const mcp_config_t *config);

/**
 * @brief Load configuration for a specific device by I2C address
 * 
 * @param i2c_address I2C address of device (0x20-0x27)
 * @param config Pointer to configuration structure to populate
 * @return true on success, false on error or device not found
 */
bool mcp_config_load_device(uint8_t i2c_address, mcp_device_config_t *config);

/**
 * @brief Save configuration for a specific device
 * 
 * @param config Pointer to device configuration to save
 * @return true on success, false on error
 */
bool mcp_config_save_device(const mcp_device_config_t *config);

/**
 * @brief Delete configuration for a specific device
 * 
 * @param i2c_address I2C address of device to delete
 * @return true on success, false on error
 */
bool mcp_config_delete_device(uint8_t i2c_address);

/**
 * @brief Find device configuration by I2C address
 * 
 * @param config Pointer to configuration structure to search
 * @param i2c_address I2C address to find
 * @return Pointer to device configuration, or NULL if not found
 */
mcp_device_config_t* mcp_config_find_device(mcp_config_t *config, uint8_t i2c_address);

/**
 * @brief Detected device information (from boot-time scan)
 */
typedef struct {
    uint8_t i2c_address;  /**< I2C address */
    uint8_t device_type;  /**< 0 = MCP23017, 1 = MCP23008, 0xFF = Unknown */
    bool detected;        /**< Whether device was detected at boot */
} mcp_detected_device_t;

/**
 * @brief Array of detected devices
 */
typedef struct {
    mcp_detected_device_t devices[MCP_MAX_DEVICES];  /**< Array of detected devices */
    uint8_t device_count;                            /**< Number of detected devices */
} mcp_detected_devices_t;

/**
 * @brief Get detected devices (from boot-time scan)
 * 
 * @return Pointer to detected devices structure, or NULL if not available
 */
mcp_detected_devices_t* mcp_config_get_detected_devices(void);

/**
 * @brief Set detected devices (called at boot)
 * 
 * @param detected Pointer to detected devices structure
 */
void mcp_config_set_detected_devices(const mcp_detected_devices_t *detected);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // MCP_CONFIG_H

