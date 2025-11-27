#!/usr/bin/env python3
"""
Convert Wireshark PDML (Packet Details Markup Language) export to concise Markdown format.

Usage:
    python pdml_to_markdown.py input.pdml output.md
"""

import xml.etree.ElementTree as ET
import sys
from datetime import datetime

def get_field_value(proto, field_name, default=''):
    """Get a field value from a protocol."""
    field = proto.find(f'.//field[@name="{field_name}"]')
    return field.get('show', default) if field is not None else default

def get_field_value_nested(proto, field_names, default=''):
    """Get a nested field value."""
    current = proto
    for name in field_names:
        field = current.find(f'.//field[@name="{name}"]')
        if field is None:
            return default
        current = field
    return current.get('show', default)

def describe_packet(packet):
    """Generate a human-readable description of what the packet is doing."""
    descriptions = []
    
    # Check for ARP
    arp_proto = packet.find('.//proto[@name="arp"]')
    if arp_proto is not None:
        opcode = get_field_value(arp_proto, 'arp.opcode', '')
        is_probe = get_field_value(arp_proto, 'arp.isprobe', '')
        src_ip = get_field_value(arp_proto, 'arp.src.proto_ipv4', '')
        dst_ip = get_field_value(arp_proto, 'arp.dst.proto_ipv4', '')
        src_mac = get_field_value(arp_proto, 'arp.src.hw_mac', '')
        
        if is_probe == 'True':
            descriptions.append(f"**ARP Probe** from `0.0.0.0` asking \"Who has {dst_ip}?\"")
        elif opcode == '1' and src_ip == dst_ip:
            descriptions.append(f"**ARP Announcement** claiming IP `{src_ip}`")
        elif opcode == '1':
            descriptions.append(f"**ARP Request** from `{src_ip}` asking \"Who has {dst_ip}?\"")
        elif opcode == '2':
            descriptions.append(f"**ARP Reply** from `{src_ip}` responding to ARP request")
    
    # Check for IP
    ip_proto = packet.find('.//proto[@name="ip"]')
    if ip_proto is not None:
        src_ip = get_field_value(ip_proto, 'ip.src', '')
        dst_ip = get_field_value(ip_proto, 'ip.dst', '')
        proto = get_field_value(ip_proto, 'ip.proto', '')
        
        # Check for TCP
        tcp_proto = packet.find('.//proto[@name="tcp"]')
        if tcp_proto is not None:
            src_port = get_field_value(tcp_proto, 'tcp.srcport', '')
            dst_port = get_field_value(tcp_proto, 'tcp.dstport', '')
            flags = get_field_value(tcp_proto, 'tcp.flags', '')
            
            if dst_port == '44818':
                descriptions.append(f"**EtherNet/IP** TCP connection to `{dst_ip}:{dst_port}`")
            elif dst_port == '502':
                descriptions.append(f"**Modbus TCP** connection to `{dst_ip}:{dst_port}`")
            elif dst_port == '80' or src_port == '80':
                descriptions.append(f"**HTTP** connection to `{dst_ip}:{dst_port}`")
            else:
                descriptions.append(f"**TCP** `{src_ip}:{src_port}` → `{dst_ip}:{dst_port}`")
        
        # Check for UDP
        udp_proto = packet.find('.//proto[@name="udp"]')
        if udp_proto is not None:
            src_port = get_field_value(udp_proto, 'udp.srcport', '')
            dst_port = get_field_value(udp_proto, 'udp.dstport', '')
            
            if dst_port == '2222':
                descriptions.append(f"**EtherNet/IP** UDP to `{dst_ip}:{dst_port}`")
            elif dst_port == '53' or src_port == '53':
                descriptions.append(f"**DNS** query/response")
            else:
                descriptions.append(f"**UDP** `{src_ip}:{src_port}` → `{dst_ip}:{dst_port}`")
        
        # Check for ICMP
        icmp_proto = packet.find('.//proto[@name="icmp"]')
        if icmp_proto is not None:
            descriptions.append(f"**ICMP** `{src_ip}` → `{dst_ip}`")
    
    # Check for Ethernet
    eth_proto = packet.find('.//proto[@name="eth"]')
    if eth_proto is not None:
        dst_mac = get_field_value(eth_proto, 'eth.dst', '')
        src_mac = get_field_value(eth_proto, 'eth.src', '')
        
        if dst_mac == 'ff:ff:ff:ff:ff:ff':
            descriptions.append(f"**Broadcast** frame from `{src_mac}`")
    
    if not descriptions:
        protocols_field = packet.find('.//field[@name="frame.protocols"]')
        protocols = protocols_field.get('show', '') if protocols_field is not None else ''
        descriptions.append(f"**Unknown** packet - Protocols: `{protocols}`")
    
    return ' | '.join(descriptions)

