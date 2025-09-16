# ArkLights PEV Lighting System

A clean, focused LED lighting system designed specifically for Personal Electric Vehicles (PEVs) like OneWheels, electric skateboards, and e-bikes. Built from the ground up with embedded devices in mind.

## Project Structure

This project has been reorganized to focus on the ArkLights implementation:

```
ARKLIGHTS_WLED/
├── src/
│   └── main.cpp                    # ArkLights implementation
├── platformio_simple.ini          # Simple build configuration
├── platformio_arklights.ini        # Full ArkLights configuration
├── ARKLIGHTS_README.md             # This documentation
├── LICENSE                         # Project license
└── legacy/                         # Original WLED files (preserved)
    ├── wled00/                     # Original WLED source
    ├── usermods/                   # WLED user modifications
    ├── tools/                      # WLED tools and utilities
    └── ...                         # Other WLED files
```

The `legacy/` folder contains all original WLED files for reference, while the root directory contains only the ArkLights-specific implementation.

## Features

### 🎯 PEV-Focused Design
- **Simplified Architecture**: Clean, maintainable codebase without WLED bloat
- **Embedded Optimized**: Designed for devices that are hard to update once installed
- **Mobile-First**: Built for devices that move and aren't connected to home WiFi
- **Low Power**: Optimized for battery-powered devices

### 💡 LED Control
- **Multi-Segment Support**: Headlight, taillight, and underglow segments
- **Core Effects**: Solid, breath, rainbow, chase, blink rainbow, twinkle
- **Motion Effects**: Automatic blinkers, parking mode, impact detection
- **Preset Modes**: Standard, Night, Party, Stealth modes

### 🎛️ Motion Control (MPU6050)
- **Auto Blinkers**: Automatic turn signals based on lean angle and acceleration
- **Parking Mode**: Automatic effect activation when stationary and tilted
- **Impact Detection**: Flash all lights on impact detection
- **Calibration System**: Orientation-independent motion detection

### 📡 Communication
- **ESP-NOW Sync**: Synchronize effects between nearby devices
- **Bluetooth API**: Mobile app control via Bluetooth
- **Serial Commands**: Debug and control via serial interface

## Architecture

```
src/
├── core/
│   ├── arklights_core.h/cpp    # Main system class
│   ├── led_segment.h/cpp       # LED strip management
│   ├── effect_engine.h/cpp     # Effect algorithms
│   ├── motion_controller.h/cpp # MPU6050 motion detection
│   └── communication_manager.h/cpp # ESP-NOW + Bluetooth
└── main.cpp                    # Arduino main loop
```

## Hardware Setup

### Required Components
- ESP32-S3 development board
- MPU6050 IMU sensor
- WS2812B/SK6812 LED strips (2-3 segments)
- Appropriate power supply

### Wiring
```
MPU6050 → ESP32-S3
VCC → 3.3V
GND → GND
SDA → GPIO 5
SCL → GPIO 6

LED Strips → ESP32-S3
Headlight → GPIO 2
Taillight → GPIO 3
Underglow → GPIO 4 (optional)
```

## Software Setup

### 1. Install Dependencies
```bash
# Install PlatformIO
pip install platformio

# Clone and build
git clone <your-repo>
cd ARKLIGHTS_WLED
pio run -e arklights_esp32s3
```

### 2. Configure Hardware
Edit `src/main.cpp` to match your hardware:
```cpp
// LED segment configuration
segments[SEGMENT_HEADLIGHT] = new LEDSegment(0, 20, 2);   // 20 LEDs on pin 2
segments[SEGMENT_TAILLIGHT] = new LEDSegment(20, 20, 3);   // 20 LEDs on pin 3
segments[SEGMENT_UNDERGLOW] = new LEDSegment(40, 30, 4);  // 30 LEDs on pin 4
```

### 3. Upload Firmware
```bash
pio run -e arklights_esp32s3 -t upload
```

## Usage

### Serial Commands
Connect to the device via serial (115200 baud):

```
p0-p3     Set preset (0=Standard, 1=Night, 2=Party, 3=Stealth)
b<0-255>  Set brightness
h<hex>    Set headlight color (e.g., hFF0000 for red)
t<hex>    Set taillight color
e<0-8>    Set effect (0=Solid, 1=Breath, 2=Rainbow, etc.)
mb        Enable blinkers
mp        Enable parking mode
ms<float> Set motion sensitivity (e.g., ms1.5)
sync<group> Enable sync with group number
cal       Start motion calibration
reset     Reset motion calibration
status    Show current status
help      Show all commands
```

### Bluetooth API
The system provides a JSON-based Bluetooth API for mobile app control:

```json
{
  "brightness": 200,
  "preset": 1,
  "headlight": {
    "color": 16777215,
    "effect": 0,
    "enabled": true
  },
  "taillight": {
    "color": 16711680,
    "effect": 1,
    "enabled": true
  },
  "motion": {
    "blinkers": true,
    "parking": true,
    "sensitivity": 1.0
  },
  "sync": {
    "enabled": true,
    "group": 1
  }
}
```

### ESP-NOW Synchronization
Devices can synchronize effects automatically:
- Set the same sync group number on multiple devices
- Effects will sync in real-time between devices
- Motion effects remain local to each device

## Preset Modes

### Standard Mode (p0)
- Headlight: Bright white
- Taillight: Red
- Brightness: 200
- Optimal for daytime riding

### Night Mode (p1)
- Headlight: Maximum brightness white
- Taillight: Red with breathing effect
- Brightness: 255
- Enhanced visibility for low-light conditions

### Party Mode (p2)
- Headlight: White
- Taillight: Rainbow effect
- Brightness: 180
- Fun, attention-grabbing effects

### Stealth Mode (p3)
- Headlight: Dim white
- Taillight: Dim red
- Brightness: 50
- Minimal visibility for stealth riding

## Motion Detection

### Blinker System
- Automatically detects left/right turns based on lean angle
- Configurable sensitivity and thresholds
- Overrides normal effects when active
- Works independently on each device

### Parking Mode
- Activates when device is stationary and tilted
- Shows breathing effect to indicate parked status
- Configurable tilt threshold

### Impact Detection
- Detects sudden acceleration changes
- Flashes all lights white briefly
- Useful for crash detection and visibility

## Development

### Adding New Effects
1. Add effect ID to `arklights_core.h`
2. Implement effect function in `effect_engine.cpp`
3. Add to effects array in constructor

### Adding New Motion Features
1. Extend `MotionController` class
2. Add detection logic in `update()` method
3. Add configuration options

### Customizing Hardware
1. Modify LED segment configuration in `main.cpp`
2. Adjust pin assignments as needed
3. Update build flags in `platformio_arklights.ini`

## Comparison with WLED

| Feature | WLED | ArkLights |
|---------|------|-----------|
| Code Size | ~500KB | ~100KB |
| Memory Usage | High | Low |
| WiFi Required | Yes | No |
| Web Server | Yes | No |
| MQTT/Alexa | Yes | No |
| Motion Control | Usermod | Built-in |
| ESP-NOW Sync | Yes | Yes |
| Bluetooth | No | Yes |
| PEV Focused | No | Yes |
| Embedded Friendly | No | Yes |

## License

This project maintains the same license as the original WLED project.

## Acknowledgments

- Original WLED project by AirCookie
- MPU6050 integration using Adafruit libraries
- PEV community for inspiration and testing

---

**🛹 Ride safe and be visible!**
