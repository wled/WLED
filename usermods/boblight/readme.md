# BobLight usermod

This usermod allows displaying BobLight ambilight protocol on WLED device with a limited command set (not a full implementation).
BobLight protocol uses a TCP connection which guarantees packet delivery at the possible expense of latency delays. It is not very efficient (as it uses plaintext comands) so is not suited for large number of LEDs.

This implementation is intended for TV backlight in combination with XBMC/Kodi BobLight add-on.

The LEDs can be configured in usermod settings page. The configuration is simple: you enter the number of LED pixels on each side of your TV (top, right, bottom, left).
The LEDs should be wired in a clockwise orientation starting in the middle of bottom side (left half of bottom leds is where the string should start).

```
+-------->-------+
|                |
^                v
|                |
+---<--+  ---<---+
       ^
     start
```

## Installation 

Add `boblight` to `custom_usermods` in your PlatformIO environment.

## Configuration

All parameters are runtime configurable though changing port may require reboot.

If you want to define default port during compile time use the following (default values in parentheses):

- `BOB_PORT=x` : defines default TCP port for usermod to listen on (19333)


## Release notes

2022-11 Initial implementation by @blazoncek (AKA Blaz Kristan)
