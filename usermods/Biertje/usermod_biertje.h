#pragma once

#include "wled.h"

static const char _data_FX_MODE_BIERTJE[] PROGMEM = "Biertje@Speed;;1;2";
#define XY(x,y) SEGMENT.XY(x,y)

uint16_t mode_Biertje() {
  if (!strip.isMatrix || !SEGMENT.is2D()) return 350; // Ensure it's a 2D setup

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
      foamHeight = 4.0; // Ensure foam height is at maximum
    }
  }

  // Update beer level based on pour speed
  uint16_t pourInterval = MAX(20, 200 - pourSpeed * 10);  // Adjust pour speed
  if (now - lastPourUpdate > pourInterval) {
    lastPourUpdate = now;

    if (beerLevel < rows) {
      beerLevel += 0.5;  // Increase beer level
      if (beerLevel > rows) beerLevel = rows;

      // Increase foam height proportionally up to 4 pixels
      if (foamHeight < 4) {
        foamHeight = (beerLevel / rows) * 4.0;
        if (foamHeight > 4) foamHeight = 4;
      }
    } else if (!bubblesActive) {
      // Start bubbles after beer has settled
      bubblesActive = true;
      textDisplayed = true;
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

  // Draw the text "BIER" if textDisplayed is true
  if (textDisplayed) {
    // Define text parameters
    int letterWidth = 5;   // Width of each character
    int letterHeight = 8;  // Height of each character
    int xOffset = (cols - (letterWidth) * textLength) / 2; // Center the text horizontally
    int yOffset = (rows - letterHeight) / 2;                   // Center the text vertically

    // Draw each character
    for (int i = 0; i < textLength; i++) {
      int charX = xOffset + i * (letterWidth);
      SEGMENT.drawCharacter(text[i], charX, yOffset, letterWidth, letterHeight, textColor, 0, 0);
    }
  }


    // Stop bubbles after 30 seconds
    if (now - bubblesStartTime > 30000) {
      bubblesActive = false;
      beerLevel = 0;
      foamHeight = 0;
      waveStartTime = 0;
      waveAmplitude = 0;    // Wave amplitude
      waveFrequency = 0;    // Wave frequency  
      textDisplayed = true;
      memset(SEGENV.data, 0, dataSize); // Clear bubble data
    }
  }

  return FRAMETIME;
}

class UsermodBiertje : public Usermod {
  private:

  public:
    void setup() {
      strip.addEffect(255, &mode_Biertje, _data_FX_MODE_BIERTJE);
    }

    void loop() {

    }

  uint16_t getId()
  {
    return USERMOD_ID_BIERTJE;
  }

};
