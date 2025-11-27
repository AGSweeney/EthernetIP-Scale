#ifndef WEBUI_H
#define WEBUI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the web UI HTTP server
 * 
 * @return true on success, false on error
 */
bool webui_init(void);

/**
 * @brief Stop the web UI HTTP server
 */
void webui_stop(void);

#ifdef __cplusplus
}
#endif

#endif // WEBUI_H

