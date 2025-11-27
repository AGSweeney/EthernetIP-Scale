#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ModbusTCP server
 * 
 * @return true on success, false on error
 */
bool modbus_tcp_init(void);

/**
 * @brief Start ModbusTCP server listening on port 502
 * 
 * @return true on success, false on error
 */
bool modbus_tcp_start(void);

/**
 * @brief Stop ModbusTCP server
 */
void modbus_tcp_stop(void);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_TCP_H

