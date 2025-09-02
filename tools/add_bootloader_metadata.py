#!/usr/bin/env python3
"""
Simple script to add bootloader requirement metadata to WLED binary files.
This adds a metadata tag that the OTA handler can detect.

Usage: python add_bootloader_metadata.py <binary_file> <required_version>
Example: python add_bootloader_metadata.py firmware.bin 4
"""

import sys
import os

def add_bootloader_metadata(binary_file, required_version):
    """Add bootloader metadata to a binary file"""
    if not os.path.exists(binary_file):
        print(f"Error: File {binary_file} does not exist")
        return False
    
    # Validate version
    try:
        version = int(required_version)
        if version < 1 or version > 9:
            print("Error: Bootloader version must be between 1 and 9")
            return False
    except ValueError:
        print("Error: Bootloader version must be a number")
        return False
    
    # Create metadata string
    metadata = f"WLED_BOOTLOADER:{version}"
    
    # Append metadata to file
    try:
        with open(binary_file, 'ab') as f:
            f.write(metadata.encode('ascii'))
        print(f"Successfully added bootloader v{version} requirement to {binary_file}")
        return True
    except Exception as e:
        print(f"Error writing to file: {e}")
        return False

def main():
    if len(sys.argv) != 3:
        print("Usage: python add_bootloader_metadata.py <binary_file> <required_version>")
        print("Example: python add_bootloader_metadata.py firmware.bin 4")
        sys.exit(1)
    
    binary_file = sys.argv[1]
    required_version = sys.argv[2]
    
    if add_bootloader_metadata(binary_file, required_version):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()