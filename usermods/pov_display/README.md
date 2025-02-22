# Usermod for POV Display
This usermod helps you display images and graphics on a moving piece of LED strip.
The whole goal of the project is to use images like the one found on this website : https://visualpoi.zone/
or any other image with a similar purpose.
It could be used for LED spoke lights attached to a bike, LED fans, or juggling devices.

## How to make it work?
The images should be the same width as the length of your LED strip, and formatted as PNG.
The usermod also handles mirroring, which means you can "fold" a led strip in two to get the same image displayed on both side.
The effect will first draw the first line of your image, then very rapidly display the second line, etc, until the last line, and then it will loop to the first line.
Think of it as a "2D image to 1D strip converter" effect.
This could be used for lightpainting or to make a cheap hologram.

The images should be uploaded to the filesystem (using /edit endpoint).
To display the image, select "POV Image" in the effect, and set the segment name as the filename of the image (including .png).

To help you to use this project you could also use the GifPlayer from https://github.com/Manut38/WLED-GIFPlayer-html
you would have to update the effect ID in the html file to reflect the effect ID of the "POV Image" effect though.