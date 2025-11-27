/**
 * @file doxygen_topics.h
 * @brief Doxygen topic pages for project components and APIs
 *
 * This file contains topic pages (@page) that appear in the Doxygen "Topics" section.
 * Each component and major feature has its own topic page for easy navigation.
 */

/**
 * @mainpage ENIP Scale Documentation
 *
 * @section intro_sec Introduction
 *
 * This project implements a full-featured EtherNet/IP adapter device on the ESP32-P4 platform
 * using the OpENer open-source EtherNet/IP stack. The device serves as a bridge between
 * EtherNet/IP networks and local I/O, sensors, and other industrial automation components.
 *
 * @section features_sec Key Features
 *
 * - **EtherNet/IP Adapter**: Full OpENer stack implementation with I/O connections
 * - **Modbus TCP Server**: Standard Modbus TCP/IP server (port 502)
 * - **Web-Based Configuration Interface**: Essential device management via web UI
 * - **OTA Firmware Updates**: Over-the-air firmware update capability
 * - **RFC 5227 Compliant Network Configuration**: Address Conflict Detection (ACD)
 *
 * @section components_sec Components
 *
 * - @ref component_modbus_tcp "Modbus TCP Server"
 * - @ref component_webui "Web UI Component"
 * - @ref component_web_api "Web API Documentation"
 * - @ref component_ota_manager "OTA Manager"
 * - @ref component_system_config "System Configuration"
 * - @ref component_log_buffer "Log Buffer"
 * - @ref component_nau7802 "NAU7802 Scale Component"
 *
 * @section api_sec APIs
 *
 * - @ref component_web_api "Web REST API"
 * - @ref component_opener_api "OpENer EtherNet/IP API"
 *
 * @section hardware_sec Hardware Requirements
 *
 * - **Microcontroller**: ESP32-P4
 * - **Ethernet PHY**: IP101 (or compatible)
 *
 * @section software_sec Software Requirements
 *
 * - **ESP-IDF**: v5.5.1 or compatible
 * - **Python**: 3.x (for build scripts)
 * - **CMake**: 3.16 or higher
 */

/**
 * @page component_modbus_tcp Modbus TCP Server
 *
 * @section modbus_overview Overview
 *
 * The Modbus TCP server component implements a standard Modbus TCP/IP server on port 502,
 * providing access to EtherNet/IP assembly data via Modbus registers.
 *
 * @section modbus_features Features
 *
 * - Standard Modbus TCP/IP protocol (port 502)
 * - Input Registers 0-15 map to Input Assembly 100
 * - Holding Registers 100-115 map to Output Assembly 150
 * - Thread-safe operation
 * - Multiple concurrent connections
 *
 * @section modbus_mapping Register Mapping
 *
 * - **Input Registers 0-15**: Maps to EtherNet/IP Input Assembly 100 (32 bytes)
 * - **Holding Registers 100-115**: Maps to EtherNet/IP Output Assembly 150 (32 bytes)
 *
 * @section modbus_api API Reference
 *
 * See @ref modbus_tcp.h for complete API documentation.
 *
 * - modbus_tcp_init() - Initialize the Modbus TCP server
 * - modbus_tcp_start() - Start the Modbus TCP server
 * - modbus_tcp_stop() - Stop the Modbus TCP server
 */

/**
 * @page component_webui Web UI Component
 *
 * @section webui_overview Overview
 *
 * The Web UI component provides a lightweight, responsive web interface accessible via HTTP
 * on port 80. It focuses on essential functions: network configuration and firmware updates.
 * All other device configuration, monitoring, and status information is available via the REST API.
 *
 * @section webui_features Features
 *
 * - **Network Configuration**: Configure DHCP/Static IP, netmask, gateway, and DNS settings
 * - **OTA Firmware Updates**: Upload and install firmware updates via web interface
 * - **REST API**: All sensor configuration, monitoring, and advanced features available via API endpoints
 * - **Responsive Design**: Works on desktop and mobile devices
 * - **No External Dependencies**: All CSS and JavaScript is self-contained (no CDN)
 *
 * @section webui_pages Web Pages
 *
 * ### Configuration Page (`/`)
 * The main configuration page provides essential device management:
 * - Network Configuration Card
 *   - DHCP/Static IP mode selection
 *   - IP address, netmask, gateway configuration
 *   - DNS server configuration (hidden when using DHCP)
 *   - All settings stored in OpENer's NVS
 *   - Reboot required to apply network changes
 *
 * ### Firmware Update Page (`/ota`)
 * Over-the-air firmware update interface:
 * - File upload for firmware binary
 * - Progress indication
 * - Auto-redirect to home page after successful update
 *
 * ### NAU7802 Scale Configuration Page (`/nau7802`)
 * NAU7802 scale configuration and monitoring interface:
 * - Basic Configuration: Enable/disable, byte offset, unit selection
 * - Device Settings: Gain, sample rate, channel, LDO voltage, reading average
 * - Status & Readings: Real-time weight, raw ADC, calibration data, status flags
 * - Calibration: Tare, known-weight, and AFE hardware calibration
 *
 * @section webui_api API Reference
 *
 * See @ref webui_api.h for complete API documentation.
 *
 * - webui_register_api_handlers() - Register all API endpoint handlers
 * - webui_get_index_html() - Get index HTML page
 * - webui_get_status_html() - Get status HTML page
 * - webui_get_ota_html() - Get OTA HTML page
 */

