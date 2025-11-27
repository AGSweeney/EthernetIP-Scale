# EtherNet/IP Assembly Data Layout

This document describes the exact byte-by-byte layout of data in the EtherNet/IP assemblies.

## Overview

The device exposes three EtherNet/IP assembly instances:

- **Assembly 100 (Input)**: 32 bytes - Input data from sensors and I/O
- **Assembly 150 (Output)**: 32 bytes - Output data to actuators and control
- **Assembly 151 (Configuration)**: 10 bytes - Configuration parameters

All data is stored in **little-endian** byte order.

---

## Assembly 100 (Input Assembly) - 32 Bytes

The Input Assembly contains sensor data and I/O input states. Data sources can be configured with byte offsets to avoid conflicts.

### Default Layout

| Byte Range | Size | Data Source | Description | Format |
|------------|------|-------------|-------------|--------|
| 0-31 | 32 bytes | Available | Reserved for sensor data and I/O | - |

### NAU7802 Scale Data (Configurable Byte Offset)

The NAU7802 scale data can be placed at any byte offset (0-22) to avoid conflicts with other sensors. The default offset is 0.

| Byte Range | Size | Field Name | Description | Format |
|------------|------|------------|-------------|--------|
| offset+0 to offset+3 | 4 bytes | Weight | Calibrated weight reading (scaled by 100) | int32 (little-endian) |
| offset+4 to offset+7 | 4 bytes | Raw Reading | Raw 24-bit ADC reading | int32 (little-endian) |
| offset+8 | 1 byte | Unit Code | Weight unit: 0=grams, 1=lbs, 2=kg | uint8 |
| offset+9 | 1 byte | Status Flags | Status flags (see below) | uint8 |

**Weight Format:**
- Stored as `int32_t` scaled by 100 (no floating point)
- Example: 100.24 lbs = 10024, 50.5 kg = 5050
- Unit is stored in byte 8: `0` = grams, `1` = lbs, `2` = kg
- Unit selection is configurable via Web UI and stored in NVS
- To get actual weight: `weight = assembly_value / 100.0`
- The unit code allows controllers to interpret the weight value correctly

**Status Flags (Byte 9):**
- Bit 0 (0x01): `available` - New reading is available (data ready)
- Bit 1 (0x02): `connected` - NAU7802 device is connected and responding
- Bit 2 (0x04): `initialized` - NAU7802 is initialized and ready
- Bits 3-7: Reserved (always 0)

**Example Status Byte Values:**
- `0x07` (0b00000111) = available + connected + initialized (fully operational)
- `0x06` (0b00000110) = connected + initialized (no new reading yet)
- `0x04` (0b00000100) = initialized only (not connected)
- `0x00` = Not initialized

**Configuration:**
- Byte offset is configurable via Web UI (`/api/nau7802`) or NVS
- Default offset: 0 (bytes 0-9)
- Maximum offset: 22 (to fit 10 bytes in 32-byte assembly)

**Example Layout (offset = 0):**
```
Byte:  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
      ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
      │ Weight (float32)        │ Raw Reading (int32)        │ Other sensor data...    │
      └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
```

**Example Layout (offset = 8):**
```
Byte:  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
      ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
      │ Other sensor data...    │ Weight (float32)        │ Raw Reading (int32)        │
      └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
```

---

## Assembly 150 (Output Assembly) - 32 Bytes

The Output Assembly contains control data and output states sent from the EtherNet/IP controller.

### Layout

| Byte Range | Size | Field Name | Description | Format |
|------------|------|------------|-------------|--------|
| 0-31 | 32 bytes | Control Data | General control and output data | Configurable |

All 32 bytes are available for application use.

---

## Assembly 151 (Configuration Assembly) - 10 Bytes

The Configuration Assembly contains device configuration parameters.

### Layout

| Byte Range | Size | Field Name | Description | Format |
|------------|------|------------|-------------|--------|
| 0-9 | 10 bytes | Configuration | Configuration parameters | TBD |

**Note:** Configuration assembly structure is currently reserved for future use.

---

## Data Format Details

### Integer Formats

All multi-byte integers are stored in **little-endian** format (least significant byte first):

**int32_t (4 bytes):**
```
Byte 0: LSB (bits 0-7)
Byte 1: (bits 8-15)
Byte 2: (bits 16-23)
Byte 3: MSB (bits 24-31, sign bit)
```

