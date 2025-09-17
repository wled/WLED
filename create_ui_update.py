#!/usr/bin/env python3
"""
ArkLights UI Update Package Creator

This script creates update packages for ArkLights UI files that can be uploaded
via the /updateui endpoint on ArkLights devices.

Usage:
    python create_ui_update.py [files...]
    
Example:
    python create_ui_update.py ui/index.html ui/styles.css ui/script.js
"""

import sys
import os
import argparse
from pathlib import Path

def create_text_update_package(files, output_file="arklights_ui_update.txt"):
    """Create a simple text-based update package"""
    print(f"Creating text-based update package: {output_file}")
    
    with open(output_file, 'w', encoding='utf-8') as outfile:
        for file_path in files:
            if os.path.exists(file_path):
                print(f"Adding file: {file_path}")
                
                # Read file content
                with open(file_path, 'r', encoding='utf-8') as infile:
                    content = infile.read()
                
                # Preserve directory structure, but ensure it starts with /
                filename = file_path.replace('\\', '/')  # Normalize path separators
                if not filename.startswith('/'):
                    filename = '/' + filename
                
                # Write in our simple format
                outfile.write(f"FILENAME:{filename}:{content}:ENDFILE\n")
            else:
                print(f"Warning: File not found: {file_path}")
    
    print(f"Update package created: {output_file}")
    print(f"Upload this file via the /updateui endpoint on your ArkLights device")
    print("\nSupported files:")
    print("- Main Interface: ui/index.html, ui/styles.css, ui/script.js")
    print("- Custom Files: Any CSS/JS/HTML files")

def main():
    parser = argparse.ArgumentParser(description='Create ArkLights UI update packages')
    parser.add_argument('files', nargs='+', help='UI files to include in update package')
    parser.add_argument('--output', '-o', default='arklights_ui_update.txt', 
                       help='Output file name (default: arklights_ui_update.txt)')
    
    args = parser.parse_args()
    
    # Validate input files
    valid_files = []
    for file_path in args.files:
        if os.path.exists(file_path):
            valid_files.append(file_path)
        else:
            print(f"Warning: File not found: {file_path}")
    
    if not valid_files:
        print("Error: No valid files found")
        sys.exit(1)
    
    # Create update package
    create_text_update_package(valid_files, args.output)
    
    print("\n" + "="*50)
    print("UPDATE INSTRUCTIONS:")
    print("1. Go to your ArkLights device's web interface")
    print("2. Navigate to /updateui")
    print("3. Upload the created update package")
    print("4. The UI will be updated without requiring a firmware flash!")
    print("="*50)

if __name__ == "__main__":
    main()
