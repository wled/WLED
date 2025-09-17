# ArkLights UI Separation

The ArkLights web interface has been separated from the main firmware code for better maintainability and organization.

## 📁 File Structure

```
ARKLIGHTS_WLED/
├── ui/                          # External UI files
│   ├── index.html              # Main HTML page
│   ├── styles.css              # CSS styles
│   └── script.js               # JavaScript functions
├── src/
│   └── main.cpp                # Main firmware (now ~44KB smaller!)
└── upload_ui.py                # UI upload script
```

## 🚀 Benefits

- **Smaller firmware**: Reduced flash usage by ~44KB
- **Easier maintenance**: UI changes don't require firmware rebuilds
- **Better organization**: HTML/CSS/JS separated from C++ code
- **Faster development**: Edit UI files directly without recompiling

## 📋 Usage

### 1. Upload UI Files to SPIFFS
```bash
python3 upload_ui.py
```

### 2. Upload Firmware
```bash
pio run -e arklights_test -t upload
```

### 3. Access Web Interface
Navigate to `http://192.168.4.1` in your browser.

## 🔧 Development

### Editing UI Files
- **HTML**: Edit `ui/index.html` for page structure
- **CSS**: Edit `ui/styles.css` for styling
- **JavaScript**: Edit `ui/script.js` for functionality

### After Making Changes
1. Run `python3 upload_ui.py` to upload changes
2. Refresh the web page to see updates

## 📊 File Sizes

| File | Size | Purpose |
|------|------|---------|
| `ui/index.html` | ~15KB | Main HTML structure |
| `ui/styles.css` | ~2KB | CSS styling |
| `ui/script.js` | ~27KB | JavaScript functionality |
| **Total UI** | **~44KB** | **External UI files** |

## 🎯 Technical Details

- UI files are served from SPIFFS (SPI Flash File System)
- Files are accessed via `/ui/` paths (e.g., `/ui/styles.css`)
- The main page (`/`) serves `ui/index.html`
- All API endpoints remain unchanged
- OTA updates still work normally

## 🔄 Hybrid UI Approach

The firmware now uses a **hybrid approach** that solves the OTA update issue:

### **🎯 How It Works:**
1. **Primary**: Serves UI files from SPIFFS (if available)
2. **Fallback**: Uses embedded UI in firmware (if SPIFFS files missing)
3. **OTA Updates**: Always update both firmware AND embedded UI

### **✅ Benefits:**
- **OTA Updates**: Always work with matching UI
- **Development**: Can still use external UI files for faster iteration
- **Reliability**: Never breaks due to missing UI files
- **Flexibility**: Best of both worlds

### **📋 Usage Scenarios:**

#### **Scenario 1: Development Mode**
```bash
python3 upload_ui.py    # Upload external UI files
pio run -e arklights_test -t upload  # Upload firmware
```
- Uses external UI files from SPIFFS
- Faster UI development
- Full feature set available

#### **Scenario 2: Production Mode**
```bash
pio run -e arklights_test -t upload  # Upload firmware only
```
- Uses embedded UI from firmware
- OTA updates work perfectly
- Simplified deployment

#### **Scenario 3: OTA Update**
- Upload new firmware via web interface
- Both firmware AND UI are updated together
- No compatibility issues

## 🔄 Migration Notes

The hybrid approach maintains full backward compatibility:
- All existing API endpoints work unchanged
- OTA updates continue to function perfectly
- Settings and configuration persist
- Motion control features remain intact
- UI always works (embedded fallback)

This hybrid approach makes the codebase maintainable while ensuring OTA updates always work! 🎉
