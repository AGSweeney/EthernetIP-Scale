#!/usr/bin/env python3
"""
ARP Conflict Simulator for ACD Testing

This script sends ARP announcements and replies to simulate a device
claiming an IP address, allowing you to test Address Conflict Detection (ACD)
on the ESP32 device.

Usage:
    python test_acd_conflict.py --ip 172.16.82.100 --interface eth0
    python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --respond-to-probes

Requirements:
    pip install scapy

Author: Adam G. Sweeney <agsweeney@gmail.com>
License: MIT
"""

import argparse
import time
import sys
from scapy.all import ARP, Ether, sendp, get_if_list, get_if_hwaddr
from scapy.layers.l2 import getmacbyip


def get_interface_mac(interface):
    """Get MAC address for the given interface"""
    try:
        return get_if_hwaddr(interface)
    except Exception as e:
        print(f"Error getting MAC for {interface}: {e}")
        return None


def send_arp_announcement(interface, target_ip, fake_mac=None, count=1, interval=2.0):
    """
    Send ARP announcements (gratuitous ARPs) claiming the target IP.
    
    Args:
        interface: Network interface name (e.g., 'eth0', 'Ethernet')
        target_ip: IP address to claim
        fake_mac: MAC address to use (default: random)
        count: Number of announcements to send
        interval: Seconds between announcements
    """
    if fake_mac is None:
        # Use a fake MAC address that won't conflict with real devices
        # Format: 02:XX:XX:XX:XX:XX (locally administered, unicast)
        import random
        fake_mac = f"02:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}"
        print(f"Using fake MAC address: {fake_mac}")
    
    print(f"Sending ARP announcements claiming {target_ip}...")
    print(f"Press Ctrl+C to stop")
    
    try:
        for i in range(count):
            # Create gratuitous ARP (announcement)
            # Gratuitous ARP: sender IP = target IP, sender MAC = our MAC
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
            print(f"[{i+1}/{count}] Sent ARP announcement: {target_ip} is at {fake_mac}")
            
            if i < count - 1:
                time.sleep(interval)
                
    except KeyboardInterrupt:
        print("\nStopped by user")
    except Exception as e:
        print(f"Error sending ARP: {e}")
        sys.exit(1)


def send_arp_reply_to_probe(interface, probe_ip, probe_mac, our_ip, our_mac=None):
    """
    Send an ARP reply in response to an ARP probe.
    
    This simulates a device responding to the ESP32's ARP probe,
    indicating that the IP is already in use.
    
    Args:
        interface: Network interface name
        probe_ip: IP address being probed (e.g., 172.16.82.100)
        probe_mac: MAC address of the device sending the probe (ESP32 MAC)
        our_ip: IP address we're claiming (same as probe_ip)
        our_mac: MAC address to use (default: random)
    """
    if our_mac is None:
        import random
        our_mac = f"02:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}"
        print(f"Using fake MAC address: {our_mac}")
    
    print(f"Sending ARP reply: {our_ip} is at {our_mac} (in response to probe from {probe_mac})")
    
    # ARP reply: op=2, target is the probe sender
    arp = ARP(
        op=2,  # ARP reply
        psrc=our_ip,  # Source IP (the IP we're claiming)
        pdst=probe_ip,  # Target IP (same as probe IP)
        hwsrc=our_mac,  # Source MAC
        hwdst=probe_mac  # Target MAC (ESP32)
    )
    
    packet = Ether(dst=probe_mac, src=our_mac) / arp
    sendp(packet, iface=interface, verbose=False)