**Example:** Value 123400 (0x0001E208) stored as:
- Byte 0: 0x08
- Byte 1: 0xE2
- Byte 2: 0x01
- Byte 3: 0x00

**uint16_t (2 bytes):**
```
Byte 0: LSB (bits 0-7)
Byte 1: MSB (bits 8-15)
```

**Example:** Value 0x1234 stored as:
- Byte 0: 0x34
- Byte 1: 0x12

---

## Modbus TCP Mapping

The assemblies are also accessible via Modbus TCP (port 502):

### Input Assembly 100 → Modbus Input Registers

- **Modbus Function**: Read Input Registers (0x04)
- **Register Range**: 0-15 (16 registers = 32 bytes)
- **Mapping**: Direct byte-to-register mapping
- **Endianness**: Assembly is little-endian, Modbus converts to big-endian

| Assembly Byte | Modbus Register | Notes |
|---------------|-----------------|-------|
| 0-1 | 0 | Little-endian in assembly, big-endian in Modbus |
| 2-3 | 1 | |
| ... | ... | |
| 30-31 | 15 | |

### Output Assembly 150 → Modbus Holding Registers

- **Modbus Function**: Read/Write Holding Registers (0x03, 0x06, 0x10)
- **Register Range**: 100-115 (16 registers = 32 bytes)
- **Mapping**: Direct byte-to-register mapping
- **Endianness**: Assembly is little-endian, Modbus converts to big-endian

| Assembly Byte | Modbus Register | Notes |
|---------------|-----------------|-------|
| 0-1 | 100 | Little-endian in assembly, big-endian in Modbus |
| 2-3 | 101 | |
| ... | ... | |
| 30-31 | 115 | Available for application use |

### Configuration Assembly 151 → Modbus Holding Registers

- **Modbus Function**: Read/Write Holding Registers (0x03, 0x06, 0x10)
- **Register Range**: 150-154 (5 registers = 10 bytes)
- **Mapping**: Direct byte-to-register mapping

---

## Configuration and Byte Offsets

Byte offsets for sensor data can be configured via API endpoints to avoid conflicts between different data sources.

---

## Thread Safety

All assembly data access is protected by a mutex (`scale_application_get_assembly_mutex()`). When reading or writing assembly data:

1. Take the mutex: `xSemaphoreTake(assembly_mutex, portMAX_DELAY)`
2. Perform read/write operations
3. Release the mutex: `xSemaphoreGive(assembly_mutex)`

**Note:** The mutex is shared across all components (Modbus TCP, EtherNet/IP).

---

## Example: Reading Assembly Data

### C Code Example

```c
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern uint8_t g_assembly_data064[32];
extern SemaphoreHandle_t scale_application_get_assembly_mutex(void);

void read_assembly_data(uint8_t *data, size_t offset, size_t length)
{
    SemaphoreHandle_t mutex = scale_application_get_assembly_mutex();
    if (mutex == NULL) return;
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    // Read data from assembly starting at offset
    memcpy(data, &g_assembly_data064[offset], length);
    
    xSemaphoreGive(mutex);
}
```

### Python Example (via Modbus TCP)

```python
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient('192.168.1.100', port=502)
client.connect()

# Read Input Assembly 100 (Modbus registers 0-15)
result = client.read_input_registers(0, 16)

# Read Output Assembly 150 (Modbus registers 100-115)
result = client.read_holding_registers(100, 16)

# Process output assembly data as needed
# All 32 bytes (16 registers) are available for application use

client.close()
```

---

## Summary Table

| Assembly | Size | Primary Data Sources | Key Fields |
|----------|------|---------------------|------------|
| **100 (Input)** | 32 bytes | Sensors and I/O | NAU7802 scale data (configurable offset) |
| **150 (Output)** | 32 bytes | EtherNet/IP controller | Control data (all 32 bytes available) |
| **151 (Config)** | 10 bytes | Reserved | TBD |

---

## Related Documentation

- [API Endpoints](API_Endpoints.md) - Web API for configuring byte offsets
- [Main README](../README.md) - Project overview
- [Modbus TCP Mapping](../README.md#modbus-tcp-mapping) - Modbus register mapping

---

**Last Updated**: See git commit history  
**Assembly Version**: 1.0

