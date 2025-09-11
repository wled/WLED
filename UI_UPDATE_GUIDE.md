# OneWheel UI Update System

This system allows you to update the web interface files on OneWheel devices without requiring a full firmware update. This is perfect for remote devices that need UI updates via OTA.

## How It Works

1. **Filesystem-based UI**: The main UI files are stored in the device's filesystem
2. **Fallback System**: If filesystem files are missing, embedded fallback UI is used
3. **Update Endpoint**: `/updateui` endpoint handles bulk UI file updates
4. **Package Format**: Supports both ZIP and text-based update packages

## Files That Can Be Updated

### OneWheel Interface
- `onewheel.htm` - Main OneWheel interface
- `welcome.htm` - Welcome page  
- `imu-debug.html` - IMU debug interface

### WLED Advanced Interface
- `index.htm` - Main WLED interface (overrides embedded version)
- `index.css` - WLED stylesheet (overrides embedded version)
- `index.js` - WLED JavaScript (overrides embedded version)
- `iro.js` - Color picker library (overrides embedded version)
- `rangetouch.js` - Touch support (overrides embedded version)

### Settings Pages
- `settings.htm` - Main settings page
- `settings_wifi.htm` - WiFi settings
- `settings_leds.htm` - LED configuration
- `settings_ui.htm` - UI settings
- `settings_sync.htm` - Sync settings
- `settings_time.htm` - Time settings
- `settings_sec.htm` - Security settings
- `settings_2D.htm` - 2D matrix settings
- `settings_dmx.htm` - DMX settings
- `settings_pin.htm` - Pin configuration
- `settings_um.htm` - Usermod settings

### Custom Files
- Any custom CSS/JS/HTML files
- Custom HTML pages

## Using the Update System

### Method 1: Web Interface

1. Connect to your OneWheel device's web interface
2. Go to **Settings** → **Security & Update setup**
3. Click **"Update UI Files"**
4. Upload a ZIP file containing your updated UI files
5. The system will extract and deploy the files automatically

### Method 2: Command Line Tool

Use the included `create_ui_update.py` script:

```bash
# Create update package from specific files
python create_ui_update.py onewheel.htm welcome.htm

# Create ZIP package (default)
python create_ui_update.py --format zip --output my_update.zip onewheel.htm

# Create text package
python create_ui_update.py --format text --output my_update.txt onewheel.htm
```

### Method 3: Mobile App Integration

For your mobile app, you can upload update packages via HTTP:

```javascript
// Upload UI update via mobile app
function updateDeviceUI(deviceIP, updateFile) {
    const formData = new FormData();
    formData.append('uiupdate', updateFile);
    
    return fetch(`http://${deviceIP}/updateui`, {
        method: 'POST',
        body: formData
    });
}
```

## Update Package Formats

### ZIP Format (Recommended)
- Standard ZIP file containing UI files
- Files should be at root level (no subdirectories)
- Automatic extraction and deployment

### Text Format (Simple)
- Custom text format for simple updates
- Format: `FILENAME:filename:content:ENDFILE`
- Good for single-file updates

## Security

- UI updates require PIN authentication (same as firmware updates)
- Files are validated before deployment
- Automatic backup of existing files before update
- Rollback capability if update fails

## Development Workflow

### For Local Development:
```bash
# Make changes to UI files
# Test locally
pio run -t uploadfs  # Upload filesystem only

# Or use the update system
python create_ui_update.py onewheel.htm
# Upload via web interface
```

### For Production Updates:
```bash
# Create update package
python create_ui_update.py onewheel.htm welcome.htm

# Deploy to remote devices via:
# 1. Web interface upload
# 2. Mobile app integration  
# 3. Automated deployment script
```

## Benefits

✅ **No Firmware Updates**: UI changes don't require full firmware flash  
✅ **OTA Friendly**: Perfect for remote device updates  
✅ **Fast Updates**: Only upload changed files  
✅ **Safe**: Automatic backup and rollback  
✅ **User Friendly**: Simple web interface for updates  
✅ **Mobile Ready**: Easy integration with mobile apps  

## Troubleshooting

### Update Fails
- Check PIN authentication
- Ensure filesystem has enough space
- Verify file format is correct
- Check device logs for error messages

### Files Not Updating
- Clear browser cache
- Check if files were actually uploaded
- Verify file permissions
- Try manual file replacement

### Rollback
- Use backup files (`.backup` extension)
- Restore via settings page
- Factory reset if needed

## API Endpoints

- `GET /updateui` - Update interface page
- `POST /updateui` - Upload update package
- `GET /settings` - Settings page with update link

This system makes it easy to iterate on your UI and deploy updates to remote OneWheel devices without the complexity of full firmware updates!
