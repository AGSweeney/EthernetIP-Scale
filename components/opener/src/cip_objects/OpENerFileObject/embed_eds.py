#!/usr/bin/env python3
"""
Script to convert EDS file to embedded C array for ESP-IDF build.
This allows the EDS file to be embedded directly in flash without SPIFFS.
"""

import sys
import os

def convert_eds_to_c_array(eds_file_path, output_header_path, output_source_path):
    """Convert EDS file to C array embedded in code."""
    
    # Read the EDS file
    with open(eds_file_path, 'rb') as f:
        eds_data = f.read()
    
    # Generate header file
    header_content = f"""/*******************************************************************************
 * Auto-generated file - DO NOT EDIT
 * This file contains the embedded EDS file data
 ******************************************************************************/

#ifndef EMBEDDED_EDS_FILE_H_
#define EMBEDDED_EDS_FILE_H_

#include <stdint.h>
#include <stddef.h>

extern const uint8_t embedded_eds_file_data[];
extern const size_t embedded_eds_file_size;

#endif /* EMBEDDED_EDS_FILE_H_ */
"""
    
    # Generate source file with the data
    source_content = f"""/*******************************************************************************
 * Auto-generated file - DO NOT EDIT
 * This file contains the embedded EDS file data
 ******************************************************************************/

#include "embedded_eds_file.h"

const uint8_t embedded_eds_file_data[] = {{
"""
    
    # Write data as hex bytes, 16 per line
    for i in range(0, len(eds_data), 16):
        line_bytes = eds_data[i:i+16]
        hex_bytes = ', '.join(f'0x{b:02x}' for b in line_bytes)
        source_content += f"    {hex_bytes}"
        if i + 16 < len(eds_data):
            source_content += ","
        source_content += "\n"
    
    source_content += f"""}};

const size_t embedded_eds_file_size = {len(eds_data)};
"""
    
    # Write header file
    with open(output_header_path, 'w') as f:
        f.write(header_content)
    
    # Write source file
    with open(output_source_path, 'w') as f:
        f.write(source_content)
    
    print(f"Converted {len(eds_data)} bytes from {eds_file_path}")
    print(f"  Header: {output_header_path}")
    print(f"  Source: {output_source_path}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: embed_eds.py <eds_file> <output_header> <output_source>")
        sys.exit(1)
    
    convert_eds_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])

