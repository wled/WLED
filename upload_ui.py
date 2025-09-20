#!/usr/bin/env python3
"""
Upload UI files to ESP32 SPIFFS
"""

import os
import sys
import subprocess

def upload_ui_files():
    """Upload UI files to ESP32 SPIFFS"""
    
    # Check if UI files exist
    ui_files = [
        "ui/index.html",
        "ui/styles.css", 
        "ui/script.js"
    ]
    
    for file_path in ui_files:
        if not os.path.exists(file_path):
            print(f"❌ UI file not found: {file_path}")
            return False
    
    print("📁 UI files found:")
    for file_path in ui_files:
        size = os.path.getsize(file_path)
        print(f"  - {file_path} ({size} bytes)")
    
    # Upload files to SPIFFS
    print("\n🚀 Uploading UI files to SPIFFS...")
    
    try:
        # First, copy UI files to the data directory that PlatformIO expects
        data_dir = "data"
        os.makedirs(data_dir, exist_ok=True)
        
        # Create ui subdirectory in data
        ui_data_dir = os.path.join(data_dir, "ui")
        os.makedirs(ui_data_dir, exist_ok=True)
        
        print(f"📁 Copying UI files to {ui_data_dir}...")
        for file_path in ui_files:
            filename = os.path.basename(file_path)
            dest_path = os.path.join(ui_data_dir, filename)
            subprocess.run(["cp", file_path, dest_path], check=True)
            print(f"  ✓ Copied {filename}")
        
        # Use PlatformIO's SPIFFS upload
        print("\n🚀 Uploading UI files to SPIFFS...")
        result = subprocess.run([
            "pio", "run", "-e", "arklights_test", "-t", "uploadfs"
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            print("✅ UI files uploaded successfully!")
            return True
        else:
            print(f"❌ Upload failed: {result.stderr}")
            return False
            
    except FileNotFoundError:
        print("❌ PlatformIO not found. Please install PlatformIO.")
        return False

if __name__ == "__main__":
    print("🎨 ArkLights UI Uploader")
    print("=" * 30)
    
    if upload_ui_files():
        print("\n🎉 UI separation complete!")
        print("📱 The web interface now uses external files")
        print("💾 Flash usage reduced by ~44KB")
        print("\n📋 Next steps:")
        print("1. Upload firmware: pio run -e arklights_test -t upload")
        print("2. Access web interface at http://192.168.4.1")
        print("3. UI files are now served from SPIFFS")
    else:
        print("\n❌ Upload failed. Check the errors above.")
        sys.exit(1)
