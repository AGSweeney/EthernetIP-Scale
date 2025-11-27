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

#include "modbus_tcp.h"
#include "modbus_protocol.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <errno.h>

static const char *TAG = "modbus_tcp";
static int s_listen_socket = -1;
static TaskHandle_t s_server_task_handle = NULL;
static bool s_running = false;
static SemaphoreHandle_t s_modbus_mutex = NULL;

#define MODBUS_TCP_PORT 502
#define MODBUS_TCP_MAX_CONNECTIONS 20

static void modbus_tcp_server_task(void *pvParameters)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_sockets[MODBUS_TCP_MAX_CONNECTIONS];
    int max_fd;
    fd_set read_fds;
    bool running = true;
    
    memset(client_sockets, -1, sizeof(client_sockets));
    
    while (running) {
        // Check running flag with mutex protection
        if (s_modbus_mutex != NULL) {
            xSemaphoreTake(s_modbus_mutex, portMAX_DELAY);
            running = s_running;
            xSemaphoreGive(s_modbus_mutex);
        }
        
        if (!running) {
            break;
        }
        FD_ZERO(&read_fds);
        FD_SET(s_listen_socket, &read_fds);
        max_fd = s_listen_socket;
        
        // Add client sockets to set
        for (int i = 0; i < MODBUS_TCP_MAX_CONNECTIONS; i++) {
            if (client_sockets[i] >= 0) {
                FD_SET(client_sockets[i], &read_fds);
                if (client_sockets[i] > max_fd) {
                    max_fd = client_sockets[i];
                }
            }
        }
        
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            ESP_LOGE(TAG, "Select error: %s", strerror(errno));
            break;
        }
        
        // Check for new connections
        if (FD_ISSET(s_listen_socket, &read_fds)) {
            int new_socket = accept(s_listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            if (new_socket >= 0) {
                // New ModbusTCP connection
                
                // Disable Nagle's algorithm for low latency
                int flag = 1;
                if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
                    ESP_LOGW(TAG, "Failed to set TCP_NODELAY on new connection: %s", strerror(errno));
                }
                
                // Find empty slot
                bool added = false;
                for (int i = 0; i < MODBUS_TCP_MAX_CONNECTIONS; i++) {
                    if (client_sockets[i] < 0) {
                        client_sockets[i] = new_socket;
                        added = true;
                        break;
                    }
                }
                
                if (!added) {
                    ESP_LOGE(TAG, "Max connections reached, closing new connection");
                    close(new_socket);
                }
            }
        }
        
        // Check client sockets for data
        for (int i = 0; i < MODBUS_TCP_MAX_CONNECTIONS; i++) {
            if (client_sockets[i] >= 0 && FD_ISSET(client_sockets[i], &read_fds)) {
                if (!modbus_tcp_handle_request(client_sockets[i])) {
                    // Connection closed or error
                    // Closing ModbusTCP connection
                    close(client_sockets[i]);
                    client_sockets[i] = -1;
                }
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < MODBUS_TCP_MAX_CONNECTIONS; i++) {
        if (client_sockets[i] >= 0) {
            close(client_sockets[i]);
        }
    }
    
    if (s_modbus_mutex != NULL) {
        xSemaphoreTake(s_modbus_mutex, portMAX_DELAY);
        s_server_task_handle = NULL;
        xSemaphoreGive(s_modbus_mutex);
    } else {
        s_server_task_handle = NULL;
    }
    vTaskDelete(NULL);
}

bool modbus_tcp_init(void)
{
    if (s_modbus_mutex == NULL) {
        s_modbus_mutex = xSemaphoreCreateMutex();
        if (s_modbus_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create ModbusTCP mutex");
            return false;
        }
    }
    
    xSemaphoreTake(s_modbus_mutex, portMAX_DELAY);
    if (s_listen_socket >= 0) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "ModbusTCP already initialized");
        return true;
    }
    
    s_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_listen_socket < 0) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "Failed to create socket: %s", strerror(errno));
        return false;
    }
    
    int opt = 1;
    if (setsockopt(s_listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "Failed to set socket options: %s", strerror(errno));
        close(s_listen_socket);
        s_listen_socket = -1;
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(MODBUS_TCP_PORT);
    
    if (bind(s_listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "Failed to bind socket: %s", strerror(errno));
        close(s_listen_socket);
        s_listen_socket = -1;
        return false;
    }
    
    if (listen(s_listen_socket, MODBUS_TCP_MAX_CONNECTIONS) < 0) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "Failed to listen: %s", strerror(errno));
        close(s_listen_socket);
        s_listen_socket = -1;
        return false;
    }
    
    xSemaphoreGive(s_modbus_mutex);
    // ModbusTCP server initialized
    return true;
}

bool modbus_tcp_start(void)
{
    if (s_modbus_mutex == NULL) {
        ESP_LOGE(TAG, "ModbusTCP not initialized");
        return false;
    }
    
    xSemaphoreTake(s_modbus_mutex, portMAX_DELAY);
    if (s_listen_socket < 0) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "ModbusTCP not initialized");
        return false;
    }
    
    if (s_server_task_handle != NULL) {
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "ModbusTCP server already running");
        return true;
    }
    
    s_running = true;
    BaseType_t result = xTaskCreate(modbus_tcp_server_task, "modbus_tcp", 8192, NULL, 5, &s_server_task_handle);
    if (result != pdPASS) {
        s_running = false;
        xSemaphoreGive(s_modbus_mutex);
        ESP_LOGE(TAG, "Failed to create ModbusTCP server task");
        return false;
    }
    
    xSemaphoreGive(s_modbus_mutex);
    // ModbusTCP server started
    return true;
}

void modbus_tcp_stop(void)
{
    if (s_modbus_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_modbus_mutex, portMAX_DELAY);
    s_running = false;
    TaskHandle_t task_handle = s_server_task_handle;
    
    // Close socket with mutex protection
    if (s_listen_socket >= 0) {
        close(s_listen_socket);
        s_listen_socket = -1;
    }
    
    xSemaphoreGive(s_modbus_mutex);
    
    if (task_handle != NULL) {
        // Wait for task to finish (with timeout)
        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreTake(s_modbus_mutex, portMAX_DELAY);
        s_server_task_handle = NULL;
        xSemaphoreGive(s_modbus_mutex);
    }
    
    // ModbusTCP server stopped
}

