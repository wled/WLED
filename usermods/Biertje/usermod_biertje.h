#pragma once

#include "wled.h"

#define XY(x,y) SEGMENT.XY(x,y)

uint16_t mode_Biertje(bool useEmoticon) {

  int beer_mug[16][16] = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 3, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 3, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 3, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 3, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  };


  const int cols = SEGMENT.virtualWidth();   // Width of the matrix
  const int rows = SEGMENT.virtualHeight();  // Height of the matrix

  // Parameters to control speed
  uint8_t pourSpeed = SEGMENT.speed / 16;       // Adjust pour speed (lower value = faster pour)

  // Static variables to retain values between function calls
  static float beerLevel = 0;        // Current beer level (height)
  static float foamHeight = 0;       // Foam height, grows from 0 to 4

  // Timing variables
  static uint32_t lastPourUpdate = 0;    // Last pour update timestamp
  static uint32_t lastBubbleUpdate = 0;  // Last bubble update timestamp

  // Variables for wave control
  static float waveAmplitude = 0;    // Wave amplitude
  static float waveFrequency = 0;    // Wave frequency
  static uint32_t waveStartTime = 0; // Time when the wave effect started

  // Variables for bubble animation
  static bool bubblesActive = false;    // Flag to indicate if bubbles are active
  static uint32_t bubblesStartTime = 0; // Time when bubbles started

  // Variables for text display
  static bool textDisplayed = false;    // Flag to indicate if text is displayed    
  static const char* text = "BIER";     // Text to display
  static uint8_t textLength = 4;        // Length of the text

  // Define text parameters
  int letterWidth = 6;   // Width of each character
  int letterHeight = 8;  // Height of each character
  int textWidth = textLength * (letterWidth + 1); // Total text width (+1 for spacing)
  int yOffset = ((rows - letterHeight) / 2) + 1;       // Center the text vertically

  // Static variables to track text position and timing
  static int textPosition = cols + textWidth;  // Start fully off-screen to the right
  static uint32_t lastTextUpdate = 0;          // Time of last position update
  const uint32_t textScrollInterval = 100;     // Scrolling speed in milliseconds
  
  // Use SEGENV.data as bit array for bubble positions
  unsigned dataSize = (SEGMENT.length() + 7) >> 3; // 1 bit per LED
  if (!SEGENV.allocateData(dataSize)) return 350; // Allocation failed

  

  // Colors (adjust as desired)
  uint32_t beerColor = RGBW32(255, 204, 0, 0);     // Amber color for beer
  uint32_t foamColor = RGBW32(255, 255, 255, 0);   // White color for foam
  uint32_t bubbleColor = RGBW32(254, 223, 99, 0); // White color for bubbles
  uint32_t textColor = RGBW32(255, 0, 0, 0);   // Color for text

  // Update current time using millis()
  uint32_t now = millis();

  // Initialize wave parameters at the start
  if (waveStartTime == 0) {
    waveStartTime = now;
    waveAmplitude = 5.0;  // Start with a big wave
    waveFrequency = 0.5;  // Start with a low frequency (long wave)
  }

  // Calculate time elapsed since wave started
  float timeElapsed = now - waveStartTime;

  // Adjust wave amplitude and frequency over time
  float waveDuration = 5000.0; // Duration over which the wave changes (in milliseconds)
  if (timeElapsed < waveDuration) {
    // Decrease wave amplitude from initial value to 0 over time
    waveAmplitude = 5.0 - 5.0 * (timeElapsed / waveDuration);
    if (waveAmplitude < 0.2) waveAmplitude = 0.0;

    // Increase wave frequency from initial value to higher value over time
    waveFrequency = 0.5 + 1.5 * (timeElapsed / waveDuration);
    if (waveFrequency > 2.0) waveFrequency = 2.0;
  } else {
    // After waveDuration, set waveAmplitude to 0 (no more waves)
    if (waveAmplitude != 0.0) {
      waveAmplitude = 0.0;
      waveFrequency = 0.0;
      //foamHeight = 2.0; // Ensure foam height is at maximum
    }
  }

  // Update beer level based on pour speed
  uint16_t pourInterval = MAX(20, 200 - pourSpeed * 10);  // Adjust pour speed
  if (now - lastPourUpdate > pourInterval) {
    lastPourUpdate = now;

    if (beerLevel < rows) {
      beerLevel += 0.5;  // Increase beer level
      foamHeight += 0.25;
      if (beerLevel > rows) beerLevel = rows;
    } else if (!textDisplayed) {
        textDisplayed = true;
    }
    if (foamHeight > 4) foamHeight = 4;

    // Start bubbles when foamHeight reaches 4
    if (foamHeight >= 2 && !bubblesActive) {
      bubblesActive = true;
      bubblesStartTime = now;

      // Initialize bubble data
      memset(SEGENV.data, 0, dataSize);
    }
  }

