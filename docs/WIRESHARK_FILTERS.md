# Wireshark Filters for LLDP Debugging

## Simple Filters (should work in all Wireshark versions)

### 1. Capture ALL traffic (no filter)
- Leave the filter box empty
- Look for frames with source MAC `30:ed:a0:e2:67:ac`

### 2. Filter by MAC address
```
eth.addr == 30:ed:a0:e2:67:ac
```
This shows ALL frames to/from the ESP32 device.

### 3. Filter by EtherType (LLDP is 0x88CC)
```
eth.type == 0x88cc
```
Or in newer Wireshark:
```
ether.type == 0x88cc
```

### 4. Filter by destination MAC (LLDP multicast)
```
eth.dst == 01:80:c2:00:00:0e
```

### 5. Combined filter (MAC + EtherType)
```
eth.addr == 30:ed:a0:e2:67:ac and eth.type == 0x88cc
```

## Alternative: Display Filter in Older Wireshark Versions

If the above don't work, try these older syntax formats:

```
ether host 30:ed:a0:e2:67:ac
```

```
ether proto 0x88cc
```

## Manual Search (if filters don't work)

1. Start capture with NO filter
2. After capturing some traffic, use Edit → Find Packet
3. Search for:
   - String: `88cc` (hex)
   - Or String: `30:ed:a0:e2:67:ac` (hex)

## Verify You're Capturing on the Right Interface

1. Capture → Options
2. Select the Ethernet interface (NOT WiFi, NOT loopback)
3. The interface name usually contains "Ethernet", "eth", or the adapter name

## Check What You Should See

An LLDP frame should have:
- Destination MAC: `01:80:c2:00:00:0e`
- Source MAC: `30:ed:a0:e2:67:ac`
- EtherType: `88 CC` (in hex dump, bytes 12-13)