def format_packet_concise(packet, packet_num):
    """Format a single packet as concise markdown."""
    # Extract general info
    geninfo = packet.find('.//proto[@name="geninfo"]')
    frame_info = packet.find('.//proto[@name="frame"]')
    
    packet_num_field = geninfo.find('.//field[@name="num"]') if geninfo is not None else None
    timestamp_field = geninfo.find('.//field[@name="timestamp"]') if geninfo is not None else None
    len_field = geninfo.find('.//field[@name="len"]') if geninfo is not None else None
    time_delta_field = frame_info.find('.//field[@name="frame.time_delta_displayed"]') if frame_info is not None else None
    if time_delta_field is None:
        time_delta_field = frame_info.find('.//field[@name="frame.time_delta"]') if frame_info is not None else None
    
    packet_number = packet_num_field.get('show', str(packet_num)) if packet_num_field is not None else str(packet_num)
    timestamp = timestamp_field.get('show', '') if timestamp_field is not None else ''
    length = len_field.get('show', '') if len_field is not None else ''
    time_delta = time_delta_field.get('show', '') if time_delta_field is not None else ''
    
    # Get protocols
    protocols_field = frame_info.find('.//field[@name="frame.protocols"]') if frame_info is not None else None
    protocols = protocols_field.get('show', '') if protocols_field is not None else ''
    
    # Generate description
    description = describe_packet(packet)
    
    # Extract key fields for summary - prioritize IP addresses
    key_fields = []
    seen_fields = set()  # Track what we've already added to avoid duplicates
    
    # ARP fields - get IPs from ARP first
    arp_proto = packet.find('.//proto[@name="arp"]')
    arp_src_ip = ''
    arp_dst_ip = ''
    arp_src_mac = ''
    if arp_proto is not None:
        arp_src_ip = get_field_value(arp_proto, 'arp.src.proto_ipv4', '')
        arp_dst_ip = get_field_value(arp_proto, 'arp.dst.proto_ipv4', '')
        arp_src_mac = get_field_value(arp_proto, 'arp.src.hw_mac', '')
        if arp_src_ip:
            key_fields.append(f"Source IP: `{arp_src_ip}`")
            seen_fields.add('src_ip')
        if arp_dst_ip:
            key_fields.append(f"Target IP: `{arp_dst_ip}`")
            seen_fields.add('dst_ip')
        if arp_src_mac:
            key_fields.append(f"Source MAC: `{arp_src_mac}`")
            seen_fields.add('arp_src_mac')
    
    # IP fields - use IP layer if ARP didn't have IPs
    ip_proto = packet.find('.//proto[@name="ip"]')
    if ip_proto is not None:
        ip_src = get_field_value(ip_proto, 'ip.src', '')
        ip_dst = get_field_value(ip_proto, 'ip.dst', '')
        # Always add IPs from IP layer if available (they're more accurate for IP packets)
        if ip_src and 'src_ip' not in seen_fields:
            key_fields.append(f"Source IP: `{ip_src}`")
            seen_fields.add('src_ip')
        elif ip_src and ip_src != arp_src_ip:
            # Replace ARP IP with IP layer IP (more accurate)
            key_fields = [f.replace(f"Source IP: `{arp_src_ip}`", f"Source IP: `{ip_src}`") for f in key_fields]
        if ip_dst and 'dst_ip' not in seen_fields:
            key_fields.append(f"Destination IP: `{ip_dst}`")
            seen_fields.add('dst_ip')
        elif ip_dst and ip_dst != arp_dst_ip:
            # Replace ARP IP with IP layer IP (more accurate)
            key_fields = [f.replace(f"Target IP: `{arp_dst_ip}`", f"Destination IP: `{ip_dst}`") for f in key_fields]
    
    # TCP fields
    tcp_proto = packet.find('.//proto[@name="tcp"]')
    if tcp_proto is not None:
        src_port = get_field_value(tcp_proto, 'tcp.srcport', '')
        dst_port = get_field_value(tcp_proto, 'tcp.dstport', '')
        flags = get_field_value(tcp_proto, 'tcp.flags', '')
        if src_port:
            key_fields.append(f"Source Port: `{src_port}`")
        if dst_port:
            key_fields.append(f"Destination Port: `{dst_port}`")
        if flags:
            key_fields.append(f"TCP Flags: `{flags}`")
    
    # UDP fields
    udp_proto = packet.find('.//proto[@name="udp"]')
    if udp_proto is not None:
        src_port = get_field_value(udp_proto, 'udp.srcport', '')
        dst_port = get_field_value(udp_proto, 'udp.dstport', '')
        if src_port:
            key_fields.append(f"Source Port: `{src_port}`")
        if dst_port:
            key_fields.append(f"Destination Port: `{dst_port}`")
    
    # Ethernet fields - only add if not already added from ARP
    eth_proto = packet.find('.//proto[@name="eth"]')
    if eth_proto is not None:
        eth_src_mac = get_field_value(eth_proto, 'eth.src', '')
        eth_dst_mac = get_field_value(eth_proto, 'eth.dst', '')
        if eth_src_mac and 'arp_src_mac' not in seen_fields and eth_src_mac != arp_src_mac:
            key_fields.append(f"Source MAC: `{eth_src_mac}`")
        if eth_dst_mac:
            key_fields.append(f"Destination MAC: `{eth_dst_mac}`")
    
    # Build markdown
    result = [f"## Packet #{packet_number}"]
    if timestamp:
        # Extract just the time part for brevity
        time_part = timestamp.split(' ')[-1] if ' ' in timestamp else timestamp
        result.append(f"**Time:** {time_part}")
    if time_delta:
        result.append(f"**Delta:** {time_delta} seconds")
    if length:
        result.append(f"**Size:** {length} bytes")
    if protocols:
        result.append(f"**Protocols:** `{protocols}`")
    
    result.append('')
    result.append(f"**Description:** {description}")
    
    if key_fields:
        result.append('')
        result.append("**Key Fields:**")
        for field in key_fields:
            result.append(f"- {field}")
    
    return '\n'.join(result)

def convert_pdml_to_markdown(input_file, output_file):
    """Convert PDML file to Markdown."""
    print(f"Parsing {input_file}...")
    tree = ET.parse(input_file)
    root = tree.getroot()
    
    # Extract metadata
    creator = root.get('creator', 'unknown')
    time_str = root.get('time', '')
    capture_file = root.get('capture_file', '')
    
    # Build markdown document
    md_lines = [
        "# Wireshark Packet Capture Export",
        "",
        f"**Capture File:** `{capture_file}`",
        f"**Creator:** {creator}",
        f"**Export Time:** {time_str}",
        "",
        "---",
        ""
    ]
    
    # Process each packet
    packets = root.findall('packet')
    print(f"Found {len(packets)} packets")
    
    for i, packet in enumerate(packets, 1):
        print(f"Processing packet {i}/{len(packets)}...", end='\r')
        packet_md = format_packet_concise(packet, i)
        md_lines.append(packet_md)
        md_lines.append('')
        md_lines.append('---')
        md_lines.append('')
    
    print(f"\nWriting {output_file}...")
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(md_lines))
    
    print(f"Done! Converted {len(packets)} packets to {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python pdml_to_markdown.py input.pdml output.md")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    convert_pdml_to_markdown(input_file, output_file)

