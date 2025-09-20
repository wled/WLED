#!/usr/bin/env python3
"""
ArkLights UI Update Debug Script

This script helps debug UI update issues by creating test files
and providing detailed debugging information.
"""

import os
import sys
import time
from pathlib import Path

def create_debug_ui_update():
    """Create a debug UI update package with minimal test files"""
    print("🔧 Creating debug UI update package...")
    
    # Create a simple test HTML file
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    test_html = f"""<!DOCTYPE html>
<html>
<head>
    <title>ArkLights Debug Test</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background: #1a1a2e; color: white; }}
        .container {{ max-width: 600px; margin: 0 auto; text-align: center; }}
        .success {{ background: #4CAF50; padding: 20px; border-radius: 8px; margin: 20px 0; }}
        .info {{ background: #2196F3; padding: 15px; border-radius: 8px; margin: 10px 0; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🎉 ArkLights UI Update Test</h1>
        <div class="success">
            <h2>✅ SUCCESS!</h2>
            <p>This page confirms that the UI update system is working correctly.</p>
            <p><strong>Timestamp:</strong> {timestamp}</p>
        </div>
        <div class="info">
            <h3>Debug Information:</h3>
            <p>• File served from SPIFFS filesystem</p>
            <p>• UI update process completed successfully</p>
            <p>• External UI files are being used</p>
        </div>
        <div class="info">
            <h3>Next Steps:</h3>
            <p>1. Run 'ls' command in serial console</p>
            <p>2. Check that /ui/index.html appears in the file listing</p>
            <p>3. Upload the full UI update package</p>
        </div>
    </div>
</body>
</html>"""
    
    # Create a simple test CSS file
    test_css = f"""/* ArkLights Debug Test CSS */
body {{ 
    font-family: Arial, sans-serif; 
    margin: 20px; 
    background: #1a1a2e; 
    color: white; 
}}

.container {{ 
    max-width: 600px; 
    margin: 0 auto; 
    text-align: center; 
}}

.success {{ 
    background: #4CAF50; 
    padding: 20px; 
    border-radius: 8px; 
    margin: 20px 0; 
}}

.info {{ 
    background: #2196F3; 
    padding: 15px; 
    border-radius: 8px; 
    margin: 10px 0; 
}}

/* Debug timestamp: {timestamp} */"""
    
    # Create a simple test JS file
    test_js = f"""// ArkLights Debug Test JavaScript
console.log('🎉 ArkLights UI Update Test - JavaScript loaded successfully!');

// Debug timestamp: {timestamp}
const debugInfo = {{
    timestamp: '{timestamp}',
    status: 'UI Update Test Successful',
    filesystem: 'SPIFFS',
    version: 'Debug Test v1.0'
}};

console.log('Debug Info:', debugInfo);

// Show success message
document.addEventListener('DOMContentLoaded', function() {{
    console.log('✅ DOM loaded - UI update test page is working!');
    
    // Add a small visual indicator
    const indicator = document.createElement('div');
    indicator.style.cssText = `
        position: fixed;
        top: 10px;
        right: 10px;
        background: #4CAF50;
        color: white;
        padding: 5px 10px;
        border-radius: 4px;
        font-size: 12px;
        z-index: 1000;
    `;
    indicator.textContent = 'UI Test ✅';
    document.body.appendChild(indicator);
}});"""
    
    # Create the update package
    update_content = f"""FILENAME:/ui/index.html:{test_html}:ENDFILE
FILENAME:/ui/styles.css:{test_css}:ENDFILE
FILENAME:/ui/script.js:{test_js}:ENDFILE"""
    
    # Write the debug update file
    with open('debug_ui_update.txt', 'w', encoding='utf-8') as f:
        f.write(update_content)
    
    print("✅ Debug UI update package created: debug_ui_update.txt")
    print(f"📊 Package size: {len(update_content)} bytes")
    print("\n📋 Debug Instructions:")
    print("1. Upload debug_ui_update.txt via /updateui endpoint")
    print("2. Check serial output for success messages")
    print("3. Run 'ls' command to see if files appear")
    print("4. Visit root URL to see the test page")
    print("5. Check browser console for JavaScript messages")
    
    return 'debug_ui_update.txt'

