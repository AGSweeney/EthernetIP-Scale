# Testing Tools

This directory contains various Python tools for testing and debugging the ESP32-P4 OpENer EtherNet/IP adapter.

## Tools

### ACD Conflict Simulator

`test_acd_conflict.py` - Python script to simulate IP address conflicts for testing Address Conflict Detection (ACD).

### Requirements

```bash
pip install scapy
```

**Note:** On Windows, you may need to install Npcap (WinPcap successor) for Scapy to work:
- Download from: https://npcap.com/
- Install Npcap (not Npcap SDK)

On Linux, you may need to run with `sudo` for raw socket access.

### Usage Examples

#### Send ARP announcements claiming an IP

```bash
# Send 3 ARP announcements
python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --count 3

# Continuously send announcements every 2 seconds
python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --continuous

# Use a specific MAC address
python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --mac 02:aa:bb:cc:dd:ee
```

#### Listen for ARP probes and automatically respond

This mode listens for ARP probes from the ESP32 and automatically sends ARP replies, simulating a device that already has the IP:

```bash
python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --respond-to-probes --duration 60
```

### Finding Your Network Interface

**Windows:**
```powershell
# List interfaces
python -c "from scapy.all import get_if_list; print('\n'.join(get_if_list()))"

# Common names: "Ethernet", "Ethernet 2", etc.
```

**Linux:**
```bash
# List interfaces
ip link show
# Common names: eth0, enp0s3, etc.
```

**macOS:**
```bash
# List interfaces
ifconfig
# Common names: en0, en1, etc.
```

### Testing Scenarios

1. **During Boot Probe Phase**: Run `--respond-to-probes` mode before booting the ESP32. The script will automatically respond to the ESP32's ARP probes, causing a conflict to be detected.

2. **After IP Assignment**: Run `--continuous` mode after the ESP32 has assigned its IP. The script will send periodic ARP announcements claiming the same IP, which should trigger ongoing conflict detection.

3. **Manual Testing**: Use `--count 1` to send a single ARP announcement at a specific time.

### Expected Behavior

When a conflict is detected, the ESP32 should:
- Log an ACD conflict error
- Set the user LED to solid (instead of blinking)
- Not assign the IP address (during probe phase)
- Report conflict details via EtherNet/IP Attribute #11

## ARP Conflict Sender

`send_conflict_arp.py` - Simple script to send ARP announcements claiming a specific IP address.

### Usage

```bash
python send_conflict_arp.py --ip 172.16.82.100 --interface eth0
```

## PDML to Markdown Converter

`pdml_to_markdown.py` - Convert Wireshark PDML (Packet Details Markup Language) export files into concise, human-readable Markdown format.

### Usage

```bash
python pdml_to_markdown.py input.pdml output.md
```

### Features

- **Concise Format**: Shows packet metadata, description, and key fields only
- **Human-Readable Descriptions**: Automatically describes what each packet is doing (ARP probes, TCP connections, etc.)
- **Key Field Extraction**: Extracts source/destination IPs, MAC addresses, ports, and other important fields
- **Protocol Detection**: Identifies EtherNet/IP, Modbus TCP, HTTP, DNS, and other protocols

### Example Output

```markdown
## Packet #77
**Time:** 14:08:43.123
**Delta:** 0.346855000 seconds
**Size:** 60 bytes
**Protocols:** `eth:ethertype:arp`

**Description:** **ARP Probe** from `0.0.0.0` asking "Who has 172.16.82.155?" | **Broadcast** frame from `30:ed:a0:e3:34:c1`

**Key Fields:**
- Source IP: `0.0.0.0`
- Target IP: `172.16.82.155`
- Source MAC: `30:ed:a0:e3:34:c1`
- Destination MAC: `ff:ff:ff:ff:ff:ff`
```

This tool is useful for documenting network captures and analyzing ACD behavior.

## ACD Retry Timing Test

`test_acd_retry_timing.py` - Comprehensive timed test for ACD retry logic verification.

### Usage

```bash
# Basic test with default settings (10s retry delay, 5 max attempts)
python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0

# Test with specific retry delay and max attempts
python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --retry-delay 10000 --max-attempts 5

# Extended test duration (5 minutes)
python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --duration 300
```

### Features

- **Automatic Conflict Triggering**: Responds to ESP32 ARP probes to trigger conflicts
- **Retry Timing Analysis**: Measures intervals between retry attempts
- **Timing Verification**: Verifies retry intervals match configured delay (with ±20% tolerance)
- **Max Attempts Verification**: Confirms device respects maximum retry attempts
- **Detailed Reporting**: Provides comprehensive test report with statistics

### Test Report Includes

- Total probes detected
- Retry attempts detected
- Retry interval statistics (average, min, max)
- Timing accuracy percentage
- Probe sequence analysis per attempt
- Verification of max attempts behavior

### Expected Behavior

When retry logic is working correctly:
- Device should retry after ~10 seconds (default `CONFIG_OPENER_ACD_RETRY_DELAY_MS`)
- Retry intervals should match configured delay within ±20% tolerance
- Device should stop retrying after max attempts (default: 5)
- Each retry attempt should send 3 ARP probes

## ARP Timing Analyzer

`analyze_arp_timing.py` - Analyze ARP timing patterns from Wireshark CSV exports.

### Usage

```bash
python analyze_arp_timing.py input.csv
```

## Interface Lister

`list_interfaces.py` - List available network interfaces for Scapy scripts.

### Usage

```bash
python list_interfaces.py
```

## Requirements

All tools require Python 3.x and the following packages (see `requirements.txt`):

```bash
pip install -r requirements.txt
```

**Note:** On Windows, you may need to install Npcap (WinPcap successor) for Scapy to work:
- Download from: https://npcap.com/
- Install Npcap (not Npcap SDK)

On Linux, you may need to run with `sudo` for raw socket access.

