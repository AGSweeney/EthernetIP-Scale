# Web UI API Endpoints Documentation

This document describes all REST API endpoints available in the ESP32-P4 EtherNet/IP adapter web interface.

## Base URL

All API endpoints are prefixed with `/api`. The base URL is typically `http://<device-ip>/api`.

## Response Format

All endpoints return JSON responses. Success responses typically include:
- `status`: "ok" or "error"
- `message`: Human-readable message
- Additional endpoint-specific fields

Error responses include:
- `status`: "error"
- `message`: Error description

HTTP status codes:
- `200 OK`: Success
- `400 Bad Request`: Invalid request parameters
- `500 Internal Server Error`: Server-side error
- `503 Service Unavailable`: Service not available (e.g., log buffer disabled)

---

## System Configuration

### GET /api/ipconfig

Get current IP network configuration.

**Response:**
```json
{
  "use_dhcp": true,
  "ip_address": "172.16.82.99",
  "netmask": "255.255.255.0",
  "gateway": "172.16.82.1",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4"
}
```

### POST /api/ipconfig

Set IP network configuration. **Reboot required** for changes to take effect.

**Request:**
```json
{
  "use_dhcp": false,
  "ip_address": "192.168.1.100",
  "netmask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4"
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "IP configuration saved successfully. Reboot required to apply changes."
}
```

**Notes:**
- If `use_dhcp` is `true`, static IP fields are ignored
- DNS fields are optional and can be set independently

---

### GET /api/reboot

Reboot the device.

**Request:** POST (no body required)

**Response:**
```json
{
  "status": "ok",
  "message": "Device rebooting..."
}
```

**Note:** The device will reboot immediately after sending the response.

---

### GET /api/logs

Get system logs from the log buffer.

**Response:**
```json
{
  "status": "ok",
  "logs": "I (12345) main: System started...\n...",
  "size": 1024,
  "total_size": 8192,
  "truncated": false
}
```

**Fields:**
- `logs`: String containing log entries (may be truncated to 32KB)
- `size`: Number of bytes returned
- `total_size`: Total size of log buffer
- `truncated`: Whether the response was truncated

**Note:** Returns 503 if log buffer is not enabled.

---

### GET /api/assemblies

Get EtherNet/IP assembly data.

**Response:**
```json
{
  "input_assembly_100": {
    "raw_bytes": [0, 1, 2, ...]
  },
  "output_assembly_150": {
    "raw_bytes": [0, 0, 0, ...]
  }
}
```

---

### GET /api/status

Get assembly data with parsed sensor information. This endpoint provides both raw assembly bytes and parsed data for configured sensors.

**Response:**
```json
{
  "input_assembly_100": {
    "raw_bytes": [0, 1, 2, ...],
    "nau7802_data": {
      "weight_scaled": 10024,
      "weight": 100.24,
      "unit": "lbs",
      "unit_code": 1,
      "raw_reading": 1234567,
      "byte_offset": 0,
      "available": true,
      "connected": true,
      "initialized": true,
      "status_byte": 7
    }
  },
  "output_assembly_150": {
    "raw_bytes": [0, 0, 0, ...]
  }
}
```

**Fields:**
- `input_assembly_100`: Object - Input Assembly 100 data
  - `raw_bytes`: Array of integers (0-255) - Raw 32-byte assembly data
  - `nau7802_data`: Object (optional) - Parsed NAU7802 data if enabled and initialized
    - `weight_scaled`: Integer - Weight value scaled by 100 (e.g., 100.24 lbs = 10024)
    - `weight`: Float - Actual weight in selected unit
    - `unit`: String - Unit label ("g", "lbs", or "kg")
    - `unit_code`: Integer - Unit code (0=grams, 1=lbs, 2=kg)
    - `raw_reading`: Integer - Raw 24-bit ADC reading
    - `byte_offset`: Integer - Byte offset in assembly where NAU7802 data starts
    - `available`: Boolean - Data ready flag (bit 0 of status byte)
    - `connected`: Boolean - Device connected flag (bit 1 of status byte)
    - `initialized`: Boolean - Device initialized flag (bit 2 of status byte)
    - `status_byte`: Integer - Raw status byte value (bits: 0=available, 1=connected, 2=initialized)
