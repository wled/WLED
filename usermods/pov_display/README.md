## POV Display usermod

This usermod adds a new effect called “POV Image”.

![the usermod at work](pov_display.gif?raw=true)

###How does it work?
With proper configuration (see below) the main segment will display a single row of pixels from an image stored on the ESP.
It displays the image row by row at a high refresh rate.
If you move the pixel segment at the right speed, you will see the full image floating in the air thanks to the persistence of vision.
RGB LEDs only (no RGBW), with grouping set to 1 and spacing set to 0.
Best results with high-density strips (e.g., 144 LEDs/m).

To get it working:
- Resize your image. The height must match the number of LEDs in your strip/segment.
- Rotate your image 90° clockwise (height becomes width).
- Upload a BMP image (24-bit, uncompressed) to the ESP filesystem using the “/edit” URL.
- Select the “POV Image” effect.
- Set the segment name to the absolute filesystem path of the image (e.g., “/myimage.bmp”).
- The path is case-sensitive and must start with “/”.
- Rotate the pixel strip at approximately 20 RPM.
- Tune as needed so that one full revolution maps to the image width (if the image appears stretched or compressed, adjust RPM slightly).
- Enjoy the show!

Notes:
- Only 24-bit uncompressed BMP files are supported.
- The image must fit into ~64 KB of RAM (width × height × 3 bytes, plus row padding to a 4-byte boundary).
- Examples (approximate, excluding row padding):
  - 128×128 (49,152 bytes) fits.
  - 160×160 (76,800 bytes) does NOT fit.
  - 96×192 (55,296 bytes) fits; padding may add a small overhead.
- If the rendered image appears mirrored or upside‑down, rotate 90° the other way or flip horizontally in your editor and try again.
- The path must be absolute.

### Requirements
- 1D rotating LED strip/segment (POV setup). Ensure the segment length equals the number of physical LEDs.
- BMP image saved as 24‑bit, uncompressed (no alpha, no palette).
- Sufficient free RAM (~64 KB) for the image buffer.

### Troubleshooting
- Nothing displays: verify the file exists at the exact absolute path (case‑sensitive) and is a 24‑bit uncompressed BMP.
- Garbled colors or wrong orientation: re‑export as 24‑bit BMP and retry the rotation/flip guidance above.
- Image too large: reduce width and/or height until it fits within ~64 KB (see examples).
- Path issues: confirm you uploaded the file via the “/edit” URL and can see it in the filesystem browser.

### Safety
- Secure the rotating assembly and keep clear of moving parts.
- Balance the strip/hub to minimize vibration before running at speed.