/**
 * @page component_web_api Web REST API
 *
 * @section webapi_overview Overview
 *
 * The Web REST API provides comprehensive access to device configuration, sensor data,
 * and system status via HTTP endpoints. All endpoints return JSON responses and are
 * accessible at `http://<device-ip>/api`.
 *
 * @section webapi_base Base URL
 *
 * All API endpoints are prefixed with `/api`. The base URL is typically `http://<device-ip>/api`.
 *
 * @section webapi_response Response Format
 *
 * All endpoints return JSON responses. Success responses typically include:
 * - `status`: "ok" or "error"
 * - `message`: Human-readable message
 * - Additional endpoint-specific fields
 *
 * Error responses include:
 * - `status`: "error"
 * - `message`: Error description
 *
 * HTTP status codes:
 * - `200 OK`: Success
 * - `400 Bad Request`: Invalid request parameters
 * - `500 Internal Server Error`: Server-side error
 * - `503 Service Unavailable`: Service not available
 *
 * @section webapi_endpoints Endpoints
 *
 * ### System Configuration
 *
 * - `GET /api/ipconfig` - Get current IP network configuration
 * - `POST /api/ipconfig` - Set IP network configuration (reboot required)
 * - `POST /api/reboot` - Reboot the device
 * - `GET /api/logs` - Get system logs from the log buffer
 *
 * ### EtherNet/IP Assemblies
 *
 * - `GET /api/assemblies` - Get EtherNet/IP assembly data
 * - `GET /api/assemblies/input` - Get Input Assembly 100 data
 * - `GET /api/assemblies/output` - Get Output Assembly 150 data
 *
 * ### NAU7802 Scale
 *
 * - `GET /api/nau7802` - Get NAU7802 configuration and readings
 * - `POST /api/nau7802` - Configure NAU7802 settings
 * - `POST /api/nau7802/calibrate` - Perform scale calibration
 *
 * ### Modbus TCP
 *
 * - `GET /api/modbus/status` - Get Modbus TCP server status
 * - `POST /api/modbus/enabled` - Enable/disable Modbus TCP server
 *
 * @section webapi_docs Complete Documentation
 *
 * For complete API documentation with request/response examples, see:
 * - @ref docs/API_Endpoints.md "API Endpoints Documentation"
 */

/**
 * @page component_ota_manager OTA Manager
 *
 * @section ota_overview Overview
 *
 * The OTA Manager component provides over-the-air firmware update capability for the ESP32-P4 device.
 * It supports both file upload via web interface and URL-based downloads.
 *
 * @section ota_features Features
 *
 * - File upload via web interface
 * - URL-based firmware downloads
 * - Automatic rollback on failure
 * - Progress tracking
 * - Partition management
 *
 * @section ota_usage Usage
 *
 * @code{.c}
 * #include "ota_manager.h"
 *
 * ota_config_t config = {
 *     .url = "http://example.com/firmware.bin",
 *     .timeout_ms = 30000
 * };
 *
 * esp_err_t err = ota_manager_start_update(&config);
 * @endcode
 *
 * @section ota_api API Reference
 *
 * See @ref ota_manager.h for complete API documentation.
 *
 * - ota_manager_init() - Initialize the OTA manager
 * - ota_manager_start_update() - Start an OTA update
 * - ota_manager_get_status() - Get current OTA status
 */

/**
 * @page component_system_config System Configuration
 *
 * @section sysconfig_overview Overview
 *
 * The System Configuration component provides centralized configuration management for
 * system-level settings including I2C configuration and MCP (Microchip I/O expander) settings.
 *
 * @section sysconfig_features Features
 *
 * - I2C bus configuration
 * - MCP I/O expander configuration
 * - NVS (Non-Volatile Storage) integration
 * - Persistent configuration storage
 *
 * @section sysconfig_api API Reference
 *
 * See @ref system_config.h, @ref i2c_config.h, and @ref mcp_config.h for complete API documentation.
 */

