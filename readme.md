# WLED-based LED Sticks
These portable LED light poles make pretty lights for a dance party - and better yet:

They're **portable**! Convenient tripod bases keep them standing at attention, and a 10Ah USB battery (like the one you charge your phone with) will power them for about 8 hours.

They're **versatile**! You can deploy them in a [wide open space at Burning Man](https://youtu.be/UtXL0ScNUoE), in a [private event space](https://youtu.be/B6OBde7zRN4), or right next to each other [in your living room](https://youtu.be/UmiRBCOAMBg).  (Click into these links for videos of them in action.)

They're **coordinated**! If you put them next to each other, they sync using near-field radio. And they'll even daisy-chain, meaning the furthest ones can be really far apart as long as there are some in the middle to relay the signal.

![IMG_1521](https://github.com/SteveEisner/tubes/blob/master/assembly/poles_in_use.jpg)

They're **sturdy**! I built over 100 of them and have deployed them at parties in all kinds of weather, from hot & dusty windstorms to torrential rain. They've been tossed in a truck, carried around as totems, knocked or blown over, and almost all of 'em are still working.  Just don't let people use them like light sabers (lesson learned.)

They're also **in progress** with lots of things that could be improved.  But for now:

* they play patterns from an expanding set of "genetically-driven" combinations
* they stay in sync or deliberately drift from each other in pleasing ways
* they operate internally at a certain BPM - allowing them to sync to music

## How do they work?

Each light tube is running custom software on an ESP32 microcontroller. The software is running a generative light program, based on the popular software WLED, that has a simple "DNA" specifying its pattern, color, effects overlay, offset, and so on.

The patterns run on a clock that's synced to a specific BPM and counts out the 4/4 rhythm of most dance music, so they morph and change on individual beats, measures, and phrases. After several beat phrases pass, a pole's DNA is mutated a little, which can cause it to change color, or adopt a new pattern, etc.

On-board radio lets a light pole broadcast its DNA and clock timer; when others hear the signal, they can choose to follow along by updating their own pattern DNA to match it. The method of coordination is pretty simple right now: each pole has an 8-bit ID, and lower-ID poles obey higher-ID poles.

If they're all close enough, they'll soon start doing the same thing as a group and keeping their clock timers in sync. In case they're not close enough, poles also re-broadcast all the signals they receive and obey. This lets them pass along a signal through the group until all of them have found it.

Some randomness and chaos is intentional. Radio isn't 100% reliable, so they sometimes fall out of contact and then re-connect. And in some cases, the poles will deliberately offset their own clock a bit so that they are clearly doing the same thing but not exactly at the same time. Each tube is actually running several copies of the software and smoothly "cross-fading" between them, to avoid any jarring transitions.

## Credits

The form factor was inspired by [Mark Lottor's Hexatron](http://www.3waylabs.com/projects/hex/). The color schemes were inspired by [the work of Christopher Schardt](https://americanart.si.edu/exhibitions/burning-man/online/christopher-schardt) and many were found on [cpt-city](http://soliton.vm.bytemark.co.uk/pub/cpt-city/). My most recent versions use the excellent [WLED](https://github.com/Aircoookie/WLED) codebase and run as a usermod. My older code was written using [FastLED](http://fastled.io/) and I still use some original patterns that are evolved versions of FastLED examples. The mesh networking is based on [Chuck Sommerville's led-swarm](https://github.com/chucks13/LED-swarm), and still follows its [theory](https://github.com/chucks13/LED-swarm/blob/master/theory.txt) for radio connection. When I kicked off this project, I used [Paul Stoffregen's WS2812Serial](https://github.com/PaulStoffregen/WS2812Serial) for smooth animation, and of course [his Teensy CPU](https://www.pjrc.com/teensy/teensyLC.html) made the whole thing possible. Standing on the shoulders of these giants let me create these in about a month, in time to debut at Burning Man 2019 and appear at many parties since!

## WLEDtubes branch changes

This is a WLED-based update to my [2019 light tube project](https://github.com/SteveEisner/tubes), which ran on Teensy + FastLED + nRF24L01 radios.

Most of the changes are in the usermod `usermods/Tubes`:
* Tubes is installed as an overlay function, which allows it to run WLED "underneath" but completely control the output.
* Its final output is a composition of multiple layers: it starts with WLED's output, then optionally overwrites with its own patterns, then runs a particle effects library on top of that.
* It stores a curated playlist of composite effects. Some are WLED stock FX, others are custom-built. It moves randomly through that playlist.
* At the time of writing, WLED could not correctly fade transitions between 2 FX. Tubes makes up for that by being able to fade between 1 WLED FX and 1 custom overlay pattern, or between 2 custom overlays. As it moves through the playlist, it ensures that it never plays two WLED FX in a row.
* The particle library runs on top of everything (including WLED FX) and introduces a variety of patterns & blit effects.
* If the user changes effects or palettes in the WLED web UI, Tubes will honor that & stop overlaying for a little while. It will eventually time out and revert to full overlay mode.
* Everything runs on a custom clock that is synced to a specific BPM. The BPM can be changed (manually now, automatic eventually) so that the effects perfectly sync to nearby music. WLED FX don't sync yet but could be speed-adjusted.
* An ESP-Now based mesh network is created, so all devices running this usermod stay in sync with each other, without Wi-Fi.

Mesh networking is based on a unidirectional broadcast protocol:
* Every device (node) is assigned a random 12-bit ID. This ID can change at any time, although in practice it only changes upon reboot.
* A node begins with the assumption that it is the only node, and therefore it is the leader, which means it's the one controlling patterns, palettes, and effects.
* As a node operates, it regularly broadcasts its status via ESP-Now, in case other device nodes are nearby. Nodes also continuously listen for ESP-Now broadcasts with node status.
* Status messages are identified by the node's ID. If a node receives a broadcast from a lower ID, it ignores it.
* When a node receives a broadcast from a higher ID, it assumes that other node must be the leader. It syncs its status to the leader's status & stops broadcasting its own status
* When a node receives a broadcast from the same ID, it assumes there's been an ID assignment collision and randomizes its own ID. (This happens sometimes even in a 12-bit space.)
* Nodes are assumed to be unstable; they can move or be turned off (or crash.) Status packets include both a current status and 30+ seconds of future states. All nodes can continue to run in sync even if they don't hear from the leader during this time.
* If a node hasn't heard from the leader in a long time (20 sec or more), it assumes the leader has permanently left. It reverts to being its own leader again until it hears from a new leader with higher ID.
* To help boost the leader's effective range, a following node will occasionally relay the leader's commands using the leader's ID. This helps sync devices that are out of range of the leader, but within range of a follower. The effective range of a single ESP32 device has been measured at hundreds feet; relays allow for an even larger mesh range.
* There's a protocol for explicit control, with commands that can be sent to specific nodes or all nodes. This allows a single master remote to directly control the entire mesh.
* This has been tested on 75+ devices in proximity, but theoretically can expand until it saturates ESPNow bandwidth (hundreds of devices? thousands? not sure)

The Tubes usermod uses several sub-libraries and helper functions:
* beats.h: an 8-bit bpm library that helps the Tubes run patterns at a specific bpm
* node.h: the ESP-Now based mesh network
* particle.h: a particle effects overlay library
* firmware.sh: successful firmware+config mass-autoupdater
* master.h: a remote that overrides & controls all ESP-Now nodes (run from a separate device)
* timer.h: a tiny library to help with timed events

There are several left-over modules that aren't used any more.
* bluetooth.h: a failed initial attempt to sync over BLE (now unused)
* updater.h + update_server.h: a failed attempt to write a peer-to-peer firmware auto-updater (unreliable)
* sound.h: an initial attempt to create some sound-reactive effect overlays

Also, there a few changes to core library files:
* New brighter, more vivid color palettes (palettes.h + wled00/FX_fcn.cpp)
* New button-press code to allow a single button to handle Wi-Fi protection (it's only turned on by explicit button press) and "Power-save" for battery operation (wled00/button.cpp)
* Fleet provisioning for flashing dozens of WLED controllers (wled00/wled_serial.cpp disabled to allow it)

# Original README: Welcome to WLED! ✨

A fast and feature-rich implementation of an ESP8266/ESP32 webserver to control NeoPixel (WS2812B, WS2811, SK6812) LEDs or also SPI based chipsets like the WS2801 and APA102!

Now with new magical sync powers!

## ⚙️ Features
- WS2812FX library with more than 100 special effects  
- FastLED noise effects and 50 palettes  
- Modern UI with color, effect and segment controls  
- Segments to set different effects and colors to user defined parts of the LED string  
- Settings page - configuration via the network  
- Access Point and station mode - automatic failsafe AP  
- Up to 10 LED outputs per instance
- Support for RGBW strips  
- Up to 250 user presets to save and load colors/effects easily, supports cycling through them.  
- Presets can be used to automatically execute API calls  
- Nightlight function (gradually dims down)  
- Full OTA software updateability (HTTP + ArduinoOTA), password protectable  
- Configurable analog clock (Cronixie, 7-segment and EleksTube IPS clock support via usermods) 
- Configurable Auto Brightness limit for safe operation  
- Filesystem-based config for easier backup of presets and settings  

## 💡 Supported light control interfaces
- WLED app for [Android](https://play.google.com/store/apps/details?id=com.aircoookie.WLED) and [iOS](https://apps.apple.com/us/app/wled/id1475695033)
- JSON and HTTP request APIs  
- MQTT   
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

[On this page](https://kno.wled.ge/basics/tutorials/) you can find excellent tutorials and tools to help you get your new project up and running!

## 🖼️ User interface
<img src="/images/macbook-pro-space-gray-on-the-wooden-table.jpg" width="50%"><img src="/images/walking-with-iphone-x.jpg" width="50%">

## 💾 Compatible hardware

See [here](https://kno.wled.ge/basics/compatible-hardware)!

## ✌️ Other

Licensed under the MIT license  
Credits [here](https://kno.wled.ge/about/contributors/)!

Join the Discord server to discuss everything about WLED!

<a href="https://discord.gg/QAh7wJHrRM"><img src="https://discordapp.com/api/guilds/473448917040758787/widget.png?style=banner2" width="25%"></a>

Check out the WLED [Discourse forum](https://wled.discourse.group)!  

You can also send me mails to [dev.aircoookie@gmail.com](mailto:dev.aircoookie@gmail.com), but please, only do so if you want to talk to me privately.  

If WLED really brightens up your day, you can [![](https://img.shields.io/badge/send%20me%20a%20small%20gift-paypal-blue.svg?style=flat-square)](https://paypal.me/aircoookie)


*Disclaimer:*   

If you are prone to photosensitive epilepsy, we recommended you do **not** use this software.  
If you still want to try, don't use strobe, lighting or noise modes or high effect speed settings.

As per the MIT license, I assume no liability for any damage to you or any other person or equipment.  

