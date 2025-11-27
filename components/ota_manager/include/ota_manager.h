/**
 * @file ota_manager.h
 * @brief Over-The-Air (OTA) firmware update manager
 * 
 * Provides OTA firmware update functionality, including URL-based updates,
 * binary data updates, and streaming updates.
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_ota_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update status enumeration
 */
typedef enum {
    OTA_STATUS_IDLE,         /**< No update in progress */
    OTA_STATUS_IN_PROGRESS,  /**< Update in progress */
    OTA_STATUS_COMPLETE,     /**< Update completed successfully */
    OTA_STATUS_ERROR         /**< Update failed */
} ota_status_t;

/**
 * @brief OTA status information structure
 */
typedef struct {
    ota_status_t status;    /**< Current OTA status */
    uint8_t progress;        /**< Progress percentage (0-100) */
    char message[128];       /**< Status message string */
} ota_status_info_t;

/**
 * @brief Function prototypes
 * @{
 */

/**
 * @brief Initialize OTA manager
 * 
 * Must be called before using any OTA functions.
 * 
 * @return true on success, false on error
 */
bool ota_manager_init(void);

/**
 * @brief Start OTA update from URL
 * 
 * Downloads firmware from the specified URL and updates the device.
 * 
 * @param url Firmware URL (HTTP or HTTPS)
 * @return true if update started successfully, false on error
 */
bool ota_manager_start_update(const char *url);

/**
 * @brief Start OTA update from uploaded binary data
 * 
 * Updates the device using firmware data provided in memory.
 * 
 * @param data Pointer to binary firmware data
 * @param data_len Length of firmware data in bytes
 * @return true if update started successfully, false on error
 */
bool ota_manager_start_update_from_data(const uint8_t *data, size_t data_len);

/**
 * @brief Start streaming OTA update
 * 
 * Starts a streaming OTA update that writes directly to the partition
 * without double buffering. Use ota_manager_write_streaming_chunk() to
 * write data chunks, then ota_manager_finish_streaming_update() to complete.
 * 
 * @param expected_size Expected firmware size in bytes (for validation)
 * @return OTA handle on success, 0 on error
 */
esp_ota_handle_t ota_manager_start_streaming_update(size_t expected_size);

/**
 * @brief Write chunk of firmware data to streaming OTA update
 * 
 * Writes a chunk of firmware data to an active streaming OTA update.
 * 
 * @param ota_handle OTA handle from ota_manager_start_streaming_update()
 * @param data Pointer to data chunk
 * @param len Length of data chunk in bytes
 * @return true on success, false on error
 */
bool ota_manager_write_streaming_chunk(esp_ota_handle_t ota_handle, const uint8_t *data, size_t len);

/**
 * @brief Finish streaming OTA update
 * 
 * Completes a streaming OTA update and sets the new partition as bootable.
 * 
 * @param ota_handle OTA handle from ota_manager_start_streaming_update()
 * @return true on success, false on error
 */
bool ota_manager_finish_streaming_update(esp_ota_handle_t ota_handle);

/**
 * @brief Get current OTA status
 * 
 * Retrieves the current status of an OTA update operation.
 * 
 * @param status_info Pointer to status structure to populate
 * @return true on success, false on error
 */
bool ota_manager_get_status(ota_status_info_t *status_info);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // OTA_MANAGER_H

