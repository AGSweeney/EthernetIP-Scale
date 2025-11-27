# EthernetIP-Scale Configuration Tool

A Windows GUI application for configuring EthernetIP-Scale devices.

## Features

- **Network Configuration**: Configure DHCP/Static IP, netmask, gateway, and DNS settings
- **NAU7802 Scale Configuration**: Full scale configuration with calibration support
- **EtherNet/IP Assembly Monitoring**: Bit-level visualization of Input and Output assemblies
- **Modbus TCP Configuration**: Enable/disable Modbus TCP server
- **OTA Firmware Updates**: Upload and install firmware updates via the application

## Requirements

- Windows 10 version 1809 (build 17763) or later
- .NET 8.0 SDK
- Visual Studio 2022 with Windows App SDK workload, or
- Visual Studio Code with C# extension

## Building

1. Open `EulerLink.sln` (or the solution file) in Visual Studio 2022
2. Restore NuGet packages
3. Build the solution (F6 or Build > Build Solution)
4. Run the application (F5)

## Usage

1. Launch the application
2. Enter the device IP address in the top bar (default: 192.168.1.100)
3. Navigate through the different pages using the left sidebar:
   - **Configuration**: Configure network, Modbus TCP, and NAU7802 scale settings
   - **Input Assembly (T->O)**: View Input Assembly 100 data
   - **Output Assembly (O->T)**: View Output Assembly 150 data
   - **Firmware Update**: Upload firmware updates

## Project Structure

```
WinUIEIP/
├── Models/              # Data models for API responses
├── Services/            # API client service
├── Views/               # UI pages
│   ├── ConfigurationPage.xaml
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
- `/api/status` - Assembly status with parsed sensor data
- `/api/assemblies` - Assembly data
- `/api/modbus` - Modbus TCP configuration
- `/api/nau7802` - NAU7802 scale configuration
- `/api/nau7802/calibrate` - Scale calibration (tare/known weight)
- `/api/i2c/pullup` - I2C pull-up configuration
- `/api/ota/update` - Firmware update

## Notes

- The device IP address can be changed in the top bar of the main window
- Network configuration changes require a device reboot to take effect
- Assembly data pages auto-refresh periodically
- Assembly pages show bit-level visualization of all 32 bytes

