#ifndef TFTS_H
#define TFTS_H

#include "wled.h"
#include <FS.h>

#include <TFT_eSPI.h>
#include "Hardware.h"
#include "ChipSelect.h"

class TFTs : public TFT_eSPI {
private:
  uint8_t digits[NUM_DIGITS];


  // These read 16- and 32-bit types from the SD card file.
  // BMP data is stored little-endian, Arduino is little-endian too.
  // May need to reverse subscript order if porting elsewhere.

  uint16_t read16(fs::File &f) {
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
  }

  uint32_t read32(fs::File &f) {
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
  }

  uint16_t output_buffer[TFT_HEIGHT][TFT_WIDTH];
  int16_t w = 135, h = 240, x = 0, y = 0, bufferedDigit = 255;
  uint16_t digitR, digitG, digitB, dimming = 255;
  uint32_t digitColor = 0;

  void drawBuffer() {
    bool oldSwapBytes = getSwapBytes();
    setSwapBytes(true);
    pushImage(x, y, w, h, (uint16_t *)output_buffer);
    setSwapBytes(oldSwapBytes);
  }

  // These BMP functions are stolen directly from the TFT_SPIFFS_BMP example in the TFT_eSPI library.
  // Unfortunately, they aren't part of the library itself, so I had to copy them.
  // I've modified drawBmp to buffer the whole image at once instead of doing it line-by-line.

  //// BEGIN STOLEN CODE