- `output_assembly_150`: Object - Output Assembly 150 data
  - `raw_bytes`: Array of integers (0-255) - Raw 32-byte assembly data

**Notes:**
- `nau7802_data` is only included if NAU7802 is enabled and initialized
- Weight is extracted from assembly data at the configured `byte_offset`
- Weight is stored in assembly as a scaled integer (value × 100) to avoid floating-point
- Status flags are extracted from the status byte at `byte_offset + 9`
- This endpoint provides a convenient way to read both raw and parsed sensor data

---

---

## Modbus TCP Configuration

### GET /api/modbus

Get Modbus TCP server enabled state.

**Response:**
```json
{
  "enabled": true
}
```

### POST /api/modbus

Set Modbus TCP server enabled state. Changes take effect immediately.

**Request:**
```json
{
  "enabled": true
}
```

**Response:**
```json
{
  "status": "ok",
  "enabled": true,
  "message": "Modbus state saved successfully"
}
```

**Notes:**
- Modbus TCP server runs on port 502
- Input Registers 0-15 map to Input Assembly 100
- Holding Registers 100-115 map to Output Assembly 150

---

## I2C Bus Configuration

### GET /api/i2c/pullup

Get I2C internal pull-up enabled state.

**Response:**
```json
{
  "enabled": true
}
```

### POST /api/i2c/pullup

Set I2C internal pull-up enabled state. **Restart required** for changes to take effect.

**Request:**
```json
{
  "enabled": true
}
```

**Response:**
```json
{
  "status": "ok",
  "enabled": true,
  "message": "I2C pull-up setting saved. Restart required for changes to take effect."
}
```

**Notes:**
- Enables ESP32 internal pull-ups (~45kΩ) for I2C SDA/SCL lines
- Disable if using external pull-ups
- System-wide setting affects all I2C devices

---

## NAU7802 Scale Configuration

### GET /api/nau7802

Get NAU7802 scale configuration, current readings, and device status.

**Response:**
```json
{
  "enabled": true,
  "byte_offset": 0,
  "unit": 1,
  "unit_label": "lbs",
  "gain": 7,
  "gain_label": "x128",
  "sample_rate": 3,
  "sample_rate_label": "80 SPS",
  "channel": 0,
  "channel_label": "Channel 1",
  "ldo_value": 4,
  "ldo_voltage": 3.3,
  "average": 1,
  "initialized": true,
  "connected": true,
  "available": true,
  "weight": 100.24,
  "raw_reading": 1234567,
  "calibration_factor": 1234.56,
  "zero_offset": 12345.0,
  "revision_code": 15,
  "channel1": {
    "offset": 12345,
    "gain": 1234567
  },
  "channel2": {
    "offset": 0,
    "gain": 0
  },
  "status": {
    "available": true,
    "power_digital": true,
    "power_analog": true,
    "power_regulator": true,
    "calibration_active": false,
    "calibration_error": false,
    "oscillator_ready": true,
    "avdd_ready": true
  }
}
```

