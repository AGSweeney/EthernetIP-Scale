#!/usr/bin/env python3
"""
Script to extract icon from EDS file and convert to embedded C array.
The icon is stored as base64 in the IconContents field.
"""

import sys
import re
import base64

def extract_icon_from_eds(eds_file_path, output_header_path, output_source_path):
    """Extract icon from EDS file and convert to C array."""
    
    # Read the EDS file
    with open(eds_file_path, 'r', encoding='utf-8') as f:
        eds_content = f.read()
    
    # Find IconContents section
    # IconContents can span multiple lines with quoted strings
    icon_match = re.search(
        r'IconContents\s*=\s*((?:\s*"[^"]+"\s*)+)',
        eds_content,
        re.MULTILINE | re.DOTALL
    )
    
    if not icon_match:
        print(f"Warning: IconContents not found in {eds_file_path} - creating empty icon file")
        # Create empty icon files (instance 201 will be FileEmpty)
        header_content = f"""/*******************************************************************************
 * Auto-generated file - DO NOT EDIT
 * No icon file data available (IconContents not found in EDS file)
 ******************************************************************************/

#ifndef EMBEDDED_ICON_FILE_H_
#define EMBEDDED_ICON_FILE_H_

#include <stdint.h>
#include <stddef.h>

/* Empty icon file - IconContents not found in EDS */
extern const uint8_t embedded_icon_file_data[];
extern const size_t embedded_icon_file_size;

#endif /* EMBEDDED_ICON_FILE_H_ */
"""
        source_content = f"""/*******************************************************************************
 * Auto-generated file - DO NOT EDIT
 * No icon file data available (IconContents not found in EDS file)
 ******************************************************************************/

#include "embedded_icon_file.h"

/* Empty icon file array */
const uint8_t embedded_icon_file_data[] = {{}};

const size_t embedded_icon_file_size = 0;
"""
        with open(output_header_path, 'w') as f:
            f.write(header_content)
        with open(output_source_path, 'w') as f:
            f.write(source_content)
        print(f"Created empty icon files (no icon in EDS)")
        print(f"  Header: {output_header_path}")
        print(f"  Source: {output_source_path}")
        return
    
    # Extract all quoted strings and concatenate
    icon_base64 = ''
    for quoted_str in re.findall(r'"([^"]+)"', icon_match.group(1)):
        icon_base64 += quoted_str
    
    # Decode base64 to binary
    try:
        icon_data = base64.b64decode(icon_base64)
    except Exception as e:
        print(f"Error decoding base64 icon data: {e}")
        sys.exit(1)
    
    print(f"Extracted icon: {len(icon_data)} bytes from base64 ({len(icon_base64)} chars)")
    
    # Generate header file
    header_content = f"""/*******************************************************************************
 * Auto-generated file - DO NOT EDIT
 * This file contains the embedded icon file data extracted from the EDS file
 ******************************************************************************/

#ifndef EMBEDDED_ICON_FILE_H_
#define EMBEDDED_ICON_FILE_H_

#include <stdint.h>
#include <stddef.h>

extern const uint8_t embedded_icon_file_data[];
extern const size_t embedded_icon_file_size;

#endif /* EMBEDDED_ICON_FILE_H_ */
"""
    
    # Generate source file with the data
    source_content = f"""/*******************************************************************************
 * Auto-generated file - DO NOT EDIT
 * This file contains the embedded icon file data extracted from the EDS file
 ******************************************************************************/

#include "embedded_icon_file.h"

const uint8_t embedded_icon_file_data[] = {{
"""
    
    # Write data as hex bytes, 16 per line
    for i in range(0, len(icon_data), 16):
        line_bytes = icon_data[i:i+16]
        hex_bytes = ', '.join(f'0x{b:02x}' for b in line_bytes)
        source_content += f"    {hex_bytes}"
        if i + 16 < len(icon_data):
            source_content += ","
        source_content += "\n"
    
    source_content += f"""}};

const size_t embedded_icon_file_size = {len(icon_data)};
"""
    
    # Write header file
    with open(output_header_path, 'w') as f:
        f.write(header_content)
    
    # Write source file
    with open(output_source_path, 'w') as f:
        f.write(source_content)
    
    print(f"  Header: {output_header_path}")
    print(f"  Source: {output_source_path}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: extract_icon.py <eds_file> <output_header> <output_source>")
        sys.exit(1)
    
    extract_icon_from_eds(sys.argv[1], sys.argv[2], sys.argv[3])

