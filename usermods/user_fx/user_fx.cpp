#include "wled.h"

// for information how FX metadata strings work see https://kno.wled.ge/interfaces/json-api/#effect-metadata

// static effect, used if an effect fails to initialize
static uint16_t mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
  return strip.isOffRefreshRequired() ? FRAMETIME : 350;
}

#define PALETTE_SOLID_WRAP   (paletteBlend == 1 || paletteBlend == 3)

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
/  Magma effect
*   2D magma/lava animation
*   Adapted from FireLamp_JeeUI implementation (https://github.com/DmytroKorniienko/FireLamp_JeeUI/tree/dev)
*   Original idea by SottNick, remastered by kostyamat
*   Adapted to WLED by Bob Loeffler and claude.ai
*   First slider (speed) is for the speed or flow rate of the moving magma.
*   Second slider (intensity) is for the height of the magma.
*   Third slider (lava bombs) is for the number of lava bombs (particles).  The max # is 1/2 the number of columns on the 2D matrix.
*   Fourth slider (gravity) is for how high the lava bombs will go.
*   The checkbox (check2) is for whether the lava bombs can be seen in the magma or behind it.
*/

// Draw the magma
void drawMagma(const uint16_t width, const uint16_t height, float *ff_y, float *ff_z, uint8_t *shiftHue) {
  // Noise parameters - adjust these for different magma characteristics
  // deltaValue: higher = more detailed/turbulent magma
  // deltaHue: higher = taller magma structures
  constexpr uint8_t magmaDeltaValue = 12U;
  constexpr uint8_t magmaDeltaHue   = 10U;

  uint16_t ff_y_int = (uint16_t)*ff_y;
  uint16_t ff_z_int = (uint16_t)*ff_z;

  for (uint16_t i = 0; i < width; i++) {
    for (uint16_t j = 0; j < height; j++) {
      // Generate Perlin noise value (0-255)
      uint8_t noise = perlin8(i * magmaDeltaValue, (j + ff_y_int + random8(2)) * magmaDeltaHue, ff_z_int);

      // Apply the vertical fade gradient: j=0 (top of loop) has shiftHue=255, j=height-1 (bottom) has shiftHue=0
      uint8_t paletteIndex = qsub8(noise, shiftHue[j]);

      // Get color from palette
      CRGB col = SEGMENT.color_from_palette(paletteIndex, false, PALETTE_SOLID_WRAP, 0);

      // magma rises from bottom of display
      SEGMENT.addPixelColorXY(i, height - 1 - j, col);
    }
  }
}

// Move and draw lava bombs (particles)
void drawLavaBombs(const uint16_t width, const uint16_t height, float *particleData, float gravity, uint8_t particleCount) {
  for (uint8_t i = 0; i < particleCount; i++) {
    uint8_t idx = i * 4;
    
    particleData[idx + 3] -= gravity;
    particleData[idx + 0] += particleData[idx + 2];
    particleData[idx + 1] += particleData[idx + 3];
    
    float posX = particleData[idx + 0];
    float posY = particleData[idx + 1];
    
    if (posY > height + height / 4) {
      particleData[idx + 3] = -particleData[idx + 3] * 0.8f;
    }
    
    if (posY < height / 8 - 1 || posX < 0 || posX >= width) {
      particleData[idx + 0] = hw_random(0, width * 100) / 100.0f;
      particleData[idx + 1] = hw_random(0, height * 25) / 100.0f;
      particleData[idx + 2] = hw_random(-75, 75) / 100.0f;
      
      float baseVelocity = hw_random(60, 120) / 100.0f;
      if (hw_random8() < 50) {
        baseVelocity *= 1.6f;
      }
      particleData[idx + 3] = baseVelocity;
      continue;
    }
    
    int16_t xi = (int16_t)posX;
    int16_t yi = (int16_t)posY;
    
    if (xi >= 0 && xi < width && yi >= 0 && yi < height) {
      float velocityY = particleData[idx + 3];
      uint8_t cooling = 0;
      if (velocityY < 0) {
        cooling = max((uint8_t)(fabsf(velocityY) * 40), (uint8_t)0);
      }
      
      CRGB pcolor = CRGB(255, 96 - cooling, 0);
      
      // Pre-calculate anti-aliasing weights
      float xf = posX - xi;
      float yf = posY - yi;
      float ix = 1.0f - xf;
      float iy = 1.0f - yf;
      
      uint8_t w0 = 255 * ix * iy;
      uint8_t w1 = 255 * xf * iy;
      uint8_t w2 = 255 * ix * yf;
      uint8_t w3 = 255 * xf * yf;
      
      int16_t yFlipped = height - 1 - yi;  // Flip Y coordinate
  
      SEGMENT.addPixelColorXY(xi, yFlipped, pcolor.scale8(w0));
      if (xi + 1 < width) 
        SEGMENT.addPixelColorXY(xi + 1, yFlipped, pcolor.scale8(w1));
      if (yf - 1 >= 0)
        SEGMENT.addPixelColorXY(xi, yFlipped - 1, pcolor.scale8(w2));
      if (xi + 1 < width && yFlipped - 1 >= 0) 
        SEGMENT.addPixelColorXY(xi + 1, yFlipped - 1, pcolor.scale8(w3));
    }
  }
} 

uint16_t mode_2D_magma(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static();  // not a 2D set-up
  const uint16_t width = SEG_W;
  const uint16_t height = SEG_H;
  const uint8_t MAGMA_MAX_PARTICLES = width / 2;
  constexpr size_t SETTINGS_SUM_BYTES = 4; // 4 bytes for settings sum

  // Allocate memory: particles (4 floats each) + 2 floats for noise counters + shiftHue cache + settingsSum
  const uint16_t dataSize = (MAGMA_MAX_PARTICLES * 4 + 2) * sizeof(float) + height * sizeof(uint8_t) + SETTINGS_SUM_BYTES;
  if (!SEGENV.allocateData(dataSize)) return mode_static();  // allocation failed

  float* particleData = reinterpret_cast<float*>(SEGENV.data);
  float* ff_y = &particleData[MAGMA_MAX_PARTICLES * 4];
  float* ff_z = &particleData[MAGMA_MAX_PARTICLES * 4 + 1];
  uint32_t* settingsSumPtr = reinterpret_cast<uint32_t*>(&particleData[MAGMA_MAX_PARTICLES * 4 + 2]);
  uint8_t* shiftHue = reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(settingsSumPtr) + SETTINGS_SUM_BYTES);

  // Check if settings changed
  uint32_t settingssum = SEGMENT.speed + SEGMENT.intensity + SEGMENT.custom1 + SEGMENT.custom2;
  bool settingsChanged = (*settingsSumPtr != settingssum);

  if (SEGENV.call == 0 || settingsChanged) {
    // Intensity slider controls magma height
    uint16_t intensity = SEGMENT.intensity;
    uint16_t fadeRange = map(intensity, 0, 255, height / 3, height);

    // shiftHue controls the vertical color gradient (magma fades out toward top)
    for (uint16_t j = 0; j < height; j++) {
      if (j < fadeRange) {
        // prevent division issues and ensure smooth gradient
        if (fadeRange > 1) {
          shiftHue[j] = (uint8_t)(j * 255 / (fadeRange - 1));
        } else {
          shiftHue[j] = 0;  // Single row magma = no fade
        }
      } else {
        shiftHue[j] = 255;
      }
    }

    // Initialize all particles
    for (uint8_t i = 0; i < MAGMA_MAX_PARTICLES; i++) {
      uint8_t idx = i * 4;
      particleData[idx + 0] = hw_random(0, width * 100) / 100.0f;
      particleData[idx + 1] = hw_random(0, height * 25) / 100.0f;
      particleData[idx + 2] = hw_random(-75, 75) / 100.0f;
      
      float baseVelocity = hw_random(60, 120) / 100.0f;
      if (hw_random8() < 50) {
        baseVelocity *= 1.6f;
      }
      particleData[idx + 3] = baseVelocity;
    }
    *ff_y = 0.0f;
    *ff_z = 0.0f;
    *settingsSumPtr = settingssum;
  }

  if (!shiftHue) return FRAMETIME;   // safety check

  // Speed control
  float speedfactor = SEGMENT.speed / 255.0f;
  speedfactor = speedfactor * speedfactor * 1.5f;
  if (speedfactor < 0.001f) speedfactor = 0.001f;

  // Gravity control
  float gravity = map(SEGMENT.custom2, 0, 255, 5, 20) / 100.0f;
  
  // Number of particles
  uint8_t particleCount = map(SEGMENT.custom1, 0, 255, 2, MAGMA_MAX_PARTICLES);
  particleCount = constrain(particleCount, 2, MAGMA_MAX_PARTICLES);

  // Draw lava bombs in front of magma (or behind it)
  if (SEGMENT.check2) {
    drawMagma(width, height, ff_y, ff_z, shiftHue);
    SEGMENT.fadeToBlackBy(70);    // Dim the entire display to create trailing effect
    drawLavaBombs(width, height, particleData, gravity, particleCount);
  }
  else {
    drawLavaBombs(width, height, particleData, gravity, particleCount);
    SEGMENT.fadeToBlackBy(70);    // Dim the entire display to create trailing effect
    drawMagma(width, height, ff_y, ff_z, shiftHue);
  }

  // noise counters based on speed slider
  *ff_y += speedfactor * 2.0f;
  *ff_z += speedfactor;

  SEGENV.step++;

  return FRAMETIME;
}
static const char _data_FX_MODE_2D_MAGMA[] PROGMEM = "Magma@Flow rate,Magma height,Lava bombs,Gravity,,,Bombs in front;;!;2;ix=192,c2=32,o2=1,pal=35";



/////////////////////
//  UserMod Class  //
/////////////////////

class UserFxUsermod : public Usermod {
 private:
 public:
  void setup() override {
    strip.addEffect(255, &mode_diffusionfire, _data_FX_MODE_DIFFUSIONFIRE);
    strip.addEffect(255, &mode_2D_magma, _data_FX_MODE_2D_MAGMA);

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