  // Draw directly from file stored in RGB565 format. Fastest
  bool drawBin(const char *filename) {
    fs::File bmpFS;

    // Open requested file on SD card
    bmpFS = WLED_FS.open(filename, "r");

    size_t sz = bmpFS.size();
    if (sz > 64800) {
      bmpFS.close();
      return false;
    }

    uint16_t r, g, b, dimming = 255;
    int16_t row, col;

    //draw img that is shorter than 240pix into the center
    w = 135;
    h = sz / (w * 2);
    x = 0;
    y = (height() - h) /2;
    
    uint8_t lineBuffer[w * 2];

    if (!realtimeMode || realtimeOverride || (realtimeMode && useMainSegmentOnly)) strip.service();

    // 0,0 coordinates are top left
    for (row = 0; row < h; row++) {

      bmpFS.read(lineBuffer, sizeof(lineBuffer));
      uint8_t PixM, PixL;
      
      // Colors are already in 16-bit R5, G6, B5 format
      for (col = 0; col < w; col++)
      {
        if (dimming == 255 && !digitColor) { // not needed, copy directly
          output_buffer[row][col] = (lineBuffer[col*2+1] << 8) | (lineBuffer[col*2]);
        } else {
          // 16 BPP pixel format: R5, G6, B5 ; bin: RRRR RGGG GGGB BBBB
          PixM = lineBuffer[col*2+1];
          PixL = lineBuffer[col*2];
          // align to 8-bit value (MSB left aligned)
          r = (PixM) & 0xF8;
          g = ((PixM << 5) | (PixL >> 3)) & 0xFC;
          b = (PixL << 3) & 0xF8;
          r *= dimming; g *= dimming; b *= dimming;
          r  = r  >> 8; g  = g  >> 8; b  = b  >> 8;
          if (digitColor) { // grayscale pixel coloring
            uint8_t l = (r > g) ? ((r > b) ? r:b) : ((g > b) ? g:b);
            r = g = b = l;
            r *= digitR; g *= digitG; b *= digitB;
            r  = r >> 8; g  = g >> 8; b  = b >> 8;
          }
          output_buffer[row][col] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
      }
    }

    drawBuffer();

    bmpFS.close();

    return true;
  }

  bool drawBmp(const char *filename) {
    fs::File bmpFS;

    // Open requested file on SD card
    bmpFS = WLED_FS.open(filename, "r");

    uint32_t seekOffset, headerSize, paletteSize = 0;
    int16_t row;
    uint16_t r, g, b, dimming = 255, bitDepth;

    uint16_t magic = read16(bmpFS);
    if (magic != ('B' | ('M' << 8))) { // File not found or not a BMP
      Serial.println(F("BMP not found!"));
      bmpFS.close();
      return false;
    }

    (void) read32(bmpFS); // filesize in bytes
    (void) read32(bmpFS); // reserved
    seekOffset = read32(bmpFS); // start of bitmap
    headerSize = read32(bmpFS); // header size
    w = read32(bmpFS); // width
    h = read32(bmpFS); // height
    (void) read16(bmpFS); // color planes (must be 1)
    bitDepth = read16(bmpFS);

    if (read32(bmpFS) != 0 || (bitDepth != 24 && bitDepth != 1 && bitDepth != 4 && bitDepth != 8)) {
      Serial.println(F("BMP format not recognized."));
      bmpFS.close();
      return false;
    }

    uint32_t palette[256];
    if (bitDepth <= 8) // 1,4,8 bit bitmap: read color palette
    {
      (void) read32(bmpFS); (void) read32(bmpFS); (void) read32(bmpFS); // size, w resolution, h resolution
      paletteSize = read32(bmpFS);
      if (paletteSize == 0) paletteSize = 1 << bitDepth; //if 0, size is 2^bitDepth
      bmpFS.seek(14 + headerSize); // start of color palette
      for (uint16_t i = 0; i < paletteSize; i++) {
        palette[i] = read32(bmpFS);
      }
    }

    // draw img that is shorter than 240pix into the center
    x = (width() - w) /2;
    y = (height() - h) /2;

    bmpFS.seek(seekOffset);

    uint32_t lineSize = ((bitDepth * w +31) >> 5) * 4;
    uint8_t lineBuffer[lineSize];
    
    uint8_t serviceStrip = (!realtimeMode || realtimeOverride || (realtimeMode && useMainSegmentOnly)) ? 7 : 0;
    // row is decremented as the BMP image is drawn bottom up
    for (row = h-1; row >= 0; row--) {
      if ((row & 0b00000111) == serviceStrip) strip.service(); //still refresh backlight to mitigate stutter every few rows
      bmpFS.read(lineBuffer, sizeof(lineBuffer));
      uint8_t*  bptr = lineBuffer;
      
      // Convert 24 to 16 bit colors while copying to output buffer.
      for (uint16_t col = 0; col < w; col++)
      {
        if (bitDepth == 24) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
        } else {
          uint32_t c = 0;
          if (bitDepth == 8) {
            c = palette[*bptr++];
          }
          else if (bitDepth == 4) {
            c = palette[(*bptr >> ((col & 0x01)?0:4)) & 0x0F];
            if (col & 0x01) bptr++;
          }
          else { // bitDepth == 1
            c = palette[(*bptr >> (7 - (col & 0x07))) & 0x01];
            if ((col & 0x07) == 0x07) bptr++;
          }
          b = c; g = c >> 8; r = c >> 16;
        }
        if (dimming != 255) { // only dim when needed
          r *= dimming; g *= dimming; b *= dimming;
          r  = r  >> 8; g  = g  >> 8; b  = b  >> 8;
        }
        if (digitColor) { // grayscale pixel coloring
          uint8_t l = (r > g) ? ((r > b) ? r:b) : ((g > b) ? g:b);
          r = g = b = l;

          r *= digitR; g *= digitG; b *= digitB;
          r  = r >> 8; g  = g >> 8; b  = b >> 8;
        }
        output_buffer[row][col] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3);
      }
    }
    
    drawBuffer();

