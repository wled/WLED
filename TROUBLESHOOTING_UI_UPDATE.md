# ArkLights UI Update System - Troubleshooting Guide

## 🚨 **White Page Issue - Debugging Steps**

If you're seeing a white page at the root URL, here's how to diagnose and fix the issue:

### **Step 1: Upload Debug Firmware**

Upload the debug firmware: `arklights_firmware_v9_debug_enhanced.bin`

This version includes extensive debug logging to help identify the issue.

### **Step 2: Check Serial Output**

Connect to the device via serial monitor (115200 baud) and look for these debug messages:

#### **Expected Debug Output:**
```
🧪 Testing Filesystem...
📁 SPIFFS file listing:
  📄 /settings.json (123 bytes)
  📄 /ui/index.html (19067 bytes)  ← Should see this if UI files exist
  📄 /ui/styles.css (1917 bytes)   ← Should see this if UI files exist
  📄 /ui/script.js (21166 bytes)   ← Should see this if UI files exist
✅ Test file written
📖 Test file content: {"test_value":123,"timestamp":12345}
🗑️ Test file cleaned up

Web server started
```

#### **When Root Page is Accessed:**
```
🔍 handleRoot: Attempting to serve /ui/index.html
✅ Found external UI file, serving from SPIFFS
```
**OR**
```
🔍 handleRoot: Attempting to serve /ui/index.html
⚠️ External UI file not found, serving embedded UI
🎨 serveEmbeddedUI: Serving embedded UI fallback
📤 serveEmbeddedUI: Sending HTML response (12345 bytes)
✅ serveEmbeddedUI: Response sent successfully
```

#### **When CSS/JS Files are Requested:**
```
🎨 handleUI: Requesting file: /ui/styles.css
✅ handleUI: Found file /ui/styles.css, serving as text/css
```
**OR**
```
🎨 handleUI: Requesting file: /ui/styles.css
❌ handleUI: File not found: /ui/styles.css
```

### **Step 3: Diagnose the Issue**

Based on the debug output, identify the problem:

#### **Issue A: No UI Files in SPIFFS**
**Symptoms:**
- SPIFFS file listing shows no `/ui/` files
- Root page shows: "⚠️ External UI file not found, serving embedded UI"

**Solution:**
1. Upload UI files using the update system
2. Go to `/updateui` endpoint
3. Upload `arklights_ui_update.txt` (created earlier)
4. Check serial output for success messages

#### **Issue B: UI Files Exist But Not Loading**
**Symptoms:**
- SPIFFS file listing shows UI files exist
- Root page shows: "✅ Found external UI file, serving from SPIFFS"
- But page still appears white

**Solution:**
1. Check browser developer tools (F12)
2. Look for 404 errors on CSS/JS files
3. Check if `handleUI()` is returning 404 errors

#### **Issue C: Embedded UI Not Working**
**Symptoms:**
- Root page shows: "🎨 serveEmbeddedUI: Serving embedded UI fallback"
- But page still appears white

**Solution:**
1. Check if HTML response is being sent
2. Look for JavaScript errors in browser console
3. Verify the embedded UI HTML is complete

### **Step 4: Test with Simple UI**

Use the test UI file to verify the system works:

1. **Upload Test UI:**
   ```bash
   python3 create_ui_update.py test_ui.html --output test_ui_update.txt
   ```

2. **Upload via Web Interface:**
   - Go to `/updateui`
   - Upload `test_ui_update.txt`
   - Check serial output for success

3. **Verify Test UI:**
   - Go to root URL
   - Should see "🎉 ArkLights UI Update Test" page
   - Test API connection button should work

### **Step 5: Common Fixes**

#### **Fix 1: Re-upload UI Files**
```bash
# Create fresh UI update package
python3 create_ui_update.py ui/index.html ui/styles.css ui/script.js

# Upload via /updateui endpoint
```

#### **Fix 2: Check File Paths**
Ensure UI files are uploaded to correct paths:
- `/ui/index.html`
- `/ui/styles.css` 
- `/ui/script.js`

#### **Fix 3: Clear Browser Cache**
- Hard refresh (Ctrl+F5 or Cmd+Shift+R)
- Clear browser cache
- Try incognito/private mode

#### **Fix 4: Check Network Connection**
- Verify device IP address (usually 192.168.4.1)
- Check WiFi connection to ArkLights AP
- Test with different browser/device

### **Step 6: Advanced Debugging**

#### **Check SPIFFS Filesystem:**
```cpp
// Add this to setup() for detailed filesystem info
File root = SPIFFS.open("/");
if (root) {
    File file = root.openNextFile();
    while (file) {
        Serial.printf("File: %s, Size: %d\n", file.name(), file.size());
        file = root.openNextFile();
    }
    root.close();
}
```

#### **Test File Serving:**
```cpp
// Test if files can be read
File testFile = SPIFFS.open("/ui/index.html", "r");
if (testFile) {
    String content = testFile.readString();
    Serial.printf("HTML content length: %d\n", content.length());
    testFile.close();
} else {
    Serial.println("Failed to open HTML file");
}
```

### **Step 7: Fallback Options**

If UI update system isn't working:

1. **Use Embedded UI:**
   - The embedded UI should always work as fallback
   - Check if `serveEmbeddedUI()` is being called
   - Verify embedded HTML is complete

2. **Manual SPIFFS Upload:**
   - Use PlatformIO's `uploadfs` target
   - Copy UI files to SPIFFS manually
   - Rebuild and upload firmware

3. **Revert to Previous Version:**
   - Use a working firmware version
   - Debug the UI update system separately

## 🎯 **Expected Behavior**

### **Working System:**
1. **Root URL (`/`)**: Shows ArkLights interface
2. **CSS/JS Files**: Load correctly (no 404 errors)
3. **API Endpoints**: Respond correctly
4. **UI Update Page (`/updateui`)**: Shows upload interface

### **Debug Output:**
- SPIFFS file listing shows UI files
- Root page serves external UI files
- CSS/JS files serve correctly
- No 404 errors in browser console

## 🔧 **Quick Fixes**

### **Immediate Solutions:**
1. **Upload debug firmware** and check serial output
2. **Upload test UI** via `/updateui` endpoint
3. **Check browser console** for JavaScript errors
4. **Clear browser cache** and hard refresh

### **If Still Not Working:**
1. **Check SPIFFS filesystem** health
2. **Verify file paths** are correct
3. **Test with minimal HTML** file
4. **Use embedded UI fallback**

The debug firmware will show exactly what's happening and help identify the root cause of the white page issue! 🚀
