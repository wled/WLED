# 🛹 WLED Onewheel Fork

This is a custom fork of WLED specifically designed for Onewheel LED lighting with motion-controlled features using an MPU6050 IMU sensor.

## Features

### 🎯 Onewheel-Specific UI
- **Simplified Interface**: Clean, mobile-friendly UI designed for Onewheel use
- **Quick Mode Switching**: Easy toggle between Standard and Night Riding modes
- **Real-time Status**: Live display of motion sensor data and system status
- **Touch-Friendly**: Optimized for mobile devices and outdoor use

### 🎛️ Motion Controls (MPU6050)
- **Auto Blinkers**: Automatic turn signals based on lean angle and acceleration
- **Parking Mode**: Automatic effect activation when stationary and tilted
- **Tilt Monitoring**: Real-time pitch and roll angle display
- **Configurable Thresholds**: Customizable sensitivity for all motion detection

### 💡 Lighting Modes

#### Standard Mode
- **Headlight**: Bright white front lighting (200 brightness)
- **Taillight**: Red rear lighting with customizable effects
- **Optimal** for daytime riding and normal visibility conditions

#### Night Riding Mode  
- **Headlight**: Maximum brightness white (255 brightness)
- **Taillight**: Red with effects enabled for maximum visibility
- **Enhanced visibility** for low-light and night riding

### ✨ Effect Selection
Curated selection of effects suitable for Onewheel strips:
- **Solid**: Static colors
- **Breathe**: Gentle pulsing effect
- **Rainbow**: Moving rainbow pattern
- **Chase**: Theater chase pattern
- **Blink Rainbow**: Attention-grabbing blinking
- **Twinkle**: Sparkle effects

## Hardware Setup

### Required Components
- ESP32 development board (recommended: ESP32-S3 or similar)
- MPU6050 IMU sensor
- WS2812B LED strips (2 segments: headlight + taillight)
- Appropriate power supply for your LED count

### Wiring
```
MPU6050 → ESP32
VCC → 3.3V
GND → GND
SDA → GPIO 5
SCL → GPIO 6
```

### LED Segments
Configure two segments in WLED:
- **Segment 0**: Headlight (front LEDs)
- **Segment 1**: Taillight (rear LEDs)

## Software Setup

### 1. Enable the MPU6050 Usermod
Add to your `platformio.ini`:
```ini
build_flags = 
  -D USERMOD_MPU6050_IMU
  ; other flags...

lib_deps = 
  adafruit/Adafruit MPU6050@^2.2.4
  adafruit/Adafruit Unified Sensor@^1.1.9
  ; other dependencies...
```

### 2. Configure Usermod
In `usermods_list.cpp`, add:
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

## Using the Onewheel Interface

### Accessing the UI
1. **Default**: Navigate to `http://your-wled-ip/onewheel.htm`
2. **From Main UI**: Click the "Onewheel" button in the main interface
3. **Direct Access**: Bookmark the onewheel.htm page for quick access

### Controls

#### Power Management
- **Power Button**: Main on/off control with visual feedback
- **Brightness Slider**: Adjust overall brightness (1-255)

#### Riding Modes
- **Standard Mode**: Normal daytime riding setup
- **Night Mode**: Maximum visibility for low-light conditions

#### Motion Controls
- **Auto Blinkers**: Toggle automatic turn signal detection
- **Parking Effect**: Enable/disable automatic parking mode effects

#### Light Controls
- **Headlight Toggle**: Enable/disable front lighting
- **Taillight Toggle**: Enable/disable rear lighting
- **Effect Selection**: Choose taillight effects

#### Status Display
Real-time information showing:
- System power status
- Current riding mode
- MPU sensor connection status
- Blinker activity (direction when active)
- Parking mode status
- Live tilt angles (pitch/roll)

## MPU6050 Configuration

### Motion Detection Settings
Access via WLED Settings → Usermods:

- **Turn Detection**: Roll angle threshold for blinkers (default: 20°)
- **Yaw Threshold**: Gyroscope threshold for turn detection (default: 20°/s)
- **Acceleration Threshold**: G-force threshold for motion (default: 0.2g)
- **Blinker Delay**: Time before blinker activates (default: 500ms)
- **End Blinker Delay**: Time to keep blinker active after turn (default: 1500ms)

### Parking Mode Settings
- **Park Detection**: Pitch angle for parking trigger (default: 15°)
- **Park FX Mode**: Effect to use when parked (default: Blink Rainbow)
- **Park FX Color**: Color for parking effect (default: Blue)

## Customization

### Adding Effects
To add more effects to the selection, modify `onewheel.htm`:
```html
<button class="effect-button" onclick="setEffect(EFFECT_ID)">Effect Name</button>
```

Find effect IDs in `wled00/FX.h` (e.g., `#define FX_MODE_RAINBOW 8`)

### Styling
The interface uses CSS custom properties for easy theming:
- Primary color: `#4CAF50` (green)
- Background: `#1a1a1a` (dark gray)
- Cards: `#2a2a2a` (medium gray)

### Advanced Configuration
For advanced users, you can modify:
- **Motion thresholds** in the MPU6050 usermod
- **Default colors** for each mode
- **Effect parameters** and timing
- **UI layout** and styling

## Troubleshooting

### MPU6050 Not Detected
1. Check wiring connections
2. Verify I2C address (usually 0x68)
3. Ensure sufficient power supply
4. Check for I2C conflicts with other devices

### Blinkers Not Working
1. Verify MPU6050 is connected and reporting data
2. Check sensitivity settings - may need adjustment for your riding style
3. Ensure blinkers are enabled in the interface
4. Monitor tilt values in the status display

### Effects Not Changing
1. Ensure LEDs are powered on
2. Check segment configuration (must have 2 segments)
3. Verify LED strip connectivity
4. Check power supply capacity

## Development

### Building from Source
1. Clone this repository
2. Install PlatformIO
3. Configure your board in `platformio.ini`
4. Add required dependencies
5. Build and upload

### Contributing
This fork focuses specifically on Onewheel use cases. Contributions should:
- Enhance the riding experience
- Improve motion detection
- Add relevant effects or features
- Maintain the simplified UI approach

## License
This project maintains the same license as the original WLED project.

## Acknowledgments
- Original WLED project by AirCookie
- MPU6050 integration using Adafruit libraries
- Onewheel community for inspiration and testing

---

**🛹 Ride safe and be visible!**