def create_minimal_test():
    """Create a minimal single-file test"""
    print("\n🔧 Creating minimal single-file test...")
    
    minimal_html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Minimal Test - {time.strftime("%H:%M:%S")}</title>
    <style>body{{font-family:Arial;margin:20px;background:#1a1a2e;color:white;text-align:center;}}</style>
</head>
<body>
    <h1>🎯 Minimal UI Test</h1>
    <p>If you see this, the UI update system is working!</p>
    <p>Timestamp: {time.strftime("%Y-%m-%d %H:%M:%S")}</p>
    <script>console.log('Minimal test loaded at {time.strftime("%H:%M:%S")}');</script>
</body>
</html>"""
    
    update_content = f"FILENAME:/ui/index.html:{minimal_html}:ENDFILE"
    
    with open('minimal_ui_test.txt', 'w', encoding='utf-8') as f:
        f.write(update_content)
    
    print("✅ Minimal test created: minimal_ui_test.txt")
    return 'minimal_ui_test.txt'

def create_root_test():
    """Create a test file in root directory"""
    print("\n🔧 Creating root directory test...")
    
    root_html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Root Test - {time.strftime("%H:%M:%S")}</title>
    <style>body{{font-family:Arial;margin:20px;background:#2a2a2e;color:white;text-align:center;}}</style>
</head>
<body>
    <h1>📁 Root Directory Test</h1>
    <p>This file is in the root directory, not /ui/</p>
    <p>Timestamp: {time.strftime("%Y-%m-%d %H:%M:%S")}</p>
    <script>console.log('Root test loaded at {time.strftime("%H:%M:%S")}');</script>
</body>
</html>"""
    
    update_content = f"FILENAME:/index.html:{root_html}:ENDFILE"
    
    with open('root_ui_test.txt', 'w', encoding='utf-8') as f:
        f.write(update_content)
    
    print("✅ Root test created: root_ui_test.txt")
    return 'root_ui_test.txt'

def print_debugging_guide():
    """Print comprehensive debugging guide"""
    print("\n" + "="*60)
    print("🔍 ARKLIGHTS UI UPDATE DEBUGGING GUIDE")
    print("="*60)
    
    print("\n📋 Step-by-Step Debugging Process:")
    print("1. Upload debug_ui_update.txt via /updateui endpoint")
    print("2. Watch serial output for these messages:")
    print("   • 'Starting UI update: /ui_update_XXXXX.zip'")
    print("   • 'Processing text-based UI update'")
    print("   • 'Updating file: /ui/index.html'")
    print("   • 'Successfully updated: /ui/index.html (XXX bytes written)'")
    print("   • '✅ Verification: /ui/index.html exists (XXX bytes)'")
    print("   • 'UI update completed successfully'")
    
    print("\n3. Run 'ls' command in serial console")
    print("   Look for these files:")
    print("   • /ui/index.html")
    print("   • /ui/styles.css") 
    print("   • /ui/script.js")
    
    print("\n4. Check the web interface:")
    print("   • Visit root URL (192.168.4.1)")
    print("   • Should see the debug test page")
    print("   • Check browser console for JavaScript messages")
    
    print("\n🚨 Common Issues & Solutions:")
    print("• Files not in 'ls' output:")
    print("  - SPIFFS might be getting reformatted")
    print("  - Files written to wrong location")
    print("  - Filesystem corruption")
    
    print("\n• 'ls' shows files but web page doesn't load:")
    print("  - Check file serving logic in handleRoot()")
    print("  - Verify file paths are correct")
    print("  - Check for file size = 0")
    
    print("\n• Serial shows success but no files:")
    print("  - Check SPIFFS.begin() in initFilesystem()")
    print("  - Verify filesystem isn't being reset")
    print("  - Check for memory issues")
    
    print("\n🔧 Advanced Debugging:")
    print("• Add this to setup() for detailed filesystem info:")
    print("  File root = SPIFFS.open('/');")
    print("  File file = root.openNextFile();")
    print("  while (file) {{")
    print("    Serial.printf('File: %s, Size: %d\\n', file.name(), file.size());")
    print("    file = root.openNextFile();")
    print("  }}")
    print("  root.close();")
    
    print("\n• Test file reading:")
    print("  File testFile = SPIFFS.open('/ui/index.html', 'r');")
    print("  if (testFile) {{")
    print("    String content = testFile.readString();")
    print("    Serial.printf('HTML content length: %d\\n', content.length());")
    print("    testFile.close();")
    print("  }}")
    
    print("\n📞 If Still Not Working:")
    print("1. Try the minimal test (minimal_ui_test.txt)")
    print("2. Try the root directory test (root_ui_test.txt)")
    print("3. Check SPIFFS filesystem health")
    print("4. Verify PlatformIO SPIFFS configuration")
    print("5. Consider using embedded UI fallback")

def main():
    print("🎨 ArkLights UI Update Debug Tool")
    print("=" * 40)
    
    # Create debug files
    debug_file = create_debug_ui_update()
    minimal_file = create_minimal_test()
    root_file = create_root_test()
    
    # Print debugging guide
    print_debugging_guide()
    
    print(f"\n✅ Debug files created:")
    print(f"  • {debug_file} - Full debug test")
    print(f"  • {minimal_file} - Minimal test")
    print(f"  • {root_file} - Root directory test")
    
    print(f"\n🚀 Next Steps:")
    print(f"1. Upload {debug_file} via /updateui endpoint")
    print(f"2. Check serial output for success messages")
    print(f"3. Run 'ls' command to verify files")
    print(f"4. Visit root URL to see test page")
    print(f"5. If that works, upload arklights_ui_update.txt")

if __name__ == "__main__":
    main()