/**
 * @page component_log_buffer Log Buffer
 *
 * @section logbuffer_overview Overview
 *
 * The Log Buffer component provides a circular buffer for storing system logs, allowing
 * retrieval of recent log entries via the web API.
 *
 * @section logbuffer_features Features
 *
 * - Circular buffer implementation
 * - Configurable buffer size
 * - Thread-safe operation
 * - Web API integration for log retrieval
 *
 * @section logbuffer_api API Reference
 *
 * See @ref log_buffer.h for complete API documentation.
 *
 * - log_buffer_init() - Initialize the log buffer
 * - log_buffer_write() - Write a log entry
 * - log_buffer_read() - Read log entries
 */

/**
 * @page component_opener_api OpENer EtherNet/IP API
 *
 * @section opener_overview Overview
 *
 * OpENer is an open-source EtherNet/IP communication stack for adapter devices (connection target).
 * It supports multiple I/O and explicit connections and includes features required by the CIP
 * specification to enable devices to comply with ODVA's conformance/interoperability tests.
 *
 * @section opener_features Features
 *
 * - EtherNet/IP adapter implementation
 * - Multiple I/O connections support
 * - Explicit messaging support
 * - CIP object support
 * - Assembly objects for I/O data
 *
 * @section opener_api API Reference
 *
 * See @ref opener_api.h for complete API documentation.
 *
 * The OpENer API includes:
 * - Stack initialization and configuration
 * - CIP object creation and management
 * - Assembly object management
 * - Connection management
 * - Callback functions for platform-specific operations
 *
 * @section opener_docs Documentation
 *
 * For detailed OpENer documentation, see the OpENer main page in the generated documentation.
 */

/**
 * @page component_nau7802 NAU7802 Scale Component
 *
 * @section nau7802_overview Overview
 *
 * The NAU7802 component provides integration with the NAU7802 24-bit precision load cell amplifier.
 * It supports configurable gain, sample rate, channel selection, and provides both software and
 * hardware calibration capabilities.
 *
 * @section nau7802_features Features
 *
 * - **24-bit ADC**: High-precision weight measurements
 * - **Configurable Gain**: x1 to x128 PGA gain settings
 * - **Sample Rate**: 10, 20, 40, 80, or 320 samples per second
 * - **Dual Channel**: Support for Channel 1 and Channel 2
 * - **Unit Selection**: Grams, pounds (lbs), or kilograms (kg)
 * - **Reading Averaging**: Configurable averaging (1-50 samples) for stable readings
 * - **Calibration**: Software calibration (tare, known-weight) and hardware AFE calibration
 * - **Assembly Integration**: Weight data mapped to EtherNet/IP Assembly 100
 * - **Web Configuration**: Dedicated configuration page at `/nau7802`
 * - **REST API**: Complete API for configuration and monitoring
 *
 * @section nau7802_calibration Calibration
 *
 * The NAU7802 supports two types of calibration:
 *
 * ### Software Calibration (Weight Calibration)
 * - **Tare (Zero Offset)**: Sets the zero point when scale is empty
 * - **Known Weight**: Calculates calibration factor using a known weight
 * - Uses fixed 10-sample average internally for accuracy
 * - Calibration data stored in NVS
 *
 * ### Hardware Calibration (AFE Calibration)
 * - **AFE (Analog Front End)**: Calibrates the chip's internal hardware
 * - Required when gain or sample rate changes
 * - Automatically performed on boot when settings change
 * - Can be manually triggered via API
 *
 * @section nau7802_assembly Assembly Data Format
 *
 * NAU7802 data is stored in EtherNet/IP Assembly 100 at a configurable byte offset (0-22):
 *
 * - **Bytes 0-3**: Weight (int32, scaled by 100, e.g., 100.24 lbs = 10024)
 * - **Bytes 4-7**: Raw ADC reading (int32)
 * - **Byte 8**: Unit code (0=grams, 1=lbs, 2=kg)
 * - **Byte 9**: Status flags (available, connected, initialized)
 *
 * @section nau7802_api API Reference
 *
 * See @ref nau7802.h for complete driver API documentation.
 *
 * Web API endpoints:
 * - `GET /api/nau7802` - Get configuration and current readings
 * - `POST /api/nau7802` - Configure settings (gain, sample rate, channel, LDO, average, etc.)
 * - `POST /api/nau7802/calibrate` - Perform calibration (tare, known-weight, or AFE)
 *
 * @section nau7802_docs Documentation
 *
 * For complete API documentation, see:
 * - @ref docs/API_Endpoints.md "API Endpoints Documentation"
 * - @ref docs/ASSEMBLY_DATA_LAYOUT.md "Assembly Data Layout"
 * - @ref components/nau7802/README.md "NAU7802 Driver Documentation"
 */

