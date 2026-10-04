#include "pov.h"

POV::POV() {}

void POV::showLine(const byte * line, uint16_t size){
    uint16_t i, pos;
    uint8_t r, g, b;
    if (!line) {
        // All-black frame on null input
        for (i = 0; i < SEGLEN; i++) {
            SEGMENT.setPixelColor(i, CRGB::Black);
        }
        strip.show();
        lastLineUpdate = micros();
        return;
    }
    for (i = 0; i < SEGLEN; i++) {
        if (i < size) {
            pos = 3 * i;
            // using bgr order
            b = line[pos++];
            g = line[pos++];
            r = line[pos];
            SEGMENT.setPixelColor(i, CRGB(r, g, b));
        } else {
            SEGMENT.setPixelColor(i, CRGB::Black);
        }
    }
    strip.show();
    lastLineUpdate = micros();
}

bool POV::loadImage(const char * filename){
  if(!image.init(filename)) return false;
  if(!image.load()) return false;
  currentLine=0;
  return true;
}

int16_t POV::showNextLine(){
    if (!image.isLoaded()) return 0;
    //move to next line
    showLine(image.line(currentLine), image.width());
    currentLine++;
    if (currentLine == image.height()) {currentLine=0;}
    return currentLine;
}
