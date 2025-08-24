## POV Display usermod

This usermod adds a new effect called “POV Image”.

To get it working:
- Resize your image. The height must match the number of LEDs in your strip/segment.
- Rotate your image 90° clockwise (height becomes width).
- Upload a BMP image (24-bit, uncompressed) to the ESP filesystem using the “/edit” URL.
- Select the “POV Image” effect.
- Set the segment name to the absolute filesystem path of the image (e.g., “/myimage.bmp”).
- Rotate the segment at approximately 20 RPM.
- Enjoy the show!

Notes:
- Only 24-bit uncompressed BMP files are supported.
- The image must fit into ~64 KB of RAM (width × height × 3 bytes, plus row padding to a 4-byte boundary).
- The path must be absolute.