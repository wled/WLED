# 🚀 Quick Setup Guide - Onewheel WLED

## Pre-Built Installation

### 1. Hardware Requirements
- ✅ ESP32 board (ESP32-S3 recommended)
- ✅ MPU6050 sensor
- ✅ WS2812B LED strips
- ✅ 5V power supply (adequate for your LED count)

### 2. Wiring (5 minutes)
```
MPU6050 → ESP32
VCC → 3.3V
GND → GND  
SDA → GPIO 5
SCL → GPIO 6

LEDs → ESP32
Data → GPIO 2 (or your configured pin)
VCC → 5V
GND → GND
```

### 3. Flash Firmware
1. Download the pre-built firmware from releases
2. Use ESP32 Flash Tool or esptool
3. Flash to your ESP32

### 4. Initial Configuration
1. Connect to WLED WiFi hotspot
2. Configure your WiFi network
3. Set up LED preferences:
   - **Total LEDs**: Your strip count
   - **Segment 0**: Headlight LEDs (start: 0, length: X)
   - **Segment 1**: Taillight LEDs (start: X, length: Y)

### 5. Access Onewheel Interface
Navigate to: `http://your-esp-ip/onewheel.htm`

## Build from Source (Advanced)

### 1. PlatformIO Setup
```bash
git clone https://github.com/your-repo/onewheel-wled
cd onewheel-wled
```

### 2. Configure platformio.ini
```ini
[env:esp32s3dev]
board = esp32-s3-devkitc-1
framework = arduino
build_flags = 
  -D USERMOD_MPU6050_IMU
  -D WLED_MAX_SEGMENTS=2
  -D WLED_ENABLE_ADALIGHT
  ; Add other flags as needed

lib_deps = 
  adafruit/Adafruit MPU6050@^2.2.4
  adafruit/Adafruit Unified Sensor@^1.1.9
  ; Other dependencies from original WLED
```

### 3. Enable Usermod
In `usermods_list.cpp`:
```cpp
#ifdef USERMOD_MPU6050_IMU
#include "../usermods/mpu6050_imu/usermod_mpu6050_imu.h"
#endif

void registerUsermods() {
  #ifdef USERMOD_MPU6050_IMU
  UsermodManager::add(new MPU6050Driver());
  #endif
}
```

### 4. Build & Upload
```bash
pio run -t upload
```

## Quick Test

### 1. Power On
- Access the Onewheel interface
- Hit the power button (should turn green)
- Both headlight and taillight should activate

### 2. Test Motion Controls
- **Tilt Test**: Lean the board - watch tilt values in status
- **Blinker Test**: Quickly lean left/right - blinkers should activate
- **Parking Test**: Tip the board up 15+ degrees - parking effect should start

### 3. Mode Switching
- Try **Standard Mode**: Normal brightness
- Try **Night Mode**: Maximum headlight brightness

## Troubleshooting

| Issue | Quick Fix |
|-------|-----------|
| No MPU data | Check wiring, restart ESP32 |
| Blinkers too sensitive | Increase turn threshold in settings |
| LEDs not working | Check power supply, data pin connection |
| UI not loading | Clear browser cache, check IP address |

## Default Settings
- **Turn threshold**: 20 degrees
- **Blinker delay**: 500ms  
- **Parking angle**: 15 degrees
- **Standard headlight**: 200 brightness
- **Night headlight**: 255 brightness

Ready to ride! 🛹✨