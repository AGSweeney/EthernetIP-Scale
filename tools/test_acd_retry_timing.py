#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Timed ACD Retry Logic Test

This script tests the ACD retry logic by:
1. Listening for ARP probes from the ESP32 device (probe phase)
2. Responding to each probe to trigger conflicts
3. Measuring timing between retry attempts
4. Verifying retry intervals match configuration
5. Testing ongoing phase defense (first conflict defended, second within 10s triggers retreat)

Usage:
    python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0
    python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --retry-delay 10000 --max-attempts 5
    python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --test-ongoing

Requirements:
    pip install scapy

Author: Adam G. Sweeney <agsweeney@gmail.com>
License: MIT
"""

import argparse
import time
import sys
import os
from datetime import datetime, timedelta
from collections import deque
from scapy.all import ARP, Ether, sendp, sniff, get_if_list
import random

# Fix Windows console encoding
if sys.platform == 'win32':
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'strict')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'strict')


class RetryTimingTest:
    def __init__(self, interface, target_ip, esp32_mac, expected_retry_delay_ms=10000, max_attempts=5, test_duration=120, test_ongoing=False):
        self.interface = interface
        self.target_ip = target_ip
        self.esp32_mac = esp32_mac.lower()
        self.expected_retry_delay_ms = expected_retry_delay_ms
        self.max_attempts = max_attempts
        self.test_duration = test_duration
        self.test_ongoing = test_ongoing
        
        # Generate a fake MAC for the conflicting device
        self.conflict_mac = f"02:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}:{random.randint(0, 255):02x}"
        
        # Tracking
        self.probe_times = []  # List of (timestamp, attempt_number)
        self.retry_intervals = []  # List of intervals between retries (ms)
        self.conflicts_triggered = 0
        self.start_time = None
        self.last_probe_time = None
        self.current_attempt = 0
        
        # Ongoing phase tracking
        self.ip_acquired = False  # Has device acquired IP?
        self.ip_acquired_time = None
        self.ongoing_conflicts_sent = 0
        self.first_conflict_time = None
        self.defensive_arps_detected = 0
        self.retreat_detected = False
        
    def send_conflict_reply(self, probe_mac):
        """Send ARP reply claiming the IP to trigger conflict"""
        arp = ARP(
            op=2,  # ARP reply
            psrc=self.target_ip,  # Source IP (the IP we're claiming)
            pdst=self.target_ip,  # Target IP
            hwsrc=self.conflict_mac,  # Source MAC (conflicting device)
            hwdst=probe_mac  # Target MAC (ESP32)
        )
        
        packet = Ether(dst=probe_mac, src=self.conflict_mac) / arp
        sendp(packet, iface=self.interface, verbose=False)
        self.conflicts_triggered += 1
    
    def send_ongoing_conflict(self):
        """Send ARP announcement claiming the IP during ongoing phase"""
        # Send ARP announcement (op=2) claiming the IP
        arp = ARP(
            op=2,  # ARP reply/announcement
            psrc=self.target_ip,  # Source IP (the IP we're claiming)
            pdst=self.target_ip,  # Target IP (same)
            hwsrc=self.conflict_mac,  # Source MAC (conflicting device)
            hwdst="ff:ff:ff:ff:ff:ff"  # Broadcast
        )
        
        packet = Ether(dst="ff:ff:ff:ff:ff:ff", src=self.conflict_mac) / arp
        sendp(packet, iface=self.interface, verbose=False)
        self.conflicts_triggered += 1
        self.ongoing_conflicts_sent += 1
        
    def handle_arp_packet(self, packet):
        """Handle incoming ARP packets"""
        if not packet.haslayer(ARP):
            return
            
        arp = packet[ARP]
        packet_mac = str(arp.hwsrc).lower()
        psrc_str = str(arp.psrc)
        pdst_str = str(arp.pdst)
        current_time = time.time()
        
        # Check for defensive ARP announcements/probes from device's own IP (ongoing phase)
        if (packet_mac == self.esp32_mac and 
            psrc_str == self.target_ip and
            (arp.op == 1 or arp.op == 2)):  # ARP request (probe) or reply (announcement)
            
            if not self.ip_acquired:
                # Device has acquired IP and is sending defensive ARPs
                self.ip_acquired = True
                self.ip_acquired_time = current_time
                elapsed = current_time - self.start_time if self.start_time else 0
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] *** IP ACQUIRED - Device entered ongoing phase ***")
                print(f"  Time to acquisition: {elapsed:.1f}s")
                
                if self.test_ongoing:
                    # Use a separate thread or async approach to send conflicts without blocking
                    import threading
                    def send_conflicts():
                        print(f"  Starting ongoing phase defense test...")
                        print(f"  Waiting 2 seconds before sending first conflict...")
                        time.sleep(2)
                        print(f"  [{datetime.now().strftime('%H:%M:%S')}] Sending FIRST conflict (should be defended)")
                        self.first_conflict_time = time.time()
                        self.send_ongoing_conflict()
                        print(f"  [{datetime.now().strftime('%H:%M:%S')}] Waiting 5 seconds before sending SECOND conflict...")
                        # Schedule second conflict in 5 seconds (within DEFEND_INTERVAL of 10s)
                        time.sleep(5)
                        print(f"  [{datetime.now().strftime('%H:%M:%S')}] Sending SECOND conflict within DEFEND_INTERVAL (should trigger retreat)")
                        self.send_ongoing_conflict()
                        print(f"  [{datetime.now().strftime('%H:%M:%S')}] Monitoring for retreat (device should remove IP and start retry)...")
                    
                    # Start conflict sending in background thread
                    conflict_thread = threading.Thread(target=send_conflicts, daemon=True)
                    conflict_thread.start()
            
            self.defensive_arps_detected += 1
            return
        
        # Check if this is an ARP probe from our ESP32 device (probe phase)
        if (arp.op == 1 and  # ARP request
            psrc_str == "0.0.0.0" and  # Source IP is 0.0.0.0 (probe)
            pdst_str == self.target_ip and  # Target is our IP
            packet_mac == self.esp32_mac):  # From ESP32 MAC
            
            if self.start_time is None:
                self.start_time = current_time
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Test started")
            
            # If we were in ongoing phase and now see probes from 0.0.0.0, device retreated
            if self.ip_acquired and not self.retreat_detected:
                self.retreat_detected = True
                retreat_time = current_time - self.first_conflict_time if self.first_conflict_time else 0
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] *** RETREAT DETECTED - Device removed IP and started retry ***")
                print(f"  Time from first conflict: {retreat_time:.1f}s")
                print(f"  Device is now retrying (sending probes from 0.0.0.0)")
                self.ip_acquired = False  # Reset for next cycle
                # After retreat, we can respond to probes again (retry phase)
                # Fall through to probe handling below
            
            # If testing ongoing phase, don't respond to probe phase probes
            # We want device to acquire IP first, then test ongoing defense
            if self.test_ongoing and not self.ip_acquired and not self.retreat_detected:
                # Just log the probe but don't respond - let device acquire IP
                print(f"[{datetime.now().strftime('%H:%M:%S')}] Probe phase probe detected (not responding - waiting for IP acquisition)")
                return False
            
            # Calculate time since start
            elapsed = current_time - self.start_time
            
            # Check if this is a new retry attempt
            if self.last_probe_time is not None:
                interval_ms = (current_time - self.last_probe_time) * 1000
                self.retry_intervals.append(interval_ms)
                
                # Determine if this is a new attempt or part of probe sequence
                # If interval is > 5 seconds, it's likely a new retry attempt
                if interval_ms > 5000:
                    self.current_attempt += 1
                    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Retry Attempt #{self.current_attempt} detected")
                    print(f"  Time since last probe: {interval_ms:.0f}ms (expected: ~{self.expected_retry_delay_ms}ms)")
                    
                    # Check if timing matches expected retry delay (allow ±20% tolerance)
                    tolerance = self.expected_retry_delay_ms * 0.2
                    if abs(interval_ms - self.expected_retry_delay_ms) <= tolerance:
                        print(f"  [OK] Timing matches expected retry delay")
                    else:
                        print(f"  [FAIL] Timing does NOT match expected retry delay (difference: {abs(interval_ms - self.expected_retry_delay_ms):.0f}ms)")
                else:
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] Probe #{len(self.probe_times) + 1} in attempt #{self.current_attempt + 1} (interval: {interval_ms:.0f}ms)")
            else:
                self.current_attempt = 1
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Initial Attempt #{self.current_attempt} detected")
            
            # Record probe time
            self.probe_times.append((current_time, self.current_attempt))
            self.last_probe_time = current_time
            
            # Send conflict reply (only if not in ongoing test mode waiting for IP acquisition)
            if not (self.test_ongoing and not self.ip_acquired and not self.retreat_detected):
                print(f"  -> Sending conflict reply (MAC: {self.conflict_mac})")
                self.send_conflict_reply(self.esp32_mac)
            
            # Check if we've exceeded max attempts
            if self.max_attempts > 0 and self.current_attempt >= self.max_attempts:
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Maximum attempts ({self.max_attempts}) reached")
                return True  # Signal to stop
            
        return False
    
    def run(self):
        """Run the timed test"""
        print("=" * 70)
        print("ACD Retry Logic Timing Test")
        print("=" * 70)
        print(f"Target IP: {self.target_ip}")
        print(f"ESP32 MAC: {self.esp32_mac}")
        print(f"Conflict MAC: {self.conflict_mac}")
        print(f"Expected Retry Delay: {self.expected_retry_delay_ms}ms ({self.expected_retry_delay_ms/1000:.1f}s)")
        print(f"Max Attempts: {self.max_attempts if self.max_attempts > 0 else 'Unlimited'}")
        print(f"Test Duration: {self.test_duration}s")
        print(f"Test Ongoing Phase: {'Yes' if self.test_ongoing else 'No (probe phase only)'}")
        print("=" * 70)
        if self.test_ongoing:
            print("\nTesting ongoing phase defense:")
            print("  1. Wait for device to acquire IP")
            print("  2. Send first conflict (should be defended)")
            print("  3. Send second conflict within 10s (should trigger retreat)")
            print("  4. Verify device removes IP and starts retry")
        else:
            print("\nTesting probe phase retry:")
            print("  - Responding to all probes to trigger conflicts")
            print("  - Measuring retry timing")
        print("\nWaiting for ARP traffic from ESP32 device...")
        print("(Make sure the ESP32 is configured with static IP and ACD retry enabled)")
        print("Press Ctrl+C to stop early\n")
        
        stop_reason = None
        
        try:
            # Sniff for ARP packets
            start_sniff_time = time.time()
            sniff(
                iface=self.interface,
                filter="arp",
                prn=lambda p: self.handle_arp_packet(p),
                timeout=self.test_duration,
                stop_filter=lambda p: False,
                store=False
            )
            
            # Check if we stopped due to timeout
            if time.time() - start_sniff_time >= self.test_duration:
                stop_reason = "Test duration expired"
            else:
                stop_reason = "User interrupt"
                
        except KeyboardInterrupt:
            stop_reason = "User interrupt"
        except Exception as e:
            print(f"\nError during test: {e}")
            import traceback
            traceback.print_exc()
            return
        
        # Generate report
        self.generate_report(stop_reason)
    
    def generate_report(self, stop_reason):
        """Generate test report"""
        print("\n" + "=" * 70)
        print("TEST REPORT")
        print("=" * 70)
        
        if len(self.probe_times) == 0:
            print("\nWARNING: No ARP probes detected from ESP32 device!")
            print("\nPossible issues:")
            print("  - ESP32 MAC address incorrect")
            print("  - ESP32 not configured for static IP")
            print("  - ACD not enabled")
            print("  - ESP32 not attempting to acquire IP")
            print("  - Network interface incorrect")
            return
        
        total_time = time.time() - self.start_time if self.start_time else 0
        
        print(f"\nTest Duration: {total_time:.1f}s")
        print(f"Stop Reason: {stop_reason}")
        print(f"\nTotal Probes Detected: {len(self.probe_times)}")
        print(f"Conflicts Triggered: {self.conflicts_triggered}")
        print(f"Retry Attempts Detected: {self.current_attempt}")
        
        if self.test_ongoing:
            print(f"\nOngoing Phase Test Results:")
            print(f"  IP Acquired: {'Yes' if self.ip_acquired_time else 'No'}")
            if self.ip_acquired_time:
                acquisition_time = self.ip_acquired_time - self.start_time if self.start_time else 0
                print(f"  Time to Acquisition: {acquisition_time:.1f}s")
            print(f"  Defensive ARPs Detected: {self.defensive_arps_detected}")
            print(f"  Ongoing Conflicts Sent: {self.ongoing_conflicts_sent}")
            print(f"  Retreat Detected: {'Yes' if self.retreat_detected else 'No'}")
            if self.first_conflict_time and self.retreat_detected:
                retreat_time = self.ip_acquired_time - self.first_conflict_time if self.ip_acquired_time else 0
                print(f"  Time from First Conflict to Retreat: {retreat_time:.1f}s")
                if retreat_time > 0 and retreat_time < 15:
                    print(f"  [OK] Retreat occurred within expected timeframe (< 15s)")
                else:
                    print(f"  [WARN] Retreat timing may be unexpected")
        
        if len(self.retry_intervals) > 0:
            print(f"\nRetry Intervals (ms):")
            for i, interval in enumerate(self.retry_intervals, 1):
                tolerance = self.expected_retry_delay_ms * 0.2
                match = "[OK]" if abs(interval - self.expected_retry_delay_ms) <= tolerance else "[FAIL]"
                print(f"  Interval {i}: {interval:.0f}ms {match}")
            
            avg_interval = sum(self.retry_intervals) / len(self.retry_intervals)
            min_interval = min(self.retry_intervals)
            max_interval = max(self.retry_intervals)
            
            print(f"\nRetry Interval Statistics:")
            print(f"  Average: {avg_interval:.0f}ms")
            print(f"  Minimum: {min_interval:.0f}ms")
            print(f"  Maximum: {max_interval:.0f}ms")
            print(f"  Expected: {self.expected_retry_delay_ms}ms")
            
            # Check if retry logic is working
            tolerance = self.expected_retry_delay_ms * 0.2
            matches = sum(1 for iv in self.retry_intervals if abs(iv - self.expected_retry_delay_ms) <= tolerance)
            match_percentage = (matches / len(self.retry_intervals)) * 100
            
            print(f"\nRetry Timing Accuracy: {matches}/{len(self.retry_intervals)} intervals match ({match_percentage:.1f}%)")
            
            if match_percentage >= 80:
                print("  [OK] Retry logic appears to be working correctly")
            else:
                print("  [FAIL] Retry timing does not match expected delay")
        
        # Check max attempts
        if self.max_attempts > 0:
            if self.current_attempt >= self.max_attempts:
                print(f"\n[OK] Maximum attempts ({self.max_attempts}) reached as expected")
            elif self.current_attempt < self.max_attempts:
                print(f"\n[WARN] Only {self.current_attempt} attempts detected (expected {self.max_attempts})")
                print("  (Test may have ended before max attempts reached)")
        
        # Probe timing analysis
        if len(self.probe_times) > 3:
            print(f"\nProbe Sequence Analysis:")
            # Group probes by attempt
            attempts = {}
            for probe_time, attempt_num in self.probe_times:
                if attempt_num not in attempts:
                    attempts[attempt_num] = []
                attempts[attempt_num].append(probe_time)
            
            for attempt_num in sorted(attempts.keys()):
                probe_times = attempts[attempt_num]
                if len(probe_times) >= 2:
                    intervals = [(probe_times[i+1] - probe_times[i]) * 1000 
                                for i in range(len(probe_times) - 1)]
                    avg_probe_interval = sum(intervals) / len(intervals)
                    print(f"  Attempt #{attempt_num}: {len(probe_times)} probes, avg interval: {avg_probe_interval:.0f}ms")
        
        print("\n" + "=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="Test ACD retry logic timing",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic test with default settings
  python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0

  # Test with specific retry delay and max attempts
  python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --retry-delay 10000 --max-attempts 5

  # Extended test duration
  python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --duration 300

  # Test ongoing phase defense (first conflict defended, second triggers retreat)
  python test_acd_retry_timing.py --ip 172.16.82.100 --esp32-mac 30:ed:a0:e3:34:c1 --interface eth0 --test-ongoing
        """
    )
    
    parser.add_argument(
        "--ip",
        required=True,
        help="IP address that ESP32 is trying to acquire (e.g., 172.16.82.100)"
    )
    
    parser.add_argument(
        "--esp32-mac",
        required=True,
        help="MAC address of ESP32 device (e.g., 30:ed:a0:e3:34:c1)"
    )
    
    parser.add_argument(
        "--interface",
        required=True,
        help="Network interface name (e.g., eth0, Ethernet, en0)"
    )
    
    parser.add_argument(
        "--retry-delay",
        type=int,
        default=10000,
        help="Expected retry delay in milliseconds (default: 10000)"
    )
    
    parser.add_argument(
        "--max-attempts",
        type=int,
        default=5,
        help="Expected maximum retry attempts (default: 5, 0 = unlimited)"
    )
    
    parser.add_argument(
        "--duration",
        type=int,
        default=120,
        help="Test duration in seconds (default: 120)"
    )
    
    parser.add_argument(
        "--test-ongoing",
        action="store_true",
        help="Test ongoing phase defense (first conflict defended, second triggers retreat)"
    )
    
    args = parser.parse_args()
    
    # Validate interface
    interfaces = get_if_list()
    if args.interface not in interfaces:
        print(f"Warning: Interface '{args.interface}' not found in list:")
        for iface in interfaces:
            print(f"  - {iface}")
        print("\nTrying anyway...")
    
    # Normalize MAC address
    args.esp32_mac = args.esp32_mac.lower().replace('-', ':')
    
    # Validate MAC format
    import re
    if not re.match(r'^([0-9a-f]{2}[:-]){5}([0-9a-f]{2})$', args.esp32_mac):
        print(f"Error: Invalid MAC address format: {args.esp32_mac}")
        print("Expected format: XX:XX:XX:XX:XX:XX")
        sys.exit(1)
    
    # Run test
    test = RetryTimingTest(
        interface=args.interface,
        target_ip=args.ip,
        esp32_mac=args.esp32_mac,
        expected_retry_delay_ms=args.retry_delay,
        max_attempts=args.max_attempts,
        test_duration=args.duration,
        test_ongoing=args.test_ongoing
    )
    
    test.run()


if __name__ == "__main__":
    main()

