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

#include "webui.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "webui_api.h"
#include "lwip/sockets.h"
#include <string.h>

// Forward declarations for HTML content functions
const char *webui_get_index_html(void);
const char *webui_get_ota_html(void);
const char *webui_get_nau7802_html(void);
// Removed - use API instead

static const char *TAG = "webui";
static httpd_handle_t server_handle = NULL;

static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html = webui_get_index_html();
    
    // Calculate actual length by scanning for null terminator
    // This handles cases where strlen() might stop early due to embedded nulls
    size_t html_len = 0;
    const char *p = html;
    while (*p != '\0' && html_len < 200000) { // Safety limit of 200KB
        html_len++;
        p++;
    }
    
    ESP_LOGI(TAG, "HTML string length: %zu bytes", html_len);
    
    // Verify the string ends with </html>
    const char *html_end = "</html>";
    size_t html_end_len = strlen(html_end);
    if (html_len < html_end_len || strncmp(html + html_len - html_end_len, html_end, html_end_len) != 0) {
        ESP_LOGW(TAG, "HTML string may be truncated! Length: %zu, Last 30 bytes: %.30s", html_len, html + (html_len > 30 ? html_len - 30 : 0));
    } else {
        ESP_LOGI(TAG, "HTML string appears complete, ends with </html>");
    }
    
    // Check if we hit the safety limit (indicates potential issue)
    if (html_len >= 200000) {
        ESP_LOGE(TAG, "HTML string appears to be too long or unterminated!");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Check for null bytes in the string (which would cause strlen to stop early)
    for (size_t i = 0; i < html_len && i < 50000; i++) {
        if (html[i] == '\0' && i < html_len - 1) {
            ESP_LOGW(TAG, "Found null byte at position %zu (before end of string)", i);
            break;
        }
    }
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    // Use chunked transfer encoding for large responses
    // This avoids buffer size limits in httpd_resp_send
    httpd_resp_set_hdr(req, "Transfer-Encoding", "chunked");
    
    const size_t chunk_size = 4096;
    size_t sent = 0;
    esp_err_t ret = ESP_OK;
    
    while (sent < html_len && ret == ESP_OK) {
        size_t to_send = (html_len - sent > chunk_size) ? chunk_size : (html_len - sent);
        ret = httpd_resp_send_chunk(req, html + sent, to_send);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send HTML chunk at offset %zu/%zu: %s", sent, html_len, esp_err_to_name(ret));
            return ret;
        }
        sent += to_send;
    }
    
    // Finalize chunked transfer
    ret = httpd_resp_send_chunk(req, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finalize chunked transfer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Successfully sent %zu bytes of HTML using chunked encoding", sent);
    return ESP_OK;
}


static esp_err_t ota_handler(httpd_req_t *req)
{
    const char *html = webui_get_ota_html();
    size_t html_len = strlen(html);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, html_len);
    return ESP_OK;
}

static esp_err_t nau7802_handler(httpd_req_t *req)
{
    const char *html = webui_get_nau7802_html();
    size_t html_len = strlen(html);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, html_len);
    return ESP_OK;
}

// Removed - use API instead

static esp_err_t favicon_handler(httpd_req_t *req)
{
    // Return 204 No Content - browsers will stop requesting favicon
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};


static const httpd_uri_t ota_uri = {
    .uri       = "/ota",
    .method    = HTTP_GET,
    .handler   = ota_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t nau7802_uri = {
    .uri       = "/nau7802",
    .method    = HTTP_GET,
    .handler   = nau7802_handler,
    .user_ctx  = NULL
};

// Removed - use API instead

static const httpd_uri_t favicon_uri = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_handler,
    .user_ctx  = NULL
};

bool webui_init(void)
{
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "Web UI server already initialized");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 35; // Increased to accommodate all API endpoints (currently 34 handlers: 4 HTML + 30 API)
    config.max_open_sockets = 7;
    config.stack_size = 20480; // Increased to 20KB for large HTML pages and file uploads
    config.task_priority = 5;
    config.core_id = 1; // Run on Core 1 with sensor task
    config.max_req_hdr_len = 2048; // Increased to 2KB for larger multipart headers

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    // Note: TCP_NODELAY (Nagle's algorithm disabled) would improve Web API responsiveness
    // but ESP-IDF httpd manages sockets internally and doesn't expose a socket callback.
    // To enable TCP_NODELAY for HTTP server, use an lwIP hook or patch ESP-IDF httpd component.
    // For now, TCP_NODELAY is enabled for Modbus TCP and EtherNet/IP (critical services).
    
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started");
        
        // Register URI handlers
        httpd_register_uri_handler(server_handle, &root_uri);
        httpd_register_uri_handler(server_handle, &ota_uri);
        httpd_register_uri_handler(server_handle, &nau7802_uri);
        httpd_register_uri_handler(server_handle, &favicon_uri);
        
        // Register API handlers
        webui_register_api_handlers(server_handle);
        
        return true;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return false;
}

void webui_stop(void)
{
    if (server_handle != NULL) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

