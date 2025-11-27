# OTA Upload Connection Close Fix

## Problem

During OTA (Over-The-Air) firmware updates via HTTP multipart upload, the upload process was being aborted with an error when the client closed the connection after successfully sending all data.

**Error Message:**
```
E (17635) webui_api: Connection error during upload (ret=0), aborting OTA
```

## Root Cause

The `httpd_req_recv()` function returns `0` when the client closes the connection (EOF - End of File). This is **normal behavior** when the client finishes sending all data and closes the connection. However, the original code treated any return value `<= 0` as an error condition.

### Original Code (Incorrect)

```c
int ret = httpd_req_recv(req, chunk_buffer, chunk_size);
if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        // Handle timeout...
        continue;
    }
    // ❌ PROBLEM: Treats ret=0 (normal EOF) as an error
    ESP_LOGE(TAG, "Connection error during upload (ret=%d), aborting OTA", ret);
    esp_ota_abort(ota_handle);
    return send_json_error(req, "Connection closed during upload", 500);
}
```

## Solution

The fix distinguishes between:
1. **Normal connection close (`ret == 0`)**: Client finished sending data
2. **Actual errors (`ret < 0`)**: Network errors, socket issues, etc.

When `ret == 0`, we check if we've received sufficient data before treating it as an error.

### Fixed Code

```c
// Calculate expected firmware size for validation (before the loop)
size_t expected_firmware_bytes = 0;
if (content_len > 0) {
    // Account for multipart overhead (boundary + headers, typically ~1KB)
    expected_firmware_bytes = (content_len > 1024) ? (content_len - 1024) : content_len;
}

while (!done) {
    int ret = httpd_req_recv(req, chunk_buffer, chunk_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            // Handle timeout...
            continue;
        }
        
        // ret == 0 means connection closed by client (EOF)
        // This is normal when client finishes sending data
        if (ret == 0) {
            if (done) {
                // Already found boundary, connection close is expected
                ESP_LOGI(TAG, "Connection closed by client after receiving all data");
                break;
            } else if (expected_firmware_bytes > 0 && 
                       total_written >= (expected_firmware_bytes * 95 / 100)) {
                // Received at least 95% of expected data, likely complete
                ESP_LOGI(TAG, "Connection closed by client, received %d bytes (expected ~%d)", 
                         total_written, expected_firmware_bytes);
                break;
            } else {
                // Connection closed but we haven't received enough data
                ESP_LOGE(TAG, "Connection closed prematurely (ret=0): received %d bytes, expected ~%d bytes", 
                         total_written, expected_firmware_bytes);
                esp_ota_abort(ota_handle);
                free(chunk_buffer);
                return send_json_error(req, "Connection closed before upload completed", 500);
            }
        } else {
            // Negative ret value indicates actual error
            ESP_LOGE(TAG, "Connection error during upload (ret=%d), aborting OTA", ret);
            esp_ota_abort(ota_handle);
            free(chunk_buffer);
            return send_json_error(req, "Connection error during upload", 500);
        }
    }
    
    // Process received data...
    // ...
}
```

## Key Changes

1. **Calculate expected size before the loop**: Compute `expected_firmware_bytes` from `Content-Length` header (accounting for multipart overhead) before entering the receive loop.

2. **Distinguish `ret == 0` from `ret < 0`**:
   - `ret == 0`: Normal EOF (connection closed by client)
   - `ret < 0`: Actual error (negative error codes)

3. **Validate data completeness when `ret == 0`**:
   - If boundary was already found (`done == true`) → Normal completion
   - If received ≥95% of expected data → Assume complete
   - Otherwise → Treat as premature close (error)

4. **Only abort on actual errors**: Negative return values or premature connection close with insufficient data.

## Implementation Checklist

When applying this fix to other projects:

- [ ] Calculate `expected_firmware_bytes` before the receive loop
- [ ] Separate handling for `ret == 0` (EOF) vs `ret < 0` (error)
- [ ] Check if boundary was found (`done` flag) when `ret == 0`
- [ ] Validate data completeness (≥95% of expected) when `ret == 0`
- [ ] Only abort OTA on actual errors or premature close with insufficient data
- [ ] Update error messages to distinguish between normal close and errors

## Testing

After applying the fix, test:

1. **Normal upload completion**: Upload should complete successfully when client closes connection after sending all data
2. **Premature close**: Abort should still occur if connection closes before receiving sufficient data
3. **Actual errors**: Network errors should still be properly detected and handled
4. **Timeout handling**: Timeout logic should remain unchanged

## Related Functions

- `httpd_req_recv()`: ESP-IDF HTTP server receive function
  - Returns: `> 0` = bytes received, `0` = EOF (connection closed), `< 0` = error code
- `HTTPD_SOCK_ERR_TIMEOUT`: Timeout error code (negative value)

## Notes

- The 95% threshold accounts for multipart boundary overhead variations
- Multipart boundaries add overhead (~1KB typically) that should be subtracted from `Content-Length`
- This fix applies to streaming OTA updates using HTTP multipart/form-data uploads
- The same principle applies to any HTTP upload handler that uses `httpd_req_recv()`

## References

- ESP-IDF HTTP Server API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html
- HTTP Multipart/Form-Data: RFC 7578

