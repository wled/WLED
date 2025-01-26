#pragma once

#include "wled.h"

struct AnimatedObject {
  const int **sprite; // Pointer to a pointer for flexible sprite width
  int width;
  int height;
  int x;
  int y;
  int direction;  
  unsigned long lastMoveTime;
  unsigned long moveInterval;
  unsigned long waitTime;
  long shade1;
  long shade2;
  long shade3;
  long shade4;
  long shade5;
};
// Fish sprite (6x9)
const int aquariumfish[6][9] = {
    {0, 0, 1, 1, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {1, 1, 5, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {0, 0, 1, 1, 1, 0, 0, 0, 1},    
};

const int gupfish[7][12] = {
  {0,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,1,1,1,1,1,0,0,1,2,1},
  {1,2,2,2,2,2,2,1,0,1,2,1},
  {1,2,5,2,1,2,2,2,1,2,1,0},
  {1,2,2,2,1,2,2,1,0,1,2,1},
  {0,1,1,1,1,1,1,0,0,1,2,1},
  {0,0,0,0,0,0,0,0,0,0,1,0}  
};


// Shark sprite (12x22)
const int aquariumshark[12][22] = {
  {0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,1,1},
  {0,0,0,0,1,1,1,1,1,2,2,1,1,1,1,0,0,0,0,1,1,0},
  {1,1,1,1,3,3,2,2,2,2,2,2,2,2,2,1,1,1,1,2,1,0},
  {1,2,2,2,5,3,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
  {0,1,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,4,1,2,1,0},
  {0,0,1,3,3,3,3,2,2,2,2,4,4,4,4,4,4,1,0,1,1,0},
  {0,0,0,1,3,3,4,4,4,4,2,2,1,1,1,1,1,0,0,0,1,1},
  {0,0,0,0,1,1,1,1,1,1,1,2,2,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0}  
};

const int jelleyfishA[12][9] = {
  {0,0,1,1,1,1,1,0,0},
  {0,1,1,1,1,1,1,1,0},
  {1,1,5,1,1,1,5,1,1},
  {1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1},
  {0,1,1,1,1,1,1,1,0},
  {0,1,0,1,0,1,0,1,0},
  {1,1,0,1,0,1,0,1,0},
  {1,0,1,0,1,0,1,0,1},
  {1,0,1,0,1,0,1,0,1},
  {0,1,0,1,1,1,0,1,0},
  {0,1,0,1,0,1,0,1,0}   
};

const int jelleyfishB[12][9] = {
  {0,0,1,1,1,1,1,0,0},
  {0,1,1,1,1,1,1,1,0},
  {1,1,2,1,1,1,2,1,1},
  {1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1},
  {0,1,1,1,1,1,1,1,0},
  {0,2,0,2,0,2,0,2,0},
  {2,0,2,0,2,0,2,0,2},
  {0,2,0,2,2,2,0,2,0},
  {0,2,0,2,0,2,0,2,0},   
  {2,0,2,0,2,0,2,0,2},
  {2,0,2,0,2,0,2,0,2}  
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

void drawObject(const AnimatedObject &obj) {
  for (int i = 0; i < obj.height; i++) {
    for (int j = 0; j < obj.width; j++) {
      int col = (obj.direction == 1) ? j : (obj.width - 1 - j);
      int drawX = obj.x + j;
      int pixel = obj.sprite[i][col];
      if (pixel == 1) {
        SEGMENT.setPixelColorXY(drawX, obj.y + i, obj.shade1);
      } else if (pixel == 2) {
        SEGMENT.setPixelColorXY(drawX, obj.y + i, obj.shade2);
      } else if (pixel == 3) {
        SEGMENT.setPixelColorXY(drawX, obj.y + i, obj.shade3);
      } else if (pixel == 4) {
        SEGMENT.setPixelColorXY(drawX, obj.y + i, obj.shade4);
      } else if (pixel == 5) {
        SEGMENT.setPixelColorXY(drawX, obj.y + i, PURPLE);
      }
    }
  }
}

bool animateObject(AnimatedObject &obj, int maxX, int minX, int maxY, int minY) {
  unsigned long currentTime = millis();
  if (currentTime - obj.lastMoveTime >= obj.moveInterval) {
    obj.lastMoveTime = currentTime;
    if (obj.direction == 0) {
      obj.x++;
      if (obj.x > maxX) {
        obj.waitTime = random(1000, 20000);
        obj.moveInterval = random(20, 300);
        obj.y = random(minY, maxY);
        obj.direction = 1;
      }
    } else if (obj.direction == 1) { // Move left
      obj.x--;
      if (obj.x < minX) {
        obj.waitTime = random(1000, 20000);
        obj.moveInterval = random(20, 300);
        obj.y = random(minY, maxY);
        obj.direction = 0; // Change direction to right
      }
    } else if (obj.direction == 2) { // Move up
      obj.y--;
      if (obj.y < minY) {
        obj.waitTime = random(1000, 20000);
        obj.moveInterval = random(20, 300);
        obj.x = random(minX, maxX);
        obj.direction = 2; // Change direction to down
        obj.y = maxY;
      }
    } 
    return true;
  }
  return false;
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

uint16_t mode_aquarium() {
  static const int *fishSprites0[6] = {
    aquariumfish[0], aquariumfish[1], aquariumfish[2], aquariumfish[3], aquariumfish[4], aquariumfish[5]
  };
  static const int *fishSprites1[6] = {
    aquariumfish[0], aquariumfish[1], aquariumfish[2], aquariumfish[3], aquariumfish[4], aquariumfish[5]
  };
  static const int *fishSprites2[7] = {
    gupfish[0], gupfish[1], gupfish[2], gupfish[3], gupfish[4], gupfish[5], gupfish[6]
  };
  static const int *fishSprites3[7] = {
    gupfish[0], gupfish[1], gupfish[2], gupfish[3], gupfish[4], gupfish[5], gupfish[6]
  };

  static const int *jellyfishSprites0[12] = {
    jelleyfishA[0], jelleyfishA[1], jelleyfishA[2], jelleyfishA[3], jelleyfishA[4], jelleyfishA[5], jelleyfishA[6], jelleyfishA[7], jelleyfishA[8], jelleyfishA[9], jelleyfishA[10], jelleyfishA[11]
  };

  static const int *jellyfishSprites1[12] = {
    jelleyfishB[0], jelleyfishB[1], jelleyfishB[2], jelleyfishB[3], jelleyfishB[4], jelleyfishB[5], jelleyfishB[6], jelleyfishB[7], jelleyfishB[8], jelleyfishB[9], jelleyfishB[10], jelleyfishB[11]
  };

  static const int *sharkSprites[12] = {
    aquariumshark[0], aquariumshark[1], aquariumshark[2], aquariumshark[3], aquariumshark[4],
    aquariumshark[5], aquariumshark[6], aquariumshark[7], aquariumshark[8], aquariumshark[9],
    aquariumshark[10], aquariumshark[11]
  };

  static AnimatedObject fish[4] = {
    {fishSprites0, 9, 6, 25, 3, 0, 0, 50, 4530, RGBW32(255, 30, 0, 0)},
    {fishSprites1, 9, 6, 25, 6, 1, 0, 250, 1000, RGBW32(128, 104, 128, 0)},
    {fishSprites2, 12, 7, -19, 9, 1, 0, 500, 10000, RGBW32(150, 100, 0, 0),  RGBW32(50, 50, 0, 0)},
    {fishSprites3, 12, 7, -19, 12, 0, 0, 100, 3440, RGBW32(150, 52, 52, 0), ORANGE}
  };

  static AnimatedObject shark = {sharkSprites, 22, 12, -22, 1, 0, 0, 300, 0, RGBW32(20, 20, 20, 0), RGBW32(40, 40, 40, 0),RGBW32(100, 100, 100, 0),RGBW32(30, 30, 30, 0),RGBW32(100, 0, 100, 0) };

  static AnimatedObject jellyfish = {jellyfishSprites0, 9, 12, 4, -22, 2, 0, 300, 0, RGBW32(128, 24, 28, 0), RGBW32(128, 24, 28, 0),RGBW32(100, 100, 100, 0),RGBW32(30, 30, 30, 0),RGBW32(100, 0, 100, 0) };
  static bool jellyfishFrame = true;

  // Clear the segment
  SEGMENT.fill(RGBW32(0, 0, 28, 0));

  // Draw and animate shark
  if (shark.lastMoveTime + shark.waitTime < millis()) {
    shark.waitTime = 0;
    drawObject(shark);
    animateObject(shark, 16, -22, 12, 15);
  }

  // Draw and animate fish
  for (int i = 0; i < 4; i++) {
    if (fish[i].lastMoveTime + fish[i].waitTime < millis()) {
      fish[i].waitTime = 0;
      drawObject(fish[i]);
      animateObject(fish[i], 25, -25, 20, -10);
    }
  }

    if (jellyfish.lastMoveTime + jellyfish.waitTime < millis()) {
      
      jellyfish.waitTime = 0;
      drawObject(jellyfish);
      if (animateObject(jellyfish, 16, 0, 25, -25)) 
      {
        jellyfish.sprite = jellyfishFrame ? jellyfishSprites0 : jellyfishSprites1; 
        jellyfishFrame = !jellyfishFrame;
      };
    }

  // Draw and animate bubbles
  static int bubbleX = 8, bubbleY = 15;
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