Usermod to allow WLED to receive via UDP port from RGB.NET (and therefore add as a device to be controlled within artemis on PC)

This is only a very simple code to support a single led strip, it does not support the full function of the RGB.NET sketch for esp8266 only what is needed to be used with Artemis. It will show as a ws281x device in artemis when you provide the correct hostname or ip. Artemis queries the number of LEDs via the web interface (/config) but communication to set the LEDs is all done via the UDP interface.

To install, copy the usermod.cpp file to wled00 folder and recompile