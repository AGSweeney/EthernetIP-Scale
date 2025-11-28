#!/usr/bin/env python3
"""
Embed compressed icon file (EDSCollection.gz) into C source files.
This script reads a gzip-compressed icon file and generates C source and header files
for embedding it into the firmware.
"""

import sys
import os

def embed_icon_gz(input_file, header_file, source_file):
    """Read the compressed icon file and generate C source/header files."""
    
    if not os.path.exists(input_file):
        print(f"Warning: Input file '{input_file}' not found. Creating empty icon files.", file=sys.stderr)
        # Create empty files
        with open(header_file, 'w') as f:
            f.write("""#ifndef EMBEDDED_ICON_FILE_H_
#define EMBEDDED_ICON_FILE_H_

#include <stddef.h>
#include <stdint.h>

extern const uint8_t embedded_icon_file_data[];
extern const size_t embedded_icon_file_size;

#endif /* EMBEDDED_ICON_FILE_H_ */
""")
        
        with open(source_file, 'w') as f:
            f.write("""#include "embedded_icon_file.h"

const uint8_t embedded_icon_file_data[] = {};
const size_t embedded_icon_file_size = 0;
""")
        return
    
    # Read the compressed icon file
    with open(input_file, 'rb') as f:
        icon_data = f.read()
    
    icon_size = len(icon_data)
    
    # Generate header file
    with open(header_file, 'w') as f:
        f.write("""#ifndef EMBEDDED_ICON_FILE_H_
#define EMBEDDED_ICON_FILE_H_

#include <stddef.h>
#include <stdint.h>

extern const uint8_t embedded_icon_file_data[];
extern const size_t embedded_icon_file_size;

#endif /* EMBEDDED_ICON_FILE_H_ */
""")
    
    # Generate source file
    with open(source_file, 'w') as f:
        f.write(f"""#include "embedded_icon_file.h"

const uint8_t embedded_icon_file_data[] = {{
""")
        
        # Write data in hex format, 16 bytes per line
        for i in range(0, icon_size, 16):
            chunk = icon_data[i:i+16]
            hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
            if i + 16 < icon_size:
                f.write(f"  {hex_values},\n")
            else:
                f.write(f"  {hex_values}\n")
        
        f.write(f"""}};

const size_t embedded_icon_file_size = {icon_size};
""")
    
    print(f"Generated embedded icon file: {icon_size} bytes from '{input_file}'")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input_gz_file> <output_header.h> <output_source.c>", file=sys.stderr)
        sys.exit(1)
    
    embed_icon_gz(sys.argv[1], sys.argv[2], sys.argv[3])