// // Debug statements
// DEBUG_PRINTF("Wave Amplitude: %.2f\n", waveAmplitude);
// DEBUG_PRINTF("Wave Frequency: %.2f\n", waveFrequency);
// DEBUG_PRINTF("Foam Height: %.2f\n", foamHeight);
// DEBUG_PRINTF("Beer Level: %.2f\n", beerLevel);
// DEBUG_PRINTF("Rows: %.2f\n", rows);
// DEBUG_PRINTF("Columns: %.2f\n", cols);

  // Clear the matrix
  SEGMENT.fill(BLACK);


  if (SEGMENT.is2D())  
  {
   
    // Draw the beer and foam
    float timeSec = now / 1000.0; // Time in seconds
    float waveSpeed = 3.0;        // Adjust wave speed

    float foamWaveAmplitude = 0.0; // No waves in foam after pour is done

    float waveDecayRate = 3.0; // Controls how quickly the wave amplitude decreases from right to left

    for (int x = 0; x < cols; x++) {
      // Calculate wave amplitude at this column
      float waveAmplitudeAtX = waveAmplitude * expf(-((cols - x) / (float)cols) * waveDecayRate);

      // Calculate the wave for this column
      float wave = waveAmplitudeAtX * sinf(((cols - x) * waveFrequency) - waveSpeed * timeSec);

      int columnBeerLevel = (int)(beerLevel + wave);

      // Limit the beer level within the matrix bounds
      if (columnBeerLevel < 0) columnBeerLevel = 0;
      if (columnBeerLevel > rows) columnBeerLevel = rows;

      // Foam is static after pour is done
      int columnFoamHeight = (int)(foamHeight);

      // Draw the beer from the bottom up with gradient
      for (int y = rows - 1; y >= rows - columnBeerLevel + columnFoamHeight; y--) {
        // Calculate gradient color based on y-position
        uint8_t gradientFactor = map(y, rows - columnBeerLevel + columnFoamHeight, rows - 1, 180, 255);
        uint32_t beerColorGradient = RGBW32(gradientFactor, (uint8_t)(gradientFactor * 0.8), 0, 0);

        // Add bubble if present
        if (bubblesActive) {
          unsigned index = XY(x, y) >> 3;
          unsigned bitNum = XY(x, y) & 0x07;
          if (bitRead(SEGENV.data[index], bitNum)) {
            SEGMENT.setPixelColorXY(x, y, bubbleColor);
          } else {
            SEGMENT.setPixelColorXY(x, y, beerColorGradient);
          }
        } else {
          SEGMENT.setPixelColorXY(x, y, beerColorGradient);
        }
      }

      // Draw the foam at the top of the beer
      for (int y = rows - columnBeerLevel; y < rows - columnBeerLevel + columnFoamHeight; y++) {
        if (y >= 0 && y < rows) {
          SEGMENT.setPixelColorXY(x, y, foamColor);
        }
      }
    }
  } // 2D FX


  // Bubble animation
  if (bubblesActive) {
    uint8_t bubbleUpdateInterval = 50; // Adjust for bubble movement speed (higher value = slower bubbles)

    // Slow down bubbles to half speed
    if (now - lastBubbleUpdate > bubbleUpdateInterval) {
      lastBubbleUpdate = now;

      // Move bubbles up
      for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
          unsigned index = XY(x, y) >> 3;
          unsigned bitNum = XY(x, y) & 0x07;
          if (bitRead(SEGENV.data[index], bitNum)) {
            // Clear current position
            bitClear(SEGENV.data[index], bitNum);
            // Move up
            if (y > 0) {
              unsigned newIndex = XY(x, y - 1) >> 3;
              unsigned newBitNum = XY(x, y - 1) & 0x07;
              bitSet(SEGENV.data[newIndex], newBitNum);
            }
          }
        }
      }

      // Spawn new bubbles at the bottom with reduced frequency
      static uint32_t lastBubbleSpawn = 0;
      if (now - lastBubbleSpawn > 500) { // Spawn bubbles every 500ms (adjust as needed)
        lastBubbleSpawn = now;

        // Spawn a couple of bubbles per second
        for (int i = 0; i < 2; i++) { // Adjust number of bubbles spawned
          int x = random16(int(cols));
          unsigned index = XY(x, int(rows) - 1) >> 3;
          unsigned bitNum = XY(x, int(rows) - 1) & 0x07;
          bitSet(SEGENV.data[index], bitNum);
        }
      }
    }


    // Stop bubbles after x seconds
    if ((now - bubblesStartTime > 27000) && (textPosition == cols + textWidth)) {
      bubblesActive = false;
      beerLevel = 0;
      foamHeight = 0;
      waveStartTime = 0;
      waveAmplitude = 0;    // Wave amplitude
      waveFrequency = 0;    // Wave frequency  
      textDisplayed = false;
      textPosition = cols + textWidth;
      memset(SEGENV.data, 0, dataSize); // Clear bubble data
    }
  }

  if (useEmoticon)
  {
    uint32_t handleColor = RGBW32(200, 200, 200, 0); // Greyish white color for the handle

    // Overlay the beer_mug over the current display
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        int mugValue = beer_mug[y][x];

          // Get the existing color at this pixel
          uint32_t currentColor = SEGMENT.getPixelColorXY(x, y);

          // Blend the handle color with the existing color to create a glass effect
          uint8_t r1 = (currentColor >> 16) & 0xFF;
          uint8_t g1 = (currentColor >> 8) & 0xFF;
          uint8_t b1 = currentColor & 0xFF;

          uint8_t r2 = (handleColor >> 16) & 0xFF;
          uint8_t g2 = (handleColor >> 8) & 0xFF;
          uint8_t b2 = handleColor & 0xFF;

          // Blend the colors (adjust the ratio for transparency)
          uint8_t r = (r1 * 3 + r2 * 1) / 4; // 75% underlying color, 25% handle color
          uint8_t g = (g1 * 3 + g2 * 1) / 4;
          uint8_t b = (b1 * 3 + b2 * 1) / 4;

          uint32_t blendedColor = (r << 16) | (g << 8) | b;

        if (mugValue == 0) {
          // Set pixel to black
          SEGMENT.setPixelColorXY(x, y, BLACK);
        } else if (mugValue == 2) {
        

          SEGMENT.setPixelColorXY(x, y, blendedColor);
        } else if (mugValue == 3) {
          SEGMENT.setPixelColorXY(x, y, handleColor);
        } else {
          if (currentColor == BLACK) {
            // No colored pixel underneath; set to handle color
            SEGMENT.setPixelColorXY(x, y, blendedColor);
          }     
        }    
      }
    }

  }

   // Draw the text "BIER" if textDisplayed is true
  if (textDisplayed) {

    // Update text position based on the interval
    if (now - lastTextUpdate >= textScrollInterval) {
      lastTextUpdate = now;
      textPosition--;

      // Reset position when text has completely scrolled off the screen
      if (textPosition < -textWidth) {
        textPosition = cols + textWidth;
      }
    }

    // Draw each character at the updated position
    for (int i = 0; i < textLength; i++) {
      int charX = textPosition + i * (letterWidth + 1); // Position with spacing
      SEGMENT.drawCharacter(text[i], charX, yOffset, letterWidth, letterHeight, textColor, 0, 0);
    }
  }



  if (!strip.isMatrix || !SEGMENT.is2D()) {
    // 1D FX
    uint16_t numLeds = SEGMENT.length();  // Number of LEDs in the strip

    // Configuration option
    bool fillFromCenter = true; // Set to true for center-out filling, false for end-to-end filling

    // Parameters to control speed
    uint8_t pourSpeed = SEGMENT.speed / 4;  // Adjust pour speed (lower value = faster pour)

    // Static variables for 1D animation
    static uint16_t beerLevel1D = 0;        // Current beer level (number of LEDs lit)
    static uint32_t lastPourUpdate1D = 0;   // Last pour update timestamp
    static uint32_t fullBeerTime = 0;       // Time when beerLevel1D reached max level
    static float foamHeightStrip = 0;            // Foam height, grows from 0 to maxFoamHeight

    // Colors
    uint32_t beerColor = RGBW32(255, 204, 0, 0);    // Amber color for beer
    uint32_t foamColor = RGBW32(255, 255, 255, 0);  // White color for foam

    // Update beer level and foam height based on pour speed
    uint16_t pourInterval = MAX(20, 200 - pourSpeed * 10);  // Adjust pour speed
    if (now - lastPourUpdate1D > pourInterval) {
      lastPourUpdate1D = now;

      // Determine maximum beer level based on filling mode
      uint16_t maxBeerLevel = fillFromCenter ? ((numLeds + 1) / 2) : numLeds;

      if (beerLevel1D < maxBeerLevel) {
        beerLevel1D++;  // Increase beer level
        if (beerLevel1D > maxBeerLevel) beerLevel1D = maxBeerLevel;
      } else if (fullBeerTime == 0) {
        fullBeerTime = now;  // Start the pause timer
      }

      // Increase foam height proportionally up to maximum
      float maxFoamHeight = 8.0;  // Maximum foam height in LEDs
      if (foamHeightStrip < maxFoamHeight) {
        float progress = (float)beerLevel1D / maxBeerLevel;
        foamHeightStrip = progress * maxFoamHeight;
        if (foamHeightStrip > maxFoamHeight) foamHeightStrip = maxFoamHeight;
      }
    }

    // Check if we should reset the beer level after a pause
    if (fullBeerTime > 0 && now - fullBeerTime > 500) {  // 3-second pause
      beerLevel1D = 0;
      foamHeightStrip = 0;  // Reset foamHeight
      fullBeerTime = 0;
    }

    // Calculate the number of foam LEDs
    int foamHeight1D = (int)(foamHeightStrip + 0.5);  // Round to nearest integer

    // Draw the beer and foam by setting pixel colors
    for (uint16_t i = 0; i < numLeds; i++) {
      uint16_t index = numLeds - 1 - i;  // Reverse the index if needed

      if (fillFromCenter) {
        // Filling from the center outward
        uint16_t center = numLeds / 2;
        int16_t distanceFromCenter = abs((int16_t)i - (int16_t)center);

        if (distanceFromCenter < beerLevel1D) {
          // Apply foam color to the top LEDs based on foamHeight1D
          if (distanceFromCenter >= beerLevel1D - foamHeight1D) {
            SEGMENT.setPixelColor(i, foamColor);
          } else {
            SEGMENT.setPixelColor(i, beerColor);
          }
        } else {
          SEGMENT.setPixelColor(i, BLACK);
        }
      } else {
        // Filling from one end to the other
        if (i < beerLevel1D) {
          // Apply foam color to the top LEDs based on foamHeight1D
          if (i >= beerLevel1D - foamHeight1D) {
            SEGMENT.setPixelColor(index, foamColor);
          } else {
            SEGMENT.setPixelColor(index, beerColor);
          }
        } else {
          SEGMENT.setPixelColor(index, BLACK);
        }
      }
    }
  }
  

  return FRAMETIME;
}


/*
 * Runs a single pixel back and forth.
 */
uint16_t mode_biertje_full(void) {
  return mode_Biertje(false);
}
static const char _data_FX_MODE_BIERTJE[] PROGMEM = "Biertje@Speed;;1;2";


/*
 * Runs two pixel back and forth in opposite directions.
 */
uint16_t mode_biertje_emoticon(void) {
  return mode_Biertje(true);
}
static const char _data_FX_MODE_BIERTJE_EMOTICON[] PROGMEM = "Biertje Emoticon@Speed;;1;2";


class UsermodBiertje : public Usermod {
  private:

  public:
    void setup() {
      strip.addEffect(255, &mode_biertje_full, _data_FX_MODE_BIERTJE);
      strip.addEffect(255, &mode_biertje_emoticon, _data_FX_MODE_BIERTJE_EMOTICON);
    }

    void loop() {

    }

  uint16_t getId()
  {
    return USERMOD_ID_BIERTJE;
  }

};