**Fields:**
- `enabled`: Boolean - Whether NAU7802 is enabled
- `byte_offset`: Integer (0-22) - Starting byte position in Assembly 100
- `unit`: Integer - Weight unit (0=grams, 1=lbs, 2=kg)
- `unit_label`: String - Human-readable unit label
- `gain`: Integer (0-7) - PGA gain setting (0=x1, 1=x2, ..., 7=x128)
- `gain_label`: String - Human-readable gain label
- `sample_rate`: Integer - Sample rate (0=10, 1=20, 2=40, 3=80, 7=320 SPS)
- `sample_rate_label`: String - Human-readable sample rate label
- `channel`: Integer (0-1) - Active channel (0=Channel 1, 1=Channel 2)
- `channel_label`: String - Human-readable channel label
- `ldo_value`: Integer (0-7) - LDO voltage setting
- `ldo_voltage`: Float - LDO voltage in volts
- `average`: Integer (1-50) - Number of samples to average for regular weight readings (default: 1 = no averaging)
- `initialized`: Boolean - Device initialization status
- `connected`: Boolean - Device connection status (I2C response)
- `available`: Boolean - New reading available (data ready)
- `weight`: Float - Current weight reading in selected unit
- `raw_reading`: Integer - Raw 24-bit ADC reading
- `calibration_factor`: Float - Current calibration factor
- `zero_offset`: Float - Current zero offset (tare value)
- `revision_code`: Integer - Device revision code (typically 0x0F)
- `channel1`: Object - Channel 1 calibration registers
  - `offset`: Integer - 24-bit signed offset value
  - `gain`: Integer - 24-bit unsigned gain value
- `channel2`: Object - Channel 2 calibration registers
  - `offset`: Integer - 24-bit signed offset value
  - `gain`: Integer - 24-bit unsigned gain value
- `status`: Object - Device status flags
  - `available`: Boolean - Data ready flag
  - `power_digital`: Boolean - Digital power status
  - `power_analog`: Boolean - Analog power status
  - `power_regulator`: Boolean - Regulator power status
  - `calibration_active`: Boolean - AFE calibration in progress
  - `calibration_error`: Boolean - Calibration error flag
  - `oscillator_ready`: Boolean - Oscillator ready flag
  - `avdd_ready`: Boolean - AVDD ready flag

**Notes:**
- All configuration values are loaded from NVS
- Weight is converted to the selected unit for display
- Status flags are read from device registers in real-time
- Channel calibration registers are for external calibration mode

---

### POST /api/nau7802

Configure NAU7802 settings. **Reboot required** for gain, sample_rate, channel, or LDO changes to take effect.

**Request:**
```json
{
  "enabled": true,
  "byte_offset": 0,
  "unit": 1,
  "gain": 7,
  "sample_rate": 3,
  "channel": 0,
  "ldo_value": 4,
  "average": 1
}
```

**Request Fields:**
- `enabled`: Boolean (optional) - Enable/disable NAU7802
- `byte_offset`: Integer (0-22, optional) - Starting byte position in Assembly 100. Must allow for 10 bytes of data (weight: 4 bytes, raw: 4 bytes, unit: 1 byte, status: 1 byte). Maximum offset = 32 - 10 = 22
- `unit`: Integer (0-2, optional) - Weight unit (0=grams, 1=lbs, 2=kg)
- `gain`: Integer (0-7, optional) - PGA gain (0=x1, 1=x2, 2=x4, 3=x8, 4=x16, 5=x32, 6=x64, 7=x128). **Requires AFE recalibration after change**
- `sample_rate`: Integer (0,1,2,3,7, optional) - Sample rate (0=10, 1=20, 2=40, 3=80, 7=320 SPS). **Requires AFE recalibration after change**
- `channel`: Integer (0-1, optional) - Active channel (0=Channel 1, 1=Channel 2)
- `ldo_value`: Integer (0-7, optional) - LDO voltage (0=4.5V, 1=4.2V, 2=3.9V, 3=3.6V, 4=3.3V, 5=3.0V, 6=2.7V, 7=2.4V). **Requires reboot**
- `average`: Integer (1-50, optional) - Number of samples to average for regular weight readings. Default: 1 (no averaging). Higher values = more stable but slower updates. **Takes effect immediately**

**Response:**
```json
{
  "status": "ok",
  "message": "Configuration saved. Reboot required to apply gain, sample rate, channel, or LDO changes."
}
```

