<p align="center">
  <img src="/images/wled_logo_akemi.png">
  <a href="https://github.com/wled-dev/WLED/releases"><img src="https://img.shields.io/github/release/wled-dev/WLED.svg?style=flat-square"></a>
  <a href="https://raw.githubusercontent.com/wled-dev/WLED/main/LICENSE"><img src="https://img.shields.io/github/license/wled-dev/wled?color=blue&style=flat-square"></a>
  <a href="https://wled.discourse.group"><img src="https://img.shields.io/discourse/topics?colorB=blue&label=forum&server=https%3A%2F%2Fwled.discourse.group%2F&style=flat-square"></a>
  <a href="https://discord.gg/QAh7wJHrRM"><img src="https://img.shields.io/discord/473448917040758787.svg?colorB=blue&label=discord&style=flat-square"></a>
  <a href="https://kno.wled.ge"><img src="https://img.shields.io/badge/quick_start-wiki-blue.svg?style=flat-square"></a>
  <a href="https://github.com/Aircoookie/WLED-App"><img src="https://img.shields.io/badge/app-wled-blue.svg?style=flat-square"></a>
  <a href="https://gitpod.io/#https://github.com/wled-dev/WLED"><img src="https://img.shields.io/badge/Gitpod-ready--to--code-blue?style=flat-square&logo=gitpod"></a>
</p>

# Welcome to WLED! ✨

A fast and feature-rich firmware for ESP32 microcontrollers to control addressable LEDs — from simple strips to large 2D matrices and HUB75 panels.

Originally created by [Aircoookie](https://github.com/Aircoookie), now maintained by a community of contributors.

## ⚙️ Features

### Effects & Visuals
- **200+ built-in effects** including classic animations, audio-reactive, and 2D/matrix effects
- 50+ color palettes plus a built-in **custom palette editor** (PixelForge)
- **2D LED matrix support** with dedicated 2D effects and flexible panel mapping
- **HUB75 RGB matrix panel support** (ESP32)
- **AudioReactive** effects — included by default, responding to sound via microphone, line-in, or network audio source
- Effect blending for smooth transitions between animations
- Antialiased drawing functions for smooth graphics

### Segments & Control
- **Segments** — apply different effects, colors and palettes to independent parts of your LED setup simultaneously
- Up to **250 presets** to save and recall colors, effects and segment configurations
- Preset playlists for automated cycling and sequencing
- Nightlight function with configurable dimming curve
- Configurable **Auto Brightness Limiter** (per output) for safe operation

### Hardware Support
- **ESP32** (all variants: original, S2, S3, C3)
- **Up to 17 LED outputs** on ESP32 using parallel I2S + RMT
- Addressable LED support: WS2812B, WS2811, WS2815, SK6812, WS2805, TM1914, APA102, WS2801, LPD8806, and many more
- RGBW, RGB+CCT and white-only strips
- PWM outputs for analog LEDs and dimmers
- **Ethernet** support for a wide range of boards (QuinLED, LILYGO, Olimex, and more)
- Filesystem-based config for easy backup and restore of presets and settings
- Full OTA firmware updates (HTTP + ArduinoOTA), password-protectable

### Connectivity & Integrations
- **WLED app** for [Android](https://play.google.com/store/apps/details?id=ca.cgagnier.wlednativeandroid) and [iOS](https://apps.apple.com/gb/app/wled-native/id6446207239)
- JSON and HTTP request APIs
- **Multi-WiFi** — connect to up to 3 networks with automatic AP fallback
- **ESP-NOW** wireless sync between devices (no WiFi router required)
- **MQTT** with Home Assistant discovery
- **E1.31, Art-Net, DDP and TPM2.net** for DMX/professional lighting control
- UDP realtime sync across multiple WLED devices
- Alexa voice control (on/off, brightness, color)
- Philips Hue sync
- [diyHue](https://github.com/diyhue/diyHue) and [Hyperion](https://github.com/hyperion-project/hyperion.ng) integration
- Adalight / TPM2 (PC ambilight via serial)
- Infrared remote control (24-key RGB, receiver required)
- Timers and schedules (NTP time sync, full timezone and DST support)

### Developer-Friendly
- **Usermod system** — extend WLED with community or custom modules without modifying core code
- Large and active [usermod library](https://github.com/wled-dev/WLED/tree/main/usermods) including AudioReactive, temperature sensors, rotary encoders, displays, and much more
- Well-documented [JSON API](https://kno.wled.ge/interfaces/json-api/)
- Licensed under the **EUPL v1.2**

## 📲 Quick start guide and documentation

See the [documentation at kno.wled.ge](https://kno.wled.ge)!

[Tutorials and getting-started guides](https://kno.wled.ge/basics/tutorials/) to help you get your project running quickly.

## 🖼️ User interface

<img src="/images/macbook-pro-space-gray-on-the-wooden-table.jpg" width="50%"><img src="/images/walking-with-iphone-x.jpg" width="50%">

## 💾 Compatible hardware

See the [compatible hardware list](https://kno.wled.ge/basics/compatible-hardware) on the wiki.

## ✌️ Other

Licensed under the [EUPL v1.2](https://raw.githubusercontent.com/wled-dev/WLED/main/LICENSE).  
Credits to all [contributors](https://kno.wled.ge/about/contributors/)!  
CORS proxy by [Corsfix](https://corsfix.com/).

Join the Discord server to discuss everything about WLED!

<a href="https://discord.gg/QAh7wJHrRM"><img src="https://discordapp.com/api/guilds/473448917040758787/widget.png?style=banner2" width="25%"></a>

Check out the WLED [Discourse forum](https://wled.discourse.group)!

If you'd like to reach the original creator privately: [dev.aircoookie@gmail.com](mailto:dev.aircoookie@gmail.com).

If WLED brightens up your day, you can [![](https://img.shields.io/badge/send%20a%20gift-paypal-blue.svg?style=flat-square)](https://paypal.me/aircoookie)

---

*Disclaimer:*

If you are prone to photosensitive epilepsy, we recommend you do **not** use this software.  
If you still want to try, avoid strobe, lightning or noise modes and high effect speed settings.

As per the EUPL license, no liability is assumed for any damage to you or any other person or equipment.
