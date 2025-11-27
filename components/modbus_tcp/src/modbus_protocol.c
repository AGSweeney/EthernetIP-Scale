/*
 * Copyright (c) 2025, Adam G. Sweeney <agsweeney@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "modbus_protocol.h"
#include "modbus_register_map.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>
#include <errno.h>

static const char *TAG = "modbus_protocol";

// ModbusTCP ADU structure
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;      // Always 0 for Modbus
    uint16_t length;           // Length of unit identifier + PDU
    uint8_t unit_id;           // Slave address
    uint8_t function_code;
    uint8_t data[256];         // PDU data
} modbus_tcp_adu_t;

// Modbus function codes
#define MODBUS_FC_READ_HOLDING_REGISTERS   0x03
#define MODBUS_FC_READ_INPUT_REGISTERS     0x04
#define MODBUS_FC_WRITE_SINGLE_REGISTER    0x06
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10

// Exception codes
#define MODBUS_EX_ILLEGAL_FUNCTION          0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDRESS      0x02
#define MODBUS_EX_ILLEGAL_DATA_VALUE        0x03
#define MODBUS_EX_SLAVE_DEVICE_FAILURE      0x04


static void send_exception(int client_socket, uint16_t transaction_id, uint8_t unit_id, 
                          uint8_t function_code, uint8_t exception_code)
{
    uint8_t response[9];
    response[0] = (transaction_id >> 8) & 0xFF;
    response[1] = transaction_id & 0xFF;
    response[2] = 0x00; // Protocol ID
    response[3] = 0x00;
    response[4] = 0x00; // Length
    response[5] = 0x03;
    response[6] = unit_id;
    response[7] = function_code | 0x80; // Exception flag
    response[8] = exception_code;
    
    send(client_socket, response, sizeof(response), 0);
}

static bool handle_read_holding_registers(int client_socket, uint16_t transaction_id, 
                                         uint8_t unit_id, const uint8_t *pdu, int pdu_len)
{
    // Read Holding Registers requires: start_addr (2 bytes) + quantity (2 bytes) = 4 bytes
    if (pdu_len < 4) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_HOLDING_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    uint16_t start_addr = (pdu[0] << 8) | pdu[1];
    uint16_t quantity = (pdu[2] << 8) | pdu[3];
    
    // Validate quantity (Modbus spec: 1-125 registers)
    if (quantity == 0 || quantity > 125) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_HOLDING_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    // Check response buffer size
    size_t response_data_size = quantity * 2;
    if (response_data_size > 250) { // Max 125 registers * 2 bytes = 250 bytes
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_HOLDING_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    uint8_t response[256];
    int response_len = 9 + response_data_size;
    response[0] = (transaction_id >> 8) & 0xFF;
    response[1] = transaction_id & 0xFF;
    response[2] = 0x00; // Protocol ID
    response[3] = 0x00;
    response[4] = ((response_len - 6) >> 8) & 0xFF; // Length
    response[5] = (response_len - 6) & 0xFF;
    response[6] = unit_id;
    response[7] = MODBUS_FC_READ_HOLDING_REGISTERS;
    response[8] = response_data_size; // Byte count
    
    // Read registers from map
    if (!modbus_read_holding_registers(start_addr, quantity, &response[9])) {
        ESP_LOGE(TAG, "Failed to read holding registers: start_addr=%d, quantity=%d", 
                 start_addr, quantity);
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_HOLDING_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return true;
    }
    
    int sent = send(client_socket, response, response_len, 0);
    if (sent != response_len) {
        ESP_LOGE(TAG, "Failed to send full response: sent %d of %d bytes", sent, response_len);
    }
    return true;
}

static bool handle_read_input_registers(int client_socket, uint16_t transaction_id, 
                                        uint8_t unit_id, const uint8_t *pdu, int pdu_len)
{
    // Read Input Registers requires: start_addr (2 bytes) + quantity (2 bytes) = 4 bytes
    if (pdu_len < 4) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_INPUT_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    uint16_t start_addr = (pdu[0] << 8) | pdu[1];
    uint16_t quantity = (pdu[2] << 8) | pdu[3];
    
    // Validate quantity (Modbus spec: 1-125 registers)
    if (quantity == 0 || quantity > 125) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_INPUT_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    // Check response buffer size
    size_t response_data_size = quantity * 2;
    if (response_data_size > 250) { // Max 125 registers * 2 bytes = 250 bytes
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_INPUT_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    uint8_t response[256];
    int response_len = 9 + response_data_size;
    response[0] = (transaction_id >> 8) & 0xFF;
    response[1] = transaction_id & 0xFF;
    response[2] = 0x00; // Protocol ID
    response[3] = 0x00;
    response[4] = ((response_len - 6) >> 8) & 0xFF; // Length
    response[5] = (response_len - 6) & 0xFF;
    response[6] = unit_id;
    response[7] = MODBUS_FC_READ_INPUT_REGISTERS;
    response[8] = response_data_size; // Byte count
    
    // Read registers from map
    if (!modbus_read_input_registers(start_addr, quantity, &response[9])) {
        ESP_LOGE(TAG, "Failed to read input registers: start_addr=%d, quantity=%d", 
                 start_addr, quantity);
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_READ_INPUT_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return true;
    }
    
    int sent = send(client_socket, response, response_len, 0);
    if (sent != response_len) {
        ESP_LOGE(TAG, "Failed to send full response: sent %d of %d bytes", sent, response_len);
    }
    return true;
}

static bool handle_write_single_register(int client_socket, uint16_t transaction_id, 
                                         uint8_t unit_id, const uint8_t *pdu, int pdu_len)
{
    // Write Single Register requires: address (2 bytes) + value (2 bytes) = 4 bytes
    if (pdu_len < 4) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_WRITE_SINGLE_REGISTER, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    uint16_t address = (pdu[0] << 8) | pdu[1];
    uint16_t value = (pdu[2] << 8) | pdu[3];
    
    // Write register to map
    if (!modbus_write_holding_register(address, value)) {
        ESP_LOGE(TAG, "Failed to write holding register: address=%d, value=%d", address, value);
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_WRITE_SINGLE_REGISTER, 
                      MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return true;
    }
    
    // Echo back the request
    uint8_t response[12];
    response[0] = (transaction_id >> 8) & 0xFF;
    response[1] = transaction_id & 0xFF;
    response[2] = 0x00; // Protocol ID
    response[3] = 0x00;
    response[4] = 0x00; // Length
    response[5] = 0x06;
    response[6] = unit_id;
    response[7] = MODBUS_FC_WRITE_SINGLE_REGISTER;
    response[8] = (address >> 8) & 0xFF;
    response[9] = address & 0xFF;
    response[10] = (value >> 8) & 0xFF;
    response[11] = value & 0xFF;
    
    int sent = send(client_socket, response, sizeof(response), 0);
    if (sent != sizeof(response)) {
        ESP_LOGE(TAG, "Failed to send write response: sent %d of %d bytes", sent, sizeof(response));
    }
    return true;
}

static bool handle_write_multiple_registers(int client_socket, uint16_t transaction_id, 
                                           uint8_t unit_id, const uint8_t *pdu, int pdu_len)
{
    // Write Multiple Registers requires: start_addr (2) + quantity (2) + byte_count (1) + data (N) = at least 6 bytes
    if (pdu_len < 6) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    uint16_t start_addr = (pdu[0] << 8) | pdu[1];
    uint16_t quantity = (pdu[2] << 8) | pdu[3];
    uint8_t byte_count = pdu[4];
    
    // Validate parameters
    if (quantity == 0 || quantity > 123 || byte_count != quantity * 2 || pdu_len < 5 + byte_count) {
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_VALUE);
        return true;
    }
    
    // Write registers to map
    if (!modbus_write_holding_registers(start_addr, quantity, &pdu[5])) {
        ESP_LOGE(TAG, "Failed to write holding registers: start_addr=%d, quantity=%d", start_addr, quantity);
        send_exception(client_socket, transaction_id, unit_id, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, 
                      MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return true;
    }
    
    // Response
    uint8_t response[12];
    response[0] = (transaction_id >> 8) & 0xFF;
    response[1] = transaction_id & 0xFF;
    response[2] = 0x00; // Protocol ID
    response[3] = 0x00;
    response[4] = 0x00; // Length
    response[5] = 0x06;
    response[6] = unit_id;
    response[7] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    response[8] = (start_addr >> 8) & 0xFF;
    response[9] = start_addr & 0xFF;
    response[10] = (quantity >> 8) & 0xFF;
    response[11] = quantity & 0xFF;
    
    int sent = send(client_socket, response, sizeof(response), 0);
    if (sent != sizeof(response)) {
        ESP_LOGE(TAG, "Failed to send write response: sent %d of %d bytes", sent, sizeof(response));
    }
    return true;
}

bool modbus_tcp_handle_request(int client_socket)
{
    uint8_t mbap_header[6];
    
    // Read MBAP header (6 bytes)
    int received = recv(client_socket, mbap_header, sizeof(mbap_header), 0);
    
    if (received <= 0) {
        if (received == 0 || errno == ECONNRESET) {
            return false; // Connection closed
        }
        return true; // Try again
    }
    
    if (received < 6) {
        return true; // Wait for more data
    }
    
    uint16_t transaction_id = (mbap_header[0] << 8) | mbap_header[1];
    uint16_t protocol_id = (mbap_header[2] << 8) | mbap_header[3];
    uint16_t length = (mbap_header[4] << 8) | mbap_header[5];
    
    if (protocol_id != 0 || length < 2 || length > 253) {
        return false;
    }
    
    // Read PDU (unit_id + function_code + data)
    // MBAP length field includes unit_id byte, so PDU is length bytes total
    uint8_t pdu[256];
    if (length > sizeof(pdu)) {
        return false;
    }
    
    // Read the full PDU - may need multiple recv calls for large PDUs
    size_t total_received = 0;
    while (total_received < length) {
        received = recv(client_socket, pdu + total_received, length - total_received, 0);
        if (received <= 0) {
            if (received == 0 || errno == ECONNRESET) {
                return false;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, try again
                continue;
            }
            ESP_LOGE(TAG, "Error reading PDU: %s", strerror(errno));
            return false;
        }
        total_received += received;
    }
    
    if (total_received != length || length < 2) {
        return false;
    }
    
    uint8_t unit_id = pdu[0];
    uint8_t function_code = pdu[1];
    const uint8_t *pdu_data = &pdu[2];
    int pdu_data_len = length - 2; // Data portion after unit_id and function_code
    
    bool result = true;
    switch (function_code) {
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            result = handle_read_holding_registers(client_socket, transaction_id, unit_id, pdu_data, pdu_data_len);
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            result = handle_read_input_registers(client_socket, transaction_id, unit_id, pdu_data, pdu_data_len);
            break;
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            result = handle_write_single_register(client_socket, transaction_id, unit_id, pdu_data, pdu_data_len);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            result = handle_write_multiple_registers(client_socket, transaction_id, unit_id, pdu_data, pdu_data_len);
            break;
        default:
            send_exception(client_socket, transaction_id, unit_id, function_code, MODBUS_EX_ILLEGAL_FUNCTION);
            result = true;
            break;
    }
    
    return result;
}

