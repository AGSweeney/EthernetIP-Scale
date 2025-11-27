#!/usr/bin/env python3
"""
Script to copy firmware binary to FirmwareImages folder with unique name.
Called as a POST_BUILD command in CMake.
"""
import os
import sys
import shutil
from datetime import datetime
import subprocess

def get_git_hash():
    """Get short git commit hash if available."""
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--short=7', 'HEAD'],
            capture_output=True,
            text=True,
            check=True,
            cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None

def main():
    if len(sys.argv) < 3:
        print("Usage: copy_firmware.py <source_binary> <firmware_images_dir> [project_name]")
        sys.exit(1)
    
    source_binary = sys.argv[1]
    firmware_images_dir = sys.argv[2]
    project_name = sys.argv[3] if len(sys.argv) > 3 else "ENIP-Scale"
    
    # Wait for binary to be created (it's generated after linking)
    # ESP-IDF generates the binary file after the elf is linked, so we need to wait
    import time
    max_retries = 20  # Increased retries to allow more time for binary generation
    retry_delay = 0.5  # seconds
    
    for attempt in range(max_retries):
        if os.path.exists(source_binary):
            break
        if attempt < max_retries - 1:
            time.sleep(retry_delay)
        else:
            print(f"ERROR: Source binary not found after {max_retries} attempts: {source_binary}")
            print("This may happen if the binary generation step hasn't completed yet.")
            print(f"Current working directory: {os.getcwd()}")
            print(f"Binary directory exists: {os.path.exists(os.path.dirname(source_binary))}")
            # List files in binary directory to help debug
            if os.path.exists(os.path.dirname(source_binary)):
                print(f"Files in binary directory:")
                try:
                    for f in os.listdir(os.path.dirname(source_binary)):
                        print(f"  {f}")
                except Exception as e:
                    print(f"  Error listing directory: {e}")
            sys.exit(1)  # Fail the build so we know something is wrong
    
    # Create FirmwareImages directory if it doesn't exist
    os.makedirs(firmware_images_dir, exist_ok=True)
    
    # Remove old firmware versions before copying new one
    # This keeps the FirmwareImages directory clean and prevents it from growing too large
    try:
        firmware_files = []
        if os.path.exists(firmware_images_dir):
            for filename in os.listdir(firmware_images_dir):
                filepath = os.path.join(firmware_images_dir, filename)
                # Only process .bin files that match the project name pattern
                if filename.endswith('.bin') and filename.startswith(project_name):
                    firmware_files.append(filepath)
        
        if firmware_files:
            print(f"Removing {len(firmware_files)} old firmware file(s)...")
            for filepath in firmware_files:
                try:
                    os.remove(filepath)
                    print(f"  Removed: {os.path.basename(filepath)}")
                except Exception as e:
                    print(f"  Warning: Failed to remove {os.path.basename(filepath)}: {e}")
    except Exception as e:
        print(f"Warning: Error while cleaning old firmware files: {e}")
        # Continue anyway - don't fail the build if cleanup fails
    
    # Generate timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Try to get git commit hash
    git_hash = get_git_hash()
    
    # Construct filename
    if git_hash:
        dest_filename = f"{project_name}_{timestamp}_{git_hash}.bin"
    else:
        dest_filename = f"{project_name}_{timestamp}.bin"
    
    dest_binary = os.path.join(firmware_images_dir, dest_filename)
    
    # Copy the binary
    try:
        shutil.copy2(source_binary, dest_binary)
        print(f"Firmware copied successfully: {dest_filename}")
        print(f"  Source: {source_binary}")
        print(f"  Destination: {dest_binary}")
    except Exception as e:
        print(f"Error copying firmware: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

