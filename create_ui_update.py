#!/usr/bin/env python3
"""
OneWheel UI Update Package Creator

This script creates update packages for OneWheel UI files that can be uploaded
via the /updateui endpoint on WLED devices.

Usage:
    python create_ui_update.py [files...]
    
Example:
    python create_ui_update.py onewheel.htm welcome.htm imu-debug.html
"""

import sys
import os
import zipfile
import argparse
from pathlib import Path

def create_text_update_package(files, output_file="ui_update.txt"):
    """Create a simple text-based update package"""
    print(f"Creating text-based update package: {output_file}")
    
    with open(output_file, 'w') as outfile:
        for file_path in files:
            if os.path.exists(file_path):
                print(f"Adding file: {file_path}")
                
                # Read file content
                with open(file_path, 'r', encoding='utf-8') as infile:
                    content = infile.read()
                
                # Get filename without path
                filename = os.path.basename(file_path)
                
                # Write in our simple format
                outfile.write(f"FILENAME:{filename}:{content}:ENDFILE\n")
            else:
                print(f"Warning: File not found: {file_path}")
    
    print(f"Update package created: {output_file}")
    print(f"Upload this file via the /updateui endpoint on your WLED device")
    print("\nSupported files:")
    print("- OneWheel Interface: onewheel.htm, welcome.htm, imu-debug.html")
    print("- WLED Advanced Interface: index.htm, index.css, index.js, iro.js, rangetouch.js")
    print("- Settings Pages: settings_*.htm")
    print("- Custom Files: Any CSS/JS/HTML files")

def create_zip_update_package(files, output_file="ui_update.zip"):
    """Create a ZIP-based update package"""
    print(f"Creating ZIP update package: {output_file}")
    
    with zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for file_path in files:
            if os.path.exists(file_path):
                print(f"Adding file: {file_path}")
                
                # Add file to zip with just the filename (no path)
                filename = os.path.basename(file_path)
                zipf.write(file_path, filename)
            else:
                print(f"Warning: File not found: {file_path}")
    
    print(f"Update package created: {output_file}")
    print(f"Upload this ZIP file via the /updateui endpoint on your WLED device")
    print("\nSupported files:")
    print("- OneWheel Interface: onewheel.htm, welcome.htm, imu-debug.html")
    print("- WLED Advanced Interface: index.htm, index.css, index.js, iro.js, rangetouch.js")
    print("- Settings Pages: settings_*.htm")
    print("- Custom Files: Any CSS/JS/HTML files")

def main():
    parser = argparse.ArgumentParser(description='Create OneWheel UI update packages')
    parser.add_argument('files', nargs='+', help='UI files to include in update package')
    parser.add_argument('--output', '-o', default='ui_update.zip', 
                       help='Output file name (default: ui_update.zip)')
    parser.add_argument('--format', '-f', choices=['zip', 'text'], default='zip',
                       help='Output format: zip or text (default: zip)')
    
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
    if args.format == 'zip':
        create_zip_update_package(valid_files, args.output)
    else:
        create_text_update_package(valid_files, args.output)
    
    print("\n" + "="*50)
    print("UPDATE INSTRUCTIONS:")
    print("1. Go to your WLED device's web interface")
    print("2. Navigate to Settings > Security & Update setup")
    print("3. Click 'Update UI Files'")
    print("4. Upload the created update package")
    print("5. The UI will be updated without requiring a firmware flash!")
    print("="*50)

if __name__ == "__main__":
    main()
