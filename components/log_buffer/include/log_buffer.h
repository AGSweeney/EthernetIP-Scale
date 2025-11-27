#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the log buffer system
 * 
 * @param buffer_size Size of the circular buffer in bytes
 * @return true if initialization successful, false otherwise
 */
bool log_buffer_init(size_t buffer_size);

/**
 * @brief Get the current log buffer contents
 * 
 * @param buffer Output buffer to store logs
 * @param buffer_size Size of output buffer
 * @return Number of bytes written to buffer
 */
size_t log_buffer_get(char *buffer, size_t buffer_size);

/**
 * @brief Get the total number of bytes in the log buffer
 * 
 * @return Number of bytes currently stored
 */
size_t log_buffer_get_size(void);

/**
 * @brief Clear the log buffer
 */
void log_buffer_clear(void);

/**
 * @brief Check if log buffer is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool log_buffer_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // LOG_BUFFER_H

