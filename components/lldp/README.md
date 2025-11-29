# LLDP Component

This component provides LLDP (Link Layer Discovery Protocol) support for EtherNet/IP adapter devices to satisfy ODVA compliance requirements.

## Components

### CIP Objects
- **LLDP Management Object** (Class 0x109): Manages LLDP configuration and status
- **LLDP Data Table Object** (Class 0x10A): Stores LLDP neighbor information

### Platform Implementation
- **ESP32 Raw Socket Layer**: Raw Ethernet frame transmission/reception using ESP-NETIF L2 TAP
- **Frame Building Utilities**: Helper functions for constructing LLDP Ethernet frames

## Files

```
components/lldp/
├── CMakeLists.txt           # Component build configuration
├── README.md                # This file
├── include/
│   ├── lldp.h              # Public component interface
│   ├── ciplldpmanagement.h # LLDP Management Object
│   ├── ciplldpdatatable.h  # LLDP Data Table Object
│   ├── lldp_raw_socket.h   # ESP32 raw socket interface
│   └── lldp_frame_builder.h # Frame building utilities
└── src/
    ├── lldp_component.c    # Component initialization/deinitialization
    ├── ciplldpmanagement.c # LLDP Management Object implementation
    ├── ciplldpdatatable.c  # LLDP Data Table Object implementation
    └── esp32/
        ├── lldp_raw_socket.c      # ESP32 L2 TAP raw socket implementation
        └── lldp_frame_builder.c   # Ethernet frame building functions
```

## Usage

### Initialization

Initialize the LLDP component in your application (typically in `opener_init()`):

```c
#include "lldp.h"

void opener_init(struct netif *netif) {
    // ... other initialization ...
    
    #if OPENER_LLDP_ENABLED
    EipStatus status = LldpComponentInit(netif);
    if (status != kEipStatusOk) {
        // Handle error
    }
    #endif
}
```

### Cleanup

Deinitialize when shutting down:

```c
void cleanup() {
    #if OPENER_LLDP_ENABLED
    LldpComponentDeinit();
    #endif
}
```

## Configuration

LLDP can be enabled/disabled via `opener_user_conf.h`:

```c
#define OPENER_LLDP_ENABLED 1
#define OPENER_LLDP_TX_INTERVAL_MS 30000  // 30 seconds
```

## Dependencies

- **opener**: OpENer EtherNet/IP stack
- **lwip**: Network stack
- **freertos**: RTOS
- **esp_netif**: ESP-IDF network interface (ESP-IDF v5.0+)

## Status

- ✅ Component structure created
- ✅ ESP32 raw socket layer implemented
- ✅ Frame building utilities implemented
- ✅ OpENer LLDP CIP objects integrated
- ⚠️ LLDP Management Object has incomplete TODO functions
- ⚠️ LLDP Data Table Object has incomplete TODO functions
- ⚠️ Actual LLDP protocol implementation (frame construction, TLV encoding) not yet integrated

## Next Steps

1. Complete the LLDP Management Object encode/decode functions
2. Complete the LLDP Data Table Object implementation
3. Implement LLDP protocol layer (frame construction, TLV encoding/decoding)
4. Add periodic transmission timer
5. Integrate with OpENer main loop

