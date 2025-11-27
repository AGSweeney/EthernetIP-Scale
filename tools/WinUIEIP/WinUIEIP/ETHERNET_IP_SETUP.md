# EtherNet/IP Setup Instructions

To enable faster real-time data reading via EtherNet/IP instead of HTTP polling:

## Step 1: Download EEIP.NET Library

1. Download the EEIP.dll from SourceForge:
   https://sourceforge.net/projects/eeip-net/files/EEIP.dll/download

   OR

2. Clone the repository and build it:
   ```bash
   git clone https://github.com/rossmann-engineering/EEIP.NET.git
   cd EEIP.NET/EEIP.NET
   # Build the project to get EEIP.dll
   ```

## Step 2: Add Reference to Project

1. Copy `EEIP.dll` to a `libs` folder in your project (create the folder if needed)
2. Add the reference in `WinUIEIP.csproj`:

```xml
<ItemGroup>
  <Reference Include="EEIP">
    <HintPath>libs\EEIP.dll</HintPath>
  </Reference>
</ItemGroup>
```

3. Ensure the DLL is copied to output:
```xml
<ItemGroup>
  <None Include="libs\EEIP.dll">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </None>
</ItemGroup>
```

## Step 3: Usage

The `EtherNetIPService` will automatically use the library once it's referenced. The MPU6050 Status page can be configured to use EtherNet/IP for real-time updates instead of HTTP polling.

## Benefits

- **Lower Latency**: Direct binary protocol, no HTTP/JSON overhead
- **Real-time Updates**: Implicit messaging provides cyclic data updates
- **More Efficient**: Binary data transfer vs JSON serialization
- **Industrial Standard**: Uses EtherNet/IP protocol designed for real-time industrial data

