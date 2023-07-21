# EleksTube IPS Clock usermod

This usermod allows WLED to run on the EleksTube IPS clock.
It enables running all WLED effects on the background SK6812 lighting, while displaying digit bitmaps on the 6 IPS screens.
Code is largely based on https://github.com/SmittyHalibut/EleksTubeHAX by Mark Smith!

Supported:
- Display with custom bitmaps (.bmp) or raw RGB565 images (.bin) from filesystem
- Background lighting
- All 4 hardware buttons
- RTC (with RTC usermod)
- Standard WLED time features (NTP, DST, timezones)

Not supported:
- On-device setup with buttons (WiFi setup only)

Your images must be 1-135 pixels wide and 1-240 pixels high.
BMP 1, 4, 8, and 24 bits per pixel formats are supported.

## Installation 

Compile and upload to clock using the `elekstube_ips` PlatformIO environment
Once uploaded (the clock can be flashed like any ESP32 module), go to `[WLED-IP]/edit` and upload the 0-9.bin files from [here](https://github.com/Aircoookie/NixieThemes/tree/master/themes/RealisticNixie/bin).
You can find more clockfaces in the [NixieThemes](https://github.com/Aircoookie/NixieThemes/) repo.
Use LED pin 12, relay pin 27 and button pin 34.

## Use of RGB565 images

Binary 16-bit per pixel RGB565 format `.bin` and `.clk` images are now supported. This has the benefit of using only 2/3rds of the file space a 24 BPP `.bmp` occupies.
The drawback is this format cannot be handled by common image programs and an extra conversion step is needed.
You can use https://lvgl.io/tools/imageconverter to convert your .bmp to a .bin file (settings `True color` and `Binary RGB565`).  
Thank you to @RedNax67 for adding .bin and .clk support.  
For most clockface designs, using 4 or 8 BPP BMP format will reduce file size even more:

| Bits per pixel | File size in kB (for 135x240 img) | % of 24 BPP BMP | Max unique colors
| --- | --- | --- | --- |
24 | 98 | 100% | 16M (66K)
16 (.clk) | 64.8 | 66% | 66K
8 | 33.7 | 34% | 256
4 | 16.4 | 17% | 16
1 | 4.9 | 5% | 2

Comparison 1 vs. 4 vs. 8 vs. 24 BPP. With this clockface on the actual clock, 4 bit looks good, and 8 bit is almost indistinguishable from 24 bit.

![comparison](https://user-images.githubusercontent.com/21045690/156899667-5b55ed9f-6e03-4066-b2aa-1260e9570369.png)
