#!/usr/bin/env python3
"""
Simple ARP Conflict Simulator

Continuously sends ARP announcements claiming an IP address.
This is simpler than sniffing and responding to probes.

Usage:
    python send_conflict_arp.py --ip 172.16.82.100 --interface "Ethernet"
"""

import argparse
import time
import sys
import random
from scapy.all import ARP, Ether, sendp, get_if_list


def send_conflict_arp(interface, target_ip, interval=1.0):
    """Continuously send ARP announcements claiming the target IP"""
    
    # Generate a fake MAC address
    fake_mac = f"02:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}"
    
    print(f"Sending ARP announcements claiming {target_ip}")
    print(f"Using MAC address: {fake_mac}")
    print(f"Interval: {interval} seconds")
    print(f"Press Ctrl+C to stop\n")
    
    count = 0
    try:
        while True:
            # Create gratuitous ARP (announcement)
            arp = ARP(
                op=1,  # ARP request
                psrc=target_ip,  # Source IP (the IP we're claiming)
                pdst=target_ip,  # Target IP (same as source for gratuitous)
                hwsrc=fake_mac,  # Source MAC
                hwdst="ff:ff:ff:ff:ff:ff"  # Broadcast MAC
            )
            
            # Send on Ethernet layer
            packet = Ether(dst="ff:ff:ff:ff:ff:ff", src=fake_mac) / arp
            
            sendp(packet, iface=interface, verbose=False)
            count += 1
            print(f"[{count}] Sent ARP announcement: {target_ip} is at {fake_mac}")
            
            time.sleep(interval)
            
    except KeyboardInterrupt:
        print(f"\n\nStopped. Sent {count} ARP announcements.")
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Send continuous ARP announcements to test ACD conflict detection"
    )
    
    parser.add_argument(
        "--ip",
        required=True,
        help="IP address to claim (e.g., 172.16.82.100)"
    )
    
    parser.add_argument(
        "--interface",
        required=True,
        help="Network interface name"
    )
    
    parser.add_argument(
        "--interval",
        type=float,
        default=1.0,
        help="Seconds between announcements (default: 1.0)"
    )
    
    args = parser.parse_args()
    
    # Validate interface
    interfaces = get_if_list()
    if args.interface not in interfaces:
        print(f"Warning: Interface '{args.interface}' not found in list:")
        for iface in interfaces:
            print(f"  - {iface}")
        print("\nTrying anyway...\n")
    
    send_conflict_arp(args.interface, args.ip, args.interval)


if __name__ == "__main__":
    main()

