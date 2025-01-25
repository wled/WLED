#pragma once

#include "wled.h"

// Fish sprite (6x9)
const int aquariumfish[6][9] = {
    {0, 0, 1, 1, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {1, 1, 2, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {0, 0, 1, 1, 1, 0, 0, 0, 1},    
};

// Vegetation sprite (3x3)
const int vegetation[3][3] = {
    {0, 1, 0},
    {1, 1, 1},
    {0, 1, 0}
};

// Bubble sprite (1x1)
const int bubble[1][1] = {
    {1}
};

void drawFish(int x, int y, int direction) {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 9; j++) {
      // Pick a column index based on whether we're flipping horizontally
      int col = (direction == 1) ? j       // normal
                                 : (8 - j); // flipped horizontally
      // Always draw left to right in screen space
      int drawX = x + j;      
      
      int pixel = aquariumfish[i][col];
      if (pixel == 1) {
        SEGMENT.setPixelColorXY(drawX, y + i, ORANGE);
      } else if (pixel == 2) {
        SEGMENT.setPixelColorXY(drawX, y + i, PURPLE);
      }
    }
  }
}

// Function to draw vegetation
void drawVegetation(int x, int y) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (vegetation[i][j] == 1) {
        SEGMENT.setPixelColorXY(x + j, y + i, RGBW32(0, 255, 0, 0)); // Green color
      }
    }
  }
}

// Function to draw bubble
void drawBubble(int x, int y) {
  if (bubble[0][0] == 1) {
    SEGMENT.setPixelColorXY(x, y, RGBW32(0, 0, 255, 0)); // Blue color
  }
}

// Function to animate fish
void animateFish(int &x, int &y, int &direction, unsigned long &lastMoveTime, unsigned long moveInterval, unsigned long &waitTime) {
  unsigned long currentTime = millis();
  if (currentTime - lastMoveTime >= moveInterval) {
    lastMoveTime = currentTime;
    // Move fish horizontally
    if (direction == 0) {
      x++;
      if (x > 16) {
        waitTime = random(1000, 15000); // Wait for 1-5 seconds
        direction = 1; // Change direction to left
      }
    } else {
      x--;
      if (x < -9) {
        waitTime = random(1000, 15000); // Wait for 1-5 seconds
        direction = 0; // Change direction to right
      }
    }
  }
}

// Function to animate bubbles
void animateBubbles(int &x, int &y) {
  // Move bubble upwards
  y--;
  if (y < 0) {
    y = 15;
    x = random(0, 16);
  }
}

// Function to animate vegetation (optional, static in this example)
void animateVegetation() {
  // Vegetation is static in this example
}

// Main aquarium animation function
uint16_t mode_aquarium() {
  static int fishX = -9, fishY = 5, fishDirection = 0;
  static int bubbleX = 8, bubbleY = 15;
  static unsigned long lastFishMoveTime = 0;
  static unsigned long fishWaitTime = 0; 
  const unsigned long fishMoveInterval = 200; // Adjust this value to control fish speed
  

  // Clear the segment
  SEGMENT.fill(RGBW32(0, 0, 28, 0));

  if (lastFishMoveTime + fishWaitTime < millis()) {
    // Draw and animate fish
    fishWaitTime = 0;
    drawFish(fishX, fishY, fishDirection);
    animateFish(fishX, fishY, fishDirection, lastFishMoveTime, fishMoveInterval, fishWaitTime);
  }
  // Draw and animate bubbles
  drawBubble(bubbleX, bubbleY);
  animateBubbles(bubbleX, bubbleY);

  // Draw vegetation
  drawVegetation(2, 13);
  drawVegetation(12, 13);

  return FRAMETIME;
}

// Effect data
static const char _data_FX_MODE_AQUARIUM[] PROGMEM = "Aquarium@Speed;;1;1";

// Usermod class
class UsermodAquarium : public Usermod {
public:
  void setup() {
    Serial.println("Aquarium mod active!");
    // Register new effect in a free slot (example: 253)
    strip.addEffect(FX_MODE_AQUARIUM, &mode_aquarium, _data_FX_MODE_AQUARIUM);
  }
  void loop() {}
  uint16_t getId() { return USERMOD_ID_AQUARIUM; }
};