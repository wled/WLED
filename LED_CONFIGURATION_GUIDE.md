# 🎨 ArkLights LED Configuration Guide

## 🚀 **Quick Start - Fix Your Red LED Issue**

Your red LEDs showing different colors is likely a **color order problem**. Here's how to fix it:

### **Step 1: Connect to Web UI**
1. Connect to WiFi: `ARKLIGHTS-AP` (password: `float420`)
2. Open browser: `http://192.168.4.1`
3. Scroll down to **"LED Configuration"** section

### **Step 2: Test Different Color Orders**
1. Click **"Test LEDs"** button (cycles through Red→Green→Blue→White)
2. If red shows as wrong color, try changing **"Headlight Color Order"**:
   - Try **RGB** instead of GRB
   - Try **BGR** if RGB doesn't work
3. Click **"Save Configuration"** when colors look correct

### **Step 3: Verify LED Type**
- If you have **SK6812 RGBW** LEDs: Keep `SK6812 (RGBW)` selected
- If you have **WS2812B** LEDs: Change to `WS2812B (RGB)`
- If you have **APA102** LEDs: Change to `APA102 (RGB)` (needs clock pin)

---

## 🔧 **Complete LED Configuration Options**

### **Supported LED Types**

| LED Type | Pin Requirements | Color Order Options | Best For |
|----------|------------------|-------------------|----------|
| **SK6812** | Data pin only | GRB, RGB, BGR | RGBW strips, high quality |
| **WS2812B** | Data pin only | GRB, RGB, BGR | Most common, RGB strips |
| **APA102** | Data + Clock pin | GRB, RGB, BGR | High speed, SPI-based |
| **LPD8806** | Data + Clock pin | GRB, RGB, BGR | Older SPI strips |

### **Pin Configuration**

**For SK6812/WS2812B (Single Pin):**
- Headlight: GPIO 3
- Taillight: GPIO 2

**For APA102/LPD8806 (Two Pins):**
- Headlight: Data=GPIO 3, Clock=GPIO 4
- Taillight: Data=GPIO 2, Clock=GPIO 5

### **Color Order Troubleshooting**

| If Red Shows As | Try This Color Order | Explanation |
|-----------------|---------------------|-------------|
| **Green** | RGB | Your LEDs expect RGB order |
| **Blue** | BGR | Your LEDs expect BGR order |
| **Correct Red** | GRB | Default order works |

---

## 🌐 **Web UI Configuration**

### **LED Configuration Panel**
The web interface now includes a comprehensive LED configuration section:

- **LED Count**: Set number of LEDs per strip (1-200)
- **LED Type**: Choose from SK6812, WS2812B, APA102, LPD8806
- **Color Order**: Choose GRB, RGB, or BGR
- **Test LEDs**: Cycles through Red→Green→Blue→White
- **Save Configuration**: Persists settings

### **API Endpoints**

**Update LED Configuration:**
```bash
POST /api/led-config
Content-Type: application/json

{
  "headlightLedCount": 20,
  "taillightLedCount": 20,
  "headlightLedType": 0,
  "taillightLedType": 0,
  "headlightColorOrder": 1,
  "taillightColorOrder": 1
}
```

**Test LEDs:**
```bash
POST /api/led-test
```

**Get Status (includes LED config):**
```bash
GET /api/status
```

---

## 🔍 **Troubleshooting Common Issues**

### **Issue: Red shows as Green/Blue**
**Solution:** Change color order in web UI
- Try RGB instead of GRB
- Try BGR if RGB doesn't work

### **Issue: LEDs don't light up at all**
**Solutions:**
1. Check wiring (data pin connections)
2. Verify LED count matches actual strip
3. Try different LED type (WS2812B vs SK6812)
4. Check power supply (5V for most strips)

### **Issue: APA102/LPD8806 not working**
**Solution:** Ensure you have both data AND clock pins connected
- Data pin: GPIO 3 (headlight) or GPIO 2 (taillight)
- Clock pin: GPIO 4 (headlight) or GPIO 5 (taillight)

### **Issue: Wrong number of LEDs lighting**
**Solution:** Update LED count in web UI to match your actual strip

### **Issue: Colors are dim/washed out**
**Solutions:**
1. Increase brightness in web UI
2. Check power supply capacity
3. Verify LED strip voltage (5V vs 12V)

---

## 📱 **Mobile App Integration**

The LED configuration API is designed for future mobile app integration:

```javascript
// Example mobile app code
const updateLEDConfig = async (config) => {
  const response = await fetch('http://192.168.4.1/api/led-config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config)
  });
  return response.json();
};

// Test LEDs
const testLEDs = async () => {
  await fetch('http://192.168.4.1/api/led-test', { method: 'POST' });
};
```

---

## 🎯 **Recommended Settings by LED Type**

### **SK6812 RGBW Strips**
- LED Type: SK6812 (RGBW)
- Color Order: GRB (default)
- Brightness: 128-200

### **WS2812B RGB Strips**
- LED Type: WS2812B (RGB)
- Color Order: GRB (try RGB if colors wrong)
- Brightness: 128-255

### **APA102 RGB Strips**
- LED Type: APA102 (RGB)
- Color Order: GRB (try RGB if colors wrong)
- Brightness: 128-255
- **Requires:** Data + Clock pins

### **LPD8806 RGB Strips**
- LED Type: LPD8806 (RGB)
- Color Order: GRB (try RGB if colors wrong)
- Brightness: 128-255
- **Requires:** Data + Clock pins

---

## 🔄 **Configuration Persistence**

Currently, LED configuration is saved in memory and will reset on reboot. Future versions will include:

- EEPROM/NVS storage for persistent settings
- Configuration backup/restore
- Multiple LED strip profiles

---

## 🆘 **Need Help?**

1. **Check Serial Output**: Connect USB and monitor serial at 115200 baud
2. **Use LED Test**: Click "Test LEDs" to verify basic functionality
3. **Try Different Settings**: Test various LED types and color orders
4. **Check Wiring**: Ensure proper connections and power supply

The web UI makes it easy to test different configurations without recompiling code!