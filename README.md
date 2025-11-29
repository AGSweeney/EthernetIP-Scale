# EthernetIP-Scale

A comprehensive EtherNet/IP communication adapter for the ESP32-P4 microcontroller, integrating NAU7802 precision scale support, Modbus TCP, and web-based configuration.

## Overview

This project implements a full-featured EtherNet/IP adapter device on the ESP32-P4 platform using the OpENer open-source EtherNet/IP stack. The device serves as a bridge between EtherNet/IP networks and industrial scale applications, providing real-time weight data via EtherNet/IP assemblies and Modbus TCP.

**Tested Hardware Configuration:**
- [Waveshare ESP32-P4-ETH](https://www.waveshare.com/product/arduino/boards-kits/esp32-p4/esp32-p4-eth.htm) development board
- [SparkFun Qwiic Scale - NAU7802](https://www.sparkfun.com/products/15242) precision scale module
- [S-Type Beam High-Precision Load Cell](https://www.amazon.com/dp/B077YHLZGQ) (4-wire strain gauge load cell)

### Key Features

- **EtherNet/IP Adapter**: Full OpENer stack implementation with I/O connections
  - Input Assembly 100 (32 bytes)
  - Output Assembly 150 (32 bytes)
  - Configuration Assembly 151 (10 bytes)
  - Support for Exclusive Owner, Input Only, and Listen Only connections

- **CIP File Object (Class 0x37)**: Embedded EDS file serving for automatic device discovery
  - Instance 200: EDS file ("EDS.txt") - automatically downloadable by RSLinx and Others
  - Instance 201: Icon file ("EDSCollection.gz") - device icon for configuration tools

- **LLDP (Link Layer Discovery Protocol) Support**: IEEE 802.1AB compliant neighbor discovery
  - **LLDP Support**: Full LLDP implementation for neighbor discovery on Ethernet networks
  - **CIP Objects**: LLDP Management Object (Class 0x109) and LLDP Data Table Object (Class 0x10A)
  - **Transmission**: Periodic LLDP frame transmission (configurable interval, default 30 seconds)
  - **Reception**: Automatic neighbor discovery and database management
  - **ESP-NETIF L2 TAP Integration**: Raw Ethernet frame access for LLDP frames (EtherType 0x88CC)
  - **Multicast MAC Filtering**: Hardware-level filtering for LLDP multicast address (01:80:c2:00:00:0e)
  - **Persistent Configuration**: LLDP settings stored in NVS and configurable via CIP services
  - **Neighbor Information**: System name, description, capabilities, management IP, and TTL tracking

- **RFC 5227 Compliant Network Configuration**: Address Conflict Detection (ACD)
  - RFC 5227 compliant static IP assignment (implemented in application layer)
  - **ACD Control via Attribute #10**: ACD can be enabled/disabled via EtherNet/IP TCP/IP Interface Object Attribute #10 (`select_acd`)
  - **Persistent Setting**: ACD setting persists across reboots (stored in NVS)
  - **Applies to Both Static IP and DHCP**: ACD setting controls conflict detection for both configuration methods
  - ACD probe sequence runs **before** IP assignment (deferred assignment) when enabled
  - Natural ACD state machine flow (PROBE_WAIT → PROBING → ANNOUNCE_WAIT → ANNOUNCING → ONGOING)
  - Probe sequence: 3 probes from `0.0.0.0` + 4 announcements (~6-10 seconds total)
  - IP assigned only after ACD confirms no conflict (ACD_IP_OK callback) when enabled
  - Callback tracking mechanism prevents false positive conflict detection on timeout
  - Configurable ACD timing parameters (probe intervals, announcement intervals, defensive ARP intervals)
  - Active IP defense with periodic ARP probes from `0.0.0.0` (matching Rockwell PLC behavior)
  - ACD retry logic with configurable delay and max attempts
  - User LED indication (GPIO27): blinks during normal operation, solid on ACD conflict
  - **EtherNet/IP Conflict Reporting**: Automatic capture and storage of conflict data in TCP/IP Interface Object Attribute #11
  - Custom lwIP modifications for EtherNet/IP requirements

- **Modbus TCP Server**: Standard Modbus TCP/IP server (port 502)
  - Input Registers 0-15 map to Input Assembly 100
  - Holding Registers 100-115 map to Output Assembly 150

- **NAU7802 Scale Integration**: 24-bit precision load cell amplifier
  - Configurable gain (x1-x128), sample rate (10-320 SPS), channel selection (Channel 1/2)
  - Unit selection (grams, lbs, kg) with scaled integer storage (no floating point)
  - Reading averaging (1-50 samples) for stable measurements
  - Software calibration (tare, known-weight) and hardware AFE (Analog Front End) calibration
  - Real-time weight data mapped to EtherNet/IP Assembly 100 (configurable byte offset)
  - Status flags (available, connected, initialized) included in assembly data
  - Dedicated web-based configuration page (`/nau7802`) and REST API

- **Web-Based Configuration Interface**: Essential device management
  - Network configuration (DHCP/Static IP) - Main page (`/`)
  - Firmware updates via OTA - Firmware page (`/ota`)
  - NAU7802 scale configuration and monitoring - Scale page (`/nau7802`)
  - All other configuration and monitoring available via REST API

- **OTA Firmware Updates**: Over-the-air firmware update capability
  - File upload via web interface
  - URL-based downloads
  - Automatic rollback on failure

## Hardware

This project is designed and tested with the following hardware components:

### Development Board: Waveshare ESP32-P4-ETH

The [Waveshare ESP32-P4-ETH](https://www.waveshare.com/product/arduino/boards-kits/esp32-p4/esp32-p4-eth.htm) development board provides the core processing and Ethernet connectivity for this project.

**Key Features:**
- **Microcontroller**: ESP32-P4 dual-core RISC-V processor
- **Ethernet**: 100 Mbps RJ45 Ethernet port with IP101 PHY
- **Connectivity**: USB-C interface for programming and power
- **Expansion**: 2×20-pin headers for peripheral integration
- **GPIO**: Multiple configurable GPIO pins for I2C, SPI, UART, and other interfaces

**Ethernet Configuration:**
- PHY Address: 1 (default)
- MDC/MDIO pins: Configurable via menuconfig
- PHY Reset: Configurable via menuconfig

### Scale Module: SparkFun Qwiic Scale - NAU7802

The [SparkFun Qwiic Scale - NAU7802](https://www.sparkfun.com/products/15242) breakout board provides high-precision weight measurement capabilities.

**Key Features:**
- **ADC**: 24-bit dual-channel analog-to-digital converter
- **Interface**: I²C communication (Qwiic connector compatible)
- **I2C Address**: 0x2A (default, configurable)
- **Programmable Gain**: PGA with gains from ×1 to ×128
- **Sample Rates**: 10, 20, 40, 80, or 320 samples per second (SPS)
- **Channels**: Dual-channel support (Channel 1 and Channel 2)
- **Power Management**: Low power consumption with programmable LDO voltage (2.4V to 4.5V)
- **Calibration**: Hardware AFE (Analog Front End) calibration and software weight calibration

**Connection:**
- Connect to ESP32-P4-ETH via I²C bus
- Default I2C pins: GPIO7 (SDA), GPIO8 (SCL)
- Pull-up resistors: External pull-ups required (typically 4.7kΩ)
- Power: 3.3V from ESP32-P4-ETH

### Load Cell: S-Type Beam High-Precision Load Cell

The [S-Type Beam High-Precision Load Cell](https://www.amazon.com/dp/B077YHLZGQ) provides the physical weight measurement interface for the scale system.

**Key Features:**
- **Type**: S-type (S-beam) strain gauge load cell
- **Design**: Suitable for both tension and compression measurements
- **Protection**: IP67 rated for environmental protection
- **Accuracy**: High linearity, low hysteresis, excellent repeatability (±0.02% F.S.)
- **Output**: Analog differential signal (millivolt output)
- **Wiring**: 4-wire configuration (Excitation+, Excitation-, Signal+, Signal-)
- **Capacity**: Available in multiple weight capacities (select based on application requirements)

**Connection:**
- Connect directly to the SparkFun Qwiic Scale - NAU7802 module
- The NAU7802 module provides excitation voltage and reads the differential signal
- Ensure proper mechanical mounting for accurate measurements
- Follow manufacturer's installation guidelines for optimal performance

**Note:** Load cell capacity should be selected based on your application's maximum expected weight. The NAU7802's programmable gain (×1 to ×128) allows optimization for different load cell sensitivities.

### Hardware Requirements

- **Development Board**: [Waveshare ESP32-P4-ETH](https://www.waveshare.com/product/arduino/boards-kits/esp32-p4/esp32-p4-eth.htm) or compatible ESP32-P4 board with Ethernet
- **Ethernet PHY**: IP101 (included on Waveshare ESP32-P4-ETH)
- **Scale Module**: [SparkFun Qwiic Scale - NAU7802](https://www.sparkfun.com/products/15242) or compatible NAU7802 breakout
- **Load Cell**: [S-Type Beam High-Precision Load Cell](https://www.amazon.com/dp/B077YHLZGQ) or compatible 4-wire strain gauge load cell
- **GPIO Configuration**:
  - Ethernet: MDC, MDIO, PHY Reset (configurable, board-specific)
  - I2C: SDA, SCL (configurable, defaults GPIO7/GPIO8)
- **Additional Components**:
  - I2C pull-up resistors (4.7kΩ recommended)
  - Load cell with appropriate capacity for your application

## Software Requirements

- **ESP-IDF**: v5.5.1 or compatible
- **Python**: 3.x (for build scripts)
- **CMake**: 3.16 or higher

## Project Structure

```
ENIP_Scale/
├── main/                    # Main application code (ScaleApplication)
├── components/              # Custom components
│   ├── opener/             # OpENer EtherNet/IP stack
│   ├── modbus_tcp/         # Modbus TCP server
│   ├── webui/              # Web interface and REST API
│   ├── ota_manager/        # OTA update manager
│   ├── system_config/      # System configuration (NVS)
│   ├── nau7802/            # NAU7802 scale driver
│   ├── lldp/               # LLDP (Link Layer Discovery Protocol) component
│   └── log_buffer/         # Log buffer component
├── eds/                     # EtherNet/IP EDS file
├── docs/                    # Documentation
│   ├── ASSEMBLY_DATA_LAYOUT.md  # Byte-by-byte assembly layout
│   ├── API_Endpoints.md         # Web API documentation
│   ├── ACD_CONFLICT_REPORTING.md # ACD conflict detection guide
│   ├── FILE_OBJECT_INTEGRATION.md # CIP File Object implementation guide
│   └── OTA_UPLOAD_FIX.md        # OTA upload implementation notes
├── dependency_modifications/ # lwIP modifications
├── tools/                   # Testing and debugging tools
│   ├── test_acd_conflict.py     # ACD conflict simulator
│   ├── send_conflict_arp.py      # ARP conflict sender
│   ├── pdml_to_markdown.py      # Wireshark PDML converter
│   └── analyze_arp_timing.py    # ARP timing analyzer
├── scripts/                 # Build scripts
└── FirmwareImages/          # Compiled firmware binaries
```

## Building the Project

1. **Install ESP-IDF v5.5.1**:
   ```bash
   # Follow ESP-IDF installation guide
   # https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/
   ```

2. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd ENIP_Scale
   ```

3. **Configure the project**:
   ```bash
   idf.py menuconfig
   ```
   
   Key configuration options:
   - **Ethernet**: PHY address, GPIO pins (MDC, MDIO, Reset)
   - **I2C**: SDA/SCL GPIO pins, pull-up configuration
   - **ACD Timing**: RFC 5227 timing parameters

4. **Build the firmware**:
   ```bash
   idf.py build
   ```

5. **Flash the firmware**:
   ```bash
   idf.py flash
   ```

6. **Monitor serial output**:
   ```bash
   idf.py monitor
   ```

## Configuration

### Network Configuration

The device supports both DHCP and static IP configuration:

- **DHCP Mode**: Automatic IP assignment via DHCP server
  - ACD conflict detection can be enabled/disabled via Attribute #10 (`select_acd`)
  - If ACD enabled: Performs conflict check before binding offered IP address
  - If ACD disabled: Binds IP address immediately without conflict detection
- **Static IP Mode**: Manual configuration with optional Address Conflict Detection (ACD)
  - IP address, netmask, gateway
  - Primary and secondary DNS servers
  - Hostname configuration
  - ACD conflict detection can be enabled/disabled via Attribute #10 (`select_acd`)
  - If ACD enabled: Runs full RFC 5227 probe sequence before assigning IP
  - If ACD disabled: Assigns IP address immediately without conflict detection

**ACD Control (Attribute #10)**:
- **Class**: 0xF5 (TCP/IP Interface Object)
- **Attribute**: #10 (`select_acd`)
- **Type**: BOOL (1 = enabled, 0 = disabled)
- **Persistent**: Yes (saved to NVS, persists across reboots)
- **Default**: Enabled (1) for new devices

Configuration can be done via:
- Web interface: `http://<device-ip>/`
- EtherNet/IP CIP services (including Attribute #10 for ACD control)
- NVS (Non-Volatile Storage)

### EtherNet/IP Configuration

The device exposes three assembly instances:

- **Assembly 100 (Input)**: 32 bytes of input data
  - NAU7802 scale data (10 bytes): Weight (int32, scaled by 100), Raw ADC reading (int32), Unit code (uint8), Status flags (uint8)
  - Configurable byte offset (0-22) to avoid conflicts with other sensors
  - Remaining space available for additional sensor data and I/O

- **Assembly 150 (Output)**: 32 bytes of output data
  - Control commands and output data (all 32 bytes available)

- **Assembly 151 (Configuration)**: 10 bytes for configuration

Connection types supported:
- **Exclusive Owner**: Bidirectional I/O connection
- **Input Only**: Unidirectional input connection
- **Listen Only**: Unidirectional input connection (multicast)

**For detailed byte-by-byte assembly data layout, see [docs/ASSEMBLY_DATA_LAYOUT.md](docs/ASSEMBLY_DATA_LAYOUT.md).**

## LLDP (Link Layer Discovery Protocol)

This device implements full LLDP support for automatic neighbor discovery on Ethernet networks, allowing network management tools to discover and display the network topology.

### Overview

LLDP is an IEEE 802.1AB standard protocol that allows devices to advertise their identity, capabilities, and management information to directly connected neighbors on an Ethernet network. This implementation provides:

- **LLDP Frame Transmission**: Periodic transmission of LLDP frames containing device information
- **LLDP Frame Reception**: Automatic reception and processing of LLDP frames from neighbors
- **Neighbor Database**: Storage and management of discovered neighbor information
- **CIP Object Integration**: Full integration with EtherNet/IP CIP services for configuration and status

### CIP Objects

Two CIP objects provide LLDP functionality:

1. **LLDP Management Object (Class 0x109)**: Manages LLDP configuration and status
   - Attribute 1: `lldp_enable_array` - Enable/disable LLDP per Ethernet link
   - Attribute 2: `msg_tx_interval` - Transmission interval in seconds (1-65535)
   - Attribute 3: `msg_tx_hold` - Transmission hold multiplier (1-255)
   - Attribute 4: `lldp_datastore` - Data store identifier
   - Attribute 5: `last_change` - Timestamp of last configuration change

2. **LLDP Data Table Object (Class 0x10A)**: Stores discovered neighbor information
   - Dynamic instances created for each discovered neighbor
   - Contains neighbor MAC address, Chassis ID, Port ID, System Name/Description, Capabilities, Management IP, and TTL

### Implementation Details

**ESP-NETIF L2 TAP Integration**:
- Uses ESP-IDF's Layer 2 TAP interface to access raw Ethernet frames
- Frames are intercepted in `esp_netif_receive` before being passed to the IP stack
- LLDP frames (EtherType 0x88CC) are filtered and passed to the LLDP reception task

**Multicast MAC Filtering**:
- LLDP uses multicast MAC address `01:80:c2:00:00:0e` (Nearest Bridge)
- Ethernet MAC hardware filter configured via `ETH_CMD_ADD_MAC_FILTER` to accept LLDP frames
- Filter is added during Ethernet link-up event

**Frame Processing**:
- **Transmission**: FreeRTOS timer triggers periodic frame transmission (default 30 seconds)
- **Reception**: Dedicated task polls L2 TAP socket for incoming frames (100ms polling interval)
- **TLV Encoding/Decoding**: Full support for mandatory and optional TLVs:
  - Mandatory: Chassis ID, Port ID, TTL
  - Optional: System Name, System Description, System Capabilities, Management Address

**Configuration**:
- LLDP can be enabled/disabled via `OPENER_LLDP_ENABLED` in `opener_user_conf.h` (default: enabled)
- Transmission interval configurable via `OPENER_LLDP_TX_INTERVAL_MS` (default: 30000ms)
- Configuration persists in NVS and is restored on boot
- Runtime configuration available via CIP services on LLDP Management Object

### Platform-Specific Modifications

The following ESP-IDF components were modified to support LLDP:

1. **`components/esp_netif/lwip/esp_netif_lwip.c`**:
   - Added L2 TAP filter call in `esp_netif_receive` to intercept frames before IP stack processing

2. **`components/esp_netif/vfs_l2tap/esp_vfs_l2tap.c`**:
   - Reduced log verbosity for production use

3. **`main/main.c`**:
   - Added LLDP multicast MAC address to Ethernet MAC filter during link-up event

**For detailed component documentation, see [components/lldp/README.md](components/lldp/README.md).**

## Web Interface

Access the web interface at `http://<device-ip>/` after the device has obtained an IP address.

### Pages

The web interface provides three dedicated pages:

1. **Main Configuration Page (`/`)**: Network configuration
   - DHCP/Static IP mode selection
   - IP address, netmask, gateway configuration
   - DNS server configuration
   - All settings stored in NVS and applied on reboot

2. **Firmware Update Page (`/ota`)**: Over-the-air firmware updates
   - File upload for firmware binary
   - Progress indication
   - Automatic rollback on failure

3. **NAU7802 Scale Page (`/nau7802`)**: Scale configuration and monitoring
   - Basic configuration: Enable/disable, byte offset, unit selection
   - Device settings: Gain, sample rate, channel, LDO voltage, reading average
   - Status & readings: Real-time weight, raw ADC, calibration data, device status
   - Calibration: Tare (zero), known-weight calibration, AFE hardware calibration

**Note:** All other device configuration, monitoring, and status information is available via the REST API. For detailed API documentation covering assembly monitoring, Modbus TCP control, system logs, and more, see [docs/API_Endpoints.md](docs/API_Endpoints.md).

## Modbus TCP Mapping

The device provides a Modbus TCP server on port 502:

- **Input Registers** (0x04 function code):
  - 0-15: Maps to Input Assembly 100 (32 bytes = 16 registers)

- **Holding Registers** (0x03, 0x06, 0x10 function codes):
  - 100-115: Maps to Output Assembly 150 (32 bytes = 16 registers)
  - 150-154: Maps to Configuration Assembly 151 (10 bytes = 5 registers)

All assembly data is stored in little-endian format (Modbus converts to big-endian for transmission).

**For detailed register-to-byte mapping, see [docs/ASSEMBLY_DATA_LAYOUT.md](docs/ASSEMBLY_DATA_LAYOUT.md).**

## EtherNet/IP EDS File

The EDS (Electronic Data Sheet) file is located at `eds/ESP32P4_OPENER.eds`. This file can be imported into EtherNet/IP configuration tools to discover and configure the device.

Key device information:
- **Vendor**: AGSweeney Automation (Vendor Code: 55512)
- **Product**: ENIP-Scale (Product Code: 1)
- **Type**: General Purpose Discrete I/O

### CIP File Object Integration

The device implements the CIP File Object (Class 0x37) to serve the EDS file and icon directly from the device, eliminating the need for manual EDS file installation:

- **Automatic EDS Download**: RSLinx can download the EDS file from the device
- **Instance 200 (0xC8)**: Serves the EDS file as "EDS.txt" with Binary encoding
- **Instance 201 (0xC9)**: Serves the device icon as "EDSCollection.gz" with CompressedFile encoding
- **Embedded Files**: Both EDS and icon files are embedded in firmware at build time
- **Message Router Support**: Message Router instance #1 advertises supported CIP objects (including File Object) for EtherNet/IP Explorer discovery

**File Object Implementation**: This project uses the [OpENer File Object](https://github.com/EIPStackGroup/OpENerFileObject) implementation, which provides an open-source CIP File Object compatible with the OpENer EtherNet/IP stack.

## Custom lwIP Modifications

This project includes custom modifications to the lwIP network stack for RFC 5227 compliance and EtherNet/IP optimization. See [dependency_modifications/LWIP_MODIFICATIONS.md](dependency_modifications/LWIP_MODIFICATIONS.md) for details.

### Key Modifications

- RFC 5227 compliant static IP assignment (implemented in application layer)
- Configurable ACD timing parameters
- Increased socket and connection limits
- IRAM optimization for network performance
- Task affinity configuration

**Note**: These modifications are required for proper EtherNet/IP operation. They must be reapplied when upgrading ESP-IDF.

## OTA Firmware Updates

Firmware updates can be performed via:

1. **Web Interface**: Upload binary file directly
2. **REST API**: POST to `/api/ota/update` with file or URL
3. **EtherNet/IP**: Via CIP services (if implemented)

The device supports automatic rollback on failed updates and maintains two OTA partitions for safe updates.

## Logging and Debugging

- **Serial Logging**: Available via UART (default 115200 baud)
- **Web Log Buffer**: 32KB circular buffer accessible via REST API (`/api/logs`)
- **Log Levels**: Configurable via menuconfig
- **Testing Tools**: See `tools/` directory for ACD conflict simulators and network analysis tools

## Contributing

This project uses custom components and modified dependencies. When contributing:

1. Follow ESP-IDF coding conventions
2. Document any lwIP modifications clearly
3. Test with real EtherNet/IP controllers
4. Update EDS file if assembly structure changes

## License

### Project Code
This project's own code (main application, Web UI, Modbus TCP, OTA manager, system configuration, etc.) is licensed under the MIT License. See individual source files for copyright attribution.

### OpENer EtherNet/IP Stack
This project uses the OpENer EtherNet/IP stack, which is licensed under an adapted BSD-style license. The OpENer license file is included in `components/opener/LICENSE.txt` and must be preserved in all distributions.

All OpENer source files retain their original copyright notices:
- Copyright (c) 2009, Rockwell Automation, Inc.
- Modifications by Adam G. Sweeney <agsweeney@gmail.com> are clearly marked

### OpENer File Object
This project uses the [OpENer File Object](https://github.com/EIPStackGroup/OpENerFileObject) implementation, which is licensed under an adapted BSD-style license. The OpENer File Object license file is included in `components/opener/src/cip_objects/OpENerFileObject/license.txt` and must be preserved in all distributions.

### lwIP Network Stack
This project includes a modified version of lwIP from ESP-IDF v5.5.1. The lwIP modifications are clearly marked with attribution. Original lwIP license terms apply.

**Note**: EtherNet/IP™ is a trademark of ODVA, Inc.

## Troubleshooting

### Device Not Visible on Network

1. Check Ethernet cable connection
2. Verify PHY address configuration (default: 1)
3. Check serial logs for link status
4. Verify GPIO pin configuration

### EtherNet/IP Connection Fails

1. Verify IP address is not in conflict (check ACD status)
2. Check firewall rules (port 44818/TCP and UDP)
3. Verify EDS file is imported correctly
4. Check connection timeout settings

### OTA Update Fails

1. Verify partition table has OTA partitions
2. Check available flash space
3. Verify firmware binary is for correct ESP32-P4 variant
4. Check serial logs for OTA error messages

### LLDP Not Working

1. Verify LLDP is enabled in `opener_user_conf.h` (`OPENER_LLDP_ENABLED` must be 1)
2. Check that Ethernet link is up (LLDP only works on active Ethernet links)
3. Verify LLDP multicast MAC filter was added (check serial logs for "LLDP multicast address added to MAC filter")
4. Use Wireshark to verify LLDP frames are being transmitted (EtherType 0x88CC, destination 01:80:c2:00:00:0e)
5. Check LLDP Management Object Attribute 1 (`lldp_enable_array`) to ensure LLDP is enabled for the Ethernet link
6. Verify neighbor devices are also sending LLDP frames
7. Check LLDP Data Table Object instances to see discovered neighbors

## References

- [OpENer Documentation](https://github.com/EIPStackGroup/OpENer)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [EtherNet/IP Specification](https://www.odva.org/)
- [Modbus TCP/IP Specification](https://modbus.org/specs.php)
- [RFC 5227 - IPv4 Address Conflict Detection](https://tools.ietf.org/html/rfc5227)
- [IEEE 802.1AB - Link Layer Discovery Protocol (LLDP)](https://standards.ieee.org/ieee/802.1AB/3995/)

## Documentation

- **[Assembly Data Layout](docs/ASSEMBLY_DATA_LAYOUT.md)** - Byte-by-byte assembly data documentation
- **[API Endpoints](docs/API_Endpoints.md)** - Web UI REST API documentation
- **[ACD Conflict Reporting](docs/ACD_CONFLICT_REPORTING.md)** - Complete guide to ACD conflict detection and EtherNet/IP integration
- **[LLDP Component](components/lldp/README.md)** - LLDP (Link Layer Discovery Protocol) component documentation
- **[NAU7802 Driver](components/nau7802/README.md)** - NAU7802 scale driver documentation
- **[Web UI Component](components/webui/README.md)** - Web interface component documentation

## Support

For issues and questions:
- Check the documentation in `docs/`
- Review serial logs for error messages
- Consult OpENer and ESP-IDF documentation

---

**Device Name**: ENIP-Scale  
**Vendor**: Adam G. Sweeney 

**Firmware Version**: See git commit or build timestamp

