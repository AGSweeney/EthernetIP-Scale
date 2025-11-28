# Media Counters Implementation Plan

## Overview
Implement hardware-level media counters for the Ethernet Link object (Attribute #5) with IP101 PHY detection and support.

## Goals
1. Detect IP101 PHY on boot when Ethernet link comes up
2. Enable media counters if IP101 is detected
3. Fill with zeros if non-IP101 PHY is detected
4. Read real hardware statistics from ESP32 EMAC and IP101 PHY

## Implementation Steps

### Step 1: Create Media Counter Module
- **File**: `components/opener/src/ports/ESP32/eth_media_counters.h`
  - Public API for media counter support
  - Functions: `EthMediaCountersInit()`, `EthMediaCountersSupported()`, `EthMediaCountersCollect()`

- **File**: `components/opener/src/ports/ESP32/eth_media_counters.c`
  - IP101 PHY detection via PHY ID registers
  - ESP32 EMAC register access for MAC-level counters
  - IP101 MIB register access for PHY-level counters
  - Graceful fallback to zeros for non-IP101 PHYs
  - **Location**: In `ports/ESP32/` directory (platform infrastructure, not application-specific)

### Step 2: Update scaleapplication.c
- Include `eth_media_counters.h`
- Replace `ZeroMediaCounters(dst)` with `EthMediaCountersCollect(dst)` in `EthLnkPreGetCallback`

### Step 3: Update main.c
- Include `eth_media_counters.h`
- Store MAC pointer globally (`s_eth_mac`)
- Call `EthMediaCountersInit()` in `ethernet_event_handler` when `ETHERNET_EVENT_CONNECTED`

### Step 4: Update CMakeLists.txt
- Add `eth_media_counters.c` to SRCS list in `ports/ESP32/CMakeLists.txt`
- Add `eth_media_counters.c` to ESP32_PORT_SRCS in top-level `opener/CMakeLists.txt`
- Add `esp_eth` to REQUIRES in `ports/ESP32/CMakeLists.txt`

## Technical Details

### IP101 PHY Detection
- PHY ID Register 1 (0x02): OUI upper 16 bits
- PHY ID Register 2 (0x03): Model number lower 16 bits
- IP101 PHY ID: 0x02430C54
  - OUI: 0x0243
  - Model: 0x0C54

### ESP32 EMAC Counters (Always Available)
- `EMAC_RXALIGNERRFRAMES_REG` → `align_errs`
- `EMAC_RXCRCERRFRAMES_REG` → `fcs_errs`
- `EMAC_RXOVERSIZEDFRAMES_REG` → `frame_too_long`
- `EMAC_TXEXCESSDEF_REG` → `def_trans`
- `EMAC_TXCOLLISIONFRAMES_REG` → `mac_tx_errs`
- `EMAC_RXJABBERFRAMES_REG` + `EMAC_RXFRAGMENTS_REG` → `mac_rx_errs`

### IP101 PHY MIB Counters (IP101 Only)
- Register 0x16 → `single_coll` (16-bit)
- Register 0x18 → `multi_coll` (16-bit)
- Register 0x1A → `late_coll` (16-bit)
- Register 0x1C → `exc_coll` (16-bit)
- Register 0x1E → `crs_errs` (16-bit)
- Register 0x22 → `sqe_test_errs` (16-bit)

## Implementation Status

✅ **Completed:**
- Created `eth_media_counters.h` header file
- Created `eth_media_counters.c` implementation
- Updated `scaleapplication.c` to use `EthMediaCountersCollect()`
- Updated `main.c` to detect IP101 PHY on link up
- Updated both CMakeLists.txt files to include new source
- Added `esp_eth` dependency to ports/ESP32 CMakeLists.txt
- No linter errors

## Testing
- Verify compilation succeeds
- Check that IP101 detection works on boot (check logs)
- Verify media counters return real values for IP101
- Verify media counters return zeros for non-IP101 PHYs

## Future Enhancements
- Add support for other PHYs (LAN8720, DP83848, etc.)
- Add counter reset functionality
- Add error handling for register read failures
- Add unit tests for PHY detection
