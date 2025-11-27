# Features to Add to WinUI Application

Based on review of the API endpoints and current UI implementation, here are features that could be added:

## High Priority Features

### 1. **MPU6050 Sensor Support** ✅ COMPLETED
   - **Status**: ✅ **IMPLEMENTED** - Configuration UI and Status page added
   - **Implementation**: MPU6050 configuration card added to Configuration page, Status page created
   - **Features Implemented**:
     - ✅ Enable/disable MPU6050 checkbox
     - ✅ Byte offset configuration (0-18, uses 14 bytes)
     - ✅ Display current enabled state and byte offset
     - ✅ "Device Disabled" warning when disabled
     - ✅ Restart required notification
     - ✅ MPU6050 Status page with accelerometer, gyroscope, and temperature readings
     - ✅ Real-time updates every 250ms
   - **API Endpoints Used**:
     - `GET /api/mpu6050/enabled` - Get enabled state ✅
     - `POST /api/mpu6050/enabled` - Set enabled state ✅
     - `GET /api/mpu6050/byteoffset` - Get byte offset ✅
     - `POST /api/mpu6050/byteoffset` - Set byte offset ✅
   - **Data Source**: Reads MPU6050 data from Input Assembly 100 bytes using configured byte offset (14 bytes: 6 accel, 6 gyro, 2 temp)

### 3. **VL53L1x Sensor Calibration UI**
   - **Status**: API endpoints exist (`/api/calibrate/offset`, `/api/calibrate/xtalk`) but no UI
   - **Implementation**: Add calibration section to Configuration page or Sensor Status page
   - **Features**:
     - Offset calibration button with target distance input (recommended: 100mm)
     - Crosstalk calibration button with target distance input (recommended: 600mm)
     - Display current calibration values (offset_mm, xtalk_cps) from sensor config
     - Show calibration results after completion
     - Progress indicator during calibration process

### 4. **Sensor Configuration - Load Current Settings**
   - **Status**: "Load Current Settings" button exists but may not load all sensor parameters
   - **Implementation**: Ensure `LoadSensorConfig_Click` loads all sensor config fields including:
     - Distance mode, timing budget, inter-measurement period
     - ROI settings (X size, Y size, center SPAD)
     - Calibration values (offset, xtalk)
     - Threshold settings (signal, sigma, low, high, window)
     - Interrupt polarity
     - I2C address

### 5. **Sensor Configuration - Reset to Defaults**
   - **Status**: Mentioned in WebUI docs but not implemented
   - **Implementation**: Add "Reset to Defaults" button that sets all sensor parameters to factory defaults
   - **Note**: May need API endpoint or implement defaults client-side

### 6. **Assembly Sizes Display**
   - **Status**: API endpoint exists (`/api/assemblies/sizes`) but not displayed
   - **Implementation**: Show assembly sizes on Input/Output Assembly pages or Configuration page
   - **Display**: Current size and total buffer size for each assembly

## Medium Priority Features

### 7. **NAU7802 Status/Monitoring Page**
   - **Status**: Configuration exists but no status/monitoring page
   - **Implementation**: Create new NAU7802 Status page similar to Sensor Status page
   - **Features**:
     - Display current load cell reading
     - Show raw ADC values
     - Display byte offset location in assembly
     - Real-time updates (similar to sensor status page)
     - "Device Disabled" warning when disabled

### 8. **MCP230xx Status/Monitoring Page**
   - **Status**: Configuration exists but no status/monitoring page
   - **Implementation**: Create new MCP230xx Status page
   - **Features**:
     - Display current pin states (inputs/outputs)
     - Show device type and update rate
     - List all configured devices with their settings
     - Real-time updates showing pin changes
     - "Device Disabled" warning when disabled

### 9. **Enhanced Sensor Status Page**
   - **Status**: Basic status exists, could be enhanced
   - **Features to Add**:
     - Display current calibration values (offset, xtalk)
     - Show I2C address being used
     - Display ROI configuration (X size, Y size, center SPAD)
     - Show threshold settings (signal, sigma, low, high)
     - Display interrupt polarity setting
     - Show distance mode (SHORT/LONG) more prominently

### 10. **Device Information/System Status Page**
   - **Status**: No system information page exists
   - **Implementation**: Create new System Status page
   - **Features**:
     - Firmware version (if available via API)
     - Uptime/System time
     - Network status (IP, MAC address if available)
     - Enabled features summary (VL53L1x, MCP230xx, NAU7802, Modbus)
     - Assembly sizes
     - Memory/Flash usage (if available via API)

## Low Priority / Nice-to-Have Features

### 11. **Configuration Export/Import**
   - **Status**: Not implemented
   - **Implementation**: Add export/import functionality for device configuration
   - **Features**:
     - Export all settings to JSON file
     - Import settings from JSON file
     - Validate settings before import
     - Show diff before applying imported settings

### 12. **Advanced Sensor Configuration UI**
   - **Status**: Basic config exists, advanced settings may be hidden or hard to access
   - **Features**:
     - Expandable/collapsible sections for different parameter groups
     - Tooltips/help text explaining each parameter
     - Validation with error messages for invalid values
     - Visual indicators for recommended values
     - Preset configurations (e.g., "High Speed", "High Accuracy", "Long Range")

