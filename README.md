# ENIP Scale

A comprehensive EtherNet/IP communication adapter for the ESP32-P4 microcontroller, integrating sensor support, Modbus TCP, and web-based configuration.

## Overview

This project implements a full-featured EtherNet/IP adapter device on the ESP32-P4 platform using the OpENer open-source EtherNet/IP stack. The device serves as a bridge between EtherNet/IP networks and local I/O, sensors, and other industrial automation components.

### Key Features

- **EtherNet/IP Adapter**: Full OpENer stack implementation with I/O connections
  - Input Assembly 100 (32 bytes)
  - Output Assembly 150 (32 bytes)
  - Configuration Assembly 151 (10 bytes)
  - Support for Exclusive Owner, Input Only, and Listen Only connections

- **Modbus TCP Server**: Standard Modbus TCP/IP server (port 502)
  - Input Registers 0-15 map to Input Assembly 100
  - Holding Registers 100-115 map to Output Assembly 150

- **NAU7802 Scale Integration**: 24-bit precision load cell amplifier
  - Configurable gain (x1-x128), sample rate (10-320 SPS), channel selection
  - Unit selection (grams, lbs, kg) with scaled integer storage
  - Reading averaging (1-50 samples) for stable measurements
  - Software calibration (tare, known-weight) and hardware AFE calibration
  - Real-time weight data mapped to EtherNet/IP Assembly 100
  - Web-based configuration page and REST API

- **Web-Based Configuration Interface**: Essential device management
  - Network configuration (DHCP/Static IP)
  - Firmware updates via OTA
  - NAU7802 scale configuration and monitoring
  - All other configuration and monitoring available via REST API

- **OTA Firmware Updates**: Over-the-air firmware update capability
  - File upload via web interface
  - URL-based downloads
  - Automatic rollback on failure


- **RFC 5227 Compliant Network Configuration**: Address Conflict Detection (ACD)
  - RFC 5227 compliant static IP assignment (implemented in application layer)
  - ACD probe sequence runs **before** IP assignment (deferred assignment)
  - Natural ACD state machine flow (PROBE_WAIT → PROBING → ANNOUNCE_WAIT → ANNOUNCING → ONGOING)
  - Probe sequence: 3 probes from `0.0.0.0` + 4 announcements (~6-10 seconds total)
  - IP assigned only after ACD confirms no conflict (ACD_IP_OK callback)
  - Callback tracking mechanism prevents false positive conflict detection on timeout
  - Configurable ACD timing parameters (probe intervals, announcement intervals, defensive ARP intervals)
  - Active IP defense with periodic ARP probes from `0.0.0.0` (matching Rockwell PLC behavior)
  - ACD retry logic with configurable delay and max attempts
  - User LED indication (GPIO27): blinks during normal operation, solid on ACD conflict
  - **EtherNet/IP Conflict Reporting**: Automatic capture and storage of conflict data in TCP/IP Interface Object Attribute #11
  - Custom lwIP modifications for EtherNet/IP requirements

## Hardware Requirements

- **Microcontroller**: ESP32-P4
- **Ethernet PHY**: IP101 (or compatible)
- **GPIO Configuration**:
  - Ethernet: MDC, MDIO, PHY Reset (configurable)
  - I2C: SDA, SCL (configurable, defaults GPIO7/GPIO8)
- **Sensors**:
  - NAU7802: 24-bit ADC load cell amplifier (I2C, address 0x2A)

## Software Requirements

- **ESP-IDF**: v5.5.1 or compatible
- **Python**: 3.x (for build scripts)
- **CMake**: 3.16 or higher

## Project Structure

```
ENIP_Scale/
├── main/                    # Main application code
├── components/              # Custom components
│   ├── opener/             # OpENer EtherNet/IP stack
│   ├── modbus_tcp/         # Modbus TCP server
│   ├── webui/              # Web interface
│   ├── ota_manager/        # OTA update manager
│   ├── system_config/      # System configuration
│   └── log_buffer/         # Log buffer component
├── eds/                     # EtherNet/IP EDS file
├── docs/                    # Documentation
│   ├── ASSEMBLY_DATA_LAYOUT.md  # Byte-by-byte assembly layout
│   ├── API_Endpoints.md         # Web API documentation
│   ├── WIRESHARK_FILTERS.md     # Wireshark filters for debugging
│   └── ExportSniff.md           # Example packet capture analysis
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
- **Static IP Mode**: Manual configuration with Address Conflict Detection (ACD)
  - IP address, netmask, gateway
  - Primary and secondary DNS servers
  - Hostname configuration

Configuration can be done via:
- Web interface: `http://<device-ip>/`
- EtherNet/IP CIP services
- NVS (Non-Volatile Storage)

### EtherNet/IP Configuration

The device exposes three assembly instances:

- **Assembly 100 (Input)**: 32 bytes of input data
  - Available space for sensor data and I/O

- **Assembly 150 (Output)**: 32 bytes of output data
  - Control commands and output data (all 32 bytes available)

- **Assembly 151 (Configuration)**: 10 bytes for configuration

Connection types supported:
- **Exclusive Owner**: Bidirectional I/O connection
- **Input Only**: Unidirectional input connection
- **Listen Only**: Unidirectional input connection (multicast)

**For detailed byte-by-byte assembly data layout, see [docs/ASSEMBLY_DATA_LAYOUT.md](docs/ASSEMBLY_DATA_LAYOUT.md).**

## Web Interface

Access the web interface at `http://<device-ip>/` after the device has obtained an IP address.

### Features

The web interface provides essential device management capabilities:

- **Network Configuration**: Set IP address, netmask, gateway, DNS (DHCP/Static IP modes)
- **Firmware Updates**: Upload firmware updates via web browser (OTA)

**Note:** All other device configuration, monitoring, and status information is available via the REST API. The web interface is intentionally minimal to keep it lightweight and focused on essential functions.

For detailed API documentation covering sensor configuration, assembly monitoring, Modbus TCP control, system logs, and more, see [docs/API_Endpoints.md](docs/API_Endpoints.md).

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
- **Web Log Buffer**: 32KB circular buffer accessible via web interface
- **Log Levels**: Configurable via menuconfig
- **Wireshark Filters**: See `docs/WIRESHARK_FILTERS.md` for ACD debugging filters
- **Testing Tools**: See `tools/README.md` for ACD conflict simulators and network analysis tools

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

## References

- [OpENer Documentation](https://github.com/EIPStackGroup/OpENer)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [EtherNet/IP Specification](https://www.odva.org/)
- [Modbus TCP/IP Specification](https://modbus.org/specs.php)
- [RFC 5227 - IPv4 Address Conflict Detection](https://tools.ietf.org/html/rfc5227)

## Documentation

- **[ACD and EtherNet/IP Conflict Reporting](docs/ACD_CONFLICT_REPORTING.md)** - Complete guide to ACD conflict detection and EtherNet/IP integration
- **[Assembly Data Layout](docs/ASSEMBLY_DATA_LAYOUT.md)** - Byte-by-byte assembly data documentation
- **[API Endpoints](docs/API_Endpoints.md)** - Web UI REST API documentation
- **[Wireshark Filters](docs/WIRESHARK_FILTERS.md)** - Network debugging filters for ACD

## Support

For issues and questions:
- Check the documentation in `docs/`
- Review serial logs for error messages
- Consult OpENer and ESP-IDF documentation

---

**Device Name**: ENIP-Scale  
**Vendor**: AG Sweeney 

**Firmware Version**: See git commit or build timestamp

