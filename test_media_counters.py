#!/usr/bin/env python3
"""
Script to send malformed packets to test Ethernet media counters.
Targets device at 172.16.82.99 (MAC: 30:ed:a0:e3:32:70)

Requires: pip install scapy
Run as: 
  - Linux: sudo python3 test_media_counters.py
  - Windows: Run PowerShell as Administrator, then: python test_media_counters.py
"""

from scapy.all import *
import time
import sys
import platform
import os

# Device information
TARGET_IP = "172.16.82.99"
TARGET_MAC = "30:ed:a0:e3:32:70"  # From your device logs

def get_interface():
    """Get the network interface to use"""
    if len(sys.argv) > 1:
        return sys.argv[1]
    
    # Try to find the interface automatically
    interfaces = get_if_list()
    
    if platform.system() == "Windows":
        # Windows interface names
        for iface in interfaces:
            if 'Ethernet' in iface or 'Local Area Connection' in iface:
                return iface
    else:
        # Linux interface names
        for iface in interfaces:
            if 'eth' in iface or 'enp' in iface or 'eno' in iface:
                return iface
    
    print("\nAvailable interfaces:", interfaces)
    return input("Enter interface name: ")

def send_oversized_frames(iface, count=10):
    """Send oversized frames (may trigger frame_too_long errors)"""
    print(f"\n[*] Sending {count} oversized frames...")
    for i in range(count):
        # Create frame larger than 1518 bytes (Ethernet MTU)
        payload = b"X" * 2000  # 2000 byte payload
        frame = Ether(dst=TARGET_MAC, src=get_if_hwaddr(iface)) / Raw(payload)
        sendp(frame, iface=iface, verbose=False)
        time.sleep(0.1)
    print("[+] Oversized frames sent")

def send_bad_fcs_packets(iface, count=20):
    """Send packets that may trigger FCS/CRC errors"""
    print(f"\n[*] Sending {count} packets with potential FCS errors...")
    for i in range(count):
        # Send malformed IP packets
        packet = Ether(dst=TARGET_MAC, src=get_if_hwaddr(iface)) / \
                 IP(dst=TARGET_IP, src="172.16.82.1") / \
                 Raw(b"X" * 100)
        # Corrupt the packet by modifying it after creation
        packet[Raw].load = packet[Raw].load[:-1] + b'\xFF'  # Modify last byte
        sendp(packet, iface=iface, verbose=False)
        time.sleep(0.05)
    print("[+] Bad FCS packets sent")

def send_fragmented_packets(iface, count=15):
    """Send fragmented packets (may trigger alignment errors)"""
    print(f"\n[*] Sending {count} fragmented/malformed packets...")
    for i in range(count):
        # Create packet with odd length (may cause alignment issues)
        payload = b"Y" * 99  # Odd length
        packet = Ether(dst=TARGET_MAC, src=get_if_hwaddr(iface)) / \
                 IP(dst=TARGET_IP, flags="MF", frag=0) / \
                 Raw(payload)
        sendp(packet, iface=iface, verbose=False)
        time.sleep(0.05)
    print("[+] Fragmented packets sent")

def send_rapid_packets(iface, count=100):
    """Send rapid burst of packets (may stress the link)"""
    print(f"\n[*] Sending {count} rapid packets...")
    for i in range(count):
        packet = Ether(dst=TARGET_MAC, src=get_if_hwaddr(iface)) / \
                 IP(dst=TARGET_IP) / \
                 UDP(dport=2222, sport=12345) / \
                 Raw(b"TEST" * 10)
        sendp(packet, iface=iface, verbose=False)
        if i % 10 == 0:
            time.sleep(0.01)  # Small delay every 10 packets
    print("[+] Rapid packets sent")

def send_jabber_frames(iface, count=5):
    """Send very long frames (jabber frames)"""
    print(f"\n[*] Sending {count} jabber frames...")
    for i in range(count):
        # Create extremely long frame
        payload = b"Z" * 5000  # 5000 bytes
        frame = Ether(dst=TARGET_MAC, src=get_if_hwaddr(iface)) / Raw(payload)
        sendp(frame, iface=iface, verbose=False)
        time.sleep(0.2)
    print("[+] Jabber frames sent")

def send_short_frames(iface, count=20):
    """Send very short frames (may trigger errors)"""
    print(f"\n[*] Sending {count} short frames...")
    for i in range(count):
        # Minimum Ethernet frame is 64 bytes, send shorter
        frame = Ether(dst=TARGET_MAC, src=get_if_hwaddr(iface)) / Raw(b"X" * 10)
        sendp(frame, iface=iface, verbose=False)
        time.sleep(0.05)
    print("[+] Short frames sent")

def main():
    print("=" * 60)
    print("Ethernet Media Counter Test Script")
    print("=" * 60)
    print(f"Target IP: {TARGET_IP}")
    print(f"Target MAC: {TARGET_MAC}")
    print("=" * 60)
    
    # Check if running with admin privileges
    is_admin = False
    if platform.system() == "Windows":
        try:
            import ctypes
            is_admin = ctypes.windll.shell32.IsUserAnAdmin() != 0
        except:
            is_admin = False
    else:
        try:
            is_admin = os.geteuid() == 0
        except AttributeError:
            # Some systems don't have geteuid
            is_admin = False
    
    if not is_admin:
        if platform.system() == "Windows":
            print("\n[!] ERROR: This script must be run as Administrator")
            print("    Right-click PowerShell and select 'Run as Administrator'")
        else:
            print("\n[!] ERROR: This script must be run as root (use sudo)")
            print("    Example: sudo python3 test_media_counters.py")
        sys.exit(1)
    
    # Get interface
    iface = get_interface()
    print(f"\n[*] Using interface: {iface}")
    
    try:
        # Get our MAC address
        our_mac = get_if_hwaddr(iface)
        print(f"[*] Source MAC: {our_mac}")
        
        print("\n[*] Starting packet injection tests...")
        print("[*] Monitor your device's media counters via EtherNet/IP")
        print("[*] Press Ctrl+C to stop\n")
        
        # Run different test types
        send_oversized_frames(iface, count=10)
        time.sleep(1)
        
        send_bad_fcs_packets(iface, count=20)
        time.sleep(1)
        
        send_fragmented_packets(iface, count=15)
        time.sleep(1)
        
        send_rapid_packets(iface, count=100)
        time.sleep(1)
        
        send_jabber_frames(iface, count=5)
        time.sleep(1)
        
        send_short_frames(iface, count=20)
        
        print("\n" + "=" * 60)
        print("[+] All test packets sent!")
        print("[*] Check your device's media counters via EtherNet/IP")
        print("[*] Look for increments in:")
        print("    - Frame Too Long")
        print("    - FCS Errors")
        print("    - Alignment Errors")
        print("    - MAC Receive Errors")
        print("=" * 60)
        
    except KeyboardInterrupt:
        print("\n\n[!] Interrupted by user")
    except Exception as e:
        print(f"\n[!] Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()

