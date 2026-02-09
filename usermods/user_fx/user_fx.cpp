#include "wled.h"

// for information how FX metadata strings work see https://kno.wled.ge/interfaces/json-api/#effect-metadata

// paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined)
#define PALETTE_SOLID_WRAP   (strip.paletteBlend == 1 || strip.paletteBlend == 3)

// static effect, used if an effect fails to initialize
static uint16_t mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
  return strip.isOffRefreshRequired() ? FRAMETIME : 350;
}

// If you define configuration options in your class and need to reference them in your effect function, add them here.
// If you only need to use them in your class you can define them as class members instead.
// bool myConfigValue = false;

/////////////////////////
//  User FX functions  //
/////////////////////////

// Diffusion Fire: fire effect intended for 2D setups smaller than 16x16
static uint16_t mode_diffusionfire(void) {
  if (!strip.isMatrix || !SEGMENT.is2D())
    return mode_static();  // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const auto XY = [&](int x, int y) { return x + y * cols; };

  const uint8_t refresh_hz = map(SEGMENT.speed, 0, 255, 20, 80);
  const unsigned refresh_ms = 1000 / refresh_hz;
  const int16_t diffusion = map(SEGMENT.custom1, 0, 255, 0, 100);
  const uint8_t spark_rate = SEGMENT.intensity;
  const uint8_t turbulence = SEGMENT.custom2;

unsigned dataSize = cols * rows;  // SEGLEN (virtual length) is equivalent to vWidth()*vHeight() for 2D
  if (!SEGENV.allocateData(dataSize))
    return mode_static();  // allocation failed

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
    SEGENV.step = 0;
  }

  if ((strip.now - SEGENV.step) >= refresh_ms) {
    // Keep for ≤~1 KiB; otherwise consider heap or reuse SEGENV.data as scratch.
    uint8_t tmp_row[cols];
    SEGENV.step = strip.now;
    // scroll up
    for (unsigned y = 1; y < rows; y++)
      for (unsigned x = 0; x < cols; x++) {
        unsigned src = XY(x, y);
        unsigned dst = XY(x, y - 1);
        SEGENV.data[dst] = SEGENV.data[src];
      }

    if (hw_random8() > turbulence) {
      // create new sparks at bottom row
      for (unsigned x = 0; x < cols; x++) {
        uint8_t p = hw_random8();
        if (p < spark_rate) {
          unsigned dst = XY(x, rows - 1);
          SEGENV.data[dst] = 255;
        }
      }
    }

    // diffuse
    for (unsigned y = 0; y < rows; y++) {
      for (unsigned x = 0; x < cols; x++) {
        unsigned v = SEGENV.data[XY(x, y)];
        if (x > 0) {
          v += SEGENV.data[XY(x - 1, y)];
        }
        if (x < (cols - 1)) {
          v += SEGENV.data[XY(x + 1, y)];
        }
        tmp_row[x] = min(255, (int)(v * 100 / (300 + diffusion)));
      }

      for (unsigned x = 0; x < cols; x++) {
        SEGENV.data[XY(x, y)] = tmp_row[x];
        if (SEGMENT.check1) {
          uint32_t color = SEGMENT.color_from_palette(tmp_row[x], true, false, 0);
          SEGMENT.setPixelColorXY(x, y, color);
        } else {
          uint32_t base = SEGCOLOR(0);
          SEGMENT.setPixelColorXY(x, y, color_fade(base, tmp_row[x]));
        }
      }
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_DIFFUSIONFIRE[] PROGMEM = "Diffusion Fire@!,Spark rate,Diffusion Speed,Turbulence,,Use palette;;Color;;2;pal=35";


/*
/  Lava Lamp 2D effect
*   Uses particles to simulate rising blobs of "lava"
*   Particles slowly rise, merge to create organic flowing shapes, and then fall to the bottom to start again
*   Created by Bob Loeffler using claude.ai
*   The first slider sets the speed of the rising and falling blobs
*   The second slider sets the number of active blobs
*   The third slider sets the size range of the blobs
*   The first checkbox sets the color mode (color wheel or palette)
*   The second checkbox sets the attraction of blobs (checked will make the blobs attract other close blobs horizontally)
*   aux0 keeps track of the blob size changes
*/

typedef struct LavaParticle {
  float x, y;           // Position
  float vx, vy;         // Velocity
  float size;           // Blob size
  uint8_t hue;          // Color
  bool active;          // will not be displayed if false
} LavaParticle;

static uint16_t mode_2D_lavalamp(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up
  
  const uint16_t cols = SEG_W;
  const uint16_t rows = SEG_H;
  
  // Allocate per-segment storage
  constexpr size_t MAX_LAVA_PARTICLES = 35;  // increasing this value could cause slowness for large matrices
  if (!SEGENV.allocateData(sizeof(LavaParticle) * MAX_LAVA_PARTICLES)) return mode_static();
  LavaParticle* lavaParticles = reinterpret_cast<LavaParticle*>(SEGENV.data);

  // Initialize particles on first call
  if (SEGENV.call == 0) {
    for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
      lavaParticles[i].active = false;
    }
  }
 
  // Intensity controls number of active particles
  uint8_t numParticles = (SEGMENT.intensity >> 3) + 3; // 3-34 particles (fewer blobs)
  if (numParticles > MAX_LAVA_PARTICLES) numParticles = MAX_LAVA_PARTICLES;
  
  // Track size slider changes
  uint8_t lastSizeControl = SEGENV.aux0;
  uint8_t currentSizeControl = SEGMENT.custom1;
  bool sizeChanged = (currentSizeControl != lastSizeControl);

  if (sizeChanged) {
    // Recalculate size range based on new slider value
    float minSize = cols * 0.15f;
    float maxSize = cols * 0.4f;
    float newRange = (maxSize - minSize) * (currentSizeControl / 255.0f);
    
    for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
      if (lavaParticles[i].active) {
        // Assign new random size within the new range
        int rangeInt = max(1, (int)(newRange));
        lavaParticles[i].size = minSize + (float)random16(rangeInt);
        // Ensure minimum size
        if (lavaParticles[i].size < minSize) lavaParticles[i].size = minSize;
      }
    }
    SEGENV.aux0 = currentSizeControl;
  }

  // Spawn new particles at the bottom near the center
  for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
    if (!lavaParticles[i].active && hw_random8() < 32) { // sporadically spawn when slot available
      // Spawn in the middle 60% of the width
      float centerStart = cols * 0.20f;
      float centerWidth = cols * 0.60f;
      int cwInt = max(1, (int)(centerWidth));
      lavaParticles[i].x = centerStart + (float)random16(cwInt);
      lavaParticles[i].y = rows - 1;
      lavaParticles[i].vx = (random16(7) - 3) / 250.0f;
      
      // Speed slider controls vertical velocity (faster = more speed)
      float speedFactor = (SEGMENT.speed + 30) / 100.0f; // 0.3 to 2.85 range
      lavaParticles[i].vy = -(random16(20) + 10) / 100.0f * speedFactor;
      
      // Custom1 slider controls blob size (based on matrix width)
      uint8_t sizeControl = SEGMENT.custom1; // 0-255
      float minSize = cols * 0.15f; // Minimum 15% of width
      float maxSize = cols * 0.4f;  // Maximum 40% of width
      float sizeRange = (maxSize - minSize) * (sizeControl / 255.0f);
      int rangeInt = max(1, (int)(sizeRange));
      constexpr float MAX_BLOB_RADIUS = 20.0f; // cap to prevent frame rate drops on large matrices
      lavaParticles[i].size = minSize + (float)random16(rangeInt);
      if (lavaParticles[i].size > MAX_BLOB_RADIUS) lavaParticles[i].size = MAX_BLOB_RADIUS;

      lavaParticles[i].hue = hw_random8();
      lavaParticles[i].active = true;
      break;
    }
  }

  // Fade background slightly for trailing effect
  SEGMENT.fadeToBlackBy(40);
  
  // Update and draw particles
  int activeCount = 0;
  unsigned long currentMillis = strip.now;
  for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
    if (!lavaParticles[i].active) continue;
    activeCount++;

    // Keep particle count on target by deactivating excess particles
    if (activeCount > numParticles) {
      lavaParticles[i].active = false;
      activeCount--;
      continue;
    }

    LavaParticle *p = &lavaParticles[i];
    
    // Physics update
    p->x += p->vx;
    p->y += p->vy;
    
    // Optional blob attraction (enabled with check2)
    if (SEGMENT.check2) {
      for (int j = 0; j < MAX_LAVA_PARTICLES; j++) {
        if (i == j || !lavaParticles[j].active) continue;
        
        LavaParticle *other = &lavaParticles[j];
        
        // Skip attraction if moving in same vertical direction (both up or both down)
        if ((p->vy < 0 && other->vy < 0) || (p->vy > 0 && other->vy > 0)) continue;
        
        float dx = other->x - p->x;
        float dy = other->y - p->y;

        // Apply weak horizontal attraction only
        float attractRange = p->size + other->size;
        float distSq = dx*dx + dy*dy;
        float attractRangeSq = attractRange * attractRange;
        if (distSq > 0 && distSq < attractRangeSq) {
          float dist = sqrt(distSq); // Only compute sqrt when needed
          float force = (1.0f - (dist / attractRange)) * 0.0001f;
          p->vx += (dx / dist) * force;
        }
      }
    }

    // Horizontal oscillation (makes it more organic)
    p->vx += sin((currentMillis / 1000.0f + i) * 0.5f) * 0.002f; // Reduced oscillation
    p->vx *= 0.92f; // Stronger damping for less drift
    
    // Bounce off sides (don't affect vertical velocity)
    if (p->x < 0) {
      p->x = 0;
      p->vx = abs(p->vx); // Just reverse horizontal, don't reduce
    }
    if (p->x >= cols) {
      p->x = cols - 1;
      p->vx = -abs(p->vx); // Just reverse horizontal, don't reduce
    }
    
    // Boundary handling with proper reversal
    // When reaching TOP (y=0 area), reverse to fall back down
    if (p->y <= 0.5f * p->size) {
      p->y = 0.5f * p->size;
      if (p->vy < 0) {
        p->vy = -p->vy * 0.5f; // Reverse to positive (fall down) at HALF speed
        // Ensure minimum downward velocity
        if (p->vy < 0.06f) p->vy = 0.06f;
      }
    }

    // When reaching BOTTOM (y=rows-1 area), reverse to rise back up
    if (p->y >= rows - 0.5f * p->size) {
      p->y = rows - 0.5f * p->size;
      if (p->vy > 0) {
        p->vy = -p->vy; // Reverse to negative (rise up)
        // Add random speed boost when rising
        p->vy -= random16(15) / 100.0f; // Subtract to make MORE negative (faster up)
        // Ensure minimum upward velocity
        if (p->vy > -0.10f) p->vy = -0.10f;
      }
    }
    
    // Get color
    uint32_t color;
    if (SEGMENT.check1) {
      color = SEGMENT.color_wheel(p->hue);  // Random colors mode
    } else {
      color = SEGMENT.color_from_palette(p->hue, true, PALETTE_SOLID_WRAP, 0);   // Palette mode
    }
    
    // Extract RGB and apply life/opacity
    uint8_t w = (W(color) * 255) >> 8;
    uint8_t r = (R(color) * 255) >> 8;
    uint8_t g = (G(color) * 255) >> 8;
    uint8_t b = (B(color) * 255) >> 8;

    // Draw blob with soft edges (gaussian-like falloff)
    float sizeSq = p->size * p->size;
    for (int dy = -(int)p->size; dy <= (int)p->size; dy++) {
      for (int dx = -(int)p->size; dx <= (int)p->size; dx++) {
        int px = (int)(p->x + dx);
        int py = (int)(p->y + dy);
        
        if (px >= 0 && px < cols && py >= 0 && py < rows) {
          float distSq = dx*dx + dy*dy;
          if (distSq < sizeSq) {
            // Soft falloff using squared distance (faster)
            float intensity = 1.0f - (distSq / sizeSq);
            intensity = intensity * intensity; // Square for smoother falloff
            
            uint8_t bw = w * intensity;
            uint8_t br = r * intensity;
            uint8_t bg = g * intensity;
            uint8_t bb = b * intensity;

            // Additive blending for organic merging
            uint32_t existing = SEGMENT.getPixelColorXY(px, py);
            uint32_t newColor = RGBW32(br, bg, bb, bw);
            uint32_t blended = color_add(existing, newColor);
            SEGMENT.setPixelColorXY(px, py, blended);
          }
        }
      }
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_2D_LAVALAMP[] PROGMEM = "Lava Lamp@Speed,# of blobs,Blob size,,,Color mode,Attract;;!;2;ix=64,o2=1,pal=47";



/////////////////////
//  UserMod Class  //
/////////////////////

class UserFxUsermod : public Usermod {
 private:
 public:
  void setup() override {
    strip.addEffect(255, &mode_diffusionfire, _data_FX_MODE_DIFFUSIONFIRE);
    strip.addEffect(255, &mode_2D_lavalamp, _data_FX_MODE_2D_LAVALAMP);

    ////////////////////////////////////////
    //  add your effect function(s) here  //
    ////////////////////////////////////////

    // use id=255 for all custom user FX (the final id is assigned when adding the effect)

    // strip.addEffect(255, &mode_your_effect, _data_FX_MODE_YOUR_EFFECT);
    // strip.addEffect(255, &mode_your_effect2, _data_FX_MODE_YOUR_EFFECT2);
    // strip.addEffect(255, &mode_your_effect3, _data_FX_MODE_YOUR_EFFECT3);
  }

  
  ///////////////////////////////////////////////////////////////////////////////////////////////
  //  If you want configuration options in the usermod settings page, implement these methods  //
  ///////////////////////////////////////////////////////////////////////////////////////////////

  // void addToConfig(JsonObject& root) override
  // {
  //   JsonObject top = root.createNestedObject(FPSTR("User FX"));
  //   top["myConfigValue"] = myConfigValue;
  // }
  // bool readFromConfig(JsonObject& root) override
  // {
  //   JsonObject top = root[FPSTR("User FX")];
  //   bool configComplete = !top.isNull();
  //   configComplete &= getJsonValue(top["myConfigValue"], myConfigValue);
  //   return configComplete;
  // }

  void loop() override {} // nothing to do in the loop
  uint16_t getId() override { return USERMOD_ID_USER_FX; }
};

static UserFxUsermod user_fx;
REGISTER_USERMOD(user_fx);
