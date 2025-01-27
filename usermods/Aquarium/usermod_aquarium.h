#pragma once

#include "wled.h"

const int STATE_NORMAL = 0;
const int STATE_MOVING_TO_CENTER = 1;
const int STATE_STAYING_AT_CENTER = 2;
const int STATE_MOVING_OUT = 3;

const int STRAIGHT = 0;
const int UP = 1;
const int DOWN = -1;

struct AnimatedObject {
  const int **sprite; // Pointer to a pointer for flexible sprite width
  int width;
  int height;
  float xPosF;
  float yPosF;
  int direction;  
  unsigned long lastMoveTime;
  unsigned long moveInterval;
  unsigned long waitTime;
  bool frame;
  int state; // Add this field to track the state of the fish
  unsigned long stateStartTime; // Add this field to track the start time of the current state
  int movementType;
  int targetX;
  int targetY;
  float speedX;
  float speedY;
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

const int aquariumfishB[6][9] = {
    {0, 0, 1, 1, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {1, 1, 5, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 0, 0, 1},
    {0, 0, 1, 1, 1, 0, 0, 0, 1},    
};


const int gupfish[7][12] = {
  {0,0,0,0,0,0,0,0,0,0,3,0},
  {0,1,3,1,3,1,3,0,0,1,3,1},
  {1,2,3,2,3,2,3,1,0,1,3,1},
  {1,2,5,2,3,2,3,2,3,2,1,0},
  {1,2,3,2,3,2,3,1,0,1,3,1},
  {0,1,3,1,3,1,3,0,0,1,3,1},
  {0,0,0,0,0,0,0,0,0,0,3,0}  
};

const int gupfishB[7][12] = {
  {0,0,0,0,0,0,0,0,0,0,3,1},
  {0,1,3,1,3,1,3,0,0,1,1,3},
  {1,2,3,2,3,2,3,1,0,1,3,1},
  {1,2,5,2,3,2,3,2,3,2,1,1},
  {1,2,3,2,3,2,3,1,0,1,3,1},
  {0,1,3,1,3,1,3,0,0,1,1,3},
  {0,0,0,0,0,0,0,0,0,0,3,1}  
};


// Shark sprite (12x22)
const int aquariumshark[12][22] = {
  {0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,1,1},
  {0,0,0,0,1,1,1,1,1,2,2,1,1,1,1,0,0,0,0,1,1,0},
  {1,1,1,1,4,4,2,2,2,2,2,2,2,2,2,1,1,1,1,2,1,0},
  {1,2,2,2,5,4,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
  {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,4,1,2,1,0},
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
  {1,1,5,1,1,1,5,1,1},
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
  int xInt = (int)(obj.xPosF + 0.5f);
  int yInt = (int)(obj.yPosF + 0.5f);

  for (int i = 0; i < obj.height; i++) {
    for (int j = 0; j < obj.width; j++) {
      int col = (obj.direction == 0) ? j : (obj.width - 1 - j);
      int drawX = xInt + j;
      int pixel = obj.sprite[i][col];
      if (pixel == 1) {
        SEGMENT.setPixelColorXY(drawX, yInt + i, obj.shade1);
      } else if (pixel == 2) {
        SEGMENT.setPixelColorXY(drawX, yInt + i, obj.shade2);
      } else if (pixel == 3) {
        SEGMENT.setPixelColorXY(drawX, yInt + i, obj.shade3);
      } else if (pixel == 4) {
        SEGMENT.setPixelColorXY(drawX, yInt + i, obj.shade4);
      } else if (pixel == 5) {
        SEGMENT.setPixelColorXY(drawX, yInt + i, PURPLE);
      }
    }
  }
}

void setNewTarget(AnimatedObject &obj, int minX, int maxX, int minY, int maxY) {
  if (obj.direction == 2) {
    // For direction 2: X is random, Y is always -20
    obj.yPosF = 40;
    obj.targetX = random(0, 16);
    obj.targetY = -30;
  } else {
    // Opposite side in X, random Y
    if (obj.direction == 0) {
      obj.targetX = maxX;
      obj.direction = 1;
    } else {
      obj.targetX = minX;
      obj.direction = 0;
    }
    obj.targetY = random(0, 16);
  }

  float dx = obj.targetX - obj.xPosF;
  float dy = obj.targetY - obj.yPosF;
  float dist = sqrt(dx * dx + dy * dy);
  float speed = 1.0f; // Adjust speed as needed
  obj.speedX = (dx / dist) * speed;
  obj.speedY = (dy / dist) * speed;
}

bool animateObject(AnimatedObject &obj) {
  unsigned long currentTime = millis();
  if (currentTime - obj.lastMoveTime >= obj.moveInterval) {
    obj.lastMoveTime = currentTime;

    // Move towards target
    obj.xPosF += obj.speedX;
    obj.yPosF += obj.speedY;
    // Check if target reached
    if (obj.direction == 2) {
      if (fabsf(obj.yPosF - obj.targetY) < 2.0f) {
        obj.targetX = 0;
        obj.targetY = 0;
      }
    } else if (obj.direction == 1 || obj.direction == 0) {
      if (fabsf(obj.xPosF - obj.targetX) < 2.0f) {
        obj.targetX = 0;
        obj.targetY = 0;
        // setNewTarget(obj, minX, maxX, minY, maxY);
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

static const int *fishSprites0[6] = {
  aquariumfish[0], aquariumfish[1], aquariumfish[2], aquariumfish[3], aquariumfish[4], aquariumfish[5]
};
static const int *fishSprites1[6] = {
  aquariumfishB[0], aquariumfishB[1], aquariumfishB[2], aquariumfishB[3], aquariumfishB[4], aquariumfishB[5]
};


static const int *gupSprites0[7] = {
  gupfish[0], gupfish[1], gupfish[2], gupfish[3], gupfish[4], gupfish[5], gupfish[6]
};
static const int *gupSprites1[7] = {
  gupfishB[0], gupfishB[1], gupfishB[2], gupfishB[3], gupfishB[4], gupfishB[5], gupfishB[6]
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


uint16_t mode_aquarium() {
 

  static AnimatedObject fish[2] = {
    {fishSprites0, 9, 6, 25, 3, 0, 0, 50, 4530, 0, STATE_NORMAL, millis(), -1, 0,0,0,0, RGBW32(255, 30, 0, 0)},
    {fishSprites1, 9, 6, 25, 6, 1, 0, 250, 1000, 0, STATE_NORMAL, millis(), 0, 0,0,0,0, RGBW32(162, 171, 0, 0)},   
  };

  static AnimatedObject gup[2] = {    
    {gupSprites0, 12, 7, -19, 12, 0, 0, 100, 3440, 0, STATE_NORMAL, millis(), 1, 0,0,0,0, RGBW32(255, 100, 0, 0), RGBW32(255, 110, 35, 0), RGBW32(128, 128, 128, 0)},
    {gupSprites0, 12, 7, -19, 9, 1, 0, 500, 10000, 0, STATE_NORMAL, millis(), 0,0,0,0,0, RGBW32(255, 172, 0, 0), RGBW32(219, 158, 36, 1), RGBW32(128, 128, 128, 0)}
  };

  static AnimatedObject shark = {sharkSprites, 22, 12, -22, 1, 0, 0, 300, 120000, 0, STATE_NORMAL, millis(), 0, 0,0,0,0, RGBW32(20, 20, 20, 0), RGBW32(40, 40, 40, 0),RGBW32(100, 100, 100, 0),RGBW32(30, 30, 30, 0),RGBW32(100, 0, 100, 0) };
  // sprite, width, height, x, y, direction, lastMoveTime, moveInterval, waitTime, frame, state, stateStartTime, movementType, targetX, targetY, speedX, speedY, shade1, shade2, shade3, shade4, shade5

  static AnimatedObject jellyfish = {jellyfishSprites0, 9, 12, 4, -22, 2, 0, 300, 58000, 0, STATE_NORMAL, millis(), 1, 0,0,0,0, RGBW32(128, 24, 28, 0), RGBW32(128, 24, 28, 0),RGBW32(100, 100, 100, 0),RGBW32(30, 30, 30, 0),RGBW32(100, 0, 100, 0) };

  // Clear the segment
  SEGMENT.fill(RGBW32(0, 0, 28, 0));


  if (shark.targetX == 0 && shark.targetY == 0) {
    setNewTarget(shark, -25, 25, -4, 20);
    jellyfish.waitTime = random(40000, 120000);
    shark.lastMoveTime = millis();
  }
  // Draw and animate shark
  if (shark.lastMoveTime + shark.waitTime < millis()) {
    shark.waitTime = 0;
    drawObject(shark);
    animateObject(shark);
  }

  // Draw and animate fish
  for (int i = 0; i < 2; i++) {
    if (fish[i].targetX == 0 && fish[i].targetY == 0) {
      setNewTarget(fish[i], -25, 25, -4, 20);
      fish[i].waitTime = random(1000, 20000);
      fish[i].lastMoveTime = millis();
    }
    if (fish[i].lastMoveTime + fish[i].waitTime < millis()) {
      fish[i].waitTime = 0;
      drawObject(fish[i]);
      if (animateObject(fish[i]))
      {
         fish[i].sprite = fish[i].frame ? fishSprites0 : fishSprites1; 
         fish[i].frame = !fish[i].frame;
      }
    }
  }

  for (int i = 0; i < 1; i++) {
    if (gup[i].targetX == 0 && gup[i].targetY == 0) {
      setNewTarget(gup[i], -25, 25, 0, 16);
      gup[i].waitTime = random(1000, 20000);
      gup[i].lastMoveTime = millis();
    }
    if (gup[i].lastMoveTime + gup[i].waitTime < millis()) {
      gup[i].waitTime = 0;
      drawObject(gup[i]);
      if (animateObject(gup[i]))
      {
         gup[i].sprite = gup[i].frame ? gupSprites0 : gupSprites1; 
         gup[i].frame = !gup[i].frame;
      }
    }
  }

  if (jellyfish.targetX == 0 && jellyfish.targetY == 0) {
    setNewTarget(jellyfish, -4, 20, -20, 30);
    jellyfish.waitTime = random(20000, 80000);
    jellyfish.lastMoveTime = millis();
  }
  if (jellyfish.lastMoveTime + jellyfish.waitTime < millis()) {
    
    jellyfish.waitTime = 0;
    drawObject(jellyfish);
    if (animateObject(jellyfish)) 
    {
      jellyfish.sprite = jellyfish.frame ? jellyfishSprites0 : jellyfishSprites1; 
      jellyfish.frame = !jellyfish.frame;
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