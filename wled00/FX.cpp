/*
  WS2812FX.cpp contains all effect methods
  Harm Aldick - 2016
  www.aldick.org

  Copyright (c) 2016  Harm Aldick
  Licensed under the EUPL v. 1.2 or later
  Adapted from code originally licensed under the MIT license

  Modified heavily for WLED
*/

#include "wled.h"
#include "FX.h"
#include "fcn_declare.h"

#if !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D))
  #include "FXparticleSystem.h"
  #ifdef ESP8266
    #if !defined(WLED_DISABLE_PARTICLESYSTEM2D) && !defined(WLED_DISABLE_PARTICLESYSTEM1D)
    #error ESP8266 does not support 1D and 2D particle systems simultaneously. Please disable one of them.
    #endif
  #endif
#else
  #define WLED_PS_DONT_REPLACE_FX
#endif

 //////////////
 // DEV INFO //
 //////////////
/*
  information for FX metadata strings: https://kno.wled.ge/interfaces/json-api/#effect-metadata

  Audio Reactive: use the following code to pass usermod variables to effect

  uint8_t  *binNum = (uint8_t*)&SEGENV.aux1, *maxVol = (uint8_t*)(&SEGENV.aux1+1); // just in case assignment
  bool      samplePeak = false;
  float     FFT_MajorPeak = 1.0;
  uint8_t  *fftResult = nullptr;
  float    *fftBin = nullptr;
  um_data_t *um_data = getAudioData();
  volumeSmth    = *(float*)   um_data->u_data[0];
  volumeRaw     = *(float*)   um_data->u_data[1];
  fftResult     =  (uint8_t*) um_data->u_data[2];
  samplePeak    = *(uint8_t*) um_data->u_data[3];
  FFT_MajorPeak = *(float*)   um_data->u_data[4];
  my_magnitude  = *(float*)   um_data->u_data[5];
  maxVol        =  (uint8_t*) um_data->u_data[6];  // requires UI element (SEGMENT.customX?), changes source element
  binNum        =  (uint8_t*) um_data->u_data[7];  // requires UI element (SEGMENT.customX?), changes source element
  fftBin        =  (float*)   um_data->u_data[8];
*/

#define IBN 5100
// paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined)
#define PALETTE_SOLID_WRAP   (strip.paletteBlend == 1 || strip.paletteBlend == 3)
#define PALETTE_MOVING_WRAP !(strip.paletteBlend == 2 || (strip.paletteBlend == 0 && SEGMENT.speed == 0))

#define indexToVStrip(index, stripNr) ((index) | (int((stripNr)+1)<<16))

// a few constants needed for AudioReactive effects
// for 22Khz sampling
#define MAX_FREQUENCY   11025    // sample frequency / 2 (as per Nyquist criterion)
#define MAX_FREQ_LOG10  4.04238f // log10(MAX_FREQUENCY)
// for 20Khz sampling
//#define MAX_FREQUENCY   10240
//#define MAX_FREQ_LOG10  4.0103f
// for 10Khz sampling
//#define MAX_FREQUENCY   5120
//#define MAX_FREQ_LOG10  3.71f

// effect utility functions
uint8_t sin_gap(uint16_t in) {
  if (in & 0x100) return 0;
  return sin8_t(in + 192); // correct phase shift of sine so that it starts and stops at 0
}

uint16_t triwave16(uint16_t in) {
  if (in < 0x8000) return in *2;
  return 0xFFFF - (in - 0x8000)*2;
}

/*
 * Generates a tristate square wave w/ attac & decay
 * @param x input value 0-255
 * @param pulsewidth 0-127
 * @param attdec attack & decay, max. pulsewidth / 2
 * @returns signed waveform value
 */
int8_t tristate_square8(uint8_t x, uint8_t pulsewidth, uint8_t attdec) {
  int8_t a = 127;
  if (x > 127) {
    a = -127;
    x -= 127;
  }

  if (x < attdec) { //inc to max
    return (int16_t) x * a / attdec;
  }
  else if (x < pulsewidth - attdec) { //max
    return a;
  }
  else if (x < pulsewidth) { //dec to 0
    return (int16_t) (pulsewidth - x) * a / attdec;
  }
  return 0;
}

static um_data_t* getAudioData() {
  um_data_t *um_data;
  if (!UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
    // add support for no audio
    um_data = simulateSound(SEGMENT.soundSim);
  }
  return um_data;
}

// effect functions

/*
 * No blinking. Just plain old static light.
 */
uint16_t mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
  return strip.isOffRefreshRequired() ? FRAMETIME : 350;
}
static const char _data_FX_MODE_STATIC[] PROGMEM = "Solid";


/*
 * Blink/strobe function
 * Alternate between color1 and color2
 * if(strobe == true) then create a strobe effect
 */
uint16_t blink(uint32_t color1, uint32_t color2, bool strobe, bool do_palette) {
  uint32_t cycleTime = (255 - SEGMENT.speed)*20;
  uint32_t onTime = FRAMETIME;
  if (!strobe) onTime += ((cycleTime * SEGMENT.intensity) >> 8);
  cycleTime += FRAMETIME*2;
  uint32_t it = strip.now / cycleTime;
  uint32_t rem = strip.now % cycleTime;

  bool on = false;
  if (it != SEGENV.step //new iteration, force on state for one frame, even if set time is too brief
      || rem <= onTime) {
    on = true;
  }

  SEGENV.step = it; //save previous iteration

  uint32_t color = on ? color1 : color2;
  if (color == color1 && do_palette)
  {
    for (unsigned i = 0; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  } else SEGMENT.fill(color);

  return FRAMETIME;
}


/*
 * Normal blinking. Intensity sets duty cycle.
 */
uint16_t mode_blink(void) {
  return blink(SEGCOLOR(0), SEGCOLOR(1), false, true);
}
static const char _data_FX_MODE_BLINK[] PROGMEM = "Blink@!,Duty cycle;!,!;!;01";


/*
 * Classic Blink effect. Cycling through the rainbow.
 */
uint16_t mode_blink_rainbow(void) {
  return blink(SEGMENT.color_wheel(SEGENV.call & 0xFF), SEGCOLOR(1), false, false);
}
static const char _data_FX_MODE_BLINK_RAINBOW[] PROGMEM = "Blink Rainbow@Frequency,Blink duration;!,!;!;01";


/*
 * Classic Strobe effect.
 */
uint16_t mode_strobe(void) {
  return blink(SEGCOLOR(0), SEGCOLOR(1), true, true);
}
static const char _data_FX_MODE_STROBE[] PROGMEM = "Strobe@!;!,!;!;01";


/*
 * Classic Strobe effect. Cycling through the rainbow.
 */
uint16_t mode_strobe_rainbow(void) {
  return blink(SEGMENT.color_wheel(SEGENV.call & 0xFF), SEGCOLOR(1), true, false);
}
static const char _data_FX_MODE_STROBE_RAINBOW[] PROGMEM = "Strobe Rainbow@!;,!;!;01";


/*
 * Color wipe function
 * LEDs are turned on (color1) in sequence, then turned off (color2) in sequence.
 * if (bool rev == true) then LEDs are turned off in reverse order
 */
uint16_t color_wipe(bool rev, bool useRandomColors) {
  if (SEGLEN <= 1) return mode_static();
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed)*150;
  uint32_t perc = strip.now % cycleTime;
  unsigned prog = (perc * 65535) / cycleTime;
  bool back = (prog > 32767);
  if (back) {
    prog -= 32767;
    if (SEGENV.step == 0) SEGENV.step = 1;
  } else {
    if (SEGENV.step == 2) SEGENV.step = 3; //trigger color change
  }

  if (useRandomColors) {
    if (SEGENV.call == 0) {
      SEGENV.aux0 = hw_random8();
      SEGENV.step = 3;
    }
    if (SEGENV.step == 1) { //if flag set, change to new random color
      SEGENV.aux1 = get_random_wheel_index(SEGENV.aux0);
      SEGENV.step = 2;
    }
    if (SEGENV.step == 3) {
      SEGENV.aux0 = get_random_wheel_index(SEGENV.aux1);
      SEGENV.step = 0;
    }
  }

  unsigned ledIndex = (prog * SEGLEN) >> 15;
  uint16_t rem = (prog * SEGLEN) * 2; //mod 0xFFFF by truncating
  rem /= (SEGMENT.intensity +1);
  if (rem > 255) rem = 255;

  uint32_t col1 = useRandomColors? SEGMENT.color_wheel(SEGENV.aux1) : SEGCOLOR(1);
  for (unsigned i = 0; i < SEGLEN; i++)
  {
    unsigned index = (rev && back)? SEGLEN -1 -i : i;
    uint32_t col0 = useRandomColors? SEGMENT.color_wheel(SEGENV.aux0) : SEGMENT.color_from_palette(index, true, PALETTE_SOLID_WRAP, 0);

    if (i < ledIndex)
    {
      SEGMENT.setPixelColor(index, back? col1 : col0);
    } else
    {
      SEGMENT.setPixelColor(index, back? col0 : col1);
      if (i == ledIndex) SEGMENT.setPixelColor(index, color_blend(back? col0 : col1, back? col1 : col0, uint8_t(rem)));
    }
  }
  return FRAMETIME;
}


/*
 * Lights all LEDs one after another.
 */
uint16_t mode_color_wipe(void) {
  return color_wipe(false, false);
}
static const char _data_FX_MODE_COLOR_WIPE[] PROGMEM = "Wipe@!,!;!,!;!";


/*
 * Lights all LEDs one after another. Turns off opposite
 */
uint16_t mode_color_sweep(void) {
  return color_wipe(true, false);
}
static const char _data_FX_MODE_COLOR_SWEEP[] PROGMEM = "Sweep@!,!;!,!;!";


/*
 * Turns all LEDs after each other to a random color.
 * Then starts over with another color.
 */
uint16_t mode_color_wipe_random(void) {
  return color_wipe(false, true);
}
static const char _data_FX_MODE_COLOR_WIPE_RANDOM[] PROGMEM = "Wipe Random@!;;!";


/*
 * Random color introduced alternating from start and end of strip.
 */
uint16_t mode_color_sweep_random(void) {
  return color_wipe(true, true);
}
static const char _data_FX_MODE_COLOR_SWEEP_RANDOM[] PROGMEM = "Sweep Random@!;;!";


/*
 * Lights all LEDs up in one random color. Then switches them
 * to the next random color.
 */
uint16_t mode_random_color(void) {
  uint32_t cycleTime = 200 + (255 - SEGMENT.speed)*50;
  uint32_t it = strip.now / cycleTime;
  uint32_t rem = strip.now % cycleTime;
  unsigned fadedur = (cycleTime * SEGMENT.intensity) >> 8;

  uint32_t fade = 255;
  if (fadedur) {
    fade = (rem * 255) / fadedur;
    if (fade > 255) fade = 255;
  }

  if (SEGENV.call == 0) {
    SEGENV.aux0 = hw_random8();
    SEGENV.step = 2;
  }
  if (it != SEGENV.step) //new color
  {
    SEGENV.aux1 = SEGENV.aux0;
    SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0); //aux0 will store our random color wheel index
    SEGENV.step = it;
  }

  SEGMENT.fill(color_blend(SEGMENT.color_wheel(SEGENV.aux1), SEGMENT.color_wheel(SEGENV.aux0), uint8_t(fade)));
  return FRAMETIME;
}
static const char _data_FX_MODE_RANDOM_COLOR[] PROGMEM = "Random Colors@!,Fade time;;!;01";


/*
 * Lights every LED in a random color. Changes all LED at the same time
 * to new random colors.
 */
uint16_t mode_dynamic(void) {
  if (!SEGENV.allocateData(SEGLEN)) return mode_static(); //allocation failed

  if(SEGENV.call == 0) {
    //SEGMENT.fill(BLACK);
    for (unsigned i = 0; i < SEGLEN; i++) SEGENV.data[i] = hw_random8();
  }

  uint32_t cycleTime = 50 + (255 - SEGMENT.speed)*15;
  uint32_t it = strip.now / cycleTime;
  if (it != SEGENV.step && SEGMENT.speed != 0) //new color
  {
    for (unsigned i = 0; i < SEGLEN; i++) {
      if (hw_random8() <= SEGMENT.intensity) SEGENV.data[i] = hw_random8(); // random color index
    }
    SEGENV.step = it;
  }

  if (SEGMENT.check1) {
    for (unsigned i = 0; i < SEGLEN; i++) {
      SEGMENT.blendPixelColor(i, SEGMENT.color_wheel(SEGENV.data[i]), 16);
    }
  } else {
    for (unsigned i = 0; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(i, SEGMENT.color_wheel(SEGENV.data[i]));
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_DYNAMIC[] PROGMEM = "Dynamic@!,!,,,,Smooth;;!";


/*
 * effect "Dynamic" with smooth color-fading
 */
uint16_t mode_dynamic_smooth(void) {
  bool old = SEGMENT.check1;
  SEGMENT.check1 = true;
  mode_dynamic();
  SEGMENT.check1 = old;
  return FRAMETIME;
 }
static const char _data_FX_MODE_DYNAMIC_SMOOTH[] PROGMEM = "Dynamic Smooth@!,!;;!";


/*
 * Does the "standby-breathing" of well known i-Devices.
 */
uint16_t mode_breath(void) {
  unsigned var = 0;
  unsigned counter = (strip.now * ((SEGMENT.speed >> 3) +10)) & 0xFFFFU;
  counter = (counter >> 2) + (counter >> 4); //0-16384 + 0-2048
  if (counter < 16384) {
    if (counter > 8192) counter = 8192 - (counter - 8192);
    var = sin16_t(counter) / 103; //close to parabolic in range 0-8192, max val. 23170
  }

  uint8_t lum = 30 + var;
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), lum));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_BREATH[] PROGMEM = "Breathe@!;!,!;!;01";


/*
 * Fades the LEDs between two colors
 */
uint16_t mode_fade(void) {
  unsigned counter = (strip.now * ((SEGMENT.speed >> 3) +10));
  uint8_t lum = triwave16(counter) >> 8;

  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), lum));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_FADE[] PROGMEM = "Fade@!;!,!;!;01";


/*
 * Scan mode parent function
 */
uint16_t scan(bool dual) {
  if (SEGLEN <= 1) return mode_static();
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed)*150;
  uint32_t perc = strip.now % cycleTime;
  int prog = (perc * 65535) / cycleTime;
  int size = 1 + ((SEGMENT.intensity * SEGLEN) >> 9);
  int ledIndex = (prog * ((SEGLEN *2) - size *2)) >> 16;

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  int led_offset = ledIndex - (SEGLEN - size);
  led_offset = abs(led_offset);

  if (dual) {
    for (int j = led_offset; j < led_offset + size; j++) {
      unsigned i2 = SEGLEN -1 -j;
      SEGMENT.setPixelColor(i2, SEGMENT.color_from_palette(i2, true, PALETTE_SOLID_WRAP, (SEGCOLOR(2))? 2:0));
    }
  }

  for (int j = led_offset; j < led_offset + size; j++) {
    SEGMENT.setPixelColor(j, SEGMENT.color_from_palette(j, true, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
}


/*
 * Runs a single pixel back and forth.
 */
uint16_t mode_scan(void) {
  return scan(false);
}
static const char _data_FX_MODE_SCAN[] PROGMEM = "Scan@!,# of dots,,,,,Overlay;!,!,!;!";


/*
 * Runs two pixel back and forth in opposite directions.
 */
uint16_t mode_dual_scan(void) {
  return scan(true);
}
static const char _data_FX_MODE_DUAL_SCAN[] PROGMEM = "Scan Dual@!,# of dots,,,,,Overlay;!,!,!;!";


/*
 * Cycles all LEDs at once through a rainbow.
 */
uint16_t mode_rainbow(void) {
  unsigned counter = (strip.now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  if (SEGMENT.intensity < 128){
    SEGMENT.fill(color_blend(SEGMENT.color_wheel(counter),WHITE,uint8_t(128-SEGMENT.intensity)));
  } else {
    SEGMENT.fill(SEGMENT.color_wheel(counter));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_RAINBOW[] PROGMEM = "Colorloop@!,Saturation;;!;01";


/*
 * Cycles a rainbow over the entire string of LEDs.
 */
uint16_t mode_rainbow_cycle(void) {
  unsigned counter = (strip.now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  for (unsigned i = 0; i < SEGLEN; i++) {
    //intensity/29 = 0 (1/16) 1 (1/8) 2 (1/4) 3 (1/2) 4 (1) 5 (2) 6 (4) 7 (8) 8 (16)
    uint8_t index = (i * (16 << (SEGMENT.intensity /29)) / SEGLEN) + counter;
    SEGMENT.setPixelColor(i, SEGMENT.color_wheel(index));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_RAINBOW_CYCLE[] PROGMEM = "Rainbow@!,Size;;!";


/*
 * Alternating pixels running function.
 */
static uint16_t running(uint32_t color1, uint32_t color2, bool theatre = false) {
  int width = (theatre ? 3 : 1) + (SEGMENT.intensity >> 4);  // window
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed);
  uint32_t it = strip.now / cycleTime;
  bool usePalette = color1 == SEGCOLOR(0);

  for (unsigned i = 0; i < SEGLEN; i++) {
    uint32_t col = color2;
    if (usePalette) color1 = SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0);
    if (theatre) {
      if ((i % width) == SEGENV.aux0) col = color1;
    } else {
      int pos = (i % (width<<1));
      if ((pos < SEGENV.aux0-width) || ((pos >= SEGENV.aux0) && (pos < SEGENV.aux0+width))) col = color1;
    }
    SEGMENT.setPixelColor(i,col);
  }

  if (it != SEGENV.step) {
    SEGENV.aux0 = (SEGENV.aux0 +1) % (theatre ? width : (width<<1));
    SEGENV.step = it;
  }
  return FRAMETIME;
}


/*
 * Theatre-style crawling lights.
 * Inspired by the Adafruit examples.
 */
uint16_t mode_theater_chase(void) {
  return running(SEGCOLOR(0), SEGCOLOR(1), true);
}
static const char _data_FX_MODE_THEATER_CHASE[] PROGMEM = "Theater@!,Gap size;!,!;!";


/*
 * Theatre-style crawling lights with rainbow effect.
 * Inspired by the Adafruit examples.
 */
uint16_t mode_theater_chase_rainbow(void) {
  return running(SEGMENT.color_wheel(SEGENV.step), SEGCOLOR(1), true);
}
static const char _data_FX_MODE_THEATER_CHASE_RAINBOW[] PROGMEM = "Theater Rainbow@!,Gap size;,!;!";


/*
 * Running lights effect with smooth sine transition base.
 */
static uint16_t running_base(bool saw, bool dual=false) {
  unsigned x_scale = SEGMENT.intensity >> 2;
  uint32_t counter = (strip.now * SEGMENT.speed) >> 9;

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned a = i*x_scale - counter;
    if (saw) {
      a &= 0xFF;
      if (a < 16)
      {
        a = 192 + a*8;
      } else {
        a = map(a,16,255,64,192);
      }
      a = 255 - a;
    }
    uint8_t s = dual ? sin_gap(a) : sin8_t(a);
    uint32_t ca = color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), s);
    if (dual) {
      unsigned b = (SEGLEN-1-i)*x_scale - counter;
      uint8_t t = sin_gap(b);
      uint32_t cb = color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), t);
      ca = color_blend(ca, cb, uint8_t(127));
    }
    SEGMENT.setPixelColor(i, ca);
  }

  return FRAMETIME;
}


/*
 * Running lights in opposite directions.
 * Idea: Make the gap width controllable with a third slider in the future
 */
uint16_t mode_running_dual(void) {
  return running_base(false, true);
}
static const char _data_FX_MODE_RUNNING_DUAL[] PROGMEM = "Running Dual@!,Wave width;L,!,R;!";


/*
 * Running lights effect with smooth sine transition.
 */
uint16_t mode_running_lights(void) {
  return running_base(false);
}
static const char _data_FX_MODE_RUNNING_LIGHTS[] PROGMEM = "Running@!,Wave width;!,!;!";


/*
 * Running lights effect with sawtooth transition.
 */
uint16_t mode_saw(void) {
  return running_base(true);
}
static const char _data_FX_MODE_SAW[] PROGMEM = "Saw@!,Width;!,!;!";


/*
 * Blink several LEDs in random colors on, reset, repeat.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t mode_twinkle(void) {
  SEGMENT.fade_out(224);

  uint32_t cycleTime = 20 + (255 - SEGMENT.speed)*5;
  uint32_t it = strip.now / cycleTime;
  if (it != SEGENV.step)
  {
    unsigned maxOn = map(SEGMENT.intensity, 0, 255, 1, SEGLEN); // make sure at least one LED is on
    if (SEGENV.aux0 >= maxOn)
    {
      SEGENV.aux0 = 0;
      SEGENV.aux1 = hw_random(); //new seed for our PRNG
    }
    SEGENV.aux0++;
    SEGENV.step = it;
  }

  unsigned PRNG16 = SEGENV.aux1;

  for (unsigned i = 0; i < SEGENV.aux0; i++)
  {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 13849; // next 'random' number
    uint32_t p = (uint32_t)SEGLEN * (uint32_t)PRNG16;
    unsigned j = p >> 16;
    SEGMENT.setPixelColor(j, SEGMENT.color_from_palette(j, true, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_TWINKLE[] PROGMEM = "Twinkle@!,!;!,!;!;;m12=0"; //pixels


/*
 * Dissolve function
 */
uint16_t dissolve(uint32_t color) {
  unsigned dataSize = sizeof(uint32_t) * SEGLEN;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  uint32_t* pixels = reinterpret_cast<uint32_t*>(SEGENV.data);

  if (SEGENV.call == 0) {
    for (unsigned i = 0; i < SEGLEN; i++) pixels[i] = SEGCOLOR(1);
    SEGENV.aux0 = 1;
  }

  for (unsigned j = 0; j <= SEGLEN / 15; j++) {
    if (hw_random8() <= SEGMENT.intensity) {
      for (size_t times = 0; times < 10; times++) { //attempt to spawn a new pixel 10 times
        unsigned i = hw_random16(SEGLEN);
        if (SEGENV.aux0) { //dissolve to primary/palette
          if (pixels[i] == SEGCOLOR(1)) {
            pixels[i] = color == SEGCOLOR(0) ? SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0) : color;
            break; //only spawn 1 new pixel per frame per 50 LEDs
          }
        } else { //dissolve to secondary
          if (pixels[i] != SEGCOLOR(1)) {
            pixels[i] = SEGCOLOR(1);
            break;
          }
        }
      }
    }
  }
  // fix for #4401
  for (unsigned i = 0; i < SEGLEN; i++) SEGMENT.setPixelColor(i, pixels[i]);

  if (SEGENV.step > (255 - SEGMENT.speed) + 15U) {
    SEGENV.aux0 = !SEGENV.aux0;
    SEGENV.step = 0;
  } else {
    SEGENV.step++;
  }

  return FRAMETIME;
}


/*
 * Blink several LEDs on and then off
 */
uint16_t mode_dissolve(void) {
  return dissolve(SEGMENT.check1 ? SEGMENT.color_wheel(hw_random8()) : SEGCOLOR(0));
}
static const char _data_FX_MODE_DISSOLVE[] PROGMEM = "Dissolve@Repeat speed,Dissolve speed,,,,Random;!,!;!";


/*
 * Blink several LEDs on and then off in random colors
 */
uint16_t mode_dissolve_random(void) {
  return dissolve(SEGMENT.color_wheel(hw_random8()));
}
static const char _data_FX_MODE_DISSOLVE_RANDOM[] PROGMEM = "Dissolve Rnd@Repeat speed,Dissolve speed;,!;!";


/*
 * Blinks one LED at a time.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t mode_sparkle(void) {
  if (!SEGMENT.check2) for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }
  uint32_t cycleTime = 10 + (255 - SEGMENT.speed)*2;
  uint32_t it = strip.now / cycleTime;
  if (it != SEGENV.step)
  {
    SEGENV.aux0 = hw_random16(SEGLEN); // aux0 stores the random led index
    SEGENV.step = it;
  }

  SEGMENT.setPixelColor(SEGENV.aux0, SEGCOLOR(0));
  return FRAMETIME;
}
static const char _data_FX_MODE_SPARKLE[] PROGMEM = "Sparkle@!,,,,,,Overlay;!,!;!;;m12=0";


/*
 * Lights all LEDs in the color. Flashes single col 1 pixels randomly. (List name: Sparkle Dark)
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t mode_flash_sparkle(void) {
  if (!SEGMENT.check2) for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
  }

  if (strip.now - SEGENV.aux0 > SEGENV.step) {
    if(hw_random8((255-SEGMENT.intensity) >> 4) == 0) {
      SEGMENT.setPixelColor(hw_random16(SEGLEN), SEGCOLOR(1)); //flash
    }
    SEGENV.step = strip.now;
    SEGENV.aux0 = 255-SEGMENT.speed;
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_FLASH_SPARKLE[] PROGMEM = "Sparkle Dark@!,!,,,,,Overlay;Bg,Fx;!;;m12=0";


/*
 * Like flash sparkle. With more flash.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t mode_hyper_sparkle(void) {
  if (!SEGMENT.check2) for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
  }

  if (strip.now - SEGENV.aux0 > SEGENV.step) {
    if (hw_random8((255-SEGMENT.intensity) >> 4) == 0) {
      int len = max(1, (int)SEGLEN/3);
      for (int i = 0; i < len; i++) {
        SEGMENT.setPixelColor(hw_random16(SEGLEN), SEGCOLOR(1));
      }
    }
    SEGENV.step = strip.now;
    SEGENV.aux0 = 255-SEGMENT.speed;
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_HYPER_SPARKLE[] PROGMEM = "Sparkle+@!,!,,,,,Overlay;Bg,Fx;!;;m12=0";


/*
 * Strobe effect with different strobe count and pause, controlled by speed.
 */
uint16_t mode_multi_strobe(void) {
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }

  SEGENV.aux0 = 50 + 20*(uint16_t)(255-SEGMENT.speed);
  unsigned count = 2 * ((SEGMENT.intensity / 10) + 1);
  if(SEGENV.aux1 < count) {
    if((SEGENV.aux1 & 1) == 0) {
      SEGMENT.fill(SEGCOLOR(0));
      SEGENV.aux0 = 15;
    } else {
      SEGENV.aux0 = 50;
    }
  }

  if (strip.now - SEGENV.aux0 > SEGENV.step) {
    SEGENV.aux1++;
    if (SEGENV.aux1 > count) SEGENV.aux1 = 0;
    SEGENV.step = strip.now;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_MULTI_STROBE[] PROGMEM = "Strobe Mega@!,!;!,!;!;01";


/*
 * Android loading circle, refactored by @dedehai
 */
uint16_t mode_android(void) {
  if (!SEGENV.allocateData(sizeof(uint32_t))) return mode_static();
  uint32_t* counter = reinterpret_cast<uint32_t*>(SEGENV.data);
  unsigned size = SEGENV.aux1 >> 1; // upper 15 bit
  unsigned shrinking = SEGENV.aux1 & 0x01; // lowest bit
  if(strip.now >= SEGENV.step) {
    SEGENV.step = strip.now + 3 + ((8 * (uint32_t)(255 - SEGMENT.speed)) / SEGLEN);
    if (size > (SEGMENT.intensity * SEGLEN) / 255)
      shrinking = 1;
    else if (size < 2)
      shrinking = 0;
    if (!shrinking) { // growing
      if ((*counter % 3) == 1)
        SEGENV.aux0++; // advance start position
      else
        size++;
    } else { // shrinking
      SEGENV.aux0++;
      if ((*counter % 3) != 1)
        size--;
    }
    SEGENV.aux1 = size << 1 | shrinking; // save back
    (*counter)++;
    if (SEGENV.aux0 >= SEGLEN) SEGENV.aux0 = 0;
  }
  uint32_t start = SEGENV.aux0;
  uint32_t end = (SEGENV.aux0 + size) % SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    if ((start < end && i >= start && i < end) || (start >= end && (i >= start || i < end)))
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    else
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_ANDROID[] PROGMEM = "Android@!,Width;!,!;!;;m12=1"; //vertical

/*
 * color chase function.
 * color1 = background color
 * color2 and color3 = colors of two adjacent leds
 */
static uint16_t chase(uint32_t color1, uint32_t color2, uint32_t color3, bool do_palette) {
  uint16_t counter = strip.now * ((SEGMENT.speed >> 2) + 1);
  uint16_t a = (counter * SEGLEN) >> 16;

  bool chase_random = (SEGMENT.mode == FX_MODE_CHASE_RANDOM);
  if (chase_random) {
    if (a < SEGENV.step) //we hit the start again, choose new color for Chase random
    {
      SEGENV.aux1 = SEGENV.aux0; //store previous random color
      SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0);
    }
    color1 = SEGMENT.color_wheel(SEGENV.aux0);
  }
  SEGENV.step = a;

  // Use intensity setting to vary chase up to 1/2 string length
  unsigned size = 1 + ((SEGMENT.intensity * SEGLEN) >> 10);

  uint16_t b = a + size; //"trail" of chase, filled with color1
  if (b > SEGLEN) b -= SEGLEN;
  uint16_t c = b + size;
  if (c > SEGLEN) c -= SEGLEN;

  //background
  if (do_palette)
  {
    for (unsigned i = 0; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
    }
  } else SEGMENT.fill(color1);

  //if random, fill old background between a and end
  if (chase_random)
  {
    color1 = SEGMENT.color_wheel(SEGENV.aux1);
    for (unsigned i = a; i < SEGLEN; i++)
      SEGMENT.setPixelColor(i, color1);
  }

  //fill between points a and b with color2
  if (a < b)
  {
    for (unsigned i = a; i < b; i++)
      SEGMENT.setPixelColor(i, color2);
  } else {
    for (unsigned i = a; i < SEGLEN; i++) //fill until end
      SEGMENT.setPixelColor(i, color2);
    for (unsigned i = 0; i < b; i++) //fill from start until b
      SEGMENT.setPixelColor(i, color2);
  }

  //fill between points b and c with color2
  if (b < c)
  {
    for (unsigned i = b; i < c; i++)
      SEGMENT.setPixelColor(i, color3);
  } else {
    for (unsigned i = b; i < SEGLEN; i++) //fill until end
      SEGMENT.setPixelColor(i, color3);
    for (unsigned i = 0; i < c; i++) //fill from start until c
      SEGMENT.setPixelColor(i, color3);
  }

  return FRAMETIME;
}


/*
 * Bicolor chase, more primary color.
 */
uint16_t mode_chase_color(void) {
  return chase(SEGCOLOR(1), (SEGCOLOR(2)) ? SEGCOLOR(2) : SEGCOLOR(0), SEGCOLOR(0), true);
}
static const char _data_FX_MODE_CHASE_COLOR[] PROGMEM = "Chase@!,Width;!,!,!;!";


/*
 * Primary running followed by random color.
 */
uint16_t mode_chase_random(void) {
  return chase(SEGCOLOR(1), (SEGCOLOR(2)) ? SEGCOLOR(2) : SEGCOLOR(0), SEGCOLOR(0), false);
}
static const char _data_FX_MODE_CHASE_RANDOM[] PROGMEM = "Chase Random@!,Width;!,,!;!";


/*
 * Primary, secondary running on rainbow.
 */
uint16_t mode_chase_rainbow(void) {
  unsigned color_sep = 256 / SEGLEN;
  if (color_sep == 0) color_sep = 1;                                           // correction for segments longer than 256 LEDs
  unsigned color_index = SEGENV.call & 0xFF;
  uint32_t color = SEGMENT.color_wheel(((SEGENV.step * color_sep) + color_index) & 0xFF);

  return chase(color, SEGCOLOR(0), SEGCOLOR(1), false);
}
static const char _data_FX_MODE_CHASE_RAINBOW[] PROGMEM = "Chase Rainbow@!,Width;!,!;!";


/*
 * Primary running on rainbow.
 */
uint16_t mode_chase_rainbow_white(void) {
  uint16_t n = SEGENV.step;
  uint16_t m = (SEGENV.step + 1) % SEGLEN;
  uint32_t color2 = SEGMENT.color_wheel(((n * 256 / SEGLEN) + (SEGENV.call & 0xFF)) & 0xFF);
  uint32_t color3 = SEGMENT.color_wheel(((m * 256 / SEGLEN) + (SEGENV.call & 0xFF)) & 0xFF);

  return chase(SEGCOLOR(0), color2, color3, false);
}
static const char _data_FX_MODE_CHASE_RAINBOW_WHITE[] PROGMEM = "Rainbow Runner@!,Size;Bg;!";


/*
 * Red - Amber - Green - Blue lights running
 */
uint16_t mode_colorful(void) {
  unsigned numColors = 4; //3, 4, or 5
  uint32_t cols[9]{0x00FF0000,0x00EEBB00,0x0000EE00,0x000077CC};
  if (SEGMENT.intensity > 160 || SEGMENT.palette) { //palette or color
    if (!SEGMENT.palette) {
      numColors = 3;
      for (size_t i = 0; i < 3; i++) cols[i] = SEGCOLOR(i);
    } else {
      unsigned fac = 80;
      if (SEGMENT.palette == 52) {numColors = 5; fac = 61;} //C9 2 has 5 colors
      for (size_t i = 0; i < numColors; i++) {
        cols[i] = SEGMENT.color_from_palette(i*fac, false, true, 255);
      }
    }
  } else if (SEGMENT.intensity < 80) //pastel (easter) colors
  {
    cols[0] = 0x00FF8040;
    cols[1] = 0x00E5D241;
    cols[2] = 0x0077FF77;
    cols[3] = 0x0077F0F0;
  }
  for (size_t i = numColors; i < numColors*2 -1U; i++) cols[i] = cols[i-numColors];

  uint32_t cycleTime = 50 + (8 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = strip.now / cycleTime;
  if (it != SEGENV.step)
  {
    if (SEGMENT.speed > 0) SEGENV.aux0++;
    if (SEGENV.aux0 >= numColors) SEGENV.aux0 = 0;
    SEGENV.step = it;
  }

  for (unsigned i = 0; i < SEGLEN; i+= numColors)
  {
    for (unsigned j = 0; j < numColors; j++) SEGMENT.setPixelColor(i + j, cols[SEGENV.aux0 + j]);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_COLORFUL[] PROGMEM = "Colorful@!,Saturation;1,2,3;!";


/*
 * Emulates a traffic light.
 */
uint16_t mode_traffic_light(void) {
  if (SEGLEN <= 1) return mode_static();
  for (unsigned i=0; i < SEGLEN; i++)
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  uint32_t mdelay = 500;
  for (unsigned i = 0; i < SEGLEN-2 ; i+=3)
  {
    switch (SEGENV.aux0)
    {
      case 0: SEGMENT.setPixelColor(i, 0x00FF0000); mdelay = 150 + (100 * (uint32_t)(255 - SEGMENT.speed));break;
      case 1: SEGMENT.setPixelColor(i, 0x00FF0000); mdelay = 150 + (20 * (uint32_t)(255 - SEGMENT.speed)); SEGMENT.setPixelColor(i+1, 0x00EECC00); break;
      case 2: SEGMENT.setPixelColor(i+2, 0x0000FF00); mdelay = 150 + (100 * (uint32_t)(255 - SEGMENT.speed));break;
      case 3: SEGMENT.setPixelColor(i+1, 0x00EECC00); mdelay = 150 + (20 * (uint32_t)(255 - SEGMENT.speed));break;
    }
  }

  if (strip.now - SEGENV.step > mdelay)
  {
    SEGENV.aux0++;
    if (SEGENV.aux0 == 1 && SEGMENT.intensity > 140) SEGENV.aux0 = 2; //skip Red + Amber, to get US-style sequence
    if (SEGENV.aux0 > 3) SEGENV.aux0 = 0;
    SEGENV.step = strip.now;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_TRAFFIC_LIGHT[] PROGMEM = "Traffic Light@!,US style;,!;!";


/*
 * Sec flashes running on prim.
 */
#define FLASH_COUNT 4
uint16_t mode_chase_flash(void) {
  if (SEGLEN <= 1) return mode_static();
  unsigned flash_step = SEGENV.call % ((FLASH_COUNT * 2) + 1);

  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
  }

  unsigned delay = 10 + ((30 * (uint16_t)(255 - SEGMENT.speed)) / SEGLEN);
  if(flash_step < (FLASH_COUNT * 2)) {
    if(flash_step % 2 == 0) {
      unsigned n = SEGENV.step;
      unsigned m = (SEGENV.step + 1) % SEGLEN;
      SEGMENT.setPixelColor( n, SEGCOLOR(1));
      SEGMENT.setPixelColor( m, SEGCOLOR(1));
      delay = 20;
    } else {
      delay = 30;
    }
  } else {
    SEGENV.step = (SEGENV.step + 1) % SEGLEN;
  }
  return delay;
}
static const char _data_FX_MODE_CHASE_FLASH[] PROGMEM = "Chase Flash@!;Bg,Fx;!";


/*
 * Prim flashes running, followed by random color.
 */
uint16_t mode_chase_flash_random(void) {
  if (SEGLEN <= 1) return mode_static();
  unsigned flash_step = SEGENV.call % ((FLASH_COUNT * 2) + 1);

  for (int i = 0; i < SEGENV.aux1; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_wheel(SEGENV.aux0));
  }

  unsigned delay = 1 + ((10 * (uint16_t)(255 - SEGMENT.speed)) / SEGLEN);
  if(flash_step < (FLASH_COUNT * 2)) {
    unsigned n = SEGENV.aux1;
    unsigned m = (SEGENV.aux1 + 1) % SEGLEN;
    if(flash_step % 2 == 0) {
      SEGMENT.setPixelColor( n, SEGCOLOR(0));
      SEGMENT.setPixelColor( m, SEGCOLOR(0));
      delay = 20;
    } else {
      SEGMENT.setPixelColor( n, SEGMENT.color_wheel(SEGENV.aux0));
      SEGMENT.setPixelColor( m, SEGCOLOR(1));
      delay = 30;
    }
  } else {
    SEGENV.aux1 = (SEGENV.aux1 + 1) % SEGLEN;

    if (SEGENV.aux1 == 0) {
      SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0);
    }
  }
  return delay;
}
static const char _data_FX_MODE_CHASE_FLASH_RANDOM[] PROGMEM = "Chase Flash Rnd@!;!,!;!";


/*
 * Alternating color/sec pixels running.
 */
uint16_t mode_running_color(void) {
  return running(SEGCOLOR(0), SEGCOLOR(1));
}
static const char _data_FX_MODE_RUNNING_COLOR[] PROGMEM = "Chase 2@!,Width;!,!;!";


/*
 * Random colored pixels running. ("Stream")
 */
uint16_t mode_running_random(void) {
  uint32_t cycleTime = 25 + (3 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = strip.now / cycleTime;
  if (SEGENV.call == 0) SEGENV.aux0 = hw_random(); // random seed for PRNG on start

  unsigned zoneSize = ((255-SEGMENT.intensity) >> 4) +1;
  uint16_t PRNG16 = SEGENV.aux0;

  unsigned z = it % zoneSize;
  bool nzone = (!z && it != SEGENV.aux1);
  for (int i=SEGLEN-1; i >= 0; i--) {
    if (nzone || z >= zoneSize) {
      unsigned lastrand = PRNG16 >> 8;
      int16_t diff = 0;
      while (abs(diff) < 42) { // make sure the difference between adjacent colors is big enough
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 13849; // next zone, next 'random' number
        diff = (PRNG16 >> 8) - lastrand;
      }
      if (nzone) {
        SEGENV.aux0 = PRNG16; // save next starting seed
        nzone = false;
      }
      z = 0;
    }
    SEGMENT.setPixelColor(i, SEGMENT.color_wheel(PRNG16 >> 8));
    z++;
  }

  SEGENV.aux1 = it;
  return FRAMETIME;
}
static const char _data_FX_MODE_RUNNING_RANDOM[] PROGMEM = "Stream@!,Zone size;;!";


/*
 * K.I.T.T.
 */
uint16_t mode_larson_scanner(void) {
  if (SEGLEN <= 1) return mode_static();

  const unsigned speed  = FRAMETIME * map(SEGMENT.speed, 0, 255, 96, 2); // map into useful range
  const unsigned pixels = SEGLEN / speed; // how many pixels to advance per frame

  SEGMENT.fade_out(255-SEGMENT.intensity);

  if (SEGENV.step > strip.now) return FRAMETIME;  // we have a pause

  unsigned index = SEGENV.aux1 + pixels;
  // are we slow enough to use frames per pixel?
  if (pixels == 0) {
    const unsigned frames = speed / SEGLEN; // how many frames per 1 pixel
    if (SEGENV.step++ < frames) return FRAMETIME;
    SEGENV.step = 0;
    index++;
  }

  if (index > SEGLEN) {

    SEGENV.aux0 = !SEGENV.aux0; // change direction
    SEGENV.aux1 = 0;            // reset position
    // set delay
    if (SEGENV.aux0 || SEGMENT.check2) SEGENV.step = strip.now + SEGMENT.custom1 * 25; // multiply by 25ms
    else SEGENV.step = 0;

  } else {

    // paint as many pixels as needed
    for (unsigned i = SEGENV.aux1; i < index; i++) {
      unsigned j = (SEGENV.aux0) ? i : SEGLEN - 1 - i;
      uint32_t c = SEGMENT.color_from_palette(j, true, PALETTE_SOLID_WRAP, 0);
      SEGMENT.setPixelColor(j, c);
      if (SEGMENT.check1) {
        SEGMENT.setPixelColor(SEGLEN - 1 - j, SEGCOLOR(2) ? SEGCOLOR(2) : c);
      }
    }
    SEGENV.aux1 = index;
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_LARSON_SCANNER[] PROGMEM = "Scanner@!,Trail,Delay,,,Dual,Bi-delay;!,!,!;!;;m12=0,c1=0";

/*
 * Creates two Larson scanners moving in opposite directions
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/DualLarson.h
 */
uint16_t mode_dual_larson_scanner(void){
  SEGMENT.check1 = true;
  return mode_larson_scanner();
}
static const char _data_FX_MODE_DUAL_LARSON_SCANNER[] PROGMEM = "Scanner Dual@!,Trail,Delay,,,Dual,Bi-delay;!,!,!;!;;m12=0,c1=0";

/*
 * Firing comets from one end. "Lighthouse"
 */
uint16_t mode_comet(void) {
  if (SEGLEN <= 1) return mode_static();
  unsigned counter = (strip.now * ((SEGMENT.speed >>2) +1)) & 0xFFFF;
  unsigned index = (counter * SEGLEN) >> 16;
  if (SEGENV.call == 0) SEGENV.aux0 = index;

  SEGMENT.fade_out(SEGMENT.intensity);

  SEGMENT.setPixelColor( index, SEGMENT.color_from_palette(index, true, PALETTE_SOLID_WRAP, 0));
  if (index > SEGENV.aux0) {
    for (unsigned i = SEGENV.aux0; i < index ; i++) {
       SEGMENT.setPixelColor( i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  } else if (index < SEGENV.aux0 && index < 10) {
    for (unsigned i = 0; i < index ; i++) {
       SEGMENT.setPixelColor( i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  }
  SEGENV.aux0 = index++;

  return FRAMETIME;
}
static const char _data_FX_MODE_COMET[] PROGMEM = "Lighthouse@!,Fade rate;!,!;!";

/*
 * Fireworks function.
 */
uint16_t mode_fireworks() {
  if (SEGLEN <= 1) return mode_static();
  const uint16_t width  = SEGMENT.is2D() ? SEG_W : SEGLEN;
  const uint16_t height = SEG_H;

  if (SEGENV.call == 0) {
    SEGENV.aux0 = UINT16_MAX;
    SEGENV.aux1 = UINT16_MAX;
  }
  SEGMENT.fade_out(128);

  uint8_t x = SEGENV.aux0%width, y = SEGENV.aux0/width; // 2D coordinates stored in upper and lower byte
  if (!SEGENV.step) {
    // fireworks mode (blur flares)
    bool valid1 = (SEGENV.aux0 < width*height);
    bool valid2 = (SEGENV.aux1 < width*height);
    uint32_t sv1 = 0, sv2 = 0;
    if (valid1) sv1 = SEGMENT.is2D() ? SEGMENT.getPixelColorXY(x, y) : SEGMENT.getPixelColor(SEGENV.aux0); // get spark color
    if (valid2) sv2 = SEGMENT.is2D() ? SEGMENT.getPixelColorXY(x, y) : SEGMENT.getPixelColor(SEGENV.aux1);
    SEGMENT.blur(16); // used in mode_rain()
    if (valid1) { if (SEGMENT.is2D()) SEGMENT.setPixelColorXY(x, y, sv1); else SEGMENT.setPixelColor(SEGENV.aux0, sv1); } // restore spark color after blur
    if (valid2) { if (SEGMENT.is2D()) SEGMENT.setPixelColorXY(x, y, sv2); else SEGMENT.setPixelColor(SEGENV.aux1, sv2); } // restore old spark color after blur
  }

  for (int i=0; i<max(1, width/20); i++) {
    if (hw_random8(129 - (SEGMENT.intensity >> 1)) == 0) {
      uint16_t index = hw_random16(width*height);
      x = index % width;
      y = index / width;
      uint32_t col = SEGMENT.color_from_palette(hw_random8(), false, false, 0);
      if (SEGMENT.is2D()) SEGMENT.setPixelColorXY(x, y, col);
      else                SEGMENT.setPixelColor(index, col);
      SEGENV.aux1 = SEGENV.aux0;  // old spark
      SEGENV.aux0 = index;        // remember where spark occurred
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_FIREWORKS[] PROGMEM = "Fireworks@,Frequency;!,!;!;12;ix=192,pal=11";

//Twinkling LEDs running. Inspired by https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Rain.h
uint16_t mode_rain() {
  if (SEGLEN <= 1) return mode_static();
  const unsigned width  = SEG_W;
  const unsigned height = SEG_H;
  SEGENV.step += FRAMETIME;
  if (SEGENV.call && SEGENV.step > SPEED_FORMULA_L) {
    SEGENV.step = 1;
    if (SEGMENT.is2D()) {
      //uint32_t ctemp[width];
      //for (int i = 0; i<width; i++) ctemp[i] = SEGMENT.getPixelColorXY(i, height-1);
      SEGMENT.move(6, 1, true);  // move all pixels down
      //for (int i = 0; i<width; i++) SEGMENT.setPixelColorXY(i, 0, ctemp[i]); // wrap around
      SEGENV.aux0 = (SEGENV.aux0 % width) + (SEGENV.aux0 / width + 1) * width;
      SEGENV.aux1 = (SEGENV.aux1 % width) + (SEGENV.aux1 / width + 1) * width;
    } else {
      //shift all leds left
      uint32_t ctemp = SEGMENT.getPixelColor(0);
      for (unsigned i = 0; i < SEGLEN - 1; i++) {
        SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i+1));
      }
      SEGMENT.setPixelColor(SEGLEN -1, ctemp); // wrap around
      SEGENV.aux0++;  // increase spark index
      SEGENV.aux1++;
    }
    if (SEGENV.aux0 == 0) SEGENV.aux0 = UINT16_MAX; // reset previous spark position
    if (SEGENV.aux1 == 0) SEGENV.aux0 = UINT16_MAX; // reset previous spark position
    if (SEGENV.aux0 >= width*height) SEGENV.aux0 = 0;     // ignore
    if (SEGENV.aux1 >= width*height) SEGENV.aux1 = 0;
  }
  return mode_fireworks();
}
static const char _data_FX_MODE_RAIN[] PROGMEM = "Rain@!,Spawning rate;!,!;!;12;ix=128,pal=0";

/*
 * Fire flicker function
 */
uint16_t mode_fire_flicker(void) {
  uint32_t cycleTime = 40 + (255 - SEGMENT.speed);
  uint32_t it = strip.now / cycleTime;
  if (SEGENV.step == it) return FRAMETIME;

  byte w = (SEGCOLOR(0) >> 24);
  byte r = (SEGCOLOR(0) >> 16);
  byte g = (SEGCOLOR(0) >>  8);
  byte b = (SEGCOLOR(0)      );
  byte lum = (SEGMENT.palette == 0) ? MAX(w, MAX(r, MAX(g, b))) : 255;
  lum /= (((256-SEGMENT.intensity)/16)+1);
  for (unsigned i = 0; i < SEGLEN; i++) {
    byte flicker = hw_random8(lum);
    if (SEGMENT.palette == 0) {
      SEGMENT.setPixelColor(i, MAX(r - flicker, 0), MAX(g - flicker, 0), MAX(b - flicker, 0), MAX(w - flicker, 0));
    } else {
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0, 255 - flicker));
    }
  }

  SEGENV.step = it;
  return FRAMETIME;
}
static const char _data_FX_MODE_FIRE_FLICKER[] PROGMEM = "Fire Flicker@!,!;!;!;01";


/*
 * Gradient run base function
 */
uint16_t gradient_base(bool loading) {
  if (SEGLEN <= 1) return mode_static();
  uint16_t counter = strip.now * ((SEGMENT.speed >> 2) + 1);
  uint16_t pp = (counter * SEGLEN) >> 16;
  if (SEGENV.call == 0) pp = 0;
  int val; //0 = sec 1 = pri
  int brd = 1 + loading ? SEGMENT.intensity/2 : SEGMENT.intensity/4;
  //if (brd < 1) brd = 1;
  int p1 = pp-SEGLEN;
  int p2 = pp+SEGLEN;

  for (int i = 0; i < (int)SEGLEN; i++) {
    if (loading) {
      val = abs(((i>pp) ? p2:pp) - i);
    } else {
      val = min(abs(pp-i),min(abs(p1-i),abs(p2-i)));
    }
    val = (brd > val) ? (val * 255) / brd : 255;
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(0), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1), uint8_t(val)));
  }

  return FRAMETIME;
}


/*
 * Gradient run
 */
uint16_t mode_gradient(void) {
  return gradient_base(false);
}
static const char _data_FX_MODE_GRADIENT[] PROGMEM = "Gradient@!,Spread;!,!;!;;ix=16";


/*
 * Gradient run with hard transition
 */
uint16_t mode_loading(void) {
  return gradient_base(true);
}
static const char _data_FX_MODE_LOADING[] PROGMEM = "Loading@!,Fade;!,!;!;;ix=16";

/*
 * Two dots running
 */
uint16_t mode_two_dots() {
 if (SEGLEN <= 1) return mode_static();
  unsigned delay = 1 + (FRAMETIME<<3) / SEGLEN;  // longer segments should change faster
  uint32_t it = strip.now / map(SEGMENT.speed, 0, 255, delay<<4, delay);
  unsigned offset = it % SEGLEN;
  unsigned width = ((SEGLEN*(SEGMENT.intensity+1))>>9); //max width is half the strip
  if (!width) width = 1;
  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(2));
  const uint32_t color1 = SEGCOLOR(0);
  const uint32_t color2 = (SEGCOLOR(1) == SEGCOLOR(2)) ? color1 : SEGCOLOR(1);
  for (unsigned i = 0; i < width; i++) {
    unsigned indexR = (offset + i) % SEGLEN;
    unsigned indexB = (offset + i + (SEGLEN>>1)) % SEGLEN;
    SEGMENT.setPixelColor(indexR, color1);
    SEGMENT.setPixelColor(indexB, color2);
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_TWO_DOTS[] PROGMEM = "Two Dots@!,Dot size,,,,,Overlay;1,2,Bg;!";


/*
 * Fairy, inspired by https://www.youtube.com/watch?v=zeOw5MZWq24
 */
//4 bytes
typedef struct Flasher {
  uint16_t stateStart;
  uint8_t stateDur;
  bool stateOn;
} flasher;

#define FLASHERS_PER_ZONE 6
#define MAX_SHIMMER 92

uint16_t mode_fairy() {
  //set every pixel to a 'random' color from palette (using seed so it doesn't change between frames)
  uint16_t PRNG16 = 5100 + strip.getCurrSegmentId();
  for (unsigned i = 0; i < SEGLEN; i++) {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0));
  }

  //amount of flasher pixels depending on intensity (0: none, 255: every LED)
  if (SEGMENT.intensity == 0) return FRAMETIME;
  unsigned flasherDistance = ((255 - SEGMENT.intensity) / 28) +1; //1-10
  unsigned numFlashers = (SEGLEN / flasherDistance) +1;

  unsigned dataSize = sizeof(flasher) * numFlashers;
  if (!SEGENV.allocateData(dataSize)) return FRAMETIME; //allocation failed
  Flasher* flashers = reinterpret_cast<Flasher*>(SEGENV.data);
  unsigned now16 = strip.now & 0xFFFF;

  //Up to 11 flashers in one brightness zone, afterwards a new zone for every 6 flashers
  unsigned zones = numFlashers/FLASHERS_PER_ZONE;
  if (!zones) zones = 1;
  unsigned flashersInZone = numFlashers/zones;
  uint8_t flasherBri[FLASHERS_PER_ZONE*2 -1];

  for (unsigned z = 0; z < zones; z++) {
    unsigned flasherBriSum = 0;
    unsigned firstFlasher = z*flashersInZone;
    if (z == zones-1) flashersInZone = numFlashers-(flashersInZone*(zones-1));

    for (unsigned f = firstFlasher; f < firstFlasher + flashersInZone; f++) {
      unsigned stateTime = uint16_t(now16 - flashers[f].stateStart);
      //random on/off time reached, switch state
      if (stateTime > flashers[f].stateDur * 10) {
        flashers[f].stateOn = !flashers[f].stateOn;
        if (flashers[f].stateOn) {
          flashers[f].stateDur = 12 + hw_random8(12 + ((255 - SEGMENT.speed) >> 2)); //*10, 250ms to 1250ms
        } else {
          flashers[f].stateDur = 20 + hw_random8(6 + ((255 - SEGMENT.speed) >> 2)); //*10, 250ms to 1250ms
        }
        //flashers[f].stateDur = 51 + hw_random8(2 + ((255 - SEGMENT.speed) >> 1));
        flashers[f].stateStart = now16;
        if (stateTime < 255) {
          flashers[f].stateStart -= 255 -stateTime; //start early to get correct bri
          flashers[f].stateDur += 26 - stateTime/10;
          stateTime = 255 - stateTime;
        } else {
          stateTime = 0;
        }
      }
      if (stateTime > 255) stateTime = 255; //for flasher brightness calculation, fades in first 255 ms of state
      //flasherBri[f - firstFlasher] = (flashers[f].stateOn) ? 255-SEGMENT.gamma8((510 - stateTime) >> 1) : SEGMENT.gamma8((510 - stateTime) >> 1);
      flasherBri[f - firstFlasher] = (flashers[f].stateOn) ? stateTime : 255 - (stateTime >> 0);
      flasherBriSum += flasherBri[f - firstFlasher];
    }
    //dim factor, to create "shimmer" as other pixels get less voltage if a lot of flashers are on
    unsigned avgFlasherBri = flasherBriSum / flashersInZone;
    unsigned globalPeakBri = 255 - ((avgFlasherBri * MAX_SHIMMER) >> 8); //183-255, suitable for 1/5th of LEDs flashers

    for (unsigned f = firstFlasher; f < firstFlasher + flashersInZone; f++) {
      uint8_t bri = (flasherBri[f - firstFlasher] * globalPeakBri) / 255;
      PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
      unsigned flasherPos = f*flasherDistance;
      SEGMENT.setPixelColor(flasherPos, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0), bri));
      for (unsigned i = flasherPos+1; i < flasherPos+flasherDistance && i < SEGLEN; i++) {
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
        SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0, globalPeakBri));
      }
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_FAIRY[] PROGMEM = "Fairy@!,# of flashers;!,!;!";


/*
 * Fairytwinkle. Like Colortwinkle, but starting from all lit and not relying on strip.getPixelColor
 * Warning: Uses 4 bytes of segment data per pixel
 */
uint16_t mode_fairytwinkle() {
  unsigned dataSize = sizeof(flasher) * SEGLEN;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  Flasher* flashers = reinterpret_cast<Flasher*>(SEGENV.data);
  unsigned now16 = strip.now & 0xFFFF;
  uint16_t PRNG16 = 5100 + strip.getCurrSegmentId();

  unsigned riseFallTime = 400 + (255-SEGMENT.speed)*3;
  unsigned maxDur = riseFallTime/100 + ((255 - SEGMENT.intensity) >> 2) + 13 + ((255 - SEGMENT.intensity) >> 1);

  for (unsigned f = 0; f < SEGLEN; f++) {
    uint16_t stateTime = now16 - flashers[f].stateStart;
    //random on/off time reached, switch state
    if (stateTime > flashers[f].stateDur * 100) {
      flashers[f].stateOn = !flashers[f].stateOn;
      bool init = !flashers[f].stateDur;
      if (flashers[f].stateOn) {
        flashers[f].stateDur = riseFallTime/100 + ((255 - SEGMENT.intensity) >> 2) + hw_random8(12 + ((255 - SEGMENT.intensity) >> 1)) +1;
      } else {
        flashers[f].stateDur = riseFallTime/100 + hw_random8(3 + ((255 - SEGMENT.speed) >> 6)) +1;
      }
      flashers[f].stateStart = now16;
      stateTime = 0;
      if (init) {
        flashers[f].stateStart -= riseFallTime; //start lit
        flashers[f].stateDur = riseFallTime/100 + hw_random8(12 + ((255 - SEGMENT.intensity) >> 1)) +5; //fire up a little quicker
        stateTime = riseFallTime;
      }
    }
    if (flashers[f].stateOn && flashers[f].stateDur > maxDur) flashers[f].stateDur = maxDur; //react more quickly on intensity change
    if (stateTime > riseFallTime) stateTime = riseFallTime; //for flasher brightness calculation, fades in first 255 ms of state
    unsigned fadeprog = 255 - ((stateTime * 255) / riseFallTime);
    uint8_t flasherBri = (flashers[f].stateOn) ? 255-gamma8(fadeprog) : gamma8(fadeprog);
    unsigned lastR = PRNG16;
    unsigned diff = 0;
    while (diff < 0x4000) { //make sure colors of two adjacent LEDs differ enough
      PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
      diff = (PRNG16 > lastR) ? PRNG16 - lastR : lastR - PRNG16;
    }
    SEGMENT.setPixelColor(f, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0), flasherBri));
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_FAIRYTWINKLE[] PROGMEM = "Fairytwinkle@!,!;!,!;!;;m12=0"; //pixels


/*
 * Tricolor chase function
 */
uint16_t tricolor_chase(uint32_t color1, uint32_t color2) {
  uint32_t cycleTime = 50 + ((255 - SEGMENT.speed)<<1);
  uint32_t it = strip.now / cycleTime;  // iterator
  unsigned width = (1 + (SEGMENT.intensity>>4)); // value of 1-16 for each colour
  unsigned index = it % (width*3);

  for (unsigned i = 0; i < SEGLEN; i++, index++) {
    if (index > (width*3)-1) index = 0;

    uint32_t color = color1;
    if (index > (width<<1)-1) color = SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1);
    else if (index > width-1) color = color2;

    SEGMENT.setPixelColor(SEGLEN - i -1, color);
  }
  return FRAMETIME;
}


/*
 * Tricolor chase mode
 */
uint16_t mode_tricolor_chase(void) {
  return tricolor_chase(SEGCOLOR(2), SEGCOLOR(0));
}
static const char _data_FX_MODE_TRICOLOR_CHASE[] PROGMEM = "Chase 3@!,Size;1,2,3;!";


/*
 * ICU mode
 */
uint16_t mode_icu(void) {
  unsigned dest = SEGENV.step & 0xFFFF;
  unsigned space = (SEGMENT.intensity >> 3) +2;

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  byte pindex = map(dest, 0, SEGLEN-SEGLEN/space, 0, 255);
  uint32_t col = SEGMENT.color_from_palette(pindex, false, false, 0);

  SEGMENT.setPixelColor(dest, col);
  SEGMENT.setPixelColor(dest + SEGLEN/space, col);

  if(SEGENV.aux0 == dest) { // pause between eye movements
    if(hw_random8(6) == 0) { // blink once in a while
      SEGMENT.setPixelColor(dest, SEGCOLOR(1));
      SEGMENT.setPixelColor(dest + SEGLEN/space, SEGCOLOR(1));
      return 200;
    }
    SEGENV.aux0 = hw_random16(SEGLEN-SEGLEN/space);
    return 1000 + hw_random16(2000);
  }

  if(SEGENV.aux0 > SEGENV.step) {
    SEGENV.step++;
    dest++;
  } else if (SEGENV.aux0 < SEGENV.step) {
    SEGENV.step--;
    dest--;
  }

  SEGMENT.setPixelColor(dest, col);
  SEGMENT.setPixelColor(dest + SEGLEN/space, col);

  return SPEED_FORMULA_L;
}
static const char _data_FX_MODE_ICU[] PROGMEM = "ICU@!,!,,,,,Overlay;!,!;!";


/*
 * Custom mode by Aircoookie. Color Wipe, but with 3 colors
 */
uint16_t mode_tricolor_wipe(void) {
  uint32_t cycleTime = 1000 + (255 - SEGMENT.speed)*200;
  uint32_t perc = strip.now % cycleTime;
  unsigned prog = (perc * 65535) / cycleTime;
  unsigned ledIndex = (prog * SEGLEN * 3) >> 16;
  unsigned ledOffset = ledIndex;

  for (unsigned i = 0; i < SEGLEN; i++)
  {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2));
  }

  if(ledIndex < SEGLEN) { //wipe from 0 to 1
    for (unsigned i = 0; i < SEGLEN; i++)
    {
      SEGMENT.setPixelColor(i, (i > ledOffset)? SEGCOLOR(0) : SEGCOLOR(1));
    }
  } else if (ledIndex < SEGLEN*2) { //wipe from 1 to 2
    ledOffset = ledIndex - SEGLEN;
    for (unsigned i = ledOffset +1; i < SEGLEN; i++)
    {
      SEGMENT.setPixelColor(i, SEGCOLOR(1));
    }
  } else //wipe from 2 to 0
  {
    ledOffset = ledIndex - SEGLEN*2;
    for (unsigned i = 0; i <= ledOffset; i++)
    {
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_TRICOLOR_WIPE[] PROGMEM = "Tri Wipe@!;1,2,3;!";


/*
 * Fades between 3 colors
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/TriFade.h
 * Modified by Aircoookie
 */
uint16_t mode_tricolor_fade(void) {
  unsigned counter = strip.now * ((SEGMENT.speed >> 3) +1);
  uint16_t prog = (counter * 768) >> 16;

  uint32_t color1 = 0, color2 = 0;
  unsigned stage = 0;

  if(prog < 256) {
    color1 = SEGCOLOR(0);
    color2 = SEGCOLOR(1);
    stage = 0;
  } else if(prog < 512) {
    color1 = SEGCOLOR(1);
    color2 = SEGCOLOR(2);
    stage = 1;
  } else {
    color1 = SEGCOLOR(2);
    color2 = SEGCOLOR(0);
    stage = 2;
  }

  byte stp = prog; // % 256
  for (unsigned i = 0; i < SEGLEN; i++) {
    uint32_t color;
    if (stage == 2) {
      color = color_blend(SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), color2, stp);
    } else if (stage == 1) {
      color = color_blend(color1, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), stp);
    } else {
      color = color_blend(color1, color2, stp);
    }
    SEGMENT.setPixelColor(i, color);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_TRICOLOR_FADE[] PROGMEM = "Tri Fade@!;1,2,3;!";

#ifdef WLED_PS_DONT_REPLACE_FX
/*
 * Creates random comets
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/MultiComet.h
 */
#define MAX_COMETS 8
uint16_t mode_multi_comet(void) {
  uint32_t cycleTime = 10 + (uint32_t)(255 - SEGMENT.speed);
  uint32_t it = strip.now / cycleTime;
  if (SEGENV.step == it) return FRAMETIME;
  if (!SEGENV.allocateData(sizeof(uint16_t) * MAX_COMETS)) return mode_static(); //allocation failed

  SEGMENT.fade_out(SEGMENT.intensity/2 + 128);

  uint16_t* comets = reinterpret_cast<uint16_t*>(SEGENV.data);

  for (unsigned i=0; i < MAX_COMETS; i++) {
    if(comets[i] < SEGLEN) {
      unsigned index = comets[i];
      if (SEGCOLOR(2) != 0)
      {
        SEGMENT.setPixelColor(index, i % 2 ? SEGMENT.color_from_palette(index, true, PALETTE_SOLID_WRAP, 0) : SEGCOLOR(2));
      } else
      {
        SEGMENT.setPixelColor(index, SEGMENT.color_from_palette(index, true, PALETTE_SOLID_WRAP, 0));
      }
      comets[i]++;
    } else {
      if(!hw_random16(SEGLEN)) {
        comets[i] = 0;
      }
    }
  }

  SEGENV.step = it;
  return FRAMETIME;
}
static const char _data_FX_MODE_MULTI_COMET[] PROGMEM = "Multi Comet@!,Fade;!,!;!;1";
#undef MAX_COMETS
#endif // WLED_PS_DONT_REPLACE_FX

/*
 * Running random pixels ("Stream 2")
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/RandomChase.h
 */
uint16_t mode_random_chase(void) {
  if (SEGENV.call == 0) {
    SEGENV.step = RGBW32(random8(), random8(), random8(), 0);
    SEGENV.aux0 = random16();
  }
  unsigned prevSeed = random16_get_seed(); // save seed so we can restore it at the end of the function
  uint32_t cycleTime = 25 + (3 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = strip.now / cycleTime;
  uint32_t color = SEGENV.step;
  random16_set_seed(SEGENV.aux0);

  for (int i = SEGLEN -1; i >= 0; i--) {
    uint8_t r = random8(6) != 0 ? (color >> 16 & 0xFF) : random8();
    uint8_t g = random8(6) != 0 ? (color >> 8  & 0xFF) : random8();
    uint8_t b = random8(6) != 0 ? (color       & 0xFF) : random8();
    color = RGBW32(r, g, b, 0);
    SEGMENT.setPixelColor(i, color);
    if (i == SEGLEN -1U && SEGENV.aux1 != (it & 0xFFFFU)) { //new first color in next frame
      SEGENV.step = color;
      SEGENV.aux0 = random16_get_seed();
    }
  }

  SEGENV.aux1 = it & 0xFFFF;

  random16_set_seed(prevSeed); // restore original seed so other effects can use "random" PRNG
  return FRAMETIME;
}
static const char _data_FX_MODE_RANDOM_CHASE[] PROGMEM = "Stream 2@!;;";


//7 bytes
typedef struct Oscillator {
  uint16_t pos;
  uint8_t  size;
  int8_t   dir;
  uint8_t  speed;
} oscillator;

/*
/  Oscillating bars of color, updated with standard framerate
*/
uint16_t mode_oscillate(void) {
  constexpr unsigned numOscillators = 3;
  constexpr unsigned dataSize = sizeof(oscillator) * numOscillators;

  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  Oscillator* oscillators = reinterpret_cast<Oscillator*>(SEGENV.data);

  if (SEGENV.call == 0)
  {
    oscillators[0] = {(uint16_t)(SEGLEN/4),   (uint8_t)(SEGLEN/8),  1, 1};
    oscillators[1] = {(uint16_t)(SEGLEN/4*3), (uint8_t)(SEGLEN/8),  1, 2};
    oscillators[2] = {(uint16_t)(SEGLEN/4*2), (uint8_t)(SEGLEN/8), -1, 1};
  }

  uint32_t cycleTime = 20 + (2 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = strip.now / cycleTime;

  for (unsigned i = 0; i < numOscillators; i++) {
    // if the counter has increased, move the oscillator by the random step
    if (it != SEGENV.step) oscillators[i].pos += oscillators[i].dir * oscillators[i].speed;
    oscillators[i].size = SEGLEN/(3+SEGMENT.intensity/8);
    if((oscillators[i].dir == -1) && (oscillators[i].pos > SEGLEN << 1)) { // use integer overflow
      oscillators[i].pos = 0;
      oscillators[i].dir = 1;
      // make bigger steps for faster speeds
      oscillators[i].speed = SEGMENT.speed > 100 ? hw_random8(2, 4):hw_random8(1, 3);
    }
    if((oscillators[i].dir == 1) && (oscillators[i].pos >= (SEGLEN - 1))) {
      oscillators[i].pos = SEGLEN - 1;
      oscillators[i].dir = -1;
      oscillators[i].speed = SEGMENT.speed > 100 ? hw_random8(2, 4):hw_random8(1, 3);
    }
  }

  for (unsigned i = 0; i < SEGLEN; i++) {
    uint32_t color = BLACK;
    for (unsigned j = 0; j < numOscillators; j++) {
      if((int)i >= (int)oscillators[j].pos - oscillators[j].size && i <= oscillators[j].pos + oscillators[j].size) {
        color = (color == BLACK) ? SEGCOLOR(j) : color_blend(color, SEGCOLOR(j), uint8_t(128));
      }
    }
    SEGMENT.setPixelColor(i, color);
  }

  SEGENV.step = it;
  return FRAMETIME;
}
static const char _data_FX_MODE_OSCILLATE[] PROGMEM = "Oscillate";


//TODO
uint16_t mode_lightning(void) {
  if (SEGLEN <= 1) return mode_static();
  unsigned ledstart = hw_random16(SEGLEN);               // Determine starting location of flash
  unsigned ledlen = 1 + hw_random16(SEGLEN -ledstart);   // Determine length of flash (not to go beyond NUM_LEDS-1)
  uint8_t bri = 255/hw_random8(1, 3);

  if (SEGENV.aux1 == 0) //init, leader flash
  {
    SEGENV.aux1 = hw_random8(4, 4 + SEGMENT.intensity/20); //number of flashes
    SEGENV.aux1 *= 2;

    bri = 52; //leader has lower brightness
    SEGENV.aux0 = 200; //200ms delay after leader
  }

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  if (SEGENV.aux1 > 3 && !(SEGENV.aux1 & 0x01)) { //flash on even number >2
    for (unsigned i = ledstart; i < ledstart + ledlen; i++)
    {
      SEGMENT.setPixelColor(i,SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0, bri));
    }
    SEGENV.aux1--;

    SEGENV.step = strip.now;
    //return hw_random8(4, 10); // each flash only lasts one frame/every 24ms... originally 4-10 milliseconds
  } else {
    if (strip.now - SEGENV.step > SEGENV.aux0) {
      SEGENV.aux1--;
      if (SEGENV.aux1 < 2) SEGENV.aux1 = 0;

      SEGENV.aux0 = (50 + hw_random8(100)); //delay between flashes
      if (SEGENV.aux1 == 2) {
        SEGENV.aux0 = (hw_random8(255 - SEGMENT.speed) * 100); // delay between strikes
      }
      SEGENV.step = strip.now;
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_LIGHTNING[] PROGMEM = "Lightning@!,!,,,,,Overlay;!,!;!";

// combined function from original pride and colorwaves
uint16_t mode_colorwaves_pride_base(bool isPride2015) {
  unsigned duration = 10 + SEGMENT.speed;
  unsigned sPseudotime = SEGENV.step;
  unsigned sHue16 = SEGENV.aux0;

  uint8_t sat8 = isPride2015 ? beatsin88_t(87, 220, 250) : 255;
  unsigned brightdepth = beatsin88_t(341, 96, 224);
  unsigned brightnessthetainc16 = beatsin88_t(203, (25 * 256), (40 * 256));
  unsigned msmultiplier = beatsin88_t(147, 23, 60);

  unsigned hue16 = sHue16;
  unsigned hueinc16 = isPride2015 ? beatsin88_t(113, 1, 3000) : 
                                     beatsin88_t(113, 60, 300) * SEGMENT.intensity * 10 / 255;

  sPseudotime += duration * msmultiplier;
  sHue16 += duration * beatsin88_t(400, 5, 9);
  unsigned brightnesstheta16 = sPseudotime;

  for (unsigned i = 0; i < SEGLEN; i++) {
    hue16 += hueinc16;
    uint8_t hue8;

    if (isPride2015) {
      hue8 = hue16 >> 8;
    } else {
      unsigned h16_128 = hue16 >> 7;
      hue8 = (h16_128 & 0x100) ? (255 - (h16_128 >> 1)) : (h16_128 >> 1);
    }

    brightnesstheta16 += brightnessthetainc16;
    unsigned b16 = sin16_t(brightnesstheta16) + 32768;
    unsigned bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    if (isPride2015) {
      CRGBW newcolor = CRGB(CHSV(hue8, sat8, bri8));
      newcolor.color32 = gamma32inv(newcolor.color32);
      SEGMENT.blendPixelColor(i, newcolor, 64);
    } else {
      SEGMENT.blendPixelColor(i, SEGMENT.color_from_palette(hue8, false, PALETTE_SOLID_WRAP, 0, bri8), 128);
    }
  }

  SEGENV.step = sPseudotime;
  SEGENV.aux0 = sHue16;

  return FRAMETIME;
}

// Pride2015
// Animated, ever-changing rainbows.
// by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
uint16_t mode_pride_2015(void) {
  return mode_colorwaves_pride_base(true);
}
static const char _data_FX_MODE_PRIDE_2015[] PROGMEM = "Pride 2015@!;;";

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
uint16_t mode_colorwaves() {
  return mode_colorwaves_pride_base(false);
}
static const char _data_FX_MODE_COLORWAVES[] PROGMEM = "Colorwaves@!,Hue;!;!;;pal=26";


//eight colored dots, weaving in and out of sync with each other
uint16_t mode_juggle(void) {
  if (SEGLEN <= 1) return mode_static();

  SEGMENT.fadeToBlackBy(192 - (3*SEGMENT.intensity/4));
  CRGB fastled_col;
  byte dothue = 0;
  for (int i = 0; i < 8; i++) {
    int index = 0 + beatsin88_t((16 + SEGMENT.speed)*(i + 7), 0, SEGLEN -1);
    fastled_col = CRGB(SEGMENT.getPixelColor(index));
    fastled_col |= (SEGMENT.palette==0)?CHSV(dothue, 220, 255):CRGB(ColorFromPalette(SEGPALETTE, dothue, 255));
    SEGMENT.setPixelColor(index, fastled_col);
    dothue += 32;
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_JUGGLE[] PROGMEM = "Juggle@!,Trail;;!;;sx=64,ix=128";


uint16_t mode_palette() {
  // Set up some compile time constants so that we can handle integer and float based modes using the same code base.
#ifdef ESP8266
  using mathType = int32_t;
  using wideMathType = int64_t;
  using angleType = unsigned;
  constexpr mathType sInt16Scale             = 0x7FFF;
  constexpr mathType maxAngle                = 0x8000;
  constexpr mathType staticRotationScale     = 256;
  constexpr mathType animatedRotationScale   = 1;
  constexpr int16_t (*sinFunction)(uint16_t) = &sin16_t;
  constexpr int16_t (*cosFunction)(uint16_t) = &cos16_t;
#else
  using mathType = float;
  using wideMathType = float;
  using angleType = float;
  constexpr mathType sInt16Scale           = 1.0f;
  constexpr mathType maxAngle              = M_PI / 256.0;
  constexpr mathType staticRotationScale   = 1.0f;
  constexpr mathType animatedRotationScale = M_TWOPI / double(0xFFFF);
  constexpr float (*sinFunction)(float)    = &sin_t;
  constexpr float (*cosFunction)(float)    = &cos_t;
#endif
  const bool isMatrix = strip.isMatrix;
  const int cols = SEG_W;
  const int rows = isMatrix ? SEG_H : strip.getActiveSegmentsNum();

  const int  inputShift           = SEGMENT.speed;
  const int  inputSize            = SEGMENT.intensity;
  const int  inputRotation        = SEGMENT.custom1;
  const bool inputAnimateShift    = SEGMENT.check1;
  const bool inputAnimateRotation = SEGMENT.check2;
  const bool inputAssumeSquare    = SEGMENT.check3;

  const angleType theta = (!inputAnimateRotation) ? ((inputRotation + 128) * maxAngle / staticRotationScale) : (((strip.now * ((inputRotation >> 4) +1)) & 0xFFFF) * animatedRotationScale);
  const mathType sinTheta = sinFunction(theta);
  const mathType cosTheta = cosFunction(theta);

  const mathType maxX    = std::max(1, cols-1);
  const mathType maxY    = std::max(1, rows-1);
  // Set up some parameters according to inputAssumeSquare, so that we can handle anamorphic mode using the same code base.
  const mathType maxXIn  =  inputAssumeSquare ? maxX : mathType(1);
  const mathType maxYIn  =  inputAssumeSquare ? maxY : mathType(1);
  const mathType maxXOut = !inputAssumeSquare ? maxX : mathType(1);
  const mathType maxYOut = !inputAssumeSquare ? maxY : mathType(1);
  const mathType centerX = sInt16Scale * maxXOut / mathType(2);
  const mathType centerY = sInt16Scale * maxYOut / mathType(2);
  // The basic idea for this effect is to rotate a rectangle that is filled with the palette along one axis, then map our
  // display to it, to find what color a pixel should have.
  // However, we want a) no areas of solid color (in front of or behind the palette), and b) we want to make use of the full palette.
  // So the rectangle needs to have exactly the right size. That size depends on the rotation.
  // This scale computation here only considers one dimension. You can think of it like the rectangle is always scaled so that
  // the left and right most points always match the left and right side of the display.
  const mathType scale = std::abs(sinTheta) + (std::abs(cosTheta) * maxYOut / maxXOut);
  // 2D simulation:
  // If we are dealing with a 1D setup, we assume that each segment represents one line on a 2-dimensional display.
  // The function is called once per segments, so we need to handle one line at a time.
  const int yFrom = isMatrix ? 0 : strip.getCurrSegmentId();
  const int yTo   = isMatrix ? maxY : yFrom;
  for (int y = yFrom; y <= yTo; ++y) {
    // translate, scale, rotate
    const mathType ytCosTheta = mathType((wideMathType(cosTheta) * wideMathType(y * sInt16Scale - centerY * maxYIn))/wideMathType(maxYIn * scale));
    for (int x = 0; x < cols; ++x) {
      // translate, scale, rotate
      const mathType xtSinTheta = mathType((wideMathType(sinTheta) * wideMathType(x * sInt16Scale - centerX * maxXIn))/wideMathType(maxXIn * scale));
      // Map the pixel coordinate to an imaginary-rectangle-coordinate.
      // The y coordinate doesn't actually matter, as our imaginary rectangle is filled with the palette from left to right,
      // so all points at a given x-coordinate have the same color.
      const mathType sourceX = xtSinTheta + ytCosTheta + centerX;
      // The computation was scaled just right so that the result should always be in range [0, maxXOut], but enforce this anyway
      // to account for imprecision. Then scale it so that the range is [0, 255], which we can use with the palette.
      int colorIndex = (std::min(std::max(sourceX, mathType(0)), maxXOut * sInt16Scale) * wideMathType(255)) / (sInt16Scale * maxXOut);
      // inputSize determines by how much we want to scale the palette:
      // values < 128 display a fraction of a palette,
      // values > 128 display multiple palettes.
      if (inputSize <= 128) {
        colorIndex = (colorIndex * inputSize) / 128;
      } else {
        // Linear function that maps colorIndex 128=>1, 256=>9.
        // With this function every full palette repetition is exactly 16 configuration steps wide.
        // That allows displaying exactly 2 repetitions for example.
        colorIndex = ((inputSize - 112) * colorIndex) / 16;
      }
      // Finally, shift the palette a bit.
      const int paletteOffset = (!inputAnimateShift) ? (inputShift) : (((strip.now * ((inputShift >> 3) +1)) & 0xFFFF) >> 8);
      colorIndex -= paletteOffset;
      const uint32_t color = SEGMENT.color_wheel((uint8_t)colorIndex);
      if (isMatrix) {
        SEGMENT.setPixelColorXY(x, y, color);
      } else {
        SEGMENT.setPixelColor(x, color);
      }
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_PALETTE[] PROGMEM = "Palette@Shift,Size,Rotation,,,Animate Shift,Animate Rotation,Anamorphic;;!;12;ix=112,c1=0,o1=1,o2=0,o3=1";

#ifdef WLED_PS_DONT_REPLACE_FX
// WLED limitation: Analog Clock overlay will NOT work when Fire2012 is active
// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on SEGLEN; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above) (Speed = COOLING), and SPARKING (used
// in step 3 above) (Effect Intensity = Sparking).
uint16_t mode_fire_2012() {
  if (SEGLEN <= 1) return mode_static();
  const unsigned strips = SEGMENT.nrOfVStrips();
  if (!SEGENV.allocateData(strips * SEGLEN)) return mode_static(); //allocation failed
  byte* heat = SEGENV.data;

  const uint32_t it = strip.now >> 5; //div 32

  struct virtualStrip {
    static void runStrip(uint16_t stripNr, byte* heat, uint32_t it) {

      const uint8_t ignition = MAX(3,SEGLEN/10);  // ignition area: 10% of segment length or minimum 3 pixels

      // Step 1.  Cool down every cell a little
      for (unsigned i = 0; i < SEGLEN; i++) {
        uint8_t cool = (it != SEGENV.step) ? hw_random8((((20 + SEGMENT.speed/3) * 16) / SEGLEN)+2) : hw_random8(4);
        uint8_t minTemp = (i<ignition) ? (ignition-i)/4 + 16 : 0;  // should not become black in ignition area
        uint8_t temp = qsub8(heat[i], cool);
        heat[i] = temp<minTemp ? minTemp : temp;
      }

      if (it != SEGENV.step) {
        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int k = SEGLEN -1; k > 1; k--) {
          heat[k] = (heat[k - 1] + (heat[k - 2]<<1) ) / 3;  // heat[k-2] multiplied by 2
        }

        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        if (hw_random8() <= SEGMENT.intensity) {
          uint8_t y = hw_random8(ignition);
          uint8_t boost = (17+SEGMENT.custom3) * (ignition - y/2) / ignition; // integer math!
          heat[y] = qadd8(heat[y], hw_random8(96+2*boost,207+boost));
        }
      }

      // Step 4.  Map from heat cells to LED colors
      for (unsigned j = 0; j < SEGLEN; j++) {
        SEGMENT.setPixelColor(indexToVStrip(j, stripNr), ColorFromPalette(SEGPALETTE, min(heat[j], byte(240)), 255, NOBLEND));
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &heat[stripNr * SEGLEN], it);

  if (SEGMENT.is2D()) {
    uint8_t blurAmount = SEGMENT.custom2 >> 2;
    if (blurAmount > 48) blurAmount += blurAmount-48;             // extra blur when slider > 192  (bush burn)
    if (blurAmount < 16) SEGMENT.blurCols(SEGMENT.custom2 >> 1);  // no side-burn when slider < 64 (faster)
    else SEGMENT.blur(blurAmount);
  }

  if (it != SEGENV.step)
    SEGENV.step = it;

  return FRAMETIME;
}
static const char _data_FX_MODE_FIRE_2012[] PROGMEM = "Fire 2012@Cooling,Spark rate,,2D Blur,Boost;;!;1;pal=35,sx=64,ix=160,m12=1,c2=128"; // bars
#endif // WLED_PS_DONT_REPLACE_FX

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
uint16_t mode_bpm() {
  uint32_t stp = (strip.now / 20) & 0xFF;
  uint8_t beat = beatsin8_t(SEGMENT.speed, 64, 255);
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(stp + (i * 2), false, PALETTE_SOLID_WRAP, 0, beat - stp + (i * 10)));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_BPM[] PROGMEM = "Bpm@!;!;!;;sx=64";


uint16_t mode_fillnoise8() {
  if (SEGENV.call == 0) SEGENV.step = hw_random();
  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned index = perlin8(i * SEGLEN, SEGENV.step + i * SEGLEN);
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
  }
  SEGENV.step += beatsin8_t(SEGMENT.speed, 1, 6); //10,1,4

  return FRAMETIME;
}
static const char _data_FX_MODE_FILLNOISE8[] PROGMEM = "Fill Noise@!;!;!";


uint16_t mode_noise16_1() {
  unsigned scale = 320;                                       // the "zoom factor" for the noise
  SEGENV.step += (1 + SEGMENT.speed/16);

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned shift_x = beatsin8_t(11);                          // the x position of the noise field swings @ 17 bpm
    unsigned shift_y = SEGENV.step/42;                        // the y position becomes slowly incremented
    unsigned real_x = (i + shift_x) * scale;                  // the x position of the noise field swings @ 17 bpm
    unsigned real_y = (i + shift_y) * scale;                  // the y position becomes slowly incremented
    uint32_t real_z = SEGENV.step;                            // the z position becomes quickly incremented
    unsigned noise = perlin16(real_x, real_y, real_z) >> 8;   // get the noise data and scale it down
    unsigned index = sin8_t(noise * 3);                         // map LED color based on noise data

    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_NOISE16_1[] PROGMEM = "Noise 1@!;!;!;;pal=20";


uint16_t mode_noise16_2() {
  unsigned scale = 1000;                                        // the "zoom factor" for the noise
  SEGENV.step += (1 + (SEGMENT.speed >> 1));

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned shift_x = SEGENV.step >> 6;                        // x as a function of time
    uint32_t real_x = (i + shift_x) * scale;                    // calculate the coordinates within the noise field
    unsigned noise = perlin16(real_x, 0, 4223) >> 8;            // get the noise data and scale it down
    unsigned index = sin8_t(noise * 3);                           // map led color based on noise data

    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0, noise));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_NOISE16_2[] PROGMEM = "Noise 2@!;!;!;;pal=43";


uint16_t mode_noise16_3() {
  unsigned scale = 800;                                       // the "zoom factor" for the noise
  SEGENV.step += (1 + SEGMENT.speed);

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned shift_x = 4223;                                  // no movement along x and y
    unsigned shift_y = 1234;
    uint32_t real_x = (i + shift_x) * scale;                  // calculate the coordinates within the noise field
    uint32_t real_y = (i + shift_y) * scale;                  // based on the precalculated positions
    uint32_t real_z = SEGENV.step*8;
    unsigned noise = perlin16(real_x, real_y, real_z) >> 8;   // get the noise data and scale it down
    unsigned index = sin8_t(noise * 3);                         // map led color based on noise data

    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0, noise));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_NOISE16_3[] PROGMEM = "Noise 3@!;!;!;;pal=35";


//https://github.com/aykevl/ledstrip-spark/blob/master/ledstrip.ino
uint16_t mode_noise16_4() {
  uint32_t stp = (strip.now * SEGMENT.speed) >> 7;
  for (unsigned i = 0; i < SEGLEN; i++) {
    int index = perlin16(uint32_t(i) << 12, stp);
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_NOISE16_4[] PROGMEM = "Noise 4@!;!;!;;pal=26";


//based on https://gist.github.com/kriegsman/5408ecd397744ba0393e
uint16_t mode_colortwinkle() {
  unsigned dataSize = (SEGLEN+7) >> 3; //1 bit per LED
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  CRGBW col, prev;
  fract8 fadeUpAmount = strip.getBrightness()>28 ? 8 + (SEGMENT.speed>>2) : 68-strip.getBrightness();
  fract8 fadeDownAmount = strip.getBrightness()>28 ? 8 + (SEGMENT.speed>>3) : 68-strip.getBrightness();
  for (unsigned i = 0; i < SEGLEN; i++) {
    CRGBW cur = SEGMENT.getPixelColor(i);
    prev = cur;
    unsigned index = i >> 3;
    unsigned  bitNum = i & 0x07;
    bool fadeUp = bitRead(SEGENV.data[index], bitNum);

    if (fadeUp) {
      CRGBW incrementalColor = color_fade(cur, fadeUpAmount, true);
      col = color_add(cur, incrementalColor);

      if (col.r == 255 || col.g == 255 || col.b == 255) {
        bitWrite(SEGENV.data[index], bitNum, false);
      }

      if (cur == prev) {  //fix "stuck" pixels
        color_add(col, col);
        SEGMENT.setPixelColor(i, col);
      }
      else SEGMENT.setPixelColor(i, col);
    }
    else {
      col = color_fade(cur, 255 - fadeDownAmount);
      SEGMENT.setPixelColor(i, col);
    }
  }

  for (unsigned j = 0; j <= SEGLEN / 50; j++) {
    if (hw_random8() <= SEGMENT.intensity) {
      for (unsigned times = 0; times < 5; times++) { //attempt to spawn a new pixel 5 times
        int i = hw_random16(SEGLEN);
        if (SEGMENT.getPixelColor(i) == 0) {
          unsigned index = i >> 3;
          unsigned  bitNum = i & 0x07;
          bitWrite(SEGENV.data[index], bitNum, true);
          SEGMENT.setPixelColor(i, ColorFromPalette(SEGPALETTE, hw_random8(), 64, NOBLEND));
          break; //only spawn 1 new pixel per frame per 50 LEDs
        }
      }
    }
  }
  return FRAMETIME_FIXED;
}
static const char _data_FX_MODE_COLORTWINKLE[] PROGMEM = "Colortwinkles@Fade speed,Spawn speed;;!;;m12=0"; //pixels


//Calm effect, like a lake at night
uint16_t mode_lake() {
  unsigned sp = SEGMENT.speed/10;
  int wave1 = beatsin8_t(sp +2, -64,64);
  int wave2 = beatsin8_t(sp +1, -64,64);
  int wave3 = beatsin8_t(sp +2,   0,80);

  for (unsigned i = 0; i < SEGLEN; i++)
  {
    int index = cos8_t((i*15)+ wave1)/2 + cubicwave8((i*23)+ wave2)/2;
    uint8_t lum = (index > wave3) ? index - wave3 : 0;
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, false, 0, lum));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_LAKE[] PROGMEM = "Lake@!;Fx;!";


// meteor effect & meteor smooth (merged by @dedehai)
// send a meteor from begining to to the end of the strip with a trail that randomly decays.
// adapted from https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/#LEDStripEffectMeteorRain
uint16_t mode_meteor() {
  if (SEGLEN <= 1) return mode_static();
  if (!SEGENV.allocateData(SEGLEN)) return mode_static(); //allocation failed
  const bool meteorSmooth = SEGMENT.check3;
  byte* trail = SEGENV.data;

  const unsigned meteorSize = 1 + SEGLEN / 20; // 5%
  uint16_t meteorstart;
  if(meteorSmooth) meteorstart = map((SEGENV.step >> 6 & 0xFF), 0, 255, 0, SEGLEN -1);
  else {
    unsigned counter = strip.now * ((SEGMENT.speed >> 2) + 8);
    meteorstart = (counter * SEGLEN) >> 16;
  }

  const int max = SEGMENT.palette==5 || !SEGMENT.check1 ? 240 : 255;
  // fade all leds to colors[1] in LEDs one step
  for (unsigned i = 0; i < SEGLEN; i++) {
    uint32_t col;
    if (hw_random8() <= 255 - SEGMENT.intensity) {
      if(meteorSmooth) {
        if (trail[i] > 0) {
          int change = trail[i] + 4 - hw_random8(24); //change each time between -20 and +4
          trail[i] = constrain(change, 0, max);
        }
        col = SEGMENT.check1 ? SEGMENT.color_from_palette(i, true, false, 0, trail[i]) : SEGMENT.color_from_palette(trail[i], false, true, 255);
      }
      else {
        trail[i] = scale8(trail[i], 128 + hw_random8(127));
        int index = trail[i];
        int idx = 255;
        int bri = SEGMENT.palette==35 || SEGMENT.palette==36 ? 255 : trail[i];
        if (!SEGMENT.check1) {
          idx = 0;
          index = map(i,0,SEGLEN,0,max);
          bri = trail[i];
        }
        col = SEGMENT.color_from_palette(index, false, false, idx, bri);  // full brightness for Fire
      }
      SEGMENT.setPixelColor(i, col);
    }
  }

  // draw meteor
  for (unsigned j = 0; j < meteorSize; j++) {
    unsigned index = (meteorstart + j) % SEGLEN;
    if(meteorSmooth) {
        trail[index] = max;
        uint32_t col = SEGMENT.check1 ? SEGMENT.color_from_palette(index, true, false, 0, trail[index]) : SEGMENT.color_from_palette(trail[index], false, true, 255);
        SEGMENT.setPixelColor(index, col);
    }
    else{
      int idx = 255;
      int i = trail[index] = max;
      if (!SEGMENT.check1) {
        i = map(index,0,SEGLEN,0,max);
        idx = 0;
      }
      uint32_t col = SEGMENT.color_from_palette(i, false, false, idx, 255); // full brightness
      SEGMENT.setPixelColor(index, col);
    }
  }

  SEGENV.step += SEGMENT.speed +1;
  return FRAMETIME;
}
static const char _data_FX_MODE_METEOR[] PROGMEM = "Meteor@!,Trail,,,,Gradient,,Smooth;;!;1";


//Railway Crossing / Christmas Fairy lights
uint16_t mode_railway() {
  if (SEGLEN <= 1) return mode_static();
  unsigned dur = (256 - SEGMENT.speed) * 40;
  uint16_t rampdur = (dur * SEGMENT.intensity) >> 8;
  if (SEGENV.step > dur)
  {
    //reverse direction
    SEGENV.step = 0;
    SEGENV.aux0 = !SEGENV.aux0;
  }
  unsigned pos = 255;
  if (rampdur != 0)
  {
    unsigned p0 = (SEGENV.step * 255) / rampdur;
    if (p0 < 255) pos = p0;
  }
  if (SEGENV.aux0) pos = 255 - pos;
  for (unsigned i = 0; i < SEGLEN; i += 2)
  {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(255 - pos, false, false, 255)); // do not use color 1 or 2, always use palette
    if (i < SEGLEN -1)
    {
      SEGMENT.setPixelColor(i + 1, SEGMENT.color_from_palette(pos, false, false, 255)); // do not use color 1 or 2, always use palette
    }
  }
  SEGENV.step += FRAMETIME;
  return FRAMETIME;
}
static const char _data_FX_MODE_RAILWAY[] PROGMEM = "Railway@!,Smoothness;1,2;!;;pal=3";


//Water ripple
//propagation velocity from speed
//drop rate from intensity

//4 bytes
typedef struct Ripple {
  uint8_t state;
  uint8_t color;
  uint16_t pos;
} ripple;

#ifdef ESP8266
  #define MAX_RIPPLES   56
#else
  #define MAX_RIPPLES  100
#endif
static uint16_t ripple_base(uint8_t blurAmount = 0) {
  unsigned maxRipples = min(1 + (int)(SEGLEN >> 2), MAX_RIPPLES);  // 56 max for 16 segment ESP8266
  unsigned dataSize = sizeof(ripple) * maxRipples;

  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  Ripple* ripples = reinterpret_cast<Ripple*>(SEGENV.data);

  //draw wave
  for (unsigned i = 0; i < maxRipples; i++) {
    unsigned ripplestate = ripples[i].state;
    if (ripplestate) {
      unsigned rippledecay = (SEGMENT.speed >> 4) +1; //faster decay if faster propagation
      unsigned rippleorigin = ripples[i].pos;
      uint32_t col = SEGMENT.color_from_palette(ripples[i].color, false, false, 255);
      unsigned propagation = ((ripplestate/rippledecay - 1) * (SEGMENT.speed + 1));
      int propI = propagation >> 8;
      unsigned propF = propagation & 0xFF;
      unsigned amp = (ripplestate < 17) ? triwave8((ripplestate-1)*8) : map(ripplestate,17,255,255,2);

      #ifndef WLED_DISABLE_2D
      if (SEGMENT.is2D()) {
        propI /= 2;
        unsigned cx = rippleorigin >> 8;
        unsigned cy = rippleorigin & 0xFF;
        unsigned mag = scale8(sin8_t((propF>>2)), amp);
        if (propI > 0) SEGMENT.drawCircle(cx, cy, propI, color_blend(SEGMENT.getPixelColorXY(cx + propI, cy), col, mag), true);
      } else
      #endif
      {
        int left = rippleorigin - propI -1;
        int right = rippleorigin + propI +2;
        for (int v = 0; v < 4; v++) {
          uint8_t mag = scale8(cubicwave8((propF>>2) + v * 64), amp);
          SEGMENT.setPixelColor(left + v, color_blend(SEGMENT.getPixelColor(left + v), col, mag)); // TODO
          SEGMENT.setPixelColor(right - v, color_blend(SEGMENT.getPixelColor(right - v), col, mag)); // TODO
        }
      }
      ripplestate += rippledecay;
      ripples[i].state = (ripplestate > 254) ? 0 : ripplestate;
    } else {//randomly create new wave
      if (hw_random16(IBN + 10000) <= (SEGMENT.intensity >> (SEGMENT.is2D()*3))) {
        ripples[i].state = 1;
        ripples[i].pos = SEGMENT.is2D() ? ((hw_random8(SEG_W)<<8) | (hw_random8(SEG_H))) : hw_random16(SEGLEN);
        ripples[i].color = hw_random8(); //color
      }
    }
  }
  SEGMENT.blur(blurAmount);
  return FRAMETIME;
}
#undef MAX_RIPPLES


uint16_t mode_ripple(void) {
  if (SEGLEN <= 1) return mode_static();
  if(SEGMENT.custom1 || SEGMENT.check2) // blur or overlay
    SEGMENT.fade_out(250);
  else
    SEGMENT.fill(SEGCOLOR(1));

  return ripple_base(SEGMENT.custom1>>1);
}
static const char _data_FX_MODE_RIPPLE[] PROGMEM = "Ripple@!,Wave #,Blur,,,,Overlay;,!;!;12;c1=0";


uint16_t mode_ripple_rainbow(void) {
  if (SEGLEN <= 1) return mode_static();
  if (SEGENV.call ==0) {
    SEGENV.aux0 = hw_random8();
    SEGENV.aux1 = hw_random8();
  }
  if (SEGENV.aux0 == SEGENV.aux1) {
    SEGENV.aux1 = hw_random8();
  } else if (SEGENV.aux1 > SEGENV.aux0) {
    SEGENV.aux0++;
  } else {
    SEGENV.aux0--;
  }
  SEGMENT.fill(color_blend(SEGMENT.color_wheel(SEGENV.aux0),BLACK,uint8_t(235)));
  return ripple_base();
}
static const char _data_FX_MODE_RIPPLE_RAINBOW[] PROGMEM = "Ripple Rainbow@!,Wave #;;!;12";


//  TwinkleFOX by Mark Kriegsman: https://gist.github.com/kriegsman/756ea6dcae8e30845b5a
//
//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette. Read more about this effect using the link above!
static CRGB twinklefox_one_twinkle(uint32_t ms, uint8_t salt, bool cat)
{
  // Overall twinkle speed (changed)
  unsigned ticks = ms / SEGENV.aux0;
  unsigned fastcycle8 = uint8_t(ticks);
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8_t(slowcycle16);
  slowcycle16 = (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

  // Overall twinkle density.
  // 0 (NONE lit) to 8 (ALL lit at once).
  // Default is 5.
  unsigned twinkleDensity = (SEGMENT.intensity >> 5) +1;

  unsigned bright = 0;
  if (((slowcycle8 & 0x0E)/2) < twinkleDensity) {
    unsigned ph = fastcycle8;
    // This is like 'triwave8', which produces a
    // symmetrical up-and-down triangle sawtooth waveform, except that this
    // function produces a triangle wave with a faster attack and a slower decay
    if (cat) //twinklecat, variant where the leds instantly turn on
    {
      bright = 255 - ph;
    } else { //vanilla twinklefox
      if (ph < 86) {
      bright = ph * 3;
      } else {
        ph -= 86;
        bright = 255 - (ph + (ph/2));
      }
    }
  }

  unsigned hue = slowcycle8 - salt;
  CRGB c;
  if (bright > 0) {
    c = ColorFromPalette(SEGPALETTE, hue, bright, NOBLEND);
    if (!SEGMENT.check1) {
      // This code takes a pixel, and if its in the 'fading down'
      // part of the cycle, it adjusts the color a little bit like the
      // way that incandescent bulbs fade toward 'red' as they dim.
      if (fastcycle8 >= 128)
      {
        unsigned cooling = (fastcycle8 - 128) >> 4;
        c.g = qsub8(c.g, cooling);
        c.b = qsub8(c.b, cooling * 2);
      }
    }
  } else {
    c = CRGB::Black;
  }
  return c;
}

//  This function loops over each pixel, calculates the
//  adjusted 'clock' that this pixel should use, and calls
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color,
//  whichever is brighter.
static uint16_t twinklefox_base(bool cat)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;

  // Calculate speed
  if (SEGMENT.speed > 100) SEGENV.aux0 = 3 + ((255 - SEGMENT.speed) >> 3);
  else SEGENV.aux0 = 22 + ((100 - SEGMENT.speed) >> 1);

  // Set up the background color, "bg".
  CRGB bg = CRGB(SEGCOLOR(1));
  unsigned bglight = bg.getAverageLight();
  if (bglight > 64) {
    bg.nscale8_video(16); // very bright, so scale to 1/16th
  } else if (bglight > 16) {
    bg.nscale8_video(64); // not that bright, so scale to 1/4th
  } else {
    bg.nscale8_video(86); // dim, scale to 1/3rd.
  }

  unsigned backgroundBrightness = bg.getAverageLight();

  for (unsigned i = 0; i < SEGLEN; i++) {

    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    unsigned myclockoffset16= PRNG16; // use that number as clock offset
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
    unsigned myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
    uint32_t myclock30 = (uint32_t)((strip.now * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
    unsigned  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

    // We now have the adjusted 'clock' for this pixel, now we call
    // the function that computes what color the pixel should be based
    // on the "brightness = f( time )" idea.
    CRGB c = twinklefox_one_twinkle(myclock30, myunique8, cat);

    unsigned cbright = c.getAverageLight();
    int deltabright = cbright - backgroundBrightness;
    if (deltabright >= 32 || (!bg)) {
      // If the new pixel is significantly brighter than the background color,
      // use the new color.
      SEGMENT.setPixelColor(i, c);
    } else if (deltabright > 0) {
      // If the new pixel is just slightly brighter than the background color,
      // mix a blend of the new color and the background color
      SEGMENT.setPixelColor(i, color_blend(RGBW32(bg.r,bg.g,bg.b,0), RGBW32(c.r,c.g,c.b,0), uint8_t(deltabright * 8)));
    } else {
      // if the new pixel is not at all brighter than the background color,
      // just use the background color.
      SEGMENT.setPixelColor(i, bg);
    }
  }
  return FRAMETIME;
}


uint16_t mode_twinklefox()
{
  return twinklefox_base(false);
}
static const char _data_FX_MODE_TWINKLEFOX[] PROGMEM = "Twinklefox@!,Twinkle rate,,,,Cool;!,!;!";


uint16_t mode_twinklecat()
{
  return twinklefox_base(true);
}
static const char _data_FX_MODE_TWINKLECAT[] PROGMEM = "Twinklecat@!,Twinkle rate,,,,Cool;!,!;!";


uint16_t mode_halloween_eyes()
{
  enum eyeState : uint8_t {
    initializeOn = 0,
    on,
    blink,
    initializeOff,
    off,

    count
  };
  struct EyeData {
    eyeState state;
    uint8_t color;
    uint16_t startPos;
    // duration + endTime could theoretically be replaced by a single endTime, however we would lose
    // the ability to end the animation early when the user reduces the animation time.
    uint16_t duration;
    uint32_t startTime;
    uint32_t blinkEndTime;
  };

  if (SEGLEN <= 1) return mode_static();
  const unsigned maxWidth = strip.isMatrix ? SEG_W : SEGLEN;
  const unsigned HALLOWEEN_EYE_SPACE = MAX(2, strip.isMatrix ? SEG_W>>4: SEGLEN>>5);
  const unsigned HALLOWEEN_EYE_WIDTH = HALLOWEEN_EYE_SPACE/2;
  unsigned eyeLength = (2*HALLOWEEN_EYE_WIDTH) + HALLOWEEN_EYE_SPACE;
  if (eyeLength >= maxWidth) return mode_static(); //bail if segment too short

  if (!SEGENV.allocateData(sizeof(EyeData))) return mode_static(); //allocation failed
  EyeData& data = *reinterpret_cast<EyeData*>(SEGENV.data);

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1)); //fill background

  data.state = static_cast<eyeState>(data.state % eyeState::count);
  unsigned duration = max(uint16_t{1u}, data.duration);
  const uint32_t elapsedTime = strip.now - data.startTime;

  switch (data.state) {
    case eyeState::initializeOn: {
      // initialize the eyes-on state:
      // - select eye position and color
      // - select a duration
      // - immediately switch to eyes on state.

      data.startPos = hw_random16(0, maxWidth - eyeLength - 1);
      data.color = hw_random8();
      if (strip.isMatrix) SEGMENT.offset = hw_random16(SEG_H-1); // a hack: reuse offset since it is not used in matrices
      duration = 128u + hw_random16(SEGMENT.intensity*64u);
      data.duration = duration;
      data.state = eyeState::on;
      [[fallthrough]];
    }
    case eyeState::on: {
      // eyes-on steate:
      // - fade eyes in for some time
      // - keep eyes on until the pre-selected duration is over
      // - randomly switch to the blink (sub-)state, and initialize it with a blink duration (more precisely, a blink end time stamp)
      // - never switch to the blink state if the animation just started or is about to end

      unsigned start2ndEye = data.startPos + HALLOWEEN_EYE_WIDTH + HALLOWEEN_EYE_SPACE;
      // If the user reduces the input while in this state, limit the duration.
      duration = min(duration, (128u + (SEGMENT.intensity * 64u)));

      constexpr uint32_t minimumOnTimeBegin = 1024u;
      constexpr uint32_t minimumOnTimeEnd = 1024u;
      const uint32_t fadeInAnimationState = elapsedTime * uint32_t{256u * 8u} / duration;
      const uint32_t backgroundColor = SEGCOLOR(1);
      const uint32_t eyeColor = SEGMENT.color_from_palette(data.color, false, false, 0);
      uint32_t c = eyeColor;
      if (fadeInAnimationState < 256u) {
        c = color_blend(backgroundColor, eyeColor, uint8_t(fadeInAnimationState));
      } else if (elapsedTime > minimumOnTimeBegin) {
        const uint32_t remainingTime = (elapsedTime >= duration) ? 0u : (duration - elapsedTime);
        if (remainingTime > minimumOnTimeEnd) {
          if (hw_random8() < 4u)
          {
            c = backgroundColor;
            data.state = eyeState::blink;
            data.blinkEndTime = strip.now + hw_random8(8, 128);
          }
        }
      }

      if (c != backgroundColor) {
        // render eyes
        for (unsigned i = 0; i < HALLOWEEN_EYE_WIDTH; i++) {
          if (strip.isMatrix) {
            SEGMENT.setPixelColorXY(data.startPos + i, (unsigned)SEGMENT.offset, c);
            SEGMENT.setPixelColorXY(start2ndEye   + i, (unsigned)SEGMENT.offset, c);
          } else {
            SEGMENT.setPixelColor(data.startPos + i, c);
            SEGMENT.setPixelColor(start2ndEye   + i, c);
          }
        }
      }
      break;
    }
    case eyeState::blink: {
      // eyes-on but currently blinking state:
      // - wait until the blink time is over, then switch back to eyes-on

      if (strip.now >= data.blinkEndTime) {
        data.state = eyeState::on;
      }
      break;
    }
    case eyeState::initializeOff: {
      // initialize eyes-off state:
      // - select a duration
      // - immediately switch to eyes-off state

      const unsigned eyeOffTimeBase = SEGMENT.speed*128u;
      duration = eyeOffTimeBase + hw_random16(eyeOffTimeBase);
      data.duration = duration;
      data.state = eyeState::off;
      [[fallthrough]];
    }
    case eyeState::off: {
      // eyes-off state:
      // - not much to do here

      // If the user reduces the input while in this state, limit the duration.
      const unsigned eyeOffTimeBase = SEGMENT.speed*128u;
      duration = min(duration, (2u * eyeOffTimeBase));
      break;
    }
    case eyeState::count: {
      // Can't happen, not an actual state.
      data.state = eyeState::initializeOn;
      break;
    }
  }

  if (elapsedTime > duration) {
    // The current state duration is over, switch to the next state.
    switch (data.state) {
      case eyeState::initializeOn:
      case eyeState::on:
      case eyeState::blink:
        data.state = eyeState::initializeOff;
        break;
      case eyeState::initializeOff:
      case eyeState::off:
      case eyeState::count:
      default:
        data.state = eyeState::initializeOn;
        break;
    }
    data.startTime = strip.now;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_HALLOWEEN_EYES[] PROGMEM = "Halloween Eyes@Eye off time,Eye on time,,,,,Overlay;!,!;!;12";


//Speed slider sets amount of LEDs lit, intensity sets unlit
uint16_t mode_static_pattern()
{
  unsigned lit = 1 + SEGMENT.speed;
  unsigned unlit = 1 + SEGMENT.intensity;
  bool drawingLit = true;
  unsigned cnt = 0;

  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, (drawingLit) ? SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0) : SEGCOLOR(1));
    cnt++;
    if (cnt >= ((drawingLit) ? lit : unlit)) {
      cnt = 0;
      drawingLit = !drawingLit;
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_STATIC_PATTERN[] PROGMEM = "Solid Pattern@Fg size,Bg size;Fg,!;!;;pal=0";


uint16_t mode_tri_static_pattern()
{
  unsigned segSize = (SEGMENT.intensity >> 5) +1;
  unsigned currSeg = 0;
  unsigned currSegCount = 0;

  for (unsigned i = 0; i < SEGLEN; i++) {
    if ( currSeg % 3 == 0 ) {
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    } else if( currSeg % 3 == 1) {
      SEGMENT.setPixelColor(i, SEGCOLOR(1));
    } else {
      SEGMENT.setPixelColor(i, SEGCOLOR(2));
    }
    currSegCount += 1;
    if (currSegCount >= segSize) {
      currSeg +=1;
      currSegCount = 0;
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_TRI_STATIC_PATTERN[] PROGMEM = "Solid Pattern Tri@,Size;1,2,3;;;pal=0";


static uint16_t spots_base(uint16_t threshold)
{
  if (SEGLEN <= 1) return mode_static();
  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  unsigned maxZones = SEGLEN >> 2;
  unsigned zones = 1 + ((SEGMENT.intensity * maxZones) >> 8);
  unsigned zoneLen = SEGLEN / zones;
  unsigned offset = (SEGLEN - zones * zoneLen) >> 1;

  for (unsigned z = 0; z < zones; z++)
  {
    unsigned pos = offset + z * zoneLen;
    for (unsigned i = 0; i < zoneLen; i++)
    {
      unsigned wave = triwave16((i * 0xFFFF) / zoneLen);
      if (wave > threshold) {
        unsigned index = 0 + pos + i;
        unsigned s = (wave - threshold)*255 / (0xFFFF - threshold);
        SEGMENT.setPixelColor(index, color_blend(SEGMENT.color_from_palette(index, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), uint8_t(255-s)));
      }
    }
  }

  return FRAMETIME;
}


//Intensity slider sets number of "lights", speed sets LEDs per light
uint16_t mode_spots()
{
  return spots_base((255 - SEGMENT.speed) << 8);
}
static const char _data_FX_MODE_SPOTS[] PROGMEM = "Spots@Spread,Width,,,,,Overlay;!,!;!";


//Intensity slider sets number of "lights", LEDs per light fade in and out
uint16_t mode_spots_fade()
{
  unsigned counter = strip.now * ((SEGMENT.speed >> 2) +8);
  unsigned t = triwave16(counter);
  unsigned tr = (t >> 1) + (t >> 2);
  return spots_base(tr);
}
static const char _data_FX_MODE_SPOTS_FADE[] PROGMEM = "Spots Fade@Spread,Width,,,,,Overlay;!,!;!";

//each needs 12 bytes
typedef struct Ball {
  unsigned long lastBounceTime;
  float impactVelocity;
  float height;
} ball;

/*
*  Bouncing Balls Effect
*/
uint16_t mode_bouncing_balls(void) {
  if (SEGLEN <= 1) return mode_static();
  //allocate segment data
  const unsigned strips = SEGMENT.nrOfVStrips(); // adapt for 2D
  const size_t maxNumBalls = 16;
  unsigned dataSize = sizeof(ball) * maxNumBalls;
  if (!SEGENV.allocateData(dataSize * strips)) return mode_static(); //allocation failed

  Ball* balls = reinterpret_cast<Ball*>(SEGENV.data);

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(2) ? BLACK : SEGCOLOR(1));

  // virtualStrip idea by @ewowi (Ewoud Wijma)
  // requires virtual strip # to be embedded into upper 16 bits of index in setPixelColor()
  // the following functions will not work on virtual strips: fill(), fade_out(), fadeToBlack(), blur()
  struct virtualStrip {
    static void runStrip(size_t stripNr, Ball* balls) {
      // number of balls based on intensity setting to max of 7 (cycles colors)
      // non-chosen color is a random color
      unsigned numBalls = (SEGMENT.intensity * (maxNumBalls - 1)) / 255 + 1; // minimum 1 ball
      const float gravity = -9.81f; // standard value of gravity
      const bool hasCol2 = SEGCOLOR(2);
      const unsigned long time = strip.now;

      if (SEGENV.call == 0) {
        for (size_t i = 0; i < maxNumBalls; i++) balls[i].lastBounceTime = time;
      }

      for (size_t i = 0; i < numBalls; i++) {
        float timeSinceLastBounce = (time - balls[i].lastBounceTime)/((255-SEGMENT.speed)/64 +1);
        float timeSec = timeSinceLastBounce/1000.0f;
        balls[i].height = (0.5f * gravity * timeSec + balls[i].impactVelocity) * timeSec; // avoid use pow(x, 2) - its extremely slow !

        if (balls[i].height <= 0.0f) {
          balls[i].height = 0.0f;
          //damping for better effect using multiple balls
          float dampening = 0.9f - float(i)/float(numBalls * numBalls); // avoid use pow(x, 2) - its extremely slow !
          balls[i].impactVelocity = dampening * balls[i].impactVelocity;
          balls[i].lastBounceTime = time;

          if (balls[i].impactVelocity < 0.015f) {
            float impactVelocityStart = sqrtf(-2.0f * gravity) * hw_random8(5,11)/10.0f; // randomize impact velocity
            balls[i].impactVelocity = impactVelocityStart;
          }
        } else if (balls[i].height > 1.0f) {
          continue; // do not draw OOB ball
        }

        uint32_t color = SEGCOLOR(0);
        if (SEGMENT.palette) {
          color = SEGMENT.color_wheel(i*(256/MAX(numBalls, 8)));
        } else if (hasCol2) {
          color = SEGCOLOR(i % NUM_COLORS);
        }

        int pos = roundf(balls[i].height * (SEGLEN - 1));
        #ifdef WLED_USE_AA_PIXELS
        if (SEGLEN<32) SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color); // encode virtual strip into index
        else           SEGMENT.setPixelColor(balls[i].height + (stripNr+1)*10.0f, color);
        #else
        SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color); // encode virtual strip into index
        #endif
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &balls[stripNr * maxNumBalls]);

  return FRAMETIME;
}
static const char _data_FX_MODE_BOUNCINGBALLS[] PROGMEM = "Bouncing Balls@Gravity,# of balls,,,,,Overlay;!,!,!;!;1;m12=1"; //bar

#ifdef WLED_PS_DONT_REPLACE_FX
/*
 *  bouncing balls on a track track Effect modified from Aircoookie's bouncing balls
 *  Courtesy of pjhatch (https://github.com/pjhatch)
 *  https://github.com/wled-dev/WLED/pull/1039
 */
// modified for balltrack mode
typedef struct RollingBall {
  unsigned long lastBounceUpdate;
  float mass; // could fix this to be = 1. if memory is an issue
  float velocity;
  float height;
} rball_t;

static uint16_t rolling_balls(void) {
  //allocate segment data
  const unsigned maxNumBalls = 16; // 255/16 + 1
  unsigned dataSize = sizeof(rball_t) * maxNumBalls;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  rball_t *balls = reinterpret_cast<rball_t *>(SEGENV.data);

  // number of balls based on intensity setting to max of 16 (cycles colors)
  // non-chosen color is a random color
  unsigned numBalls = SEGMENT.intensity/16 + 1;
  bool hasCol2 = SEGCOLOR(2);

  if (SEGENV.call == 0) {
    SEGMENT.fill(hasCol2 ? BLACK : SEGCOLOR(1));                    // start clean
    for (unsigned i = 0; i < maxNumBalls; i++) {
      balls[i].lastBounceUpdate = strip.now;
      balls[i].velocity = 20.0f * float(hw_random16(1000, 10000))/10000.0f;  // number from 1 to 10
      if (hw_random8()<128) balls[i].velocity = -balls[i].velocity;    // 50% chance of reverse direction
      balls[i].height = (float(hw_random16(0, 10000)) / 10000.0f);     // from 0. to 1.
      balls[i].mass   = (float(hw_random16(1000, 10000)) / 10000.0f);  // from .1 to 1.
    }
  }

  float cfac = float(scale8(8, 255-SEGMENT.speed) +1)*20000.0f; // this uses the Aircoookie conversion factor for scaling time using speed slider

  if (SEGMENT.check3) SEGMENT.fade_out(250); // 2-8 pixel trails (optional)
  else {
  	if (!SEGMENT.check2) SEGMENT.fill(hasCol2 ? BLACK : SEGCOLOR(1)); // don't fill with background color if user wants to see trails
  }

  for (unsigned i = 0; i < numBalls; i++) {
    float timeSinceLastUpdate = float((strip.now - balls[i].lastBounceUpdate))/cfac;
    float thisHeight = balls[i].height + balls[i].velocity * timeSinceLastUpdate; // this method keeps higher resolution
    // test if intensity level was increased and some balls are way off the track then put them back
    if (thisHeight < -0.5f || thisHeight > 1.5f) {
      thisHeight = balls[i].height = (float(hw_random16(0, 10000)) / 10000.0f); // from 0. to 1.
      balls[i].lastBounceUpdate = strip.now;
    }
    // check if reached ends of the strip
    if ((thisHeight <= 0.0f && balls[i].velocity < 0.0f) || (thisHeight >= 1.0f && balls[i].velocity > 0.0f)) {
      balls[i].velocity = -balls[i].velocity; // reverse velocity
      balls[i].lastBounceUpdate = strip.now;
      balls[i].height = thisHeight;
    }
    // check for collisions
    if (SEGMENT.check1) {
      for (unsigned j = i+1; j < numBalls; j++) {
        if (balls[j].velocity != balls[i].velocity) {
          //  tcollided + balls[j].lastBounceUpdate is acutal time of collision (this keeps precision with long to float conversions)
          float tcollided = (cfac*(balls[i].height - balls[j].height) +
                balls[i].velocity*float(balls[j].lastBounceUpdate - balls[i].lastBounceUpdate))/(balls[j].velocity - balls[i].velocity);

          if ((tcollided > 2.0f) && (tcollided < float(strip.now - balls[j].lastBounceUpdate))) { // 2ms minimum to avoid duplicate bounces
            balls[i].height = balls[i].height + balls[i].velocity*(tcollided + float(balls[j].lastBounceUpdate - balls[i].lastBounceUpdate))/cfac;
            balls[j].height = balls[i].height;
            balls[i].lastBounceUpdate = (unsigned long)(tcollided + 0.5f) + balls[j].lastBounceUpdate;
            balls[j].lastBounceUpdate = balls[i].lastBounceUpdate;
            float vtmp = balls[i].velocity;
            balls[i].velocity = ((balls[i].mass - balls[j].mass)*vtmp              + 2.0f*balls[j].mass*balls[j].velocity)/(balls[i].mass + balls[j].mass);
            balls[j].velocity = ((balls[j].mass - balls[i].mass)*balls[j].velocity + 2.0f*balls[i].mass*vtmp)             /(balls[i].mass + balls[j].mass);
            thisHeight = balls[i].height + balls[i].velocity*(strip.now - balls[i].lastBounceUpdate)/cfac;
          }
        }
      }
    }

    uint32_t color = SEGCOLOR(0);
    if (SEGMENT.palette) {
      //color = SEGMENT.color_wheel(i*(256/MAX(numBalls, 8)));
      color = SEGMENT.color_from_palette(i*255/numBalls, false, PALETTE_SOLID_WRAP, 0);
    } else if (hasCol2) {
      color = SEGCOLOR(i % NUM_COLORS);
    }

    if (thisHeight < 0.0f) thisHeight = 0.0f;
    if (thisHeight > 1.0f) thisHeight = 1.0f;
    unsigned pos = round(thisHeight * (SEGLEN - 1));
    SEGMENT.setPixelColor(pos, color);
    balls[i].lastBounceUpdate = strip.now;
    balls[i].height = thisHeight;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_ROLLINGBALLS[] PROGMEM = "Rolling Balls@!,# of balls,,,,Collide,Overlay,Trails;!,!,!;!;1;m12=1"; //bar
#endif // WLED_PS_DONT_REPLACE_FX

/*
* Sinelon stolen from FASTLED examples
*/
static uint16_t sinelon_base(bool dual, bool rainbow=false) {
  if (SEGLEN <= 1) return mode_static();
  SEGMENT.fade_out(SEGMENT.intensity);
  unsigned pos = beatsin16_t(SEGMENT.speed/10,0,SEGLEN-1);
  if (SEGENV.call == 0) SEGENV.aux0 = pos;
  uint32_t color1 = SEGMENT.color_from_palette(pos, true, false, 0);
  uint32_t color2 = SEGCOLOR(2);
  if (rainbow) {
    color1 = SEGMENT.color_wheel((pos & 0x07) * 32);
  }
  SEGMENT.setPixelColor(pos, color1);
  if (dual) {
    if (!color2) color2 = SEGMENT.color_from_palette(pos, true, false, 0);
    if (rainbow) color2 = color1; //rainbow
    SEGMENT.setPixelColor(SEGLEN-1-pos, color2);
  }
  if (SEGENV.aux0 != pos) {
    if (SEGENV.aux0 < pos) {
      for (unsigned i = SEGENV.aux0; i < pos ; i++) {
        SEGMENT.setPixelColor(i, color1);
        if (dual) SEGMENT.setPixelColor(SEGLEN-1-i, color2);
      }
    } else {
      for (unsigned i = SEGENV.aux0; i > pos ; i--) {
        SEGMENT.setPixelColor(i, color1);
        if (dual) SEGMENT.setPixelColor(SEGLEN-1-i, color2);
      }
    }
    SEGENV.aux0 = pos;
  }

  return FRAMETIME;
}


uint16_t mode_sinelon(void) {
  return sinelon_base(false);
}
static const char _data_FX_MODE_SINELON[] PROGMEM = "Sinelon@!,Trail;!,!,!;!";


uint16_t mode_sinelon_dual(void) {
  return sinelon_base(true);
}
static const char _data_FX_MODE_SINELON_DUAL[] PROGMEM = "Sinelon Dual@!,Trail;!,!,!;!";


uint16_t mode_sinelon_rainbow(void) {
  return sinelon_base(false, true);
}
static const char _data_FX_MODE_SINELON_RAINBOW[] PROGMEM = "Sinelon Rainbow@!,Trail;,,!;!";


// utility function that will add random glitter to SEGMENT
void glitter_base(uint8_t intensity, uint32_t col = ULTRAWHITE) {
  if (intensity > hw_random8()) SEGMENT.setPixelColor(hw_random16(SEGLEN), col);
}

//Glitter with palette background, inspired by https://gist.github.com/kriegsman/062e10f7f07ba8518af6
uint16_t mode_glitter()
{
  if (!SEGMENT.check2) { // use "* Color 1" palette for solid background (replacing "Solid glitter")
    unsigned counter = 0;
    if (SEGMENT.speed != 0) {
      counter = (strip.now * ((SEGMENT.speed >> 3) +1)) & 0xFFFF;
      counter = counter >> 8;
    }

    bool noWrap = (strip.paletteBlend == 2 || (strip.paletteBlend == 0 && SEGMENT.speed == 0));
    for (unsigned i = 0; i < SEGLEN; i++) {
      unsigned colorIndex = (i * 255 / SEGLEN) - counter;
      if (noWrap) colorIndex = map(colorIndex, 0, 255, 0, 240); //cut off blend at palette "end"
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(colorIndex, false, true, 255));
    }
  }
  glitter_base(SEGMENT.intensity, SEGCOLOR(2) ? SEGCOLOR(2) : ULTRAWHITE);
  return FRAMETIME;
}
static const char _data_FX_MODE_GLITTER[] PROGMEM = "Glitter@!,!,,,,,Overlay;,,Glitter color;!;;pal=11,m12=0"; //pixels


//Solid colour background with glitter (can be replaced by Glitter)
uint16_t mode_solid_glitter()
{
  SEGMENT.fill(SEGCOLOR(0));
  glitter_base(SEGMENT.intensity, SEGCOLOR(2) ? SEGCOLOR(2) : ULTRAWHITE);
  return FRAMETIME;
}
static const char _data_FX_MODE_SOLID_GLITTER[] PROGMEM = "Solid Glitter@,!;Bg,,Glitter color;;;m12=0";

//each needs 20 bytes
//Spark type is used for popcorn, 1D fireworks, and drip
typedef struct Spark {
  float pos, posX;
  float vel, velX;
  uint16_t col;
  uint8_t colIndex;
} spark;

#define maxNumPopcorn 21 // max 21 on 16 segment ESP8266
/*
*  POPCORN
*  modified from https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Popcorn.h
*/
uint16_t mode_popcorn(void) {
  if (SEGLEN <= 1) return mode_static();
  //allocate segment data
  unsigned strips = SEGMENT.nrOfVStrips();
  unsigned usablePopcorns = maxNumPopcorn;
  if (usablePopcorns * strips * sizeof(spark) > FAIR_DATA_PER_SEG) usablePopcorns = FAIR_DATA_PER_SEG / (strips * sizeof(spark)) + 1; // at least 1 popcorn per vstrip
  unsigned dataSize = sizeof(spark) * usablePopcorns; // on a matrix 64x64 this could consume a little less than 27kB when Bar expansion is used
  if (!SEGENV.allocateData(dataSize * strips)) return mode_static(); //allocation failed

  Spark* popcorn = reinterpret_cast<Spark*>(SEGENV.data);

  bool hasCol2 = SEGCOLOR(2);
  if (!SEGMENT.check2) SEGMENT.fill(hasCol2 ? BLACK : SEGCOLOR(1));

  struct virtualStrip {
    static void runStrip(uint16_t stripNr, Spark* popcorn, unsigned usablePopcorns) {
      float gravity = -0.0001f - (SEGMENT.speed/200000.0f); // m/s/s
      gravity *= SEGLEN;

      unsigned numPopcorn = SEGMENT.intensity * usablePopcorns / 255;
      if (numPopcorn == 0) numPopcorn = 1;

      for (unsigned i = 0; i < numPopcorn; i++) {
        if (popcorn[i].pos >= 0.0f) { // if kernel is active, update its position
          popcorn[i].pos += popcorn[i].vel;
          popcorn[i].vel += gravity;
        } else { // if kernel is inactive, randomly pop it
          if (hw_random8() < 2) { // POP!!!
            popcorn[i].pos = 0.01f;

            unsigned peakHeight = 128 + hw_random8(128); //0-255
            peakHeight = (peakHeight * (SEGLEN -1)) >> 8;
            popcorn[i].vel = sqrtf(-2.0f * gravity * peakHeight);

            if (SEGMENT.palette)
            {
              popcorn[i].colIndex = hw_random8();
            } else {
              byte col = hw_random8(0, NUM_COLORS);
              if (!SEGCOLOR(2) || !SEGCOLOR(col)) col = 0;
              popcorn[i].colIndex = col;
            }
          }
        }
        if (popcorn[i].pos >= 0.0f) { // draw now active popcorn (either active before or just popped)
          uint32_t col = SEGMENT.color_wheel(popcorn[i].colIndex);
          if (!SEGMENT.palette && popcorn[i].colIndex < NUM_COLORS) col = SEGCOLOR(popcorn[i].colIndex);
          unsigned ledIndex = popcorn[i].pos;
          if (ledIndex < SEGLEN) SEGMENT.setPixelColor(indexToVStrip(ledIndex, stripNr), col);
        }
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &popcorn[stripNr * usablePopcorns], usablePopcorns);

  return FRAMETIME;
}
static const char _data_FX_MODE_POPCORN[] PROGMEM = "Popcorn@!,!,,,,,Overlay;!,!,!;!;;m12=1"; //bar

//values close to 100 produce 5Hz flicker, which looks very candle-y
//Inspired by https://github.com/avanhanegem/ArduinoCandleEffectNeoPixel
//and https://cpldcpu.wordpress.com/2016/01/05/reverse-engineering-a-real-candle/

uint16_t candle(bool multi)
{
  if (multi && SEGLEN > 1) {
    //allocate segment data
    unsigned dataSize = max(1, (int)SEGLEN -1) *3; //max. 1365 pixels (ESP8266)
    if (!SEGENV.allocateData(dataSize)) return candle(false); //allocation failed
  }

  //max. flicker range controlled by intensity
  unsigned valrange = SEGMENT.intensity;
  unsigned rndval = valrange >> 1; //max 127

  //step (how much to move closer to target per frame) coarsely set by speed
  unsigned speedFactor = 4;
  if (SEGMENT.speed > 252) { //epilepsy
    speedFactor = 1;
  } else if (SEGMENT.speed > 99) { //regular candle (mode called every ~25 ms, so 4 frames to have a new target every 100ms)
    speedFactor = 2;
  } else if (SEGMENT.speed > 49) { //slower fade
    speedFactor = 3;
  } //else 4 (slowest)

  unsigned numCandles = (multi) ? SEGLEN : 1;

  for (unsigned i = 0; i < numCandles; i++)
  {
    unsigned d = 0; //data location

    unsigned s = SEGENV.aux0, s_target = SEGENV.aux1, fadeStep = SEGENV.step;
    if (i > 0) {
      d = (i-1) *3;
      s = SEGENV.data[d]; s_target = SEGENV.data[d+1]; fadeStep = SEGENV.data[d+2];
    }
    if (fadeStep == 0) { //init vals
      s = 128; s_target = 130 + hw_random8(4); fadeStep = 1;
    }

    bool newTarget = false;
    if (s_target > s) { //fade up
      s = qadd8(s, fadeStep);
      if (s >= s_target) newTarget = true;
    } else {
      s = qsub8(s, fadeStep);
      if (s <= s_target) newTarget = true;
    }

    if (newTarget) {
      s_target = hw_random8(rndval) + hw_random8(rndval); //between 0 and rndval*2 -2 = 252
      if (s_target < (rndval >> 1)) s_target = (rndval >> 1) + hw_random8(rndval);
      unsigned offset = (255 - valrange);
      s_target += offset;

      unsigned dif = (s_target > s) ? s_target - s : s - s_target;

      fadeStep = dif >> speedFactor;
      if (fadeStep == 0) fadeStep = 1;
    }

    if (i > 0) {
      SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), uint8_t(s)));

      SEGENV.data[d] = s; SEGENV.data[d+1] = s_target; SEGENV.data[d+2] = fadeStep;
    } else {
      for (unsigned j = 0; j < SEGLEN; j++) {
        SEGMENT.setPixelColor(j, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(j, true, PALETTE_SOLID_WRAP, 0), uint8_t(s)));
      }

      SEGENV.aux0 = s; SEGENV.aux1 = s_target; SEGENV.step = fadeStep;
    }
  }

  return FRAMETIME_FIXED;
}


uint16_t mode_candle()
{
  return candle(false);
}
static const char _data_FX_MODE_CANDLE[] PROGMEM = "Candle@!,!;!,!;!;01;sx=96,ix=224,pal=0";


uint16_t mode_candle_multi()
{
  return candle(true);
}
static const char _data_FX_MODE_CANDLE_MULTI[] PROGMEM = "Candle Multi@!,!;!,!;!;;sx=96,ix=224,pal=0";

#ifdef WLED_PS_DONT_REPLACE_FX
/*
/ Fireworks in starburst effect
/ based on the video: https://www.reddit.com/r/arduino/comments/c3sd46/i_made_this_fireworks_effect_for_my_led_strips/
/ Speed sets frequency of new starbursts, intensity is the intensity of the burst
*/
#ifdef ESP8266
  #define STARBURST_MAX_FRAG   8 //52 bytes / star
#else
  #define STARBURST_MAX_FRAG  10 //60 bytes / star
#endif
//each needs 20+STARBURST_MAX_FRAG*4 bytes
typedef struct particle {
  CRGB     color;
  uint32_t birth  =0;
  uint32_t last   =0;
  float    vel    =0;
  uint16_t pos    =-1;
  float    fragment[STARBURST_MAX_FRAG];
} star;

uint16_t mode_starburst(void) {
  if (SEGLEN <= 1) return mode_static();
  unsigned maxData = FAIR_DATA_PER_SEG; //ESP8266: 256 ESP32: 640
  unsigned segs = strip.getActiveSegmentsNum();
  if (segs <= (strip.getMaxSegments() /2)) maxData *= 2; //ESP8266: 512 if <= 8 segs ESP32: 1280 if <= 16 segs
  if (segs <= (strip.getMaxSegments() /4)) maxData *= 2; //ESP8266: 1024 if <= 4 segs ESP32: 2560 if <= 8 segs
  unsigned maxStars = maxData / sizeof(star); //ESP8266: max. 4/9/19 stars/seg, ESP32: max. 10/21/42 stars/seg

  unsigned numStars = 1 + (SEGLEN >> 3);
  if (numStars > maxStars) numStars = maxStars;
  unsigned dataSize = sizeof(star) * numStars;

  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  uint32_t it = strip.now;

  star* stars = reinterpret_cast<star*>(SEGENV.data);

  float          maxSpeed                = 375.0f;  // Max velocity
  float          particleIgnition        = 250.0f;  // How long to "flash"
  float          particleFadeTime        = 1500.0f; // Fade out time

  for (unsigned j = 0; j < numStars; j++)
  {
    // speed to adjust chance of a burst, max is nearly always.
    if (hw_random8((144-(SEGMENT.speed >> 1))) == 0 && stars[j].birth == 0)
    {
      // Pick a random color and location.
      unsigned startPos = hw_random16(SEGLEN-1);
      float multiplier = (float)(hw_random8())/255.0f * 1.0f;

      stars[j].color = CRGB(SEGMENT.color_wheel(hw_random8()));
      stars[j].pos = startPos;
      stars[j].vel = maxSpeed * (float)(hw_random8())/255.0f * multiplier;
      stars[j].birth = it;
      stars[j].last = it;
      // more fragments means larger burst effect
      int num = hw_random8(3,6 + (SEGMENT.intensity >> 5));

      for (int i=0; i < STARBURST_MAX_FRAG; i++) {
        if (i < num) stars[j].fragment[i] = startPos;
        else stars[j].fragment[i] = -1;
      }
    }
  }

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  for (unsigned j=0; j<numStars; j++)
  {
    if (stars[j].birth != 0) {
      float dt = (it-stars[j].last)/1000.0;

      for (int i=0; i < STARBURST_MAX_FRAG; i++) {
        int var = i >> 1;

        if (stars[j].fragment[i] > 0) {
          //all fragments travel right, will be mirrored on other side
          stars[j].fragment[i] += stars[j].vel * dt * (float)var/3.0;
        }
      }
      stars[j].last = it;
      stars[j].vel -= 3*stars[j].vel*dt;
    }

    CRGB c = stars[j].color;

    // If the star is brand new, it flashes white briefly.
    // Otherwise it just fades over time.
    float fade = 0.0f;
    float age = it-stars[j].birth;

    if (age < particleIgnition) {
      c = CRGB(color_blend(WHITE, RGBW32(c.r,c.g,c.b,0), uint8_t(254.5f*((age / particleIgnition)))));
    } else {
      // Figure out how much to fade and shrink the star based on
      // its age relative to its lifetime
      if (age > particleIgnition + particleFadeTime) {
        fade = 1.0f;                  // Black hole, all faded out
        stars[j].birth = 0;
        c = CRGB(SEGCOLOR(1));
      } else {
        age -= particleIgnition;
        fade = (age / particleFadeTime);  // Fading star
        c = CRGB(color_blend(RGBW32(c.r,c.g,c.b,0), SEGCOLOR(1), uint8_t(254.5f*fade)));
      }
    }

    float particleSize = (1.0f - fade) * 2.0f;

    for (size_t index=0; index < STARBURST_MAX_FRAG*2; index++) {
      bool mirrored = index & 0x1;
      unsigned i = index >> 1;
      if (stars[j].fragment[i] > 0) {
        float loc = stars[j].fragment[i];
        if (mirrored) loc -= (loc-stars[j].pos)*2;
        unsigned start = loc - particleSize;
        unsigned end = loc + particleSize;
        if (start < 0) start = 0;
        if (start == end) end++;
        if (end > SEGLEN) end = SEGLEN;
        for (unsigned p = start; p < end; p++) {
          SEGMENT.setPixelColor(p, c);
        }
      }
    }
  }
  return FRAMETIME;
}
#undef STARBURST_MAX_FRAG
static const char _data_FX_MODE_STARBURST[] PROGMEM = "Fireworks Starburst@Chance,Fragments,,,,,Overlay;,!;!;;pal=11,m12=0";
#endif // WLED_PS_DONT_REPLACE_FX

 #ifdef WLED_PS_DONT_REPLACE_FX
/*
 * Exploding fireworks effect
 * adapted from: http://www.anirama.com/1000leds/1d-fireworks/
 * adapted for 2D WLED by blazoncek (Blaz Kristan (AKA blazoncek))
 */
uint16_t mode_exploding_fireworks(void)
{
  if (SEGLEN <= 1) return mode_static();
  const int cols = SEGMENT.is2D() ? SEG_W : 1;
  const int rows = SEGMENT.is2D() ? SEG_H : SEGLEN;

  //allocate segment data
  unsigned maxData = FAIR_DATA_PER_SEG; //ESP8266: 256 ESP32: 640
  unsigned segs = strip.getActiveSegmentsNum();
  if (segs <= (strip.getMaxSegments() /2)) maxData *= 2; //ESP8266: 512 if <= 8 segs ESP32: 1280 if <= 16 segs
  if (segs <= (strip.getMaxSegments() /4)) maxData *= 2; //ESP8266: 1024 if <= 4 segs ESP32: 2560 if <= 8 segs
  int maxSparks = maxData / sizeof(spark); //ESP8266: max. 21/42/85 sparks/seg, ESP32: max. 53/106/213 sparks/seg

  unsigned numSparks = min(5 + ((rows*cols) >> 1), maxSparks);
  unsigned dataSize = sizeof(spark) * numSparks;
  if (!SEGENV.allocateData(dataSize + sizeof(float))) return mode_static(); //allocation failed
  float *dying_gravity = reinterpret_cast<float*>(SEGENV.data + dataSize);

  if (dataSize != SEGENV.aux1) { //reset to flare if sparks were reallocated (it may be good idea to reset segment if bounds change)
    *dying_gravity = 0.0f;
    SEGENV.aux0 = 0;
    SEGENV.aux1 = dataSize;
  }

  SEGMENT.fade_out(252);

  Spark* sparks = reinterpret_cast<Spark*>(SEGENV.data);
  Spark* flare = sparks; //first spark is flare data

  float gravity = -0.0004f - (SEGMENT.speed/800000.0f); // m/s/s
  gravity *= rows;

  if (SEGENV.aux0 < 2) { //FLARE
    if (SEGENV.aux0 == 0) { //init flare
      flare->pos = 0;
      flare->posX = SEGMENT.is2D() ? hw_random16(2,cols-3) : (SEGMENT.intensity > hw_random8()); // will enable random firing side on 1D
      unsigned peakHeight = 75 + hw_random8(180); //0-255
      peakHeight = (peakHeight * (rows -1)) >> 8;
      flare->vel = sqrtf(-2.0f * gravity * peakHeight);
      flare->velX = SEGMENT.is2D() ? (hw_random8(9)-4)/64.0f : 0; // no X velocity on 1D
      flare->col = 255; //brightness
      SEGENV.aux0 = 1;
    }

    // launch
    if (flare->vel > 12 * gravity) {
      // flare
      if (SEGMENT.is2D()) SEGMENT.setPixelColorXY(unsigned(flare->posX), rows - uint16_t(flare->pos) - 1, flare->col, flare->col, flare->col);
      else                SEGMENT.setPixelColor((flare->posX > 0.0f) ? rows - int(flare->pos) - 1 : int(flare->pos), flare->col, flare->col, flare->col);
      flare->pos  += flare->vel;
      flare->pos  = constrain(flare->pos, 0, rows-1);
      if (SEGMENT.is2D()) {
        flare->posX += flare->velX;
        flare->posX = constrain(flare->posX, 0, cols-1);
      }
      flare->vel  += gravity;
      flare->col  -= 2;
    } else {
      SEGENV.aux0 = 2;  // ready to explode
    }
  } else if (SEGENV.aux0 < 4) {
    /*
     * Explode!
     *
     * Explosion happens where the flare ended.
     * Size is proportional to the height.
     */
    unsigned nSparks = flare->pos + hw_random8(4);
    nSparks = std::max(nSparks, 4U);  // This is not a standard constrain; numSparks is not guaranteed to be at least 4
    nSparks = std::min(nSparks, numSparks);

    // initialize sparks
    if (SEGENV.aux0 == 2) {
      for (unsigned i = 1; i < nSparks; i++) {
        sparks[i].pos  = flare->pos;
        sparks[i].posX = flare->posX;
        sparks[i].vel  = (float(hw_random16(20001)) / 10000.0f) - 0.9f; // from -0.9 to 1.1
        sparks[i].vel *= rows<32 ? 0.5f : 1; // reduce velocity for smaller strips
        sparks[i].velX = SEGMENT.is2D() ? (float(hw_random16(20001)) / 10000.0f) - 1.0f : 0; // from -1 to 1
        sparks[i].col  = 345;//abs(sparks[i].vel * 750.0); // set colors before scaling velocity to keep them bright
        //sparks[i].col = constrain(sparks[i].col, 0, 345);
        sparks[i].colIndex = hw_random8();
        sparks[i].vel  *= flare->pos/rows; // proportional to height
        sparks[i].velX *= SEGMENT.is2D() ? flare->posX/cols : 0; // proportional to width
        sparks[i].vel  *= -gravity *50;
      }
      //sparks[1].col = 345; // this will be our known spark
      *dying_gravity = gravity/2;
      SEGENV.aux0 = 3;
    }

    if (sparks[1].col > 4) {//&& sparks[1].pos > 0) { // as long as our known spark is lit, work with all the sparks
      for (unsigned i = 1; i < nSparks; i++) {
        sparks[i].pos  += sparks[i].vel;
        sparks[i].posX += sparks[i].velX;
        sparks[i].vel  += *dying_gravity;
        sparks[i].velX += SEGMENT.is2D() ? *dying_gravity : 0;
        if (sparks[i].col > 3) sparks[i].col -= 4;

        if (sparks[i].pos > 0 && sparks[i].pos < rows) {
          if (SEGMENT.is2D() && !(sparks[i].posX >= 0 && sparks[i].posX < cols)) continue;
          unsigned prog = sparks[i].col;
          uint32_t spColor = (SEGMENT.palette) ? SEGMENT.color_wheel(sparks[i].colIndex) : SEGCOLOR(0);
          CRGBW c = BLACK; //HeatColor(sparks[i].col);
          if (prog > 300) { //fade from white to spark color
            c = color_blend(spColor, WHITE, uint8_t((prog - 300)*5));
          } else if (prog > 45) { //fade from spark color to black
            c = color_blend(BLACK, spColor, uint8_t(prog - 45));
            unsigned cooling = (300 - prog) >> 5;
            c.g = qsub8(c.g, cooling);
            c.b = qsub8(c.b, cooling * 2);
          }
          if (SEGMENT.is2D()) SEGMENT.setPixelColorXY(int(sparks[i].posX), rows - int(sparks[i].pos) - 1, c);
          else                SEGMENT.setPixelColor(int(sparks[i].posX) ? rows - int(sparks[i].pos) - 1 : int(sparks[i].pos), c);
        }
      }
      if (SEGMENT.check3) SEGMENT.blur(16);
      *dying_gravity *= .8f; // as sparks burn out they fall slower
    } else {
      SEGENV.aux0 = 6 + hw_random8(10); //wait for this many frames
    }
  } else {
    SEGENV.aux0--;
    if (SEGENV.aux0 < 4) {
      SEGENV.aux0 = 0; //back to flare
    }
  }

  return FRAMETIME;
}
#undef MAX_SPARKS
static const char _data_FX_MODE_EXPLODING_FIREWORKS[] PROGMEM = "Fireworks 1D@Gravity,Firing side;!,!;!;12;pal=11,ix=128";
#endif // WLED_PS_DONT_REPLACE_FX

/*
 * Drip Effect
 * ported of: https://www.youtube.com/watch?v=sru2fXh4r7k
 */
uint16_t mode_drip(void)
{
  if (SEGLEN <= 1) return mode_static();
  //allocate segment data
  unsigned strips = SEGMENT.nrOfVStrips();
  const int maxNumDrops = 4;
  unsigned dataSize = sizeof(spark) * maxNumDrops;
  if (!SEGENV.allocateData(dataSize * strips)) return mode_static(); //allocation failed
  Spark* drops = reinterpret_cast<Spark*>(SEGENV.data);

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  struct virtualStrip {
    static void runStrip(uint16_t stripNr, Spark* drops) {

      unsigned numDrops = 1 + (SEGMENT.intensity >> 6); // 255>>6 = 3

      float gravity = -0.0005f - (SEGMENT.speed/50000.0f);
      gravity *= max(1, (int)SEGLEN-1);
      int sourcedrop = 12;

      for (unsigned j=0;j<numDrops;j++) {
        if (drops[j].colIndex == 0) { //init
          drops[j].pos = SEGLEN-1;    // start at end
          drops[j].vel = 0;           // speed
          drops[j].col = sourcedrop;  // brightness
          drops[j].colIndex = 1;      // drop state (0 init, 1 forming, 2 falling, 5 bouncing)
        }

        SEGMENT.setPixelColor(indexToVStrip(SEGLEN-1, stripNr), color_blend(BLACK,SEGCOLOR(0), uint8_t(sourcedrop)));// water source
        if (drops[j].colIndex==1) {
          if (drops[j].col>255) drops[j].col=255;
          SEGMENT.setPixelColor(indexToVStrip(uint16_t(drops[j].pos), stripNr), color_blend(BLACK,SEGCOLOR(0),uint8_t(drops[j].col)));

          drops[j].col += map(SEGMENT.speed, 0, 255, 1, 6); // swelling

          if (hw_random8() < drops[j].col/10) {               // random drop
            drops[j].colIndex=2;               //fall
            drops[j].col=255;
          }
        }
        if (drops[j].colIndex > 1) {           // falling
          if (drops[j].pos > 0) {              // fall until end of segment
            drops[j].pos += drops[j].vel;
            if (drops[j].pos < 0) drops[j].pos = 0;
            drops[j].vel += gravity;           // gravity is negative

            for (int i=1;i<7-drops[j].colIndex;i++) { // some minor math so we don't expand bouncing droplets
              unsigned pos = constrain(unsigned(drops[j].pos) +i, 0, SEGLEN-1); //this is BAD, returns a pos >= SEGLEN occasionally
              SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color_blend(BLACK,SEGCOLOR(0),uint8_t(drops[j].col/i))); //spread pixel with fade while falling
            }

            if (drops[j].colIndex > 2) {       // during bounce, some water is on the floor
              SEGMENT.setPixelColor(indexToVStrip(0, stripNr), color_blend(SEGCOLOR(0),BLACK,uint8_t(drops[j].col)));
            }
          } else {                             // we hit bottom
            if (drops[j].colIndex > 2) {       // already hit once, so back to forming
              drops[j].colIndex = 0;
              drops[j].col = sourcedrop;

            } else {

              if (drops[j].colIndex==2) {      // init bounce
                drops[j].vel = -drops[j].vel/4;// reverse velocity with damping
                drops[j].pos += drops[j].vel;
              }
              drops[j].col = sourcedrop*2;
              drops[j].colIndex = 5;           // bouncing
            }
          }
        }
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &drops[stripNr*maxNumDrops]);

  return FRAMETIME;
}
static const char _data_FX_MODE_DRIP[] PROGMEM = "Drip@Gravity,# of drips,,,,,Overlay;!,!;!;;m12=1"; //bar

/*
 * Tetris or Stacking (falling bricks) Effect
 * by Blaz Kristan (AKA blazoncek) (https://github.com/blazoncek, https://blaz.at/home)
 */
//20 bytes
typedef struct Tetris {
  float    pos;
  float    speed;
  uint8_t  col;   // color index
  uint16_t brick; // brick size in pixels
  uint16_t stack; // stack size in pixels
  uint32_t step;  // 2D-fication of SEGENV.step (state)
} tetris;

uint16_t mode_tetrix(void) {
  if (SEGLEN <= 1) return mode_static();
  unsigned strips = SEGMENT.nrOfVStrips(); // allow running on virtual strips (columns in 2D segment)
  unsigned dataSize = sizeof(tetris);
  if (!SEGENV.allocateData(dataSize * strips)) return mode_static(); //allocation failed
  Tetris* drops = reinterpret_cast<Tetris*>(SEGENV.data);

  //if (SEGENV.call == 0) SEGMENT.fill(SEGCOLOR(1));  // will fill entire segment (1D or 2D), then use drop->step = 0 below

  // virtualStrip idea by @ewowi (Ewoud Wijma)
  // requires virtual strip # to be embedded into upper 16 bits of index in setPixelcolor()
  // the following functions will not work on virtual strips: fill(), fade_out(), fadeToBlack(), blur()
  struct virtualStrip {
    static void runStrip(size_t stripNr, Tetris *drop) {
      // initialize dropping on first call or segment full
      if (SEGENV.call == 0) {
        drop->stack = 0;                  // reset brick stack size
        drop->step = strip.now + 2000;     // start by fading out strip
        if (SEGMENT.check1) drop->col = 0;// use only one color from palette
      }

      if (drop->step == 0) {              // init brick
        // speed calculation: a single brick should reach bottom of strip in X seconds
        // if the speed is set to 1 this should take 5s and at 255 it should take 0.25s
        // as this is dependant on SEGLEN it should be taken into account and the fact that effect runs every FRAMETIME s
        int speed = SEGMENT.speed ? SEGMENT.speed : hw_random8(1,255);
        speed = map(speed, 1, 255, 5000, 250); // time taken for full (SEGLEN) drop
        drop->speed = float(SEGLEN * FRAMETIME) / float(speed); // set speed
        drop->pos   = SEGLEN;             // start at end of segment (no need to subtract 1)
        if (!SEGMENT.check1) drop->col = hw_random8(0,15)<<4;   // limit color choices so there is enough HUE gap
        drop->step  = 1;                  // drop state (0 init, 1 forming, 2 falling)
        drop->brick = (SEGMENT.intensity ? (SEGMENT.intensity>>5)+1 : hw_random8(1,5)) * (1+(SEGLEN>>6));  // size of brick
      }

      if (drop->step == 1) {              // forming
        if (hw_random8()>>6) {               // random drop
          drop->step = 2;                 // fall
        }
      }

      if (drop->step == 2) {              // falling
        if (drop->pos > drop->stack) {    // fall until top of stack
          drop->pos -= drop->speed;       // may add gravity as: speed += gravity
          if (int(drop->pos) < int(drop->stack)) drop->pos = drop->stack;
          for (unsigned i = unsigned(drop->pos); i < SEGLEN; i++) {
            uint32_t col = i < unsigned(drop->pos)+drop->brick ? SEGMENT.color_from_palette(drop->col, false, false, 0) : SEGCOLOR(1);
            SEGMENT.setPixelColor(indexToVStrip(i, stripNr), col);
          }
        } else {                          // we hit bottom
          drop->step = 0;                 // proceed with next brick, go back to init
          drop->stack += drop->brick;     // increase the stack size
          if (drop->stack >= SEGLEN) drop->step = strip.now + 2000; // fade out stack
        }
      }

      if (drop->step > 2) {               // fade strip
        drop->brick = 0;                  // reset brick size (no more growing)
        if (drop->step > strip.now) {
          // allow fading of virtual strip
          for (unsigned i = 0; i < SEGLEN; i++) SEGMENT.blendPixelColor(indexToVStrip(i, stripNr), SEGCOLOR(1), 25); // 10% blend
        } else {
          drop->stack = 0;                // reset brick stack size
          drop->step = 0;                 // proceed with next brick
          if (SEGMENT.check1) drop->col += 8;   // gradually increase palette index
        }
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &drops[stripNr]);

  return FRAMETIME;
}
static const char _data_FX_MODE_TETRIX[] PROGMEM = "Tetrix@!,Width,,,,One color;!,!;!;;sx=0,ix=0,pal=11,m12=1";


/*
/ Plasma Effect
/ adapted from https://github.com/atuline/FastLED-Demos/blob/master/plasma/plasma.ino
*/
uint16_t mode_plasma(void) {
  // initialize phases on start
  if (SEGENV.call == 0) {
    SEGENV.aux0 = hw_random8(0,2);  // add a bit of randomness
  }
  unsigned thisPhase = beatsin8_t(6+SEGENV.aux0,-64,64);
  unsigned thatPhase = beatsin8_t(7+SEGENV.aux0,-64,64);

  for (unsigned i = 0; i < SEGLEN; i++) {   // For each of the LED's in the strand, set color &  brightness based on a wave as follows:
    unsigned colorIndex = cubicwave8((i*(2+ 3*(SEGMENT.speed >> 5))+thisPhase) & 0xFF)/2   // factor=23 // Create a wave and add a phase change and add another wave with its own phase change.
                              + cos8_t((i*(1+ 2*(SEGMENT.speed >> 5))+thatPhase) & 0xFF)/2;  // factor=15 // Hey, you can even change the frequencies if you wish.
    unsigned thisBright = qsub8(colorIndex, beatsin8_t(7,0, (128 - (SEGMENT.intensity>>1))));
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(colorIndex, false, PALETTE_SOLID_WRAP, 0, thisBright));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_PLASMA[] PROGMEM = "Plasma@Phase,!;!;!";


/*
 * Percentage display
 * Intensity values from 0-100 turn on the leds.
 */
uint16_t mode_percent(void) {

  unsigned percent = SEGMENT.intensity;
  percent = constrain(percent, 0, 200);
  unsigned active_leds = (percent < 100) ? roundf(SEGLEN * percent / 100.0f)
                                         : roundf(SEGLEN * (200 - percent) / 100.0f);

  unsigned size = (1 + ((SEGMENT.speed * SEGLEN) >> 11));
  if (SEGMENT.speed == 255) size = 255;

  if (percent <= 100) {
    for (unsigned i = 0; i < SEGLEN; i++) {
    	if (i < SEGENV.aux1) {
        if (SEGMENT.check1)
          SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(map(percent,0,100,0,255), false, false, 0));
        else
          SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    	}
    	else {
        SEGMENT.setPixelColor(i, SEGCOLOR(1));
    	}
    }
  } else {
    for (unsigned i = 0; i < SEGLEN; i++) {
    	if (i < (SEGLEN - SEGENV.aux1)) {
        SEGMENT.setPixelColor(i, SEGCOLOR(1));
    	}
    	else {
        if (SEGMENT.check1)
          SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(map(percent,100,200,255,0), false, false, 0));
        else
          SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    	}
    }
  }

  if(active_leds > SEGENV.aux1) {  // smooth transition to the target value
    SEGENV.aux1 += size;
    if (SEGENV.aux1 > active_leds) SEGENV.aux1 = active_leds;
  } else if (active_leds < SEGENV.aux1) {
    if (SEGENV.aux1 > size) SEGENV.aux1 -= size; else SEGENV.aux1 = 0;
    if (SEGENV.aux1 < active_leds) SEGENV.aux1 = active_leds;
  }

 	return FRAMETIME;
}
static const char _data_FX_MODE_PERCENT[] PROGMEM = "Percent@,% of fill,,,,One color;!,!;!";


/*
 * Modulates the brightness similar to a heartbeat
 * (unimplemented?) tries to draw an ECG approximation on a 2D matrix
 */
uint16_t mode_heartbeat(void) {
  unsigned bpm = 40 + (SEGMENT.speed >> 3);
  uint32_t msPerBeat = (60000L / bpm);
  uint32_t secondBeat = (msPerBeat / 3);
  uint32_t bri_lower = SEGENV.aux1;
  unsigned long beatTimer = strip.now - SEGENV.step;

  bri_lower = bri_lower * 2042 / (2048 + SEGMENT.intensity);
  SEGENV.aux1 = bri_lower;

  if ((beatTimer > secondBeat) && !SEGENV.aux0) { // time for the second beat?
    SEGENV.aux1 = UINT16_MAX; //3/4 bri
    SEGENV.aux0 = 1;
  }
  if (beatTimer > msPerBeat) { // time to reset the beat timer?
    SEGENV.aux1 = UINT16_MAX; //full bri
    SEGENV.aux0 = 0;
    SEGENV.step = strip.now;
  }

  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, color_blend(SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), uint8_t(255 - (SEGENV.aux1 >> 8))));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_HEARTBEAT[] PROGMEM = "Heartbeat@!,!;!,!;!;01;m12=1";


//  "Pacifica"
//  Gentle, blue-green ocean waves.
//  December 2019, Mark Kriegsman and Mary Corey March.
//  For Dan.
//
//
// In this animation, there are four "layers" of waves of light.
//
// Each layer moves independently, and each is scaled separately.
//
// All four wave layers are added together on top of each other, and then
// another filter is applied that adds "whitecaps" of brightness where the
// waves line up with each other more.  Finally, another pass is taken
// over the led array to 'deepen' (dim) the blues and greens.
//
// The speed and scale and motion each layer varies slowly within independent
// hand-chosen ranges, which is why the code has a lot of low-speed 'beatsin8' functions
// with a lot of oddly specific numeric ranges.
//
// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https://goo.gl/maps/QQgd97jjHesHZVxQ7
//
// Modified for WLED, based on https://github.com/FastLED/FastLED/blob/master/examples/Pacifica/Pacifica.ino
//
// Add one layer of waves into the led array
static CRGB pacifica_one_layer(uint16_t i, const CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  unsigned ci = cistart;
  unsigned waveangle = ioff;
  unsigned wavescale_half = (wavescale >> 1) + 20;

  waveangle += ((120 + SEGMENT.intensity) * i); //original 250 * i
  unsigned s16 = sin16_t(waveangle) + 32768;
  unsigned cs = scale16(s16, wavescale_half) + wavescale_half;
  ci += (cs * i);
  unsigned sindex16 = sin16_t(ci) + 32768;
  unsigned sindex8 = scale16(sindex16, 240);
  return CRGB(ColorFromPalette(p, sindex8, bri, LINEARBLEND));
}

uint16_t mode_pacifica()
{
  uint32_t nowOld = strip.now;

  CRGBPalette16 pacifica_palette_1 =
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
  CRGBPalette16 pacifica_palette_2 =
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
  CRGBPalette16 pacifica_palette_3 =
    { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
      0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };

  if (SEGMENT.palette) {
    pacifica_palette_1 = SEGPALETTE;
    pacifica_palette_2 = SEGPALETTE;
    pacifica_palette_3 = SEGPALETTE;
  }

  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  unsigned sCIStart1 = SEGENV.aux0, sCIStart2 = SEGENV.aux1, sCIStart3 = SEGENV.step & 0xFFFF, sCIStart4 = (SEGENV.step >> 16);
  uint32_t deltams = (FRAMETIME >> 2) + ((FRAMETIME * SEGMENT.speed) >> 7);
  uint64_t deltat = (strip.now >> 2) + ((strip.now * SEGMENT.speed) >> 7);
  strip.now = deltat;

  unsigned speedfactor1 = beatsin16_t(3, 179, 269);
  unsigned speedfactor2 = beatsin16_t(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88_t(1011,10,13));
  sCIStart2 -= (deltams21 * beatsin88_t(777,8,11));
  sCIStart3 -= (deltams1 * beatsin88_t(501,5,7));
  sCIStart4 -= (deltams2 * beatsin88_t(257,4,6));
  SEGENV.aux0 = sCIStart1; SEGENV.aux1 = sCIStart2;
  SEGENV.step = (sCIStart4 << 16) | (sCIStart3 & 0xFFFF);

  // Clear out the LED array to a dim background blue-green
  //SEGMENT.fill(132618);

  unsigned basethreshold = beatsin8_t( 9, 55, 65);
  unsigned wave = beat8( 7 );

  for (unsigned i = 0; i < SEGLEN; i++) {
    CRGB c = CRGB(2, 6, 10);
    // Render each of four layers, with different scales and speeds, that vary over time
    c += pacifica_one_layer(i, pacifica_palette_1, sCIStart1, beatsin16_t(3, 11 * 256, 14 * 256), beatsin8_t(10, 70, 130), 0-beat16(301));
    c += pacifica_one_layer(i, pacifica_palette_2, sCIStart2, beatsin16_t(4,  6 * 256,  9 * 256), beatsin8_t(17, 40,  80),   beat16(401));
    c += pacifica_one_layer(i, pacifica_palette_3, sCIStart3,                         6 * 256 , beatsin8_t(9, 10,38)   , 0-beat16(503));
    c += pacifica_one_layer(i, pacifica_palette_3, sCIStart4,                         5 * 256 , beatsin8_t(8, 10,28)   ,   beat16(601));

    // Add extra 'white' to areas where the four layers of light have lined up brightly
    unsigned threshold = scale8( sin8_t( wave), 20) + basethreshold;
    wave += 7;
    unsigned l = c.getAverageLight();
    if (l > threshold) {
      unsigned overage = l - threshold;
      unsigned overage2 = qadd8(overage, overage);
      c += CRGB(overage, overage2, qadd8(overage2, overage2));
    }

    //deepen the blues and greens
    c.blue  = scale8(c.blue,  145);
    c.green = scale8(c.green, 200);
    c |= CRGB( 2, 5, 7);

    SEGMENT.setPixelColor(i, c);
  }

  strip.now = nowOld;
  return FRAMETIME;
}
static const char _data_FX_MODE_PACIFICA[] PROGMEM = "Pacifica@!,Angle;;!;;pal=51";


/*
 * Mode simulates a gradual sunrise
 */
uint16_t mode_sunrise() {
  if (SEGLEN <= 1) return mode_static();
  //speed 0 - static sun
  //speed 1 - 60: sunrise time in minutes
  //speed 60 - 120 : sunset time in minutes - 60;
  //speed above: "breathing" rise and set
  if (SEGENV.call == 0 || SEGMENT.speed != SEGENV.aux0) {
    SEGENV.step = millis(); //save starting time, millis() because strip.now can change from sync
    SEGENV.aux0 = SEGMENT.speed;
  }

  SEGMENT.fill(BLACK);
  unsigned stage = 0xFFFF;

  uint32_t s10SinceStart = (millis() - SEGENV.step) /100; //tenths of seconds

  if (SEGMENT.speed > 120) { //quick sunrise and sunset
    unsigned counter = (strip.now >> 1) * (((SEGMENT.speed -120) >> 1) +1);
    stage = triwave16(counter);
  } else if (SEGMENT.speed) { //sunrise
    unsigned durMins = SEGMENT.speed;
    if (durMins > 60) durMins -= 60;
    uint32_t s10Target = durMins * 600;
    if (s10SinceStart > s10Target) s10SinceStart = s10Target;
    stage = map(s10SinceStart, 0, s10Target, 0, 0xFFFF);
    if (SEGMENT.speed > 60) stage = 0xFFFF - stage; //sunset
  }

  for (unsigned i = 0; i <= SEGLEN/2; i++)
  {
    //default palette is Fire    
    unsigned wave = triwave16((i * stage) / SEGLEN);
    wave = (wave >> 8) + ((wave * SEGMENT.intensity) >> 15);
    uint32_t c;
    if (wave > 240) { //clipped, full white sun
      c = SEGMENT.color_from_palette( 240, false, true, 255);
    } else { //transition
      c = SEGMENT.color_from_palette(wave, false, true, 255);
    }
    SEGMENT.setPixelColor(i, c);
    SEGMENT.setPixelColor(SEGLEN - i - 1, c);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_SUNRISE[] PROGMEM = "Sunrise@Time [min],Width;;!;;pal=35,sx=60";


/*
 * Effects by Andrew Tuline
 */
static uint16_t phased_base(uint8_t moder) {                  // We're making sine waves here. By Andrew Tuline.

  unsigned allfreq = 16;                                          // Base frequency.
  float *phase = reinterpret_cast<float*>(&SEGENV.step);         // Phase change value gets calculated (float fits into unsigned long).
  unsigned cutOff = (255-SEGMENT.intensity);                      // You can change the number of pixels.  AKA INTENSITY (was 192).
  unsigned modVal = 5;//SEGMENT.fft1/8+1;                         // You can change the modulus. AKA FFT1 (was 5).

  unsigned index = strip.now/64;                                  // Set color rotation speed
  *phase += SEGMENT.speed/32.0;                                  // You can change the speed of the wave. AKA SPEED (was .4)

  for (unsigned i = 0; i < SEGLEN; i++) {
    if (moder == 1) modVal = (perlin8(i*10 + i*10) /16);         // Let's randomize our mod length with some Perlin noise.
    unsigned val = (i+1) * allfreq;                              // This sets the frequency of the waves. The +1 makes sure that led 0 is used.
    if (modVal == 0) modVal = 1;
    val += *phase * (i % modVal +1) /2;                          // This sets the varying phase change of the waves. By Andrew Tuline.
    unsigned b = cubicwave8(val);                                 // Now we make an 8 bit sinewave.
    b = (b > cutOff) ? (b - cutOff) : 0;                         // A ternary operator to cutoff the light.
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(index, false, false, 0), uint8_t(b)));
    index += 256 / SEGLEN;
    if (SEGLEN > 256) index ++;                                  // Correction for segments longer than 256 LEDs
  }

  return FRAMETIME;
}


uint16_t mode_phased(void) {
  return phased_base(0);
}
static const char _data_FX_MODE_PHASED[] PROGMEM = "Phased@!,!;!,!;!";


uint16_t mode_phased_noise(void) {
  return phased_base(1);
}
static const char _data_FX_MODE_PHASEDNOISE[] PROGMEM = "Phased Noise@!,!;!,!;!";


uint16_t mode_twinkleup(void) {                 // A very short twinkle routine with fade-in and dual controls. By Andrew Tuline.
  unsigned prevSeed = random16_get_seed();      // save seed so we can restore it at the end of the function
  random16_set_seed(535);                       // The randomizer needs to be re-set each time through the loop in order for the same 'random' numbers to be the same each time through.

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned ranstart = random8();               // The starting value (aka brightness) for each pixel. Must be consistent each time through the loop for this to work.
    unsigned pixBri = sin8_t(ranstart + 16 * strip.now/(256-SEGMENT.speed));
    if (random8() > SEGMENT.intensity) pixBri = 0;
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(random8()+strip.now/100, false, PALETTE_SOLID_WRAP, 0), pixBri));
  }

  random16_set_seed(prevSeed); // restore original seed so other effects can use "random" PRNG
  return FRAMETIME;
}
static const char _data_FX_MODE_TWINKLEUP[] PROGMEM = "Twinkleup@!,Intensity;!,!;!;;m12=0";


// Peaceful noise that's slow and with gradually changing palettes. Does not support WLED palettes or default colours or controls.
uint16_t mode_noisepal(void) {                                    // Slow noise palette by Andrew Tuline.
  unsigned scale = 15 + (SEGMENT.intensity >> 2); //default was 30
  //#define scale 30

  unsigned dataSize = sizeof(CRGBPalette16) * 2; //allocate space for 2 Palettes (2 * 16 * 3 = 96 bytes)
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  CRGBPalette16* palettes = reinterpret_cast<CRGBPalette16*>(SEGENV.data);

  unsigned changePaletteMs = 4000 + SEGMENT.speed *10; //between 4 - 6.5sec
  if (strip.now - SEGENV.step > changePaletteMs)
  {
    SEGENV.step = strip.now;

    unsigned baseI = hw_random8();
    palettes[1] = CRGBPalette16(CHSV(baseI+hw_random8(64), 255, hw_random8(128,255)), CHSV(baseI+128, 255, hw_random8(128,255)), CHSV(baseI+hw_random8(92), 192, hw_random8(128,255)), CHSV(baseI+hw_random8(92), 255, hw_random8(128,255)));
  }

  //EVERY_N_MILLIS(10) { //(don't have to time this, effect function is only called every 24ms)
  nblendPaletteTowardPalette(palettes[0], palettes[1], 48);               // Blend towards the target palette over 48 iterations.

  if (SEGMENT.palette > 0) palettes[0] = SEGPALETTE;

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned index = perlin8(i*scale, SEGENV.aux0+i*scale);                // Get a value from the noise function. I'm using both x and y axis.
    SEGMENT.setPixelColor(i,  ColorFromPalette(palettes[0], index, 255, LINEARBLEND));  // Use my own palette.
  }

  SEGENV.aux0 += beatsin8_t(10,1,4);                                        // Moving along the distance. Vary it a bit with a sine wave.

  return FRAMETIME;
}
static const char _data_FX_MODE_NOISEPAL[] PROGMEM = "Noise Pal@!,Scale;;!";


// Sine waves that have controllable phase change speed, frequency and cutoff. By Andrew Tuline.
// SEGMENT.speed ->Speed, SEGMENT.intensity -> Frequency (SEGMENT.fft1 -> Color change, SEGMENT.fft2 -> PWM cutoff)
//
uint16_t mode_sinewave(void) {             // Adjustable sinewave. By Andrew Tuline
  //#define qsuba(x, b)  ((x>b)?x-b:0)               // Analog Unsigned subtraction macro. if result <0, then => 0

  unsigned colorIndex = strip.now /32;//(256 - SEGMENT.fft1);  // Amount of colour change.

  SEGENV.step += SEGMENT.speed/16;                   // Speed of animation.
  unsigned freq = SEGMENT.intensity/4;//SEGMENT.fft2/8;                       // Frequency of the signal.

  for (unsigned i = 0; i < SEGLEN; i++) {                 // For each of the LED's in the strand, set a brightness based on a wave as follows:
    uint8_t pixBri = cubicwave8((i*freq)+SEGENV.step);//qsuba(cubicwave8((i*freq)+SEGENV.step), (255-SEGMENT.intensity)); // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
    //setPixCol(i, i*colorIndex/255, pixBri);
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i*colorIndex/255, false, PALETTE_SOLID_WRAP, 0), pixBri));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_SINEWAVE[] PROGMEM = "Sine@!,Scale;;!";


/*
 * Best of both worlds from Palette and Spot effects. By Aircoookie
 */
uint16_t mode_flow(void)
{
  unsigned counter = 0;
  if (SEGMENT.speed != 0)
  {
    counter = strip.now * ((SEGMENT.speed >> 2) +1);
    counter = counter >> 8;
  }

  unsigned maxZones = SEGLEN / 6; //only looks good if each zone has at least 6 LEDs
  unsigned zones = (SEGMENT.intensity * maxZones) >> 8;
  if (zones & 0x01) zones++; //zones must be even
  if (zones < 2) zones = 2;
  unsigned zoneLen = SEGLEN / zones;
  unsigned offset = (SEGLEN - zones * zoneLen) >> 1;

  SEGMENT.fill(SEGMENT.color_from_palette(-counter, false, true, 255));

  for (unsigned z = 0; z < zones; z++)
  {
    unsigned pos = offset + z * zoneLen;
    for (unsigned i = 0; i < zoneLen; i++)
    {
      unsigned colorIndex = (i * 255 / zoneLen) - counter;
      unsigned led = (z & 0x01) ? i : (zoneLen -1) -i;
      if (SEGMENT.reverse) led = (zoneLen -1) -led;
      SEGMENT.setPixelColor(pos + led, SEGMENT.color_from_palette(colorIndex, false, true, 255));
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_FLOW[] PROGMEM = "Flow@!,Zones;;!;;m12=1"; //vertical


/*
 * Dots waving around in a sine/pendulum motion.
 * Little pixel birds flying in a circle. By Aircoookie
 */
uint16_t mode_chunchun(void)
{
  if (SEGLEN <= 1) return mode_static();
  SEGMENT.fade_out(254); // add a bit of trail
  unsigned counter = strip.now * (6 + (SEGMENT.speed >> 4));
  unsigned numBirds = 2 + (SEGLEN >> 3);  // 2 + 1/8 of a segment
  unsigned span = (SEGMENT.intensity << 8) / numBirds;

  for (unsigned i = 0; i < numBirds; i++)
  {
    counter -= span;
    unsigned megumin = sin16_t(counter) + 0x8000;
    unsigned bird = uint32_t(megumin * SEGLEN) >> 16;
    bird = constrain(bird, 0U, SEGLEN-1U);
    SEGMENT.setPixelColor(bird, SEGMENT.color_from_palette((i * 255)/ numBirds, false, false, 0)); // no palette wrapping
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_CHUNCHUN[] PROGMEM = "Chunchun@!,Gap size;!,!;!";

#define SPOT_TYPE_SOLID       0
#define SPOT_TYPE_GRADIENT    1
#define SPOT_TYPE_2X_GRADIENT 2
#define SPOT_TYPE_2X_DOT      3
#define SPOT_TYPE_3X_DOT      4
#define SPOT_TYPE_4X_DOT      5
#define SPOT_TYPES_COUNT      6
#ifdef ESP8266
  #define SPOT_MAX_COUNT 17          //Number of simultaneous waves
#else
  #define SPOT_MAX_COUNT 49          //Number of simultaneous waves
#endif

#ifdef WLED_PS_DONT_REPLACE_FX
//13 bytes
typedef struct Spotlight {
  float speed;
  uint8_t colorIdx;
  int16_t position;
  unsigned long lastUpdateTime;
  uint8_t width;
  uint8_t type;
} spotlight;

/*
 * Spotlights moving back and forth that cast dancing shadows.
 * Shine this through tree branches/leaves or other close-up objects that cast
 * interesting shadows onto a ceiling or tarp.
 *
 * By Steve Pomeroy @xxv
 */
uint16_t mode_dancing_shadows(void)
{
  if (SEGLEN <= 1) return mode_static();
  unsigned numSpotlights = map(SEGMENT.intensity, 0, 255, 2, SPOT_MAX_COUNT);  // 49 on 32 segment ESP32, 17 on 16 segment ESP8266
  bool initialize = SEGENV.aux0 != numSpotlights;
  SEGENV.aux0 = numSpotlights;

  unsigned dataSize = sizeof(spotlight) * numSpotlights;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  Spotlight* spotlights = reinterpret_cast<Spotlight*>(SEGENV.data);

  SEGMENT.fill(BLACK);

  unsigned long time = strip.now;
  bool respawn = false;

  for (size_t i = 0; i < numSpotlights; i++) {
    if (!initialize) {
      // advance the position of the spotlight
      int delta = (float)(time - spotlights[i].lastUpdateTime) *
                  (spotlights[i].speed * ((1.0 + SEGMENT.speed)/100.0));

      if (abs(delta) >= 1) {
        spotlights[i].position += delta;
        spotlights[i].lastUpdateTime = time;
      }

      respawn = (spotlights[i].speed > 0.0 && spotlights[i].position > (int)(SEGLEN + 2))
             || (spotlights[i].speed < 0.0 && spotlights[i].position < -(spotlights[i].width + 2));
    }

    if (initialize || respawn) {
      spotlights[i].colorIdx = hw_random8();
      spotlights[i].width = hw_random8(1, 10);

      spotlights[i].speed = 1.0/hw_random8(4, 50);

      if (initialize) {
        spotlights[i].position = hw_random16(SEGLEN);
        spotlights[i].speed *= hw_random8(2) ? 1.0 : -1.0;
      } else {
        if (hw_random8(2)) {
          spotlights[i].position = SEGLEN + spotlights[i].width;
          spotlights[i].speed *= -1.0;
        }else {
          spotlights[i].position = -spotlights[i].width;
        }
      }

      spotlights[i].lastUpdateTime = time;
      spotlights[i].type = hw_random8(SPOT_TYPES_COUNT);
    }

    uint32_t color = SEGMENT.color_from_palette(spotlights[i].colorIdx, false, false, 255);
    int start = spotlights[i].position;

    if (spotlights[i].width <= 1) {
      if (start >= 0 && start < (int)SEGLEN) {
        SEGMENT.blendPixelColor(start, color, 128);
      }
    } else {
      switch (spotlights[i].type) {
        case SPOT_TYPE_SOLID:
          for (size_t j = 0; j < spotlights[i].width; j++) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;

        case SPOT_TYPE_GRADIENT:
          for (size_t j = 0; j < spotlights[i].width; j++) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, cubicwave8(map(j, 0, spotlights[i].width - 1, 0, 255)));
            }
          }
        break;

        case SPOT_TYPE_2X_GRADIENT:
          for (size_t j = 0; j < spotlights[i].width; j++) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, cubicwave8(2 * map(j, 0, spotlights[i].width - 1, 0, 255)));
            }
          }
        break;

        case SPOT_TYPE_2X_DOT:
          for (size_t j = 0; j < spotlights[i].width; j += 2) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;

        case SPOT_TYPE_3X_DOT:
          for (size_t j = 0; j < spotlights[i].width; j += 3) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;

        case SPOT_TYPE_4X_DOT:
          for (size_t j = 0; j < spotlights[i].width; j += 4) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;
      }
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_DANCING_SHADOWS[] PROGMEM = "Dancing Shadows@!,# of shadows;!;!";
#endif // WLED_PS_DONT_REPLACE_FX

/*
  Imitates a washing machine, rotating same waves forward, then pause, then backward.
  By Stefan Seegel
*/
uint16_t mode_washing_machine(void) {
  int speed = tristate_square8(strip.now >> 7, 90, 15);

  SEGENV.step += (speed * 2048) / (512 - SEGMENT.speed);

  for (unsigned i = 0; i < SEGLEN; i++) {
    uint8_t col = sin8_t(((SEGMENT.intensity / 25 + 1) * 255 * i / SEGLEN) + (SEGENV.step >> 7));
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(col, false, PALETTE_SOLID_WRAP, 3));
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_WASHING_MACHINE[] PROGMEM = "Washing Machine@!,!;;!";


/*
  Image effect
  Draws a .gif image from filesystem on the matrix/strip
*/
uint16_t mode_image(void) {
  #ifndef WLED_ENABLE_GIF
  return mode_static();
  #else
  renderImageToSegment(SEGMENT);
  return FRAMETIME;
  #endif
  // if (status != 0 && status != 254 && status != 255) {
  //   Serial.print("GIF renderer return: ");
  //   Serial.println(status);
  // }
}
static const char _data_FX_MODE_IMAGE[] PROGMEM = "Image@!,;;;12;sx=128";

/*
  Blends random colors across palette
  Modified, originally by Mark Kriegsman https://gist.github.com/kriegsman/1f7ccbbfa492a73c015e
*/
uint16_t mode_blends(void) {
  unsigned pixelLen = SEGLEN > UINT8_MAX ? UINT8_MAX : SEGLEN;
  unsigned dataSize = sizeof(uint32_t) * (pixelLen + 1);  // max segment length of 56 pixels on 16 segment ESP8266
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  uint32_t* pixels = reinterpret_cast<uint32_t*>(SEGENV.data);
  uint8_t blendSpeed = map(SEGMENT.intensity, 0, UINT8_MAX, 10, 128);
  unsigned shift = (strip.now * ((SEGMENT.speed >> 3) +1)) >> 8;

  for (unsigned i = 0; i < pixelLen; i++) {
    pixels[i] = color_blend(pixels[i], SEGMENT.color_from_palette(shift + quadwave8((i + 1) * 16), false, PALETTE_SOLID_WRAP, 255), blendSpeed);
    shift += 3;
  }

  unsigned offset = 0;
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, pixels[offset++]);
    if (offset >= pixelLen) offset = 0;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_BLENDS[] PROGMEM = "Blends@Shift speed,Blend speed;;!";


/*
  TV Simulator
  Modified and adapted to WLED by Def3nder, based on "Fake TV Light for Engineers" by Phillip Burgess https://learn.adafruit.com/fake-tv-light-for-engineers/arduino-sketch
*/
//43 bytes
typedef struct TvSim {
  uint32_t totalTime = 0;
  uint32_t fadeTime  = 0;
  uint32_t startTime = 0;
  uint32_t elapsed   = 0;
  uint32_t pixelNum  = 0;
  uint16_t sliderValues = 0;
  uint32_t sceeneStart    = 0;
  uint32_t sceeneDuration = 0;
  uint16_t sceeneColorHue = 0;
  uint8_t  sceeneColorSat = 0;
  uint8_t  sceeneColorBri = 0;
  uint8_t  actualColorR = 0;
  uint8_t  actualColorG = 0;
  uint8_t  actualColorB = 0;
  uint16_t pr = 0; // Prev R, G, B
  uint16_t pg = 0;
  uint16_t pb = 0;
} tvSim;

uint16_t mode_tv_simulator(void) {
  int nr, ng, nb, r, g, b, i, hue;
  uint8_t  sat, bri, j;

  if (!SEGENV.allocateData(sizeof(tvSim))) return mode_static(); //allocation failed
  TvSim* tvSimulator = reinterpret_cast<TvSim*>(SEGENV.data);

  uint8_t colorSpeed     = map(SEGMENT.speed,     0, UINT8_MAX,  1, 20);
  uint8_t colorIntensity = map(SEGMENT.intensity, 0, UINT8_MAX, 10, 30);

  i = SEGMENT.speed << 8 | SEGMENT.intensity;
  if (i != tvSimulator->sliderValues) {
    tvSimulator->sliderValues = i;
    SEGENV.aux1 = 0;
  }

    // create a new sceene
    if (((strip.now - tvSimulator->sceeneStart) >= tvSimulator->sceeneDuration) || SEGENV.aux1 == 0) {
      tvSimulator->sceeneStart    = strip.now;                                               // remember the start of the new sceene
      tvSimulator->sceeneDuration = hw_random16(60* 250* colorSpeed, 60* 750 * colorSpeed);    // duration of a "movie sceene" which has similar colors (5 to 15 minutes with max speed slider)
      tvSimulator->sceeneColorHue = hw_random16(   0, 768);                                    // random start color-tone for the sceene
      tvSimulator->sceeneColorSat = hw_random8 ( 100, 130 + colorIntensity);                   // random start color-saturation for the sceene
      tvSimulator->sceeneColorBri = hw_random8 ( 200, 240);                                    // random start color-brightness for the sceene
      SEGENV.aux1 = 1;
      SEGENV.aux0 = 0;
    }

    // slightly change the color-tone in this sceene
    if (SEGENV.aux0 == 0) {
      // hue change in both directions
      j = hw_random8(4 * colorIntensity);
      hue = (hw_random8() < 128) ? ((j < tvSimulator->sceeneColorHue)       ? tvSimulator->sceeneColorHue - j : 767 - tvSimulator->sceeneColorHue - j) :  // negative
                                ((j + tvSimulator->sceeneColorHue) < 767 ? tvSimulator->sceeneColorHue + j : tvSimulator->sceeneColorHue + j - 767) ;  // positive

      // saturation
      j = hw_random8(2 * colorIntensity);
      sat = (tvSimulator->sceeneColorSat - j) < 0 ? 0 : tvSimulator->sceeneColorSat - j;

      // brightness
      j = hw_random8(100);
      bri = (tvSimulator->sceeneColorBri - j) < 0 ? 0 : tvSimulator->sceeneColorBri - j;

      // calculate R,G,B from HSV
      // Source: https://blog.adafruit.com/2012/03/14/constant-brightness-hsb-to-rgb-algorithm/
      { // just to create a local scope for  the variables
        uint8_t temp[5], n = (hue >> 8) % 3;
        uint8_t x = ((((hue & 255) * sat) >> 8) * bri) >> 8;
        uint8_t s = (  (256 - sat) * bri) >> 8;
        temp[0] = temp[3] =       s;
        temp[1] = temp[4] =   x + s;
        temp[2] =           bri - x;
        tvSimulator->actualColorR = temp[n + 2];
        tvSimulator->actualColorG = temp[n + 1];
        tvSimulator->actualColorB = temp[n    ];
      }
    }
    // Apply gamma correction, further expand to 16/16/16
    nr = (uint8_t)gamma8(tvSimulator->actualColorR) * 257; // New R/G/B
    ng = (uint8_t)gamma8(tvSimulator->actualColorG) * 257;
    nb = (uint8_t)gamma8(tvSimulator->actualColorB) * 257;

  if (SEGENV.aux0 == 0) {  // initialize next iteration
    SEGENV.aux0 = 1;

    // randomize total duration and fade duration for the actual color
    tvSimulator->totalTime = hw_random16(250, 2500);                   // Semi-random pixel-to-pixel time
    tvSimulator->fadeTime  = hw_random16(0, tvSimulator->totalTime);   // Pixel-to-pixel transition time
    if (hw_random8(10) < 3) tvSimulator->fadeTime = 0;                 // Force scene cut 30% of time

    tvSimulator->startTime = strip.now;
  } // end of initialization

  // how much time is elapsed ?
  tvSimulator->elapsed = strip.now - tvSimulator->startTime;

  // fade from prev color to next color
  if (tvSimulator->elapsed < tvSimulator->fadeTime) {
    r = map(tvSimulator->elapsed, 0, tvSimulator->fadeTime, tvSimulator->pr, nr);
    g = map(tvSimulator->elapsed, 0, tvSimulator->fadeTime, tvSimulator->pg, ng);
    b = map(tvSimulator->elapsed, 0, tvSimulator->fadeTime, tvSimulator->pb, nb);
  } else { // Avoid divide-by-zero in map()
    r = nr;
    g = ng;
    b = nb;
  }

  // set strip color
  for (i = 0; i < (int)SEGLEN; i++) {
    SEGMENT.setPixelColor(i, r >> 8, g >> 8, b >> 8);  // Quantize to 8-bit
  }

  // if total duration has passed, remember last color and restart the loop
  if ( tvSimulator->elapsed >= tvSimulator->totalTime) {
    tvSimulator->pr = nr; // Prev RGB = new RGB
    tvSimulator->pg = ng;
    tvSimulator->pb = nb;
    SEGENV.aux0 = 0;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_TV_SIMULATOR[] PROGMEM = "TV Simulator@!,!;;!;01";


/*
  Aurora effect
*/

//CONFIG
#ifdef ESP8266
  #define W_MAX_COUNT  9          //Number of simultaneous waves
#else
  #define W_MAX_COUNT 20          //Number of simultaneous waves
#endif
#define W_MAX_SPEED 6             //Higher number, higher speed
#define W_WIDTH_FACTOR 6          //Higher number, smaller waves

//24 bytes
class AuroraWave {
  private:
    uint16_t ttl;
    CRGB basecolor;
    float basealpha;
    uint16_t age;
    uint16_t width;
    float center;
    bool goingleft;
    float speed_factor;
    bool alive = true;

  public:
    void init(uint32_t segment_length, CRGB color) {
      ttl = hw_random16(500, 1501);
      basecolor = color;
      basealpha = hw_random8(60, 101) / (float)100;
      age = 0;
      width = hw_random16(segment_length / 20, segment_length / W_WIDTH_FACTOR); //half of width to make math easier
      if (!width) width = 1;
      center = hw_random8(101) / (float)100 * segment_length;
      goingleft = hw_random8(0, 2) == 0;
      speed_factor = (hw_random8(10, 31) / (float)100 * W_MAX_SPEED / 255);
      alive = true;
    }

    CRGB getColorForLED(int ledIndex) {
      if(ledIndex < center - width || ledIndex > center + width) return 0; //Position out of range of this wave

      CRGB rgb;

      //Offset of this led from center of wave
      //The further away from the center, the dimmer the LED
      float offset = ledIndex - center;
      if (offset < 0) offset = -offset;
      float offsetFactor = offset / width;

      //The age of the wave determines it brightness.
      //At half its maximum age it will be the brightest.
      float ageFactor = 0.1;
      if((float)age / ttl < 0.5) {
        ageFactor = (float)age / (ttl / 2);
      } else {
        ageFactor = (float)(ttl - age) / ((float)ttl * 0.5);
      }

      //Calculate color based on above factors and basealpha value
      float factor = (1 - offsetFactor) * ageFactor * basealpha;
      rgb.r = basecolor.r * factor;
      rgb.g = basecolor.g * factor;
      rgb.b = basecolor.b * factor;

      return rgb;
    };

    //Change position and age of wave
    //Determine if its sill "alive"
    void update(uint32_t segment_length, uint32_t speed) {
      if(goingleft) {
        center -= speed_factor * speed;
      } else {
        center += speed_factor * speed;
      }

      age++;

      if(age > ttl) {
        alive = false;
      } else {
        if(goingleft) {
          if(center + width < 0) {
            alive = false;
          }
        } else {
          if(center - width > segment_length) {
            alive = false;
          }
        }
      }
    };

    bool stillAlive() {
      return alive;
    };
};

uint16_t mode_aurora(void) {
  //aux1 = Wavecount
  //aux2 = Intensity in last loop

  AuroraWave* waves;

//TODO: I am not sure this is a correct way of handling memory allocation since if it fails on 1st run
// it will display static effect but on second run it may crash ESP since data will be nullptr

  if(SEGENV.aux0 != SEGMENT.intensity || SEGENV.call == 0) {
    //Intensity slider changed or first call
    SEGENV.aux1 = map(SEGMENT.intensity, 0, 255, 2, W_MAX_COUNT);
    SEGENV.aux0 = SEGMENT.intensity;

    if(!SEGENV.allocateData(sizeof(AuroraWave) * SEGENV.aux1)) { // 26 on 32 segment ESP32, 9 on 16 segment ESP8266
      return mode_static(); //allocation failed
    }

    waves = reinterpret_cast<AuroraWave*>(SEGENV.data);

    for (int i = 0; i < SEGENV.aux1; i++) {
      waves[i].init(SEGLEN, CRGB(SEGMENT.color_from_palette(hw_random8(), false, false, hw_random8(0, 3))));
    }
  } else {
    waves = reinterpret_cast<AuroraWave*>(SEGENV.data);
  }

  for (int i = 0; i < SEGENV.aux1; i++) {
    //Update values of wave
    waves[i].update(SEGLEN, SEGMENT.speed);

    if(!(waves[i].stillAlive())) {
      //If a wave dies, reinitialize it starts over.
      waves[i].init(SEGLEN, CRGB(SEGMENT.color_from_palette(hw_random8(), false, false, hw_random8(0, 3))));
    }
  }

  uint8_t backlight = 1; //dimmer backlight if less active colors
  if (SEGCOLOR(0)) backlight++;
  if (SEGCOLOR(1)) backlight++;
  if (SEGCOLOR(2)) backlight++;
  //Loop through LEDs to determine color
  for (unsigned i = 0; i < SEGLEN; i++) {
    CRGB mixedRgb = CRGB(backlight, backlight, backlight);

    //For each LED we must check each wave if it is "active" at this position.
    //If there are multiple waves active on a LED we multiply their values.
    for (int  j = 0; j < SEGENV.aux1; j++) {
      CRGB rgb = waves[j].getColorForLED(i);

      if(rgb != CRGB(0)) {
        mixedRgb += rgb;
      }
    }

    SEGMENT.setPixelColor(i, mixedRgb[0], mixedRgb[1], mixedRgb[2]);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_AURORA[] PROGMEM = "Aurora@!,!;1,2,3;!;;sx=24,pal=50";

// WLED-SR effects

/////////////////////////
//     Perlin Move     //
/////////////////////////
// 16 bit perlinmove. Use Perlin Noise instead of sinewaves for movement. By Andrew Tuline.
// Controls are speed, # of pixels, faderate.
uint16_t mode_perlinmove(void) {
  if (SEGLEN <= 1) return mode_static();
  SEGMENT.fade_out(255-SEGMENT.custom1);
  for (int i = 0; i < SEGMENT.intensity/16 + 1; i++) {
    unsigned locn = perlin16(strip.now*128/(260-SEGMENT.speed)+i*15000, strip.now*128/(260-SEGMENT.speed)); // Get a new pixel location from moving noise.
    unsigned pixloc = map(locn, 50*256, 192*256, 0, SEGLEN-1);                                            // Map that to the length of the strand, and ensure we don't go over.
    SEGMENT.setPixelColor(pixloc, SEGMENT.color_from_palette(pixloc%255, false, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
} // mode_perlinmove()
static const char _data_FX_MODE_PERLINMOVE[] PROGMEM = "Perlin Move@!,# of pixels,Fade rate;!,!;!";


/////////////////////////
//     Waveins         //
/////////////////////////
// Uses beatsin8() + phase shifting. By: Andrew Tuline
uint16_t mode_wavesins(void) {

  for (unsigned i = 0; i < SEGLEN; i++) {
    uint8_t bri = sin8_t(strip.now/4 + i * SEGMENT.intensity);
    uint8_t index = beatsin8_t(SEGMENT.speed, SEGMENT.custom1, SEGMENT.custom1+SEGMENT.custom2, 0, i * (SEGMENT.custom3<<3)); // custom3 is reduced resolution slider
    //SEGMENT.setPixelColor(i, ColorFromPalette(SEGPALETTE, index, bri, LINEARBLEND));
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0, bri));
  }

  return FRAMETIME;
} // mode_waveins()
static const char _data_FX_MODE_WAVESINS[] PROGMEM = "Wavesins@!,Brightness variation,Starting color,Range of colors,Color variation;!;!";


//////////////////////////////
//     Flow Stripe          //
//////////////////////////////
// By: ldirko  https://editor.soulmatelights.com/gallery/392-flow-led-stripe , modifed by: Andrew Tuline, fixed by @DedeHai
uint16_t mode_FlowStripe(void) {
  if (SEGLEN <= 1) return mode_static();
  const int hl = SEGLEN * 10 / 13;
  uint8_t hue = strip.now / (SEGMENT.speed+1);
  uint32_t t = strip.now / (SEGMENT.intensity/8+1);

  for (unsigned i = 0; i < SEGLEN; i++) {
    int c = ((abs((int)i - hl) * 127) / hl);
    c = sin8_t(c);
    c = sin8_t(c / 2 + t);
    byte b = sin8_t(c + t/8);
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(b + hue, false, true, 3));
  }

  return FRAMETIME;
} // mode_FlowStripe()
static const char _data_FX_MODE_FLOWSTRIPE[] PROGMEM = "Flow Stripe@Hue speed,Effect speed;;!;pal=11";


#ifndef WLED_DISABLE_2D
///////////////////////////////////////////////////////////////////////////////
//***************************  2D routines  ***********************************


// Black hole
uint16_t mode_2DBlackHole(void) {            // By: Stepko https://editor.soulmatelights.com/gallery/1012 , Modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  int x, y;

  SEGMENT.fadeToBlackBy(16 + (SEGMENT.speed>>3)); // create fading trails
  unsigned long t = strip.now/128;                 // timebase
  // outer stars
  for (size_t i = 0; i < 8; i++) {
    x = beatsin8_t(SEGMENT.custom1>>3,   0, cols - 1, 0, ((i % 2) ? 128 : 0) + t * i);
    y = beatsin8_t(SEGMENT.intensity>>3, 0, rows - 1, 0, ((i % 2) ? 192 : 64) + t * i);
    SEGMENT.addPixelColorXY(x, y, SEGMENT.color_from_palette(i*32, false, PALETTE_SOLID_WRAP, SEGMENT.check1?0:255));
  }
  // inner stars
  for (size_t i = 0; i < 4; i++) {
    x = beatsin8_t(SEGMENT.custom2>>3, cols/4, cols - 1 - cols/4, 0, ((i % 2) ? 128 : 0) + t * i);
    y = beatsin8_t(SEGMENT.custom3   , rows/4, rows - 1 - rows/4, 0, ((i % 2) ? 192 : 64) + t * i);
    SEGMENT.addPixelColorXY(x, y, SEGMENT.color_from_palette(255-i*64, false, PALETTE_SOLID_WRAP, SEGMENT.check1?0:255));
  }
  // central white dot
  SEGMENT.setPixelColorXY(cols/2, rows/2, WHITE);
  // blur everything a bit
  if (SEGMENT.check3) SEGMENT.blur(16, cols*rows < 100);

  return FRAMETIME;
} // mode_2DBlackHole()
static const char _data_FX_MODE_2DBLACKHOLE[] PROGMEM = "Black Hole@Fade rate,Outer Y freq.,Outer X freq.,Inner X freq.,Inner Y freq.,Solid,,Blur;!;!;2;pal=11";


////////////////////////////
//     2D Colored Bursts  //
////////////////////////////
uint16_t mode_2DColoredBursts() {              // By: ldirko   https://editor.soulmatelights.com/gallery/819-colored-bursts , modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGENV.aux0 = 0; // start with red hue
  }

  bool dot = SEGMENT.check3;
  bool grad = SEGMENT.check1;

  byte numLines = SEGMENT.intensity/16 + 1;

  SEGENV.aux0++;  // hue
  SEGMENT.fadeToBlackBy(40 - SEGMENT.check2 * 8);
  for (size_t i = 0; i < numLines; i++) {
    byte x1 = beatsin8_t(2 + SEGMENT.speed/16, 0, (cols - 1));
    byte x2 = beatsin8_t(1 + SEGMENT.speed/16, 0, (rows - 1));
    byte y1 = beatsin8_t(5 + SEGMENT.speed/16, 0, (cols - 1), 0, i * 24);
    byte y2 = beatsin8_t(3 + SEGMENT.speed/16, 0, (rows - 1), 0, i * 48 + 64);
    uint32_t color = ColorFromPalette(SEGPALETTE, i * 255 / numLines + (SEGENV.aux0&0xFF), 255, LINEARBLEND);

    byte xsteps = abs8(x1 - y1) + 1;
    byte ysteps = abs8(x2 - y2) + 1;
    byte steps = xsteps >= ysteps ? xsteps : ysteps;
    //Draw gradient line
    for (size_t j = 1; j <= steps; j++) {
      uint8_t rate = j * 255 / steps;
      byte dx = lerp8by8(x1, y1, rate);
      byte dy = lerp8by8(x2, y2, rate);
      //SEGMENT.setPixelColorXY(dx, dy, grad ? color.nscale8_video(255-rate) : color); // use addPixelColorXY for different look
      SEGMENT.addPixelColorXY(dx, dy, color); // use setPixelColorXY for different look
      if (grad) SEGMENT.fadePixelColorXY(dx, dy, rate);
    }

    if (dot) { //add white point at the ends of line
      SEGMENT.setPixelColorXY(x1, x2, WHITE);
      SEGMENT.setPixelColorXY(y1, y2, DARKSLATEGRAY);
    }
  }
  SEGMENT.blur(SEGMENT.custom3>>1, SEGMENT.check2);

  return FRAMETIME;
} // mode_2DColoredBursts()
static const char _data_FX_MODE_2DCOLOREDBURSTS[] PROGMEM = "Colored Bursts@Speed,# of lines,,,Blur,Gradient,Smear,Dots;;!;2;c3=16";


/////////////////////
//      2D DNA     //
/////////////////////
uint16_t mode_2Ddna(void) {         // dna originally by by ldirko at https://pastebin.com/pCkkkzcs. Updated by Preyy. WLED conversion by Andrew Tuline.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  SEGMENT.fadeToBlackBy(64);
  for (int i = 0; i < cols; i++) {
    SEGMENT.setPixelColorXY(i, beatsin8_t(SEGMENT.speed/8, 0, rows-1, 0, i*4    ), ColorFromPalette(SEGPALETTE, i*5+strip.now/17, beatsin8_t(5, 55, 255, 0, i*10), LINEARBLEND));
    SEGMENT.setPixelColorXY(i, beatsin8_t(SEGMENT.speed/8, 0, rows-1, 0, i*4+128), ColorFromPalette(SEGPALETTE, i*5+128+strip.now/17, beatsin8_t(5, 55, 255, 0, i*10+128), LINEARBLEND));
  }
  SEGMENT.blur(SEGMENT.intensity / (8 - (SEGMENT.check1 * 2)), SEGMENT.check1);

  return FRAMETIME;
} // mode_2Ddna()
static const char _data_FX_MODE_2DDNA[] PROGMEM = "DNA@Scroll speed,Blur,,,,Smear;;!;2;ix=0";

/////////////////////////
//     2D DNA Spiral   //
/////////////////////////
uint16_t mode_2DDNASpiral() {               // By: ldirko  https://editor.soulmatelights.com/gallery/512-dna-spiral-variation , modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  unsigned speeds = SEGMENT.speed/2 + 7;
  unsigned freq = SEGMENT.intensity/8;

  uint32_t ms = strip.now / 20;
  SEGMENT.fadeToBlackBy(135);

  for (int i = 0; i < rows; i++) {
    int x  = beatsin8_t(speeds, 0, cols - 1, 0, i * freq) + beatsin8_t(speeds - 7, 0, cols - 1, 0, i * freq + 128);
    int x1 = beatsin8_t(speeds, 0, cols - 1, 0, 128 + i * freq) + beatsin8_t(speeds - 7, 0, cols - 1, 0, 128 + 64 + i * freq);
    unsigned hue = (i * 128 / rows) + ms;
    // skip every 4th row every now and then (fade it more)
    if ((i + ms / 8) & 3) {
      // draw a gradient line between x and x1
      x = x / 2; x1 = x1 / 2;
      unsigned steps = abs8(x - x1) + 1;
      bool positive = (x1 >= x);                         // direction of drawing
      for (size_t k = 1; k <= steps; k++) {
        unsigned rate = k * 255 / steps;
        //unsigned dx = lerp8by8(x, x1, rate);
        unsigned dx = positive? (x + k-1) : (x - k+1);   // behaves the same as "lerp8by8" but does not create holes
        //SEGMENT.setPixelColorXY(dx, i, ColorFromPalette(SEGPALETTE, hue, 255, LINEARBLEND).nscale8_video(rate));
        SEGMENT.addPixelColorXY(dx, i, ColorFromPalette(SEGPALETTE, hue, 255, LINEARBLEND)); // use setPixelColorXY for different look
        SEGMENT.fadePixelColorXY(dx, i, rate);
      }
      SEGMENT.setPixelColorXY(x, i, DARKSLATEGRAY);
      SEGMENT.setPixelColorXY(x1, i, WHITE);
    }
  }
  SEGMENT.blur(((uint16_t)SEGMENT.custom1 * 3) / (6 + SEGMENT.check1), SEGMENT.check1);

  return FRAMETIME;
} // mode_2DDNASpiral()
static const char _data_FX_MODE_2DDNASPIRAL[] PROGMEM = "DNA Spiral@Scroll speed,Y frequency,Blur,,,Smear;;!;2;c1=0";


/////////////////////////
//     2D Drift        //
/////////////////////////
uint16_t mode_2DDrift() {              // By: Stepko   https://editor.soulmatelights.com/gallery/884-drift , Modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  const int colsCenter = (cols>>1) + (cols%2);
  const int rowsCenter = (rows>>1) + (rows%2);

  SEGMENT.fadeToBlackBy(128);
  const float maxDim = MAX(cols, rows)/2;
  unsigned long t = strip.now / (32 - (SEGMENT.speed>>3));
  unsigned long t_20 = t/20; // softhack007: pre-calculating this gives about 10% speedup
  for (float i = 1.0f; i < maxDim; i += 0.25f) {
    float angle = radians(t * (maxDim - i));
    int mySin = sin_t(angle) * i;
    int myCos = cos_t(angle) * i;
    SEGMENT.setPixelColorXY(colsCenter + mySin, rowsCenter + myCos, ColorFromPalette(SEGPALETTE, (i * 20) + t_20, 255, LINEARBLEND));
    if (SEGMENT.check1) SEGMENT.setPixelColorXY(colsCenter + myCos, rowsCenter + mySin, ColorFromPalette(SEGPALETTE, (i * 20) + t_20, 255, LINEARBLEND));
  }
  SEGMENT.blur(SEGMENT.intensity>>(3 - SEGMENT.check2), SEGMENT.check2);

  return FRAMETIME;
} // mode_2DDrift()
static const char _data_FX_MODE_2DDRIFT[] PROGMEM = "Drift@Rotation speed,Blur,,,,Twin,Smear;;!;2;ix=0";


//////////////////////////
//     2D Firenoise     //
//////////////////////////
uint16_t mode_2Dfirenoise(void) {               // firenoise2d. By Andrew Tuline. Yet another short routine.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  unsigned xscale = SEGMENT.intensity*4;
  unsigned yscale = SEGMENT.speed*8;
  unsigned indexx = 0;

  //CRGBPalette16 pal = SEGMENT.check1 ? SEGPALETTE : SEGMENT.loadPalette(pal, 35);  
  CRGBPalette16 pal = SEGMENT.check1 ? SEGPALETTE : CRGBPalette16(CRGB::Black,     CRGB::Black,      CRGB::Black,  CRGB::Black,
                                                                  CRGB::Red,       CRGB::Red,        CRGB::Red,    CRGB::DarkOrange,
                                                                  CRGB::DarkOrange,CRGB::DarkOrange, CRGB::Orange, CRGB::Orange,
                                                                  CRGB::Yellow,    CRGB::Orange,     CRGB::Yellow, CRGB::Yellow);
  for (int j=0; j < cols; j++) {
    for (int i=0; i < rows; i++) {
      indexx = perlin8(j*yscale*rows/255, i*xscale+strip.now/4);                                               // We're moving along our Perlin map.
      SEGMENT.setPixelColorXY(j, i, ColorFromPalette(pal, min(i*indexx/11, 225U), i*255/rows, LINEARBLEND));   // With that value, look up the 8 bit colour palette value and assign it to the current LED.    
    } // for i
  } // for j

  return FRAMETIME;
} // mode_2Dfirenoise()
static const char _data_FX_MODE_2DFIRENOISE[] PROGMEM = "Firenoise@X scale,Y scale,,,,Palette;;!;2;pal=66";


//////////////////////////////
//     2D Frizzles          //
//////////////////////////////
uint16_t mode_2DFrizzles(void) {                 // By: Stepko https://editor.soulmatelights.com/gallery/640-color-frizzles , Modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  SEGMENT.fadeToBlackBy(16 + SEGMENT.check1 * 10);
  for (size_t i = 8; i > 0; i--) {
    SEGMENT.addPixelColorXY(beatsin8_t(SEGMENT.speed/8 + i, 0, cols - 1),
                            beatsin8_t(SEGMENT.intensity/8 - i, 0, rows - 1),
                            ColorFromPalette(SEGPALETTE, beatsin8_t(12, 0, 255), 255, LINEARBLEND));
  }
  SEGMENT.blur(SEGMENT.custom1 >> (3 + SEGMENT.check1), SEGMENT.check1);
  return FRAMETIME;
} // mode_2DFrizzles()
static const char _data_FX_MODE_2DFRIZZLES[] PROGMEM = "Frizzles@X frequency,Y frequency,Blur,,,Smear;;!;2";


///////////////////////////////////////////
//   2D Cellular Automata Game of life   //
///////////////////////////////////////////
typedef struct ColorCount {
  CRGB color;
  int8_t count;
} colorCount;

uint16_t mode_2Dgameoflife(void) { // Written by Ewoud Wijma, inspired by https://natureofcode.com/book/chapter-7-cellular-automata/ and https://github.com/DougHaber/nlife-color
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const auto XY = [&](int x, int y) { return (x%cols) + (y%rows) * cols; };
  const unsigned dataSize = sizeof(CRGB) * SEGMENT.length();  // using width*height prevents reallocation if mirroring is enabled
  const int crcBufferLen = 2; //(SEGMENT.width() + SEGMENT.height())*71/100; // roughly sqrt(2)/2 for better repetition detection (Ewowi)

  if (!SEGENV.allocateData(dataSize + sizeof(uint16_t)*crcBufferLen)) return mode_static(); //allocation failed
  CRGB *prevLeds = reinterpret_cast<CRGB*>(SEGENV.data);
  uint16_t *crcBuffer = reinterpret_cast<uint16_t*>(SEGENV.data + dataSize); 

  CRGB backgroundColor = SEGCOLOR(1);

  if (SEGENV.call == 0 || strip.now - SEGMENT.step > 3000) {
    SEGENV.step = strip.now;
    SEGENV.aux0 = 0;

    //give the leds random state and colors (based on intensity, colors from palette or all posible colors are chosen)
    for (int x = 0; x < cols; x++) for (int y = 0; y < rows; y++) {
      unsigned state = hw_random8()%2;
      if (state == 0)
        SEGMENT.setPixelColorXY(x,y, backgroundColor);
      else
        SEGMENT.setPixelColorXY(x,y, SEGMENT.color_from_palette(hw_random8(), false, PALETTE_SOLID_WRAP, 255));
    }

    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) prevLeds[XY(x,y)] = CRGB::Black;
    memset(crcBuffer, 0, sizeof(uint16_t)*crcBufferLen);
  } else if (strip.now - SEGENV.step < FRAMETIME_FIXED * (uint32_t)map(SEGMENT.speed,0,255,64,4)) {
    // update only when appropriate time passes (in 42 FPS slots)
    return FRAMETIME;
  }

  //copy previous leds (save previous generation)
  //NOTE: using lossy getPixelColor() is a benefit as endlessly repeating patterns will eventually fade out causing a reset
  for (int x = 0; x < cols; x++) for (int y = 0; y < rows; y++) prevLeds[XY(x,y)] = SEGMENT.getPixelColorXY(x,y);

  //calculate new leds
  for (int x = 0; x < cols; x++) for (int y = 0; y < rows; y++) {

    colorCount colorsCount[9]; // count the different colors in the 3*3 matrix
    for (int i=0; i<9; i++) colorsCount[i] = {backgroundColor, 0}; // init colorsCount

    // iterate through neighbors and count them and their different colors
    int neighbors = 0;
    for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++) { // iterate through 3*3 matrix
      if (i==0 && j==0) continue; // ignore itself
      // wrap around segment
      int xx = x+i, yy = y+j;
      if (x+i < 0) xx = cols-1; else if (x+i >= cols) xx = 0;
      if (y+j < 0) yy = rows-1; else if (y+j >= rows) yy = 0;

      unsigned xy = XY(xx, yy); // previous cell xy to check
      // count different neighbours and colors
      if (prevLeds[xy] != backgroundColor) {
        neighbors++;
        bool colorFound = false;
        int k;
        for (k=0; k<9 && colorsCount[k].count != 0; k++)
          if (colorsCount[k].color == prevLeds[xy]) {
            colorsCount[k].count++;
            colorFound = true;
          }
        if (!colorFound) colorsCount[k] = {prevLeds[xy], 1}; //add new color found in the array
      }
    } // i,j

    // Rules of Life
    uint32_t col = uint32_t(prevLeds[XY(x,y)]) & 0x00FFFFFF;  // uint32_t operator returns RGBA, we want RGBW -> cut off "alpha" byte
    uint32_t bgc = RGBW32(backgroundColor.r, backgroundColor.g, backgroundColor.b, 0);
    if      ((col != bgc) && (neighbors <  2)) SEGMENT.setPixelColorXY(x,y, bgc); // Loneliness
    else if ((col != bgc) && (neighbors >  3)) SEGMENT.setPixelColorXY(x,y, bgc); // Overpopulation
    else if ((col == bgc) && (neighbors == 3)) {                                  // Reproduction
      // find dominant color and assign it to a cell
      colorCount dominantColorCount = {backgroundColor, 0};
      for (int i=0; i<9 && colorsCount[i].count != 0; i++)
        if (colorsCount[i].count > dominantColorCount.count) dominantColorCount = colorsCount[i];
      // assign the dominant color w/ a bit of randomness to avoid "gliders"
      if (dominantColorCount.count > 0 && hw_random8(128)) SEGMENT.setPixelColorXY(x,y, dominantColorCount.color);
    } else if ((col == bgc) && (neighbors == 2) && !hw_random8(128)) {               // Mutation
      SEGMENT.setPixelColorXY(x,y, SEGMENT.color_from_palette(hw_random8(), false, PALETTE_SOLID_WRAP, 255));
    }
    // else do nothing!
  } //x,y

  // calculate CRC16 of leds
  uint16_t crc = crc16((const unsigned char*)prevLeds, dataSize);
  // check if we had same CRC and reset if needed
  bool repetition = false;
  for (int i=0; i<crcBufferLen && !repetition; i++) repetition = (crc == crcBuffer[i]); // (Ewowi)
  // same CRC would mean image did not change or was repeating itself
  if (!repetition) SEGENV.step = strip.now; //if no repetition avoid reset
  // remember CRCs across frames
  crcBuffer[SEGENV.aux0] = crc;
  ++SEGENV.aux0 %= crcBufferLen;

  return FRAMETIME;
} // mode_2Dgameoflife()
static const char _data_FX_MODE_2DGAMEOFLIFE[] PROGMEM = "Game Of Life@!;!,!;!;2";


/////////////////////////
//     2D Hiphotic     //
/////////////////////////
uint16_t mode_2DHiphotic() {                        //  By: ldirko  https://editor.soulmatelights.com/gallery/810 , Modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const uint32_t a = strip.now / ((SEGMENT.custom3>>1)+1);

  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      SEGMENT.setPixelColorXY(x, y, SEGMENT.color_from_palette(sin8_t(cos8_t(x * SEGMENT.speed/16 + a / 3) + sin8_t(y * SEGMENT.intensity/16 + a / 4) + a), false, PALETTE_SOLID_WRAP, 0));
    }
  }

  return FRAMETIME;
} // mode_2DHiphotic()
static const char _data_FX_MODE_2DHIPHOTIC[] PROGMEM = "Hiphotic@X scale,Y scale,,,Speed;!;!;2";


/////////////////////////
//     2D Julia        //
/////////////////////////
// Sliders are:
// intensity = Maximum number of iterations per pixel.
// Custom1 = Location of X centerpoint
// Custom2 = Location of Y centerpoint
// Custom3 = Size of the area (small value = smaller area)
typedef struct Julia {
  float xcen;
  float ycen;
  float xymag;
} julia;

uint16_t mode_2DJulia(void) {                           // An animated Julia set by Andrew Tuline.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (!SEGENV.allocateData(sizeof(julia))) return mode_static();
  Julia* julias = reinterpret_cast<Julia*>(SEGENV.data);

  float reAl;
  float imAg;

  if (SEGENV.call == 0) {           // Reset the center if we've just re-started this animation.
    julias->xcen = 0.;
    julias->ycen = 0.;
    julias->xymag = 1.0;

    SEGMENT.custom1 = 128;              // Make sure the location widgets are centered to start.
    SEGMENT.custom2 = 128;
    SEGMENT.custom3 = 16;
    SEGMENT.intensity = 24;
  }

  julias->xcen  = julias->xcen  + (float)(SEGMENT.custom1 - 128)/100000.f;
  julias->ycen  = julias->ycen  + (float)(SEGMENT.custom2 - 128)/100000.f;
  julias->xymag = julias->xymag + (float)((SEGMENT.custom3 - 16)<<3)/100000.f; // reduced resolution slider
  if (julias->xymag < 0.01f) julias->xymag = 0.01f;
  if (julias->xymag > 1.0f) julias->xymag = 1.0f;

  float xmin = julias->xcen - julias->xymag;
  float xmax = julias->xcen + julias->xymag;
  float ymin = julias->ycen - julias->xymag;
  float ymax = julias->ycen + julias->xymag;

  // Whole set should be within -1.2,1.2 to -.8 to 1.
  xmin = constrain(xmin, -1.2f, 1.2f);
  xmax = constrain(xmax, -1.2f, 1.2f);
  ymin = constrain(ymin, -0.8f, 1.0f);
  ymax = constrain(ymax, -0.8f, 1.0f);

  float dx;                       // Delta x is mapped to the matrix size.
  float dy;                       // Delta y is mapped to the matrix size.

  int maxIterations = 15;         // How many iterations per pixel before we give up. Make it 8 bits to match our range of colours.
  float maxCalc = 16.0;           // How big is each calculation allowed to be before we give up.

  maxIterations = SEGMENT.intensity/2;


  // Resize section on the fly for some animaton.
  reAl = -0.94299f;               // PixelBlaze example
  imAg = 0.3162f;

  reAl += (float)sin16_t(strip.now * 34) / 655340.f;
  imAg += (float)sin16_t(strip.now * 26) / 655340.f;

  dx = (xmax - xmin) / (cols);     // Scale the delta x and y values to our matrix size.
  dy = (ymax - ymin) / (rows);

  // Start y
  float y = ymin;
  for (int j = 0; j < rows; j++) {

    // Start x
    float x = xmin;
    for (int i = 0; i < cols; i++) {

      // Now we test, as we iterate z = z^2 + c does z tend towards infinity?
      float a = x;
      float b = y;
      int iter = 0;

      while (iter < maxIterations) {    // Here we determine whether or not we're out of bounds.
        float aa = a * a;
        float bb = b * b;
        float len = aa + bb;
        if (len > maxCalc) {            // |z| = sqrt(a^2+b^2) OR z^2 = a^2+b^2 to save on having to perform a square root.
          break;  // Bail
        }

       // This operation corresponds to z -> z^2+c where z=a+ib c=(x,y). Remember to use 'foil'.
        b = 2*a*b + imAg;
        a = aa - bb + reAl;
        iter++;
      } // while

      // We color each pixel based on how long it takes to get to infinity, or black if it never gets there.
      if (iter == maxIterations) {
        SEGMENT.setPixelColorXY(i, j, 0);
      } else {
        SEGMENT.setPixelColorXY(i, j, SEGMENT.color_from_palette(iter*255/maxIterations, false, PALETTE_SOLID_WRAP, 0));
      }
      x += dx;
    }
    y += dy;
  }
  if(SEGMENT.check1)
    SEGMENT.blur(100, true);

  return FRAMETIME;
} // mode_2DJulia()
static const char _data_FX_MODE_2DJULIA[] PROGMEM = "Julia@,Max iterations per pixel,X center,Y center,Area size, Blur;!;!;2;ix=24,c1=128,c2=128,c3=16";


//////////////////////////////
//     2D Lissajous         //
//////////////////////////////
uint16_t mode_2DLissajous(void) {            // By: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  SEGMENT.fadeToBlackBy(SEGMENT.intensity);
  uint_fast16_t phase = (strip.now * (1 + SEGENV.custom3)) /32;  // allow user to control rotation speed

  //for (int i=0; i < 4*(cols+rows); i ++) {
  for (int i=0; i < 256; i ++) {
    //float xlocn = float(sin8_t(now/4+i*(SEGMENT.speed>>5))) / 255.0f;
    //float ylocn = float(cos8_t(now/4+i*2)) / 255.0f;
    uint_fast8_t xlocn = sin8_t(phase/2 + (i*SEGMENT.speed)/32);
    uint_fast8_t ylocn = cos8_t(phase/2 + i*2);
    xlocn = (cols < 2) ? 1 : (map(2*xlocn, 0,511, 0,2*(cols-1)) +1) /2;    // softhack007: "(2* ..... +1) /2" for proper rounding
    ylocn = (rows < 2) ? 1 : (map(2*ylocn, 0,511, 0,2*(rows-1)) +1) /2;    // "rows > 1" is needed to avoid div/0 in map()
    SEGMENT.setPixelColorXY((uint8_t)xlocn, (uint8_t)ylocn, SEGMENT.color_from_palette(strip.now/100+i, false, PALETTE_SOLID_WRAP, 0));
  }
  SEGMENT.blur(SEGMENT.custom1 >> (1 + SEGMENT.check1 * 3), SEGMENT.check1);

  return FRAMETIME;
} // mode_2DLissajous()
static const char _data_FX_MODE_2DLISSAJOUS[] PROGMEM = "Lissajous@X frequency,Fade rate,Blur,,Speed,Smear;!;!;2;c1=0";


///////////////////////
//    2D Matrix      //
///////////////////////
uint16_t mode_2Dmatrix(void) {                  // Matrix2D. By Jeremy Williams. Adapted by Andrew Tuline & improved by merkisoft and ewowi, and softhack007.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const auto XY = [&](int x, int y) { return (x%cols) + (y%rows) * cols; };

  unsigned dataSize = (SEGMENT.length()+7) >> 3; //1 bit per LED for trails
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
    SEGENV.step = 0;
  }

  uint8_t fade = map(SEGMENT.custom1, 0, 255, 50, 250);    // equals trail size
  uint8_t speed = (256-SEGMENT.speed) >> map(min(rows, 150), 0, 150, 0, 3);    // slower speeds for small displays

  uint32_t spawnColor;
  uint32_t trailColor;
  if (SEGMENT.check1) {
    spawnColor = SEGCOLOR(0);
    trailColor = SEGCOLOR(1);
  } else {
    spawnColor = RGBW32(175,255,175,0);
    trailColor = RGBW32(27,130,39,0);
  }

  bool emptyScreen = true;
  if (strip.now - SEGENV.step >= speed) {
    SEGENV.step = strip.now;
    // move pixels one row down. Falling codes keep color and add trail pixels; all others pixels are faded
    // TODO: it would be better to paint trails idividually instead of relying on fadeToBlackBy()
    SEGMENT.fadeToBlackBy(fade);
    for (int row = rows-1; row >= 0; row--) {
      for (int col = 0; col < cols; col++) {
        unsigned index = XY(col, row) >> 3;
        unsigned bitNum = XY(col, row) & 0x07;
        if (bitRead(SEGENV.data[index], bitNum)) {
          SEGMENT.setPixelColorXY(col, row, trailColor);  // create trail
          bitClear(SEGENV.data[index], bitNum);
          if (row < rows-1) {
            SEGMENT.setPixelColorXY(col, row+1, spawnColor);
            index = XY(col, row+1) >> 3;
            bitNum = XY(col, row+1) & 0x07;
            bitSet(SEGENV.data[index], bitNum);
            emptyScreen = false;
          }
        }
      }
    }

    // spawn new falling code
    if (hw_random8() <= SEGMENT.intensity || emptyScreen) {
      uint8_t spawnX = hw_random8(cols);
      SEGMENT.setPixelColorXY(spawnX, 0, spawnColor);
      // update hint for next run
      unsigned index = XY(spawnX, 0) >> 3;
      unsigned bitNum = XY(spawnX, 0) & 0x07;
      bitSet(SEGENV.data[index], bitNum);
    }
  }

  return FRAMETIME;
} // mode_2Dmatrix()
static const char _data_FX_MODE_2DMATRIX[] PROGMEM = "Matrix@!,Spawning rate,Trail,,,Custom color;Spawn,Trail;;2";


/////////////////////////
//     2D Metaballs    //
/////////////////////////
uint16_t mode_2Dmetaballs(void) {   // Metaballs by Stefan Petrick. Cannot have one of the dimensions be 2 or less. Adapted by Andrew Tuline.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  float speed = 0.25f * (1+(SEGMENT.speed>>6));

  // get some 2 random moving points
  int x2 = map(perlin8(strip.now * speed, 25355, 685), 0, 255, 0, cols-1);
  int y2 = map(perlin8(strip.now * speed, 355, 11685), 0, 255, 0, rows-1);

  int x3 = map(perlin8(strip.now * speed, 55355, 6685), 0, 255, 0, cols-1);
  int y3 = map(perlin8(strip.now * speed, 25355, 22685), 0, 255, 0, rows-1);

  // and one Lissajou function
  int x1 = beatsin8_t(23 * speed, 0, cols-1);
  int y1 = beatsin8_t(28 * speed, 0, rows-1);

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      // calculate distances of the 3 points from actual pixel
      // and add them together with weightening
      unsigned dx = abs(x - x1);
      unsigned dy = abs(y - y1);
      unsigned dist = 2 * sqrt32_bw((dx * dx) + (dy * dy));

      dx = abs(x - x2);
      dy = abs(y - y2);
      dist += sqrt32_bw((dx * dx) + (dy * dy));

      dx = abs(x - x3);
      dy = abs(y - y3);
      dist += sqrt32_bw((dx * dx) + (dy * dy));

      // inverse result
      int color = dist ? 1000 / dist : 255;

      // map color between thresholds
      if (color > 0 and color < 60) {
        SEGMENT.setPixelColorXY(x, y, SEGMENT.color_from_palette(map(color * 9, 9, 531, 0, 255), false, PALETTE_SOLID_WRAP, 0));
      } else {
        SEGMENT.setPixelColorXY(x, y, SEGMENT.color_from_palette(0, false, PALETTE_SOLID_WRAP, 0));
      }
      // show the 3 points, too
      SEGMENT.setPixelColorXY(x1, y1, WHITE);
      SEGMENT.setPixelColorXY(x2, y2, WHITE);
      SEGMENT.setPixelColorXY(x3, y3, WHITE);
    }
  }

  return FRAMETIME;
} // mode_2Dmetaballs()
static const char _data_FX_MODE_2DMETABALLS[] PROGMEM = "Metaballs@!;;!;2";


//////////////////////
//    2D Noise      //
//////////////////////
uint16_t mode_2Dnoise(void) {                  // By Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  const unsigned scale  = SEGMENT.intensity+2;

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      uint8_t pixelHue8 = perlin8(x * scale, y * scale, strip.now / (16 - SEGMENT.speed/16));
      SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, pixelHue8));
    }
  }

  return FRAMETIME;
} // mode_2Dnoise()
static const char _data_FX_MODE_2DNOISE[] PROGMEM = "Noise2D@!,Scale;;!;2";


//////////////////////////////
//     2D Plasma Ball       //
//////////////////////////////
uint16_t mode_2DPlasmaball(void) {                   // By: Stepko https://editor.soulmatelights.com/gallery/659-plasm-ball , Modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  SEGMENT.fadeToBlackBy(SEGMENT.custom1>>2);
  uint_fast32_t t = (strip.now * 8) / (256 - SEGMENT.speed);  // optimized to avoid float
  for (int i = 0; i < cols; i++) {
    unsigned thisVal = perlin8(i * 30, t, t);
    unsigned thisMax = map(thisVal, 0, 255, 0, cols-1);
    for (int j = 0; j < rows; j++) {
      unsigned thisVal_ = perlin8(t, j * 30, t);
      unsigned thisMax_ = map(thisVal_, 0, 255, 0, rows-1);
      int x = (i + thisMax_ - cols / 2);
      int y = (j + thisMax - cols / 2);
      int cx = (i + thisMax_);
      int cy = (j + thisMax);

      SEGMENT.addPixelColorXY(i, j, ((x - y > -2) && (x - y < 2)) ||
                                    ((cols - 1 - x - y) > -2 && (cols - 1 - x - y < 2)) ||
                                    (cols - cx == 0) ||
                                    (cols - 1 - cx == 0) ||
                                    ((rows - cy == 0) ||
                                    (rows - 1 - cy == 0)) ? ColorFromPalette(SEGPALETTE, beat8(5), thisVal, LINEARBLEND) : CRGB::Black);
    }
  }
  SEGMENT.blur(SEGMENT.custom2>>5);

  return FRAMETIME;
} // mode_2DPlasmaball()
static const char _data_FX_MODE_2DPLASMABALL[] PROGMEM = "Plasma Ball@Speed,,Fade,Blur;;!;2";


////////////////////////////////
//  2D Polar Lights           //
////////////////////////////////

uint16_t mode_2DPolarLights(void) {        // By: Kostyantyn Matviyevskyy  https://editor.soulmatelights.com/gallery/762-polar-lights , Modified by: Andrew Tuline & @dedehai (palette support)
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
    SEGENV.step = 0;
  }

  float adjustHeight = (float)map(rows, 8, 32, 28, 12); // maybe use mapf() ???
  unsigned adjScale = map(cols, 8, 64, 310, 63);
  unsigned _scale = map(SEGMENT.intensity, 0, 255, 30, adjScale);
  int _speed = map(SEGMENT.speed, 0, 255, 128, 16);

  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      SEGENV.step++;
      uint8_t palindex = qsub8(perlin8((SEGENV.step%2) + x * _scale, y * 16 + SEGENV.step % 16, SEGENV.step / _speed), fabsf((float)rows / 2.0f - (float)y) * adjustHeight);
      uint8_t palbrightness = palindex;
      if(SEGMENT.check1) palindex = 255 - palindex; //flip palette
      SEGMENT.setPixelColorXY(x, y, SEGMENT.color_from_palette(palindex, false, false, 255, palbrightness));
    }
  }

  return FRAMETIME;
} // mode_2DPolarLights()
static const char _data_FX_MODE_2DPOLARLIGHTS[] PROGMEM = "Polar Lights@!,Scale,,,,Flip Palette;;!;2;pal=71";


/////////////////////////
//     2D Pulser       //
/////////////////////////
uint16_t mode_2DPulser(void) {                       // By: ldirko   https://editor.soulmatelights.com/gallery/878-pulse-test , modifed by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  SEGMENT.fadeToBlackBy(8 - (SEGMENT.intensity>>5));
  uint32_t a = strip.now / (18 - SEGMENT.speed / 16);
  int x = (a / 14) % cols;
  int y = map((sin8_t(a * 5) + sin8_t(a * 4) + sin8_t(a * 2)), 0, 765, rows-1, 0);
  SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, map(y, 0, rows-1, 0, 255), 255, LINEARBLEND));

  SEGMENT.blur(SEGMENT.intensity>>4);

  return FRAMETIME;
} // mode_2DPulser()
static const char _data_FX_MODE_2DPULSER[] PROGMEM = "Pulser@!,Blur;;!;2";


/////////////////////////
//     2D Sindots      //
/////////////////////////
uint16_t mode_2DSindots(void) {                             // By: ldirko   https://editor.soulmatelights.com/gallery/597-sin-dots , modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  SEGMENT.fadeToBlackBy((SEGMENT.custom1>>3) + (SEGMENT.check1 * 24));

  byte t1 = strip.now / (257 - SEGMENT.speed); // 20;
  byte t2 = sin8_t(t1) / 4 * 2;
  for (int i = 0; i < 13; i++) {
    int x = sin8_t(t1 + i * SEGMENT.intensity/8)*(cols-1)/255;  // max index now 255x15/255=15!
    int y = sin8_t(t2 + i * SEGMENT.intensity/8)*(rows-1)/255;  // max index now 255x15/255=15!
    SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, i * 255 / 13, 255, LINEARBLEND));
  }
  SEGMENT.blur(SEGMENT.custom2 >> (3 + SEGMENT.check1), SEGMENT.check1);

  return FRAMETIME;
} // mode_2DSindots()
static const char _data_FX_MODE_2DSINDOTS[] PROGMEM = "Sindots@!,Dot distance,Fade rate,Blur,,Smear;;!;2;";


//////////////////////////////
//     2D Squared Swirl     //
//////////////////////////////
// custom3 affects the blur amount.
uint16_t mode_2Dsquaredswirl(void) {            // By: Mark Kriegsman. https://gist.github.com/kriegsman/368b316c55221134b160
                                                          // Modifed by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  const uint8_t kBorderWidth = 2;

  SEGMENT.fadeToBlackBy(1 + SEGMENT.intensity / 5);
  SEGMENT.blur(SEGMENT.custom3>>1);

  // Use two out-of-sync sine waves
  int i = beatsin8_t(19, kBorderWidth, cols-kBorderWidth);
  int j = beatsin8_t(22, kBorderWidth, cols-kBorderWidth);
  int k = beatsin8_t(17, kBorderWidth, cols-kBorderWidth);
  int m = beatsin8_t(18, kBorderWidth, rows-kBorderWidth);
  int n = beatsin8_t(15, kBorderWidth, rows-kBorderWidth);
  int p = beatsin8_t(20, kBorderWidth, rows-kBorderWidth);

  SEGMENT.addPixelColorXY(i, m, ColorFromPalette(SEGPALETTE, strip.now/29, 255, LINEARBLEND));
  SEGMENT.addPixelColorXY(j, n, ColorFromPalette(SEGPALETTE, strip.now/41, 255, LINEARBLEND));
  SEGMENT.addPixelColorXY(k, p, ColorFromPalette(SEGPALETTE, strip.now/73, 255, LINEARBLEND));

  return FRAMETIME;
} // mode_2Dsquaredswirl()
static const char _data_FX_MODE_2DSQUAREDSWIRL[] PROGMEM = "Squared Swirl@,Fade,,,Blur;;!;2";


//////////////////////////////
//     2D Sun Radiation     //
//////////////////////////////
uint16_t mode_2DSunradiation(void) {                   // By: ldirko https://editor.soulmatelights.com/gallery/599-sun-radiation  , modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (!SEGENV.allocateData(sizeof(byte)*(cols+2)*(rows+2))) return mode_static(); //allocation failed
  byte *bump = reinterpret_cast<byte*>(SEGENV.data);

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  unsigned long t = strip.now / 4;
  unsigned index = 0;
  uint8_t someVal = SEGMENT.speed/4;             // Was 25.
  for (int j = 0; j < (rows + 2); j++) {
    for (int i = 0; i < (cols + 2); i++) {
      //byte col = (inoise8_raw(i * someVal, j * someVal, t)) / 2;
      byte col = ((int16_t)perlin8(i * someVal, j * someVal, t) - 0x7F) / 3;
      bump[index++] = col;
    }
  }

  int yindex = cols + 3;
  int vly = -(rows / 2 + 1);
  for (int y = 0; y < rows; y++) {
    ++vly;
    int vlx = -(cols / 2 + 1);
    for (int x = 0; x < cols; x++) {
      ++vlx;
      int nx = bump[x + yindex + 1] - bump[x + yindex - 1];
      int ny = bump[x + yindex + (cols + 2)] - bump[x + yindex - (cols + 2)];
      unsigned difx = abs8(vlx * 7 - nx);
      unsigned dify = abs8(vly * 7 - ny);
      int temp = difx * difx + dify * dify;
      int col = 255 - temp / 8; //8 its a size of effect
      if (col < 0) col = 0;
      SEGMENT.setPixelColorXY(x, y, HeatColor(col / (3.0f-(float)(SEGMENT.intensity)/128.f)));
    }
    yindex += (cols + 2);
  }

  return FRAMETIME;
} // mode_2DSunradiation()
static const char _data_FX_MODE_2DSUNRADIATION[] PROGMEM = "Sun Radiation@Variance,Brightness;;;2";


/////////////////////////
//     2D Tartan       //
/////////////////////////
uint16_t mode_2Dtartan(void) {          // By: Elliott Kember  https://editor.soulmatelights.com/gallery/3-tartan , Modified by: Andrew Tuline
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  uint8_t hue, bri;
  size_t intensity;
  int offsetX = beatsin16_t(3, -360, 360);
  int offsetY = beatsin16_t(2, -360, 360);
  int sharpness = SEGMENT.custom3 / 8; // 0-3

  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      hue = x * beatsin16_t(10, 1, 10) + offsetY;
      intensity = bri = sin8_t(x * SEGMENT.speed/2 + offsetX);
      for (int i=0; i<sharpness; i++) intensity *= bri;
      intensity >>= 8*sharpness;
      SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, hue, intensity, LINEARBLEND));
      hue = y * 3 + offsetX;
      intensity = bri = sin8_t(y * SEGMENT.intensity/2 + offsetY);
      for (int i=0; i<sharpness; i++) intensity *= bri;
      intensity >>= 8*sharpness;
      SEGMENT.addPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, hue, intensity, LINEARBLEND));
    }
  }

  return FRAMETIME;
} // mode_2DTartan()
static const char _data_FX_MODE_2DTARTAN[] PROGMEM = "Tartan@X scale,Y scale,,,Sharpness;;!;2";


/////////////////////////
//     2D spaceships   //
/////////////////////////
uint16_t mode_2Dspaceships(void) {    //// Space ships by stepko (c)05.02.21 [https://editor.soulmatelights.com/gallery/639-space-ships], adapted by Blaz Kristan (AKA blazoncek)
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  uint32_t tb = strip.now >> 12;  // every ~4s
  if (tb > SEGENV.step) {
    int dir = ++SEGENV.aux0;
    dir  += (int)hw_random8(3)-1;
    if      (dir > 7) SEGENV.aux0 = 0;
    else if (dir < 0) SEGENV.aux0 = 7;
    else              SEGENV.aux0 = dir;
    SEGENV.step = tb + hw_random8(4);
  }

  SEGMENT.fadeToBlackBy(map(SEGMENT.speed, 0, 255, 248, 16));
  SEGMENT.move(SEGENV.aux0, 1);

  for (size_t i = 0; i < 8; i++) {
    int x = beatsin8_t(12 + i, 2, cols - 3);
    int y = beatsin8_t(15 + i, 2, rows - 3);
    uint32_t color = ColorFromPalette(SEGPALETTE, beatsin8_t(12 + i, 0, 255), 255);
    SEGMENT.addPixelColorXY(x, y, color);
    if (cols > 24 || rows > 24) {
      SEGMENT.addPixelColorXY(x+1, y, color);
      SEGMENT.addPixelColorXY(x-1, y, color);
      SEGMENT.addPixelColorXY(x, y+1, color);
      SEGMENT.addPixelColorXY(x, y-1, color);
    }
  }
  SEGMENT.blur(SEGMENT.intensity >> 3, SEGMENT.check1);

  return FRAMETIME;
}
static const char _data_FX_MODE_2DSPACESHIPS[] PROGMEM = "Spaceships@!,Blur,,,,Smear;;!;2";


/////////////////////////
//     2D Crazy Bees   //
/////////////////////////
//// Crazy bees by stepko (c)12.02.21 [https://editor.soulmatelights.com/gallery/651-crazy-bees], adapted by Blaz Kristan (AKA blazoncek), improved by @dedehai
#define MAX_BEES 5
uint16_t mode_2Dcrazybees(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  byte n = MIN(MAX_BEES, (rows * cols) / 256 + 1);

  typedef struct Bee {
    uint8_t posX, posY, aimX, aimY, hue;
    int8_t deltaX, deltaY, signX, signY, error;
    void aimed(uint16_t w, uint16_t h) {
      //random16_set_seed(millis());
      aimX   = random8(0, w);
      aimY   = random8(0, h);
      hue    = random8();
      deltaX = abs(aimX - posX);
      deltaY = abs(aimY - posY);
      signX  = posX < aimX ? 1 : -1;
      signY  = posY < aimY ? 1 : -1;
      error  = deltaX - deltaY;
    };
  } bee_t;

  if (!SEGENV.allocateData(sizeof(bee_t)*MAX_BEES)) return mode_static(); //allocation failed
  bee_t *bee = reinterpret_cast<bee_t*>(SEGENV.data);

  if (SEGENV.call == 0) {
    random16_set_seed(strip.now);
    for (size_t i = 0; i < n; i++) {
      bee[i].posX = random8(0, cols);
      bee[i].posY = random8(0, rows);
      bee[i].aimed(cols, rows);
    }
  }

  if (strip.now > SEGENV.step) {
    SEGENV.step = strip.now + (FRAMETIME * 16 / ((SEGMENT.speed>>4)+1));
    SEGMENT.fadeToBlackBy(32 + ((SEGMENT.check1*SEGMENT.intensity) / 25));
    SEGMENT.blur(SEGMENT.intensity / (2 + SEGMENT.check1 * 9), SEGMENT.check1);
    for (size_t i = 0; i < n; i++) {
      uint32_t flowerCcolor = SEGMENT.color_from_palette(bee[i].hue, false, true, 255);
      SEGMENT.addPixelColorXY(bee[i].aimX + 1, bee[i].aimY, flowerCcolor);
      SEGMENT.addPixelColorXY(bee[i].aimX, bee[i].aimY + 1, flowerCcolor);
      SEGMENT.addPixelColorXY(bee[i].aimX - 1, bee[i].aimY, flowerCcolor);
      SEGMENT.addPixelColorXY(bee[i].aimX, bee[i].aimY - 1, flowerCcolor);
      if (bee[i].posX != bee[i].aimX || bee[i].posY != bee[i].aimY) {
        SEGMENT.setPixelColorXY(bee[i].posX, bee[i].posY, CRGB(CHSV(bee[i].hue, 60, 255)));
        int error2 = bee[i].error * 2;
        if (error2 > -bee[i].deltaY) {
          bee[i].error -= bee[i].deltaY;
          bee[i].posX += bee[i].signX;
        }
        if (error2 < bee[i].deltaX) {
          bee[i].error += bee[i].deltaX;
          bee[i].posY += bee[i].signY;
        }
      } else {
        bee[i].aimed(cols, rows);
      }
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_2DCRAZYBEES[] PROGMEM = "Crazy Bees@!,Blur,,,,Smear;;!;2;pal=11,ix=0";
#undef MAX_BEES

#ifdef WLED_PS_DONT_REPLACE_FX
/////////////////////////
//     2D Ghost Rider  //
/////////////////////////
//// Ghost Rider by stepko (c)2021 [https://editor.soulmatelights.com/gallery/716-ghost-rider], adapted by Blaz Kristan (AKA blazoncek)
#define LIGHTERS_AM 64  // max lighters (adequate for 32x32 matrix)
uint16_t mode_2Dghostrider(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  typedef struct Lighter {
    int16_t  gPosX;
    int16_t  gPosY;
    uint16_t gAngle;
    int8_t   angleSpeed;
    uint16_t lightersPosX[LIGHTERS_AM];
    uint16_t lightersPosY[LIGHTERS_AM];
    uint16_t Angle[LIGHTERS_AM];
    uint16_t time[LIGHTERS_AM];
    bool     reg[LIGHTERS_AM];
    int8_t   Vspeed;
  } lighter_t;

  if (!SEGENV.allocateData(sizeof(lighter_t))) return mode_static(); //allocation failed
  lighter_t *lighter = reinterpret_cast<lighter_t*>(SEGENV.data);

  const size_t maxLighters = min(cols + rows, LIGHTERS_AM);

  if (SEGENV.aux0 != cols || SEGENV.aux1 != rows) {
    SEGENV.aux0 = cols;
    SEGENV.aux1 = rows;
    lighter->angleSpeed = hw_random8(0,20) - 10;
    lighter->gAngle = hw_random16();
    lighter->Vspeed = 5;
    lighter->gPosX = (cols/2) * 10;
    lighter->gPosY = (rows/2) * 10;
    for (size_t i = 0; i < maxLighters; i++) {
      lighter->lightersPosX[i] = lighter->gPosX;
      lighter->lightersPosY[i] = lighter->gPosY + i;
      lighter->time[i] = i * 2;
      lighter->reg[i] = false;
    }
  }

  if (strip.now > SEGENV.step) {
    SEGENV.step = strip.now + 1024 / (cols+rows);

    SEGMENT.fadeToBlackBy((SEGMENT.speed>>2)+64);

    CRGB color = CRGB::White;
    SEGMENT.wu_pixel(lighter->gPosX * 256 / 10, lighter->gPosY * 256 / 10, color);

    lighter->gPosX += lighter->Vspeed * sin_t(radians(lighter->gAngle));
    lighter->gPosY += lighter->Vspeed * cos_t(radians(lighter->gAngle));
    lighter->gAngle += lighter->angleSpeed;
    if (lighter->gPosX < 0)               lighter->gPosX = (cols - 1) * 10;
    if (lighter->gPosX > (cols - 1) * 10) lighter->gPosX = 0;
    if (lighter->gPosY < 0)               lighter->gPosY = (rows - 1) * 10;
    if (lighter->gPosY > (rows - 1) * 10) lighter->gPosY = 0;
    for (size_t i = 0; i < maxLighters; i++) {
      lighter->time[i] += hw_random8(5, 20);
      if (lighter->time[i] >= 255 ||
        (lighter->lightersPosX[i] <= 0) ||
          (lighter->lightersPosX[i] >= (cols - 1) * 10) ||
          (lighter->lightersPosY[i] <= 0) ||
          (lighter->lightersPosY[i] >= (rows - 1) * 10)) {
        lighter->reg[i] = true;
      }
      if (lighter->reg[i]) {
        lighter->lightersPosY[i] = lighter->gPosY;
        lighter->lightersPosX[i] = lighter->gPosX;
        lighter->Angle[i] = lighter->gAngle + ((int)hw_random8(20) - 10);
        lighter->time[i] = 0;
        lighter->reg[i] = false;
      } else {
        lighter->lightersPosX[i] += -7 * sin_t(radians(lighter->Angle[i]));
        lighter->lightersPosY[i] += -7 * cos_t(radians(lighter->Angle[i]));
      }
      SEGMENT.wu_pixel(lighter->lightersPosX[i] * 256 / 10, lighter->lightersPosY[i] * 256 / 10, ColorFromPalette(SEGPALETTE, (256 - lighter->time[i])));
    }
    SEGMENT.blur(SEGMENT.intensity>>3);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_2DGHOSTRIDER[] PROGMEM = "Ghost Rider@Fade rate,Blur;;!;2";
#undef LIGHTERS_AM

////////////////////////////
//     2D Floating Blobs  //
////////////////////////////
//// Floating Blobs by stepko (c)2021 [https://editor.soulmatelights.com/gallery/573-blobs], adapted by Blaz Kristan (AKA blazoncek)
#define MAX_BLOBS 8
uint16_t mode_2Dfloatingblobs(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  typedef struct Blob {
    float x[MAX_BLOBS], y[MAX_BLOBS];
    float sX[MAX_BLOBS], sY[MAX_BLOBS]; // speed
    float r[MAX_BLOBS];
    bool grow[MAX_BLOBS];
    byte color[MAX_BLOBS];
  } blob_t;

  size_t Amount = (SEGMENT.intensity>>5) + 1; // NOTE: be sure to update MAX_BLOBS if you change this

  if (!SEGENV.allocateData(sizeof(blob_t))) return mode_static(); //allocation failed
  blob_t *blob = reinterpret_cast<blob_t*>(SEGENV.data);

  if (SEGENV.aux0 != cols || SEGENV.aux1 != rows) {
    SEGENV.aux0 = cols; // re-initialise if virtual size changes
    SEGENV.aux1 = rows;
    //SEGMENT.fill(BLACK);
    for (size_t i = 0; i < MAX_BLOBS; i++) {
      blob->r[i]  = hw_random8(1, cols>8 ? (cols/4) : 2);
      blob->sX[i] = (float) hw_random8(3, cols) / (float)(256 - SEGMENT.speed); // speed x
      blob->sY[i] = (float) hw_random8(3, rows) / (float)(256 - SEGMENT.speed); // speed y
      blob->x[i]  = hw_random8(0, cols-1);
      blob->y[i]  = hw_random8(0, rows-1);
      blob->color[i] = hw_random8();
      blob->grow[i]  = (blob->r[i] < 1.f);
      if (blob->sX[i] == 0) blob->sX[i] = 1;
      if (blob->sY[i] == 0) blob->sY[i] = 1;
    }
  }

  SEGMENT.fadeToBlackBy((SEGMENT.custom2>>3)+1);

  // Bounce balls around
  for (size_t i = 0; i < Amount; i++) {
    if (SEGENV.step < strip.now) blob->color[i] = add8(blob->color[i], 4); // slowly change color
    // change radius if needed
    if (blob->grow[i]) {
      // enlarge radius until it is >= 4
      blob->r[i] += (fabsf(blob->sX[i]) > fabsf(blob->sY[i]) ? fabsf(blob->sX[i]) : fabsf(blob->sY[i])) * 0.05f;
      if (blob->r[i] >= MIN(cols/4.f,2.f)) {
        blob->grow[i] = false;
      }
    } else {
      // reduce radius until it is < 1
      blob->r[i] -= (fabsf(blob->sX[i]) > fabsf(blob->sY[i]) ? fabsf(blob->sX[i]) : fabsf(blob->sY[i])) * 0.05f;
      if (blob->r[i] < 1.f) {
        blob->grow[i] = true;
      }
    }
    uint32_t c = SEGMENT.color_from_palette(blob->color[i], false, false, 0);
    if (blob->r[i] > 1.f) SEGMENT.fillCircle(roundf(blob->x[i]), roundf(blob->y[i]), roundf(blob->r[i]), c);
    else                  SEGMENT.setPixelColorXY((int)roundf(blob->x[i]), (int)roundf(blob->y[i]), c);
    // move x
    if (blob->x[i] + blob->r[i] >= cols - 1) blob->x[i] += (blob->sX[i] * ((cols - 1 - blob->x[i]) / blob->r[i] + 0.005f));
    else if (blob->x[i] - blob->r[i] <= 0)   blob->x[i] += (blob->sX[i] * (blob->x[i] / blob->r[i] + 0.005f));
    else                                     blob->x[i] += blob->sX[i];
    // move y
    if (blob->y[i] + blob->r[i] >= rows - 1) blob->y[i] += (blob->sY[i] * ((rows - 1 - blob->y[i]) / blob->r[i] + 0.005f));
    else if (blob->y[i] - blob->r[i] <= 0)   blob->y[i] += (blob->sY[i] * (blob->y[i] / blob->r[i] + 0.005f));
    else                                     blob->y[i] += blob->sY[i];
    // bounce x
    if (blob->x[i] < 0.01f) {
      blob->sX[i] = (float)hw_random8(3, cols) / (256 - SEGMENT.speed);
      blob->x[i]  = 0.01f;
    } else if (blob->x[i] > (float)cols - 1.01f) {
      blob->sX[i] = (float)hw_random8(3, cols) / (256 - SEGMENT.speed);
      blob->sX[i] = -blob->sX[i];
      blob->x[i]  = (float)cols - 1.01f;
    }
    // bounce y
    if (blob->y[i] < 0.01f) {
      blob->sY[i] = (float)hw_random8(3, rows) / (256 - SEGMENT.speed);
      blob->y[i]  = 0.01f;
    } else if (blob->y[i] > (float)rows - 1.01f) {
      blob->sY[i] = (float)hw_random8(3, rows) / (256 - SEGMENT.speed);
      blob->sY[i] = -blob->sY[i];
      blob->y[i]  = (float)rows - 1.01f;
    }
  }
  SEGMENT.blur(SEGMENT.custom1>>2);

  if (SEGENV.step < strip.now) SEGENV.step = strip.now + 2000; // change colors every 2 seconds

  return FRAMETIME;
}
static const char _data_FX_MODE_2DBLOBS[] PROGMEM = "Blobs@!,# blobs,Blur,Trail;!;!;2;c1=8";
#undef MAX_BLOBS
#endif // WLED_PS_DONT_REPLACE_FX

////////////////////////////
//     2D Scrolling text  //
////////////////////////////
uint16_t mode_2Dscrollingtext(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  unsigned letterWidth, rotLW;
  unsigned letterHeight, rotLH;
  switch (map(SEGMENT.custom2, 0, 255, 1, 5)) {
    default:
    case 1: letterWidth = 4; letterHeight =  6; break;
    case 2: letterWidth = 5; letterHeight =  8; break;
    case 3: letterWidth = 6; letterHeight =  8; break;
    case 4: letterWidth = 7; letterHeight =  9; break;
    case 5: letterWidth = 5; letterHeight = 12; break;
  }
  // letters are rotated
  const int8_t rotate = map(SEGMENT.custom3, 0, 31, -2, 2);
  if (rotate == 1 || rotate == -1) {
    rotLH = letterWidth;
    rotLW = letterHeight;
  } else {
    rotLW = letterWidth;
    rotLH = letterHeight;
  }

  char text[WLED_MAX_SEGNAME_LEN+1] = {'\0'};
  if (SEGMENT.name) for (size_t i=0,j=0; i<strlen(SEGMENT.name); i++) if (SEGMENT.name[i]>31 && SEGMENT.name[i]<128) text[j++] = SEGMENT.name[i];
  const bool zero = strchr(text, '0') != nullptr;

  char sec[5];
  int  AmPmHour = hour(localTime);
  bool isitAM = true;
  if (useAMPM) {
    if (AmPmHour > 11) { AmPmHour -= 12; isitAM = false; }
    if (AmPmHour == 0) { AmPmHour  = 12; }
    sprintf_P(sec, PSTR(" %2s"), (isitAM ? "AM" : "PM"));
  } else {
    sprintf_P(sec, PSTR(":%02d"), second(localTime));
  }

  if (!strlen(text)) { // fallback if empty segment name: display date and time
    sprintf_P(text, PSTR("%s %d, %d %d:%02d%s"), monthShortStr(month(localTime)), day(localTime), year(localTime), AmPmHour, minute(localTime), sec);
  } else {
    if (text[0] == '#') for (auto &c : text) c = std::toupper(c);
    if      (!strncmp_P(text,PSTR("#DATE"),5)) sprintf_P(text, zero?PSTR("%02d.%02d.%04d"):PSTR("%d.%d.%d"),   day(localTime),   month(localTime),  year(localTime));
    else if (!strncmp_P(text,PSTR("#DDMM"),5)) sprintf_P(text, zero?PSTR("%02d.%02d")     :PSTR("%d.%d"),      day(localTime),   month(localTime));
    else if (!strncmp_P(text,PSTR("#MMDD"),5)) sprintf_P(text, zero?PSTR("%02d/%02d")     :PSTR("%d/%d"),      month(localTime), day(localTime));
    else if (!strncmp_P(text,PSTR("#TIME"),5)) sprintf_P(text, zero?PSTR("%02d:%02d%s")   :PSTR("%2d:%02d%s"), AmPmHour,         minute(localTime), sec);
    else if (!strncmp_P(text,PSTR("#HHMM"),5)) sprintf_P(text, zero?PSTR("%02d:%02d")     :PSTR("%d:%02d"),    AmPmHour,         minute(localTime));
    else if (!strncmp_P(text,PSTR("#HH"),3))   sprintf  (text, zero?    ("%02d")          :    ("%d"),         AmPmHour);
    else if (!strncmp_P(text,PSTR("#MM"),3))   sprintf  (text, zero?    ("%02d")          :    ("%d"),         minute(localTime));
    else if (!strncmp_P(text,PSTR("#SS"),3))   sprintf  (text,          ("%02d")                     ,         second(localTime));
    else if (!strncmp_P(text,PSTR("#DD"),3))   sprintf  (text, zero?    ("%02d")          :    ("%d"),         day(localTime));
    else if (!strncmp_P(text,PSTR("#DAY"),4))  sprintf  (text,          ("%s")                       ,         dayShortStr(day(localTime)));
    else if (!strncmp_P(text,PSTR("#DDDD"),5)) sprintf  (text,          ("%s")                       ,         dayStr(day(localTime)));
    else if (!strncmp_P(text,PSTR("#DAYL"),5)) sprintf  (text,          ("%s")                       ,         dayStr(day(localTime)));
    else if (!strncmp_P(text,PSTR("#MO"),3))   sprintf  (text, zero?    ("%02d")          :    ("%d"),         month(localTime));
    else if (!strncmp_P(text,PSTR("#MON"),4))  sprintf  (text,          ("%s")                       ,         monthShortStr(month(localTime)));
    else if (!strncmp_P(text,PSTR("#MMMM"),5)) sprintf  (text,          ("%s")                       ,         monthStr(month(localTime)));
    else if (!strncmp_P(text,PSTR("#YY"),3))   sprintf  (text,          ("%02d")                     ,         year(localTime)%100);
    else if (!strncmp_P(text,PSTR("#YYYY"),5)) sprintf_P(text, zero?PSTR("%04d")          :    ("%d"),         year(localTime));
  }

  const int  numberOfLetters = strlen(text);
  int width = (numberOfLetters * rotLW);
  int yoffset = map(SEGMENT.intensity, 0, 255, -rows/2, rows/2) + (rows-rotLH)/2;
  if (width <= cols) {
    // scroll vertically (e.g. ^^ Way out ^^) if it fits
    int speed = map(SEGMENT.speed, 0, 255, 5000, 1000);
    int frac = strip.now % speed + 1;
    if (SEGMENT.intensity == 255) {
      yoffset = (2 * frac * rows)/speed - rows;
    } else if (SEGMENT.intensity == 0) {
      yoffset = rows - (2 * frac * rows)/speed;
    }
  }

  if (SEGENV.step < strip.now) {
    // calculate start offset
    if (width > cols) {
      if (SEGMENT.check3) {
        if (SEGENV.aux0 == 0) SEGENV.aux0  = width + cols - 1;
        else                --SEGENV.aux0;
      } else                ++SEGENV.aux0 %= width + cols;
    } else                    SEGENV.aux0  = (cols + width)/2;
    ++SEGENV.aux1 &= 0xFF; // color shift
    SEGENV.step = strip.now + map(SEGMENT.speed, 0, 255, 250, 50); // shift letters every ~250ms to ~50ms
  }

  SEGMENT.fade_out(255 - (SEGMENT.custom1>>4));  // trail
  uint32_t col1 = SEGMENT.color_from_palette(SEGENV.aux1, false, PALETTE_SOLID_WRAP, 0);
  uint32_t col2 = BLACK;
  // if gradient is selected and palette is default (0) drawCharacter() uses gradient from SEGCOLOR(0) to SEGCOLOR(2)
  // otherwise col2 == BLACK means use currently selected palette for gradient
  // if gradient is not selected set both colors the same
  if (SEGMENT.check1) { // use gradient
    if (SEGMENT.palette == 0) { // use colors for gradient
      col1 = SEGCOLOR(0);
      col2 = SEGCOLOR(2);
    }
  } else col2 = col1; // force characters to use single color (from palette)

  for (int i = 0; i < numberOfLetters; i++) {
    int xoffset = int(cols) - int(SEGENV.aux0) + rotLW*i;
    if (xoffset + rotLW < 0) continue; // don't draw characters off-screen
    SEGMENT.drawCharacter(text[i], xoffset, yoffset, letterWidth, letterHeight, col1, col2, rotate);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_2DSCROLLTEXT[] PROGMEM = "Scrolling Text@!,Y Offset,Trail,Font size,Rotate,Gradient,,Reverse;!,!,Gradient;!;2;ix=128,c1=0,rev=0,mi=0,rY=0,mY=0";


////////////////////////////
//     2D Drift Rose      //
////////////////////////////
//// Drift Rose by stepko (c)2021 [https://editor.soulmatelights.com/gallery/1369-drift-rose-pattern], adapted by Blaz Kristan (AKA blazoncek) improved by @dedehai
uint16_t mode_2Ddriftrose(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  const float CX = (cols-cols%2)/2.f - .5f;
  const float CY = (rows-rows%2)/2.f - .5f;
  const float L = min(cols, rows) / 2.f;

  SEGMENT.fadeToBlackBy(32+(SEGMENT.speed>>3));
  for (size_t i = 1; i < 37; i++) {
    float angle = radians(i * 10);
    uint32_t x = (CX + (sin_t(angle) * (beatsin8_t(i, 0, L*2)-L))) * 255.f;
    uint32_t y = (CY + (cos_t(angle) * (beatsin8_t(i, 0, L*2)-L))) * 255.f;
    if(SEGMENT.palette == 0) SEGMENT.wu_pixel(x, y, CHSV(i * 10, 255, 255));
    else SEGMENT.wu_pixel(x, y, ColorFromPalette(SEGPALETTE, i * 10));
  }
  SEGMENT.blur(SEGMENT.intensity >> 4, SEGMENT.check1);

  return FRAMETIME;
}
static const char _data_FX_MODE_2DDRIFTROSE[] PROGMEM = "Drift Rose@Fade,Blur,,,,Smear;;!;2;pal=11";

/////////////////////////////
//  2D PLASMA ROTOZOOMER   //
/////////////////////////////
// Plasma Rotozoomer by ldirko (c)2020 [https://editor.soulmatelights.com/gallery/457-plasma-rotozoomer], adapted for WLED by Blaz Kristan (AKA blazoncek)
uint16_t mode_2Dplasmarotozoom() {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  unsigned dataSize = SEGMENT.length() + sizeof(float);
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  float *a = reinterpret_cast<float*>(SEGENV.data);
  byte *plasma = reinterpret_cast<byte*>(SEGENV.data+sizeof(float));

  unsigned ms = strip.now/15;  

  // plasma
  for (int j = 0; j < rows; j++) {
    int index = j*cols;
    for (int i = 0; i < cols; i++) {
      if (SEGMENT.check1) plasma[index+i] = (i * 4 ^ j * 4) + ms / 6;
      else                plasma[index+i] = inoise8(i * 40, j * 40, ms);
    }
  }

  // rotozoom
  float f       = (sin_t(*a/2)+((128-SEGMENT.intensity)/128.0f)+1.1f)/1.5f;  // scale factor
  float kosinus = cos_t(*a) * f;
  float sinus   = sin_t(*a) * f;
  for (int i = 0; i < cols; i++) {
    float u1 = i * kosinus;
    float v1 = i * sinus;
    for (int j = 0; j < rows; j++) {
        byte u = abs8(u1 - j * sinus) % cols;
        byte v = abs8(v1 + j * kosinus) % rows;
        SEGMENT.setPixelColorXY(i, j, SEGMENT.color_from_palette(plasma[v*cols+u], false, PALETTE_SOLID_WRAP, 255));
    }
  }
  *a -= 0.03f + float(SEGENV.speed-128)*0.0002f;  // rotation speed
  if(*a < -6283.18530718f) *a += 6283.18530718f; // 1000*2*PI, protect sin/cos from very large input float values (will give wrong results)

  return FRAMETIME;
}
static const char _data_FX_MODE_2DPLASMAROTOZOOM[] PROGMEM = "Rotozoomer@!,Scale,,,,Alt;;!;2;pal=54";

#endif // WLED_DISABLE_2D


///////////////////////////////////////////////////////////////////////////////
/********************     audio enhanced routines     ************************/
///////////////////////////////////////////////////////////////////////////////


/////////////////////////////////
//     * Ripple Peak           //
/////////////////////////////////
uint16_t mode_ripplepeak(void) {                // * Ripple peak. By Andrew Tuline.
                                                          // This currently has no controls.
  #define MAXSTEPS 16                                     // Case statement wouldn't allow a variable.

  unsigned maxRipples = 16;
  unsigned dataSize = sizeof(Ripple) * maxRipples;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  Ripple* ripples = reinterpret_cast<Ripple*>(SEGENV.data);

  um_data_t *um_data = getAudioData();
  uint8_t samplePeak    = *(uint8_t*)um_data->u_data[3];
  #ifdef ESP32
  float   FFT_MajorPeak = *(float*)  um_data->u_data[4];
  #endif
  uint8_t *maxVol       =  (uint8_t*)um_data->u_data[6];
  uint8_t *binNum       =  (uint8_t*)um_data->u_data[7];

  // printUmData();

  if (SEGENV.call == 0) {
    SEGMENT.custom1 = *binNum;
    SEGMENT.custom2 = *maxVol * 2;
  }

  *binNum = SEGMENT.custom1;                              // Select a bin.
  *maxVol = SEGMENT.custom2 / 2;                          // Our volume comparator.

  SEGMENT.fade_out(240);                                  // Lower frame rate means less effective fading than FastLED
  SEGMENT.fade_out(240);

  for (int i = 0; i < SEGMENT.intensity/16; i++) {   // Limit the number of ripples.
    if (samplePeak) ripples[i].state = 255;

    switch (ripples[i].state) {
      case 254:     // Inactive mode
        break;

      case 255:                                           // Initialize ripple variables.
        ripples[i].pos = hw_random16(SEGLEN);
        #ifdef ESP32
          if (FFT_MajorPeak > 1)                          // log10(0) is "forbidden" (throws exception)
          ripples[i].color = (int)(log10f(FFT_MajorPeak)*128);
          else ripples[i].color = 0;
        #else
          ripples[i].color = hw_random8();
        #endif
        ripples[i].state = 0;
        break;

      case 0:
        SEGMENT.setPixelColor(ripples[i].pos, SEGMENT.color_from_palette(ripples[i].color, false, PALETTE_SOLID_WRAP, 0));
        ripples[i].state++;
        break;

      case MAXSTEPS:                                      // At the end of the ripples. 254 is an inactive mode.
        ripples[i].state = 254;
        break;

      default:                                            // Middle of the ripples.
        SEGMENT.setPixelColor((ripples[i].pos + ripples[i].state + SEGLEN) % SEGLEN, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(ripples[i].color, false, PALETTE_SOLID_WRAP, 0), uint8_t(2*255/ripples[i].state)));
        SEGMENT.setPixelColor((ripples[i].pos - ripples[i].state + SEGLEN) % SEGLEN, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(ripples[i].color, false, PALETTE_SOLID_WRAP, 0), uint8_t(2*255/ripples[i].state)));
        ripples[i].state++;                               // Next step.
        break;
    } // switch step
  } // for i

  return FRAMETIME;
} // mode_ripplepeak()
static const char _data_FX_MODE_RIPPLEPEAK[] PROGMEM = "Ripple Peak@Fade rate,Max # of ripples,Select bin,Volume (min);!,!;!;1v;c2=0,m12=0,si=0"; // Pixel, Beatsin


#ifndef WLED_DISABLE_2D
/////////////////////////
//    * 2D Swirl       //
/////////////////////////
// By: Mark Kriegsman https://gist.github.com/kriegsman/5adca44e14ad025e6d3b , modified by Andrew Tuline
uint16_t mode_2DSwirl(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  const uint8_t borderWidth = 2;

  SEGMENT.blur(SEGMENT.custom1);

  int  i = beatsin8_t( 27*SEGMENT.speed/255, borderWidth, cols - borderWidth);
  int  j = beatsin8_t( 41*SEGMENT.speed/255, borderWidth, rows - borderWidth);
  int ni = (cols - 1) - i;
  int nj = (cols - 1) - j;

  um_data_t *um_data = getAudioData();
  float volumeSmth  = *(float*)   um_data->u_data[0]; //ewowi: use instead of sampleAvg???
  int   volumeRaw   = *(int16_t*) um_data->u_data[1];

  SEGMENT.addPixelColorXY( i, j, ColorFromPalette(SEGPALETTE, (strip.now / 11 + volumeSmth*4), volumeRaw * SEGMENT.intensity / 64, LINEARBLEND)); //CHSV( ms / 11, 200, 255);
  SEGMENT.addPixelColorXY( j, i, ColorFromPalette(SEGPALETTE, (strip.now / 13 + volumeSmth*4), volumeRaw * SEGMENT.intensity / 64, LINEARBLEND)); //CHSV( ms / 13, 200, 255);
  SEGMENT.addPixelColorXY(ni,nj, ColorFromPalette(SEGPALETTE, (strip.now / 17 + volumeSmth*4), volumeRaw * SEGMENT.intensity / 64, LINEARBLEND)); //CHSV( ms / 17, 200, 255);
  SEGMENT.addPixelColorXY(nj,ni, ColorFromPalette(SEGPALETTE, (strip.now / 29 + volumeSmth*4), volumeRaw * SEGMENT.intensity / 64, LINEARBLEND)); //CHSV( ms / 29, 200, 255);
  SEGMENT.addPixelColorXY( i,nj, ColorFromPalette(SEGPALETTE, (strip.now / 37 + volumeSmth*4), volumeRaw * SEGMENT.intensity / 64, LINEARBLEND)); //CHSV( ms / 37, 200, 255);
  SEGMENT.addPixelColorXY(ni, j, ColorFromPalette(SEGPALETTE, (strip.now / 41 + volumeSmth*4), volumeRaw * SEGMENT.intensity / 64, LINEARBLEND)); //CHSV( ms / 41, 200, 255);

  return FRAMETIME;
} // mode_2DSwirl()
static const char _data_FX_MODE_2DSWIRL[] PROGMEM = "Swirl@!,Sensitivity,Blur;,Bg Swirl;!;2v;ix=64,si=0"; // Beatsin // TODO: color 1 unused?


/////////////////////////
//    * 2D Waverly     //
/////////////////////////
// By: Stepko, https://editor.soulmatelights.com/gallery/652-wave , modified by Andrew Tuline
uint16_t mode_2DWaverly(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  um_data_t *um_data = getAudioData();
  float   volumeSmth  = *(float*)   um_data->u_data[0];

  SEGMENT.fadeToBlackBy(SEGMENT.speed);

  long t = strip.now / 2;
  for (int i = 0; i < cols; i++) {
    unsigned thisVal = (1 + SEGMENT.intensity/64) * perlin8(i * 45 , t , t)/2;
    // use audio if available
    if (um_data) {
      thisVal /= 32; // reduce intensity of perlin8()
      thisVal *= volumeSmth;
    }
    int thisMax = map(thisVal, 0, 512, 0, rows);

    for (int j = 0; j < thisMax; j++) {
      SEGMENT.addPixelColorXY(i, j, ColorFromPalette(SEGPALETTE, map(j, 0, thisMax, 250, 0), 255, LINEARBLEND));
      SEGMENT.addPixelColorXY((cols - 1) - i, (rows - 1) - j, ColorFromPalette(SEGPALETTE, map(j, 0, thisMax, 250, 0), 255, LINEARBLEND));
    }
  }
  if (SEGMENT.check3) SEGMENT.blur(16, cols*rows < 100);

  return FRAMETIME;
} // mode_2DWaverly()
static const char _data_FX_MODE_2DWAVERLY[] PROGMEM = "Waverly@Amplification,Sensitivity,,,,,Blur;;!;2v;ix=64,si=0"; // Beatsin

#endif // WLED_DISABLE_2D

// Gravity struct requited for GRAV* effects
typedef struct Gravity {
  int    topLED;
  int    gravityCounter;
} gravity;

///////////////////////
//   * GRAVCENTER    //
///////////////////////
// Gravcenter effects By Andrew Tuline.
// Gravcenter base function for Gravcenter (0), Gravcentric (1), Gravimeter (2), Gravfreq (3) (merged by @dedehai)

uint16_t mode_gravcenter_base(unsigned mode) {
  if (SEGLEN == 1) return mode_static();

  const unsigned dataSize = sizeof(gravity);
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  Gravity* gravcen = reinterpret_cast<Gravity*>(SEGENV.data);

  um_data_t *um_data = getAudioData();
  float   volumeSmth  = *(float*)  um_data->u_data[0];

  if(mode == 1) SEGMENT.fade_out(253);  // //Gravcentric
  else if(mode == 2) SEGMENT.fade_out(249);  // Gravimeter
  else if(mode == 3) SEGMENT.fade_out(250);  // Gravfreq
  else SEGMENT.fade_out(251);  // Gravcenter

  float mySampleAvg;
  int tempsamp;
  float segmentSampleAvg = volumeSmth * (float)SEGMENT.intensity / 255.0f;

  if(mode == 2) { //Gravimeter
    segmentSampleAvg *= 0.25; // divide by 4, to compensate for later "sensitivity" upscaling
    mySampleAvg = mapf(segmentSampleAvg*2.0, 0, 64, 0, (SEGLEN-1)); // map to pixels availeable in current segment
    tempsamp = constrain(mySampleAvg,0,SEGLEN-1);       // Keep the sample from overflowing.
  }
  else { // Gravcenter or Gravcentric or Gravfreq
    segmentSampleAvg *= 0.125f; // divide by 8, to compensate for later "sensitivity" upscaling
    mySampleAvg = mapf(segmentSampleAvg*2.0, 0.0f, 32.0f, 0.0f, (float)SEGLEN/2.0f); // map to pixels availeable in current segment
    tempsamp = constrain(mySampleAvg, 0, SEGLEN/2);     // Keep the sample from overflowing.
  }

  uint8_t gravity = 8 - SEGMENT.speed/32;
  int offset = 1;
  if(mode == 2) offset = 0;  // Gravimeter
  if (tempsamp >= gravcen->topLED) gravcen->topLED = tempsamp-offset;
  else if (gravcen->gravityCounter % gravity == 0) gravcen->topLED--;
  
  if(mode == 1) {  //Gravcentric
    for (int i=0; i<tempsamp; i++) {
      uint8_t index = segmentSampleAvg*24+strip.now/200;
      SEGMENT.setPixelColor(i+SEGLEN/2, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
      SEGMENT.setPixelColor(SEGLEN/2-1-i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
    }
    if (gravcen->topLED >= 0) {
      SEGMENT.setPixelColor(gravcen->topLED+SEGLEN/2, CRGB::Gray);
      SEGMENT.setPixelColor(SEGLEN/2-1-gravcen->topLED, CRGB::Gray);
    }
  }
  else if(mode == 2) { //Gravimeter
    for (int i=0; i<tempsamp; i++) {
      uint8_t index = perlin8(i*segmentSampleAvg+strip.now, 5000+i*segmentSampleAvg);
      SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0), uint8_t(segmentSampleAvg*8)));
    }
    if (gravcen->topLED > 0) {
      SEGMENT.setPixelColor(gravcen->topLED, SEGMENT.color_from_palette(strip.now, false, PALETTE_SOLID_WRAP, 0));
    }
  }
  else if(mode == 3) { //Gravfreq
    for (int i=0; i<tempsamp; i++) {
      float   FFT_MajorPeak = *(float*)um_data->u_data[4]; // used in mode 3: Gravfreq
      if (FFT_MajorPeak < 1) FFT_MajorPeak = 1;
      uint8_t index = (log10f(FFT_MajorPeak) - (MAX_FREQ_LOG10 - 1.78f)) * 255;
      SEGMENT.setPixelColor(i+SEGLEN/2, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
      SEGMENT.setPixelColor(SEGLEN/2-i-1, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
    }
    if (gravcen->topLED >= 0) {
      SEGMENT.setPixelColor(gravcen->topLED+SEGLEN/2, CRGB::Gray);
      SEGMENT.setPixelColor(SEGLEN/2-1-gravcen->topLED, CRGB::Gray);
    }
  }
  else { //Gravcenter
    for (int i=0; i<tempsamp; i++) {
      uint8_t index = perlin8(i*segmentSampleAvg+strip.now, 5000+i*segmentSampleAvg);
      SEGMENT.setPixelColor(i+SEGLEN/2, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0), uint8_t(segmentSampleAvg*8)));
      SEGMENT.setPixelColor(SEGLEN/2-i-1, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0), uint8_t(segmentSampleAvg*8)));
    }
    if (gravcen->topLED >= 0) {
      SEGMENT.setPixelColor(gravcen->topLED+SEGLEN/2, SEGMENT.color_from_palette(strip.now, false, PALETTE_SOLID_WRAP, 0));
      SEGMENT.setPixelColor(SEGLEN/2-1-gravcen->topLED, SEGMENT.color_from_palette(strip.now, false, PALETTE_SOLID_WRAP, 0));
    }
  } 
  gravcen->gravityCounter = (gravcen->gravityCounter + 1) % gravity;

  return FRAMETIME;
}

uint16_t mode_gravcenter(void) {                // Gravcenter. By Andrew Tuline.
  return mode_gravcenter_base(0);
}
static const char _data_FX_MODE_GRAVCENTER[] PROGMEM = "Gravcenter@Rate of fall,Sensitivity;!,!;!;1v;ix=128,m12=2,si=0"; // Circle, Beatsin

///////////////////////
//   * GRAVCENTRIC   //
///////////////////////
uint16_t mode_gravcentric(void) {               // Gravcentric. By Andrew Tuline.
  return mode_gravcenter_base(1);
}
static const char _data_FX_MODE_GRAVCENTRIC[] PROGMEM = "Gravcentric@Rate of fall,Sensitivity;!,!;!;1v;ix=128,m12=3,si=0"; // Corner, Beatsin


///////////////////////
//   * GRAVIMETER    //
///////////////////////
uint16_t mode_gravimeter(void) {                // Gravmeter. By Andrew Tuline.
 return mode_gravcenter_base(2);
}
static const char _data_FX_MODE_GRAVIMETER[] PROGMEM = "Gravimeter@Rate of fall,Sensitivity;!,!;!;1v;ix=128,m12=2,si=0"; // Circle, Beatsin


///////////////////////
//    ** Gravfreq    //
///////////////////////
uint16_t mode_gravfreq(void) {                  // Gravfreq. By Andrew Tuline.
  return mode_gravcenter_base(3);
}
static const char _data_FX_MODE_GRAVFREQ[] PROGMEM = "Gravfreq@Rate of fall,Sensitivity;!,!;!;1f;ix=128,m12=0,si=0"; // Pixels, Beatsin


//////////////////////
//   * JUGGLES      //
//////////////////////
uint16_t mode_juggles(void) {                   // Juggles. By Andrew Tuline.
  um_data_t *um_data = getAudioData();
  float   volumeSmth   = *(float*)  um_data->u_data[0];

  SEGMENT.fade_out(224); // 6.25%
  uint8_t my_sampleAgc = fmax(fmin(volumeSmth, 255.0), 0);

  for (size_t i=0; i<SEGMENT.intensity/32+1U; i++) {
    // if SEGLEN equals 1, we will always set color to the first and only pixel, but the effect is still good looking
    SEGMENT.setPixelColor(beatsin16_t(SEGMENT.speed/4+i*2,0,SEGLEN-1), color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(strip.now/4+i*2, false, PALETTE_SOLID_WRAP, 0), my_sampleAgc));
  }

  return FRAMETIME;
} // mode_juggles()
static const char _data_FX_MODE_JUGGLES[] PROGMEM = "Juggles@!,# of balls;!,!;!;01v;m12=0,si=0"; // Pixels, Beatsin


//////////////////////
//   * MATRIPIX     //
//////////////////////
uint16_t mode_matripix(void) {                  // Matripix. By Andrew Tuline.
  // effect can work on single pixels, we just lose the shifting effect
  unsigned dataSize = sizeof(uint32_t) * SEGLEN;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  uint32_t* pixels = reinterpret_cast<uint32_t*>(SEGENV.data);

  um_data_t *um_data = getAudioData();
  int volumeRaw    = *(int16_t*)um_data->u_data[1];

  if (SEGENV.call == 0) {
    for (unsigned i = 0; i < SEGLEN; i++) pixels[i] = BLACK;   // may not be needed as resetIfRequired() clears buffer
  }

  uint8_t secondHand = micros()/(256-SEGMENT.speed)/500 % 16;
  if(SEGENV.aux0 != secondHand) {
    SEGENV.aux0 = secondHand;

    int pixBri = volumeRaw * SEGMENT.intensity / 64;
    unsigned k = SEGLEN-1;
    // loop will not execute if SEGLEN equals 1
    for (unsigned i = 0; i < k; i++) {
      pixels[i] = pixels[i+1]; // shift left
      SEGMENT.setPixelColor(i, pixels[i]);
    }
    pixels[k] = color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(strip.now, false, PALETTE_SOLID_WRAP, 0), pixBri);
    SEGMENT.setPixelColor(k, pixels[k]);
  }

  return FRAMETIME;
} // mode_matripix()
static const char _data_FX_MODE_MATRIPIX[] PROGMEM = "Matripix@!,Brightness;!,!;!;1v;ix=64,m12=2,si=1"; //,rev=1,mi=1,rY=1,mY=1 Circle, WeWillRockYou, reverseX


//////////////////////
//   * MIDNOISE     //
//////////////////////
uint16_t mode_midnoise(void) {                  // Midnoise. By Andrew Tuline.
  if (SEGLEN <= 1) return mode_static();
// Changing xdist to SEGENV.aux0 and ydist to SEGENV.aux1.

  um_data_t *um_data = getAudioData();
  float   volumeSmth   = *(float*)  um_data->u_data[0];

  SEGMENT.fade_out(SEGMENT.speed);
  SEGMENT.fade_out(SEGMENT.speed);

  float tmpSound2 = volumeSmth * (float)SEGMENT.intensity / 256.0;  // Too sensitive.
  tmpSound2 *= (float)SEGMENT.intensity / 128.0;              // Reduce sensitivity/length.

  unsigned maxLen = mapf(tmpSound2, 0, 127, 0, SEGLEN/2);
  if (maxLen >SEGLEN/2) maxLen = SEGLEN/2;

  for (unsigned i=(SEGLEN/2-maxLen); i<(SEGLEN/2+maxLen); i++) {
    uint8_t index = perlin8(i*volumeSmth+SEGENV.aux0, SEGENV.aux1+i*volumeSmth);  // Get a value from the noise function. I'm using both x and y axis.
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
  }

  SEGENV.aux0=SEGENV.aux0+beatsin8_t(5,0,10);
  SEGENV.aux1=SEGENV.aux1+beatsin8_t(4,0,10);

  return FRAMETIME;
} // mode_midnoise()
static const char _data_FX_MODE_MIDNOISE[] PROGMEM = "Midnoise@Fade rate,Max. length;!,!;!;1v;ix=128,m12=1,si=0"; // Bar, Beatsin


//////////////////////
//   * NOISEFIRE    //
//////////////////////
// I am the god of hellfire. . . Volume (only) reactive fire routine. Oh, look how short this is.
uint16_t mode_noisefire(void) {                 // Noisefire. By Andrew Tuline.
  CRGBPalette16 myPal = CRGBPalette16(CHSV(0,255,2),    CHSV(0,255,4),    CHSV(0,255,8), CHSV(0, 255, 8),  // Fire palette definition. Lower value = darker.
                                      CHSV(0, 255, 16), CRGB::Red,        CRGB::Red,     CRGB::Red,
                                      CRGB::DarkOrange, CRGB::DarkOrange, CRGB::Orange,  CRGB::Orange,
                                      CRGB::Yellow,     CRGB::Orange,     CRGB::Yellow,  CRGB::Yellow);

  um_data_t *um_data = getAudioData();
  float   volumeSmth   = *(float*)  um_data->u_data[0];

  if (SEGENV.call == 0) SEGMENT.fill(BLACK);

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned index = perlin8(i*SEGMENT.speed/64,strip.now*SEGMENT.speed/64*SEGLEN/255);  // X location is constant, but we move along the Y at the rate of millis(). By Andrew Tuline.
    index = (255 - i*256/SEGLEN) * index/(256-SEGMENT.intensity);                       // Now we need to scale index so that it gets blacker as we get close to one of the ends.
                                                                                        // This is a simple y=mx+b equation that's been scaled. index/128 is another scaling.

    SEGMENT.setPixelColor(i, ColorFromPalette(myPal, index, volumeSmth*2, LINEARBLEND)); // Use my own palette.
  }

  return FRAMETIME;
} // mode_noisefire()
static const char _data_FX_MODE_NOISEFIRE[] PROGMEM = "Noisefire@!,!;;;01v;m12=2,si=0"; // Circle, Beatsin


///////////////////////
//   * Noisemeter    //
///////////////////////
uint16_t mode_noisemeter(void) {                // Noisemeter. By Andrew Tuline.

  um_data_t *um_data = getAudioData();
  float   volumeSmth   = *(float*)  um_data->u_data[0];
  int volumeRaw    = *(int16_t*)um_data->u_data[1];

  //uint8_t fadeRate = map(SEGMENT.speed,0,255,224,255);
  uint8_t fadeRate = map(SEGMENT.speed,0,255,200,254);
  SEGMENT.fade_out(fadeRate);

  float tmpSound2 = volumeRaw * 2.0 * (float)SEGMENT.intensity / 255.0;
  unsigned maxLen = mapf(tmpSound2, 0, 255, 0, SEGLEN); // map to pixels availeable in current segment              // Still a bit too sensitive.
  if (maxLen < 0) maxLen = 0;
  if (maxLen > SEGLEN) maxLen = SEGLEN;

  for (unsigned i=0; i<maxLen; i++) {                                    // The louder the sound, the wider the soundbar. By Andrew Tuline.
    uint8_t index = perlin8(i*volumeSmth+SEGENV.aux0, SEGENV.aux1+i*volumeSmth);  // Get a value from the noise function. I'm using both x and y axis.
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(index, false, PALETTE_SOLID_WRAP, 0));
  }

  SEGENV.aux0+=beatsin8_t(5,0,10);
  SEGENV.aux1+=beatsin8_t(4,0,10);

  return FRAMETIME;
} // mode_noisemeter()
static const char _data_FX_MODE_NOISEMETER[] PROGMEM = "Noisemeter@Fade rate,Width;!,!;!;1v;ix=128,m12=2,si=0"; // Circle, Beatsin


//////////////////////
//   * PIXELWAVE    //
//////////////////////
uint16_t mode_pixelwave(void) {                 // Pixelwave. By Andrew Tuline.
  if (SEGLEN <= 1) return mode_static();
  // even with 1D effect we have to take logic for 2D segments for allocation as fill_solid() fills whole segment

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  um_data_t *um_data = getAudioData();
  int volumeRaw    = *(int16_t*)um_data->u_data[1];

  uint8_t secondHand = micros()/(256-SEGMENT.speed)/500+1 % 16;
  if (SEGENV.aux0 != secondHand) {
    SEGENV.aux0 = secondHand;

    uint8_t pixBri = volumeRaw * SEGMENT.intensity / 64;

    SEGMENT.setPixelColor(SEGLEN/2, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(strip.now, false, PALETTE_SOLID_WRAP, 0), pixBri));
    for (unsigned i = SEGLEN - 1; i > SEGLEN/2; i--) SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i-1)); //move to the left
    for (unsigned i = 0; i < SEGLEN/2; i++)          SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i+1)); // move to the right
  }

  return FRAMETIME;
} // mode_pixelwave()
static const char _data_FX_MODE_PIXELWAVE[] PROGMEM = "Pixelwave@!,Sensitivity;!,!;!;1v;ix=64,m12=2,si=0"; // Circle, Beatsin


//////////////////////
//   * PLASMOID     //
//////////////////////
typedef struct Plasphase {
  int16_t    thisphase;
  int16_t    thatphase;
} plasphase;

uint16_t mode_plasmoid(void) {                  // Plasmoid. By Andrew Tuline.
  // even with 1D effect we have to take logic for 2D segments for allocation as fill_solid() fills whole segment
  if (!SEGENV.allocateData(sizeof(plasphase))) return mode_static(); //allocation failed
  Plasphase* plasmoip = reinterpret_cast<Plasphase*>(SEGENV.data);

  um_data_t *um_data = getAudioData();
  float   volumeSmth   = *(float*)  um_data->u_data[0];

  SEGMENT.fadeToBlackBy(32);

  plasmoip->thisphase += beatsin8_t(6,-4,4);                          // You can change direction and speed individually.
  plasmoip->thatphase += beatsin8_t(7,-4,4);                          // Two phase values to make a complex pattern. By Andrew Tuline.

  for (unsigned i = 0; i < SEGLEN; i++) {                          // For each of the LED's in the strand, set a brightness based on a wave as follows.
    // updated, similar to "plasma" effect - softhack007
    uint8_t thisbright = cubicwave8(((i*(1 + (3*SEGMENT.speed/32)))+plasmoip->thisphase) & 0xFF)/2;
    thisbright += cos8_t(((i*(97 +(5*SEGMENT.speed/32)))+plasmoip->thatphase) & 0xFF)/2; // Let's munge the brightness a bit and animate it all with the phases.

    uint8_t colorIndex=thisbright;
    if (volumeSmth * SEGMENT.intensity / 64 < thisbright) {thisbright = 0;}

    SEGMENT.addPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(colorIndex, false, PALETTE_SOLID_WRAP, 0), thisbright));
  }

  return FRAMETIME;
} // mode_plasmoid()
static const char _data_FX_MODE_PLASMOID[] PROGMEM = "Plasmoid@Phase,# of pixels;!,!;!;01v;sx=128,ix=128,m12=0,si=0"; // Pixels, Beatsin


//////////////////////
//   * PUDDLES      //
//////////////////////
// Puddles/Puddlepeak By Andrew Tuline. Merged by @dedehai
uint16_t mode_puddles_base(bool peakdetect) {
  if (SEGLEN <= 1) return mode_static();
  unsigned size = 0;
  uint8_t fadeVal = map(SEGMENT.speed, 0, 255, 224, 254);
  unsigned pos = hw_random16(SEGLEN);                          // Set a random starting position.
  SEGMENT.fade_out(fadeVal);

  um_data_t *um_data = getAudioData();
  int volumeRaw    = *(int16_t*)um_data->u_data[1];
  uint8_t samplePeak = *(uint8_t*)um_data->u_data[3];
  uint8_t *maxVol    =  (uint8_t*)um_data->u_data[6];
  uint8_t *binNum    =  (uint8_t*)um_data->u_data[7];
  float   volumeSmth   = *(float*)  um_data->u_data[0];

  if(peakdetect) {                                          // puddles peak
    *binNum = SEGMENT.custom1;                              // Select a bin.
    *maxVol = SEGMENT.custom2 / 2;                          // Our volume comparator.
    if (samplePeak == 1) {
      size = volumeSmth * SEGMENT.intensity /256 /4 + 1;    // Determine size of the flash based on the volume.
      if (pos+size>= SEGLEN) size = SEGLEN - pos;
    }
  }
  else {                                                    // puddles  
    if (volumeRaw > 1) {
      size = volumeRaw * SEGMENT.intensity /256 /8 + 1;     // Determine size of the flash based on the volume.
      if (pos+size >= SEGLEN) size = SEGLEN - pos;
    } 
  }
  
  for (unsigned i=0; i<size; i++) {                          // Flash the LED's.
    SEGMENT.setPixelColor(pos+i, SEGMENT.color_from_palette(strip.now, false, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
} 

uint16_t mode_puddlepeak(void) {                // Puddlepeak. By Andrew Tuline.
  return mode_puddles_base(true);
} 
static const char _data_FX_MODE_PUDDLEPEAK[] PROGMEM = "Puddlepeak@Fade rate,Puddle size,Select bin,Volume (min);!,!;!;1v;c2=0,m12=0,si=0"; // Pixels, Beatsin

uint16_t mode_puddles(void) {                   // Puddles. By Andrew Tuline.
  return mode_puddles_base(false);
} 
static const char _data_FX_MODE_PUDDLES[] PROGMEM = "Puddles@Fade rate,Puddle size;!,!;!;1v;m12=0,si=0"; // Pixels, Beatsin


//////////////////////
//     * PIXELS     //
//////////////////////
uint16_t mode_pixels(void) {                    // Pixels. By Andrew Tuline.
  if (SEGLEN <= 1) return mode_static();

  if (!SEGENV.allocateData(32*sizeof(uint8_t))) return mode_static(); //allocation failed
  uint8_t *myVals = reinterpret_cast<uint8_t*>(SEGENV.data); // Used to store a pile of samples because WLED frame rate and WLED sample rate are not synchronized. Frame rate is too low.

  um_data_t *um_data;
  if (!UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
    um_data = simulateSound(SEGMENT.soundSim);
  }
  float   volumeSmth   = *(float*)  um_data->u_data[0];

  myVals[strip.now%32] = volumeSmth;    // filling values semi randomly

  SEGMENT.fade_out(64+(SEGMENT.speed>>1));

  for (int i=0; i <SEGMENT.intensity/8; i++) {
    unsigned segLoc = hw_random16(SEGLEN);                    // 16 bit for larger strands of LED's.
    SEGMENT.setPixelColor(segLoc, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(myVals[i%32]+i*4, false, PALETTE_SOLID_WRAP, 0), uint8_t(volumeSmth)));
  }

  return FRAMETIME;
} // mode_pixels()
static const char _data_FX_MODE_PIXELS[] PROGMEM = "Pixels@Fade rate,# of pixels;!,!;!;1v;m12=0,si=0"; // Pixels, Beatsin

//////////////////////
//    ** Blurz      //
//////////////////////
uint16_t mode_blurz(void) {                    // Blurz. By Andrew Tuline.
  if (SEGLEN <= 1) return mode_static();
  // even with 1D effect we have to take logic for 2D segments for allocation as fill_solid() fills whole segment

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
    SEGENV.aux0 = 0;
  }

  int fadeoutDelay = (256 - SEGMENT.speed) / 32;
  if ((fadeoutDelay <= 1 ) || ((SEGENV.call % fadeoutDelay) == 0)) SEGMENT.fade_out(SEGMENT.speed);

  SEGENV.step += FRAMETIME;
  if (SEGENV.step > SPEED_FORMULA_L) {
    unsigned segLoc = hw_random16(SEGLEN);
    SEGMENT.setPixelColor(segLoc, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(2*fftResult[SEGENV.aux0%16]*240/max(1, (int)SEGLEN-1), false, PALETTE_SOLID_WRAP, 0), uint8_t(2*fftResult[SEGENV.aux0%16])));
    ++(SEGENV.aux0) %= 16; // make sure it doesn't cross 16

    SEGENV.step = 1;
    SEGMENT.blur(SEGMENT.intensity); // note: blur > 210 results in a alternating pattern, this could be fixed by mapping but some may like it (very old bug)
  }

  return FRAMETIME;
} // mode_blurz()
static const char _data_FX_MODE_BLURZ[] PROGMEM = "Blurz@Fade rate,Blur;!,Color mix;!;1f;m12=0,si=0"; // Pixels, Beatsin


/////////////////////////
//   ** DJLight        //
/////////////////////////
uint16_t mode_DJLight(void) {                   // Written by ??? Adapted by Will Tatam.
  // No need to prevent from executing on single led strips, only mid will be set (mid = 0)
  const int mid = SEGLEN / 2;

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  uint8_t secondHand = micros()/(256-SEGMENT.speed)/500+1 % 64;
  if (SEGENV.aux0 != secondHand) {                        // Triggered millis timing.
    SEGENV.aux0 = secondHand;

    CRGB color = CRGB(fftResult[15]/2, fftResult[5]/2, fftResult[0]/2); // 16-> 15 as 16 is out of bounds
    SEGMENT.setPixelColor(mid, color.fadeToBlackBy(map(fftResult[4], 0, 255, 255, 4)));     // TODO - Update

    // if SEGLEN equals 1 these loops won't execute
    for (int i = SEGLEN - 1; i > mid; i--)   SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i-1)); // move to the left
    for (int i = 0; i < mid; i++)            SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i+1)); // move to the right
  }

  return FRAMETIME;
} // mode_DJLight()
static const char _data_FX_MODE_DJLIGHT[] PROGMEM = "DJ Light@Speed;;;01f;m12=2,si=0"; // Circle, Beatsin


////////////////////
//   ** Freqmap   //
////////////////////
uint16_t mode_freqmap(void) {                   // Map FFT_MajorPeak to SEGLEN. Would be better if a higher framerate.
  if (SEGLEN <= 1) return mode_static();
  // Start frequency = 60 Hz and log10(60) = 1.78
  // End frequency = MAX_FREQUENCY in Hz and lo10(MAX_FREQUENCY) = MAX_FREQ_LOG10

  um_data_t *um_data = getAudioData();
  float FFT_MajorPeak = *(float*)um_data->u_data[4];
  float my_magnitude  = *(float*)um_data->u_data[5] / 4.0f;
  if (FFT_MajorPeak < 1) FFT_MajorPeak = 1;                                         // log10(0) is "forbidden" (throws exception)

  if (SEGENV.call == 0) SEGMENT.fill(BLACK);
  int fadeoutDelay = (256 - SEGMENT.speed) / 32;
  if ((fadeoutDelay <= 1 ) || ((SEGENV.call % fadeoutDelay) == 0)) SEGMENT.fade_out(SEGMENT.speed);

  int locn = (log10f((float)FFT_MajorPeak) - 1.78f) * (float)SEGLEN/(MAX_FREQ_LOG10 - 1.78f);  // log10 frequency range is from 1.78 to 3.71. Let's scale to SEGLEN.
  if (locn < 1) locn = 0; // avoid underflow

  if (locn >= (int)SEGLEN) locn = SEGLEN-1;
  unsigned pixCol = (log10f(FFT_MajorPeak) - 1.78f) * 255.0f/(MAX_FREQ_LOG10 - 1.78f);   // Scale log10 of frequency values to the 255 colour index.
  if (FFT_MajorPeak < 61.0f) pixCol = 0;                                                 // handle underflow

  uint8_t bright = (uint8_t)my_magnitude;

  SEGMENT.setPixelColor(locn, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(SEGMENT.intensity+pixCol, false, PALETTE_SOLID_WRAP, 0), bright));

  return FRAMETIME;
} // mode_freqmap()
static const char _data_FX_MODE_FREQMAP[] PROGMEM = "Freqmap@Fade rate,Starting color;!,!;!;1f;m12=0,si=0"; // Pixels, Beatsin


///////////////////////
//   ** Freqmatrix   //
///////////////////////
uint16_t mode_freqmatrix(void) {                // Freqmatrix. By Andreas Pleschung.
  // No need to prevent from executing on single led strips, we simply change pixel 0 each time and avoid the shift
  um_data_t *um_data = getAudioData();
  float FFT_MajorPeak = *(float*)um_data->u_data[4];
  float volumeSmth    = *(float*)um_data->u_data[0];

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  uint8_t secondHand = micros()/(256-SEGMENT.speed)/500 % 16;
  if(SEGENV.aux0 != secondHand) {
    SEGENV.aux0 = secondHand;

    uint8_t sensitivity = map(SEGMENT.custom3, 0, 31, 1, 10); // reduced resolution slider
    int pixVal = (volumeSmth * SEGMENT.intensity * sensitivity) / 256.0f;
    if (pixVal > 255) pixVal = 255;

    float intensity = map(pixVal, 0, 255, 0, 100) / 100.0f;  // make a brightness from the last avg

    CRGB color = CRGB::Black;

    if (FFT_MajorPeak > MAX_FREQUENCY) FFT_MajorPeak = 1;
    // MajorPeak holds the freq. value which is most abundant in the last sample.
    // With our sampling rate of 10240Hz we have a usable freq range from roughly 80Hz to 10240/2 Hz
    // we will treat everything with less than 65Hz as 0

    if (FFT_MajorPeak < 80) {
      color = CRGB::Black;
    } else {
      int upperLimit = 80 + 42 * SEGMENT.custom2;
      int lowerLimit = 80 + 3 * SEGMENT.custom1;
      uint8_t i =  lowerLimit!=upperLimit ? map(FFT_MajorPeak, lowerLimit, upperLimit, 0, 255) : FFT_MajorPeak;  // may under/overflow - so we enforce uint8_t
      unsigned b = 255 * intensity;
      if (b > 255) b = 255;
      color = CHSV(i, 240, (uint8_t)b); // implicit conversion to RGB supplied by FastLED
    }

    // shift the pixels one pixel up
    SEGMENT.setPixelColor(0, color);
    // if SEGLEN equals 1 this loop won't execute
    for (int i = SEGLEN - 1; i > 0; i--) SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i-1)); //move to the left
  }

  return FRAMETIME;
} // mode_freqmatrix()
static const char _data_FX_MODE_FREQMATRIX[] PROGMEM = "Freqmatrix@Speed,Sound effect,Low bin,High bin,Sensitivity;;;01f;m12=3,si=0"; // Corner, Beatsin


//////////////////////
//   ** Freqpixels  //
//////////////////////
// Start frequency = 60 Hz and log10(60) = 1.78
// End frequency = 5120 Hz and lo10(5120) = 3.71
//  SEGMENT.speed select faderate
//  SEGMENT.intensity select colour index
uint16_t mode_freqpixels(void) {                // Freqpixel. By Andrew Tuline.
  um_data_t *um_data = getAudioData();
  float FFT_MajorPeak = *(float*)um_data->u_data[4];
  float my_magnitude  = *(float*)um_data->u_data[5] / 16.0f;
  if (FFT_MajorPeak < 1) FFT_MajorPeak = 1.0f; // log10(0) is "forbidden" (throws exception)

  // this code translates to speed * (2 - speed/255) which is a) speed*2 or b) speed (when speed is 255)
  // and since fade_out() can only take 0-255 it will behave incorrectly when speed > 127
  //uint16_t fadeRate = 2*SEGMENT.speed - SEGMENT.speed*SEGMENT.speed/255;    // Get to 255 as quick as you can.
  unsigned fadeRate = SEGMENT.speed*SEGMENT.speed; // Get to 255 as quick as you can.
  fadeRate = map(fadeRate, 0, 65535, 1, 255);

  int fadeoutDelay = (256 - SEGMENT.speed) / 64;
  if ((fadeoutDelay <= 1 ) || ((SEGENV.call % fadeoutDelay) == 0)) SEGMENT.fade_out(fadeRate);

  uint8_t pixCol = (log10f(FFT_MajorPeak) - 1.78f) * 255.0f/(MAX_FREQ_LOG10 - 1.78f);  // Scale log10 of frequency values to the 255 colour index.
  if (FFT_MajorPeak < 61.0f) pixCol = 0;                                               // handle underflow
  for (int i=0; i < SEGMENT.intensity/32+1; i++) {
    unsigned locn = hw_random16(0,SEGLEN);
    SEGMENT.setPixelColor(locn, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(SEGMENT.intensity+pixCol, false, PALETTE_SOLID_WRAP, 0), (uint8_t)my_magnitude));
  }

  return FRAMETIME;
} // mode_freqpixels()
static const char _data_FX_MODE_FREQPIXELS[] PROGMEM = "Freqpixels@Fade rate,Starting color and # of pixels;!,!,;!;1f;m12=0,si=0"; // Pixels, Beatsin


//////////////////////
//   ** Freqwave    //
//////////////////////
// Assign a color to the central (starting pixels) based on the predominant frequencies and the volume. The color is being determined by mapping the MajorPeak from the FFT
// and then mapping this to the HSV color circle. Currently we are sampling at 10240 Hz, so the highest frequency we can look at is 5120Hz.
//
// SEGMENT.custom1: the lower cut off point for the FFT. (many, most time the lowest values have very little information since they are FFT conversion artifacts. Suggested value is close to but above 0
// SEGMENT.custom2: The high cut off point. This depends on your sound profile. Most music looks good when this slider is between 50% and 100%.
// SEGMENT.custom3: "preamp" for the audio signal for audio10.
//
// I suggest that for this effect you turn the brightness to 95%-100% but again it depends on your soundprofile you find yourself in.
// Instead of using colorpalettes, This effect works on the HSV color circle with red being the lowest frequency
//
// As a compromise between speed and accuracy we are currently sampling with 10240Hz, from which we can then determine with a 512bin FFT our max frequency is 5120Hz.
// Depending on the music stream you have you might find it useful to change the frequency mapping.
uint16_t mode_freqwave(void) {                  // Freqwave. By Andreas Pleschung.
  // As before, this effect can also work on single pixels, we just lose the shifting effect
  um_data_t *um_data = getAudioData();
  float FFT_MajorPeak = *(float*)um_data->u_data[4];
  float volumeSmth    = *(float*)um_data->u_data[0];

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  uint8_t secondHand = micros()/(256-SEGMENT.speed)/500 % 16;
  if(SEGENV.aux0 != secondHand) {
    SEGENV.aux0 = secondHand;

    float sensitivity = mapf(SEGMENT.custom3, 1, 31, 1, 10); // reduced resolution slider
    float pixVal = min(255.0f, volumeSmth * (float)SEGMENT.intensity / 256.0f * sensitivity);
    float intensity = mapf(pixVal, 0.0f, 255.0f, 0.0f, 100.0f) / 100.0f;  // make a brightness from the last avg

    CRGB color = 0;

    if (FFT_MajorPeak > MAX_FREQUENCY) FFT_MajorPeak = 1.0f;
    // MajorPeak holds the freq. value which is most abundant in the last sample.
    // With our sampling rate of 10240Hz we have a usable freq range from roughly 80Hz to 10240/2 Hz
    // we will treat everything with less than 65Hz as 0

    if (FFT_MajorPeak < 80) {
      color = CRGB::Black;
    } else {
      int upperLimit = 80 + 42 * SEGMENT.custom2;
      int lowerLimit = 80 + 3 * SEGMENT.custom1;
      uint8_t i =  lowerLimit!=upperLimit ? map(FFT_MajorPeak, lowerLimit, upperLimit, 0, 255) : FFT_MajorPeak; // may under/overflow - so we enforce uint8_t
      unsigned b = min(255.0f, 255.0f * intensity);
      color = CHSV(i, 240, (uint8_t)b); // implicit conversion to RGB supplied by FastLED
    }

    SEGMENT.setPixelColor(SEGLEN/2, color);

    // shift the pixels one pixel outwards
    // if SEGLEN equals 1 these loops won't execute
    for (unsigned i = SEGLEN - 1; i > SEGLEN/2; i--) SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i-1)); //move to the left
    for (unsigned i = 0; i < SEGLEN/2; i++)          SEGMENT.setPixelColor(i, SEGMENT.getPixelColor(i+1)); // move to the right
  }

  return FRAMETIME;
} // mode_freqwave()
static const char _data_FX_MODE_FREQWAVE[] PROGMEM = "Freqwave@Speed,Sound effect,Low bin,High bin,Pre-amp;;;01f;m12=2,si=0"; // Circle, Beatsin


//////////////////////
//   ** Noisemove   //
//////////////////////
uint16_t mode_noisemove(void) {                 // Noisemove.    By: Andrew Tuline
  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];

  int fadeoutDelay = (256 - SEGMENT.speed) / 96;
  if ((fadeoutDelay <= 1 ) || ((SEGENV.call % fadeoutDelay) == 0)) SEGMENT.fadeToBlackBy(4+ SEGMENT.speed/4);

  uint8_t numBins = map(SEGMENT.intensity,0,255,0,16);    // Map slider to fftResult bins.
  for (int i=0; i<numBins; i++) {                         // How many active bins are we using.
    unsigned locn = perlin16(strip.now*SEGMENT.speed+i*50000, strip.now*SEGMENT.speed);   // Get a new pixel location from moving noise.
    // if SEGLEN equals 1 locn will be always 0, hence we set the first pixel only
    locn = map(locn, 7500, 58000, 0, SEGLEN-1);           // Map that to the length of the strand, and ensure we don't go over.
    SEGMENT.setPixelColor(locn, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i*64, false, PALETTE_SOLID_WRAP, 0), uint8_t(fftResult[i % 16]*4)));
  }

  return FRAMETIME;
} // mode_noisemove()
static const char _data_FX_MODE_NOISEMOVE[] PROGMEM = "Noisemove@Move speed,Fade rate;!,!;!;01f;m12=0,si=0"; // Pixels, Beatsin


//////////////////////
//   ** Rocktaves   //
//////////////////////
uint16_t mode_rocktaves(void) {                 // Rocktaves. Same note from each octave is same colour.    By: Andrew Tuline
  um_data_t *um_data = getAudioData();
  float   FFT_MajorPeak = *(float*)  um_data->u_data[4];
  float   my_magnitude  = *(float*)   um_data->u_data[5] / 16.0f;

  SEGMENT.fadeToBlackBy(16);                              // Just in case something doesn't get faded.

  float frTemp = FFT_MajorPeak;
  uint8_t octCount = 0;                                   // Octave counter.
  uint8_t volTemp = 0;

  volTemp = 32.0f + my_magnitude * 1.5f;                  // brightness = volume (overflows are handled in next lines)
  if (my_magnitude < 48) volTemp = 0;                     // We need to squelch out the background noise.
  if (my_magnitude > 144) volTemp = 255;                  // everything above this is full brightness

  while ( frTemp > 249 ) {
    octCount++;                                           // This should go up to 5.
    frTemp = frTemp/2;
  }

  frTemp -= 132.0f;                                       // This should give us a base musical note of C3
  frTemp  = fabsf(frTemp * 2.1f);                         // Fudge factors to compress octave range starting at 0 and going to 255;

  unsigned i = map(beatsin8_t(8+octCount*4, 0, 255, 0, octCount*8), 0, 255, 0, SEGLEN-1);
  i = constrain(i, 0U, SEGLEN-1U);
  SEGMENT.addPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette((uint8_t)frTemp, false, PALETTE_SOLID_WRAP, 0), volTemp));

  return FRAMETIME;
} // mode_rocktaves()
static const char _data_FX_MODE_ROCKTAVES[] PROGMEM = "Rocktaves@;!,!;!;01f;m12=1,si=0"; // Bar, Beatsin


///////////////////////
//   ** Waterfall    //
///////////////////////
// Combines peak detection with FFT_MajorPeak and FFT_Magnitude.
uint16_t mode_waterfall(void) {                   // Waterfall. By: Andrew Tuline
  // effect can work on single pixels, we just lose the shifting effect
  unsigned dataSize = sizeof(uint32_t) * SEGLEN;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed
  uint32_t* pixels = reinterpret_cast<uint32_t*>(SEGENV.data);

  um_data_t *um_data    = getAudioData();
  uint8_t samplePeak    = *(uint8_t*)um_data->u_data[3];
  float   FFT_MajorPeak = *(float*)  um_data->u_data[4];
  uint8_t *maxVol       =  (uint8_t*)um_data->u_data[6];
  uint8_t *binNum       =  (uint8_t*)um_data->u_data[7];
  float   my_magnitude  = *(float*)   um_data->u_data[5] / 8.0f;

  if (FFT_MajorPeak < 1) FFT_MajorPeak = 1;                                         // log10(0) is "forbidden" (throws exception)

  if (SEGENV.call == 0) {
    for (unsigned i = 0; i < SEGLEN; i++) pixels[i] = BLACK;   // may not be needed as resetIfRequired() clears buffer
    SEGENV.aux0 = 255;
    SEGMENT.custom1 = *binNum;
    SEGMENT.custom2 = *maxVol * 2;
  }

  *binNum = SEGMENT.custom1;                              // Select a bin.
  *maxVol = SEGMENT.custom2 / 2;                          // Our volume comparator.

  uint8_t secondHand = micros() / (256-SEGMENT.speed)/500 + 1 % 16;
  if (SEGENV.aux0 != secondHand) {                        // Triggered millis timing.
    SEGENV.aux0 = secondHand;

    //uint8_t pixCol = (log10f((float)FFT_MajorPeak) - 2.26f) * 177;  // 10Khz sampling - log10 frequency range is from 2.26 (182hz) to 3.7 (5012hz). Let's scale accordingly.
    uint8_t pixCol = (log10f(FFT_MajorPeak) - 2.26f) * 150;           // 22Khz sampling - log10 frequency range is from 2.26 (182hz) to 3.967 (9260hz). Let's scale accordingly.
    if (FFT_MajorPeak < 182.0f) pixCol = 0;                           // handle underflow

    unsigned k = SEGLEN-1;
    if (samplePeak) {
      pixels[k] = (uint32_t)CRGB(CHSV(92,92,92));
    } else {
      pixels[k] = color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(pixCol+SEGMENT.intensity, false, PALETTE_SOLID_WRAP, 0), (uint8_t)my_magnitude);
    }
    SEGMENT.setPixelColor(k, pixels[k]);
    // loop will not execute if SEGLEN equals 1
    for (unsigned i = 0; i < k; i++) {
      pixels[i] = pixels[i+1]; // shift left
      SEGMENT.setPixelColor(i, pixels[i]);
    }
  }

  return FRAMETIME;
} // mode_waterfall()
static const char _data_FX_MODE_WATERFALL[] PROGMEM = "Waterfall@!,Adjust color,Select bin,Volume (min);!,!;!;01f;c2=0,m12=2,si=0"; // Circles, Beatsin


#ifndef WLED_DISABLE_2D
/////////////////////////
//     ** 2D GEQ       //
/////////////////////////
uint16_t mode_2DGEQ(void) { // By Will Tatam. Code reduction by Ewoud Wijma.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int NUM_BANDS = map(SEGMENT.custom1, 0, 255, 1, 16);
  const int cols = SEG_W;
  const int rows = SEG_H;

  if (!SEGENV.allocateData(cols*sizeof(uint16_t))) return mode_static(); //allocation failed
  uint16_t *previousBarHeight = reinterpret_cast<uint16_t*>(SEGENV.data); //array of previous bar heights per frequency band

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];

  if (SEGENV.call == 0) for (int i=0; i<cols; i++) previousBarHeight[i] = 0;

  bool rippleTime = false;
  if (strip.now - SEGENV.step >= (256U - SEGMENT.intensity)) {
    SEGENV.step = strip.now;
    rippleTime = true;
  }

  int fadeoutDelay = (256 - SEGMENT.speed) / 64;
  if ((fadeoutDelay <= 1 ) || ((SEGENV.call % fadeoutDelay) == 0)) SEGMENT.fadeToBlackBy(SEGMENT.speed);

  for (int x=0; x < cols; x++) {
    uint8_t  band       = map(x, 0, cols, 0, NUM_BANDS);
    if (NUM_BANDS < 16) band = map(band, 0, NUM_BANDS - 1, 0, 15); // always use full range. comment out this line to get the previous behaviour.
    band = constrain(band, 0, 15);
    unsigned colorIndex = band * 17;
    int barHeight  = map(fftResult[band], 0, 255, 0, rows); // do not subtract -1 from rows here
    if (barHeight > previousBarHeight[x]) previousBarHeight[x] = barHeight; //drive the peak up

    uint32_t ledColor = BLACK;
    for (int y=0; y < barHeight; y++) {
      if (SEGMENT.check1) //color_vertical / color bars toggle
        colorIndex = map(y, 0, rows-1, 0, 255);

      ledColor = SEGMENT.color_from_palette(colorIndex, false, PALETTE_SOLID_WRAP, 0);
      SEGMENT.setPixelColorXY(x, rows-1 - y, ledColor);
    }
    if (previousBarHeight[x] > 0)
      SEGMENT.setPixelColorXY(x, rows - previousBarHeight[x], (SEGCOLOR(2) != BLACK) ? SEGCOLOR(2) : ledColor);

    if (rippleTime && previousBarHeight[x]>0) previousBarHeight[x]--;    //delay/ripple effect
  }

  return FRAMETIME;
} // mode_2DGEQ()
static const char _data_FX_MODE_2DGEQ[] PROGMEM = "GEQ@Fade speed,Ripple decay,# of bands,,,Color bars;!,,Peaks;!;2f;c1=255,c2=64,pal=11,si=0"; // Beatsin


/////////////////////////
//  ** 2D Funky plank  //
/////////////////////////
uint16_t mode_2DFunkyPlank(void) {              // Written by ??? Adapted by Will Tatam.
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  int NUMB_BANDS = map(SEGMENT.custom1, 0, 255, 1, 16);
  int barWidth = (cols / NUMB_BANDS);
  int bandInc = 1;
  if (barWidth == 0) {
    // Matrix narrower than fft bands
    barWidth = 1;
    bandInc = (NUMB_BANDS / cols);
  }

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
  }

  uint8_t secondHand = micros()/(256-SEGMENT.speed)/500+1 % 64;
  if (SEGENV.aux0 != secondHand) {                        // Triggered millis timing.
    SEGENV.aux0 = secondHand;

    // display values of
    int b = 0;
    for (int band = 0; band < NUMB_BANDS; band += bandInc, b++) {
      int hue = fftResult[band % 16];
      int v = map(fftResult[band % 16], 0, 255, 10, 255);
      for (int w = 0; w < barWidth; w++) {
         int xpos = (barWidth * b) + w;
         SEGMENT.setPixelColorXY(xpos, 0, CHSV(hue, 255, v));
      }
    }

    // Update the display:
    for (int i = (rows - 1); i > 0; i--) {
      for (int j = (cols - 1); j >= 0; j--) {
        SEGMENT.setPixelColorXY(j, i, SEGMENT.getPixelColorXY(j, i-1));
      }
    }
  }

  return FRAMETIME;
} // mode_2DFunkyPlank
static const char _data_FX_MODE_2DFUNKYPLANK[] PROGMEM = "Funky Plank@Scroll speed,,# of bands;;;2f;si=0"; // Beatsin


/////////////////////////
//     2D Akemi        //
/////////////////////////
static uint8_t akemi[] PROGMEM = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,2,2,3,3,3,3,3,3,2,2,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,2,3,3,0,0,0,0,0,0,3,3,2,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,2,3,0,0,0,6,5,5,4,0,0,0,3,2,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,2,3,0,0,6,6,5,5,5,5,4,4,0,0,3,2,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,2,3,0,6,5,5,5,5,5,5,5,5,4,0,3,2,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,2,3,0,6,5,5,5,5,5,5,5,5,5,5,4,0,3,2,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,3,2,0,6,5,5,5,5,5,5,5,5,5,5,4,0,2,3,0,0,0,0,0,0,0,
  0,0,0,0,0,0,3,2,3,6,5,5,7,7,5,5,5,5,7,7,5,5,4,3,2,3,0,0,0,0,0,0,
  0,0,0,0,0,2,3,1,3,6,5,1,7,7,7,5,5,1,7,7,7,5,4,3,1,3,2,0,0,0,0,0,
  0,0,0,0,0,8,3,1,3,6,5,1,7,7,7,5,5,1,7,7,7,5,4,3,1,3,8,0,0,0,0,0,
  0,0,0,0,0,8,3,1,3,6,5,5,1,1,5,5,5,5,1,1,5,5,4,3,1,3,8,0,0,0,0,0,
  0,0,0,0,0,2,3,1,3,6,5,5,5,5,5,5,5,5,5,5,5,5,4,3,1,3,2,0,0,0,0,0,
  0,0,0,0,0,0,3,2,3,6,5,5,5,5,5,5,5,5,5,5,5,5,4,3,2,3,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,6,5,5,5,5,5,7,7,5,5,5,5,5,4,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,6,5,5,5,5,5,5,5,5,5,5,5,5,4,0,0,0,0,0,0,0,0,0,
  1,0,0,0,0,0,0,0,0,6,5,5,5,5,5,5,5,5,5,5,5,5,4,0,0,0,0,0,0,0,0,2,
  0,2,2,2,0,0,0,0,0,6,5,5,5,5,5,5,5,5,5,5,5,5,4,0,0,0,0,0,2,2,2,0,
  0,0,0,3,2,0,0,0,6,5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,0,0,0,2,2,0,0,0,
  0,0,0,3,2,0,0,0,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,4,0,0,0,2,3,0,0,0,
  0,0,0,0,3,2,0,0,0,0,3,3,0,3,3,0,0,3,3,0,3,3,0,0,0,0,2,2,0,0,0,0,
  0,0,0,0,3,2,0,0,0,0,3,2,0,3,2,0,0,3,2,0,3,2,0,0,0,0,2,3,0,0,0,0,
  0,0,0,0,0,3,2,0,0,3,2,0,0,3,2,0,0,3,2,0,0,3,2,0,0,2,3,0,0,0,0,0,
  0,0,0,0,0,3,2,2,2,2,0,0,0,3,2,0,0,3,2,0,0,0,3,2,2,2,3,0,0,0,0,0,
  0,0,0,0,0,0,3,3,3,0,0,0,0,3,2,0,0,3,2,0,0,0,0,3,3,3,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

uint16_t mode_2DAkemi(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  unsigned counter = (strip.now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  const float lightFactor  = 0.15f;
  const float normalFactor = 0.4f;

  um_data_t *um_data;
  if (!UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
    um_data = simulateSound(SEGMENT.soundSim);
  }
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];
  float base = fftResult[0]/255.0f;

  //draw and color Akemi
  for (int y=0; y < rows; y++) for (int x=0; x < cols; x++) {
    CRGB color;
    CRGB soundColor = CRGB::Orange;
    CRGB faceColor  = CRGB(SEGMENT.color_wheel(counter));
    CRGB armsAndLegsColor = CRGB(SEGCOLOR(1) > 0 ? SEGCOLOR(1) : 0xFFE0A0); //default warmish white 0xABA8FF; //0xFF52e5;//
    uint8_t ak = pgm_read_byte_near(akemi + ((y * 32)/rows) * 32 + (x * 32)/cols); // akemi[(y * 32)/rows][(x * 32)/cols]
    switch (ak) {
      case 3: armsAndLegsColor.r *= lightFactor;  armsAndLegsColor.g *= lightFactor;  armsAndLegsColor.b *= lightFactor;  color = armsAndLegsColor; break; //light arms and legs 0x9B9B9B
      case 2: armsAndLegsColor.r *= normalFactor; armsAndLegsColor.g *= normalFactor; armsAndLegsColor.b *= normalFactor; color = armsAndLegsColor; break; //normal arms and legs 0x888888
      case 1: color = armsAndLegsColor; break; //dark arms and legs 0x686868
      case 6: faceColor.r *= lightFactor;  faceColor.g *= lightFactor;  faceColor.b *= lightFactor;  color=faceColor; break; //light face 0x31AAFF
      case 5: faceColor.r *= normalFactor; faceColor.g *= normalFactor; faceColor.b *= normalFactor; color=faceColor; break; //normal face 0x0094FF
      case 4: color = faceColor; break; //dark face 0x007DC6
      case 7: color = SEGCOLOR(2) > 0 ? SEGCOLOR(2) : 0xFFFFFF; break; //eyes and mouth default white
      case 8: if (base > 0.4) {soundColor.r *= base; soundColor.g *= base; soundColor.b *= base; color=soundColor;} else color = armsAndLegsColor; break;
      default: color = BLACK; break;
    }

    if (SEGMENT.intensity > 128 && fftResult && fftResult[0] > 128) { //dance if base is high
      SEGMENT.setPixelColorXY(x, 0, BLACK);
      SEGMENT.setPixelColorXY(x, y+1, color);
    } else
      SEGMENT.setPixelColorXY(x, y, color);
  }

  //add geq left and right
  if (um_data && fftResult) {
    int xMax = cols/8;
    for (int x=0; x < xMax; x++) {
      unsigned band = map(x, 0, max(xMax,4), 0, 15);  // map 0..cols/8 to 16 GEQ bands
      band = constrain(band, 0, 15);
      int barHeight = map(fftResult[band], 0, 255, 0, 17*rows/32);
      uint32_t color = SEGMENT.color_from_palette((band * 35), false, PALETTE_SOLID_WRAP, 0);

      for (int y=0; y < barHeight; y++) {
        SEGMENT.setPixelColorXY(x, rows/2-y, color);
        SEGMENT.setPixelColorXY(cols-1-x, rows/2-y, color);
      }
    }
  }

  return FRAMETIME;
} // mode_2DAkemi
static const char _data_FX_MODE_2DAKEMI[] PROGMEM = "Akemi@Color speed,Dance;Head palette,Arms & Legs,Eyes & Mouth;Face palette;2f;si=0"; //beatsin


// Distortion waves - ldirko
// https://editor.soulmatelights.com/gallery/1089-distorsion-waves
// adapted for WLED by @blazoncek, improvements by @dedehai
uint16_t mode_2Ddistortionwaves() {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  uint8_t speed = SEGMENT.speed/32;
  uint8_t scale = SEGMENT.intensity/32;
  if(SEGMENT.check2) scale += 192 / (cols+rows); // zoom out some more. note: not changing scale slider for backwards compatibility

  unsigned a  = strip.now/32;
  unsigned a2 = a/2;
  unsigned a3 = a/3;
  unsigned colsScaled = cols * scale;
  unsigned rowsScaled = rows * scale;

  unsigned cx =  beatsin16_t(10-speed,0,colsScaled);
  unsigned cy =  beatsin16_t(12-speed,0,rowsScaled);
  unsigned cx1 = beatsin16_t(13-speed,0,colsScaled);
  unsigned cy1 = beatsin16_t(15-speed,0,rowsScaled);
  unsigned cx2 = beatsin16_t(17-speed,0,colsScaled);
  unsigned cy2 = beatsin16_t(14-speed,0,rowsScaled);

  byte rdistort, gdistort, bdistort;

  unsigned xoffs = 0;
  for (int x = 0; x < cols; x++) {
    xoffs += scale;
    unsigned yoffs = 0;

    for (int y = 0; y < rows; y++) {
       yoffs += scale;

      if(SEGMENT.check3) {
        // alternate mode from original code
        rdistort = cos8_t (((x+y)*8+a2)&255)>>1;
        gdistort = cos8_t (((x+y)*8+a3+32)&255)>>1;
        bdistort = cos8_t (((x+y)*8+a+64)&255)>>1;
      } else {
        rdistort = cos8_t((cos8_t(((x<<3)+a )&255)+cos8_t(((y<<3)-a2)&255)+a3   )&255)>>1;
        gdistort = cos8_t((cos8_t(((x<<3)-a2)&255)+cos8_t(((y<<3)+a3)&255)+a+32 )&255)>>1;
        bdistort = cos8_t((cos8_t(((x<<3)+a3)&255)+cos8_t(((y<<3)-a) &255)+a2+64)&255)>>1;
      }

      byte valueR = rdistort + ((a- ( ((xoffs - cx)  * (xoffs - cx)  + (yoffs - cy)  * (yoffs - cy))>>7  ))<<1);
      byte valueG = gdistort + ((a2-( ((xoffs - cx1) * (xoffs - cx1) + (yoffs - cy1) * (yoffs - cy1))>>7 ))<<1);
      byte valueB = bdistort + ((a3-( ((xoffs - cx2) * (xoffs - cx2) + (yoffs - cy2) * (yoffs - cy2))>>7 ))<<1);

      valueR = gamma8(cos8_t(valueR));
      valueG = gamma8(cos8_t(valueG));
      valueB = gamma8(cos8_t(valueB));

      if(SEGMENT.palette == 0) {
        // use RGB values (original color mode)
        SEGMENT.setPixelColorXY(x, y, RGBW32(valueR, valueG, valueB, 0));
      } else {
        // use palette
        uint8_t brightness = (valueR + valueG + valueB) / 3;
        if(SEGMENT.check1) { // map brightness to palette index
          SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, brightness, 255, LINEARBLEND_NOWRAP));
        } else {
          // color mapping: calculate hue from pixel color, map it to palette index
          CHSV hsvclr = rgb2hsv_approximate(CRGB(valueR>>2, valueG>>2, valueB>>2)); // scale colors down to not saturate for better hue extraction
          SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, hsvclr.h, brightness));
        }
      }
    }
  }

  // palette mode and not filling: smear-blur to cover up palette wrapping artefacts
  if(!SEGMENT.check1 && SEGMENT.palette)
    SEGMENT.blur(200, true);

  return FRAMETIME;
}
static const char _data_FX_MODE_2DDISTORTIONWAVES[] PROGMEM = "Distortion Waves@!,Scale,,,,Fill,Zoom,Alt;;!;2;pal=0";


//Soap
//@Stepko
//Idea from https://www.youtube.com/watch?v=DiHBgITrZck&ab_channel=StefanPetrick
// adapted for WLED by @blazoncek, optimization by @dedehai
static void soapPixels(bool isRow, uint8_t *noise3d, CRGB *pixels) {
  const int  cols = SEG_W;
  const int  rows = SEG_H;
  const auto XY   = [&](int x, int y) { return x + y * cols; };
  const auto abs  = [](int x) { return x<0 ? -x : x; };
  const int  tRC  = isRow ? rows : cols; // transpose if isRow
  const int  tCR  = isRow ? cols : rows; // transpose if isRow
  const int  amplitude = max(1, (tCR - 8) >> 3) * (1 + (SEGMENT.custom1 >> 5));
  const int  shift = 0; //(128 - SEGMENT.custom2)*2;

  CRGB ledsbuff[tCR];

  for (int i = 0; i < tRC; i++) {
    int amount   = ((int)noise3d[isRow ? i*cols : i] - 128) * amplitude + shift; // use first row/column: XY(0,i)/XY(i,0)
    int delta    = abs(amount) >> 8;
    int fraction = abs(amount) & 255;
    for (int j = 0; j < tCR; j++) {
      int zD, zF;
      if (amount < 0) {
        zD = j - delta;
        zF = zD - 1;
      } else {
        zD = j + delta;
        zF = zD + 1;
      }
      int yA = abs(zD)%tCR;
      int yB = abs(zF)%tCR;
      int xA = i;
      int xB = i;
      if (isRow) {
        std::swap(xA,yA);
        std::swap(xB,yB);
      }
      const int indxA = XY(xA,yA);
      const int indxB = XY(xB,yB);
      CRGB PixelA;
      CRGB PixelB;
      if ((zD >= 0) && (zD < tCR)) PixelA = pixels[indxA];
      else                         PixelA = ColorFromPalette(SEGPALETTE, ~noise3d[indxA]*3);
      if ((zF >= 0) && (zF < tCR)) PixelB = pixels[indxB];
      else                         PixelB = ColorFromPalette(SEGPALETTE, ~noise3d[indxB]*3);
      ledsbuff[j] = (PixelA.nscale8(ease8InOutApprox(255 - fraction))) + (PixelB.nscale8(ease8InOutApprox(fraction)));
    }
    for (int j = 0; j < tCR; j++) {
      CRGB c = ledsbuff[j];
      if (isRow) std::swap(j,i);
      SEGMENT.setPixelColorXY(i, j, pixels[XY(i,j)] = c);
      if (isRow) std::swap(j,i);
    }
  }
}

uint16_t mode_2Dsoap() {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const auto XY = [&](int x, int y) { return x + y * cols; };

  const size_t segSize = SEGMENT.width() * SEGMENT.height(); // prevent reallocation if mirrored or grouped
  const size_t dataSize = segSize * (sizeof(uint8_t) + sizeof(CRGB)); // pixels and noise
  if (!SEGENV.allocateData(dataSize + sizeof(uint32_t)*3)) return mode_static(); //allocation failed

  uint8_t  *noise3d    = reinterpret_cast<uint8_t*>(SEGENV.data);
  CRGB     *pixels     = reinterpret_cast<CRGB*>(SEGENV.data + segSize * sizeof(uint8_t));
  uint32_t *noisecoord = reinterpret_cast<uint32_t*>(SEGENV.data + dataSize); // x, y, z coordinates
  const uint32_t scale32_x = 160000U/cols;
  const uint32_t scale32_y = 160000U/rows;
  const uint32_t mov = MIN(cols,rows)*(SEGMENT.speed+2)/2;
  const uint8_t  smoothness = MIN(250,SEGMENT.intensity); // limit as >250 produces very little changes

  if (SEGENV.call == 0) for (int i = 0; i < 3; i++) noisecoord[i] = hw_random(); // init
  else                  for (int i = 0; i < 3; i++) noisecoord[i] += mov;

  for (int i = 0; i < cols; i++) {
    int32_t ioffset = scale32_x * (i - cols / 2);
    for (int j = 0; j < rows; j++) {
      int32_t joffset = scale32_y * (j - rows / 2);
      uint8_t data = perlin16(noisecoord[0] + ioffset, noisecoord[1] + joffset, noisecoord[2]) >> 8;
      noise3d[XY(i,j)] = scale8(noise3d[XY(i,j)], smoothness) + scale8(data, 255 - smoothness);
    }
  }
  // init also if dimensions changed
  if (SEGENV.call == 0 || SEGMENT.aux0 != cols || SEGMENT.aux1 != rows) {
    SEGMENT.aux0 = cols;
    SEGMENT.aux1 = rows;
    for (int i = 0; i < cols; i++) {
      for (int j = 0; j < rows; j++) {
        SEGMENT.setPixelColorXY(i, j, ColorFromPalette(SEGPALETTE,~noise3d[XY(i,j)]*3));
      }
    }
  }

  soapPixels(true,  noise3d, pixels); // rows
  soapPixels(false, noise3d, pixels); // cols

  return FRAMETIME;
}
static const char _data_FX_MODE_2DSOAP[] PROGMEM = "Soap@!,Smoothness,Density;;!;2;pal=11";


//Idea from https://www.youtube.com/watch?v=HsA-6KIbgto&ab_channel=GreatScott%21
//Octopus (https://editor.soulmatelights.com/gallery/671-octopus)
//Stepko and Sutaburosu
// adapted for WLED by @blazoncek
uint16_t mode_2Doctopus() {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const auto XY = [&](int x, int y) { return (x%cols) + (y%rows) * cols; };
  const uint8_t mapp = 180 / MAX(cols,rows);

  typedef struct {
    uint8_t angle;
    uint8_t radius;
  } map_t;

  const size_t dataSize = SEGMENT.width() * SEGMENT.height() * sizeof(map_t); // prevent reallocation if mirrored or grouped
  if (!SEGENV.allocateData(dataSize + 2)) return mode_static(); //allocation failed

  map_t *rMap = reinterpret_cast<map_t*>(SEGENV.data);
  uint8_t *offsX = reinterpret_cast<uint8_t*>(SEGENV.data + dataSize);
  uint8_t *offsY = reinterpret_cast<uint8_t*>(SEGENV.data + dataSize + 1);

  // re-init if SEGMENT dimensions or offset changed
  if (SEGENV.call == 0 || SEGENV.aux0 != cols || SEGENV.aux1 != rows || SEGMENT.custom1 != *offsX || SEGMENT.custom2 != *offsY) {
    SEGENV.step = 0; // t
    SEGENV.aux0 = cols;
    SEGENV.aux1 = rows;
    *offsX = SEGMENT.custom1;
    *offsY = SEGMENT.custom2;
    const int C_X = (cols / 2) + ((SEGMENT.custom1 - 128)*cols)/255;
    const int C_Y = (rows / 2) + ((SEGMENT.custom2 - 128)*rows)/255;
    for (int x = 0; x < cols; x++) {
      for (int y = 0; y < rows; y++) {
        int dx = (x - C_X);
        int dy = (y - C_Y);
        rMap[XY(x, y)].angle  = int(40.7436f * atan2_t(dy, dx));  // avoid 128*atan2()/PI
        rMap[XY(x, y)].radius = sqrtf(dx * dx + dy * dy) * mapp; //thanks Sutaburosu
      }
    }
  }

  SEGENV.step += SEGMENT.speed / 32 + 1;  // 1-4 range
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      byte angle = rMap[XY(x,y)].angle;
      byte radius = rMap[XY(x,y)].radius;
      //CRGB c = CHSV(SEGENV.step / 2 - radius, 255, sin8_t(sin8_t((angle * 4 - radius) / 4 + SEGENV.step) + radius - SEGENV.step * 2 + angle * (SEGMENT.custom3/3+1)));
      unsigned intensity = sin8_t(sin8_t((angle * 4 - radius) / 4 + SEGENV.step/2) + radius - SEGENV.step + angle * (SEGMENT.custom3/4+1));
      intensity = map((intensity*intensity) & 0xFFFF, 0, 65535, 0, 255); // add a bit of non-linearity for cleaner display
      SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, SEGENV.step / 2 - radius, intensity));
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_2DOCTOPUS[] PROGMEM = "Octopus@!,,Offset X,Offset Y,Legs,fasttan;;!;2;";


//Waving Cell
//@Stepko (https://editor.soulmatelights.com/gallery/1704-wavingcells)
// adapted for WLED by @blazoncek, improvements by @dedehai
uint16_t mode_2Dwavingcell() {
  if (!strip.isMatrix || !SEGMENT.is2D()) return mode_static(); // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  uint32_t t = (strip.now*(SEGMENT.speed + 1))>>3;
  uint32_t aX = SEGMENT.custom1/16 + 9;
  uint32_t aY = SEGMENT.custom2/16 + 1;
  uint32_t aZ = SEGMENT.custom3 + 1;
   for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      uint32_t wave = sin8_t((x * aX) + sin8_t((((y<<8) + t) * aY)>>8)) + cos8_t(y * aZ); // bit shifts to increase temporal resolution
      uint8_t colorIndex = wave + (t>>(8-(SEGMENT.check2*3)));
      SEGMENT.setPixelColorXY(x, y, ColorFromPalette(SEGPALETTE, colorIndex));
    }
  }
  SEGMENT.blur(SEGMENT.intensity);
  return FRAMETIME;
}
static const char _data_FX_MODE_2DWAVINGCELL[] PROGMEM = "Waving Cell@!,Blur,Amplitude 1,Amplitude 2,Amplitude 3,,Flow;;!;2;ix=0";

#ifndef WLED_DISABLE_PARTICLESYSTEM2D

/*
  Particle System Vortex
  Particles sprayed from center with a rotating spray
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
#define NUMBEROFSOURCES 8
uint16_t mode_particlevortex(void) {
  if (SEGLEN == 1)
    return mode_static();
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i, j;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, NUMBEROFSOURCES))
      return mode_static(); // allocation failed
    #ifdef ESP8266
    PartSys->setMotionBlur(180);
    #else
    PartSys->setMotionBlur(130);
    #endif
    for (i = 0; i < min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES); i++) {
      PartSys->sources[i].source.x = (PartSys->maxX + 1) >> 1; // center
      PartSys->sources[i].source.y = (PartSys->maxY + 1) >> 1; // center
      PartSys->sources[i].maxLife = 900;
      PartSys->sources[i].minLife = 800;
    }
    PartSys->setKillOutOfBounds(true);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  uint32_t spraycount = min(PartSys->numSources, (uint32_t)(1 + (SEGMENT.custom1 >> 5))); // number of sprays to display, 1-8
  #ifdef ESP8266
  for (i = 1; i < 4; i++) { // need static particles in the center to reduce blinking (would be black every other frame without this hack), just set them there fixed
    int partindex = (int)PartSys->usedParticles - (int)i;
    if (partindex >= 0) {
      PartSys->particles[partindex].x = (PartSys->maxX + 1) >> 1; // center
      PartSys->particles[partindex].y = (PartSys->maxY + 1) >> 1; // center
      PartSys->particles[partindex].sat = 230;
      PartSys->particles[partindex].ttl = 256; //keep alive
    }
  }
  #endif

  if (SEGMENT.check1)
    PartSys->setSmearBlur(90); // enable smear blur
  else
    PartSys->setSmearBlur(0); // disable smear blur

  // update colors of the sprays
  for (i = 0; i < spraycount; i++) {
      uint32_t coloroffset = 0xFF / spraycount;
      PartSys->sources[i].source.hue = coloroffset * i;
  }

  // set rotation direction and speed
  // can use direction flag to determine current direction
  bool direction = SEGMENT.check2; //no automatic direction change, set it to flag
  int32_t currentspeed = (int32_t)SEGENV.step; // make a signed integer out of step

  if (SEGMENT.custom2 > 0) { // automatic direction change enabled
    uint32_t changeinterval = 1040 - ((uint32_t)SEGMENT.custom2 << 2);
    direction = SEGENV.aux1 & 0x01; //set direction according to flag

    if (SEGMENT.check3) // random interval
      changeinterval = 20 + changeinterval + hw_random16(changeinterval);

    if (SEGMENT.call % changeinterval == 0) { //flip direction on next frame
      SEGENV.aux1 |= 0x02; // set the update flag (for random interval update)
      if (direction)
        SEGENV.aux1 &= ~0x01; // clear the direction flag
      else
        SEGENV.aux1 |= 0x01; // set the direction flag
    }
  }

  int32_t targetspeed = (direction ? 1 : -1) * (SEGMENT.speed << 3);
  int32_t speeddiff = targetspeed - currentspeed;
  int32_t speedincrement = speeddiff / 50;

  if (speedincrement == 0) { //if speeddiff is not zero, make the increment at least 1 so it reaches target speed
    if (speeddiff < 0)
      speedincrement = -1;
    else if (speeddiff > 0)
      speedincrement = 1;
  }

  currentspeed += speedincrement;
  SEGENV.aux0 += currentspeed;
  SEGENV.step = (uint32_t)currentspeed; //save it back

  uint16_t angleoffset = 0xFFFF / spraycount; // angle offset for an even distribution
  uint32_t skip = PS_P_HALFRADIUS / (SEGMENT.intensity + 1) + 1; // intensity is emit speed, emit less on low speeds
  if (SEGMENT.call % skip == 0) {
    j = hw_random16(spraycount); // start with random spray so all get a chance to emit a particle if maximum number of particles alive is reached.
    for (i = 0; i < spraycount; i++) { // emit one particle per spray (if available)
      PartSys->sources[j].var = (SEGMENT.custom3 >> 1); //update speed variation
      #ifdef ESP8266
      if (SEGMENT.call & 0x01) // every other frame, do not emit to save particles
      #endif
      PartSys->angleEmit(PartSys->sources[j], SEGENV.aux0 + angleoffset * j, (SEGMENT.intensity >> 2)+1);
      j = (j + 1) % spraycount;
    }
  }
  PartSys->update(); //update all particles and render to frame
  return FRAMETIME;
}
#undef NUMBEROFSOURCES
static const char _data_FX_MODE_PARTICLEVORTEX[] PROGMEM = "PS Vortex@Rotation Speed,Particle Speed,Arms,Flip,Nozzle,Smear,Direction,Random Flip;;!;2;pal=27,c1=200,c2=0,c3=0";

/*
  Particle Fireworks
  Rockets shoot up and explode in a random color, sometimes in a defined pattern
  by DedeHai (Damian Schneider)
*/
#define NUMBEROFSOURCES 8

uint16_t mode_particlefireworks(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t numRockets;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, NUMBEROFSOURCES))
      return mode_static(); // allocation failed

    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return (except on top, taken care of by gravity setting)
    PartSys->setWallHardness(120); // ground bounce is fixed
    numRockets = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES);
    for (uint32_t j = 0; j < numRockets; j++) {
      PartSys->sources[j].source.ttl = 500 * j; // first rocket starts immediately, others follow soon
      PartSys->sources[j].source.vy = -1; // at negative speed, no particles are emitted and if rocket dies, it will be relaunched
    }
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  numRockets = map(SEGMENT.speed, 0 , 255, 4, min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES));

  PartSys->setWrapX(SEGMENT.check1);
  PartSys->setBounceY(SEGMENT.check2);
  PartSys->setGravity(map(SEGMENT.custom3, 0, 31, SEGMENT.check2 ? 1 : 0, 10)); // if bounded, set gravity to minimum of 1 or they will bounce at top
  PartSys->setMotionBlur(map(SEGMENT.custom2, 0, 255, 0, 245)); // anable motion blur

  // update the rockets, set the speed state
  for (uint32_t j = 0; j < numRockets; j++) {
      PartSys->applyGravity(PartSys->sources[j].source);
      PartSys->particleMoveUpdate(PartSys->sources[j].source, PartSys->sources[j].sourceFlags);
      if (PartSys->sources[j].source.ttl == 0) {
        if (PartSys->sources[j].source.vy > 0) { // rocket has died and is moving up. stop it so it will explode (is handled in the code below)
          PartSys->sources[j].source.vy = 0;
        }
        else if (PartSys->sources[j].source.vy < 0) { // rocket is exploded and time is up (ttl=0 and negative speed), relaunch it
          PartSys->sources[j].source.y = PS_P_RADIUS; // start from bottom
          PartSys->sources[j].source.x = (PartSys->maxX >> 2) + hw_random(PartSys->maxX >> 1); // centered half
          PartSys->sources[j].source.vy = (SEGMENT.custom3) + hw_random16(SEGMENT.custom1 >> 3) + 5; // rocket speed TODO: need to adjust for segment height
          PartSys->sources[j].source.vx = hw_random16(7) - 3; // not perfectly straight up
          PartSys->sources[j].source.sat = 30; // low saturation -> exhaust is off-white
          PartSys->sources[j].source.ttl = hw_random16(SEGMENT.custom1) + (SEGMENT.custom1 >> 1); // set fuse time
          PartSys->sources[j].maxLife = 40; // exhaust particle life
          PartSys->sources[j].minLife = 10;
          PartSys->sources[j].vx = 0;  // emitting speed
          PartSys->sources[j].vy = -5;  // emitting speed
          PartSys->sources[j].var = 4; // speed variation around vx,vy (+/- var)
        }
     }
  }
  // check each rocket's state and emit particles according to its state: moving up = emit exhaust, at top = explode; falling down = standby time
  uint32_t emitparticles, frequency, baseangle, hueincrement; // number of particles to emit for each rocket's state
  // variables for circular explosions
  [[maybe_unused]] int32_t speed, currentspeed, speedvariation, percircle;
  int32_t counter = 0;
  [[maybe_unused]] uint16_t angle;
  [[maybe_unused]] unsigned angleincrement;
  bool circularexplosion = false;

  // emit particles for each rocket
  for (uint32_t j = 0; j < numRockets; j++) {
    // determine rocket state by its speed:
    if (PartSys->sources[j].source.vy > 0) { // moving up, emit exhaust
      emitparticles = 1;
    }
    else if (PartSys->sources[j].source.vy < 0) { // falling down, standby time
      emitparticles = 0;
    }
    else { // speed is zero, explode!
      PartSys->sources[j].source.hue = hw_random16(); // random color
      PartSys->sources[j].source.sat = hw_random16(55) + 200;
      PartSys->sources[j].maxLife = 200;
      PartSys->sources[j].minLife = 100;
      PartSys->sources[j].source.ttl = hw_random16((2000 - ((uint32_t)SEGMENT.speed << 2))) + 550 - (SEGMENT.speed << 1); // standby time til next launch
      PartSys->sources[j].var = ((SEGMENT.intensity >> 4) + 5); // speed variation around vx,vy (+/- var)
      PartSys->sources[j].source.vy = -1; // set speed negative so it will emit no more particles after this explosion until relaunch
      #ifdef ESP8266
      emitparticles = hw_random16(SEGMENT.intensity >> 3) + (SEGMENT.intensity >> 3) + 5; // defines the size of the explosion
      #else
      emitparticles = hw_random16(SEGMENT.intensity >> 2) + (SEGMENT.intensity >> 2) + 5; // defines the size of the explosion
      #endif

      if (random16() & 1) { // 50% chance for circular explosion
        circularexplosion = true;
        speed = 2 + hw_random16(3) + ((SEGMENT.intensity >> 6));
        currentspeed = speed;
        angleincrement = 2730 + hw_random16(5461); // minimum 15° + random(30°)
        angle = hw_random16(); // random start angle
        baseangle = angle; // save base angle for modulation
        percircle = 0xFFFF / angleincrement + 1; // number of particles to make complete circles
        hueincrement = hw_random16() & 127; // &127 is equivalent to %128
        int circles = 1 + hw_random16(3) + ((SEGMENT.intensity >> 6));
        frequency = hw_random16() & 127; // modulation frequency (= "waves per circle"), x.4 fixed point
        emitparticles = percircle * circles;
        PartSys->sources[j].var = angle & 1; // 0 or 1 variation, angle is random
      }
    }
    uint32_t i;
    for (i = 0; i < emitparticles; i++) {
      if (circularexplosion) {
        int32_t sineMod = 0xEFFF + sin16_t((uint16_t)(((angle * frequency) >> 4) + baseangle)); // shifted to positive values
        currentspeed = (speed/2 + ((sineMod * speed) >> 16)) >> 1; // sine modulation on speed based on emit angle
        PartSys->angleEmit(PartSys->sources[j], angle, currentspeed); // note: compiler warnings can be ignored, variables are set just above
        counter++;
        if (counter > percircle) { // full circle completed, increase speed
          counter = 0;
          speed += 3 + ((SEGMENT.intensity >> 6)); // increase speed to form a second wave
          PartSys->sources[j].source.hue += hueincrement; // new color for next circle
          PartSys->sources[j].source.sat = 100 + hw_random16(156);
        }
        angle += angleincrement; // set angle for next particle
      }
      else { // random explosion or exhaust
        PartSys->sprayEmit(PartSys->sources[j]);
        if ((j % 3) == 0) {
          PartSys->sources[j].source.hue = hw_random16(); // random color for each particle (this is also true for exhaust, but that is white anyways)
        }
      }
    }
    if (i == 0) // no particles emitted, this rocket is falling
      PartSys->sources[j].source.y = 1000; // reset position so gravity wont pull it to the ground and bounce it (vy MUST stay negative until relaunch)
    circularexplosion = false; // reset for next rocket
  }
  if (SEGMENT.check3) { // fast speed, move particles twice
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
      PartSys->particleMoveUpdate(PartSys->particles[i], PartSys->particleFlags[i], nullptr, nullptr);
    }
  }
  PartSys->update(); // update and render
  return FRAMETIME;
}
#undef NUMBEROFSOURCES
static const char _data_FX_MODE_PARTICLEFIREWORKS[] PROGMEM = "PS Fireworks@Launches,Explosion Size,Fuse,Blur,Gravity,Cylinder,Ground,Fast;;!;2;pal=11,ix=50,c1=40,c2=0,c3=12";

/*
  Particle Volcano
  Particles are sprayed from below, spray moves back and forth if option is set
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
#define NUMBEROFSOURCES 1
uint16_t mode_particlevolcano(void) {
  ParticleSystem2D *PartSys = nullptr;
  PSsettings2D volcanosettings;
  volcanosettings.asByte = 0b00000100; // PS settings for volcano movement: bounceX is enabled
  uint8_t numSprays; // note: so far only one tested but more is possible
  uint32_t i = 0;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, NUMBEROFSOURCES)) // init, no additional data needed
      return mode_static(); // allocation failed or not 2D

    PartSys->setBounceY(true);
    PartSys->setGravity(); // enable with default gforce
    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return (except on top, taken care of by gravity setting)
    PartSys->setMotionBlur(230); // anable motion blur

    numSprays = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES); // number of sprays
    for (i = 0; i < numSprays; i++) {
      PartSys->sources[i].source.hue = hw_random16();
      PartSys->sources[i].source.x = PartSys->maxX / (numSprays + 1) * (i + 1); // distribute evenly
      PartSys->sources[i].maxLife = 300; // lifetime in frames
      PartSys->sources[i].minLife = 250;
      PartSys->sources[i].sourceFlags.collide = true; // seeded particles will collide (if enabled)
      PartSys->sources[i].sourceFlags.perpetual = true; // source never dies
    }
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  numSprays = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES); // number of volcanoes

  // change source emitting color from time to time, emit one particle per spray
  if (SEGMENT.call % (11 - (SEGMENT.intensity / 25)) == 0) { // every nth frame, cycle color and emit particles (and update the sources)
    for (i = 0; i < numSprays; i++) {
      PartSys->sources[i].source.y = PS_P_RADIUS + 5; // reset to just above the lower edge that is allowed for bouncing particles, if zero, particles already 'bounce' at start and loose speed.
      PartSys->sources[i].source.vy = 0; //reset speed (so no extra particlesettin is required to keep the source 'afloat')
      PartSys->sources[i].source.hue++; // = hw_random16(); //change hue of spray source (note: random does not look good)
      PartSys->sources[i].source.vx = PartSys->sources[i].source.vx > 0 ? (SEGMENT.custom1 >> 2) : -(SEGMENT.custom1 >> 2); // set moving speed but keep the direction given by PS
      PartSys->sources[i].vy = SEGMENT.speed >> 2; // emitting speed (upwards)
      PartSys->sources[i].vx = 0;
      PartSys->sources[i].var = SEGMENT.custom3 >> 1; // emiting variation = nozzle size (custom 3 goes from 0-31)
      PartSys->sprayEmit(PartSys->sources[i]);
      PartSys->setWallHardness(255); // full hardness for source bounce
      PartSys->particleMoveUpdate(PartSys->sources[i].source, PartSys->sources[i].sourceFlags, &volcanosettings); //move the source
    }
  }

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setColorByAge(SEGMENT.check1);
  PartSys->setBounceX(SEGMENT.check2);
  PartSys->setWallHardness(SEGMENT.custom2);

  if (SEGMENT.check3) // collisions enabled
    PartSys->enableParticleCollisions(true, SEGMENT.custom2); // enable collisions and set particle collision hardness
  else
    PartSys->enableParticleCollisions(false);

  PartSys->update(); // update and render
  return FRAMETIME;
}
#undef NUMBEROFSOURCES
static const char _data_FX_MODE_PARTICLEVOLCANO[] PROGMEM = "PS Volcano@Speed,Intensity,Move,Bounce,Spread,AgeColor,Walls,Collide;;!;2;pal=35,sx=100,ix=190,c1=0,c2=160,c3=6,o1=1";

/*
  Particle Fire
  realistic fire effect using particles. heat based and using perlin-noise for wind
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlefire(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i; // index variable
  uint32_t numFlames; // number of flames: depends on fire width. for a fire width of 16 pixels, about 25-30 flames give good results

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, SEGMENT.virtualWidth(), 4)) //maximum number of source (PS may limit based on segment size); need 4 additional bytes for time keeping (uint32_t lastcall)
      return mode_static(); // allocation failed or not 2D
    SEGENV.aux0 = hw_random16(); // aux0 is wind position (index) in the perlin noise
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setWrapX(SEGMENT.check2);
  PartSys->setMotionBlur(SEGMENT.check1 * 170); // anable/disable motion blur
  PartSys->setSmearBlur(!SEGMENT.check1 * 60);  // enable smear blur if motion blur is not enabled

  uint32_t firespeed = max((uint8_t)100, SEGMENT.speed); //limit speed to 100 minimum, reduce frame rate to make it slower (slower speeds than 100 do not look nice)
  if (SEGMENT.speed < 100) { //slow, limit FPS
    uint32_t *lastcall = reinterpret_cast<uint32_t *>(PartSys->PSdataEnd);
    uint32_t period = strip.now - *lastcall;
    if (period < (uint32_t)map(SEGMENT.speed, 0, 99, 50, 10)) { // limit to 90FPS - 20FPS
      SEGMENT.call--; //skipping a frame, decrement the counter (on call0, this is never executed as lastcall is 0, so its fine to not check if >0)
      //still need to render the frame or flickering will occur in transitions
      PartSys->updateFire(SEGMENT.intensity, true); // render the fire without updating particles (render only)
      return FRAMETIME; //do not update this frame
    }
    *lastcall = strip.now;
  }

  uint32_t spread = (PartSys->maxX >> 5) * (SEGMENT.custom3 + 1); //fire around segment center (in subpixel points)
  numFlames = min((uint32_t)PartSys->numSources, (4 + ((spread / PS_P_RADIUS) << 1))); // number of flames used depends on spread with, good value is (fire width in pixel) * 2
  uint32_t percycle = (numFlames * 2) / 3; // maximum number of particles emitted per cycle (TODO: for ESP826 maybe use flames/2)

  // update the flame sprays:
  for (i = 0; i < numFlames; i++) {
    if (SEGMENT.call & 1 && PartSys->sources[i].source.ttl > 0) { // every second frame
      PartSys->sources[i].source.ttl--;
    } else { // flame source is dead: initialize new flame: set properties of source
      PartSys->sources[i].source.x = (PartSys->maxX >> 1) - (spread >> 1) + hw_random(spread); // change flame position: distribute randomly on chosen width
      PartSys->sources[i].source.y = -(PS_P_RADIUS << 2); // set the source below the frame
      PartSys->sources[i].source.ttl = 20 + hw_random16((SEGMENT.custom1 * SEGMENT.custom1) >> 8) / (1 + (firespeed >> 5)); //'hotness' of fire, faster flames reduce the effect or flame height will scale too much with speed
      PartSys->sources[i].maxLife = hw_random16(SEGMENT.virtualHeight() >> 1) + 16; // defines flame height together with the vy speed, vy speed*maxlife/PS_P_RADIUS is the average flame height
      PartSys->sources[i].minLife = PartSys->sources[i].maxLife >> 1;
      PartSys->sources[i].vx = hw_random16(5) - 2; // emitting speed (sideways)
      PartSys->sources[i].vy = (SEGMENT.virtualHeight() >> 1) + (firespeed >> 4) + (SEGMENT.custom1 >> 4); // emitting speed (upwards)
      PartSys->sources[i].var = 2 + hw_random16(2 + (firespeed >> 4)); // speed variation around vx,vy (+/- var)
    }
  }

  if (SEGMENT.call % 3 == 0) { // update noise position and add wind
    SEGENV.aux0++; // position in the perlin noise matrix for wind generation
    if (SEGMENT.call % 10 == 0)
      SEGENV.aux1++; // move in noise y direction so noise does not repeat as often
    // add wind force to all particles
    int8_t windspeed = ((int16_t)(perlin8(SEGENV.aux0, SEGENV.aux1) - 127) * SEGMENT.custom2) >> 7;
    PartSys->applyForce(windspeed, 0);
  }
  SEGENV.step++;

  if (SEGMENT.check3) { //add turbulance (parameters and algorithm found by experimentation)
    if (SEGMENT.call % map(firespeed, 0, 255, 4, 15) == 0) {
      for (i = 0; i < PartSys->usedParticles; i++) {
        if (PartSys->particles[i].y < PartSys->maxY / 4) { // do not apply turbulance everywhere -> bottom quarter seems a good balance
          int32_t curl = ((int32_t)perlin8(PartSys->particles[i].x, PartSys->particles[i].y, SEGENV.step << 4) - 127);
          PartSys->particles[i].vx += (curl * (firespeed + 10)) >> 9;
        }
      }
    }
  }

  // emit faster sparks at first flame position, amount and speed mostly dependends on intensity
  if(hw_random8() < 10 + (SEGMENT.intensity >> 2)) {
    for (i = 0; i < PartSys->usedParticles; i++) {
      if (PartSys->particles[i].ttl == 0) { // find a dead particle
        PartSys->particles[i].ttl = hw_random16(SEGMENT.virtualHeight()) + 30;
        PartSys->particles[i].x = PartSys->sources[0].source.x;
        PartSys->particles[i].y = PartSys->sources[0].source.y;
        PartSys->particles[i].vx = PartSys->sources[0].source.vx;
        PartSys->particles[i].vy = (SEGMENT.virtualHeight() >> 1) + (firespeed >> 4) + ((30 + (SEGMENT.intensity >> 1) + SEGMENT.custom1) >> 4); // emitting speed (upwards)
        break; // emit only one particle
      }
    }
  }

  uint8_t j = hw_random16(); // start with a random flame (so each flame gets the chance to emit a particle if available particles is smaller than number of flames)
  for (i = 0; i < percycle; i++) {
    j = (j + 1) % numFlames;
    PartSys->flameEmit(PartSys->sources[j]);
  }

  PartSys->updateFire(SEGMENT.intensity, false); // update and render the fire

  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEFIRE[] PROGMEM = "PS Fire@Speed,Intensity,Flame Height,Wind,Spread,Smooth,Cylinder,Turbulence;;!;2;pal=35,sx=110,c1=110,c2=50,c3=31,o1=1";

/*
  PS Ballpit: particles falling down, user can enable these three options: X-wraparound, side bounce, ground bounce
  sliders control falling speed, intensity (number of particles spawned), inter-particle collision hardness (0 means no particle collisions) and render saturation
  this is quite versatile, can be made to look like rain or snow or confetti etc.
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlepit(void) {
  ParticleSystem2D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 0, 0, true, false)) // init
      return mode_static(); // allocation failed or not 2D
    PartSys->setKillOutOfBounds(true);
    PartSys->setGravity(); // enable with default gravity
    PartSys->setUsedParticles(170); // use 75% of available particles
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)

  PartSys->setWrapX(SEGMENT.check1);
  PartSys->setBounceX(SEGMENT.check2);
  PartSys->setBounceY(SEGMENT.check3);
  PartSys->setWallHardness(min(SEGMENT.custom2, (uint8_t)150)); // limit to 100 min (if collisions are disabled, still want bouncy)
  if (SEGMENT.custom2 > 0)
    PartSys->enableParticleCollisions(true, SEGMENT.custom2); // enable collisions and set particle collision hardness
  else
    PartSys->enableParticleCollisions(false);

  uint32_t i;
  if (SEGMENT.call % (128 - (SEGMENT.intensity >> 1)) == 0 && SEGMENT.intensity > 0) { // every nth frame emit particles, stop emitting if set to zero
    for (i = 0; i < PartSys->usedParticles; i++) { // emit particles
      if (PartSys->particles[i].ttl == 0) { // find a dead particle
        // emit particle at random position over the top of the matrix (random16 is not random enough)
        PartSys->particles[i].ttl = 1500 - (SEGMENT.speed << 2) + hw_random16(500); // if speed is higher, make them die sooner
        PartSys->particles[i].x = hw_random(PartSys->maxX); //random(PartSys->maxX >> 1) + (PartSys->maxX >> 2);
        PartSys->particles[i].y = (PartSys->maxY << 1); // particles appear somewhere above the matrix, maximum is double the height
        PartSys->particles[i].vx = (int16_t)hw_random16(SEGMENT.speed >> 1) - (SEGMENT.speed >> 2); // side speed is +/-
        PartSys->particles[i].vy = map(SEGMENT.speed, 0, 255, -5, -100); // downward speed
        PartSys->particles[i].hue = hw_random16(); // set random color
        PartSys->particleFlags[i].collide = true; // enable collision for particle
        PartSys->particles[i].sat = ((SEGMENT.custom3) << 3) + 7;
        // set particle size
        if (SEGMENT.custom1 == 255) {
          PartSys->setParticleSize(1); // set global size to 1 for advanced rendering (no single pixel particles)
          PartSys->advPartProps[i].size = hw_random16(SEGMENT.custom1); // set each particle to random size
        } else {
          PartSys->setParticleSize(SEGMENT.custom1); // set global size
          PartSys->advPartProps[i].size = 0; // use global size
        }
        break; // emit only one particle per round
      }
    }
  }

  uint32_t frictioncoefficient = 1 + SEGMENT.check1; //need more friction if wrapX is set, see below note
  if (SEGMENT.speed < 50) // for low speeds, apply more friction
    frictioncoefficient = 50 - SEGMENT.speed;

  if (SEGMENT.call % 6 == 0)// (3 + max(3, (SEGMENT.speed >> 2))) == 0) // note: if friction is too low, hard particles uncontrollably 'wander' left and right if wrapX is enabled
    PartSys->applyFriction(frictioncoefficient);

  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEPIT[] PROGMEM = "PS Ballpit@Speed,Intensity,Size,Hardness,Saturation,Cylinder,Walls,Ground;;!;2;pal=11,sx=100,ix=220,c1=120,c2=130,c3=31,o3=1";

/*
  Particle Waterfall
  Uses palette for particle color, spray source at top emitting particles, many config options
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlewaterfall(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint8_t numSprays;
  uint32_t i = 0;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 12)) // init, request 12 sources, no additional data needed
      return mode_static(); // allocation failed or not 2D

    PartSys->setGravity();  // enable with default gforce
    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return (except on top, taken care of by gravity setting)
    PartSys->setMotionBlur(190); // anable motion blur
    PartSys->setSmearBlur(30); // enable 2D blurring (smearing)
    for (i = 0; i < PartSys->numSources; i++) {
      PartSys->sources[i].source.hue = i*90;
      PartSys->sources[i].sourceFlags.collide = true; // seeded particles will collide
    #ifdef ESP8266
      PartSys->sources[i].maxLife = 250; // lifetime in frames (ESP8266 has less particles, make them short lived to keep the water flowing)
      PartSys->sources[i].minLife = 100;
    #else
      PartSys->sources[i].maxLife = 400; // lifetime in frames
      PartSys->sources[i].minLife = 150;
    #endif
    }
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setWrapX(SEGMENT.check1);   // cylinder
  PartSys->setBounceX(SEGMENT.check2); // walls
  PartSys->setBounceY(SEGMENT.check3); // ground
  PartSys->setWallHardness(SEGMENT.custom2);
  numSprays = min((int32_t)PartSys->numSources, max(PartSys->maxXpixel / 6, (int32_t)2)); // number of sprays depends on segment width
  if (SEGMENT.custom2 > 0) // collisions enabled
    PartSys->enableParticleCollisions(true, SEGMENT.custom2); // enable collisions and set particle collision hardness
  else {
    PartSys->enableParticleCollisions(false);
    PartSys->setWallHardness(120); // set hardness (for ground bounce) to fixed value if not using collisions
  }

  for (i = 0; i < numSprays; i++) {
      PartSys->sources[i].source.hue += 1 + hw_random16(SEGMENT.custom1>>1); // change hue of spray source
  }

  if (SEGMENT.call % (12 - (SEGMENT.intensity >> 5)) == 0 && SEGMENT.intensity > 0) { // every nth frame, emit particles, do not emit if intensity is zero
    for (i = 0; i < numSprays; i++) {
      PartSys->sources[i].vy = -SEGMENT.speed >> 3; // emitting speed, down
      //PartSys->sources[i].source.x = map(SEGMENT.custom3, 0, 31, 0, (PartSys->maxXpixel - numSprays * 2) * PS_P_RADIUS) + i * PS_P_RADIUS * 2; // emitter position
      PartSys->sources[i].source.x = map(SEGMENT.custom3, 0, 31, 0, (PartSys->maxXpixel - numSprays) * PS_P_RADIUS) + i * PS_P_RADIUS * 2; // emitter position
      PartSys->sources[i].source.y = PartSys->maxY + (PS_P_RADIUS * ((i<<2) + 4)); // source y position, few pixels above the top to increase spreading before entering the matrix
      PartSys->sources[i].var = (SEGMENT.custom1 >> 3); // emiting variation 0-32
      PartSys->sprayEmit(PartSys->sources[i]);
    }
  }

  if (SEGMENT.call % 20 == 0)
    PartSys->applyFriction(1); // add just a tiny amount of friction to help smooth things

  PartSys->update();   // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEWATERFALL[] PROGMEM = "PS Waterfall@Speed,Intensity,Variation,Collide,Position,Cylinder,Walls,Ground;;!;2;pal=9,sx=15,ix=200,c1=32,c2=160,o3=1";

/*
  Particle Box, applies gravity to particles in either a random direction or random but only downwards (sloshing)
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlebox(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1)) // init
      return mode_static(); // allocation failed or not 2D
    PartSys->setBounceX(true);
    PartSys->setBounceY(true);
    SEGENV.aux0 = hw_random16(); // position in perlin noise
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setParticleSize(SEGMENT.custom3<<3);
  PartSys->setWallHardness(min(SEGMENT.custom2, (uint8_t)200)); // wall hardness is 200 or more
  PartSys->enableParticleCollisions(true, max(2, (int)SEGMENT.custom2)); // enable collisions and set particle collision hardness
  PartSys->setUsedParticles(map(SEGMENT.intensity, 0, 255, 2, 153)); // 1% - 60%
  // add in new particles if amount has changed
  for (i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->particles[i].ttl < 260) { // initialize handed over particles and dead particles
      PartSys->particles[i].ttl = 260; // full brigthness
      PartSys->particles[i].x = hw_random16(PartSys->maxX);
      PartSys->particles[i].y = hw_random16(PartSys->maxY);
      PartSys->particles[i].hue = hw_random8(); // make it colorful
      PartSys->particleFlags[i].perpetual = true; // never die
      PartSys->particleFlags[i].collide = true; // all particles colllide
      break; // only spawn one particle per frame for less chaotic transitions
    }
  }

  if (SEGMENT.call % (((255 - SEGMENT.speed) >> 6) + 1) == 0 && SEGMENT.speed > 0) { // how often the force is applied depends on speed setting
    int32_t xgravity;
    int32_t ygravity;
    int32_t increment = (SEGMENT.speed >> 6) + 1;

    if (SEGMENT.check2) { // washing machine
      int speed = tristate_square8(strip.now >> 7, 90, 15) / ((400 - SEGMENT.speed) >> 3);
      SEGENV.aux0 += speed;
      if (speed == 0) SEGENV.aux0 = 190; //down (= 270°)
    }
    else
      SEGENV.aux0 -= increment;

    if (SEGMENT.check1) { // random, use perlin noise
      xgravity = ((int16_t)perlin8(SEGENV.aux0) - 127);
      ygravity = ((int16_t)perlin8(SEGENV.aux0 + 10000) - 127);
      // scale the gravity force
      xgravity = (xgravity * SEGMENT.custom1) / 128;
      ygravity = (ygravity * SEGMENT.custom1) / 128;
    }
    else { // go in a circle
      xgravity = ((int32_t)(SEGMENT.custom1) * cos16_t(SEGENV.aux0 << 8)) / 0xFFFF;
      ygravity = ((int32_t)(SEGMENT.custom1) * sin16_t(SEGENV.aux0 << 8)) / 0xFFFF;
    }
    if (SEGMENT.check3) { // sloshing, y force is always downwards
      if (ygravity > 0)
        ygravity = -ygravity;
    }

    PartSys->applyForce(xgravity, ygravity);
  }

  if ((SEGMENT.call & 0x0F) == 0) // every 16th frame
    PartSys->applyFriction(1);

  PartSys->update();   // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEBOX[] PROGMEM = "PS Box@!,Particles,Tilt,Hardness,Size,Random,Washing Machine,Sloshing;;!;2;pal=53,ix=50,c3=1,o1=1";

/*
  Fuzzy Noise: Perlin noise 'gravity' mapping as in particles on 'noise hills' viewed from above
  calculates slope gradient at the particle positions and applies 'downhill' force, resulting in a fuzzy perlin noise display
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleperlin(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1, 0, true)) // init with 1 source and advanced properties
      return mode_static(); // allocation failed or not 2D

    PartSys->setKillOutOfBounds(true); // should never happen, but lets make sure there are no stray particles
    PartSys->setMotionBlur(230); // anable motion blur
    PartSys->setBounceY(true);
    SEGENV.aux0 = rand();
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setWrapX(SEGMENT.check1);
  PartSys->setBounceX(!SEGMENT.check1);
  PartSys->setWallHardness(SEGMENT.custom1); // wall hardness
  PartSys->enableParticleCollisions(SEGMENT.check3, SEGMENT.custom1); // enable collisions and set particle collision hardness
  PartSys->setUsedParticles(map(SEGMENT.intensity, 0, 255, 25, 128)); // min is 10%, max is 50%
  PartSys->setSmearBlur(SEGMENT.check2 * 15); // enable 2D blurring (smearing)

  // apply 'gravity' from a 2D perlin noise map
  SEGENV.aux0 += 1 + (SEGMENT.speed >> 5); // noise z-position
  // update position in noise
  for (i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->particles[i].ttl == 0) { // revive dead particles (do not keep them alive forever, they can clump up, need to reseed)
      PartSys->particles[i].ttl = hw_random16(500) + 200;
      PartSys->particles[i].x = hw_random(PartSys->maxX);
      PartSys->particles[i].y = hw_random(PartSys->maxY);
      PartSys->particleFlags[i].collide = true; // particle colllides
    }
    uint32_t scale = 16 - ((31 - SEGMENT.custom3) >> 1);
    uint16_t xnoise = PartSys->particles[i].x / scale; // position in perlin noise, scaled by slider
    uint16_t ynoise = PartSys->particles[i].y / scale;
    int16_t baseheight = perlin8(xnoise, ynoise, SEGENV.aux0); // noise value at particle position
    PartSys->particles[i].hue = baseheight; // color particles to perlin noise value
    if (SEGMENT.call % 8 == 0) { // do not apply the force every frame, is too chaotic
      int8_t xslope = (baseheight + (int16_t)perlin8(xnoise - 10, ynoise, SEGENV.aux0));
      int8_t yslope = (baseheight + (int16_t)perlin8(xnoise, ynoise - 10, SEGENV.aux0));
      PartSys->applyForce(i, xslope, yslope);
    }
  }

  if (SEGMENT.call % (16 - (SEGMENT.custom2 >> 4)) == 0)
    PartSys->applyFriction(2);

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEPERLIN[] PROGMEM = "PS Fuzzy Noise@Speed,Particles,Bounce,Friction,Scale,Cylinder,Smear,Collide;;!;2;pal=64,sx=50,ix=200,c1=130,c2=30,c3=5,o3=1";

/*
  Particle smashing down like meteors and exploding as they hit the ground, has many parameters to play with
  by DedeHai (Damian Schneider)
*/
#define NUMBEROFSOURCES 8
uint16_t mode_particleimpact(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i = 0;
  uint32_t numMeteors;
  PSsettings2D meteorsettings;
  meteorsettings.asByte = 0b00101000; // PS settings for meteors: bounceY and gravity enabled

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, NUMBEROFSOURCES)) // init, no additional data needed
      return mode_static(); // allocation failed or not 2D
    PartSys->setKillOutOfBounds(true);
    PartSys->setGravity(); // enable default gravity
    PartSys->setBounceY(true); // always use ground bounce
    PartSys->setWallRoughness(220); // high roughness
    numMeteors = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES);
    for (i = 0; i < numMeteors; i++) {
      PartSys->sources[i].source.ttl = hw_random16(10 * i); // set initial delay for meteors
      PartSys->sources[i].source.vy = 10; // at positive speeds, no particles are emitted and if particle dies, it will be relaunched
    }
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setWrapX(SEGMENT.check1);
  PartSys->setBounceX(SEGMENT.check2);
  PartSys->setMotionBlur(SEGMENT.custom3<<3);
  uint8_t hardness = map(SEGMENT.custom2, 0, 255, PS_P_MINSURFACEHARDNESS - 2, 255);
  PartSys->setWallHardness(hardness);
  PartSys->enableParticleCollisions(SEGMENT.check3, hardness); // enable collisions and set particle collision hardness
  numMeteors = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES);
  uint32_t emitparticles; // number of particles to emit for each rocket's state

  for (i = 0; i < numMeteors; i++) {
    // determine meteor state by its speed:
    if ( PartSys->sources[i].source.vy < 0) // moving down, emit sparks
      emitparticles = 1;
    else if ( PartSys->sources[i].source.vy > 0) // moving up means meteor is on 'standby'
      emitparticles = 0;
    else { // speed is zero, explode!
      PartSys->sources[i].source.vy = 10; // set source speed positive so it goes into timeout and launches again
      emitparticles = map(SEGMENT.intensity, 0, 255, 10, hw_random16(PartSys->usedParticles>>2)); // defines the size of the explosion
    }
    for (int e = emitparticles; e > 0; e--) {
        PartSys->sprayEmit(PartSys->sources[i]);
    }
  }

  // update the meteors, set the speed state
  for (i = 0; i < numMeteors; i++) {
    if (PartSys->sources[i].source.ttl) {
      PartSys->sources[i].source.ttl--; // note: this saves an if statement, but moving down particles age twice
      if (PartSys->sources[i].source.vy < 0) { // move down
        PartSys->applyGravity(PartSys->sources[i].source);
        PartSys->particleMoveUpdate(PartSys->sources[i].source, PartSys->sources[i].sourceFlags, &meteorsettings);

        // if source reaches the bottom, set speed to 0 so it will explode on next function call (handled above)
        if (PartSys->sources[i].source.y < PS_P_RADIUS<<1) { // reached the bottom pixel on its way down
          PartSys->sources[i].source.vy = 0; // set speed zero so it will explode
          PartSys->sources[i].source.vx = 0;
          PartSys->sources[i].sourceFlags.collide = true;
          #ifdef ESP8266
          PartSys->sources[i].maxLife = 900;
          PartSys->sources[i].minLife = 100;
          #else
          PartSys->sources[i].maxLife = 1250;
          PartSys->sources[i].minLife = 250;
          #endif
          PartSys->sources[i].source.ttl = hw_random16((768 - (SEGMENT.speed << 1))) + 40; // standby time til next launch (in frames)
          PartSys->sources[i].vy = (SEGMENT.custom1 >> 2);  // emitting speed y
          PartSys->sources[i].var = (SEGMENT.custom1 >> 2); // speed variation around vx,vy (+/- var)
        }
      }
    }
    else if (PartSys->sources[i].source.vy > 0) {  // meteor is exploded and time is up (ttl==0 and positive speed), relaunch it
      // reinitialize meteor
      PartSys->sources[i].source.y = PartSys->maxY + (PS_P_RADIUS << 2); // start 4 pixels above the top
      PartSys->sources[i].source.x = hw_random(PartSys->maxX);
      PartSys->sources[i].source.vy = -hw_random16(30) - 30; // meteor downward speed
      PartSys->sources[i].source.vx = hw_random16(50) - 25; // TODO: make this dependent on position so they do not move out of frame
      PartSys->sources[i].source.hue = hw_random16(); // random color
      PartSys->sources[i].source.ttl = 500; // long life, will explode at bottom
      PartSys->sources[i].sourceFlags.collide = false; // trail particles will not collide
      PartSys->sources[i].maxLife = 300; // spark particle life
      PartSys->sources[i].minLife = 100;
      PartSys->sources[i].vy = -9; // emitting speed (down)
      PartSys->sources[i].var = 3; // speed variation around vx,vy (+/- var)
    }
  }

  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->particles[i].ttl > 5) PartSys->particles[i].ttl -= 5; //ttl is linked to brightness, this allows to use higher brightness but still a short spark lifespan
  }

  PartSys->update(); // update and render
  return FRAMETIME;
}
#undef NUMBEROFSOURCES
static const char _data_FX_MODE_PARTICLEIMPACT[] PROGMEM = "PS Impact@Launches,!,Force,Hardness,Blur,Cylinder,Walls,Collide;;!;2;pal=0,sx=32,ix=85,c1=70,c2=130,c3=0,o3=1";

/*
  Particle Attractor, a particle attractor sits in the matrix center, a spray bounces around and seeds particles
  uses inverse square law like in planetary motion
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleattractor(void) {
  ParticleSystem2D *PartSys = nullptr;
  PSsettings2D sourcesettings;
  sourcesettings.asByte = 0b00001100; // PS settings for bounceY, bounceY used for source movement (it always bounces whereas particles do not)
  PSparticleFlags attractorFlags;
  attractorFlags.asByte = 0; // no flags set
  PSparticle *attractor; // particle pointer to the attractor
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1, sizeof(PSparticle), true)) // init using 1 source and advanced particle settings
      return mode_static(); // allocation failed or not 2D
    PartSys->sources[0].source.hue = hw_random16();
    PartSys->sources[0].source.vx = -7; // will collied with wall and get random bounce direction
    PartSys->sources[0].sourceFlags.collide = true; // seeded particles will collide
    PartSys->sources[0].sourceFlags.perpetual = true; //source does not age
    #ifdef ESP8266
    PartSys->sources[0].maxLife = 200; // lifetime in frames (ESP8266 has less particles)
    PartSys->sources[0].minLife = 30;
    #else
    PartSys->sources[0].maxLife = 350; // lifetime in frames
    PartSys->sources[0].minLife = 50;
    #endif
    PartSys->sources[0].var = 4; // emiting variation
    PartSys->setWallHardness(255);  //bounce forever
    PartSys->setWallRoughness(200); //randomize wall bounce
  }
  else {
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  }

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  attractor = reinterpret_cast<PSparticle *>(PartSys->PSdataEnd);

  PartSys->setColorByAge(SEGMENT.check1);
  PartSys->setParticleSize(SEGMENT.custom1 >> 1); //set size globally
  PartSys->setUsedParticles(map(SEGMENT.intensity, 0, 255, 25, 190));

  if (SEGMENT.custom2 > 0) // collisions enabled
    PartSys->enableParticleCollisions(true, map(SEGMENT.custom2, 1, 255, 120, 255)); // enable collisions and set particle collision hardness
  else
    PartSys->enableParticleCollisions(false);

  if (SEGMENT.call == 0) {
    attractor->vx = PartSys->sources[0].source.vy; // set to spray movemement but reverse x and y
    attractor->vy = PartSys->sources[0].source.vx;
  }

  // set attractor properties
  attractor->ttl = 100; // never dies
  if (SEGMENT.check2) {
    if ((SEGMENT.call % 3) == 0) // move slowly
      PartSys->particleMoveUpdate(*attractor, attractorFlags, &sourcesettings); // move the attractor
  }
  else {
    attractor->x = PartSys->maxX >> 1; // set to center
    attractor->y = PartSys->maxY >> 1;
  }

  if (SEGMENT.call % 5 == 0)
    PartSys->sources[0].source.hue++;

  SEGENV.aux0 += 256; // emitting angle, one full turn in 255 frames (0xFFFF is 360°)
  if (SEGMENT.call % 2 == 0) // alternate direction of emit
    PartSys->angleEmit(PartSys->sources[0], SEGENV.aux0, 12);
  else
    PartSys->angleEmit(PartSys->sources[0], SEGENV.aux0 + 0x7FFF, 12); // emit at 180° as well
  // apply force
  uint32_t strength = SEGMENT.speed;
  #ifdef USERMOD_AUDIOREACTIVE
  um_data_t *um_data;
  if (UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) { // AR active, do not use simulated data
    uint32_t volumeSmth = (uint32_t)(*(float*) um_data->u_data[0]); // 0-255
    strength = (SEGMENT.speed * volumeSmth) >> 8;
  }
  #endif
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    PartSys->pointAttractor(i, *attractor, strength, SEGMENT.check3);
  }


  if (SEGMENT.call % (33 - SEGMENT.custom3) == 0)
    PartSys->applyFriction(2);
  PartSys->particleMoveUpdate(PartSys->sources[0].source, PartSys->sources[0].sourceFlags, &sourcesettings); // move the source
  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEATTRACTOR[] PROGMEM = "PS Attractor@Mass,Particles,Size,Collide,Friction,AgeColor,Move,Swallow;;!;2;pal=9,sx=100,ix=82,c1=2,c2=0";

/*
  Particle Spray, just a particle spray with many parameters
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlespray(void) {
  ParticleSystem2D *PartSys = nullptr;
  const uint8_t hardness = 200; // collision hardness is fixed

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1)) // init, no additional data needed
      return mode_static(); // allocation failed or not 2D
    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return (except on top, taken care of by gravity setting)
    PartSys->setBounceY(true);
    PartSys->setMotionBlur(200); // anable motion blur
    PartSys->setSmearBlur(10); // anable motion blur
    PartSys->sources[0].source.hue = hw_random16();
    PartSys->sources[0].sourceFlags.collide = true; // seeded particles will collide (if enabled)
    PartSys->sources[0].var = 3;
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setBounceX(!SEGMENT.check2);
  PartSys->setWrapX(SEGMENT.check2);
  PartSys->setWallHardness(hardness);
  PartSys->setGravity(8 * SEGMENT.check1); // enable gravity if checked (8 is default strength)
  //numSprays = min(PartSys->numSources, (uint8_t)1); // number of sprays

  if (SEGMENT.check3) // collisions enabled
    PartSys->enableParticleCollisions(true, hardness); // enable collisions and set particle collision hardness
  else
    PartSys->enableParticleCollisions(false);

  //position according to sliders
  PartSys->sources[0].source.x = map(SEGMENT.custom1, 0, 255, 0, PartSys->maxX);
  PartSys->sources[0].source.y = map(SEGMENT.custom2, 0, 255, 0, PartSys->maxY);
  uint16_t angle = (256 - (((int32_t)SEGMENT.custom3 + 1) << 3)) << 8;

  #ifdef USERMOD_AUDIOREACTIVE
  um_data_t *um_data;
  if (UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) { // get AR data, do not use simulated data
    uint32_t volumeSmth  = (uint8_t)(*(float*)   um_data->u_data[0]); //0 to 255
    uint32_t volumeRaw    = *(int16_t*)um_data->u_data[1]; //0 to 255
    PartSys->sources[0].minLife = 30;

    if (SEGMENT.call % 20 == 0 || SEGMENT.call % (11 - volumeSmth / 25) == 0) { // defines interval of particle emit
      PartSys->sources[0].maxLife = (volumeSmth >> 1) + (SEGMENT.intensity >> 1); // lifetime in frames
      PartSys->sources[0].var = 1 + ((volumeRaw * SEGMENT.speed)  >> 12);
      uint32_t emitspeed = (SEGMENT.speed >> 2) + (volumeRaw >> 3);
      PartSys->sources[0].source.hue += volumeSmth/30;
      PartSys->angleEmit(PartSys->sources[0], angle, emitspeed);
    }
  }
  else { //no AR data, fall back to normal mode
    // change source properties
    if (SEGMENT.call % (11 - (SEGMENT.intensity / 25)) == 0) { // every nth frame, cycle color and emit particles
      PartSys->sources[0].maxLife = 300 + SEGMENT.intensity; // lifetime in frames
      PartSys->sources[0].minLife = 150 + SEGMENT.intensity;
      PartSys->sources[0].source.hue++; // = hw_random16(); //change hue of spray source
      PartSys->angleEmit(PartSys->sources[0], angle, SEGMENT.speed >> 2);
    }
  }
  #else
  // change source properties
  if (SEGMENT.call % (11 - (SEGMENT.intensity / 25)) == 0) { // every nth frame, cycle color and emit particles
    PartSys->sources[0].maxLife = 300; // lifetime in frames. note: could be done in init part, but AR moderequires this to be dynamic
    PartSys->sources[0].minLife = 100;
    PartSys->sources[0].source.hue++; // = hw_random16(); //change hue of spray source
    // PartSys->sources[i].var = SEGMENT.custom3; // emiting variation = nozzle size (custom 3 goes from 0-32)
    // spray[j].source.hue = hw_random16(); //set random color for each particle (using palette)
    PartSys->angleEmit(PartSys->sources[0], angle, SEGMENT.speed >> 2);
  }
  #endif

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLESPRAY[] PROGMEM = "PS Spray@Speed,!,Left/Right,Up/Down,Angle,Gravity,Cylinder/Square,Collide;;!;2v;pal=0,sx=150,ix=150,c1=220,c2=30,c3=21";


/*
  Particle base Graphical Equalizer
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleGEQ(void) {
  ParticleSystem2D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1))
      return mode_static(); // allocation failed or not 2D
    PartSys->setKillOutOfBounds(true);
    PartSys->setUsedParticles(170); // use 2/3 of available particles
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  uint32_t i;
  // set particle system properties
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setWrapX(SEGMENT.check1);
  PartSys->setBounceX(SEGMENT.check2);
  PartSys->setBounceY(SEGMENT.check3);
  //PartSys->enableParticleCollisions(false);
  PartSys->setWallHardness(SEGMENT.custom2);
  PartSys->setGravity(SEGMENT.custom3 << 2); // set gravity strength

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t *)um_data->u_data[2]; // 16 bins with FFT data, log mapped already, each band contains frequency amplitude 0-255

  //map the bands into 16 positions on x axis, emit some particles according to frequency loudness
  i = 0;
  uint32_t binwidth = (PartSys->maxX + 1)>>4; //emit poisition variation for one bin (+/-) is equal to width/16 (for 16 bins)
  uint32_t threshold = 300 - SEGMENT.intensity;
  uint32_t emitparticles = 0;

  for (uint32_t bin = 0; bin < 16; bin++) {
    uint32_t xposition = binwidth*bin + (binwidth>>1); // emit position according to frequency band
    uint8_t emitspeed = ((uint32_t)fftResult[bin] * (uint32_t)SEGMENT.speed) >> 9; // emit speed according to loudness of band (127 max!)
    emitparticles = 0;

    if (fftResult[bin] > threshold) {
      emitparticles = 1;// + (fftResult[bin]>>6);
    }
    else if (fftResult[bin] > 0) { // band has low volue
      uint32_t restvolume = ((threshold - fftResult[bin])>>2) + 2;
      if (hw_random16() % restvolume == 0)
        emitparticles = 1;
    }

    while (i < PartSys->usedParticles && emitparticles > 0) { // emit particles if there are any left, low frequencies take priority
      if (PartSys->particles[i].ttl == 0) { // find a dead particle
        //set particle properties TODO: could also use the spray...
        PartSys->particles[i].ttl = 20 + map(SEGMENT.intensity, 0,255, emitspeed>>1, emitspeed + hw_random16(emitspeed)) ; // set particle alive, particle lifespan is in number of frames
        PartSys->particles[i].x = xposition + hw_random16(binwidth) - (binwidth>>1); // position randomly, deviating half a bin width
        PartSys->particles[i].y = PS_P_RADIUS; // start at the bottom (PS_P_RADIUS is minimum position a particle is fully in frame)
        PartSys->particles[i].vx = hw_random16(SEGMENT.custom1>>1)-(SEGMENT.custom1>>2) ; //x-speed variation: +/- custom1/4
        PartSys->particles[i].vy = emitspeed;
        PartSys->particles[i].hue = (bin<<4) + hw_random16(17) - 8; // color from palette according to bin
        emitparticles--;
      }
      i++;
    }
  }

  PartSys->update(); // update and render
  return FRAMETIME;
}

static const char _data_FX_MODE_PARTICLEGEQ[] PROGMEM = "PS GEQ 2D@Speed,Intensity,Diverge,Bounce,Gravity,Cylinder,Walls,Floor;;!;2f;pal=0,sx=155,ix=200,c1=0";

/*
  Particle rotating GEQ
  Particles sprayed from center with rotating spray
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
#define NUMBEROFSOURCES 16
uint16_t mode_particlecenterGEQ(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint8_t numSprays;
  uint32_t i;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, NUMBEROFSOURCES))  // init, request 16 sources
      return mode_static(); // allocation failed or not 2D

    numSprays = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES);
    for (i = 0; i < numSprays; i++) {
      PartSys->sources[i].source.x = (PartSys->maxX + 1) >> 1; // center
      PartSys->sources[i].source.y = (PartSys->maxY + 1) >> 1; // center
      PartSys->sources[i].source.hue = i * 16; // even color distribution
      PartSys->sources[i].maxLife = 400;
      PartSys->sources[i].minLife = 200;
    }
    PartSys->setKillOutOfBounds(true);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  numSprays = min(PartSys->numSources, (uint32_t)NUMBEROFSOURCES);

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t *)um_data->u_data[2]; // 16 bins with FFT data, log mapped already, each band contains frequency amplitude 0-255
  uint32_t threshold = 300 - SEGMENT.intensity;

  if (SEGMENT.check2)
    SEGENV.aux0 += SEGMENT.custom1 << 2;
  else
    SEGENV.aux0 -= SEGMENT.custom1 << 2;

  uint16_t angleoffset = (uint16_t)0xFFFF / (uint16_t)numSprays;
  uint32_t j = hw_random16(numSprays); // start with random spray so all get a chance to emit a particle if maximum number of particles alive is reached.
  for (i = 0; i < numSprays; i++) {
    if (SEGMENT.call % (32 - (SEGMENT.custom2 >> 3)) == 0 && SEGMENT.custom2 > 0)
      PartSys->sources[j].source.hue += 1 + (SEGMENT.custom2 >> 4);

    PartSys->sources[j].var = SEGMENT.custom3 >> 2;
    int8_t emitspeed = 5 + (((uint32_t)fftResult[j] * ((uint32_t)SEGMENT.speed + 20)) >> 10); // emit speed according to loudness of band
    uint16_t emitangle = j * angleoffset + SEGENV.aux0;

    uint32_t emitparticles = 0;
    if (fftResult[j] > threshold)
      emitparticles = 1;
    else if (fftResult[j] > 0) { // band has low value
      uint32_t restvolume = ((threshold - fftResult[j]) >> 2) + 2;
      if (hw_random16() % restvolume == 0)
        emitparticles = 1;
    }
    if (emitparticles)
      PartSys->angleEmit(PartSys->sources[j], emitangle, emitspeed);

    j = (j + 1) % numSprays;
  }
  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLECIRCULARGEQ[] PROGMEM = "PS GEQ Nova@Speed,Intensity,Rotation Speed,Color Change,Nozzle,,Direction;;!;2f;pal=13,ix=180,c1=0,c2=0,c3=8";

/*
  Particle replacement of Ghost Rider by DedeHai (Damian Schneider), original FX by stepko adapted by Blaz Kristan (AKA blazoncek)
*/
#define MAXANGLESTEP 2200 //32767 means 180°
uint16_t mode_particleghostrider(void) {
  ParticleSystem2D *PartSys = nullptr;
  PSsettings2D ghostsettings;
  ghostsettings.asByte = 0b0000011; //enable wrapX and wrapY

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1)) // init, no additional data needed
      return mode_static(); // allocation failed or not 2D
    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return (except on top, taken care of by gravity setting)
    PartSys->sources[0].maxLife = 260; // lifetime in frames
    PartSys->sources[0].minLife = 250;
    PartSys->sources[0].source.x = hw_random16(PartSys->maxX);
    PartSys->sources[0].source.y = hw_random16(PartSys->maxY);
    SEGENV.step = hw_random16(MAXANGLESTEP) - (MAXANGLESTEP>>1); // angle increment
  }
  else {
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  }

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  if (SEGMENT.intensity > 0) { // spiraling
    if (SEGENV.aux1) {
      SEGENV.step += SEGMENT.intensity>>3;
      if ((int32_t)SEGENV.step > MAXANGLESTEP)
        SEGENV.aux1 = 0;
    }
    else {
      SEGENV.step -= SEGMENT.intensity>>3;
      if ((int32_t)SEGENV.step < -MAXANGLESTEP)
        SEGENV.aux1 = 1;
    }
  }
  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(SEGMENT.custom1);
  PartSys->sources[0].var = SEGMENT.custom3 >> 1;

  // color by age (PS 'color by age' always starts with hue = 255, don't want that here)
  if (SEGMENT.check1) {
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
      PartSys->particles[i].hue = PartSys->sources[0].source.hue + (PartSys->particles[i].ttl<<2);
    }
  }

  // enable/disable walls
  ghostsettings.bounceX = SEGMENT.check2;
  ghostsettings.bounceY = SEGMENT.check2;

  SEGENV.aux0 += (int32_t)SEGENV.step; // step is angle increment
  uint16_t emitangle = SEGENV.aux0 + 32767; // +180°
  int32_t speed = map(SEGMENT.speed, 0, 255, 12, 64);
  PartSys->sources[0].source.vx = ((int32_t)cos16_t(SEGENV.aux0) * speed) / (int32_t)32767;
  PartSys->sources[0].source.vy = ((int32_t)sin16_t(SEGENV.aux0) * speed) / (int32_t)32767;
  PartSys->sources[0].source.ttl = 500; // source never dies (note: setting 'perpetual' is not needed if replenished each frame)
  PartSys->particleMoveUpdate(PartSys->sources[0].source, PartSys->sources[0].sourceFlags, &ghostsettings);
  // set head (steal one of the particles)
  PartSys->particles[PartSys->usedParticles-1].x = PartSys->sources[0].source.x;
  PartSys->particles[PartSys->usedParticles-1].y = PartSys->sources[0].source.y;
  PartSys->particles[PartSys->usedParticles-1].ttl = 255;
  PartSys->particles[PartSys->usedParticles-1].sat = 0; //white
  // emit two particles
  PartSys->angleEmit(PartSys->sources[0], emitangle, speed);
  PartSys->angleEmit(PartSys->sources[0], emitangle, speed);
  if (SEGMENT.call % (11 - (SEGMENT.custom2 / 25)) == 0) { // every nth frame, cycle color and emit particles
    PartSys->sources[0].source.hue++;
  }
  if (SEGMENT.custom2 > 190) //fast color change
    PartSys->sources[0].source.hue += (SEGMENT.custom2 - 190) >> 2;

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEGHOSTRIDER[] PROGMEM = "PS Ghost Rider@Speed,Spiral,Blur,Color Cycle,Spread,AgeColor,Walls;;!;2;pal=1,sx=70,ix=0,c1=220,c2=30,c3=21,o1=1";

/*
  PS Blobs: large particles bouncing around, changing size and form
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleblobs(void) {
  ParticleSystem2D *PartSys = nullptr;

  if (SEGMENT.call == 0) {
    if (!initParticleSystem2D(PartSys, 0, 0, true, true)) //init, no additional bytes, advanced size & size control
      return mode_static(); // allocation failed or not 2D
    PartSys->setBounceX(true);
    PartSys->setBounceY(true);
    PartSys->setWallHardness(255);
    PartSys->setWallRoughness(255);
    PartSys->setCollisionHardness(255);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setUsedParticles(map(SEGMENT.intensity, 0, 255, 25, 128)); // minimum 10%, maximum 50% of available particles (note: PS ensures at least 1)
  PartSys->enableParticleCollisions(SEGMENT.check2);

  for (uint32_t i = 0; i < PartSys->usedParticles; i++) { // update particles
    if (SEGENV.aux0 != SEGMENT.speed || PartSys->particles[i].ttl == 0) { // speed changed or dead
      PartSys->particles[i].vx = (int8_t)hw_random16(SEGMENT.speed >> 1) - (SEGMENT.speed >> 2); // +/- speed/4
      PartSys->particles[i].vy = (int8_t)hw_random16(SEGMENT.speed >> 1) - (SEGMENT.speed >> 2);
    }
    if (SEGENV.aux1 != SEGMENT.custom1 || PartSys->particles[i].ttl == 0) // size changed or dead
      PartSys->advPartSize[i].maxsize = 60 + (SEGMENT.custom1 >> 1) + hw_random16((SEGMENT.custom1 >> 2)); // set each particle to slightly randomized size

    //PartSys->particles[i].perpetual = SEGMENT.check2; //infinite life if set
    if (PartSys->particles[i].ttl == 0) { // find dead particle, renitialize
      PartSys->particles[i].ttl = 300 + hw_random16(((uint16_t)SEGMENT.custom2 << 3) + 100);
      PartSys->particles[i].x = hw_random(PartSys->maxX);
      PartSys->particles[i].y = hw_random16(PartSys->maxY);
      PartSys->particles[i].hue = hw_random16(); // set random color
      PartSys->particleFlags[i].collide = true; // enable collision for particle
      PartSys->advPartProps[i].size = 0; // start out small
      PartSys->advPartSize[i].asymmetry = hw_random16(220);
      PartSys->advPartSize[i].asymdir = hw_random16(255);
      // set advanced size control properties
      PartSys->advPartSize[i].grow = true;
      PartSys->advPartSize[i].growspeed = 1 + hw_random16(9);
      PartSys->advPartSize[i].shrinkspeed = 1 + hw_random16(9);
      PartSys->advPartSize[i].wobblespeed = 1 + hw_random16(3);
    }
    //PartSys->advPartSize[i].asymmetry++;
    PartSys->advPartSize[i].pulsate = SEGMENT.check3;
    PartSys->advPartSize[i].wobble = SEGMENT.check1;
  }
  SEGENV.aux0 = SEGMENT.speed; //write state back
  SEGENV.aux1 = SEGMENT.custom1;

  #ifdef USERMOD_AUDIOREACTIVE
  um_data_t *um_data;
  if (UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) { // get AR data, do not use simulated data
    uint8_t volumeSmth = (uint8_t)(*(float*)um_data->u_data[0]);
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) { // update particles
      if (SEGMENT.check3) //pulsate selected
        PartSys->advPartProps[i].size = volumeSmth;
    }
  }
  #endif

  PartSys->setMotionBlur(((SEGMENT.custom3) << 3) + 7);
  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEBLOBS[] PROGMEM = "PS Blobs@Speed,Blobs,Size,Life,Blur,Wobble,Collide,Pulsate;;!;2v;sx=30,ix=64,c1=200,c2=130,c3=0,o3=1";

/*
  Particle Galaxy, particles spiral like in a galaxy
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlegalaxy(void) {
  ParticleSystem2D *PartSys = nullptr;
  PSsettings2D sourcesettings;
  sourcesettings.asByte = 0b00001100; // PS settings for bounceY, bounceY used for source movement (it always bounces whereas particles do not)
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem2D(PartSys, 1, 0, true)) // init using 1 source and advanced particle settings
      return mode_static(); // allocation failed or not 2D
    PartSys->sources[0].source.vx = -4; // will collide with wall and get random bounce direction
    PartSys->sources[0].source.x =  PartSys->maxX >> 1; // start in the center
    PartSys->sources[0].source.y =  PartSys->maxY >> 1;
    PartSys->sources[0].sourceFlags.perpetual = true; //source does not age
    PartSys->sources[0].maxLife = 4000; // lifetime in frames
    PartSys->sources[0].minLife = 800;
    PartSys->sources[0].source.hue = hw_random16(); // start with random color
    PartSys->setWallHardness(255);  //bounce forever
    PartSys->setWallRoughness(200); //randomize wall bounce
  }
  else {
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  }
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!
  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  uint8_t particlesize = SEGMENT.custom1;
  if(SEGMENT.check3)
    particlesize =  SEGMENT.custom1 ? 1 : 0; // set size to 0 (single pixel) or 1 (quad pixel) so motion blur works and adds streaks
  PartSys->setParticleSize(particlesize); // set size globally
  PartSys->setMotionBlur(250 * SEGMENT.check3); // adds trails to single/quad pixel particles, no effect if size > 1

  if ((SEGMENT.call % ((33 - SEGMENT.custom3) >> 1)) == 0) // change hue of emitted particles
    PartSys->sources[0].source.hue+=2;

  if (hw_random8() < (10 + (SEGMENT.intensity >> 1))) // 5%-55% chance to emit a particle in this frame
    PartSys->sprayEmit(PartSys->sources[0]);

  if ((SEGMENT.call & 0x3) == 0) // every 4th frame, move the emitter
    PartSys->particleMoveUpdate(PartSys->sources[0].source, PartSys->sources[0].sourceFlags, &sourcesettings);

  // move alive particles in a spiral motion (or almost straight in fast starfield mode)
  int32_t centerx = PartSys->maxX >> 1; // center of matrix in subpixel coordinates
  int32_t centery = PartSys->maxY >> 1;
  if (SEGMENT.check2) { // starfield mode
    PartSys->setKillOutOfBounds(true);
    PartSys->sources[0].var = 7; // emiting variation
    PartSys->sources[0].source.x =  centerx; // set emitter to center
    PartSys->sources[0].source.y =  centery;
  }
  else {
    PartSys->setKillOutOfBounds(false);
    PartSys->sources[0].var = 1; // emiting variation
  }
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) { //check all particles
    if (PartSys->particles[i].ttl == 0) continue; //skip dead particles
    // (dx/dy): vector pointing from particle to center
    int32_t dx = centerx - PartSys->particles[i].x;
    int32_t dy = centery - PartSys->particles[i].y;
    //speed towards center:
    int32_t distance = sqrt32_bw(dx * dx + dy * dy); // absolute distance to center
    if (distance < 20) distance = 20; // avoid division by zero, keep a minimum
    int32_t speedfactor;
    if (SEGMENT.check2) { // starfield mode
      speedfactor = 1 + (1 + (SEGMENT.speed >> 1)) * distance; // speed increases towards edge
      //apply velocity
      PartSys->particles[i].x += (-speedfactor * dx) / 400000 - (dy >> 6);
      PartSys->particles[i].y += (-speedfactor * dy) / 400000 + (dx >> 6);
    }
    else {
      speedfactor = 2 + (((50 + SEGMENT.speed) << 6) / distance); // speed increases towards center
      // rotate clockwise
      int32_t tempVx = (-speedfactor * dy); // speed is orthogonal to center vector
      int32_t tempVy =  (speedfactor * dx);
      //add speed towards center to make particles spiral in
      int vxc = (dx << 9) / (distance - 19); // subtract value from distance to make the pull-in force a bit stronger (helps on faster speeds)
      int vyc = (dy << 9) / (distance - 19);
      //apply velocity
      PartSys->particles[i].x += (tempVx + vxc) / 1024; // note: cannot use bit shift as that causes asymmetric rounding
      PartSys->particles[i].y += (tempVy + vyc) / 1024;

      if (distance < 128) { // close to center
        if (PartSys->particles[i].ttl > 3)
          PartSys->particles[i].ttl -= 4; //age fast
        PartSys->particles[i].sat = distance << 1; // turn white towards center
      }
    }
    if(SEGMENT.custom3 == 31) // color by age but mapped to 1024 as particles have a long life, since age is random, this gives more or less random colors
      PartSys->particles[i].hue = PartSys->particles[i].ttl >> 2;
    else if(SEGMENT.custom3 == 0) // color by distance
      PartSys->particles[i].hue = map(distance, 20, (PartSys->maxX + PartSys->maxY) >> 2, 0, 180); // color by distance to center
  }

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEGALAXY[] PROGMEM = "PS Galaxy@!,!,Size,,Color,,Starfield,Trace;;!;2;pal=59,sx=80,c1=2,c3=4";

#endif //WLED_DISABLE_PARTICLESYSTEM2D
#endif // WLED_DISABLE_2D

///////////////////////////
// 1D Particle System FX //
///////////////////////////

#ifndef WLED_DISABLE_PARTICLESYSTEM1D
/*
  Particle version of Drip and Rain
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleDrip(void) {
  ParticleSystem1D *PartSys = nullptr;
  //uint8_t numSprays;
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 4)) // init
      return mode_static(); // allocation failed or single pixel
    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return (except on top, taken care of by gravity setting)
    PartSys->sources[0].source.hue = hw_random16();
    SEGENV.aux1 = 0xFFFF; // invalidate
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setBounce(true);
  PartSys->setWallHardness(50);

  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur
  PartSys->setGravity(SEGMENT.custom3 >> 1); // set gravity (8 is default strength)
  PartSys->setParticleSize(SEGMENT.check3); // 1 or 2 pixel rendering

  if (SEGMENT.check2) { //collisions enabled
    PartSys->enableParticleCollisions(true); //enable, full hardness
  }
  else
    PartSys->enableParticleCollisions(false);

  PartSys->sources[0].sourceFlags.collide = false; //drops do not collide

  if (SEGMENT.check1) { //rain mode, emit at random position, short life (3-8 seconds at 50fps)
    if (SEGMENT.custom1 == 0) //splash disabled, do not bounce raindrops
      PartSys->setBounce(false);
    PartSys->sources[0].var = 5;
    PartSys->sources[0].v = -(8 + (SEGMENT.speed >> 2)); //speed + var must be < 128, inverted speed (=down)
    // lifetime in frames
    PartSys->sources[0].minLife = 30;
    PartSys->sources[0].maxLife = 200;
    PartSys->sources[0].source.x = hw_random(PartSys->maxX); //random emit position
  }
  else { //drip
    PartSys->sources[0].var = 0;
    PartSys->sources[0].v = -(SEGMENT.speed >> 1); //speed + var must be < 128, inverted speed (=down)
    PartSys->sources[0].minLife = 3000;
    PartSys->sources[0].maxLife = 3000;
    PartSys->sources[0].source.x = PartSys->maxX - PS_P_RADIUS_1D;
  }

  if (SEGENV.aux1 != SEGMENT.intensity) //slider changed
    SEGENV.aux0 = 1; //must not be zero or "% 0" happens below which crashes on ESP32

  SEGENV.aux1 = SEGMENT.intensity; // save state

  // every nth frame emit a particle
  if (SEGMENT.call % SEGENV.aux0 == 0) {
    int32_t interval = 300 / ((SEGMENT.intensity) + 1);
    SEGENV.aux0 = interval + hw_random(interval + 5);
    // if (SEGMENT.check1) // rain mode
    //   PartSys->sources[0].source.hue = 0;
    // else
    PartSys->sources[0].source.hue = hw_random8(); //set random color  TODO: maybe also not random but color cycling? need another slider or checkmark for this.
    PartSys->sprayEmit(PartSys->sources[0]);
  }

  for (uint32_t i = 0; i < PartSys->usedParticles; i++) { //check all particles
    if (PartSys->particles[i].ttl && PartSys->particleFlags[i].collide == false) { // use collision flag to identify splash particles
      if (SEGMENT.custom1 > 0 && PartSys->particles[i].x < (PS_P_RADIUS_1D << 1)) { //splash enabled and reached bottom
        PartSys->particles[i].ttl = 0; //kill origin particle
        PartSys->sources[0].maxLife = 80;
        PartSys->sources[0].minLife = 20;
        PartSys->sources[0].var = 10 + (SEGMENT.custom1 >> 3);
        PartSys->sources[0].v = 0;
        PartSys->sources[0].source.hue = PartSys->particles[i].hue;
        PartSys->sources[0].source.x = PS_P_RADIUS_1D;
        PartSys->sources[0].sourceFlags.collide = true; //splashes do collide if enabled
        for (int j = 0; j < 2 + (SEGMENT.custom1 >> 2); j++) {
          PartSys->sprayEmit(PartSys->sources[0]);
        }
      }
    }

    if (SEGMENT.check1) { //rain mode, fade hue to max
      if (PartSys->particles[i].hue < 245)
        PartSys->particles[i].hue += 8;
    }
    //increase speed on high settings by calling the move function twice
    if (SEGMENT.speed > 200)
      PartSys->particleMoveUpdate(PartSys->particles[i], PartSys->particleFlags[i]);
  }

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEDRIP[] PROGMEM = "PS DripDrop@Speed,!,Splash,Blur,Gravity,Rain,PushSplash,Smooth;,!;!;1;pal=0,sx=150,ix=25,c1=220,c2=30,c3=21";


/*
  Particle Replacement for "Bbouncing Balls by Aircoookie"
  Also replaces rolling balls and juggle (and maybe popcorn)
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particlePinball(void) {
  ParticleSystem1D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 128, 0, true)) // init
      return mode_static(); // allocation failed or is single pixel
    PartSys->sources[0].sourceFlags.collide = true; // seeded particles will collide (if enabled)
    PartSys->sources[0].source.x = PS_P_RADIUS_1D; //emit at bottom
    PartSys->setKillOutOfBounds(true); // out of bounds particles dont return
    SEGENV.aux0 = 1;
    SEGENV.aux1 = 5000; //set out of range to ensure uptate on first call
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  //uint32_t hardness = 240 + (SEGMENT.custom1>>4);
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setGravity(map(SEGMENT.custom3, 0 , 31, 0 , 16)); // set gravity (8 is default strength)
  PartSys->setBounce(SEGMENT.custom3); // disables bounce if no gravity is used
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur
  PartSys->enableParticleCollisions(SEGMENT.check1, 255); // enable collisions and set particle collision to high hardness
  PartSys->setUsedParticles(SEGMENT.intensity);
  PartSys->setColorByPosition(SEGMENT.check3);

  bool updateballs = false;
  if (SEGENV.aux1 != SEGMENT.speed + SEGMENT.intensity + SEGMENT.check2 + SEGMENT.custom1 + PartSys->usedParticles) { // user settings change or more particles are available
    SEGENV.step = SEGMENT.call; // reset delay
    updateballs = true;
    PartSys->sources[0].maxLife = SEGMENT.custom3 ? 5000 : 0xFFFF; // maximum lifetime in frames/2 (very long if not using gravity, this is enough to travel 4000 pixels at min speed)
    PartSys->sources[0].minLife = PartSys->sources[0].maxLife >> 1;
  }

  if (SEGMENT.check2) { //rolling balls
    PartSys->setGravity(0);
    PartSys->setWallHardness(255);
    int speedsum = 0;
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
        PartSys->particles[i].ttl = 260; // keep particles alive
      if (updateballs) { //speed changed or particle is dead, set particle properties
        PartSys->particleFlags[i].collide = true;
        if (PartSys->particles[i].x == 0) { // still at initial position (when not switching from a PS)
          PartSys->particles[i].x = hw_random16(PartSys->maxX); // random initial position for all particles
          PartSys->particles[i].vx = (hw_random16() & 0x01) ? 1 : -1; // random initial direction
        }
        PartSys->particles[i].hue = hw_random8(); //set ball colors to random
        PartSys->advPartProps[i].sat = 255;
        PartSys->advPartProps[i].size = SEGMENT.custom1;
      }
      speedsum += abs(PartSys->particles[i].vx);
    }
    int32_t avgSpeed = speedsum / PartSys->usedParticles;
    int32_t setSpeed = 2 + (SEGMENT.speed >> 3);
    if (avgSpeed < setSpeed) { // if balls are slow, speed up some of them at random to keep the animation going
      for (int i = 0; i < setSpeed - avgSpeed; i++) {
        int idx = hw_random16(PartSys->usedParticles);
        PartSys->particles[idx].vx += PartSys->particles[idx].vx >= 0 ? 1 : -1; // add 1, keep direction
      }
    }
    else if (avgSpeed > setSpeed + 8) // if avg speed is too high, apply friction to slow them down
      PartSys->applyFriction(1);
  }
  else { //bouncing balls
    PartSys->setWallHardness(220);
    PartSys->sources[0].var = SEGMENT.speed >> 3;
    int32_t newspeed = 2 + (SEGMENT.speed >> 1) - (SEGMENT.speed >> 3);
    PartSys->sources[0].v = newspeed;
    //check for balls that are 'laying on the ground' and remove them
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
      if (PartSys->particles[i].vx == 0 && PartSys->particles[i].x < (PS_P_RADIUS_1D + SEGMENT.custom1))
        PartSys->particles[i].ttl = 0;
      if (updateballs) {
        PartSys->advPartProps[i].size = SEGMENT.custom1;
        if (SEGMENT.custom3 == 0) //gravity off, update speed
          PartSys->particles[i].vx = PartSys->particles[i].vx > 0 ? newspeed : -newspeed; //keep the direction
      }
    }

    // every nth frame emit a ball
    if (SEGMENT.call > SEGENV.step) {
      int interval = 260 - ((int)SEGMENT.intensity);
      SEGENV.step += interval + hw_random16(interval);
      PartSys->sources[0].source.hue = hw_random16(); //set ball color
      PartSys->sources[0].sat = 255;
      PartSys->sources[0].size = SEGMENT.custom1;
      PartSys->sprayEmit(PartSys->sources[0]);
    }
  }
  SEGENV.aux1 = SEGMENT.speed + SEGMENT.intensity + SEGMENT.check2 + SEGMENT.custom1 + PartSys->usedParticles;
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    PartSys->particleMoveUpdate(PartSys->particles[i], PartSys->particleFlags[i]); // double the speed
  }

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PSPINBALL[] PROGMEM = "PS Pinball@Speed,!,Size,Blur,Gravity,Collide,Rolling,Position Color;,!;!;1;pal=0,ix=220,c2=0,c3=8,o1=1";

/*
  Particle Replacement for original Dancing Shadows:
  "Spotlights moving back and forth that cast dancing shadows.
  Shine this through tree branches/leaves or other close-up objects that cast
  interesting shadows onto a ceiling or tarp.
  By Steve Pomeroy @xxv"
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleDancingShadows(void) {
  ParticleSystem1D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1)) // init, one source
      return mode_static(); // allocation failed or is single pixel
    PartSys->sources[0].maxLife = 1000; //set long life (kill out of bounds is done in custom way)
    PartSys->sources[0].minLife = PartSys->sources[0].maxLife;
  }
  else {
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  }

  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(SEGMENT.custom1);
  if (SEGMENT.check1)
    PartSys->setSmearBlur(120); // enable smear blur
  else
    PartSys->setSmearBlur(0); // disable smear blur
  PartSys->setParticleSize(SEGMENT.check3); // 1 or 2 pixel rendering
  PartSys->setColorByPosition(SEGMENT.check2); // color fixed by position
  PartSys->setUsedParticles(map(SEGMENT.intensity, 0, 255, 10, 255)); // set percentage of particles to use

  uint32_t deadparticles = 0;
  //kill out of bounds and moving away plus change color
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (((SEGMENT.call & 0x07) == 0) && PartSys->particleFlags[i].outofbounds) { //check if out of bounds particle move away from strip, only update every 8th frame
      if ((int32_t)PartSys->particles[i].vx * PartSys->particles[i].x > 0) PartSys->particles[i].ttl = 0; //particle is moving away, kill it
    }
    PartSys->particleFlags[i].perpetual = true; //particles do not age
    if (SEGMENT.call % (32 / (1 + (SEGMENT.custom2 >> 3))) == 0)
       PartSys->particles[i].hue += 2 + (SEGMENT.custom2 >> 5);
    //note: updating speed on the fly is not accurately possible, since it is unknown which particles are assigned to which spot
    if (SEGENV.aux0 != SEGMENT.speed) { //speed changed
      //update all particle speed by setting them to current value
       PartSys->particles[i].vx = PartSys->particles[i].vx > 0 ? SEGMENT.speed >> 3 : -SEGMENT.speed >> 3;
    }
    if (PartSys->particles[i].ttl == 0) deadparticles++; // count dead particles
  }
  SEGENV.aux0 = SEGMENT.speed;

  //generate a spotlight: generates particles just outside of view
  if (deadparticles > 5 && (SEGMENT.call & 0x03) == 0) {
    //random color, random type
    uint32_t type = hw_random16(SPOT_TYPES_COUNT);
    int8_t speed = 2 + hw_random16(2 + (SEGMENT.speed >> 1)) + (SEGMENT.speed >> 4);
    int32_t width = hw_random16(1, 10);
    uint32_t ttl = 300; //ttl is particle brightness (below perpetual is set so it does not age, i.e. ttl stays at this value)
    int32_t position;
    //choose random start position, left and right from the segment
    if (hw_random() & 0x01) {
      position = PartSys->maxXpixel;
      speed = -speed;
    }
    else
      position = -width;

    PartSys->sources[0].v = speed; //emitted particle speed
    PartSys->sources[0].source.hue = hw_random8(); //random spotlight color
    for (int32_t i = 0; i < width; i++) {
      if (width > 1) {
        switch (type) {
          case SPOT_TYPE_SOLID:
            //nothing to do
            break;

          case SPOT_TYPE_GRADIENT:
            ttl = cubicwave8(map(i, 0, width - 1, 0, 255));
            ttl = ttl*ttl >> 8; //make gradient more pronounced
            break;

          case SPOT_TYPE_2X_GRADIENT:
            ttl = cubicwave8(2 * map(i, 0, width - 1, 0, 255));
            ttl = ttl*ttl >> 8;
            break;

          case SPOT_TYPE_2X_DOT:
            if (i > 0) position++; //skip one pixel
            i++;
            break;

          case SPOT_TYPE_3X_DOT:
            if (i > 0) position += 2; //skip two pixels
            i+=2;
            break;

          case SPOT_TYPE_4X_DOT:
            if (i > 0) position += 3; //skip three pixels
            i+=3;
            break;
        }
      }
      //emit particle
      //set the particle source position:
      PartSys->sources[0].source.x = position * PS_P_RADIUS_1D;
      uint32_t partidx = PartSys->sprayEmit(PartSys->sources[0]);
      PartSys->particles[partidx].ttl = ttl;
      position++; //do the next pixel
    }
  }

  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PARTICLEDANCINGSHADOWS[] PROGMEM = "PS Dancing Shadows@Speed,!,Blur,Color Cycle,,Smear,Position Color,Smooth;,!;!;1;sx=100,ix=180,c1=0,c2=0";

/*
  Particle Fireworks 1D replacement
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleFireworks1D(void) {
  ParticleSystem1D *PartSys = nullptr;
  uint8_t *forcecounter;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 4, 150, 4, true)) // init advanced particle system
    if (!initParticleSystem1D(PartSys, 4, 150, 4, true)) // init advanced particle system
      return mode_static(); // allocation failed or is single pixel
    PartSys->setKillOutOfBounds(true);
    PartSys->sources[0].sourceFlags.custom1 = 1; // set rocket state to standby
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  forcecounter = PartSys->PSdataEnd;
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur
  int32_t gravity = (1 + (SEGMENT.speed >> 3)); // gravity value used for rocket speed calculation
  PartSys->setGravity(SEGMENT.speed ? gravity : 0); // set gravity

  if (PartSys->sources[0].sourceFlags.custom1 == 1) { // rocket is on standby
    PartSys->sources[0].source.ttl--;
    if (PartSys->sources[0].source.ttl == 0) { // time is up, relaunch

      if (hw_random8() < SEGMENT.custom1) // randomly choose direction according to slider, fire at start of segment if true
        SEGENV.aux0 = 1;
      else
        SEGENV.aux0 = 0;

      PartSys->sources[0].sourceFlags.custom1 = 0; //flag used for rocket state
      PartSys->sources[0].source.hue = hw_random16(); // different color for each launch
      PartSys->sources[0].var = 10; // emit variation
      PartSys->sources[0].v = -10; // emit speed
      PartSys->sources[0].minLife = 30;
      PartSys->sources[0].maxLife = SEGMENT.check2 ? 400 : 60;
      PartSys->sources[0].source.x = 0; // start from bottom
      uint32_t speed = sqrt((gravity * ((PartSys->maxX >> 2) + hw_random16(PartSys->maxX >> 1))) >> 4); // set speed such that rocket explods in frame
      PartSys->sources[0].source.vx = min(speed, (uint32_t)127);
      PartSys->sources[0].source.ttl = 4000;
      PartSys->sources[0].sat = 30; // low saturation exhaust
      PartSys->sources[0].size = SEGMENT.check3; // single or double pixel rendering
      PartSys->sources[0].sourceFlags.reversegrav = false ; // normal gravity

      if (SEGENV.aux0) { // inverted rockets launch from end
        PartSys->sources[0].sourceFlags.reversegrav = true;
        PartSys->sources[0].source.x = PartSys->maxX; // start from top
        PartSys->sources[0].source.vx = -PartSys->sources[0].source.vx; // revert direction
        PartSys->sources[0].v = -PartSys->sources[0].v; // invert exhaust emit speed
      }
    }
  }
  else { // rocket is launched
    int32_t rocketgravity = -gravity;
    int32_t currentspeed = PartSys->sources[0].source.vx;
    if (SEGENV.aux0) { // negative speed rocket
      rocketgravity = -rocketgravity;
      currentspeed = -currentspeed;
    }
    PartSys->applyForce(PartSys->sources[0].source, rocketgravity, forcecounter[0]);
    PartSys->particleMoveUpdate(PartSys->sources[0].source, PartSys->sources[0].sourceFlags);
    PartSys->particleMoveUpdate(PartSys->sources[0].source, PartSys->sources[0].sourceFlags); // increase rocket speed by calling the move function twice, also ages twice
    uint32_t rocketheight = SEGENV.aux0 ? PartSys->maxX - PartSys->sources[0].source.x : PartSys->sources[0].source.x;

    if (currentspeed < 0 && PartSys->sources[0].source.ttl > 50) // reached apogee
      PartSys->sources[0].source.ttl = min((uint32_t)50, rocketheight >> (PS_P_RADIUS_SHIFT_1D + 3)); // alive for a few more frames

    if (PartSys->sources[0].source.ttl < 2) { // explode
      PartSys->sources[0].sourceFlags.custom1 = 1; // set standby state
      PartSys->sources[0].var = 5 + ((((PartSys->maxX >> 1) + rocketheight) * (200 + SEGMENT.intensity)) / (PartSys->maxX << 2)); // set explosion particle speed
      PartSys->sources[0].minLife = 600;
      PartSys->sources[0].maxLife = 1300;
      PartSys->sources[0].source.ttl = 100 + hw_random16(64 - (SEGMENT.speed >> 2)); // standby time til next launch
      PartSys->sources[0].sat = SEGMENT.custom3 < 16 ? 10 + (SEGMENT.custom3 << 4) : 255; //color saturation
      PartSys->sources[0].size = SEGMENT.check3 ? hw_random16(SEGMENT.intensity) : 0; // random particle size in explosion
      uint32_t explosionsize = 8 + (PartSys->maxXpixel >> 2) + (PartSys->sources[0].source.x >> (PS_P_RADIUS_SHIFT_1D - 1));
      explosionsize += hw_random16((explosionsize * SEGMENT.intensity) >> 8);
      for (uint32_t e = 0; e < explosionsize; e++) { // emit explosion particles
        int idx = PartSys->sprayEmit(PartSys->sources[0]); // emit a particle
        if(SEGMENT.custom3 > 23) {
          if(SEGMENT.custom3 == 31) { // highest slider value
            PartSys->setColorByAge(SEGMENT.check1); // color by age if colorful mode is enabled
            PartSys->setColorByPosition(!SEGMENT.check1); // color by position otherwise
          }
          else { // if custom3 is set to high value (but not highest), set particle color by initial speed
            PartSys->particles[idx].hue = map(abs(PartSys->particles[idx].vx), 0, PartSys->sources[0].var, 0, 16 + hw_random16(200)); // set hue according to speed, use random amount of palette width
            PartSys->particles[idx].hue += PartSys->sources[0].source.hue; // add hue offset of the rocket (random starting color)
          }
        }
        else {
          if (SEGMENT.check1) // colorful mode
            PartSys->sources[0].source.hue = hw_random16(); //random color for each particle
        }
      }
    }
  }
  if ((SEGMENT.call & 0x01) == 0 && PartSys->sources[0].sourceFlags.custom1 == false && PartSys->sources[0].source.ttl > 50) // every second frame and not in standby and not about to explode
    PartSys->sprayEmit(PartSys->sources[0]); // emit exhaust particle

  if ((SEGMENT.call & 0x03) == 0) // every fourth frame
    PartSys->applyFriction(1); // apply friction to all particles

  PartSys->update(); // update and render

  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->particles[i].ttl > 10) PartSys->particles[i].ttl -= 10; //ttl is linked to brightness, this allows to use higher brightness but still a short spark lifespan
    else PartSys->particles[i].ttl = 0;
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_PS_FIREWORKS1D[] PROGMEM = "PS Fireworks 1D@Gravity,Explosion,Firing side,Blur,Color,Colorful,Trail,Smooth;,!;!;1;c2=30,o1=1";

/*
  Particle based Sparkle effect
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleSparkler(void) {
  ParticleSystem1D *PartSys = nullptr;
  uint32_t numSparklers;
  PSsettings1D sparklersettings;
  sparklersettings.asByte = 0; // PS settings for sparkler (set below)

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 16, 128 ,0, true)) // init, no additional data needed
      return mode_static(); // allocation failed or is single pixel
  } else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)

  sparklersettings.wrap = !SEGMENT.check2;
  sparklersettings.bounce = SEGMENT.check2; // note: bounce always takes priority over wrap

  numSparklers = PartSys->numSources;
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur/overlay
  //PartSys->setSmearBlur(SEGMENT.custom2); // anable smearing blur

  for (uint32_t i = 0; i < numSparklers; i++) {
    PartSys->sources[i].source.hue = hw_random16();
    PartSys->sources[i].var = 0; // sparks stationary
    PartSys->sources[i].minLife = 150 + SEGMENT.intensity;
    PartSys->sources[i].maxLife = 250 + (SEGMENT.intensity << 1);
    int32_t speed = SEGMENT.speed >> 1;
    if (SEGMENT.check1) // sparks move (slide option)
      PartSys->sources[i].var = SEGMENT.intensity >> 3;
    PartSys->sources[i].source.vx = PartSys->sources[i].source.vx > 0 ? speed : -speed; // update speed, do not change direction
    PartSys->sources[i].source.ttl = 400; // replenish its life (setting it perpetual uses more code)
    PartSys->sources[i].sat = SEGMENT.custom1; // color saturation
    PartSys->sources[i].size = SEGMENT.check3 ? 120 : 0;
    if (SEGMENT.speed == 255) // random position at highest speed setting
      PartSys->sources[i].source.x = hw_random16(PartSys->maxX);
    else
      PartSys->particleMoveUpdate(PartSys->sources[i].source, PartSys->sources[i].sourceFlags, &sparklersettings); //move sparkler
  }

  numSparklers = min(1 + (SEGMENT.custom3 >> 1), (int)numSparklers);  // set used sparklers, 1 to 16

  if (SEGENV.aux0 != SEGMENT.custom3) { //number of used sparklers changed, redistribute
    for (uint32_t i = 1; i < numSparklers; i++) {
      PartSys->sources[i].source.x = (PartSys->sources[0].source.x + (PartSys->maxX / numSparklers) * i ) % PartSys->maxX; //distribute evenly
    }
  }
  SEGENV.aux0 = SEGMENT.custom3;

  for (uint32_t i = 0; i < numSparklers; i++) {
    if (hw_random()  % (((271 - SEGMENT.intensity) >> 4)) == 0)
      PartSys->sprayEmit(PartSys->sources[i]); //emit a particle
  }

  PartSys->update(); // update and render

  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->particles[i].ttl > (64 - (SEGMENT.intensity >> 2))) PartSys->particles[i].ttl -= (64 - (SEGMENT.intensity >> 2)); //ttl is linked to brightness, this allows to use higher brightness but still a short spark lifespan
    else PartSys->particles[i].ttl = 0;
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_PS_SPARKLER[] PROGMEM = "PS Sparkler@Move,!,Saturation,Blur,Sparklers,Slide,Bounce,Large;,!;!;1;pal=0,sx=255,c1=0,c2=0,c3=6";

/*
  Particle based Hourglass, particles falling at defined intervals
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleHourglass(void) {
  ParticleSystem1D *PartSys = nullptr;
  constexpr int positionOffset = PS_P_RADIUS_1D / 2;; // resting position offset
  bool* direction;
  uint32_t* settingTracker;
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 0, 255, 8, false)) // init
      return mode_static(); // allocation failed or is single pixel
    PartSys->setBounce(true);
    PartSys->setWallHardness(100);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  settingTracker = reinterpret_cast<uint32_t *>(PartSys->PSdataEnd);  //assign data pointer
  direction = reinterpret_cast<bool *>(PartSys->PSdataEnd + 4);  //assign data pointer
  PartSys->setUsedParticles(1 + ((SEGMENT.intensity * 255) >> 8));
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur
  PartSys->setGravity(map(SEGMENT.custom3, 0, 31, 1, 30));
  PartSys->enableParticleCollisions(true, 32); // hardness value found by experimentation on different settings

  uint32_t colormode = SEGMENT.custom1 >> 5; // 0-7

  if (SEGMENT.intensity != *settingTracker) { // initialize
    *settingTracker = SEGMENT.intensity;
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
      PartSys->particleFlags[i].reversegrav = true; // resting particles dont fall
      *direction = 0; // down
      SEGENV.aux1 = 1; // initialize below
    }
    SEGENV.aux0 = PartSys->usedParticles - 1; // initial state, start with highest number particle
  }

  // calculate target position depending on direction
  auto calcTargetPos = [&](size_t i) {
    return PartSys->particleFlags[i].reversegrav ?
          PartSys->maxX - i * PS_P_RADIUS_1D - positionOffset
        : (PartSys->usedParticles - i) * PS_P_RADIUS_1D - positionOffset;
  };


  for (uint32_t i = 0; i < PartSys->usedParticles; i++) { // check if particle reached target position after falling
    if (PartSys->particleFlags[i].fixed == false && abs(PartSys->particles[i].vx) < 5) {
      int32_t targetposition = calcTargetPos(i);
      bool closeToTarget = abs(targetposition - PartSys->particles[i].x) < 3 * PS_P_RADIUS_1D;
      if (closeToTarget) { // close to target and slow speed
        PartSys->particles[i].x = targetposition; // set exact position
        PartSys->particleFlags[i].fixed = true;   // pin particle
      }
    }
    if (colormode == 7)
      PartSys->setColorByPosition(true); // color fixed by position
    else {
      PartSys->setColorByPosition(false);
      uint8_t basehue = ((SEGMENT.custom1 & 0x1F) << 3); // use 5 LSBs to select color
      switch(colormode) {
        case 0: PartSys->particles[i].hue = 120; break; // fixed at 120, if flip is activated, this can make red and green (use palette 34)
        case 1: PartSys->particles[i].hue = basehue; break; // fixed selectable color
        case 2: // 2 colors inverleaved (same code as 3)
        case 3: PartSys->particles[i].hue = ((SEGMENT.custom1 & 0x1F) << 1) + (i % colormode)*74; break; // interleved colors (every 2 or 3 particles)
        case 4: PartSys->particles[i].hue = basehue + (i * 255) / PartSys->usedParticles;  break; // gradient palette colors
        case 5: PartSys->particles[i].hue = basehue + (i * 1024) / PartSys->usedParticles;  break; // multi gradient palette colors
        case 6: PartSys->particles[i].hue = i + (strip.now >> 3);  break; // disco! moving color gradient
        default: break;
      }
    }
    if (SEGMENT.check1 && !PartSys->particleFlags[i].reversegrav) // flip color when fallen
      PartSys->particles[i].hue += 120;
  }

  // re-order particles in case collisions flipped particles (highest number index particle is on the "bottom")
  for (uint32_t i = 0; i < PartSys->usedParticles - 1; i++) {
    if (PartSys->particles[i].x < PartSys->particles[i+1].x && PartSys->particleFlags[i].fixed == false && PartSys->particleFlags[i+1].fixed == false) {
      std::swap(PartSys->particles[i].x, PartSys->particles[i+1].x);
    }
  }


  if (SEGENV.aux1 == 1) { // last countdown call before dropping starts, reset all particles
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
      PartSys->particleFlags[i].collide = true;
      PartSys->particleFlags[i].perpetual = true;
      PartSys->particles[i].ttl = 260;
      PartSys->particles[i].x = calcTargetPos(i);
      PartSys->particleFlags[i].fixed = true;
    }
  }

  if (SEGENV.aux1 == 0) { // countdown passed, run
    if (strip.now >= SEGENV.step) { // drop a particle, do not drop more often than every second frame or particles tangle up quite badly
      // set next drop time
      if (SEGMENT.check3 && *direction) // fast reset
        SEGENV.step = strip.now + 100; // drop one particle every 100ms
      else // normal interval
        SEGENV.step = strip.now + max(20, SEGMENT.speed * 20); // map speed slider from 0.1s to 5s
      if (SEGENV.aux0 < PartSys->usedParticles) {
        PartSys->particleFlags[SEGENV.aux0].reversegrav = *direction; // let this particle fall or rise
        PartSys->particleFlags[SEGENV.aux0].fixed = false; // unpin
      }
      else { // overflow
        *direction = !(*direction); // flip direction
        SEGENV.aux1 = SEGMENT.virtualLength() + 100; // set countdown
      }
      if (*direction == 0) // down, start dropping the highest number particle
        SEGENV.aux0--; // next particle
      else
        SEGENV.aux0++;
    }
  }
  else if (SEGMENT.check2) // auto reset
    SEGENV.aux1--; // countdown

  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PS_HOURGLASS[] PROGMEM = "PS Hourglass@Interval,!,Color,Blur,Gravity,Colorflip,Start,Fast Reset;,!;!;1;pal=34,sx=50,ix=200,c1=140,c2=80,c3=4,o1=1,o2=1,o3=1";

/*
  Particle based Spray effect (like a volcano, possible replacement for popcorn)
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particle1Dspray(void) {
  ParticleSystem1D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1))
      return mode_static(); // allocation failed or is single pixel
    PartSys->setKillOutOfBounds(true);
    PartSys->setWallHardness(150);
    PartSys->setParticleSize(1);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setBounce(SEGMENT.check2);
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur
  int32_t gravity = -((int32_t)SEGMENT.custom3 - 16);  // gravity setting, 0-15 is positive (down), 17 - 31 is negative (up)
  PartSys->setGravity(abs(gravity)); // use reversgrav setting to invert gravity (for proper 'floor' and out of bounce handling)

  PartSys->sources[0].source.hue = SEGMENT.aux0; // hw_random16();
  PartSys->sources[0].var = 20;
  PartSys->sources[0].minLife = 200;
  PartSys->sources[0].maxLife = 400;
  PartSys->sources[0].source.x = map(SEGMENT.custom1, 0 , 255, 0, PartSys->maxX); // spray position
  PartSys->sources[0].v = map(SEGMENT.speed, 0 , 255, -127 + PartSys->sources[0].var, 127 - PartSys->sources[0].var); // particle emit speed
  PartSys->sources[0].sourceFlags.reversegrav = gravity < 0 ? true : false;

  if (hw_random()  % (1 + ((255 - SEGMENT.intensity) >> 3)) == 0) {
    PartSys->sprayEmit(PartSys->sources[0]); // emit a particle
    SEGMENT.aux0++; // increment hue
  }

  //update color settings
  PartSys->setColorByAge(SEGMENT.check1); // overruled by 'color by position'
  PartSys->setColorByPosition(SEGMENT.check3);
  for (uint i = 0; i < PartSys->usedParticles; i++) {
    PartSys->particleFlags[i].reversegrav = PartSys->sources[0].sourceFlags.reversegrav; // update gravity direction
  }
  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PS_1DSPRAY[] PROGMEM = "PS Spray 1D@Speed(+/-),!,Position,Blur,Gravity(+/-),AgeColor,Bounce,Position Color;,!;!;1;sx=200,ix=220,c1=0,c2=0";

/*
  Particle based balance: particles move back and forth (1D pendent to 2D particle box)
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleBalance(void) {
  ParticleSystem1D *PartSys = nullptr;
  uint32_t i;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 128)) // init, no additional data needed, use half of max particles
      return mode_static(); // allocation failed or is single pixel
    PartSys->setParticleSize(1);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(SEGMENT.custom2); // enable motion blur
  PartSys->setBounce(!SEGMENT.check2);
  PartSys->setWrap(SEGMENT.check2);
  uint8_t hardness = SEGMENT.custom1 > 0 ? map(SEGMENT.custom1, 0, 255, 50, 250) : 200; // set hardness,  make the walls hard if collisions are disabled
  PartSys->enableParticleCollisions(SEGMENT.custom1, hardness); // enable collisions if custom1 > 0
  PartSys->setWallHardness(200);
  PartSys->setUsedParticles(map(SEGMENT.intensity, 0, 255, 10, 255));
  if (PartSys->usedParticles > SEGENV.aux1) { // more particles, reinitialize
    for (i = 0; i < PartSys->usedParticles; i++) {
      PartSys->particles[i].x = i * PS_P_RADIUS_1D;
      PartSys->particles[i].ttl = 300;
      PartSys->particleFlags[i].perpetual = true;
      PartSys->particleFlags[i].collide = true;
    }
  }
  SEGENV.aux1 = PartSys->usedParticles;

  // re-order particles in case collisions flipped particles
  for (i = 0; i < PartSys->usedParticles - 1; i++) {
    if (PartSys->particles[i].x > PartSys->particles[i+1].x) {
      if (SEGMENT.check2) { // check for wrap around
        if (PartSys->particles[i].x - PartSys->particles[i+1].x > 3 * PS_P_RADIUS_1D)
          continue;
      }
      std::swap(PartSys->particles[i].x, PartSys->particles[i+1].x);
    }
  }

  if (SEGMENT.call % (((255 - SEGMENT.speed) >> 6) + 1) == 0) { // how often the force is applied depends on speed setting
    int32_t xgravity;
    int32_t increment = (SEGMENT.speed >> 6) + 1;
    SEGENV.aux0 += increment;
    if (SEGMENT.check3) // random, use perlin noise
      xgravity = ((int16_t)perlin8(SEGENV.aux0) - 128);
    else // sinusoidal
      xgravity = (int16_t)cos8(SEGENV.aux0) - 128;//((int32_t)(SEGMENT.custom3 << 2) * cos8(SEGENV.aux0)
    // scale the force
    xgravity = (xgravity * ((SEGMENT.custom3+1) << 2)) / 128; // xgravity: -127 to +127
    PartSys->applyForce(xgravity);
  }

  uint32_t randomindex = hw_random16(PartSys->usedParticles);
  PartSys->particles[randomindex].vx = ((int32_t)PartSys->particles[randomindex].vx * 200) / 255;  // apply friction to random particle to reduce clumping (without collisions)

  //if (SEGMENT.check2 && (SEGMENT.call & 0x07) == 0) // no walls, apply friction to smooth things out
  if ((SEGMENT.call & 0x0F) == 0 && SEGMENT.custom3 > 4) // apply friction every 16th frame to smooth things out (except for low tilt)
    PartSys->applyFriction(1); // apply friction to all particles

  //update colors
  PartSys->setColorByPosition(SEGMENT.check1);
  if (!SEGMENT.check1) {
    for (i = 0; i < PartSys->usedParticles; i++) {
        PartSys->particles[i].hue = (1024 * i) / PartSys->usedParticles; // color by particle index
    }
  }
  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PS_BALANCE[] PROGMEM = "PS 1D Balance@!,!,Hardness,Blur,Tilt,Position Color,Wrap,Random;,!;!;1;pal=18,c2=0,c3=4,o1=1";

/*
Particle based Chase effect
Uses palette for particle color
by DedeHai (Damian Schneider)
*/
uint16_t mode_particleChase(void) {
  ParticleSystem1D *PartSys = nullptr;
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 255, 2, true)) // init
      return mode_static(); // allocation failed or is single pixel
    SEGENV.aux0 = 0xFFFF; // invalidate
    *PartSys->PSdataEnd = 1; // huedir
    *(PartSys->PSdataEnd + 1) = 1; // sizedir
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!
  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setColorByPosition(SEGMENT.check3);
  PartSys->setMotionBlur(7 + ((SEGMENT.custom3) << 3)); // anable motion blur
  uint32_t numParticles = 1 + map(SEGMENT.intensity, 0, 255, 2, 255 / (1 + (SEGMENT.custom1 >> 6))); // depends on intensity and particle size (custom1), minimum 1
  numParticles = min(numParticles, PartSys->usedParticles); // limit to available particles
  int32_t huestep = 1 + ((((uint32_t)SEGMENT.custom2 << 19) / numParticles) >> 16); // hue increment
  uint32_t settingssum = SEGMENT.speed + SEGMENT.intensity + SEGMENT.custom1 + SEGMENT.custom2 + SEGMENT.check1 + SEGMENT.check2 + SEGMENT.check3;
  if (SEGENV.aux0 != settingssum) { // settings changed changed, update
    if (SEGMENT.check1)
      SEGENV.step = PartSys->advPartProps[0].size / 2 + (PartSys->maxX / numParticles);
    else
      SEGENV.step = (PartSys->maxX + (PS_P_RADIUS_1D << 5)) / numParticles; // spacing between particles
    for (int32_t i = 0; i < (int32_t)PartSys->usedParticles; i++) {
      PartSys->advPartProps[i].sat = 255;
      PartSys->particles[i].x = (i - 1) * SEGENV.step; // distribute evenly (starts out of frame for i=0)
      PartSys->particles[i].vx =  SEGMENT.speed >> 2;
      PartSys->advPartProps[i].size = SEGMENT.custom1;
      if (SEGMENT.custom2 < 255)
        PartSys->particles[i].hue = i * huestep; // gradient distribution
      else
        PartSys->particles[i].hue = hw_random16();
    }
    SEGENV.aux0 = settingssum;
  }

  if(SEGMENT.check1) {
    huestep = 1 + (max((int)huestep, 3)  * ((int(sin16_t(strip.now * 3) + 32767))) >> 15); // changes gradient spread (scale hue step)
  }

  // wrap around (cannot use particle system wrap if distributing colors manually, it also wraps rendering which does not look good)
  for (int32_t i = (int32_t)PartSys->usedParticles - 1; i >= 0; i--) { // check from the back, last particle wraps first, multiple particles can overrun per frame
    if (PartSys->particles[i].x > PartSys->maxX + PS_P_RADIUS_1D + PartSys->advPartProps[i].size) { // wrap it around
      uint32_t nextindex = (i + 1) % PartSys->usedParticles;
      PartSys->particles[i].x = PartSys->particles[nextindex].x - (int)SEGENV.step;
      if(SEGMENT.check1) // playful mode, vary size
        PartSys->advPartProps[i].size = max(1 + (SEGMENT.custom1 >> 1), ((int(sin16_t(strip.now << 1) + 32767)) >> 8)); // cycle size
      if (SEGMENT.custom2 < 255)
        PartSys->particles[i].hue = PartSys->particles[nextindex].hue - huestep;
      else
        PartSys->particles[i].hue = hw_random16();
    }
    PartSys->particles[i].ttl = 300; // reset ttl, cannot use perpetual because memmanager can change pointer at any time
  }

  if (SEGMENT.check1) { // playful mode, changes hue, size, speed, density dynamically
    int8_t* huedir = reinterpret_cast<int8_t *>(PartSys->PSdataEnd);  //assign data pointer
    int8_t* stepdir = reinterpret_cast<int8_t *>(PartSys->PSdataEnd + 1);
    if(*stepdir == 0) *stepdir = 1; // initialize directions
    if(*huedir == 0) *huedir = 1;
    if (SEGENV.step >= (PartSys->advPartProps[0].size + PS_P_RADIUS_1D * 4) + PartSys->maxX / numParticles)
      *stepdir = -1; // increase density (decrease space between particles)
    else if (SEGENV.step <= (PartSys->advPartProps[0].size >> 1) + ((PartSys->maxX / numParticles)))
      *stepdir = 1; // decrease density
    if (SEGENV.aux1 > 512)
      *huedir = -1;
    else if (SEGENV.aux1 < 50)
      *huedir = 1;
    if (SEGMENT.call % (1024 / (1 + (SEGMENT.speed >> 2))) == 0)
      SEGENV.aux1 += *huedir;
    int8_t globalhuestep = 0; // global hue increment
    if (SEGMENT.call % (1 + (int(sin16_t(strip.now) + 32767) >> 12))  == 0)
      globalhuestep = 2; // global hue change to add some color variation
    if ((SEGMENT.call & 0x1F) == 0)
      SEGENV.step += *stepdir; // change density
    for(uint32_t i = 0; i < PartSys->usedParticles; i++) {
      PartSys->particles[i].hue -= globalhuestep; // shift global hue (both directions)
      PartSys->particles[i].vx = 1 + (SEGMENT.speed >> 2) + ((int32_t(sin16_t(strip.now >> 1) + 32767) * (SEGMENT.speed >> 2)) >> 16);
    }
  }

  PartSys->setParticleSize(SEGMENT.custom1); // if custom1 == 0 this sets rendering size to one pixel
  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PS_CHASE[] PROGMEM = "PS Chase@!,Density,Size,Hue,Blur,Playful,,Position Color;,!;!;1;pal=11,sx=50,c2=5,c3=0";

/*
  Particle Fireworks Starburst replacement (smoother rendering, more settings)
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleStarburst(void) {
  ParticleSystem1D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 200, 0, true)) // init
      return mode_static(); // allocation failed or is single pixel
    PartSys->setKillOutOfBounds(true);
    PartSys->enableParticleCollisions(true, 200);
    PartSys->sources[0].source.ttl = 1; // set initial stanby time
    PartSys->sources[0].sat = 0; // emitted particles start out white
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur
  PartSys->setGravity(SEGMENT.check1 * 8); // enable gravity

  if (PartSys->sources[0].source.ttl-- == 0) { // stanby time elapsed TODO: make it a timer?
    uint32_t explosionsize = 4 + hw_random16(SEGMENT.intensity >> 2);
    PartSys->sources[0].source.hue = hw_random16();
    PartSys->sources[0].var = 10 + (explosionsize << 1);
    PartSys->sources[0].minLife = 250;
    PartSys->sources[0].maxLife = 300;
    PartSys->sources[0].source.x = hw_random(PartSys->maxX); //random explosion position
    PartSys->sources[0].source.ttl = 10 + hw_random16(255 - SEGMENT.speed);
    PartSys->sources[0].size = SEGMENT.custom1; // Fragment size
    PartSys->setParticleSize(SEGMENT.custom1); // enable advanced size rendering
    PartSys->sources[0].sourceFlags.collide = SEGMENT.check3;
    for (uint32_t e = 0; e < explosionsize; e++) { // emit particles
      if (SEGMENT.check2)
        PartSys->sources[0].source.hue = hw_random16(); //random color for each particle
      PartSys->sprayEmit(PartSys->sources[0]); //emit a particle
    }
  }
  //shrink all particles
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->advPartProps[i].size)
      PartSys->advPartProps[i].size--;
    if (PartSys->advPartProps[i].sat < 251)
      PartSys->advPartProps[i].sat += 1 + (SEGMENT.custom3 >> 2); //note: it should be >> 3, the >> 2 creates overflows resulting in blinking if custom3 > 27, which is a bonus feature
  }

  if (SEGMENT.call % 5 == 0) {
    PartSys->applyFriction(1); //slow down particles
  }

  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PS_STARBURST[] PROGMEM = "PS Starburst@Chance,Fragments,Size,Blur,Cooling,Gravity,Colorful,Push;,!;!;1;pal=52,sx=150,ix=150,c1=120,c2=0,c3=21";

/*
  Particle based 1D GEQ effect, each frequency bin gets an emitter, distributed over the strip
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particle1DGEQ(void) {
  ParticleSystem1D *PartSys = nullptr;
  uint32_t numSources;
  uint32_t i;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 16, 255, 0, true)) // init, no additional data needed
      return mode_static(); // allocation failed or is single pixel
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  numSources = PartSys->numSources;
  PartSys->setMotionBlur(SEGMENT.custom2); // anable motion blur

  uint32_t spacing = PartSys->maxX / numSources;
  for (i = 0; i < numSources; i++) {
    PartSys->sources[i].source.hue = i * 16; // hw_random16();   //TODO: make adjustable, maybe even colorcycle?
    PartSys->sources[i].var = SEGMENT.speed >> 2;
    PartSys->sources[i].minLife = 180 + (SEGMENT.intensity >> 1);
    PartSys->sources[i].maxLife = 240 + SEGMENT.intensity;
    PartSys->sources[i].sat = 255;
    PartSys->sources[i].size = SEGMENT.custom1;
    PartSys->sources[i].source.x = (spacing >> 1) + spacing * i; //distribute evenly
  }

  for (i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->particles[i].ttl > 20) PartSys->particles[i].ttl -= 20; //ttl is linked to brightness, this allows to use higher brightness but still a short lifespan
    else PartSys->particles[i].ttl = 0;
  }

  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t *)um_data->u_data[2]; // 16 bins with FFT data, log mapped already, each band contains frequency amplitude 0-255

  //map the bands into 16 positions on x axis, emit some particles according to frequency loudness
  i = 0;
  uint32_t bin = hw_random16(numSources); //current bin , start with random one to distribute available particles fairly
  uint32_t threshold = 300 - SEGMENT.intensity;

  for (i = 0; i < numSources; i++) {
    bin++;
    bin = bin % numSources;
    uint32_t emitparticle = 0;
    // uint8_t emitspeed = ((uint32_t)fftResult[bin] * (uint32_t)SEGMENT.speed) >> 10; // emit speed according to loudness of band (127 max!)
    if (fftResult[bin] > threshold) {
      emitparticle = 1;
    }
    else if (fftResult[bin] > 0) { // band has low volue
      uint32_t restvolume = ((threshold - fftResult[bin]) >> 2) + 2;
      if (hw_random() % restvolume == 0) {
        emitparticle = 1;
      }
    }

    if (emitparticle)
      PartSys->sprayEmit(PartSys->sources[bin]);
  }
  //TODO: add color control?

  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PS_1D_GEQ[] PROGMEM = "PS GEQ 1D@Speed,!,Size,Blur,,,,;,!;!;1f;pal=0,sx=50,ix=200,c1=0,c2=0,c3=0,o1=1,o2=1";

/*
  Particle based Fire effect
  Uses palette for particle color
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particleFire1D(void) {
  ParticleSystem1D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 5)) // init
      return mode_static(); // allocation failed or is single pixel
    PartSys->setKillOutOfBounds(true);
    PartSys->setParticleSize(1);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(128 + (SEGMENT.custom2 >> 1)); // enable motion blur
  PartSys->setColorByAge(true);
  uint32_t emitparticles = 1;
  uint32_t j = hw_random16();
  for (uint i = 0; i < 3; i++) { // 3 base flames
    if (PartSys->sources[i].source.ttl > 50)
      PartSys->sources[i].source.ttl -= 10; // TODO: in 2D making the source fade out slow results in much smoother flames, need to check if it can be done the same
    else
      PartSys->sources[i].source.ttl = 100 + hw_random16(200);
  }
  for (uint i = 0; i < PartSys->numSources; i++) {
    j = (j + 1) % PartSys->numSources;
    PartSys->sources[j].source.x = 0;
    PartSys->sources[j].var = 2 + (SEGMENT.speed >> 4);
    // base flames
    if (j > 2) {
      PartSys->sources[j].minLife = 150 + SEGMENT.intensity + (j << 2); // TODO: in 2D, min life is maxlife/2 and that looks very nice
      PartSys->sources[j].maxLife = 200 + SEGMENT.intensity + (j << 3);
      PartSys->sources[j].v = (SEGMENT.speed >> (2 + (j << 1)));
      if (emitparticles) {
        emitparticles--;
        PartSys->sprayEmit(PartSys->sources[j]); // emit a particle
      }
    }
    else {
      PartSys->sources[j].minLife = PartSys->sources[j].source.ttl + SEGMENT.intensity;
      PartSys->sources[j].maxLife = PartSys->sources[j].minLife + 50;
      PartSys->sources[j].v = SEGMENT.speed >> 2;
      if (SEGENV.call & 0x01) // every second frame
        PartSys->sprayEmit(PartSys->sources[j]); // emit a particle
    }
  }

  for (uint i = 0; i < PartSys->usedParticles; i++) {
    PartSys->particles[i].x += PartSys->particles[i].ttl >> 7; // 'hot' particles are faster, apply some extra velocity
    if (PartSys->particles[i].ttl > 3 + ((255 - SEGMENT.custom1) >> 1))
      PartSys->particles[i].ttl -= map(SEGMENT.custom1, 0, 255, 1, 3); // age faster
  }

  PartSys->update(); // update and render

  return FRAMETIME;
}
static const char _data_FX_MODE_PS_FIRE1D[] PROGMEM = "PS Fire 1D@!,!,Cooling,Blur;,!;!;1;pal=35,sx=100,ix=50,c1=80,c2=100,c3=28,o1=1,o2=1";

/*
  Particle based AR effect, swoop particles along the strip with selected frequency loudness
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particle1DsonicStream(void) {
  ParticleSystem1D *PartSys = nullptr;

  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 255, 0, true)) // init, no additional data needed
      return mode_static(); // allocation failed or is single pixel
    PartSys->setKillOutOfBounds(true);
    PartSys->sources[0].source.x = 0; // at start
    //PartSys->sources[1].source.x = PartSys->maxX; // at end
    PartSys->sources[0].var = 0;//SEGMENT.custom1 >> 3;
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(20 + (SEGMENT.custom2 >> 1)); // anable motion blur
  PartSys->setSmearBlur(200); // smooth out the edges
  PartSys->sources[0].v = 5 + (SEGMENT.speed >> 2);

  // FFT processing
  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t *)um_data->u_data[2]; // 16 bins with FFT data, log mapped already, each band contains frequency amplitude 0-255
  uint32_t loudness;
  uint32_t baseBin = SEGMENT.custom3 >> 1; // 0 - 15 map(SEGMENT.custom3, 0, 31, 0, 14);

  loudness = fftResult[baseBin];// + fftResult[baseBin + 1];
  if (baseBin > 12)
    loudness = loudness << 2; // double loudness for high frequencies (better detecion)

  uint32_t threshold = 140 - (SEGMENT.intensity >> 1);
  if (SEGMENT.check2) { // enable low pass filter for dynamic threshold
    SEGMENT.step = (SEGMENT.step * 31500 + loudness * (32768 - 31500)) >> 15; // low pass filter for simple beat detection: add average to base threshold
    threshold = 20 + (threshold >> 1) + SEGMENT.step; // add average to threshold
  }

  // color
  uint32_t hueincrement = (SEGMENT.custom1 >> 3); // 0-31
  PartSys->sources[0].sat = SEGMENT.custom1 > 0 ? 255 : 0; // color slider at zero: set to white
  PartSys->setColorByPosition(SEGMENT.custom1 == 255);

  // particle manipulation
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (PartSys->sources[0].sourceFlags.perpetual == false) { // age faster if not perpetual
      if (PartSys->particles[i].ttl > 2) {
        PartSys->particles[i].ttl -= 2; //ttl is linked to brightness, this allows to use higher brightness but still a short lifespan
      }
      else PartSys->particles[i].ttl = 0;
    }
    if (SEGMENT.check1) { // modulate colors by mid frequencies
      int mids = sqrt32_bw((int)fftResult[5] + (int)fftResult[6] + (int)fftResult[7] + (int)fftResult[8] + (int)fftResult[9] + (int)fftResult[10]); // average the mids, bin 5 is ~500Hz, bin 10 is ~2kHz (see audio_reactive.h)
      PartSys->particles[i].hue += (mids * perlin8(PartSys->particles[i].x << 2, SEGMENT.step << 2)) >> 9; // color by perlin noise from mid frequencies
    }
  }

  if (loudness > threshold) {
    SEGMENT.aux0 += hueincrement; // change color
    PartSys->sources[0].minLife = 100 + (((unsigned)SEGMENT.intensity * loudness * loudness) >> 13);
    PartSys->sources[0].maxLife = PartSys->sources[0].minLife;
    PartSys->sources[0].source.hue = SEGMENT.aux0;
    PartSys->sources[0].size = SEGMENT.speed;
    if (PartSys->particles[SEGMENT.aux1].x > 3 * PS_P_RADIUS_1D || PartSys->particles[SEGMENT.aux1].ttl == 0) { // only emit if last particle is far enough away or dead
      int partindex = PartSys->sprayEmit(PartSys->sources[0]); // emit a particle
      if (partindex >= 0) SEGMENT.aux1 = partindex; // track last emitted particle
    }
  }
  else loudness = 0; // required for push mode

  PartSys->update(); // update and render (needs to be done before manipulation for initial particle spacing to be right)

  if (SEGMENT.check3) { // push mode
    PartSys->sources[0].sourceFlags.perpetual = true; // emitted particles dont age
    PartSys->applyFriction(1); //slow down particles
    int32_t movestep = (((int)SEGMENT.speed + 2) * loudness) >> 10;
    if (movestep) {
      for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
        if (PartSys->particles[i].ttl) {
          PartSys->particles[i].x += movestep; // push particles
          PartSys->particles[i].vx = 10 + (SEGMENT.speed >> 4) ; // give particles some speed for smooth movement (friction will slow them down)
        }
      }
    }
  }
  else {
    PartSys->sources[0].sourceFlags.perpetual = false; // emitted particles age
    // move all particles (again) to allow faster speeds
    for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
      if (PartSys->particles[i].vx == 0)
        PartSys->particles[i].vx = PartSys->sources[0].v; // move static particles (after disabling push mode)
      PartSys->particleMoveUpdate(PartSys->particles[i], PartSys->particleFlags[i], nullptr, &PartSys->advPartProps[i]);
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_PS_SONICSTREAM[] PROGMEM = "PS Sonic Stream@!,!,Color,Blur,Bin,Mod,Filter,Push;,!;!;1f;c3=0,o2=1";


/*
  Particle based AR effect, creates exploding particles on beats
  by DedeHai (Damian Schneider)
*/
uint16_t mode_particle1DsonicBoom(void) {
  ParticleSystem1D *PartSys = nullptr;
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 255, 0, true)) // init, no additional data needed
      return mode_static(); // allocation failed or is single pixel
    PartSys->setKillOutOfBounds(true);
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!

  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(180 * SEGMENT.check3);
  PartSys->setSmearBlur(64 * SEGMENT.check3);
  PartSys->sources[0].var = map(SEGMENT.speed, 0, 255, 10, 127);

  // FFT processing
  um_data_t *um_data = getAudioData();
  uint8_t *fftResult = (uint8_t *)um_data->u_data[2]; // 16 bins with FFT data, log mapped already, each band contains frequency amplitude 0-255
  uint32_t loudness;
  uint32_t baseBin = SEGMENT.custom3 >> 1; // 0 - 15 map(SEGMENT.custom3, 0, 31, 0, 14);
  loudness = fftResult[baseBin];// + fftResult[baseBin + 1];

  if (baseBin > 12)
    loudness = loudness << 2; // double loudness for high frequencies (better detecion)
  uint32_t threshold = 150 - (SEGMENT.intensity >> 1);
  if (SEGMENT.check2) { // enable low pass filter for dynamic threshold
    SEGMENT.step = (SEGMENT.step * 31500 + loudness * (32768 - 31500)) >> 15; // low pass filter for simple beat detection: add average to base threshold
    threshold = 20 + (threshold >> 1) + SEGMENT.step; // add average to threshold
  }

  // particle manipulation
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (SEGMENT.check1) { // modulate colors by mid frequencies
      int mids = sqrt32_bw((int)fftResult[5] + (int)fftResult[6] + (int)fftResult[7] + (int)fftResult[8] + (int)fftResult[9] + (int)fftResult[10]); // average the mids, bin 5 is ~500Hz, bin 10 is ~2kHz (see audio_reactive.h)
      PartSys->particles[i].hue += (mids * perlin8(PartSys->particles[i].x << 2, SEGMENT.step << 2)) >> 9; // color by perlin noise from mid frequencies
    }
    if (PartSys->particles[i].ttl > 16) {
      PartSys->particles[i].ttl -= 16; //ttl is linked to brightness, this allows to use higher brightness but still a (very) short lifespan
    }
  }

  if (loudness > threshold) {
    if (SEGMENT.aux1 == 0) { // edge detected, code only runs once per "beat"
      // update position
      if (SEGMENT.custom2 < 128) // fixed position
        PartSys->sources[0].source.x = map(SEGMENT.custom2, 0, 127, 0, PartSys->maxX);
      else if (SEGMENT.custom2 < 255) { // advances on each "beat"
        int32_t step = PartSys->maxX / (((270 - SEGMENT.custom2) >> 3)); // step: 2 - 33 steps for full segment width
        PartSys->sources[0].source.x = (PartSys->sources[0].source.x + step) % PartSys->maxX;
        if (PartSys->sources[0].source.x < step) // align to be symmetrical by making the first position half a step from start
          PartSys->sources[0].source.x = step >> 1;
      }
      else // position set to max, use random postion per beat
        PartSys->sources[0].source.x = hw_random(PartSys->maxX);

      // update color
      //PartSys->setColorByPosition(SEGMENT.custom1 == 255);     // color slider at max: particle color by position
      PartSys->sources[0].sat = SEGMENT.custom1 > 0 ? 255 : 0; // color slider at zero: set to white
      if (SEGMENT.custom1 == 255) // emit color by position
        SEGMENT.aux0 = map(PartSys->sources[0].source.x , 0, PartSys->maxX, 0, 255);
      else if (SEGMENT.custom1 > 0)
        SEGMENT.aux0 += (SEGMENT.custom1 >> 1); // change emit color per "beat"
    }
    SEGMENT.aux1 = 1; // track edge detection

    PartSys->sources[0].minLife = 200;
    PartSys->sources[0].maxLife = PartSys->sources[0].minLife + (((unsigned)SEGMENT.intensity * loudness * loudness) >> 13);
    PartSys->sources[0].source.hue = SEGMENT.aux0;
    PartSys->sources[0].size = 1; //SEGMENT.speed>>3;
    uint32_t explosionsize = 4 + (PartSys->maxXpixel >> 2);
    explosionsize = hw_random16((explosionsize * loudness) >> 10);
    for (uint32_t e = 0; e < explosionsize; e++) { // emit explosion particles
        PartSys->sprayEmit(PartSys->sources[0]); // emit a particle
      }
  }
  else
    SEGMENT.aux1 = 0; // reset edge detection

  PartSys->update(); // update and render (needs to be done before manipulation for initial particle spacing to be right)
  return FRAMETIME;
}
static const char _data_FX_MODE_PS_SONICBOOM[] PROGMEM = "PS Sonic Boom@!,!,Color,Position,Bin,Mod,Filter,Blur;,!;!;1f;c2=63,c3=0,o2=1";

/*
Particles bound by springs
by DedeHai (Damian Schneider)
*/
uint16_t mode_particleSpringy(void) {
  ParticleSystem1D *PartSys = nullptr;
  if (SEGMENT.call == 0) { // initialization
    if (!initParticleSystem1D(PartSys, 1, 128, 0, true)) // init
      return mode_static(); // allocation failed or is single pixel
    SEGENV.aux0 = SEGENV.aux1 = 0xFFFF; // invalidate settings
  }
  else
    PartSys = reinterpret_cast<ParticleSystem1D *>(SEGENV.data); // if not first call, just set the pointer to the PS
  if (PartSys == nullptr)
    return mode_static(); // something went wrong, no data!
  // Particle System settings
  PartSys->updateSystem(); // update system properties (dimensions and data pointers)
  PartSys->setMotionBlur(220 * SEGMENT.check1); // anable motion blur
  PartSys->setSmearBlur(50); // smear a little
  PartSys->setUsedParticles(map(SEGMENT.custom1, 0, 255, 30 >> SEGMENT.check2, 255  >> (SEGMENT.check2*2))); // depends on density and particle size
 // PartSys->enableParticleCollisions(true, 140); // enable particle collisions, can not be set too hard or impulses will not strech the springs if soft.
  int32_t springlength = PartSys->maxX / (PartSys->usedParticles); // spring length (spacing between particles)
  int32_t springK = map(SEGMENT.speed, 0, 255, 5, 35); // spring constant (stiffness)

  uint32_t settingssum = SEGMENT.custom1 + SEGMENT.check2;
  if (SEGENV.aux0 != settingssum) { // number of particles changed, update distribution
    for (int32_t i = 0; i < (int32_t)PartSys->usedParticles; i++) {
      PartSys->advPartProps[i].sat = 255; // full saturation
      //PartSys->particleFlags[i].collide = true; // enable collision for particles
      PartSys->particles[i].x = (i+1) * ((PartSys->maxX) / (PartSys->usedParticles)); // distribute
      //PartSys->particles[i].vx = 0; //reset speed
      PartSys->advPartProps[i].size = SEGMENT.check2 ? 190 : 2; // set size, small or big
    }
    SEGENV.aux0 = settingssum;
  }
  int dxlimit = (2 + ((255 - SEGMENT.speed) >> 5)) * springlength; // limit for spring length to avoid overstretching

  int springforce[PartSys->usedParticles]; // spring forces
  memset(springforce, 0, PartSys->usedParticles * sizeof(int32_t)); // reset spring forces

  // calculate spring forces and limit particle positions
  if (PartSys->particles[0].x < -springlength)
    PartSys->particles[0].x = -springlength; // limit the spring length
  else if (PartSys->particles[0].x > dxlimit)
    PartSys->particles[0].x = dxlimit; // limit the spring length
  springforce[0] += ((springlength >> 1) - (PartSys->particles[0].x)) * springK; // first particle anchors to x=0

  for (uint32_t i = 1; i < PartSys->usedParticles; i++) {
    // reorder particles if they are out of order to prevent chaos
    if (PartSys->particles[i].x < PartSys->particles[i-1].x)
        std::swap(PartSys->particles[i].x, PartSys->particles[i-1].x); // swap particle positions to maintain order
    int dx = PartSys->particles[i].x - PartSys->particles[i-1].x; // distance, always positive
    if (dx > dxlimit) { // limit the spring length
      PartSys->particles[i].x = PartSys->particles[i-1].x + dxlimit;
      dx = dxlimit;
    }
    int dxleft = (springlength - dx); // offset from spring resting position
    springforce[i] += dxleft * springK;
    springforce[i-1] -= dxleft * springK;
    if (i == (PartSys->usedParticles - 1)) {
     if (PartSys->particles[i].x >= PartSys->maxX + springlength)
        PartSys->particles[i].x = PartSys->maxX + springlength;
      int dxright = (springlength >> 1) - (PartSys->maxX - PartSys->particles[i].x); // last particle anchors to x=maxX
      springforce[i] -= dxright * springK;
    }
  }
  // apply spring forces to particles
  bool dampenoscillations = (SEGMENT.call % (9 - (SEGMENT.speed >> 5))) == 0; // dampen oscillation if particles are slow, more damping on stiffer springs
  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    springforce[i] = springforce[i] / 64; // scale spring force (cannot use shifts because of negative values)
    int maxforce = 120; // limit spring force
    springforce[i] = springforce[i] > maxforce ? maxforce : springforce[i] < -maxforce ? -maxforce : springforce[i]; // limit spring force
    PartSys->applyForce(PartSys->particles[i], springforce[i], PartSys->advPartProps[i].forcecounter);
    //dampen slow particles to avoid persisting oscillations on higher stiffness
    if (dampenoscillations) {
      if (abs(PartSys->particles[i].vx) < 3 && abs(springforce[i]) < (springK >> 2))
        PartSys->particles[i].vx = (PartSys->particles[i].vx * 254) / 256; // take out some energy
    }
    PartSys->particles[i].ttl = 300; // reset ttl, cannot use perpetual
  }

  if (SEGMENT.call % ((65 - ((SEGMENT.intensity * (1 + (SEGMENT.speed>>3))) >> 7))) == 0) // more damping for higher stiffness
    PartSys->applyFriction((SEGMENT.intensity >> 2));

  // add a small resetting force so particles return to resting position even under high damping
  for (uint32_t i = 1; i < PartSys->usedParticles - 1; i++) {
    int restposition = (springlength >> 1) + i * springlength; // resting position
    int dx = restposition - PartSys->particles[i].x; // distance, always positive
    PartSys->applyForce(PartSys->particles[i], dx > 0 ? 1 : (dx < 0 ? -1 : 0), PartSys->advPartProps[i].forcecounter);
  }

  // Modes
  if (SEGMENT.check3) { // use AR, custom 3 becomes frequency band to use, applies velocity to center particle according to loudness
    um_data_t *um_data = getAudioData();
    uint8_t *fftResult = (uint8_t *)um_data->u_data[2]; // 16 bins with FFT data, log mapped already, each band contains frequency amplitude 0-255
    uint32_t baseBin = map(SEGMENT.custom3, 0, 31, 0, 14);
    uint32_t loudness = fftResult[baseBin] + fftResult[baseBin+1];
    uint32_t threshold = 80; //150 - (SEGMENT.intensity >> 1);
    if (loudness > threshold) {
        int offset = (PartSys->maxX >> 1) - PartSys->particles[PartSys->usedParticles>>1].x; // offset from center
        if (abs(offset) < PartSys->maxX >> 5) // push particle around in center sector
          PartSys->particles[PartSys->usedParticles>>1].vx = ((PartSys->particles[PartSys->usedParticles>>1].vx > 0 ? 1 : -1)) * (loudness >> 3);
    }
  }
  else{
    if (SEGMENT.custom3 <= 10) { // periodic pulse: 0-5 apply at start, 6-10 apply at center
      if (strip.now > SEGMENT.step) {
        int speed = (SEGMENT.custom3 > 5) ? (SEGMENT.custom3 - 6) : SEGMENT.custom3;
        SEGMENT.step = strip.now + 7500 - ((SEGMENT.speed << 3) + (speed << 10));
        int amplitude = 40 + (SEGMENT.custom1 >> 2);
        int index = (SEGMENT.custom3 > 5) ? (PartSys->usedParticles / 2) : 0; // center or start particle
        PartSys->particles[index].vx += amplitude;
      }
    }
    else if (SEGMENT.custom3 <= 30) { // sinusoidal wave: 11-20 apply at start, 21-30 apply at center
      int index = (SEGMENT.custom3 > 20) ? (PartSys->usedParticles / 2) : 0; // center or start particle
      int restposition = 0;
      if (index > 0) restposition = PartSys->maxX >> 1; // center
      //int amplitude = 5 + (SEGMENT.speed >> 3) + (SEGMENT.custom1 >> 2); // amplitude depends on density
      int amplitude = 5 + (SEGMENT.custom1 >> 2); // amplitude depends on density
      int speed = SEGMENT.custom3 - 10 - (index ? 10 : 0); // map 11-20 and 21-30 to 1-10
      int phase = strip.now * ((1 + (SEGMENT.speed >> 4)) * speed);
      if (SEGMENT.check2) amplitude <<= 1; // double amplitude for XL particles
      //PartSys->applyForce(PartSys->particles[index], (sin16_t(phase) * amplitude) >> 15, PartSys->advPartProps[index].forcecounter); // apply acceleration
      PartSys->particles[index].x = restposition + ((sin16_t(phase) * amplitude) >> 12); // apply position
    }
    else {
      if (hw_random16() < 656) { // ~1% chance to add a pulse
        int amplitude = 60;
        if (SEGMENT.check2) amplitude <<= 1; // double amplitude for XL particles
        PartSys->particles[PartSys->usedParticles >> 1].vx += hw_random16(amplitude << 1) - amplitude; // apply acceleration
      }
    }
  }

  for (uint32_t i = 0; i < PartSys->usedParticles; i++) {
    if (SEGMENT.custom2 == 255) { // map speed to hue
       int speedclr = ((int8_t(abs(PartSys->particles[i].vx))) >> 2) << 4; // scale for greater color variation, dump small values to avoid flickering
       //int speed = PartSys->particles[i].vx << 2; // +/- 512
       if (speedclr > 240) speedclr = 240; // limit color to non-wrapping part of palette
       PartSys->particles[i].hue = speedclr;
    }
    else if (SEGMENT.custom2 > 0)
      PartSys->particles[i].hue = i * (SEGMENT.custom2 >> 2); // gradient distribution
    else {
      // map hue to particle density
      int deviation;
      if (i == 0) // First particle: measure density based on distance to anchor point
        deviation = springlength/2 - PartSys->particles[i].x;
      else if (i == PartSys->usedParticles - 1) // Last particle: measure density based on distance to right boundary
        deviation = springlength/2 - (PartSys->maxX - PartSys->particles[i].x);
      else {
        // Middle particles: average of compression/expansion from both sides
        int leftDx = PartSys->particles[i].x - PartSys->particles[i-1].x;
        int rightDx = PartSys->particles[i+1].x - PartSys->particles[i].x;
        int avgDistance = (leftDx + rightDx) >> 1;
        if (avgDistance < 0) avgDistance = 0; // avoid negative distances (not sure why this happens)
        deviation = (springlength - avgDistance);
      }
      deviation = constrain(deviation, -127, 112); // limit deviation to -127..112 (do not go intwo wrapping part of palette)
      PartSys->particles[i].hue = 127 + deviation; // map density to hue
    }
  }
  PartSys->update(); // update and render
  return FRAMETIME;
}
static const char _data_FX_MODE_PS_SPRINGY[] PROGMEM = "PS Springy@Stiffness,Damping,Density,Hue,Mode,Smear,XL,AR;,!;!;1f;pal=54,c2=0,c3=23";

#endif // WLED_DISABLE_PARTICLESYSTEM1D

//////////////////////////////////////////////////////////////////////////////////////////
// mode data
static const char _data_RESERVED[] PROGMEM = "RSVD";

// add (or replace reserved) effect mode and data into vector
// use id==255 to find unallocated gaps (with "Reserved" data string)
// if vector size() is smaller than id (single) data is appended at the end (regardless of id)
// return the actual id used for the effect or 255 if the add failed.
uint8_t WS2812FX::addEffect(uint8_t id, mode_ptr mode_fn, const char *mode_name) {
  if (id == 255) { // find empty slot
    for (size_t i=1; i<_mode.size(); i++) if (_modeData[i] == _data_RESERVED) { id = i; break; }
  }
  if (id < _mode.size()) {
    if (_modeData[id] != _data_RESERVED) return 255; // do not overwrite an already added effect
    _mode[id]     = mode_fn;
    _modeData[id] = mode_name;
    return id;
  } else if (_mode.size() < 255) { // 255 is reserved for indicating the effect wasn't added
    _mode.push_back(mode_fn);
    _modeData.push_back(mode_name);
    if (_modeCount < _mode.size()) _modeCount++;
    return _mode.size() - 1;
  } else {
    return 255; // The vector is full so return 255
  }
}

void WS2812FX::setupEffectData() {
  // Solid must be first! (assuming vector is empty upon call to setup)
  _mode.push_back(&mode_static);
  _modeData.push_back(_data_FX_MODE_STATIC);
  // fill reserved word in case there will be any gaps in the array
  for (size_t i=1; i<_modeCount; i++) {
    _mode.push_back(&mode_static);
    _modeData.push_back(_data_RESERVED);
  }
  // now replace all pre-allocated effects
  // --- 1D non-audio effects ---
  addEffect(FX_MODE_BLINK, &mode_blink, _data_FX_MODE_BLINK);
  addEffect(FX_MODE_BREATH, &mode_breath, _data_FX_MODE_BREATH);
  addEffect(FX_MODE_COLOR_WIPE, &mode_color_wipe, _data_FX_MODE_COLOR_WIPE);
  addEffect(FX_MODE_COLOR_WIPE_RANDOM, &mode_color_wipe_random, _data_FX_MODE_COLOR_WIPE_RANDOM);
  addEffect(FX_MODE_RANDOM_COLOR, &mode_random_color, _data_FX_MODE_RANDOM_COLOR);
  addEffect(FX_MODE_COLOR_SWEEP, &mode_color_sweep, _data_FX_MODE_COLOR_SWEEP);
  addEffect(FX_MODE_DYNAMIC, &mode_dynamic, _data_FX_MODE_DYNAMIC);
  addEffect(FX_MODE_RAINBOW, &mode_rainbow, _data_FX_MODE_RAINBOW);
  addEffect(FX_MODE_RAINBOW_CYCLE, &mode_rainbow_cycle, _data_FX_MODE_RAINBOW_CYCLE);
  addEffect(FX_MODE_SCAN, &mode_scan, _data_FX_MODE_SCAN);
  addEffect(FX_MODE_DUAL_SCAN, &mode_dual_scan, _data_FX_MODE_DUAL_SCAN);
  addEffect(FX_MODE_FADE, &mode_fade, _data_FX_MODE_FADE);
  addEffect(FX_MODE_THEATER_CHASE, &mode_theater_chase, _data_FX_MODE_THEATER_CHASE);
  addEffect(FX_MODE_THEATER_CHASE_RAINBOW, &mode_theater_chase_rainbow, _data_FX_MODE_THEATER_CHASE_RAINBOW);
  addEffect(FX_MODE_RUNNING_LIGHTS, &mode_running_lights, _data_FX_MODE_RUNNING_LIGHTS);
  addEffect(FX_MODE_SAW, &mode_saw, _data_FX_MODE_SAW);
  addEffect(FX_MODE_TWINKLE, &mode_twinkle, _data_FX_MODE_TWINKLE);
  addEffect(FX_MODE_DISSOLVE, &mode_dissolve, _data_FX_MODE_DISSOLVE);
  addEffect(FX_MODE_DISSOLVE_RANDOM, &mode_dissolve_random, _data_FX_MODE_DISSOLVE_RANDOM);
  addEffect(FX_MODE_FLASH_SPARKLE, &mode_flash_sparkle, _data_FX_MODE_FLASH_SPARKLE);
  addEffect(FX_MODE_HYPER_SPARKLE, &mode_hyper_sparkle, _data_FX_MODE_HYPER_SPARKLE);
  addEffect(FX_MODE_STROBE, &mode_strobe, _data_FX_MODE_STROBE);
  addEffect(FX_MODE_STROBE_RAINBOW, &mode_strobe_rainbow, _data_FX_MODE_STROBE_RAINBOW);
  addEffect(FX_MODE_MULTI_STROBE, &mode_multi_strobe, _data_FX_MODE_MULTI_STROBE);
  addEffect(FX_MODE_BLINK_RAINBOW, &mode_blink_rainbow, _data_FX_MODE_BLINK_RAINBOW);
  addEffect(FX_MODE_ANDROID, &mode_android, _data_FX_MODE_ANDROID);
  addEffect(FX_MODE_CHASE_COLOR, &mode_chase_color, _data_FX_MODE_CHASE_COLOR);
  addEffect(FX_MODE_CHASE_RANDOM, &mode_chase_random, _data_FX_MODE_CHASE_RANDOM);
  addEffect(FX_MODE_CHASE_RAINBOW, &mode_chase_rainbow, _data_FX_MODE_CHASE_RAINBOW);
  addEffect(FX_MODE_CHASE_FLASH, &mode_chase_flash, _data_FX_MODE_CHASE_FLASH);
  addEffect(FX_MODE_CHASE_FLASH_RANDOM, &mode_chase_flash_random, _data_FX_MODE_CHASE_FLASH_RANDOM);
  addEffect(FX_MODE_CHASE_RAINBOW_WHITE, &mode_chase_rainbow_white, _data_FX_MODE_CHASE_RAINBOW_WHITE);
  addEffect(FX_MODE_COLORFUL, &mode_colorful, _data_FX_MODE_COLORFUL);
  addEffect(FX_MODE_TRAFFIC_LIGHT, &mode_traffic_light, _data_FX_MODE_TRAFFIC_LIGHT);
  addEffect(FX_MODE_COLOR_SWEEP_RANDOM, &mode_color_sweep_random, _data_FX_MODE_COLOR_SWEEP_RANDOM);
  addEffect(FX_MODE_RUNNING_COLOR, &mode_running_color, _data_FX_MODE_RUNNING_COLOR);
  addEffect(FX_MODE_AURORA, &mode_aurora, _data_FX_MODE_AURORA);
  addEffect(FX_MODE_RUNNING_RANDOM, &mode_running_random, _data_FX_MODE_RUNNING_RANDOM);
  addEffect(FX_MODE_LARSON_SCANNER, &mode_larson_scanner, _data_FX_MODE_LARSON_SCANNER);
  addEffect(FX_MODE_RAIN, &mode_rain, _data_FX_MODE_RAIN);
  addEffect(FX_MODE_PRIDE_2015, &mode_pride_2015, _data_FX_MODE_PRIDE_2015);
  addEffect(FX_MODE_COLORWAVES, &mode_colorwaves, _data_FX_MODE_COLORWAVES);
  addEffect(FX_MODE_FIREWORKS, &mode_fireworks, _data_FX_MODE_FIREWORKS);
  addEffect(FX_MODE_TETRIX, &mode_tetrix, _data_FX_MODE_TETRIX);
  addEffect(FX_MODE_FIRE_FLICKER, &mode_fire_flicker, _data_FX_MODE_FIRE_FLICKER);
  addEffect(FX_MODE_GRADIENT, &mode_gradient, _data_FX_MODE_GRADIENT);
  addEffect(FX_MODE_LOADING, &mode_loading, _data_FX_MODE_LOADING);
  addEffect(FX_MODE_FAIRY, &mode_fairy, _data_FX_MODE_FAIRY);
  addEffect(FX_MODE_TWO_DOTS, &mode_two_dots, _data_FX_MODE_TWO_DOTS);
  addEffect(FX_MODE_FAIRYTWINKLE, &mode_fairytwinkle, _data_FX_MODE_FAIRYTWINKLE);
  addEffect(FX_MODE_RUNNING_DUAL, &mode_running_dual, _data_FX_MODE_RUNNING_DUAL);
  #ifdef WLED_ENABLE_GIF
  addEffect(FX_MODE_IMAGE, &mode_image, _data_FX_MODE_IMAGE);
  #endif
  addEffect(FX_MODE_TRICOLOR_CHASE, &mode_tricolor_chase, _data_FX_MODE_TRICOLOR_CHASE);
  addEffect(FX_MODE_TRICOLOR_WIPE, &mode_tricolor_wipe, _data_FX_MODE_TRICOLOR_WIPE);
  addEffect(FX_MODE_TRICOLOR_FADE, &mode_tricolor_fade, _data_FX_MODE_TRICOLOR_FADE);
  addEffect(FX_MODE_LIGHTNING, &mode_lightning, _data_FX_MODE_LIGHTNING);
  addEffect(FX_MODE_ICU, &mode_icu, _data_FX_MODE_ICU);
  addEffect(FX_MODE_DUAL_LARSON_SCANNER, &mode_dual_larson_scanner, _data_FX_MODE_DUAL_LARSON_SCANNER);
  addEffect(FX_MODE_RANDOM_CHASE, &mode_random_chase, _data_FX_MODE_RANDOM_CHASE);
  addEffect(FX_MODE_OSCILLATE, &mode_oscillate, _data_FX_MODE_OSCILLATE);
  addEffect(FX_MODE_JUGGLE, &mode_juggle, _data_FX_MODE_JUGGLE);
  addEffect(FX_MODE_PALETTE, &mode_palette, _data_FX_MODE_PALETTE);
  addEffect(FX_MODE_BPM, &mode_bpm, _data_FX_MODE_BPM);
  addEffect(FX_MODE_FILLNOISE8, &mode_fillnoise8, _data_FX_MODE_FILLNOISE8);
  addEffect(FX_MODE_NOISE16_1, &mode_noise16_1, _data_FX_MODE_NOISE16_1);
  addEffect(FX_MODE_NOISE16_2, &mode_noise16_2, _data_FX_MODE_NOISE16_2);
  addEffect(FX_MODE_NOISE16_3, &mode_noise16_3, _data_FX_MODE_NOISE16_3);
  addEffect(FX_MODE_NOISE16_4, &mode_noise16_4, _data_FX_MODE_NOISE16_4);
  addEffect(FX_MODE_COLORTWINKLE, &mode_colortwinkle, _data_FX_MODE_COLORTWINKLE);
  addEffect(FX_MODE_LAKE, &mode_lake, _data_FX_MODE_LAKE);
  addEffect(FX_MODE_METEOR, &mode_meteor, _data_FX_MODE_METEOR);
  //addEffect(FX_MODE_METEOR_SMOOTH, &mode_meteor_smooth, _data_FX_MODE_METEOR_SMOOTH); // merged with mode_meteor 
  addEffect(FX_MODE_RAILWAY, &mode_railway, _data_FX_MODE_RAILWAY);
  addEffect(FX_MODE_RIPPLE, &mode_ripple, _data_FX_MODE_RIPPLE);
  addEffect(FX_MODE_TWINKLEFOX, &mode_twinklefox, _data_FX_MODE_TWINKLEFOX);
  addEffect(FX_MODE_TWINKLECAT, &mode_twinklecat, _data_FX_MODE_TWINKLECAT);
  addEffect(FX_MODE_HALLOWEEN_EYES, &mode_halloween_eyes, _data_FX_MODE_HALLOWEEN_EYES);
  addEffect(FX_MODE_STATIC_PATTERN, &mode_static_pattern, _data_FX_MODE_STATIC_PATTERN);
  addEffect(FX_MODE_TRI_STATIC_PATTERN, &mode_tri_static_pattern, _data_FX_MODE_TRI_STATIC_PATTERN);
  addEffect(FX_MODE_SPOTS, &mode_spots, _data_FX_MODE_SPOTS);
  addEffect(FX_MODE_SPOTS_FADE, &mode_spots_fade, _data_FX_MODE_SPOTS_FADE);
  addEffect(FX_MODE_COMET, &mode_comet, _data_FX_MODE_COMET);
  #ifdef WLED_PS_DONT_REPLACE_FX
  addEffect(FX_MODE_MULTI_COMET, &mode_multi_comet, _data_FX_MODE_MULTI_COMET);  
  addEffect(FX_MODE_ROLLINGBALLS, &rolling_balls, _data_FX_MODE_ROLLINGBALLS);
  addEffect(FX_MODE_SPARKLE, &mode_sparkle, _data_FX_MODE_SPARKLE);
  addEffect(FX_MODE_GLITTER, &mode_glitter, _data_FX_MODE_GLITTER);
  addEffect(FX_MODE_SOLID_GLITTER, &mode_solid_glitter, _data_FX_MODE_SOLID_GLITTER);
  addEffect(FX_MODE_STARBURST, &mode_starburst, _data_FX_MODE_STARBURST);
  addEffect(FX_MODE_DANCING_SHADOWS, &mode_dancing_shadows, _data_FX_MODE_DANCING_SHADOWS);
  addEffect(FX_MODE_FIRE_2012, &mode_fire_2012, _data_FX_MODE_FIRE_2012);
  addEffect(FX_MODE_EXPLODING_FIREWORKS, &mode_exploding_fireworks, _data_FX_MODE_EXPLODING_FIREWORKS);
  #endif
  addEffect(FX_MODE_CANDLE, &mode_candle, _data_FX_MODE_CANDLE);
  addEffect(FX_MODE_BOUNCINGBALLS, &mode_bouncing_balls, _data_FX_MODE_BOUNCINGBALLS);
  addEffect(FX_MODE_POPCORN, &mode_popcorn, _data_FX_MODE_POPCORN);
  addEffect(FX_MODE_DRIP, &mode_drip, _data_FX_MODE_DRIP);
  addEffect(FX_MODE_SINELON, &mode_sinelon, _data_FX_MODE_SINELON);
  addEffect(FX_MODE_SINELON_DUAL, &mode_sinelon_dual, _data_FX_MODE_SINELON_DUAL);
  addEffect(FX_MODE_SINELON_RAINBOW, &mode_sinelon_rainbow, _data_FX_MODE_SINELON_RAINBOW);
  addEffect(FX_MODE_PLASMA, &mode_plasma, _data_FX_MODE_PLASMA);
  addEffect(FX_MODE_PERCENT, &mode_percent, _data_FX_MODE_PERCENT);
  addEffect(FX_MODE_RIPPLE_RAINBOW, &mode_ripple_rainbow, _data_FX_MODE_RIPPLE_RAINBOW);
  addEffect(FX_MODE_HEARTBEAT, &mode_heartbeat, _data_FX_MODE_HEARTBEAT);
  addEffect(FX_MODE_PACIFICA, &mode_pacifica, _data_FX_MODE_PACIFICA);
  addEffect(FX_MODE_CANDLE_MULTI, &mode_candle_multi, _data_FX_MODE_CANDLE_MULTI);
  addEffect(FX_MODE_SUNRISE, &mode_sunrise, _data_FX_MODE_SUNRISE);
  addEffect(FX_MODE_PHASED, &mode_phased, _data_FX_MODE_PHASED);
  addEffect(FX_MODE_TWINKLEUP, &mode_twinkleup, _data_FX_MODE_TWINKLEUP);
  addEffect(FX_MODE_NOISEPAL, &mode_noisepal, _data_FX_MODE_NOISEPAL);
  addEffect(FX_MODE_SINEWAVE, &mode_sinewave, _data_FX_MODE_SINEWAVE);
  addEffect(FX_MODE_PHASEDNOISE, &mode_phased_noise, _data_FX_MODE_PHASEDNOISE);
  addEffect(FX_MODE_FLOW, &mode_flow, _data_FX_MODE_FLOW);
  addEffect(FX_MODE_CHUNCHUN, &mode_chunchun, _data_FX_MODE_CHUNCHUN);  
  addEffect(FX_MODE_WASHING_MACHINE, &mode_washing_machine, _data_FX_MODE_WASHING_MACHINE);
  addEffect(FX_MODE_BLENDS, &mode_blends, _data_FX_MODE_BLENDS);
  addEffect(FX_MODE_TV_SIMULATOR, &mode_tv_simulator, _data_FX_MODE_TV_SIMULATOR);
  addEffect(FX_MODE_DYNAMIC_SMOOTH, &mode_dynamic_smooth, _data_FX_MODE_DYNAMIC_SMOOTH);

  // --- 1D audio effects ---
  addEffect(FX_MODE_PIXELS, &mode_pixels, _data_FX_MODE_PIXELS);
  addEffect(FX_MODE_PIXELWAVE, &mode_pixelwave, _data_FX_MODE_PIXELWAVE);
  addEffect(FX_MODE_JUGGLES, &mode_juggles, _data_FX_MODE_JUGGLES);
  addEffect(FX_MODE_MATRIPIX, &mode_matripix, _data_FX_MODE_MATRIPIX);
  addEffect(FX_MODE_GRAVIMETER, &mode_gravimeter, _data_FX_MODE_GRAVIMETER);
  addEffect(FX_MODE_PLASMOID, &mode_plasmoid, _data_FX_MODE_PLASMOID);
  addEffect(FX_MODE_PUDDLES, &mode_puddles, _data_FX_MODE_PUDDLES);
  addEffect(FX_MODE_MIDNOISE, &mode_midnoise, _data_FX_MODE_MIDNOISE);
  addEffect(FX_MODE_NOISEMETER, &mode_noisemeter, _data_FX_MODE_NOISEMETER);
  addEffect(FX_MODE_FREQWAVE, &mode_freqwave, _data_FX_MODE_FREQWAVE);
  addEffect(FX_MODE_FREQMATRIX, &mode_freqmatrix, _data_FX_MODE_FREQMATRIX);
  addEffect(FX_MODE_WATERFALL, &mode_waterfall, _data_FX_MODE_WATERFALL);
  addEffect(FX_MODE_FREQPIXELS, &mode_freqpixels, _data_FX_MODE_FREQPIXELS);
  addEffect(FX_MODE_NOISEFIRE, &mode_noisefire, _data_FX_MODE_NOISEFIRE);
  addEffect(FX_MODE_PUDDLEPEAK, &mode_puddlepeak, _data_FX_MODE_PUDDLEPEAK);
  addEffect(FX_MODE_NOISEMOVE, &mode_noisemove, _data_FX_MODE_NOISEMOVE);
  addEffect(FX_MODE_PERLINMOVE, &mode_perlinmove, _data_FX_MODE_PERLINMOVE);
  addEffect(FX_MODE_RIPPLEPEAK, &mode_ripplepeak, _data_FX_MODE_RIPPLEPEAK);
  addEffect(FX_MODE_FREQMAP, &mode_freqmap, _data_FX_MODE_FREQMAP);
  addEffect(FX_MODE_GRAVCENTER, &mode_gravcenter, _data_FX_MODE_GRAVCENTER);
  addEffect(FX_MODE_GRAVCENTRIC, &mode_gravcentric, _data_FX_MODE_GRAVCENTRIC);
  addEffect(FX_MODE_GRAVFREQ, &mode_gravfreq, _data_FX_MODE_GRAVFREQ);
  addEffect(FX_MODE_DJLIGHT, &mode_DJLight, _data_FX_MODE_DJLIGHT);
  addEffect(FX_MODE_BLURZ, &mode_blurz, _data_FX_MODE_BLURZ);
  addEffect(FX_MODE_FLOWSTRIPE, &mode_FlowStripe, _data_FX_MODE_FLOWSTRIPE);
  addEffect(FX_MODE_WAVESINS, &mode_wavesins, _data_FX_MODE_WAVESINS);
  addEffect(FX_MODE_ROCKTAVES, &mode_rocktaves, _data_FX_MODE_ROCKTAVES);

  // --- 2D  effects ---
#ifndef WLED_DISABLE_2D
  addEffect(FX_MODE_2DPLASMAROTOZOOM, &mode_2Dplasmarotozoom, _data_FX_MODE_2DPLASMAROTOZOOM);
  addEffect(FX_MODE_2DSPACESHIPS, &mode_2Dspaceships, _data_FX_MODE_2DSPACESHIPS);
  addEffect(FX_MODE_2DCRAZYBEES, &mode_2Dcrazybees, _data_FX_MODE_2DCRAZYBEES);

  #ifdef WLED_PS_DONT_REPLACE_FX
  addEffect(FX_MODE_2DGHOSTRIDER, &mode_2Dghostrider, _data_FX_MODE_2DGHOSTRIDER);
  addEffect(FX_MODE_2DBLOBS, &mode_2Dfloatingblobs, _data_FX_MODE_2DBLOBS);
  #endif

  addEffect(FX_MODE_2DSCROLLTEXT, &mode_2Dscrollingtext, _data_FX_MODE_2DSCROLLTEXT);
  addEffect(FX_MODE_2DDRIFTROSE, &mode_2Ddriftrose, _data_FX_MODE_2DDRIFTROSE);
  addEffect(FX_MODE_2DDISTORTIONWAVES, &mode_2Ddistortionwaves, _data_FX_MODE_2DDISTORTIONWAVES);
  addEffect(FX_MODE_2DGEQ, &mode_2DGEQ, _data_FX_MODE_2DGEQ); // audio
  addEffect(FX_MODE_2DNOISE, &mode_2Dnoise, _data_FX_MODE_2DNOISE);
  addEffect(FX_MODE_2DFIRENOISE, &mode_2Dfirenoise, _data_FX_MODE_2DFIRENOISE);
  addEffect(FX_MODE_2DSQUAREDSWIRL, &mode_2Dsquaredswirl, _data_FX_MODE_2DSQUAREDSWIRL);

  //non audio
  addEffect(FX_MODE_2DDNA, &mode_2Ddna, _data_FX_MODE_2DDNA);
  addEffect(FX_MODE_2DMATRIX, &mode_2Dmatrix, _data_FX_MODE_2DMATRIX);
  addEffect(FX_MODE_2DMETABALLS, &mode_2Dmetaballs, _data_FX_MODE_2DMETABALLS);
  addEffect(FX_MODE_2DFUNKYPLANK, &mode_2DFunkyPlank, _data_FX_MODE_2DFUNKYPLANK); // audio
  addEffect(FX_MODE_2DPULSER, &mode_2DPulser, _data_FX_MODE_2DPULSER);
  addEffect(FX_MODE_2DDRIFT, &mode_2DDrift, _data_FX_MODE_2DDRIFT);
  addEffect(FX_MODE_2DWAVERLY, &mode_2DWaverly, _data_FX_MODE_2DWAVERLY); // audio
  addEffect(FX_MODE_2DSUNRADIATION, &mode_2DSunradiation, _data_FX_MODE_2DSUNRADIATION);
  addEffect(FX_MODE_2DCOLOREDBURSTS, &mode_2DColoredBursts, _data_FX_MODE_2DCOLOREDBURSTS);
  addEffect(FX_MODE_2DJULIA, &mode_2DJulia, _data_FX_MODE_2DJULIA);
  addEffect(FX_MODE_2DGAMEOFLIFE, &mode_2Dgameoflife, _data_FX_MODE_2DGAMEOFLIFE);
  addEffect(FX_MODE_2DTARTAN, &mode_2Dtartan, _data_FX_MODE_2DTARTAN);
  addEffect(FX_MODE_2DPOLARLIGHTS, &mode_2DPolarLights, _data_FX_MODE_2DPOLARLIGHTS);
  addEffect(FX_MODE_2DSWIRL, &mode_2DSwirl, _data_FX_MODE_2DSWIRL); // audio
  addEffect(FX_MODE_2DLISSAJOUS, &mode_2DLissajous, _data_FX_MODE_2DLISSAJOUS);
  addEffect(FX_MODE_2DFRIZZLES, &mode_2DFrizzles, _data_FX_MODE_2DFRIZZLES);
  addEffect(FX_MODE_2DPLASMABALL, &mode_2DPlasmaball, _data_FX_MODE_2DPLASMABALL);
  addEffect(FX_MODE_2DHIPHOTIC, &mode_2DHiphotic, _data_FX_MODE_2DHIPHOTIC);
  addEffect(FX_MODE_2DSINDOTS, &mode_2DSindots, _data_FX_MODE_2DSINDOTS);
  addEffect(FX_MODE_2DDNASPIRAL, &mode_2DDNASpiral, _data_FX_MODE_2DDNASPIRAL);
  addEffect(FX_MODE_2DBLACKHOLE, &mode_2DBlackHole, _data_FX_MODE_2DBLACKHOLE);
  addEffect(FX_MODE_2DSOAP, &mode_2Dsoap, _data_FX_MODE_2DSOAP);
  addEffect(FX_MODE_2DOCTOPUS, &mode_2Doctopus, _data_FX_MODE_2DOCTOPUS);
  addEffect(FX_MODE_2DWAVINGCELL, &mode_2Dwavingcell, _data_FX_MODE_2DWAVINGCELL);
  addEffect(FX_MODE_2DAKEMI, &mode_2DAkemi, _data_FX_MODE_2DAKEMI); // audio

#ifndef WLED_DISABLE_PARTICLESYSTEM2D
  addEffect(FX_MODE_PARTICLEVOLCANO, &mode_particlevolcano, _data_FX_MODE_PARTICLEVOLCANO);
  addEffect(FX_MODE_PARTICLEFIRE, &mode_particlefire, _data_FX_MODE_PARTICLEFIRE);
  addEffect(FX_MODE_PARTICLEFIREWORKS, &mode_particlefireworks, _data_FX_MODE_PARTICLEFIREWORKS);
  addEffect(FX_MODE_PARTICLEVORTEX, &mode_particlevortex, _data_FX_MODE_PARTICLEVORTEX);
  addEffect(FX_MODE_PARTICLEPERLIN, &mode_particleperlin, _data_FX_MODE_PARTICLEPERLIN);
  addEffect(FX_MODE_PARTICLEPIT, &mode_particlepit, _data_FX_MODE_PARTICLEPIT);
  addEffect(FX_MODE_PARTICLEBOX, &mode_particlebox, _data_FX_MODE_PARTICLEBOX);
  addEffect(FX_MODE_PARTICLEATTRACTOR, &mode_particleattractor, _data_FX_MODE_PARTICLEATTRACTOR); // 872 bytes
  addEffect(FX_MODE_PARTICLEIMPACT, &mode_particleimpact, _data_FX_MODE_PARTICLEIMPACT);
  addEffect(FX_MODE_PARTICLEWATERFALL, &mode_particlewaterfall, _data_FX_MODE_PARTICLEWATERFALL);
  addEffect(FX_MODE_PARTICLESPRAY, &mode_particlespray, _data_FX_MODE_PARTICLESPRAY);
  addEffect(FX_MODE_PARTICLESGEQ, &mode_particleGEQ, _data_FX_MODE_PARTICLEGEQ);
  addEffect(FX_MODE_PARTICLECENTERGEQ, &mode_particlecenterGEQ, _data_FX_MODE_PARTICLECIRCULARGEQ);
  addEffect(FX_MODE_PARTICLEGHOSTRIDER, &mode_particleghostrider, _data_FX_MODE_PARTICLEGHOSTRIDER);
  addEffect(FX_MODE_PARTICLEBLOBS, &mode_particleblobs, _data_FX_MODE_PARTICLEBLOBS);
  addEffect(FX_MODE_PARTICLEGALAXY, &mode_particlegalaxy, _data_FX_MODE_PARTICLEGALAXY);
#endif // WLED_DISABLE_PARTICLESYSTEM2D
#endif // WLED_DISABLE_2D

#ifndef WLED_DISABLE_PARTICLESYSTEM1D
addEffect(FX_MODE_PSDRIP, &mode_particleDrip, _data_FX_MODE_PARTICLEDRIP);
addEffect(FX_MODE_PSPINBALL, &mode_particlePinball, _data_FX_MODE_PSPINBALL); //potential replacement for: bouncing balls, rollingballs, popcorn
addEffect(FX_MODE_PSDANCINGSHADOWS, &mode_particleDancingShadows, _data_FX_MODE_PARTICLEDANCINGSHADOWS);
addEffect(FX_MODE_PSFIREWORKS1D, &mode_particleFireworks1D, _data_FX_MODE_PS_FIREWORKS1D);
addEffect(FX_MODE_PSSPARKLER, &mode_particleSparkler, _data_FX_MODE_PS_SPARKLER);
addEffect(FX_MODE_PSHOURGLASS, &mode_particleHourglass, _data_FX_MODE_PS_HOURGLASS);
addEffect(FX_MODE_PS1DSPRAY, &mode_particle1Dspray, _data_FX_MODE_PS_1DSPRAY);
addEffect(FX_MODE_PSBALANCE, &mode_particleBalance, _data_FX_MODE_PS_BALANCE);
addEffect(FX_MODE_PSCHASE, &mode_particleChase, _data_FX_MODE_PS_CHASE);
addEffect(FX_MODE_PSSTARBURST, &mode_particleStarburst, _data_FX_MODE_PS_STARBURST);
addEffect(FX_MODE_PS1DGEQ, &mode_particle1DGEQ, _data_FX_MODE_PS_1D_GEQ);
addEffect(FX_MODE_PSFIRE1D, &mode_particleFire1D, _data_FX_MODE_PS_FIRE1D);
addEffect(FX_MODE_PS1DSONICSTREAM, &mode_particle1DsonicStream, _data_FX_MODE_PS_SONICSTREAM);
addEffect(FX_MODE_PS1DSONICBOOM, &mode_particle1DsonicBoom, _data_FX_MODE_PS_SONICBOOM);
addEffect(FX_MODE_PS1DSPRINGY, &mode_particleSpringy, _data_FX_MODE_PS_SPRINGY);
#endif // WLED_DISABLE_PARTICLESYSTEM1D

}
