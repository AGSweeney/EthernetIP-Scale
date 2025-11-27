# NAU7802 API Enhancements - Recommended Additions

## Current API Status

### Currently Exposed (GET /api/nau7802)
- ✅ `enabled` - Enable/disable NAU7802
- ✅ `byte_offset` - Assembly data byte offset (0-22)
- ✅ `unit` - Weight unit (0=grams, 1=lbs, 2=kg)
- ✅ `gain` - PGA gain setting (0-7: x1-x128)
- ✅ `sample_rate` - Sample rate (0,1,2,3,7: 10,20,40,80,320 SPS)
- ✅ `channel` - Active channel (0=Channel 1, 1=Channel 2)
- ✅ `ldo_value` - LDO voltage setting (0-7: 2.4V-4.5V)
- ✅ `average` - Reading average samples (1-50) for regular weight readings
- ✅ `initialized` - Device initialization status
- ✅ `connected` - Device connection status
- ✅ `weight` - Current weight reading (in selected unit)
- ✅ `raw_reading` - Raw 24-bit ADC reading
- ✅ `calibration_factor` - Current calibration factor
- ✅ `zero_offset` - Current zero offset (tare value)
- ✅ `revision_code` - Device revision code
- ✅ `status` - Detailed status flags (power, calibration, oscillator, AVDD)
- ✅ `channel1` / `channel2` - Channel calibration registers

### Currently Exposed (POST /api/nau7802)
- ✅ `enabled` - Enable/disable NAU7802
- ✅ `byte_offset` - Set assembly data byte offset
- ✅ `unit` - Set weight unit preference
- ✅ `gain` - Set PGA gain (requires reboot and AFE recalibration)
- ✅ `sample_rate` - Set sample rate (requires reboot and AFE recalibration)
- ✅ `channel` - Set active channel (requires reboot)
- ✅ `ldo_value` - Set LDO voltage (requires reboot)
- ✅ `average` - Set reading average samples (1-50, takes effect immediately)

### Currently Exposed (POST /api/nau7802/calibrate)
- ✅ `action: "tare"` - Perform zero offset calibration (uses fixed 10-sample average internally)
- ✅ `action: "calibrate"` - Perform known-weight calibration (uses fixed 10-sample average internally)
- ✅ `action: "afe"` - Perform AFE (Analog Front End) hardware calibration
- ✅ `known_weight` - Known weight value (in selected unit, for "calibrate" action)

---

## Recommended Additions

### 1. **Gain Setting** (HIGH PRIORITY)
**Why:** Critical for different load cell sensitivities. Affects measurement range and resolution.

**Current Default:** 128x (set in `nau7802_begin()`)

**Options:**
- `NAU7802_GAIN_1` (x1)
- `NAU7802_GAIN_2` (x2)
- `NAU7802_GAIN_4` (x4)
- `NAU7802_GAIN_8` (x8)
- `NAU7802_GAIN_16` (x16)
- `NAU7802_GAIN_32` (x32)
- `NAU7802_GAIN_64` (x64)
- `NAU7802_GAIN_128` (x128)

**Implementation:**
- Store in NVS (new key: `NVS_KEY_NAU7802_GAIN`)
- Add to GET response: `"gain": 7` (0-7 enum value)
- Add to POST request: `"gain": 7`
- Apply during initialization or via API
- **Note:** Changing gain requires AFE recalibration

**API Changes:**
```json
GET /api/nau7802:
{
  "gain": 7,
  "gain_label": "x128"
}

POST /api/nau7802:
{
  "gain": 5  // Set to x32
}
```

---

### 2. **Sample Rate** (MEDIUM PRIORITY)
**Why:** Affects update rate and noise characteristics. Lower rates = more stable, higher rates = faster updates.

**Current Default:** 80 SPS (set in `nau7802_begin()`)

**Options:**
- `NAU7802_SPS_10` (10 samples/second)
- `NAU7802_SPS_20` (20 samples/second)
- `NAU7802_SPS_40` (40 samples/second)
- `NAU7802_SPS_80` (80 samples/second)
- `NAU7802_SPS_320` (320 samples/second)

**Implementation:**
- Store in NVS (new key: `NVS_KEY_NAU7802_SAMPLE_RATE`)
- Add to GET response: `"sample_rate": 3` (enum value)
- Add to POST request: `"sample_rate": 3`
- Apply during initialization or via API
- **Note:** Changing sample rate requires AFE recalibration

**API Changes:**
```json
GET /api/nau7802:
{
  "sample_rate": 3,
  "sample_rate_label": "80 SPS"
}

POST /api/nau7802:
{
  "sample_rate": 0  // Set to 10 SPS
}
```

---

### 3. **Channel Selection** (MEDIUM PRIORITY)
**Why:** NAU7802 supports dual-channel operation. Useful if using two load cells.

**Current Default:** Channel 1 (set implicitly in `nau7802_begin()`)

**Options:**
- `NAU7802_CHANNEL_1` (Channel 1)
- `NAU7802_CHANNEL_2` (Channel 2)

**Implementation:**
- Store in NVS (new key: `NVS_KEY_NAU7802_CHANNEL`)
- Add to GET response: `"channel": 0` (0=Channel 1, 1=Channel 2)
- Add to POST request: `"channel": 0`
- Apply during initialization or via API
- **Note:** Each channel has separate calibration registers

**API Changes:**
```json
GET /api/nau7802:
{
  "channel": 0,
  "channel_label": "Channel 1"
}

POST /api/nau7802:
{
  "channel": 1  // Switch to Channel 2
}
```

---

### 4. **Revision Code** (LOW PRIORITY - Read-Only Status)
**Why:** Device identification and troubleshooting.

