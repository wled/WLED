# PIR Sensor Switch v2

**By rawframe**

## Background

Back in the day, I added PIR functionality to WLED before it became a built-in feature. Since then, a PIR usermod was added (credit to @gegu & @blazoncek! for PIR_sensor_switch). 

However, I needed something more flexible for my setup - multiple PIRs triggering different actions in various combinations. The official version works great for simple cases, but when you need complex linking between sensors and actions, macro chains get messy real quick, and moreover we still cannot easily have seemless fx (i.e avoid restarting an fx animation when called again), So I built this.

## Screenshots

![alt text](<Usermod Settings Page (3x PIRs, 3x Actions example).jpg>)
![alt text](<Usermod Info Page (3x PIRs, 3x Actions example).jpg>)

## What It Does

### Multiple PIRs
- Support for multiple PIR sensors (configurable up to 8)
- Each PIR can be independently enabled/disabled
- Configure different GPIO pins for each sensor
- Real-time status monitoring in the Web UI

### Flexible Actions
- Define multiple "Actions" (also configurable up to 8)
- Each Action has:
  - **On Preset** - triggers when motion detected
  - **Off Preset** - triggers when motion stops (with delay)
  - **Off Delay** - how long to wait before triggering the off preset
  - **Enable/Disable toggle** - turn actions on/off without losing settings

### Smart Linking
- Link any PIR to any Action (many-to-many relationships)
- One PIR can trigger multiple Actions
- One Action can be triggered by multiple PIRs
- Actions stay active as long as ANY linked PIR detects motion
- Off timers only start when ALL linked PIRs go idle

### Web UI
- Toggle PIRs and Actions on/off directly from the Info page
- See motion status in real-time (● motion / ○ idle)
- View countdown timers for off delays
- Config page for full setup (pins, presets, delays, linking)

### API Control
- Enable/disable PIRs via JSON API: `{"MotionDetection":{"pir0":true}}`
- Enable/disable Actions via JSON API: `{"MotionDetection":{"action0":true}}`
- Integrates with Home Assistant, Node-RED, etc.

## Use Cases

This helps if you need:
- Multiple PIRs in different rooms/zones
- Different lighting presets for different sensors
- Combined motion detection (e.g., "turn off only when ALL sensors are idle")
- Independent motion zones that don't interfere with each other
- Complex automation without macro spaghetti

## Technical Bits

- Uses a preset FIFO queue to prevent flooding
- Optimized RAM usage with bitmasks instead of 2D arrays, but you could change this if you need more than 8 pirs or actions.
- Contributor tracking system ensures Actions behave correctly with multiple PIRs
- Persistent configuration saved to `cfg.json`
- Fully compatible with WLED's preset system

## Notes

- To add more pirs or actions, just change the defines in the the cpp file (defaults) or the platformio.ini / override
  - PIR_SENSOR_MAX
  - ACTION_MAX

- Add if anyone wants to add the other options from the @gegu & @blazoncek usermod then feel free to do so.

