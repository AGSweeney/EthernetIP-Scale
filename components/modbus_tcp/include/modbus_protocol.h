#ifndef MODBUS_PROTOCOL_H
#define MODBUS_PROTOCOL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle ModbusTCP request from client
 * 
 * @param client_socket Client socket file descriptor
 * @return true if connection should remain open, false to close
 */
bool modbus_tcp_handle_request(int client_socket);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_PROTOCOL_H

