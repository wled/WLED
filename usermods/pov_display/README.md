##POV Display usermod

this usermod adds a new effect called "POV Image".

To get it working:
- resize your image, the height should be same number of pixels as your led strip.
- rotate your image 90 degrees clockwise (height is now width...)
- upload a bmp image to the ESP filesystem using "/edit" url.
- select "POV Image" effect.
- set the segment name with the absolute path of the image (ie: "/myimage.bmp").
- rotate the segment at approximately 20 RPM.
- enjoy the show!