### 13. **Logs Page Enhancements**
   - **Status**: Basic logs page exists
   - **Features to Add**:
     - Filter logs by level (INFO, WARN, ERROR, DEBUG)
     - Search/find functionality in logs
     - Export logs to file
     - Auto-scroll to bottom option
     - Clear logs button (if API supports it)
     - Timestamp formatting options

### 14. **Connection Status Indicator**
   - **Status**: Connection state exists but could be more visible
   - **Features**:
     - Visual indicator in title bar (green/yellow/red dot)
     - Connection quality indicator (latency/ping time)
     - Last successful connection timestamp
     - Connection retry count/status

### 15. **OTA Firmware Update Status** ⭐ NEW FROM API DOCS
   - **Status**: API endpoint exists (`GET /api/ota/status`) but not implemented in UI
   - **Implementation**: Add OTA status display to Firmware Update page
   - **Features**:
     - Display current OTA status (idle, in_progress, complete, error)
     - Show progress percentage (0-100)
     - Display status message
     - Auto-refresh during update process
     - Visual progress indicator
   - **API Endpoint**: `GET /api/ota/status` returns:
     ```json
     {
       "status": "idle|in_progress|complete|error",
       "progress": 0-100,
       "message": "Status message"
     }
     ```

### 16. **Firmware Update Enhancements**
   - **Status**: Basic update exists
   - **Features**:
     - Progress bar during upload (can use `/api/ota/status`)
     - Firmware version check before update
     - Update history/log
     - Rollback option (if supported by device)
     - Update verification after completion

### 17. **Assembly Data Visualization Enhancements**
   - **Status**: Basic byte display exists
   - **Features**:
     - Hex dump view option
     - Interpreted data view (show sensor data, MCP pins, etc. in human-readable format)
     - Data history/graph for specific bytes
     - Copy to clipboard functionality
     - Export assembly data to CSV/JSON

### 18. **Multi-Device Management**
   - **Status**: Single device connection only
   - **Features**:
     - Save multiple device IPs/profiles
     - Quick switch between devices
     - Device naming/labeling
     - Compare configurations between devices

## API Endpoints Already Implemented (No UI Needed)

✅ Network Configuration (`/api/ipconfig`) - **Implemented**
✅ Sensor Configuration (`/api/config`) - **Implemented**
✅ Sensor Enable/Disable (`/api/sensor/enabled`) - **Implemented**
✅ Sensor Byte Offset (`/api/sensor/byteoffset`) - **Implemented**
✅ Modbus Configuration (`/api/modbus`) - **Implemented**
✅ Sensor Status (`/api/status`) - **Implemented**
✅ Assembly Data (`/api/assemblies`) - **Implemented**
✅ MCP230xx Configuration (`/api/mcp/*`) - **Implemented**
✅ I2C Pullup Configuration (`/api/i2c/pullup`) - **Implemented**
✅ NAU7802 Configuration (`/api/nau7802/*`) - **Implemented**
✅ MPU6050 Configuration (`/api/mpu6050/*`) - **✅ IMPLEMENTED** (Config + Status page)
✅ Reboot (`/api/reboot`) - **Implemented**
✅ Logs (`/api/logs`) - **Implemented**
✅ Firmware Update (`/api/ota/update`) - **Implemented**
✅ OTA Status (`/api/ota/status`) - **API exists, not implemented in UI** ⭐ NEW
✅ Assembly Sizes (`/api/assemblies/sizes`) - **API exists, not displayed**

## Missing API Endpoints (May Need Device-Side Implementation)

❓ Firmware version endpoint (`/api/system/version` or similar)
❓ System uptime endpoint (`/api/system/uptime`)
❓ MAC address endpoint (`/api/system/mac`)
❓ Memory/flash usage endpoints
❓ Clear logs endpoint (`/api/logs/clear`)
❓ Export configuration endpoint (`/api/config/export`)
❓ Import configuration endpoint (`/api/config/import`)
❓ Reset to defaults endpoint (`/api/config/reset`)

## Summary

**Total Features Identified**: 18 potential features
**High Priority**: 5 features (Calibration UI, Load Settings, Reset Defaults, Assembly Sizes, OTA Status)
**Completed**: 1 feature (MPU6050 Config + Status) ✅
**Medium Priority**: 4 features (NAU7802 Status, MCP230xx Status, Enhanced Sensor Status, System Status)
**Low Priority**: 7 features (Export/Import, Advanced Config UI, Logs Enhancements, Connection Status, Firmware Enhancements, Assembly Visualization, Multi-Device)

**Most Critical Missing Feature**: **VL53L1x Sensor Calibration UI** - The API endpoints exist (`/api/calibrate/offset`, `/api/calibrate/xtalk`) but there's no way for users to perform calibration through the WinUI application.

**Second Most Critical**: **OTA Firmware Update Status** - The API endpoint exists (`GET /api/ota/status`) but there's no UI to show update progress and status. This would improve the firmware update experience significantly.

**Recently Completed**: ✅ **MPU6050 Sensor Support** - Configuration UI and Status/Monitoring page have been fully implemented.