**Validation:**
- `byte_offset` must be between 0 and 22 (inclusive)
- `unit` must be 0, 1, or 2
- `gain` must be between 0 and 7 (inclusive)
- `sample_rate` must be 0, 1, 2, 3, or 7
- `channel` must be 0 or 1
- `ldo_value` must be between 0 and 7 (inclusive)

**Error Response (400 Bad Request):**
```json
{
  "status": "error",
  "message": "Byte offset too large. Maximum is 22 (assembly size 32 - data size 10)"
}
```

**Notes:**
- Only provided fields are updated (partial updates supported)
- Changes to `enabled`, `byte_offset`, `unit`, and `average` take effect immediately
- Changes to `gain`, `sample_rate`, `channel`, or `ldo_value` require reboot
- After changing gain or sample rate, AFE recalibration is required (performed automatically on next boot)
- `average` setting is separate from calibration `samples` parameter (see `/api/nau7802/calibrate`)
- All settings are persisted to NVS

---

### POST /api/nau7802/calibrate

Perform scale calibration. Supports three types of calibration:

1. **Tare (Zero Offset)** - Software calibration to set zero point
2. **Known Weight Calibration** - Software calibration to calculate calibration factor
3. **AFE (Analog Front End) Calibration** - Hardware calibration of the chip's analog front end

**Request (Tare):**
```json
{
  "action": "tare"
}
```

**Request (Known Weight):**
```json
{
  "action": "calibrate",
  "known_weight": 100.0
}
```

**Request (AFE Calibration):**
```json
{
  "action": "afe"
}
```

**Request Fields:**
- `action`: String (required) - Calibration action: `"tare"`, `"calibrate"`, or `"afe"`
- `known_weight`: Float (required for "calibrate") - Known weight value in the currently selected unit (grams, lbs, or kg)

**Response (Tare):**
```json
{
  "status": "ok",
  "message": "Tare calibration completed",
  "zero_offset": 12345.0
}
```

**Response (Known Weight):**
```json
{
  "status": "ok",
  "message": "Calibration completed successfully",
  "calibration_factor": 1234.56,
  "zero_offset": 12345.0
}
```

**Response (AFE Calibration):**
```json
{
  "status": "ok",
  "message": "AFE calibration completed successfully"
}
```

**Error Response (400 Bad Request):**
```json
{
  "status": "error",
  "message": "Missing or invalid 'action' field (must be 'tare', 'calibrate', or 'afe')"
}
```

or

```json
{
  "status": "error",
  "message": "Missing or invalid 'known_weight' field"
}
```

**Error Response (500 Internal Server Error):**
```json
{
  "status": "error",
  "message": "NAU7802 not initialized"
}
```

**Calibration Types:**

1. **Tare (Zero Offset) - Software Calibration:**
   - Removes all weight from scale
   - Waits for scale to stabilize
   - Averages the specified number of samples
   - Calculates and stores zero offset
   - Used to compensate for tare weight (container, platform, etc.)

2. **Known Weight Calibration - Software Calibration:**
   - Perform tare first (zero offset must be set)
   - Place known weight on scale
   - Wait for scale to stabilize
   - Averages the specified number of samples
   - Converts weight to grams internally based on configured unit
   - Calculates calibration factor: `calibration_factor = (reading - zero_offset) / known_weight`
   - Both calibration factor and zero offset are saved to NVS
   - Used to convert raw ADC readings to weight values

3. **AFE (Analog Front End) Calibration - Hardware Calibration:**
   - Calibrates the chip's internal analog front end hardware
   - Must be performed when gain or sample rate changes
   - Automatically performed on boot when gain/sample rate changes are detected
   - Scale must be empty and stable
   - Stores calibration values in device hardware registers
   - Different from weight calibration (which is software-based)

