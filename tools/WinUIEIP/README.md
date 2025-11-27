# EulerLink - EtherNet/IP Device Configuration Tool

A Windows GUI application for configuring ESP32-P4 OpENer EtherNet/IP adapter devices.

## Features

- **Network Configuration**: Configure DHCP/Static IP, netmask, gateway, and DNS settings
- **VL53L1x Sensor Configuration**: Full sensor parameter configuration with calibration support
- **Real-time Sensor Monitoring**: Live distance readings with visual bar chart
- **EtherNet/IP Assembly Monitoring**: Bit-level visualization of Input and Output assemblies
- **Modbus TCP Configuration**: Enable/disable Modbus TCP server
- **OTA Firmware Updates**: Upload and install firmware updates via the application

## Requirements

- Windows 10 version 1809 (build 17763) or later
- .NET 8.0 SDK
- Visual Studio 2022 with Windows App SDK workload, or
- Visual Studio Code with C# extension

## Building

1. Open `EulerLink.sln` in Visual Studio 2022
2. Restore NuGet packages
3. Build the solution (F6 or Build > Build Solution)
4. Run the application (F5)

## Usage

1. Launch the application
2. Enter the device IP address in the top bar (default: 192.168.1.100)
3. Navigate through the different pages using the left sidebar:
   - **Configuration**: Configure network, Modbus TCP, and sensor settings
   - **VL53L1x Status**: Monitor real-time sensor readings
   - **Input Assembly (T->O)**: View Input Assembly 100 data
   - **Output Assembly (O->T)**: View Output Assembly 150 data
   - **Firmware Update**: Upload firmware updates

## Project Structure

```
EulerLink/
├── Models/              # Data models for API responses
├── Services/            # API client service
├── Views/               # UI pages
│   ├── ConfigurationPage.xaml
│   ├── SensorStatusPage.xaml
│   ├── InputAssemblyPage.xaml
│   ├── OutputAssemblyPage.xaml
│   └── FirmwareUpdatePage.xaml
├── App.xaml             # Application entry point
├── MainWindow.xaml      # Main window with navigation
└── Package.appxmanifest # App manifest
```

## API Endpoints

The application communicates with the device using REST API endpoints documented in `WebUIReadme.md`:

- `/api/ipconfig` - Network configuration
- `/api/config` - Sensor configuration
- `/api/status` - Sensor status
- `/api/assemblies` - Assembly data
- `/api/modbus` - Modbus TCP configuration
- `/api/sensor/enabled` - Sensor enable/disable
- `/api/sensor/byteoffset` - Sensor byte offset
- `/api/calibrate/offset` - Offset calibration
- `/api/calibrate/xtalk` - Xtalk calibration
- `/api/ota/update` - Firmware update

## Notes

- The device IP address can be changed in the top bar of the main window
- Network configuration changes require a device reboot to take effect
- Sensor monitoring pages auto-refresh every 250ms
- Assembly pages show bit-level visualization of all 32 bytes

