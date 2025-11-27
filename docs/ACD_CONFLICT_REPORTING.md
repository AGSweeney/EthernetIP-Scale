# ACD and EtherNet/IP Conflict Reporting

This document explains how Address Conflict Detection (ACD) integrates with EtherNet/IP to report IP address conflicts for diagnostic purposes.

## Overview

The device implements RFC 5227 compliant Address Conflict Detection (ACD) for static IP addresses. When a conflict is detected, the device captures detailed information about the conflicting device and stores it in the EtherNet/IP TCP/IP Interface Object (Class 0xF5, Attribute #11) for diagnostic access.

### Quick Summary

**Boot Conflict (Probe Phase)**:
- Device boots with static IP → Sends probes → Conflict detected → **IP NOT assigned** → Retries

**Ongoing Conflict (After IP Assigned)**:
- Device has IP → First conflict detected → **Defends IP** (sends ARP announcement) → IP remains assigned
- Second conflict within 10 seconds → **Retreats** (removes IP) → Retries

## How It Works

### 1. Conflict Detection

When the device attempts to use a static IP address, it follows the RFC 5227 ACD sequence:

1. **Probe Phase**: Sends 3 ARP probes from `0.0.0.0` asking "Who has IP X?"
2. **Announce Phase**: If no conflicts detected, sends 2 ARP announcements claiming IP X
3. **Ongoing Phase**: Periodically sends defensive ARP probes to actively defend the IP

If another device responds to a probe or announcement, a conflict is detected.

### 2. Conflict Data Capture

When a conflict is detected, the ACD module (`components/lwip/lwip/src/core/ipv4/acd.c`) immediately captures:

- **Conflicting Device MAC Address**: The MAC address of the device that claimed the IP
- **Raw ARP Frame**: The complete 28-byte ARP frame that triggered the conflict

This data is captured at two points:

#### Probe Phase Conflicts
```c
// In acd_arp_reply(), when probe conflict detected:
CipTcpIpSetLastAcdMac(hdr->shwaddr.addr);
CipTcpIpSetLastAcdRawData((const uint8_t *)hdr, sizeof(struct etharp_hdr));
acd_restart(netif, acd);
```

#### Ongoing Phase Conflicts
```c
// In acd_arp_reply(), when ongoing conflict detected:
CipTcpIpSetLastAcdMac(hdr->shwaddr.addr);
CipTcpIpSetLastAcdRawData((const uint8_t *)hdr, sizeof(struct etharp_hdr));
acd_handle_arp_conflict(netif, acd);
```

### 3. Activity Status Updates

The application callback (`main/main.c`, `tcpip_acd_conflict_callback`) updates the activity status:

- **Activity = 0**: NoConflictDetected — ACD disabled or no conflict detected
- **Activity = 1**: OngoingDetection — Ongoing phase: actively defending the IP with periodic defensive ARP probes (normal operation after IP assignment)
- **Activity = 2**: ProbeIpv4Address — Probe phase: sending initial ARP probes before claiming the IP
- **Activity = 3**: SemiActiveProbe/ConflictDetected — Conflict detected: another device is using the IP address

```c
case ACD_IP_OK:
    CipTcpIpSetLastAcdActivity(1);  // OngoingDetection - entering ongoing defense phase
    break;
case ACD_DECLINE:
case ACD_RESTART_CLIENT:
    CipTcpIpSetLastAcdActivity(3);  // Conflict detected
    break;
```

**Note**: The `ACD_IP_OK` callback fires **after** the announce phase completes, which means ACD has already naturally transitioned to `ACD_STATE_ONGOING` state. The application does not manually stop/restart ACD - it relies on the natural state machine transition from PROBE_WAIT → PROBING → ANNOUNCE_WAIT → ANNOUNCING → ONGOING. This ensures the probe sequence completes correctly without interference.

### 4. EtherNet/IP Attribute #11 Storage

The captured data is stored in OpENer's TCP/IP Interface Object:

**Storage Structure** (`components/opener/src/cip/ciptcpipinterface.c`):
```c
static struct {
    CipUsint activity;        // 1 byte: Conflict activity status
    CipUsint remote_mac[6];   // 6 bytes: MAC address of conflicting device
    CipUsint raw_data[28];    // 28 bytes: Raw ARP frame data
} s_tcpip_last_conflict = {0};
```

**Storage Functions**:
- `CipTcpIpSetLastAcdMac(const uint8_t mac[6])` - Stores conflicting MAC address
- `CipTcpIpSetLastAcdRawData(const uint8_t *data, size_t length)` - Stores ARP frame
- `CipTcpIpSetLastAcdActivity(uint8_t activity)` - Sets activity status

### 5. EtherNet/IP Access

The conflict data is accessible via EtherNet/IP CIP services:

**Class**: 0xF5 (TCP/IP Interface Object)  
**Attribute**: #11 (Last Conflict Detected)  
**Service**: Get_Attribute_Single (0x0E)

**Data Format** (35 bytes total):
- Byte 0: Activity status (0-3)
- Bytes 1-6: Remote MAC address (6 bytes)
- Bytes 7-34: Raw ARP frame data (28 bytes)

## Visual Indicators

### User LED (GPIO27)

- **Blinking**: Normal operation (no conflict)
- **Solid ON**: Conflict detected

The LED state is updated in `tcpip_acd_conflict_callback()`:
```c
case ACD_IP_OK:
    user_led_start_flash();  // Resume blinking
    break;
case ACD_DECLINE:
case ACD_RESTART_CLIENT:
    user_led_stop_flash();   // Stop blinking
    user_led_set(true);      // Turn solid ON
    break;
```

### Status Flags

The OpENer TCP/IP status flags are also updated:
- `kTcpipStatusAcdStatus`: ACD is active
- `kTcpipStatusAcdFault`: ACD conflict detected

## Conflict Handling Behavior

The device implements RFC 5227 option (b) for ongoing conflict detection, which allows defending the IP address on the first conflict but requires retreating if a second conflict occurs within the DEFEND_INTERVAL (10 seconds).

### ACD State Machine Flow

The ACD implementation uses the natural lwIP state machine without manual intervention:

1. **PROBE_WAIT**: Initial random delay before probing
2. **PROBING**: Sends 3 ARP probes from `0.0.0.0` asking "Who has IP X?"
3. **ANNOUNCE_WAIT**: Waits before announcing (if no conflicts detected)
4. **ANNOUNCING**: Sends 2 ARP announcements claiming the IP
5. **ONGOING**: Periodically sends defensive ARP probes to actively defend the IP

The `ACD_IP_OK` callback fires **after** the announce phase completes (when transitioning to ONGOING state), ensuring the full probe sequence has completed successfully. The application does not manually stop/restart ACD - it relies on the natural state machine transition.

### Scenario 1: Boot Conflict (Probe Phase)

When the device boots with a static IP address and another device is already using it:

1. **Probe Phase**: Device sends 3 ARP probes from `0.0.0.0` asking "Who has IP X?" (**BEFORE IP assignment**)
2. **Conflict Detected**: Another device responds to the probe
3. **IP Assignment**: IP is **NOT assigned** (device never claims the address)
4. **Conflict Data**: MAC address and ARP frame are captured and stored
5. **Retry Logic** (if enabled):
   - Waits `CONFIG_OPENER_ACD_RETRY_DELAY_MS` (default: 10 seconds)
   - Retries the full ACD probe sequence
   - Retries up to `CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS` times (default: 5)
   - Each retry attempts to acquire the IP address again

**Result**: Device does not assign the conflicting IP and retries periodically to acquire it.

**Note**: The probe sequence (3 probes + 2-4 announcements) takes approximately 6-10 seconds to complete. The device uses a callback tracking mechanism (`s_acd_callback_received` flag) to distinguish between actual conflict callbacks and timeout conditions, preventing false positive conflict detection. IP assignment occurs when the `ACD_IP_OK` callback fires after the announce phase completes, ensuring the full probe sequence completes before IP assignment.

### Scenario 2: Ongoing Conflict (After IP Assigned)

When the device has successfully acquired an IP address and another device attempts to use it:

1. **First Conflict** (within DEFEND_INTERVAL):
   - Device detects another device claiming the same IP
   - **Defense Action**: Sends ARP announcement to assert ownership
   - **IP Status**: IP address **remains assigned** (device defends its address)
   - Conflict data is captured and stored
   - Sets `lastconflict` timer to track DEFEND_INTERVAL

2. **Second Conflict** (within DEFEND_INTERVAL, ~10 seconds after first):
   - Device detects another conflict while `lastconflict` timer is still active
   - **Retreat Action**: Calls `acd_restart()` which triggers conflict callback
   - **IP Removal**: IP address is **removed** (set to 0.0.0.0)
   - ACD monitoring is stopped
   - **Retry Logic** (if enabled):
     - Waits `CONFIG_OPENER_ACD_RETRY_DELAY_MS` (default: 10 seconds)
     - Retries the full ACD probe sequence
     - Retries up to `CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS` times (default: 5)

**Result**: Device defends the first conflict but retreats and retries if a second conflict occurs within 10 seconds.

### RFC 5227 Defense Strategy

The device uses **RFC 5227 option (b)**:
- **First conflict**: Defend by sending ARP announcement (keeps IP)
- **Second conflict within DEFEND_INTERVAL**: Retreat (removes IP and retries)
- This strategy helps improve network stability by allowing one of two conflicting hosts to retain its address while being flexible enough to help network performance

### DHCP Mode

- Uses simplified ACD (not fully RFC 5227 compliant)
- Conflict detection handled internally by lwIP DHCP client
- Conflict reporting still works if DHCP ACD detects a conflict

## Configuration

### ACD Retry Logic

If `CONFIG_OPENER_ACD_RETRY_ENABLED` is enabled:

- **On Probe Phase Conflict**: 
  - IP is never assigned
  - Waits `CONFIG_OPENER_ACD_RETRY_DELAY_MS` (default: 10 seconds)
  - Retries the full ACD probe sequence
  - Retries up to `CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS` times (default: 5)

- **On Ongoing Phase Conflict** (second conflict within DEFEND_INTERVAL):
  - IP address is removed (set to 0.0.0.0)
  - ACD monitoring is stopped
  - Waits `CONFIG_OPENER_ACD_RETRY_DELAY_MS` (default: 10 seconds)
  - Retries the full ACD probe sequence
  - Retries up to `CONFIG_OPENER_ACD_RETRY_MAX_ATTEMPTS` times (default: 5)

**Note**: The first conflict during ongoing phase is defended (IP remains assigned). Only the second conflict within DEFEND_INTERVAL triggers retry logic.

### Defensive ARP Interval

- Configurable via `CONFIG_OPENER_ACD_PERIODIC_DEFEND_INTERVAL_MS`
- Default: 90000ms (90 seconds)
- Matches typical Rockwell PLC behavior

## Accessing Conflict Data

### Via EtherNet/IP CIP

Use an EtherNet/IP configuration tool (e.g., Rockwell Studio 5000, Molex EtherNet/IP Tools):

1. Connect to device via EtherNet/IP
2. Browse to TCP/IP Interface Object (Class 0xF5)
3. Read Attribute #11 (Last Conflict Detected)
4. Parse the 35-byte response:
   - Activity status
   - Conflicting MAC address
   - Raw ARP frame

### Via Web API

Currently, conflict data is not exposed via the Web API. This could be added as a future enhancement.

### Via Serial Logs

Conflict detection is logged:
```
I (12345) opener_main: ACD callback received: state=2 (DECLINE)
W (12346) opener_main: ACD: Conflict detected (state=2) - LED set to solid
```

## Example: Reading Conflict Data

### Using Python with pycomm3 (EtherNet/IP)

```python
from pycomm3 import LogixDriver

with LogixDriver('172.16.82.100') as plc:
    # Read TCP/IP Interface Object, Attribute #11
    result = plc.generic_message(
        service=b'\x0e',  # Get_Attribute_Single
        class_code=b'\xf5\x00',  # TCP/IP Interface Object
        instance=b'\x01\x00',  # Instance 1
        attribute=b'\x0b\x00',  # Attribute #11
        data=b'',
        data_type='SINT',
        elements=35  # 35 bytes total
    )
    
    if result.error:
        print(f"Error: {result.error}")
    else:
        data = result.value
        activity = data[0]
        mac = ':'.join(f'{b:02x}' for b in data[1:7])
        arp_frame = data[7:35]
        
        print(f"Activity Status: {activity}")
        print(f"Conflicting MAC: {mac}")
        print(f"ARP Frame: {arp_frame.hex()}")
```

## Troubleshooting

### No Conflict Data in Attribute #11

- **Check Activity Status**: If activity = 0, no conflict has occurred
- **Verify ACD is Enabled**: Check `g_tcpip.select_acd` flag
- **Check Logs**: Look for "ACD: Conflict detected" messages

### Conflict Data Not Updating

- **Verify Implementation**: Check that `CipTcpIpSetLastAcdMac()` and `CipTcpIpSetLastAcdRawData()` are being called
- **Check Thread Safety**: Ensure ACD callback is executing properly
- **Verify Storage**: Check that `s_tcpip_last_conflict` structure is being updated

### LED Not Indicating Conflict

- **Check GPIO Configuration**: Verify GPIO27 is configured correctly
- **Check LED Task**: Ensure `user_led_flash_task` is running
- **Check Callback**: Verify `tcpip_acd_conflict_callback` is being called

## Implementation Details

### Files Involved

1. **`components/lwip/lwip/src/core/ipv4/acd.c`**:
   - Conflict detection logic
   - Calls `CipTcpIpSetLastAcdMac()` and `CipTcpIpSetLastAcdRawData()` at conflict points

2. **`main/main.c`**:
   - ACD callback handler (`tcpip_acd_conflict_callback`)
   - Updates activity status via `CipTcpIpSetLastAcdActivity()`
   - Controls user LED indication
   - **Callback Tracking**: Uses `s_acd_callback_received` flag to distinguish between actual callback events and timeout conditions, preventing false positive conflict detection
   - **Legacy Mode IP Assignment**: Assigns IP address in callback when `ACD_IP_OK` fires (after announce phase completes), ensuring probes complete before IP assignment

3. **`components/opener/src/cip/ciptcpipinterface.c`**:
   - Storage structure (`s_tcpip_last_conflict`)
   - Storage functions (`CipTcpIpSetLastAcdMac`, `CipTcpIpSetLastAcdRawData`, `CipTcpIpSetLastAcdActivity`)
   - Attribute #11 encoder (`EncodeCipLastConflictDetected`)

4. **`components/opener/src/cip/ciptcpipinterface.h`**:
   - Function declarations for storage functions

### Thread Safety

- ACD callbacks execute on the lwIP tcpip thread
- Storage functions are called synchronously during conflict detection
- No additional locking required (single-threaded execution context)

### Callback Tracking and Timeout Handling

The implementation uses a callback tracking mechanism to prevent false positive conflict detection:

- **`s_acd_callback_received` flag**: Tracks whether the ACD callback was actually received (not just initialized value)
- **Timeout vs Conflict**: When `tcpip_perform_acd()` times out (after 2000ms), it returns `true` to indicate "no conflict detected yet, waiting for callback". This is distinguished from returning `false` (which indicates an actual conflict callback was received).
- **IP Assignment Timing**: In legacy mode, IP assignment occurs when the `ACD_IP_OK` callback fires (after announce phase completes), not on timeout. This ensures the full probe sequence (3 probes + 2-4 announcements, ~6-10 seconds) completes before IP assignment.
- **False Positive Prevention**: The caller checks `s_acd_callback_received` before treating a timeout as a conflict, preventing premature IP assignment or false conflict detection.

## References

- **RFC 5227**: IPv4 Address Conflict Detection - https://tools.ietf.org/html/rfc5227
- **EtherNet/IP Specification**: TCP/IP Interface Object (Class 0xF5), Attribute #11
- **Implementation Details**: ARP frame storage implementation is documented in this document (see "Conflict Data Capture" and "EtherNet/IP Attribute #11 Storage" sections above)
- **Wireshark Filters**: See `docs/WIRESHARK_FILTERS.md` for debugging ACD conflicts

---

**Last Updated**: 2025  
**Status**: ✅ Implemented and Functional

