# ArkLights Serial Commands - Quick Reference

## 🖥️ **New SPIFFS File Listing Command**

### **Command: `list_files` or `ls`**

Lists all files in the SPIFFS filesystem with detailed information.

### **Usage:**
```
list_files
```
or
```
ls
```

### **Example Output:**
```
📁 SPIFFS File Listing:
========================
📄 settings.json          123 bytes [JSON]
📄 ui/index.html        19067 bytes [HTML]
📄 ui/styles.css         1917 bytes [CSS]
📄 ui/script.js         21166 bytes [JS]
📄 ui_update_12345.zip   5000 bytes [ZIP]
========================
📊 Total: 5 files, 47273 bytes

🎨 UI Files Check:
✅ /ui/index.html (19067 bytes)
✅ /ui/styles.css (1917 bytes)
✅ /ui/script.js (21166 bytes)
🎉 All UI files present - external UI should work
```

### **What It Shows:**

1. **File List**: All files in SPIFFS with names, sizes, and types
2. **File Types**: Automatically detects HTML, CSS, JS, JSON, TXT, ZIP files
3. **Total Stats**: Number of files and total size used
4. **UI Files Check**: Specifically checks for the 3 main UI files
5. **Status**: Tells you if external UI will work or if fallback will be used

### **File Type Indicators:**
- `[HTML]` - HTML files (.html, .htm)
- `[CSS]` - CSS stylesheets (.css)
- `[JS]` - JavaScript files (.js)
- `[JSON]` - JSON configuration files (.json)
- `[TXT]` - Text files (.txt)
- `[ZIP]` - ZIP archives (.zip)

### **UI Files Status:**
- **✅ All UI files present**: External UI will be served from SPIFFS
- **⚠️ Some UI files missing**: Embedded UI fallback will be used

## 🔧 **Troubleshooting White Page Issues**

### **Step 1: Check SPIFFS Files**
```
ls
```

### **Step 2: Look for UI Files**
The command will show if these files exist:
- `/ui/index.html`
- `/ui/styles.css`
- `/ui/script.js`

### **Step 3: Upload UI Files if Missing**
1. Go to `/updateui` endpoint in web browser
2. Upload `arklights_ui_update.txt` (created earlier)
3. Run `ls` again to verify files were uploaded

### **Step 4: Test with Simple UI**
1. Upload `test_ui_update.txt` (created earlier)
2. Go to root URL - should see test page
3. Run `ls` to see the test file

## 📋 **Complete Command List**

### **LED Control:**
- `p0-p3` - Set preset (0=Standard, 1=Night, 2=Party, 3=Stealth)
- `b<0-255>` - Set brightness
- `h<hex>` - Set headlight color (e.g., hFF0000)
- `t<hex>` - Set taillight color (e.g., t00FF00)
- `eh<0-19>` - Set headlight effect
- `et<0-19>` - Set taillight effect

### **Startup Sequences:**
- `startup<0-5>` - Set startup sequence
- `test_startup` - Test current startup sequence

### **Motion Control:**
- `calibrate/cal` - Start motion calibration
- `reset_cal` - Reset motion calibration
- `motion_on/off` - Enable/disable motion control
- `blinker_on/off` - Enable/disable auto blinkers
- `park_on/off` - Enable/disable park mode

### **System:**
- `status` - Show current status
- `list_files/ls` - **List SPIFFS files** ⭐ NEW!
- `help` - Show help

### **OTA Updates:**
- `ota_status` - Show OTA update status

## 🎯 **Quick Debugging Workflow**

1. **Check SPIFFS**: `ls`
2. **Check Status**: `status`
3. **Upload UI Files**: Go to `/updateui` endpoint
4. **Verify Upload**: `ls` again
5. **Test Web Interface**: Go to root URL

The `ls` command is perfect for debugging UI update issues and understanding what files are stored on the device! 🚀