**Implementation:**
- Read-only status field
- Use `nau7802_get_revision_code()` function
- Add to GET response only

**API Changes:**
```json
GET /api/nau7802:
{
  "revision_code": 15  // Typically 0x0F
}
```

---

### 5. **LDO Voltage** (LOW PRIORITY)
**Why:** Power supply configuration. Rarely changed after initial setup.

**Current Default:** 3.3V (LDO value 4, set in `nau7802_begin()`)

**Options:**
- `0b000` = 4.5V
- `0b001` = 4.2V
- `0b010` = 3.9V
- `0b011` = 3.6V
- `0b100` = 3.3V (recommended for Qwiic)
- `0b101` = 3.0V
- `0b110` = 2.7V
- `0b111` = 2.4V

**Implementation:**
- Store in NVS (new key: `NVS_KEY_NAU7802_LDO`)
- Add to GET response: `"ldo_voltage": 3.3` (calculated from LDO value)
- Add to POST request: `"ldo_value": 4` (0-7)
- Apply during initialization only (requires power cycle)

**API Changes:**
```json
GET /api/nau7802:
{
  "ldo_value": 4,
  "ldo_voltage": 3.3
}

POST /api/nau7802:
{
  "ldo_value": 4  // Set to 3.3V (requires reboot)
}
```

---

### 6. **Additional Status Flags** (MEDIUM PRIORITY)
**Why:** Better diagnostics and monitoring.

**Status Bits Available:**
- Power status (PUD, PUA, PUR bits)
- Calibration status (CALS, CAL_ERROR bits)
- Data ready status (CR bit) - already exposed via `available`
- Oscillator status (OSCS bit)
- AVDD status (AVDDS bit)

**Implementation:**
- Add to GET response as a status object
- Read from `NAU7802_REGISTER_PU_CTRL` and `NAU7802_REGISTER_CTRL2`

**API Changes:**
```json
GET /api/nau7802:
{
  "status": {
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

---

### 7. **Channel Calibration Registers** (LOW PRIORITY - Advanced)
**Why:** For external calibration mode users who want to see stored register values.

**Implementation:**
- Read-only fields
- Use `nau7802_get_channel1_offset()`, `nau7802_get_channel1_gain()`, etc.

**API Changes:**
```json
GET /api/nau7802:
{
  "channel1": {
    "offset": 12345,
    "gain": 1234567
  },
  "channel2": {
    "offset": 0,
    "gain": 0
  }
}
```

---

## Implementation Priority

### Phase 1 (High Priority) - ✅ COMPLETED
1. ✅ **Gain Setting** - Critical for different load cell types
2. ✅ **Sample Rate** - Important for performance tuning

### Phase 2 (Medium Priority) - ✅ COMPLETED
3. ✅ **Channel Selection** - If dual-channel support needed
4. ✅ **Additional Status Flags** - Better diagnostics

### Phase 3 (Low Priority) - ✅ COMPLETED
5. ✅ **Revision Code** - Nice to have for troubleshooting
6. ✅ **LDO Voltage** - Rarely changed, less critical
7. ✅ **Channel Calibration Registers** - Advanced users only

### Additional Features Implemented
8. ✅ **Reading Average Setting** - Configurable averaging (1-50 samples) for regular weight readings, separate from calibration averaging
   - Stored in NVS as `NVS_KEY_NAU7802_AVERAGE`
   - Takes effect immediately (no reboot required)
   - Used by scale task for all regular readings
   - Default: 1 (no averaging)

---

## Notes

### Important Considerations

1. **AFE Recalibration Required:**
   - Changing **gain** requires AFE recalibration (automatically performed on boot)
   - Changing **sample rate** requires AFE recalibration (automatically performed on boot)
   - Changing **channel** uses separate calibration registers
   - Manual AFE calibration available via `action: "afe"` in calibration endpoint

2. **NVS Storage:**
   - All configuration persists in NVS
   - Settings loaded during initialization
   - Settings applied before calibration

3. **Web UI Updates:**
   - ✅ Dropdowns for gain (x1-x128)
   - ✅ Dropdown for sample rate (10-320 SPS)
   - ✅ Channel selector (Channel 1/2)
   - ✅ Status display section with detailed flags
   - ✅ Reading average setting (1-50 samples)
   - ✅ Warnings when settings require recalibration
   - ✅ Dedicated NAU7802 configuration page (`/nau7802`)

4. **Backward Compatibility:**
   - Default values match `nau7802_begin()` defaults
   - If NVS keys don't exist, defaults are used
   - Existing functionality preserved

5. **Calibration Averaging:**
   - Calibration (tare and known-weight) uses a fixed 10-sample average internally
   - This is separate from the `average` setting used for regular readings
   - The `average` setting (1-50 samples) only affects regular weight readings in the scale task

---

## Example Enhanced API Response

```json
{
  "enabled": true,
  "byte_offset": 0,
  "unit": 1,
  "unit_label": "lbs",
  "initialized": true,
  "connected": true,
  "weight": 100.24,
  "raw_reading": 1234567,
  "calibration_factor": 1234.56,
  "zero_offset": 12345,
  "gain": 7,
  "gain_label": "x128",
  "sample_rate": 3,
  "sample_rate_label": "80 SPS",
  "channel": 0,
  "channel_label": "Channel 1",
  "revision_code": 15,
  "ldo_value": 4,
  "ldo_voltage": 3.3,
  "status": {
    "available": true,
    "power_digital": true,
    "power_analog": true,
    "power_regulator": true,
    "calibration_active": false,
    "calibration_error": false,
    "oscillator_ready": true,
    "avdd_ready": true
  },
  "channel1": {
    "offset": 12345,
    "gain": 1234567
  }
}
```

