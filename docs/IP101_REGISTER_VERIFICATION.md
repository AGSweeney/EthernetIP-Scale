# IP101 PHY Register Address Verification

## Purpose
This document lists the register addresses currently used for reading IP101 PHY MIB counters. These addresses need to be verified against the official IP101 datasheet.

## Current Register Addresses (VERIFIED - INCORRECT!)

**⚠️ CRITICAL FINDING**: Based on the IP101 datasheet register map (Table 2), the current addresses are **INCORRECT**:

| Register Address | What We Think It Is | What It Actually Is (from datasheet) | Default Value | Status |
|-----------------|-------------------|--------------------------------------|---------------|--------|
| **0x16** | Single Collision Frames | ❌ Unknown (not in visible register map) | - | **INCORRECT** |
| **0x18** | Multiple Collision Frames | ❌ Unknown (not in visible register map) | - | **INCORRECT** |
| **0x1A** | Late Collisions | ✅ **Page 16, Reg 26 = Digital IO Pin Driving Control** | 0x1249 | **INCORRECT** ✅ CONFIRMED |
| **0x1C** | Excessive Collisions | ❌ Unknown (not in visible register map) | - | **INCORRECT** |
| **0x1E** | Carrier Sense Errors | ❌ Unknown (not in visible register map) | - | **INCORRECT** |
| **0x22** | SQE Test Errors | ✅ **Page 1, Reg 22 = Linear Regulator Output Control** | 0x2020 | **INCORRECT** |

**Key Findings:**
- ✅ **CONFIRMED**: Register 0x1A reads 0x1249 (4681), which **exactly matches** Page 16, Register 26's default value (0x1249)!
  - This proves we're reading "Digital IO Pin Driving Control Register", not a collision counter
- Register 0x22 is on Page 1 and is "Linear Regulator Output Control Register" (default 0x2020), not SQE test errors
- **Page Control Register is 0x14 (20), not 0x1F** - default value is 0x0010
- **MIB counters are NOT in the visible register map table** - they may be:
  - In MMD registers (MMD 3.x or 7.x) - accessed via registers 13-14
  - On a page not shown in Table 2 (need to check other pages in datasheet)
  - Not implemented in IP101 (may need to rely on ESP32 EMAC counters only)

## IP101 Register Map (from Datasheet)

**Page Control Register**: Register 20 (0x14) - default 0x0010
- Bits 4-0: Page number selection
- Used to switch between different register pages

**Known Pages from Datasheet:**
- **Page 0 (default)**: Standard IEEE 802.3 registers (0x00-0x1F)
- **Page 1**: Extended registers
  - Register 18: **RX CRC Error Counter Register** ✅ (this is a counter!)
  - Register 22: Linear Regulator Output Control Register
- **Page 16**: PHY-specific control registers (Digital IO Control at Reg 26)
- **Page 17**: WOL+ Status Register
- **Page 18**: RX Counter Interrupt Control/Status Register (indicates counters exist)
- **Page 2, 3, 4, 5, 8, 11**: Additional vendor-specific pages
- **MMD Registers**: MMD 3.x, 7.x (accessed via registers 13-14)

**MIB Counters Location**: 
- ✅ **FOUND**: Page 1, Register 18 = "RX CRC Error Counter Register" (this is a counter!)
- ✅ **FOUND**: Page 18, Register 17 = "RX Counter Interrupt Control/Status Register" (indicates counters exist)
- ❌ **NOT FOUND**: Collision counters (single, multiple, late, excessive) - still need to locate
- ❌ **NOT FOUND**: Carrier sense errors, SQE test errors
- May be in MMD registers (MMD 3.x or 7.x) - accessed via registers 13-14
- May be on other pages not yet shown in the register map
- Need to check full datasheet for "MIB", "Statistics", or "Counters" section

## Important Notes

1. **Register 0x22**: This address is outside the standard 0x00-0x1F range. This suggests it may be:
   - A vendor-specific extended register
   - Require page selection (register 0x1F) to access
   - Part of an MMD register space

2. **Register 0x22 Value**: The value read (0x0243 = 579) matches the IP101 OUI (0x0243), which is suspicious and suggests this may not be the SQE test error register.

3. **Page Selection**: Many PHYs use register 0x1F to select between pages:
   - Page 0: Standard registers
   - Page 1: Extended/MIB registers
   - Other pages: Vendor-specific

## How to Verify

1. **Obtain the IP101 Datasheet**:
   - Manufacturer: IC Plus Corporation
   - Search for "IP101 datasheet" or "IP101GA datasheet"
   - Common sources: manufacturer website, distributor sites, datasheet repositories

2. **Check the Register Map Section**:
   - Look for "Register Map", "Register Description", or "MIB Registers"
   - Verify each address (0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x22)
   - Check if page selection is required

3. **Verify Register Functions**:
   - Confirm that each register address corresponds to the expected counter
   - Check register bit definitions and data format (16-bit vs 32-bit)
   - Verify if registers are read-only or read/write

4. **Check for Page Selection**:
   - Look for register 0x1F (Page Select Register)
   - Determine which page contains MIB counters
   - Check if registers need to be accessed on a specific page

## Expected Register Map (Based on Common PHY Layouts)

Many 10/100 PHYs follow a similar pattern, but IP101 may differ:

| Register | Standard Function | Notes |
|----------|-------------------|-------|
| 0x00 | Control Register | Standard IEEE 802.3 |
| 0x01 | Status Register | Standard IEEE 802.3 |
| 0x02-0x03 | PHY Identifier | Standard IEEE 802.3 |
| 0x04-0x0F | Auto-negotiation, etc. | Standard IEEE 802.3 |
| 0x10-0x1F | Vendor-specific | May contain MIB counters |
| 0x1F | Page Select | If supported |
| 0x20+ | Extended/MMD | May require special access |

## Current Implementation Status

- ✅ **PHY Detection**: Working (IP101 detected correctly: ID 0x02430C54)
- ✅ **Register Reading**: Working (values are being read)
- ⚠️ **Register Addresses**: **NEEDS VERIFICATION** - addresses may be incorrect
- ⚠️ **Counter Values**: Values read (4681, 1, 262, 579) are persistent and don't change, suggesting:
  - Addresses may be incorrect
  - Counters may be on a different page
  - Values may be vendor-specific data, not counters

## Next Steps

1. ✅ **Datasheet Obtained**: Register map table reviewed (Table 2, page 19/65)
2. ✅ **Register Addresses Verified**: Confirmed current addresses are INCORRECT
3. **Find MIB Counters**: Check datasheet for:
   - MMD register section (MMD 3.x, 7.x) - may contain MIB statistics
   - Other pages in register map (pages beyond what's shown in Table 2)
   - "MIB", "Statistics", or "Counters" section in datasheet
4. **Alternative**: If IP101 doesn't have MIB counters, use ESP32 EMAC counters only
5. **Update Code**: Once correct addresses found, update `eth_media_counters.c`
6. **Test**: Verify counters increment when errors occur

## Resources

- IC Plus Corporation website (manufacturer)
- Datasheet repositories: alldatasheet.com, datasheetq.com, embedic.com
- IEEE 802.3 standard (for standard register definitions)
- ESP-IDF documentation (may have IP101-specific notes)

## Related Files

- `components/opener/src/ports/ESP32/eth_media_counters.c` - Implementation
- `components/opener/src/ports/ESP32/eth_media_counters.h` - Header
- `main/main.c` - Initialization code

