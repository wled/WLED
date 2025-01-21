#pragma once

#include "wled.h"

#define XY(x,y) SEGMENT.XY(x,y)

// Define the BiertjeData struct to hold persistent data
typedef struct {
  // Static variables previously used in mode_Biertje
  float beerLevel;
  float foamHeight;
  uint32_t lastPourUpdate;
  uint32_t lastBubbleUpdate;
  float waveAmplitude;
  float waveFrequency;
  uint32_t waveStartTime;
  bool bubblesActive;
  uint32_t bubblesStartTime;
  bool textDisplayed;
  const char* text;  
  int textPosition;
  uint32_t lastTextUpdate;
  uint8_t pourSpeed;
  unsigned dataSize;
  // Additional variables if needed
  uint32_t lastTime;
} BiertjeData;

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

 // Constants
const int heart[16][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 2, 2, 2, 2, 0, 0, 2, 2, 2, 2, 0, 0, 0},
    {0, 0, 2, 2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 0, 0},
    {0, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 0},
    {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
    {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
    {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
    {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
    {0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0},
    {0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0},
    {0, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0},
    {0, 0, 0, 0, 2, 2, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
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
  const uint32_t TEXT_SCROLL_INTERVAL_MS = 100;     // Scrolling speed in milliseconds
  const int LETTER_WIDTH = 6;   // Width of each character
  const int LETTER_HEIGHT = 8;  // Height of each character
  const int TEXT_LENGTH = 7;
  const int TEXT_WIDTH = TEXT_LENGTH * (LETTER_WIDTH + 1); // Total text width (+1 for spacing)
  const char* TEXT_CHARS = "BIERTJE";

  // Colors
  const uint32_t BEER_COLOR = RGBW32(255, 204, 0, 0);
  const uint32_t FOAM_COLOR = RGBW32(255, 255, 255, 0);
  const uint32_t BUBBLE_COLOR = RGBW32(254, 223, 99, 0);
  const uint32_t TEXT_COLOR = RGBW32(255, 0, 0, 0);
  const uint32_t HANDLE_COLOR = RGBW32(200, 200, 200, 0);

  // Variables
  int cols = SEGMENT.virtualWidth();
  int rows = SEGMENT.virtualHeight();

  // Allocate data for biertje effect
  if (!SEGENV.allocateData(sizeof(BiertjeData))) return 350; // Allocation failed
  BiertjeData* data = reinterpret_cast<BiertjeData*>(SEGENV.data);

  // Initialize data if it's the first run
  if (SEGENV.call == 0) {

    DEBUG_PRINTF("SEGENV.call: %u\n", SEGENV.call);    
    data->beerLevel = 0;
    data->foamHeight = 0;
    data->lastPourUpdate = 0;
    data->lastBubbleUpdate = 0;
    data->waveAmplitude = 0;
    data->waveFrequency = 0;
    data->waveStartTime = 0;
    data->bubblesActive = false;
    data->bubblesStartTime = 0;
    data->textDisplayed = false;
    data->textPosition = 0;
    data->lastTextUpdate = 0;
    data->pourSpeed = SEGMENT.speed / 16;

    // Use SEGENV.data as bit array for bubble positions
    data->dataSize = (SEGMENT.length() + 7) >> 3; // 1 bit per LED
    if (!SEGENV.allocateData(data->dataSize + sizeof(BiertjeData))) return 350; // Allocation failed
    memset(SEGENV.data + sizeof(BiertjeData), 0, data->dataSize); // Clear bubble data
  }

  uint8_t* bubbleData = SEGENV.data + sizeof(BiertjeData);

  // Update current time using millis()
  uint32_t now = millis();

  // Initialize wave parameters at the start
  if (data->waveStartTime == 0) {
    data->waveStartTime = now;
    data->waveAmplitude = INITIAL_WAVE_AMPLITUDE;
    data->waveFrequency = INITIAL_WAVE_FREQUENCY;
  }

  // Calculate time elapsed since wave started
  float timeElapsed = now - data->waveStartTime;

  // Adjust wave amplitude and frequency over time
  if (timeElapsed < WAVE_DURATION_MS) {
    data->waveAmplitude = INITIAL_WAVE_AMPLITUDE - INITIAL_WAVE_AMPLITUDE * (timeElapsed / WAVE_DURATION_MS);
    if (data->waveAmplitude < MIN_WAVE_AMPLITUDE) data->waveAmplitude = 0.0;

    data->waveFrequency = INITIAL_WAVE_FREQUENCY + (MAX_WAVE_FREQUENCY - INITIAL_WAVE_FREQUENCY) * (timeElapsed / WAVE_DURATION_MS);
    if (data->waveFrequency > MAX_WAVE_FREQUENCY) data->waveFrequency = MAX_WAVE_FREQUENCY;
  } else {
    if (data->waveAmplitude != 0.0) {
      data->waveAmplitude = 0.0;
      data->waveFrequency = 0.0;
    }
  }

  // Update beer level based on pour speed
  uint16_t pourInterval = MAX(MIN_POUR_INTERVAL_MS, MAX_POUR_INTERVAL_MS - data->pourSpeed * POUR_SPEED_MULTIPLIER);
  if (now - data->lastPourUpdate > pourInterval) {
    data->lastPourUpdate = now;

    if (data->beerLevel < rows) {
      data->beerLevel += BEER_LEVEL_INCREMENT;
      data->foamHeight += FOAM_HEIGHT_INCREMENT;
      if (data->beerLevel > rows) data->beerLevel = rows;
    } else if (!data->textDisplayed) {
      data->textPosition = cols + TEXT_WIDTH;
      data->textDisplayed = true;
    }
    if (data->foamHeight > MAX_FOAM_HEIGHT) data->foamHeight = MAX_FOAM_HEIGHT;

    // Start bubbles when foamHeight reaches threshold
    if (data->foamHeight >= BUBBLES_START_FOAM_HEIGHT && !data->bubblesActive) {
      data->bubblesActive = true;
      data->bubblesStartTime = now;

      // Initialize bubble data
      memset(bubbleData, 0, data->dataSize);
    }
  }

  // Clear the matrix
  SEGMENT.fill(BLACK);

  if (SEGMENT.is2D()) {
    // 2D FX
    // Draw the beer and foam
    float timeSec = now / 1000.0; // Time in seconds
    float waveSpeed = 4.0;        // Adjust wave speed

    for (int x = 0; x < cols; x++) {
      // Calculate wave amplitude at this column
      float waveAmplitudeAtX = data->waveAmplitude * expf(-((cols - x) / (float)cols) * WAVE_DECAY_RATE);

      // Calculate the wave for this column
      float wave = waveAmplitudeAtX * sinf(((cols - x) * data->waveFrequency) - waveSpeed * timeSec);

      int columnBeerLevel = (int)(data->beerLevel + wave);

      // Limit the beer level within the matrix bounds
      if (columnBeerLevel < 0) columnBeerLevel = 0;
      if (columnBeerLevel > rows) columnBeerLevel = rows;

      // Foam is static after pour is done
      int columnFoamHeight = (int)(data->foamHeight);

      // Draw the beer from the bottom up with gradient
      for (int y = rows - 1; y >= rows - columnBeerLevel + columnFoamHeight; y--) {
        // Calculate gradient color based on y-position
        uint8_t gradientFactor = map(y, rows - columnBeerLevel + columnFoamHeight, rows - 1, GRADIENT_START, GRADIENT_END);
        uint32_t beerColorGradient = RGBW32(gradientFactor, (uint8_t)(gradientFactor * 0.8), 0, 0);

        // Add bubble if present
        if (data->bubblesActive) {
          unsigned index = XY(x, y) >> 3;
          unsigned bitNum = XY(x, y) & 0x07;
          if (bitRead(bubbleData[index], bitNum)) {
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

    // Bubble animation
    if (data->bubblesActive) {
      // Slow down bubbles to half speed
      if (now - data->lastBubbleUpdate > BUBBLE_UPDATE_INTERVAL_MS) {
        data->lastBubbleUpdate = now;

        // Move bubbles up
        for (int y = 0; y < rows; y++) {
          for (int x = 0; x < cols; x++) {
            unsigned index = XY(x, y) >> 3;
            unsigned bitNum = XY(x, y) & 0x07;
            if (bitRead(bubbleData[index], bitNum)) {
              // Clear current position
              bitClear(bubbleData[index], bitNum);
              // Move up
              if (y > 0) {
                unsigned newIndex = XY(x, y - 1) >> 3;
                unsigned newBitNum = XY(x, y - 1) & 0x07;
                bitSet(bubbleData[newIndex], newBitNum);
              }
            }
          }
        }

        // Spawn new bubbles at the bottom with reduced frequency
        static uint32_t lastBubbleSpawn = 0;
        if (now - lastBubbleSpawn > BUBBLE_SPAWN_INTERVAL_MS) { // Spawn bubbles every interval
          lastBubbleSpawn = now;

          // Spawn bubbles
          for (int i = 0; i < BUBBLES_PER_SPAWN; i++) {
            int x = random16(int(cols));
            unsigned index = XY(x, int(rows) - 1) >> 3;
            unsigned bitNum = XY(x, int(rows) - 1) & 0x07;
            bitSet(bubbleData[index], bitNum);
          }
        }
      }

      // Stop bubbles after duration
      if ((now - data->bubblesStartTime > BUBBLES_ACTIVE_DURATION_MS) && (data->textPosition == cols + TEXT_WIDTH)) {
        data->bubblesActive = false;
        data->beerLevel = 0;
        data->foamHeight = 0;
        data->waveStartTime = 0;
        data->waveAmplitude = 0.0;
        data->waveFrequency = 0.0;
        data->textDisplayed = false;
        memset(bubbleData, 0, data->dataSize); // Clear bubble data
      }
    }

    // Overlay beer mug if useEmoticon is true
    if (useEmoticon) {
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

    // Draw the text if textDisplayed is true
    if (data->textDisplayed) {

      int yOffset = ((rows - LETTER_HEIGHT) / 2) + 1;       // Center the text vertically
      // Update text position based on the interval
      if (now - data->lastTextUpdate >= TEXT_SCROLL_INTERVAL_MS) {
        data->lastTextUpdate = now;
        data->textPosition--;
        // Reset position when text has completely scrolled off the screen
        if (data->textPosition < -TEXT_WIDTH) {
          data->textPosition = cols + TEXT_WIDTH;
        }
      }

      
      // Draw each character at the updated position
      for (int i = 0; i < TEXT_LENGTH; i++) {
        int16_t charX = data->textPosition + i * (LETTER_WIDTH + 1); // Position with spacing

        SEGMENT.drawCharacter(TEXT_CHARS[i], charX, yOffset, LETTER_WIDTH, LETTER_HEIGHT, TEXT_COLOR, 0, 0);

      }
    }
  } else {
    // 1D FX
    uint16_t numLeds = SEGMENT.length();  // Number of LEDs in the strip

    // Configuration option
    bool fillFromCenter = true; // Set to true for center-out filling, false for end-to-end filling

    // Parameters to control speed
    data->pourSpeed = SEGMENT.speed / 4;  // Adjust pour speed (lower value = faster pour)

    // Static variables for 1D animation
    static uint16_t beerLevel1D = 0;        // Current beer level (number of LEDs lit)
    static uint32_t lastPourUpdate1D = 0;   // Last pour update timestamp
    static uint32_t fullBeerTime = 0;       // Time when beerLevel1D reached max level
    static float foamHeightStrip = 0;       // Foam height, grows from 0 to maxFoamHeight

    // Update beer level and foam height based on pour speed
    uint16_t pourInterval = MAX(20, 200 - data->pourSpeed * 10);  // Adjust pour speed
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
    if (fullBeerTime > 0 && now - fullBeerTime > 500) {  // 500ms pause
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
            SEGMENT.setPixelColor(i, FOAM_COLOR);
          } else {
            SEGMENT.setPixelColor(i, BEER_COLOR);
          }
        } else {
          SEGMENT.setPixelColor(i, BLACK);
        }
      } else {
        // Filling from one end to the other
        if (i < beerLevel1D) {
          // Apply foam color to the top LEDs based on foamHeight1D
          if (i >= beerLevel1D - foamHeight1D) {
            SEGMENT.setPixelColor(index, FOAM_COLOR);
          } else {
            SEGMENT.setPixelColor(index, BEER_COLOR);
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
 * Effect wrapper functions
 */
uint16_t mode_biertje_full(void) {
  return mode_Biertje(false);
}
static const char _data_FX_MODE_BIERTJE[] PROGMEM = "Biertje@Speed;;1;2";

uint16_t mode_biertje_emoticon(void) {
  return mode_Biertje(true);
}
static const char _data_FX_MODE_BIERTJE_EMOTICON[] PROGMEM = "Biertje Emoticon@Speed;;1;2";

/*
 * Usermod class
 */
class UsermodBiertje : public Usermod {
  public:
    void setup() {
      strip.addEffect(255, &mode_biertje_full, _data_FX_MODE_BIERTJE);
      strip.addEffect(FX_MODE_BIERTJE, &mode_biertje_emoticon, _data_FX_MODE_BIERTJE_EMOTICON);      
    }

    void loop() {
      // Usermod loop code if needed
    }

    uint16_t getId() {
      return USERMOD_ID_BIERTJE; // Replace with your Usermod ID
    }
};
