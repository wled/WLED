<p align="center">
  <img src="/images/wled_logo_akemi.png">
  <a href="https://github.com/Aircoookie/WLED/releases"><img src="https://img.shields.io/github/release/Aircoookie/WLED.svg?style=flat-square"></a>
  <a href="https://raw.githubusercontent.com/Aircoookie/WLED/master/LICENSE"><img src="https://img.shields.io/github/license/Aircoookie/wled?color=blue&style=flat-square"></a>
  <a href="https://wled.discourse.group"><img src="https://img.shields.io/discourse/topics?colorB=blue&label=forum&server=https%3A%2F%2Fwled.discourse.group%2F&style=flat-square"></a>
  <a href="https://discord.gg/KuqP7NE"><img src="https://img.shields.io/discord/473448917040758787.svg?colorB=blue&label=discord&style=flat-square"></a>
  <a href="https://kno.wled.ge"><img src="https://img.shields.io/badge/quick_start-wiki-blue.svg?style=flat-square"></a>
  <a href="https://github.com/Aircoookie/WLED-App"><img src="https://img.shields.io/badge/app-wled-blue.svg?style=flat-square"></a>
  <a href="https://gitpod.io/#https://github.com/Aircoookie/WLED"><img src="https://img.shields.io/badge/Gitpod-ready--to--code-blue?style=flat-square&logo=gitpod"></a>

  </p>
  
# WLED on hexagon structures ⬡
This fork of WLED implements further effects designed for hexagon structures (specifically a struture of 3 * 5 hexagons (a total of 60 edges) with an edge length of 10 LEDs (=16.67 cm with a 60 LEDs/m strip). The basic functionality of WLED is still given. Current added effects include:
- 6 rotating hexagons (color adapts to chosen palette)
- 6 reverse rotating hexagons (color adapts to chosen palette)
- randomly strobing edges (color adapts to chosen palette)
- matrix style descending stripes on the 6 "vertical" edges (color adapts to chosen palette)
- matrix style ascending stripes on the 6 "vertical" edges (color adapts to chosen palette)
- more to come 

Original project description of WLED by Aircoookie below.

# Welcome to my project WLED! ✨

A fast and feature-rich implementation of an ESP8266/ESP32 webserver to control NeoPixel (WS2812B, WS2811, SK6812) LEDs or also SPI based chipsets like the WS2801 and APA102!

## ⚙️ Features
- WS2812FX library integrated for over 100 special effects  
- FastLED noise effects and 50 palettes  
- Modern UI with color, effect and segment controls  
- Segments to set different effects and colors to parts of the LEDs  
- Settings page - configuration over network  
- Access Point and station mode - automatic failsafe AP  
- Up to 10 LED outputs per instance
- Support for RGBW strips  
- Up to 250 user presets to save and load colors/effects easily, supports cycling through them.  
- Presets can be used to automatically execute API calls  
- Nightlight function (gradually dims down)  
- Full OTA software updatability (HTTP + ArduinoOTA), password protectable  
- Configurable analog clock (Cronixie, 7-segment and EleksTube IPS clock support via usermods) 
- Configurable Auto Brightness limit for safer operation  
- Filesystem-based config for easier backup of presets and settings  

## 💡 Supported light control interfaces
- WLED app for [Android](https://play.google.com/store/apps/details?id=com.aircoookie.WLED) and [iOS](https://apps.apple.com/us/app/wled/id1475695033)
- JSON and HTTP request APIs  
- MQTT  
- Blynk IoT  
- E1.31, Art-Net, DDP and TPM2.net
- [diyHue](https://github.com/diyhue/diyHue) (Wled is supported by diyHue, including Hue Sync Entertainment under udp. Thanks to [Gregory Mallios](https://github.com/gmallios))
- [Hyperion](https://github.com/hyperion-project/hyperion.ng)
- UDP realtime  
- Alexa voice control (including dimming and color)  
- Sync to Philips hue lights  
- Adalight (PC ambilight via serial) and TPM2  
- Sync color of multiple WLED devices (UDP notifier)  
- Infrared remotes (24-key RGB, receiver required)  
- Simple timers/schedules (time from NTP, timezones/DST supported)  

## 📲 Quick start guide and documentation

See the [documentation on our official site](https://kno.wled.ge)!

[On this page](https://kno.wled.ge/basics/tutorials/) you can find excellent tutorials made by the community and helpful tools to help you get your new lamp up and running!

## 🖼️ User interface
<img src="/images/macbook-pro-space-gray-on-the-wooden-table.jpg" width="50%"><img src="/images/walking-with-iphone-x.jpg" width="50%">

## 💾 Compatible hardware

See [here](https://kno.wled.ge/basics/compatible-hardware)!

## ✌️ Other

Licensed under the MIT license  
Credits [here](https://kno.wled.ge/about/contributors/)!

Join the Discord server to discuss everything about WLED!

<a href="https://discord.gg/KuqP7NE"><img src="https://discordapp.com/api/guilds/473448917040758787/widget.png?style=banner2" width="25%"></a>

Check out the WLED [Discourse forum](https://wled.discourse.group)!  
You can also send me mails to [dev.aircoookie@gmail.com](mailto:dev.aircoookie@gmail.com), but please only do so if you want to talk to me privately.  
If WLED really brightens up your every day, you can [![](https://img.shields.io/badge/send%20me%20a%20small%20gift-paypal-blue.svg?style=flat-square)](https://paypal.me/aircoookie)


*Disclaimer:*   
If you are sensitive to photosensitive epilepsy it is not recommended that you use this software.  
In case you still want to try, don't use strobe, lighting or noise modes or high effect speed settings.
As per the MIT license, I assume no liability for any damage to you or any other person or equipment.  

