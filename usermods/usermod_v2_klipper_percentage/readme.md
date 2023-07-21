# Klipper Percentage Usermod
This usermod polls the Klipper API every 10s for the progressvalue.
The leds are then filled with a solid color according to that progress percentage. 
the solid color is the secondary color of the segment.

A corresponding curl command would be:
```
curl --location --request GET 'http://[]/printer/objects/query?virtual_sdcard=progress'
```
## Usage
Compile the source with the buildflag  `-D USERMOD_KLIPPER_PERCENTAGE` added.

You can also use the WLBD bot in the Discord by simply extending an exsisting build enviroment:
```
[env:esp32klipper]
extends = env:esp32dev
build_flags = ${common.build_flags_esp32} -D USERMOD_KLIPPER_PERCENTAGE
```

## Settings 

### Enabled:
Checkbox to enable or disable the overlay

### Klipper IP: 
IP adress of your Klipper instance you want to poll. ESP has to be restarted after change

### Direction : 
0 = normal

1 = reversed

2 = center

-----
Author:

Sören Willrodt

Discord: Sören#5281