def listen_and_respond(interface, target_ip, our_mac=None, duration=60):
    """
    Listen for ARP probes and automatically respond.
    
    This function sniffs for ARP probes targeting the specified IP
    and automatically sends ARP replies.
    """
    from scapy.all import sniff, IP, ARP
    
    if our_mac is None:
        import random
        our_mac = f"02:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}"
        print(f"Using fake MAC address: {our_mac}")
    
    print(f"Listening for ARP probes for {target_ip}...")
    print(f"Will automatically respond for {duration} seconds")
    print(f"Press Ctrl+C to stop")
    
    probe_count = 0
    
    def handle_arp_probe(packet):
        nonlocal probe_count
        if packet.haslayer(ARP):
            arp = packet[ARP]
            # Check if this is an ARP probe (op=1, psrc=0.0.0.0, pdst=target_ip)
            # Also check if pdst matches target_ip (ARP probe for our IP)
            psrc_str = str(arp.psrc)
            pdst_str = str(arp.pdst)
            
            print(f"DEBUG: ARP packet - op={arp.op}, psrc={psrc_str}, pdst={pdst_str}, hwsrc={arp.hwsrc}")
            
            if arp.op == 1:  # ARP request
                # Check if source IP is 0.0.0.0 (probe) and target is our IP
                if psrc_str == "0.0.0.0" and pdst_str == target_ip:
                    probe_count += 1
                    print(f"[{probe_count}] Detected ARP probe from {arp.hwsrc} for {target_ip}")
                    send_arp_reply_to_probe(interface, target_ip, arp.hwsrc, target_ip, our_mac)
                elif pdst_str == target_ip:
                    print(f"DEBUG: ARP request for {target_ip} but psrc={psrc_str} (not a probe)")
    
    try:
        # Use a simpler filter - just catch all ARP packets and filter in Python
        # Windows BPF filter syntax can be tricky
        print("Starting packet capture...")
        sniff(iface=interface, filter="arp", 
              prn=handle_arp_probe, timeout=duration, store=False)
        print(f"\nCapture completed. Responded to {probe_count} ARP probes.")
    except KeyboardInterrupt:
        print(f"\nStopped by user. Responded to {probe_count} ARP probes.")
    except Exception as e:
        print(f"Error during capture: {e}")
        import traceback
        traceback.print_exc()


def main():
    parser = argparse.ArgumentParser(
        description="Send ARP announcements to test ACD conflict detection",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Send 3 ARP announcements claiming 172.16.82.100
  python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --count 3

  # Continuously send announcements every 2 seconds
  python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --continuous

  # Listen for ARP probes and automatically respond
  python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --respond-to-probes

  # Use a specific MAC address
  python test_acd_conflict.py --ip 172.16.82.100 --interface eth0 --mac 02:aa:bb:cc:dd:ee
        """
    )
    
    parser.add_argument(
        "--ip",
        required=True,
        help="IP address to claim (e.g., 172.16.82.100)"
    )
    
    parser.add_argument(
        "--interface",
        required=True,
        help="Network interface name (e.g., eth0, Ethernet, en0)"
    )
    
    parser.add_argument(
        "--mac",
        help="MAC address to use (default: random fake MAC)"
    )
    
    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help="Number of ARP announcements to send (default: 1)"
    )
    
    parser.add_argument(
        "--interval",
        type=float,
        default=2.0,
        help="Seconds between announcements (default: 2.0)"
    )
    
    parser.add_argument(
        "--continuous",
        action="store_true",
        help="Continuously send announcements until stopped (Ctrl+C)"
    )
    
    parser.add_argument(
        "--respond-to-probes",
        action="store_true",
        help="Listen for ARP probes and automatically respond"
    )
    
    parser.add_argument(
        "--duration",
        type=int,
        default=60,
        help="Duration in seconds for probe response mode (default: 60)"
    )
    
    args = parser.parse_args()
    
    # Validate interface
    interfaces = get_if_list()
    if args.interface not in interfaces:
        print(f"Warning: Interface '{args.interface}' not found in list:")
        for iface in interfaces:
            print(f"  - {iface}")
        print("\nTrying anyway...")
    
    # Validate MAC format if provided
    if args.mac:
        import re
        if not re.match(r'^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$', args.mac):
            print(f"Error: Invalid MAC address format: {args.mac}")
            print("Expected format: XX:XX:XX:XX:XX:XX or XX-XX-XX-XX-XX-XX")
            sys.exit(1)
        # Normalize to colon format
        args.mac = args.mac.replace('-', ':')
    
    # Run the appropriate mode
    if args.respond_to_probes:
        listen_and_respond(args.interface, args.ip, args.mac, args.duration)
    elif args.continuous:
        send_arp_announcement(args.interface, args.ip, args.mac, count=999999, interval=args.interval)
    else:
        send_arp_announcement(args.interface, args.ip, args.mac, count=args.count, interval=args.interval)


if __name__ == "__main__":
    main()

