#ifndef WEBUI_API_H
#define WEBUI_API_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all API endpoint handlers
 * 
 * @param server HTTP server handle
 */
void webui_register_api_handlers(httpd_handle_t server);

/**
 * @brief Get index HTML page
 * 
 * @return HTML content as string
 */
const char *webui_get_index_html(void);

/**
 * @brief Get status HTML page
 * 
 * @return HTML content as string
 */
const char *webui_get_status_html(void);

/**
 * @brief Get EtherNet/IP HTML page
 * 
 * @return HTML content as string
 */
const char *webui_get_ethernetip_html(void);

/**
 * @brief Get Input Assembly HTML page
 * 
 * @return HTML content as string
 */
const char *webui_get_input_assembly_html(void);

/**
 * @brief Get OTA HTML page
 * 
 * @return HTML content as string
 */
const char *webui_get_ota_html(void);

#ifdef __cplusplus
}
#endif

#endif // WEBUI_API_H

