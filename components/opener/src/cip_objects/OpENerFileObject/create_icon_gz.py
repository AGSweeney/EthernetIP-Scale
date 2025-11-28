#!/usr/bin/env python3
"""
Create a GZ-compressed icon file for the File Object.
This script takes a favicon.ico file, renames it to ESP32P4-EIP.ico,
and creates a GZ-compressed archive (EDSCollection.gz) containing it.
The filename is stored in the GZ header so RSLinx extracts it correctly.
"""

import sys
import os
import gzip
import shutil

def create_icon_gz(input_ico, output_gz):
    """Create a GZ file containing the icon with the correct name."""
    
    if not os.path.exists(input_ico):
        print(f"Error: Input icon file '{input_ico}' not found.", file=sys.stderr)
        sys.exit(1)
    
    # Read the icon file
    with open(input_ico, 'rb') as f:
        icon_data = f.read()
    
    # Create a temporary file with the desired name
    temp_ico = "ESP32P4-EIP.ico"
    with open(temp_ico, 'wb') as f:
        f.write(icon_data)
    
    try:
        # Compress the icon file into a GZ archive with the filename in the header
        # Use gzip with filename parameter to store the filename in the GZ header
        with open(temp_ico, 'rb') as f_in:
            with open(output_gz, 'wb') as f_gz:
                with gzip.GzipFile(filename='ESP32P4-EIP.ico', fileobj=f_gz, mode='wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)
        
        # Get the size of the created GZ file
        gz_size = os.path.getsize(output_gz)
        print(f"Created {output_gz} ({gz_size} bytes) containing ESP32P4-EIP.ico")
    finally:
        # Clean up temporary file
        if os.path.exists(temp_ico):
            os.remove(temp_ico)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_favicon.ico> <output_EDSCollection.gz>", file=sys.stderr)
        sys.exit(1)
    
    create_icon_gz(sys.argv[1], sys.argv[2])