    bmpFS.close();
    return true;
  }

  bool drawClk(const char *filename) {
    fs::File bmpFS;

    // Open requested file on SD card
    bmpFS = WLED_FS.open(filename, "r");

    if (!bmpFS)
    {
      Serial.print("File not found: ");
      Serial.println(filename);
      return false;
    }

    uint16_t r, g, b, dimming = 255, magic;
    int16_t row, col;
    
    magic = read16(bmpFS);
    if (magic != 0x4B43) { // look for "CK" header
      Serial.print(F("File not a CLK. Magic: "));
      Serial.println(magic);
      bmpFS.close();
      return false;
    }

    w = read16(bmpFS);
    h = read16(bmpFS);
    x = (width() - w) / 2;
    y = (height() - h) / 2;
    
    uint8_t lineBuffer[w * 2];
    
    if (!realtimeMode || realtimeOverride || (realtimeMode && useMainSegmentOnly)) strip.service();

    // 0,0 coordinates are top left
    for (row = 0; row < h; row++) {

      bmpFS.read(lineBuffer, sizeof(lineBuffer));
      uint8_t PixM, PixL;
      
      // Colors are already in 16-bit R5, G6, B5 format
      for (col = 0; col < w; col++)
      {
        if (dimming == 255 && !digitColor) { // not needed, copy directly
          output_buffer[row][col+x] = (lineBuffer[col*2+1] << 8) | (lineBuffer[col*2]);
        } else {
          // 16 BPP pixel format: R5, G6, B5 ; bin: RRRR RGGG GGGB BBBB
          PixM = lineBuffer[col*2+1];
          PixL = lineBuffer[col*2];
          // align to 8-bit value (MSB left aligned)
          r = (PixM) & 0xF8;
          g = ((PixM << 5) | (PixL >> 3)) & 0xFC;
          b = (PixL << 3) & 0xF8;
          r *= dimming; g *= dimming; b *= dimming;
          r  = r  >> 8; g  = g  >> 8; b  = b  >> 8;
          if (digitColor) { // grayscale pixel coloring
            uint8_t l = (r > g) ? ((r > b) ? r:b) : ((g > b) ? g:b);
            r = g = b = l;
            r *= digitR; g *= digitG; b *= digitB;
            r  = r >> 8; g  = g >> 8; b  = b >> 8;
          }
          output_buffer[row][col+x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
      }
    }

    drawBuffer();

    bmpFS.close();
    return true;
  }


public:
  TFTs() : TFT_eSPI(), chip_select()
    { for (uint8_t digit=0; digit < NUM_DIGITS; digit++) digits[digit] = 0; }

  // no == Do not send to TFT. yes == Send to TFT if changed. force == Send to TFT.
  enum show_t { no, yes, force };
  // A digit of 0xFF means blank the screen.
  const static uint8_t blanked = 255;

  uint8_t tubeSegment = 1;
  uint8_t digitOffset = 0;
  
  void begin() {
    pinMode(TFT_ENABLE_PIN, OUTPUT);
    digitalWrite(TFT_ENABLE_PIN, HIGH); //enable displays on boot

    // Start with all displays selected.
    chip_select.begin();
    chip_select.setAll();

    // Initialize the super class.
    init();
  }

  void showDigit(uint8_t digit) {
    chip_select.setDigit(digit);
    uint8_t digitToDraw = digits[digit];
    if (digitToDraw < 10) digitToDraw += digitOffset;

    if (digitToDraw == blanked) {
      fillScreen(TFT_BLACK); return;
    }

    // if last digit was the same, skip loading from FS to buffer
    if (!digitColor && digitToDraw == bufferedDigit) drawBuffer();
    digitR = R(digitColor); digitG = G(digitColor); digitB = B(digitColor);

    // Filenames are no bigger than "254.bmp\0"
    char file_name[10];
    // Fastest, raw RGB565
    sprintf(file_name, "/%d.bin", digitToDraw);
    if (WLED_FS.exists(file_name)) {
      if (drawBin(file_name)) bufferedDigit = digitToDraw;
      return;
    }
    // Fast, raw RGB565, see https://github.com/aly-fly/EleksTubeHAX on how to create this clk format
    sprintf(file_name, "/%d.clk", digitToDraw);
    if (WLED_FS.exists(file_name)) {
      if (drawClk(file_name)) bufferedDigit = digitToDraw;
      return;
    }
    // Slow, regular RGB888 or 1,4,8 bit palette BMP
    sprintf(file_name, "/%d.bmp", digitToDraw);
    if (drawBmp(file_name)) bufferedDigit = digitToDraw;
    return;
  } 

  void setDigit(uint8_t digit, uint8_t value, show_t show=yes) {
    uint8_t old_value = digits[digit];
    digits[digit] = value;

    // Color in grayscale bitmaps if Segment 1 exists
    // TODO If secondary and tertiary are black, color all in primary,
    // else color first three from Seg 1 color slots and last three from Seg 2 color slots
    Segment& seg1 = strip.getSegment(tubeSegment);
    if (seg1.isActive()) {
      digitColor = strip.getPixelColor(seg1.start + digit);
      dimming = seg1.opacity;
    } else {
      digitColor = 0;
      dimming = 255;
    }

    if (show != no && (old_value != value || show == force)) {
      showDigit(digit);
    }
  }
  uint8_t getDigit(uint8_t digit) {return digits[digit];}

  void showAllDigits()            {for (uint8_t digit=0; digit < NUM_DIGITS; digit++) showDigit(digit);}

  // Making chip_select public so we don't have to proxy all methods, and the caller can just use it directly.
  ChipSelect chip_select;
};

#endif // TFTS_H
