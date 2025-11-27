#!/usr/bin/env python3
"""
Analyze ARP Timing from Wireshark Export

This script helps calculate the interval between ARP probes/announcements
from a specific device by analyzing packet timestamps.

Usage:
    1. In Wireshark, filter for the device you want to analyze:
       arp.src.hw_mac == 08:89:61:XX:XX:XX  (for Rockwell)
    2. Export packets as CSV: File -> Export Packet Dissections -> As CSV
    3. Run this script: python analyze_arp_timing.py <wireshark_export.csv>

Or manually provide timestamps:
    python analyze_arp_timing.py --timestamps 10.811 50.234 90.456 130.678
"""

import argparse
import csv
import sys
from typing import List, Optional


def calculate_intervals(timestamps: List[float]) -> List[float]:
    """Calculate intervals between consecutive timestamps"""
    if len(timestamps) < 2:
        return []
    
    intervals = []
    for i in range(1, len(timestamps)):
        interval = timestamps[i] - timestamps[i-1]
        intervals.append(interval)
    
    return intervals


def analyze_csv(filename: str, mac_filter: Optional[str] = None) -> List[float]:
    """Parse Wireshark CSV export and extract timestamps"""
    timestamps = []
    
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Check MAC filter if provided
                if mac_filter:
                    source = row.get('Source', '')
                    if mac_filter.lower() not in source.lower():
                        continue
                
                # Extract timestamp (format varies by Wireshark version)
                time_str = row.get('Time', '') or row.get('Time (relative)', '')
                try:
                    timestamp = float(time_str)
                    timestamps.append(timestamp)
                except ValueError:
                    continue
    
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading CSV: {e}")
        sys.exit(1)
    
    return timestamps


def main():
    parser = argparse.ArgumentParser(
        description="Analyze ARP timing intervals from Wireshark capture"
    )
    
    parser.add_argument(
        'csv_file',
        nargs='?',
        help='Wireshark CSV export file'
    )
    
    parser.add_argument(
        '--timestamps',
        nargs='+',
        type=float,
        help='Manual timestamps (space-separated)'
    )
    
    parser.add_argument(
        '--mac',
        help='Filter by MAC address (partial match, e.g., "08:89:61" for Rockwell)'
    )
    
    args = parser.parse_args()
    
    if args.timestamps:
        timestamps = sorted(args.timestamps)
    elif args.csv_file:
        timestamps = analyze_csv(args.csv_file, args.mac)
        if not timestamps:
            print("No matching packets found")
            sys.exit(1)
    else:
        parser.print_help()
        sys.exit(1)
    
    if len(timestamps) < 2:
        print("Need at least 2 timestamps to calculate intervals")
        sys.exit(1)
    
    intervals = calculate_intervals(timestamps)
    
    print(f"\nAnalyzed {len(timestamps)} packets")
    print(f"Time range: {timestamps[0]:.3f}s to {timestamps[-1]:.3f}s")
    print(f"Total duration: {timestamps[-1] - timestamps[0]:.3f}s")
    print(f"\nIntervals between packets:")
    
    for i, interval in enumerate(intervals, 1):
        print(f"  Interval {i}: {interval:.3f}s ({interval*1000:.0f}ms)")
    
    if intervals:
        avg_interval = sum(intervals) / len(intervals)
        min_interval = min(intervals)
        max_interval = max(intervals)
        
        print(f"\nStatistics:")
        print(f"  Average interval: {avg_interval:.3f}s ({avg_interval*1000:.0f}ms)")
        print(f"  Minimum interval: {min_interval:.3f}s ({min_interval*1000:.0f}ms)")
        print(f"  Maximum interval: {max_interval:.3f}s ({max_interval*1000:.0f}ms)")
        
        # Check if intervals are consistent (within 10% of average)
        consistent = all(abs(i - avg_interval) / avg_interval < 0.1 for i in intervals)
        if consistent:
            print(f"\n[OK] Intervals are consistent - recommended value: {avg_interval*1000:.0f}ms")
        else:
            print(f"\n[WARN] Intervals vary significantly - may need to investigate")


if __name__ == "__main__":
    main()

