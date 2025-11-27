#ifndef MODBUS_REGISTER_MAP_H
#define MODBUS_REGISTER_MAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read input registers (read-only, maps to Input Assembly 100)
 * 
 * @param start_addr Starting register address (0-15)
 * @param quantity Number of registers to read (1-125)
 * @param data Buffer to store register values (big-endian, 2 bytes per register)
 * @return true on success, false on invalid address/quantity
 */
bool modbus_read_input_registers(uint16_t start_addr, uint16_t quantity, uint8_t *data);

/**
 * @brief Read holding registers (read-write, maps to Output Assembly 150 and Config Assembly 151)
 * 
 * @param start_addr Starting register address
 * @param quantity Number of registers to read (1-125)
 * @param data Buffer to store register values (big-endian, 2 bytes per register)
 * @return true on success, false on invalid address/quantity
 */
bool modbus_read_holding_registers(uint16_t start_addr, uint16_t quantity, uint8_t *data);

/**
 * @brief Write single holding register
 * 
 * @param address Register address
 * @param value Register value (big-endian)
 * @return true on success, false on invalid address
 */
bool modbus_write_holding_register(uint16_t address, uint16_t value);

/**
 * @brief Write multiple holding registers
 * 
 * @param start_addr Starting register address
 * @param quantity Number of registers to write
 * @param data Register values (big-endian, 2 bytes per register)
 * @return true on success, false on invalid address/quantity
 */
bool modbus_write_holding_registers(uint16_t start_addr, uint16_t quantity, const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_REGISTER_MAP_H

