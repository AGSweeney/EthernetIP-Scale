# Code Review: Race Conditions, Memory Leaks, and Potential Pitfalls

## Executive Summary

This document identifies potential race conditions, memory leaks, and other pitfalls found during code review. Issues are categorized by severity and include recommendations for fixes.

---

## 🔴 CRITICAL ISSUES

### 1. NAU7802 Device Access Race Condition

**Location:** `main/main.c` - NAU7802 scale task and API handlers

**Problem:**
The NAU7802 device (`s_nau7802_device`) is accessed from multiple contexts without proper synchronization:
- `nau7802_scale_task()` reads from the device continuously
- API handlers (`api_get_nau7802_handler`, `api_post_nau7802_calibrate_handler`) read/write device registers
- No mutex protects concurrent I2C operations

**Impact:**
- Corrupted I2C transactions
- Incorrect calibration results
- Device state corruption
- Potential I2C bus lockups

**Current Code:**
```c
// In nau7802_scale_task (line 1472-1493)
nau7802_available(&s_nau7802_device);
nau7802_get_reading(&s_nau7802_device);
nau7802_get_weight(&s_nau7802_device, ...);

// In api_post_nau7802_calibrate_handler (line 1443, 1488)
nau7802_calculate_zero_offset(nau7802, ...);
nau7802_calculate_calibration_factor(nau7802, ...);
nau7802_calibrate_af(nau7802);
```

**Recommendation:**
Add a mutex to protect all NAU7802 device operations:

```c
// In main.c
static SemaphoreHandle_t s_nau7802_mutex = NULL;

// Initialize in app_main()
s_nau7802_mutex = xSemaphoreCreateMutex();

// Protect all device access
if (xSemaphoreTake(s_nau7802_mutex, portMAX_DELAY) == pdTRUE) {
    // Perform device operation
    nau7802_get_reading(&s_nau7802_device);
    xSemaphoreGive(s_nau7802_mutex);
}
```

**Files Affected:**
- `main/main.c` (scale task, initialization)
- `components/webui/src/webui_api.c` (API handlers)

---

### 2. NAU7802 Configuration Race Condition

**Location:** `main/main.c:1466-1467`

**Problem:**
The scale task loads `byte_offset` and `average_samples` once at startup. If these values change via the API, the task continues using stale values until it's restarted.

**Impact:**
- Configuration changes don't take effect immediately
- Inconsistent behavior between API and actual readings

**Current Code:**
```c
static void nau7802_scale_task(void *pvParameters)
{
    uint8_t byte_offset = system_nau7802_byte_offset_load();  // Loaded once
    uint8_t average_samples = system_nau7802_average_load();  // Loaded once
    
    while (1) {
        // Uses stale byte_offset and average_samples
    }
}
```

**Recommendation:**
Reload configuration values periodically or on-demand:

```c
static void nau7802_scale_task(void *pvParameters)
{
    const TickType_t config_reload_interval = pdMS_TO_TICKS(5000);  // Reload every 5s
    TickType_t last_config_reload = xTaskGetTickCount();
    
    uint8_t byte_offset = system_nau7802_byte_offset_load();
    uint8_t average_samples = system_nau7802_average_load();
    
    while (1) {
        // Reload config periodically
        TickType_t now = xTaskGetTickCount();
        if (now - last_config_reload >= config_reload_interval) {
            byte_offset = system_nau7802_byte_offset_load();
            average_samples = system_nau7802_average_load();
            last_config_reload = now;
        }
        // ... rest of task
    }
}
```

**Alternative:** Use a notification mechanism to reload config immediately when changed via API.

---

### 3. NAU7802 Initialization Flag Race Condition

**Location:** `main/main.c:157, 1433, 1472, 1451-1458`

**Problem:**
The `s_nau7802_initialized` flag is accessed from multiple contexts without protection:
- Set in `app_main()` (initialization thread)
- Read in `nau7802_scale_task()` (scale task)
- Read in `scale_application_get_nau7802_handle()` (API handlers)

**Impact:**
- Potential use-after-initialization if flag is cleared while task is running
- Inconsistent state visibility

**Recommendation:**
Protect flag access with a mutex or use atomic operations:

```c
static SemaphoreHandle_t s_nau7802_state_mutex = NULL;

// Initialize in app_main()
s_nau7802_state_mutex = xSemaphoreCreateMutex();

// Access with protection
bool scale_application_is_nau7802_initialized(void)
{
    bool initialized = false;
    if (s_nau7802_state_mutex != NULL) {
        xSemaphoreTake(s_nau7802_state_mutex, portMAX_DELAY);
        initialized = s_nau7802_initialized;
        xSemaphoreGive(s_nau7802_state_mutex);
    }
    return initialized;
}
```

