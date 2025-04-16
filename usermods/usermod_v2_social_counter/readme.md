# Social Counter Usermod

This usermod allows you to display follower counts from various social networks on a 7-segment display using WLED LEDs.

## Installation

There are two ways to enable this usermod:

### Method 1: Using platformio_override.ini (Recommended)

Create or modify your `platformio_override.ini` file to include the Social Counter:

```ini
[env:esp32dev_social_counter]
extends = env:esp32dev
custom_usermods = social_counter
```

### Method 2: Using the compile option

Add the compile-time option `-D USERMOD_SOCIAL_COUNTER` to your `platformio.ini` (or `platformio_override.ini`) or use `#define USERMOD_SOCIAL_COUNTER` in `my_config.h`.

## Usage

After installation, configure the module through the WLED interface:

1. Access the WLED web interface
2. Go to "Config" > "Usermods"
3. Find the "Social Counter" section
4. Set:
   - Enable/disable the module
   - Desired social network (Instagram, TikTok, Twitch, YouTube)
   - Link to your profile
   - Update interval (in seconds)
   - WLED segment index to use for display
   - LEDs per segment
   - Digit spacing
   - Segment direction

## Supported Social Networks

- Instagram
- TikTok
- Twitch
- YouTube

## 7-Segment Display

This usermod uses a 7-segment display to show the follower count:

- Segments are arranged in the FABCDEG pattern
- You can define how many LEDs each segment uses
- The display automatically uses WLED segments and their defined colors
- The maximum number of digits displayed is automatically calculated based on segment size

## Customization

The code uses the Strategy pattern to implement different social networks. You can easily add support for new social networks by creating a new strategy class in the `strategies` folder.

## File Structure

- `usermod_v2_social_counter.cpp` - Main usermod implementation
- `SocialNetworkTypes.h` - Social network type definitions
- `SocialNetworkFactory.h` - Factory for creating strategy instances
- `strategies/` - Folder containing implementations for each social network
  - `SocialNetworkStrategy.h` - Interface for strategies
  - `InstagramStrategy.h` - Instagram implementation
  - `TikTokStrategy.h` - TikTok implementation
  - `TwitchStrategy.h` - Twitch implementation
  - `YouTubeStrategy.h` - YouTube implementation

## Debugging

If you're experiencing issues:

1. Enable serial logging in WLED
2. Check for messages with the "Social Counter" prefix in the serial output
3. Verify that the time interval configuration is correct
