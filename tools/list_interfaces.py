#!/usr/bin/env python3
from scapy.all import get_if_list, get_if_addr, get_if_hwaddr

interfaces = get_if_list()
print("Available interfaces:")
for iface in interfaces:
    if iface == "\\Device\\NPF_Loopback":
        continue
    try:
        mac = get_if_hwaddr(iface)
        ip = get_if_addr(iface)
        print(f"  {iface}")
        print(f"    MAC: {mac}")
        print(f"    IP:  {ip}")
    except:
        print(f"  {iface} (error getting details)")

