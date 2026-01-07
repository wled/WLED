# ArkLights Android App

A clean Android application for managing ArkLights PEV LED systems over Bluetooth.

## Features

### Main Controls
- **Presets**: Quick access to Standard, Night, Party, and Stealth modes
- **Headlight Control**: Color picker and effect selection
- **Taillight Control**: Color picker and effect selection  
- **Brightness Control**: Global brightness slider (0-255)
- **Effect Speed**: Animation speed control (0-255)
- **Motion Control**: Enable/disable motion features with sensitivity adjustment

### Settings
- **Calibration**: Device calibration for motion sensors
- **Motion Status**: Real-time motion sensor status
- **Startup Sequence**: Configure power-on animations
- **Advanced Motion Control**: Fine-tune motion detection parameters
- **Park Mode Settings**: Configure park mode effects and colors
- **WiFi Configuration**: Set up device WiFi access point
- **ESPNow Configuration**: Configure mesh networking
- **Group Management**: Create and join group rides
- **LED Configuration**: Configure LED strip settings

## Architecture

### Bluetooth Communication
- Uses Bluetooth Serial Port Profile (SPP) for communication
- Sends HTTP requests over Bluetooth to match existing web API
- Handles device discovery and pairing
- Manages connection state and error handling

### Data Models
- `LEDControlRequest`: API request model for LED control
- `LEDStatus`: Device status response model
- `BluetoothDevice`: Bluetooth device wrapper
- `ConnectionState`: Connection state enumeration

### Services
- `BluetoothService`: Handles Bluetooth communication
- `ArkLightsApiService`: Manages API calls over Bluetooth
- `ArkLightsViewModel`: Manages app state and business logic

### UI Components
- Jetpack Compose-based modern UI
- Tabbed navigation (Main Controls / Settings)
- Real-time status updates
- Material Design 3 theming

## API Compatibility

The Android app replicates the exact same API calls as the web interface:

- `POST /api` - Main control endpoint
- `GET /api/status` - Status information
- `POST /api/led-config` - LED configuration
- `POST /api/led-test` - LED testing

## Permissions

Required permissions:
- `BLUETOOTH` / `BLUETOOTH_CONNECT` (Android 12+)
- `BLUETOOTH_ADMIN` / `BLUETOOTH_SCAN` (Android 12+)
- `ACCESS_FINE_LOCATION` / `ACCESS_COARSE_LOCATION`
- `INTERNET` / `ACCESS_NETWORK_STATE`

## Usage

1. **Pair Device**: Use Android's Bluetooth settings to pair with your ArkLights device
2. **Connect**: Open the app and select your paired device
3. **Control**: Use the main controls tab for quick adjustments
4. **Configure**: Use the settings tab for advanced configuration

## Development

Built with:
- Kotlin
- Jetpack Compose
- Material Design 3
- Kotlinx Serialization
- Ktor Client (for HTTP over Bluetooth)

The app maintains full compatibility with the existing ArkLights firmware and web interface while providing a native Android experience.