**Notes:**
- `known_weight` is interpreted in the currently configured unit (see `unit` field from GET /api/nau7802)
- The device converts the weight to grams internally before calibration
- Software calibration data (calibration factor and zero offset) is automatically saved to NVS
- Software calibration persists across reboots
- AFE calibration is hardware-based and stored in device registers
- AFE calibration is automatically performed when gain or sample rate changes (on next boot)
- For best results, use a stable known weight and wait for readings to stabilize
- Calibration uses a fixed 10-sample average internally for accuracy. This is separate from the `average` setting used for regular weight readings (configured via POST /api/nau7802).

**Unit Conversion:**
- Grams (unit=0): No conversion
- Pounds (unit=1): `grams = lbs × 453.592`
- Kilograms (unit=2): `grams = kg × 1000`

---

## OTA (Over-The-Air) Firmware Update

### POST /api/ota/update

Trigger OTA firmware update. Supports two methods:

#### Method 1: File Upload (multipart/form-data)

Upload firmware binary file directly.

**Request:**
- Method: POST
- Content-Type: `multipart/form-data`
- Body: Multipart form with firmware file

**Response:**
```json
{
  "status": "ok",
  "message": "Firmware uploaded successfully. Finishing update and rebooting..."
}
```

**Notes:**
- Maximum file size: 2MB
- Device will reboot after successful upload
- Uses streaming to handle large files efficiently

#### Method 2: URL-based Update (application/json)

Download firmware from URL.

**Request:**
```json
{
  "url": "http://example.com/firmware.bin"
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "OTA update started"
}
```

**Notes:**
- Update runs in background
- Check status with `/api/ota/status`

---

### GET /api/ota/status

Get OTA update status.

**Response:**
```json
{
  "status": "idle",
  "progress": 0,
  "message": "No update in progress"
}
```

**Status values:**
- `idle`: No update in progress
- `in_progress`: Update currently running
- `complete`: Update completed successfully
- `error`: Update failed

**Fields:**
- `progress`: Progress percentage (0-100)
- `message`: Status message

---

## Error Responses

All endpoints may return error responses in the following format:

```json
{
  "status": "error",
  "message": "Error description"
}
```

Common HTTP status codes:
- `400 Bad Request`: Invalid request parameters or JSON
- `500 Internal Server Error`: Server-side error (e.g., NVS save failed)
- `503 Service Unavailable`: Service not available (e.g., log buffer disabled)

---

## Notes

1. **Restart Required**: Some endpoints require a device restart for changes to take effect. These are clearly marked in the documentation.
   - NAU7802: Changes to `gain`, `sample_rate`, `channel`, or `ldo_value` require reboot
   - IP Configuration: All changes require reboot
   - I2C Pull-up: Changes require reboot

2. **Immediate Effect**: Some endpoints apply changes immediately (e.g., NAU7802 `enabled`, `byte_offset`, `unit`).

3. **Thread Safety**: Assembly data access is thread-safe using mutexes.

4. **Caching**: Some endpoints use caching to avoid frequent NVS reads:
   - NAU7802: `enabled`, `byte_offset`, `unit`, `gain`, `sample_rate`, `channel`, `ldo_value`
   - Cache is invalidated when values are updated via POST requests

5. **Validation**: All endpoints validate input parameters before processing:
   - NAU7802 `byte_offset`: Must be 0-22 (allows 10 bytes: weight + raw + unit + status)
   - NAU7802 `gain`: Must be 0-7
   - NAU7802 `sample_rate`: Must be 0, 1, 2, 3, or 7
   - NAU7802 `channel`: Must be 0 or 1
   - NAU7802 `ldo_value`: Must be 0-7
   - NAU7802 `unit`: Must be 0, 1, or 2

6. **NVS Persistence**: Configuration changes are persisted to Non-Volatile Storage (NVS) and survive reboots.