---

## 🟡 MEDIUM PRIORITY ISSUES

### 4. NAU7802 Task Never Deleted

**Location:** `main/main.c:1206, 158`

**Problem:**
The NAU7802 scale task is created but never deleted. If the device is disabled or reinitialized, the old task continues running.

**Impact:**
- Resource waste (stack memory, CPU cycles)
- Potential conflicts if device is reinitialized
- Multiple tasks writing to same assembly location

**Current Code:**
```c
// Task is created but never deleted
xTaskCreate(nau7802_scale_task, "nau7802_task", 4096, NULL, 5, &s_nau7802_task_handle);
```

**Recommendation:**
Delete task before creating a new one, or when device is disabled:

```c
// Before creating new task
if (s_nau7802_task_handle != NULL) {
    vTaskDelete(s_nau7802_task_handle);
    s_nau7802_task_handle = NULL;
}

// Create new task
xTaskCreate(nau7802_scale_task, "nau7802_task", 4096, NULL, 5, &s_nau7802_task_handle);
```

---

### 5. Missing Null Check in Assembly Access

**Location:** `main/main.c:1474-1475`

**Problem:**
The code checks if `assembly_mutex` is NULL, but doesn't handle the case where `scale_application_get_assembly_mutex()` might return NULL during initialization or shutdown.

**Current Code:**
```c
SemaphoreHandle_t assembly_mutex = scale_application_get_assembly_mutex();
if (assembly_mutex != NULL && xSemaphoreTake(assembly_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Access assembly
}
```

**Status:** ✅ **Already Handled** - The NULL check is present, but consider adding a warning log if mutex is NULL.

---

### 6. Potential Buffer Overflow in Boundary Extraction

**Location:** `components/webui/src/webui_api.c:204-209`

**Problem:**
Boundary string extraction has bounds checking, but the buffer size (128) might not be sufficient for very long boundaries.

**Current Code:**
```c
char boundary[128];
int boundary_len = 0;
while (*boundary_str && ... && boundary_len < 127) {
    boundary[boundary_len++] = *boundary_str++;
}
boundary[boundary_len] = '\0';
```

**Status:** ✅ **Safe** - Bounds checking is correct (127 max, null terminator at 128).

**Recommendation:**
Consider validating boundary length and rejecting overly long boundaries:

```c
if (boundary_len == 127) {
    // Boundary too long, reject request
    return send_json_error(req, "Boundary string too long", 400);
}
```

---

### 7. OTA Upload Header Buffer Size

**Location:** `components/webui/src/webui_api.c:213`

**Problem:**
64KB buffer for multipart headers is very large and could cause memory pressure on ESP32.

**Current Code:**
```c
const size_t header_buffer_size = 64 * 1024;  // 64KB
char *header_buffer = malloc(header_buffer_size);
```

**Recommendation:**
Reduce buffer size or use streaming approach:

```c
const size_t header_buffer_size = 4 * 1024;  // 4KB should be sufficient
// Or implement streaming header parsing
```

---

### 8. Missing Error Handling in NAU7802 Calibration

**Location:** `components/webui/src/webui_api.c:1443, 1488`

**Problem:**
If `nau7802_calculate_zero_offset()` or `nau7802_calculate_calibration_factor()` fail, the error is logged but the device state might be inconsistent.

**Current Code:**
```c
esp_err_t err = nau7802_calculate_zero_offset(nau7802, 10, 5000);
if (err == ESP_OK) {
    // Save to NVS
} else {
    // Only sets error status in response
}
```

**Recommendation:**
Ensure device state is rolled back on error, or at least log the specific error:

```c
esp_err_t err = nau7802_calculate_zero_offset(nau7802, 10, 5000);
if (err == ESP_OK) {
    float zero_offset = nau7802_get_zero_offset(nau7802);
    system_nau7802_zero_offset_save(zero_offset);
    // Success response
} else {
    ESP_LOGE(TAG, "Tare calibration failed: %s", esp_err_to_name(err));
    // Error response with specific error message
}
```

---

## 🟢 LOW PRIORITY / MINOR ISSUES

### 9. I2C Bus Handle Cleanup

**Location:** `main/main.c:156, 1345`

**Problem:**
The I2C bus handle (`s_i2c_bus_handle`) is created but never explicitly deleted. ESP-IDF should handle this on shutdown, but explicit cleanup is better practice.

