#!/usr/bin/env python3
"""
ArkLights UI Files Checker

This script checks the current state of UI files and provides
diagnostic information about the UI update system.
"""

import os
import sys
from pathlib import Path

def check_ui_files():
    """Check the current state of UI files"""
    print("🔍 ArkLights UI Files Diagnostic")
    print("=" * 40)
    
    # Check if UI directory exists
    ui_dir = Path("ui")
    if not ui_dir.exists():
        print("❌ UI directory not found")
        return False
    
    print(f"✅ UI directory found: {ui_dir.absolute()}")
    
    # Check individual UI files
    ui_files = {
        "index.html": "Main HTML interface",
        "styles.css": "CSS stylesheet", 
        "script.js": "JavaScript functions"
    }
    
    all_files_exist = True
    total_size = 0
    
    print("\n📁 UI Files Status:")
    for filename, description in ui_files.items():
        file_path = ui_dir / filename
        if file_path.exists():
            size = file_path.stat().st_size
            total_size += size
            print(f"✅ {filename:<15} {size:>8} bytes - {description}")
        else:
            print(f"❌ {filename:<15} {'MISSING':>8} - {description}")
            all_files_exist = False
    
    print(f"\n📊 Total UI files size: {total_size:,} bytes")
    
    if all_files_exist:
        print("🎉 All UI files present!")
    else:
        print("⚠️ Some UI files are missing")
    
    return all_files_exist

def check_update_files():
    """Check for update package files"""
    print("\n📦 Update Package Files:")
    
    update_files = [
        "arklights_ui_update.txt",
        "debug_ui_update.txt", 
        "minimal_ui_test.txt",
        "root_ui_test.txt"
    ]
    
    for filename in update_files:
        if os.path.exists(filename):
            size = os.path.getsize(filename)
            print(f"✅ {filename:<25} {size:>8} bytes")
        else:
            print(f"❌ {filename:<25} {'MISSING':>8}")

def show_usage_instructions():
    """Show usage instructions"""
    print("\n" + "=" * 50)
    print("🚀 USAGE INSTRUCTIONS")
    print("=" * 50)
    
    print("\n1. **Upload UI Files via Web Interface:**")
    print("   • Connect to ArkLights device (192.168.4.1)")
    print("   • Go to /updateui endpoint")
    print("   • Upload debug_ui_update.txt first")
    print("   • Check serial output for success messages")
    print("   • Run 'ls' command in serial console")
    
    print("\n2. **Check Serial Output:**")
    print("   Look for these messages:")
    print("   • 'Starting UI update: /ui_update_XXXXX.zip'")
    print("   • 'Processing text-based UI update'")
    print("   • 'Successfully updated: /ui/index.html (XXX bytes written)'")
    print("   • '✅ Verification: /ui/index.html exists (XXX bytes)'")
    print("   • 'UI update completed successfully'")
    
    print("\n3. **Verify Files in SPIFFS:**")
    print("   Run 'ls' command in serial console")
    print("   Should see:")
    print("   • /ui/index.html")
    print("   • /ui/styles.css")
    print("   • /ui/script.js")
    
    print("\n4. **Test Web Interface:**")
    print("   • Visit root URL (192.168.4.1)")
    print("   • Should see the debug test page")
    print("   • Check browser console for JavaScript messages")
    
    print("\n5. **If Debug Test Works:**")
    print("   • Upload arklights_ui_update.txt")
    print("   • Should see the full ArkLights interface")
    
    print("\n🔧 **Troubleshooting:**")
    print("• If 'ls' shows no files: SPIFFS issue")
    print("• If files exist but page doesn't load: File serving issue")
    print("• If serial shows success but no files: Filesystem corruption")
    print("• Try minimal_ui_test.txt for simpler debugging")

def main():
    print("🎨 ArkLights UI Files Diagnostic Tool")
    print("=" * 50)
    
    # Check UI files
    ui_files_ok = check_ui_files()
    
    # Check update files
    check_update_files()
    
    # Show usage instructions
    show_usage_instructions()
    
    print(f"\n📋 **Summary:**")
    if ui_files_ok:
        print("✅ UI files are ready for upload")
        print("🚀 Next: Upload debug_ui_update.txt via /updateui endpoint")
    else:
        print("⚠️ Some UI files are missing")
        print("🔧 Next: Create missing files or use debug_ui_update.txt")
    
    print(f"\n💡 **Pro Tip:**")
    print("Always start with debug_ui_update.txt to test the system,")
    print("then upload arklights_ui_update.txt for the full interface!")

if __name__ == "__main__":
    main()