7. **NAU7802 Calibration**: 
   - **Software Calibration (Tare/Known Weight):**
     - Calibration data (calibration factor and zero offset) is stored in NVS
     - Calibration persists across reboots
     - Known weight is converted to grams internally based on configured unit
     - Uses a fixed 10-sample average internally for accuracy
     - Formula: `weight = (reading - zero_offset) / calibration_factor`
   - **Hardware Calibration (AFE):**
     - Calibrates the chip's Analog Front End hardware
     - Required when gain or sample rate changes
     - Automatically performed on boot when gain/sample rate changes are detected
     - Can be manually triggered via `action: "afe"`
     - Stores values in device hardware registers
     - Different from software weight calibration

8. **NAU7802 Assembly Data Format**:
   - Weight is stored as scaled integer (value × 100) to avoid floating-point
   - Example: 100.24 lbs = 10024 in assembly
   - Unit code (0=grams, 1=lbs, 2=kg) is stored at `byte_offset + 8`
   - Status flags are packed into a single byte at `byte_offset + 9`
   - Total data size: 10 bytes (4 weight + 4 raw + 1 unit + 1 status)

---

## Example Usage

### Using curl

```bash
# Get assembly status with parsed sensor data
curl http://172.16.82.99/api/status

# Get NAU7802 configuration and readings
curl http://172.16.82.99/api/nau7802

# Configure NAU7802
curl -X POST http://172.16.82.99/api/nau7802 \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "byte_offset": 0, "unit": 1, "gain": 7, "sample_rate": 3}'

# Perform tare calibration
curl -X POST http://172.16.82.99/api/nau7802/calibrate \
  -H "Content-Type: application/json" \
  -d '{"action": "tare", "samples": 10}'

# Perform known-weight calibration (100 lbs)
curl -X POST http://172.16.82.99/api/nau7802/calibrate \
  -H "Content-Type: application/json" \
  -d '{"action": "calibrate", "known_weight": 100.0}'

# Perform AFE (Analog Front End) calibration
curl -X POST http://172.16.82.99/api/nau7802/calibrate \
  -H "Content-Type: application/json" \
  -d '{"action": "afe"}'

# Set IP configuration
curl -X POST http://172.16.82.99/api/ipconfig \
  -H "Content-Type: application/json" \
  -d '{"use_dhcp": false, "ip_address": "192.168.1.100", "netmask": "255.255.255.0"}'

# Upload firmware
curl -X POST http://172.16.82.99/api/ota/update \
  -F "file=@firmware.bin"
```

### Using JavaScript (fetch)

```javascript
// Get assembly status with parsed sensor data
fetch('/api/status')
  .then(r => r.json())
  .then(data => {
    console.log('NAU7802 weight:', data.input_assembly_100.nau7802_data?.weight);
    console.log('Raw reading:', data.input_assembly_100.nau7802_data?.raw_reading);
  });

// Get NAU7802 configuration and readings
fetch('/api/nau7802')
  .then(r => r.json())
  .then(data => {
    console.log('Weight:', data.weight, data.unit);
    console.log('Gain:', data.gain_label);
    console.log('Sample Rate:', data.sample_rate_label);
  });

// Configure NAU7802
fetch('/api/nau7802', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    enabled: true,
    byte_offset: 0,
    unit: 1,  // lbs
    gain: 7,  // x128
    sample_rate: 3  // 80 SPS
  })
})
  .then(r => r.json())
  .then(data => console.log(data.message));

// Perform tare calibration
fetch('/api/nau7802/calibrate', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ action: 'tare' })
})
  .then(r => r.json())
  .then(data => console.log('Tare completed:', data.zero_offset));

// Perform known-weight calibration
fetch('/api/nau7802/calibrate', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    action: 'calibrate',
    known_weight: 100.0  // Weight in currently selected unit
  })
})
  .then(r => r.json())
  .then(data => console.log('Calibration factor:', data.calibration_factor));

// Perform AFE (Analog Front End) calibration
fetch('/api/nau7802/calibrate', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ action: 'afe' })
})
  .then(r => r.json())
  .then(data => console.log('AFE calibration:', data.message));
```