**Recommendation:**
Add cleanup function called on shutdown or error:

```c
if (s_i2c_bus_handle != NULL) {
    i2c_del_master_bus(s_i2c_bus_handle);
    s_i2c_bus_handle = NULL;
}
```

---

### 10. Assembly Mutex Timeout Handling

**Location:** `main/main.c:1475`

**Problem:**
If the assembly mutex cannot be acquired within 100ms, the scale reading is silently skipped. This could lead to stale data.

**Current Code:**
```c
if (assembly_mutex != NULL && xSemaphoreTake(assembly_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Update assembly
}
// If timeout, silently skip update
```

**Recommendation:**
Log a warning if timeout occurs frequently:

```c
static uint32_t s_mutex_timeout_count = 0;
if (assembly_mutex != NULL && xSemaphoreTake(assembly_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    s_mutex_timeout_count = 0;  // Reset on success
    // Update assembly
} else {
    s_mutex_timeout_count++;
    if (s_mutex_timeout_count % 100 == 0) {  // Log every 100 timeouts
        ESP_LOGW(TAG, "Assembly mutex timeout (count: %lu)", s_mutex_timeout_count);
    }
}
```

---

### 11. Potential Integer Overflow in Weight Scaling

**Location:** `main/main.c:1508`

**Problem:**
Weight scaling calculation could overflow if weight is very large:

```c
int32_t weight_scaled = (int32_t)(weight_converted * 100.0f + 0.5f);
```

**Impact:**
- Very large weights (>21,474,836.47 in selected unit) would overflow int32_t
- Negative values in assembly data

**Recommendation:**
Add bounds checking:

```c
// Clamp to int32_t range
const float max_weight = 21474836.47f;  // Max int32_t / 100
const float min_weight = -21474836.48f;  // Min int32_t / 100

if (weight_converted > max_weight) {
    weight_converted = max_weight;
    ESP_LOGW(TAG, "Weight clamped to maximum");
} else if (weight_converted < min_weight) {
    weight_converted = min_weight;
    ESP_LOGW(TAG, "Weight clamped to minimum");
}

int32_t weight_scaled = (int32_t)(weight_converted * 100.0f + 0.5f);
```

---

### 12. Missing Validation in API Handlers

**Location:** `components/webui/src/webui_api.c:1411-1417`

**Problem:**
The calibration handler reads only 256 bytes for JSON content. Very large JSON payloads could be truncated.

**Current Code:**
```c
char content[256];
int ret = httpd_req_recv(req, content, sizeof(content) - 1);
```

**Status:** ✅ **Acceptable** - Calibration requests should be small. Consider adding explicit size validation.

---

## ✅ GOOD PRACTICES OBSERVED

1. **Memory Management:**
   - ACD context structures are properly freed
   - cJSON objects are properly deleted
   - OTA header buffer is freed on all error paths
   - Log buffer cleanup on error

2. **Thread Safety:**
   - Assembly data access is protected by mutex
   - Modbus TCP uses mutex for state protection
   - OpENer initialization uses mutex

3. **Buffer Safety:**
   - String operations use `snprintf` (safe)
   - Boundary extraction has bounds checking
   - Byte offset validation prevents overflow

4. **Error Handling:**
   - Most error paths clean up resources
   - Error responses are properly formatted

---

## Summary of Recommendations

### Immediate Actions (Critical):
1. ✅ Add mutex for NAU7802 device access
2. ✅ Reload configuration in scale task periodically
3. ✅ Protect `s_nau7802_initialized` flag access

### Short-term (Medium Priority):
4. ✅ Delete NAU7802 task before creating new one
5. ✅ Add weight clamping to prevent integer overflow
6. ✅ Reduce OTA header buffer size

### Long-term (Low Priority):
7. ✅ Add explicit I2C bus cleanup
8. ✅ Add mutex timeout logging
9. ✅ Improve error messages in calibration handlers

---

## Testing Recommendations

1. **Stress Testing:**
   - Rapid API calls while scale task is running
   - Configuration changes during active readings
   - Multiple calibration requests in quick succession

2. **Memory Testing:**
   - Monitor heap usage during OTA uploads
   - Check for memory leaks during long-running operation
   - Verify task stack usage

3. **Race Condition Testing:**
   - Concurrent API access to NAU7802
   - Configuration changes during calibration
   - Device disable/enable cycles

---

**Last Updated:** See git commit history  
**Review Date:** 2025-01-XX

