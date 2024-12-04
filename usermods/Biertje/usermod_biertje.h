#pragma once

#include "wled.h"

#define XY(x,y) SEGMENT.XY(x,y)

uint16_t mode_Biertje(bool useEmoticon) {

  // Constants
  const int beer_mug[16][16] = {
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



  // Constants for wave calculations
  const float INITIAL_WAVE_AMPLITUDE = 5.0;
  const float INITIAL_WAVE_FREQUENCY = 0.5;
  const float MIN_WAVE_AMPLITUDE = 0.2;
  const float MAX_WAVE_FREQUENCY = 2.0;
  const float WAVE_DURATION_MS = 5000.0;
  const float WAVE_DECAY_RATE = 1.0;

  // Constants for pour speed calculations
  const uint16_t MIN_POUR_INTERVAL_MS = 20;
  const uint16_t MAX_POUR_INTERVAL_MS = 200;
  const uint8_t POUR_SPEED_MULTIPLIER = 10;

  // Constants for beer and foam levels
  const float BEER_LEVEL_INCREMENT = 0.5;
  const float FOAM_HEIGHT_INCREMENT = 0.25;
  const float MAX_FOAM_HEIGHT = 4.0;
  const float BUBBLES_START_FOAM_HEIGHT = 2.0;

  // Constants for color gradients
  const uint8_t GRADIENT_START = 180;
  const uint8_t GRADIENT_END = 255;

  // Constants for bubble animation
  const uint8_t BUBBLE_UPDATE_INTERVAL_MS = 50;
  const uint32_t BUBBLE_SPAWN_INTERVAL_MS = 500;
  const uint8_t BUBBLES_PER_SPAWN = 2;
  const uint32_t BUBBLES_ACTIVE_DURATION_MS = 27000;

  // Constants for text animation
  const uint32_t TEXT_SCROLL_INTERVAL = 100;     // Scrolling speed in milliseconds
  const int LETTER_WIDTH = 6;   // Width of each character
  const int LETTER_HEIGHT = 8;  // Height of each character

  // Colors
  const uint32_t BEER_COLOR = RGBW32(255, 204, 0, 0);
  const uint32_t FOAM_COLOR = RGBW32(255, 255, 255, 0);
  const uint32_t BUBBLE_COLOR = RGBW32(254, 223, 99, 0);
  const uint32_t TEXT_COLOR = RGBW32(255, 0, 0, 0);
  const uint32_t HANDLE_COLOR = RGBW32(200, 200, 200, 0);

  // Variables
  int cols = SEGMENT.virtualWidth();
  int rows = SEGMENT.virtualHeight();

  uint8_t pourSpeed = SEGMENT.speed / 16;

  // Static variables
  static float beerLevel = 0;
  static float foamHeight = 0;
  static uint32_t lastPourUpdate = 0;
  static uint32_t lastBubbleUpdate = 0;
  static float waveAmplitude = 0;
  static float waveFrequency = 0;
  static uint32_t waveStartTime = 0;
  static bool bubblesActive = false;
  static uint32_t bubblesStartTime = 0;
  static bool textDisplayed = false;
  static const char* text = "BIER";
  static uint8_t textLength = 4;

  int textWidth = textLength * (LETTER_WIDTH + 1); // Total text width (+1 for spacing)
  int yOffset = ((rows - LETTER_HEIGHT) / 2) + 1;       // Center the text vertically

  // Static variables to track text position and timing
  static int textPosition = cols + textWidth;  // Start fully off-screen to the right
  static uint32_t lastTextUpdate = 0;
  
  
  // Use SEGENV.data as bit array for bubble positions
  unsigned dataSize = (SEGMENT.length() + 7) >> 3; // 1 bit per LED
  if (!SEGENV.allocateData(dataSize)) return 350; // Allocation failed

  // Update current time using millis()
  uint32_t now = millis();

  // Initialize wave parameters at the start
  if (waveStartTime == 0) {
    waveStartTime = now;
    waveAmplitude = INITIAL_WAVE_AMPLITUDE;
    waveFrequency = INITIAL_WAVE_FREQUENCY;
  }

  // Calculate time elapsed since wave started
  float timeElapsed = now - waveStartTime;

  // Adjust wave amplitude and frequency over time
  if (timeElapsed < WAVE_DURATION_MS) {
    waveAmplitude = INITIAL_WAVE_AMPLITUDE - INITIAL_WAVE_AMPLITUDE * (timeElapsed / WAVE_DURATION_MS);
    if (waveAmplitude < MIN_WAVE_AMPLITUDE) waveAmplitude = 0.0;

    waveFrequency = INITIAL_WAVE_FREQUENCY + (MAX_WAVE_FREQUENCY - INITIAL_WAVE_FREQUENCY) * (timeElapsed / WAVE_DURATION_MS);
    if (waveFrequency > MAX_WAVE_FREQUENCY) waveFrequency = MAX_WAVE_FREQUENCY;
  } else {
    if (waveAmplitude != 0.0) {
      waveAmplitude = 0.0;
      waveFrequency = 0.0;
    }
  }

  // Update beer level based on pour speed
  uint16_t pourInterval = MAX(MIN_POUR_INTERVAL_MS, MAX_POUR_INTERVAL_MS - pourSpeed * POUR_SPEED_MULTIPLIER);
  if (now - lastPourUpdate > pourInterval) {
    lastPourUpdate = now;

    if (beerLevel < rows) {
      beerLevel += BEER_LEVEL_INCREMENT;
      foamHeight += FOAM_HEIGHT_INCREMENT;
      if (beerLevel > rows) beerLevel = rows;
    } else if (!textDisplayed) {
        textDisplayed = true;
    }
    if (foamHeight > MAX_FOAM_HEIGHT) foamHeight = MAX_FOAM_HEIGHT;

    // Start bubbles when foamHeight reaches 4
    if (foamHeight >= BUBBLES_START_FOAM_HEIGHT && !bubblesActive) {
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
    float waveSpeed = 4.0;        // Adjust wave speed

    float foamWaveAmplitude = 0.0; // No waves in foam after pour is done

    for (int x = 0; x < cols; x++) {
      // Calculate wave amplitude at this column
      float waveAmplitudeAtX = waveAmplitude * expf(-((cols - x) / (float)cols) * WAVE_DECAY_RATE);

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
        uint8_t gradientFactor = map(y, rows - columnBeerLevel + columnFoamHeight, rows - 1, GRADIENT_START, GRADIENT_END);
        uint32_t beerColorGradient = RGBW32(gradientFactor, (uint8_t)(gradientFactor * 0.8), 0, 0);

        // Add bubble if present
        if (bubblesActive) {
          unsigned index = XY(x, y) >> 3;
          unsigned bitNum = XY(x, y) & 0x07;
          if (bitRead(SEGENV.data[index], bitNum)) {
            SEGMENT.setPixelColorXY(x, y, BUBBLE_COLOR);
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
          SEGMENT.setPixelColorXY(x, y, FOAM_COLOR);
        }
      }
    }
  } // 2D FX


  // Bubble animation
  if (bubblesActive) {
    // Slow down bubbles to half speed
    if (now - lastBubbleUpdate > BUBBLE_UPDATE_INTERVAL_MS) {
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
      if (now - lastBubbleSpawn > BUBBLE_SPAWN_INTERVAL_MS) { // Spawn bubbles every 500ms (adjust as needed)
        lastBubbleSpawn = now;

        // Spawn a couple of bubbles per second
        for (int i = 0; i < BUBBLES_PER_SPAWN; i++) { // Adjust number of bubbles spawned
          int x = random16(int(cols));
          unsigned index = XY(x, int(rows) - 1) >> 3;
          unsigned bitNum = XY(x, int(rows) - 1) & 0x07;
          bitSet(SEGENV.data[index], bitNum);
        }
      }
    }


    // Stop bubbles after x seconds
    if ((now - bubblesStartTime > BUBBLES_ACTIVE_DURATION_MS) && (textPosition == cols + textWidth)) {
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

          uint8_t r2 = (HANDLE_COLOR >> 16) & 0xFF;
          uint8_t g2 = (HANDLE_COLOR >> 8) & 0xFF;
          uint8_t b2 = HANDLE_COLOR & 0xFF;

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
          SEGMENT.setPixelColorXY(x, y, HANDLE_COLOR);
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
    if (now - lastTextUpdate >= TEXT_SCROLL_INTERVAL) {
      lastTextUpdate = now;
      textPosition--;

      // Reset position when text has completely scrolled off the screen
      if (textPosition < -textWidth) {
        textPosition = cols + textWidth;
      }
    }

    // Draw each character at the updated position
    for (int i = 0; i < textLength; i++) {
      int charX = textPosition + i * (LETTER_WIDTH + 1); // Position with spacing
      SEGMENT.drawCharacter(text[i], charX, yOffset, LETTER_WIDTH, LETTER_HEIGHT, TEXT_COLOR, 0, 0);
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
