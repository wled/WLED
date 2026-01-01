#include "wled.h"

// for information how FX metadata strings work see https://kno.wled.ge/interfaces/json-api/#effect-metadata

// paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined)
#define PALETTE_SOLID_WRAP   (strip.paletteBlend == 1 || strip.paletteBlend == 3)
#define PALETTE_MOVING_WRAP !(strip.paletteBlend == 2 || (strip.paletteBlend == 0 && SEGMENT.speed == 0))

// static effect, used if an effect fails to initialize
static uint16_t mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
  return strip.isOffRefreshRequired() ? FRAMETIME : 350;
}

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
/  Scrolling Morse Code by Bob Loeffler
*   Adapted from code by automaticaddison.com and then optimized by claude.ai
*   aux0 is the pattern offset for scrolling
*   aux1 is the total pattern length
*   The sx slider selects the scrolling speed
*   Checkbox1 selects the color mode
*   Checkbox2 displays punctuation or not
*   Checkbox3 displays the End-of-message code or not
*   We get the text from the SEGMENT.name and convert it to morse code
*   Morse Code rules:
*    - there is one space between each part of a letter or number
*    - there are 3 spaces between each letter or number
*    - there are 7 spaces between each word
*/

// Build morse pattern into a buffer
void build_morsecode_pattern(const char *morse_code, bool *pattern, int &index, int maxSize) {
  const char *c = morse_code;
  
  // Build the dots and dashes into pattern array
  while (*c != '\0') {
    // it's a dot which is 1 pixel
    if (*c == '.') {
      if (index >= maxSize - 1) return; // Reserve space for spacing
      pattern[index++] = true;
    }
    else { // Must be a dash which is 3 pixels
      if (index >= maxSize - 3) return;
      pattern[index++] = true;
      pattern[index++] = true;
      pattern[index++] = true;
    }
    
    // 1 space between parts of a letter/number
    if (index >= maxSize) return;
    pattern[index++] = false;
    c++;
  }
    
  // 3 spaces between two letters
  if (index >= maxSize - 2) return;
  pattern[index++] = false;
  if (index >= maxSize - 1) return;
  pattern[index++] = false;
  if (index >= maxSize) return;
  pattern[index++] = false;
}

static uint16_t mode_morsecode(void) {
  if (SEGLEN < 1) return mode_static();
  
  // A-Z in Morse Code
  static const char * letters[] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
                     "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};
  // 0-9 in Morse Code
  static const char * numbers[] = {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};

  // Get the text to display
  char text[WLED_MAX_SEGNAME_LEN+1] = {'\0'};
  size_t len = 0;

  if (SEGMENT.name) len = strlen(SEGMENT.name);
  if (len == 0) { // fallback if empty segment name
    strcpy_P(text, PSTR("I Love WLED!"));
  } else {
    strcpy(text, SEGMENT.name);
  }

  // Convert to uppercase in place
  for (char *p = text; *p; p++) {
    *p = toupper(*p);
  }

  // Allocate per-segment storage for pattern
  constexpr size_t MORSECODE_MAX_PATTERN_SIZE = 1024;
  if (!SEGENV.allocateData(MORSECODE_MAX_PATTERN_SIZE)) return mode_static();
  bool* morsecodePattern = reinterpret_cast<bool*>(SEGENV.data);

  static bool lastCheck2 = false;
  static bool lastCheck3 = false;
  static char lastText[WLED_MAX_SEGNAME_LEN+1] = {'\0'};  // Track last text

  bool settingsChanged = (SEGMENT.check2 != lastCheck2) || (SEGMENT.check3 != lastCheck3);  // check if any checkbox settings were changed since last frame
  bool textChanged = (strcmp(text, lastText) != 0);   // check if the text has changed since the last frame

  // Initialize on first call or rebuild pattern
  if (SEGENV.call == 0 || textChanged || settingsChanged) {
    strcpy(lastText, text); // Save current text
    lastCheck2 = SEGMENT.check2;  // Save current state
    lastCheck3 = SEGMENT.check3;  // Save current state
    int patternLength = 0;

    // Build complete morse code pattern
    for (char *c = text; *c; c++) {
      if (patternLength >= MORSECODE_MAX_PATTERN_SIZE - 10) break; // Reserve space for trailing pattern
      // Check for letters
      if (*c >= 'A' && *c <= 'Z') {
        build_morsecode_pattern(letters[*c - 'A'], morsecodePattern, patternLength, MORSECODE_MAX_PATTERN_SIZE);
      }
      // Check for numbers
      else if (*c >= '0' && *c <= '9') {
        build_morsecode_pattern(numbers[*c - '0'], morsecodePattern, patternLength, MORSECODE_MAX_PATTERN_SIZE);
      }
      // Check for a space between words
      else if (*c == ' ') {
        for (int x = 0; x < 4; x++) {   // 7 spaces after the morse code pattern (3 after the last character and now 4 more)
          if (patternLength >= MORSECODE_MAX_PATTERN_SIZE) break;
          morsecodePattern[patternLength++] = false;
        }
      }
      // Check for punctuation
      else if (SEGMENT.check2) {
        const char *punctuationCode = nullptr;
        switch (*c) {
          case '.': punctuationCode = ".-.-.-"; break;
          case ',': punctuationCode = "--..--"; break;
          case '?': punctuationCode = "..--.."; break;
          case ':': punctuationCode = "---..."; break;
          case '-': punctuationCode = "-....-"; break;
          case '!': punctuationCode = "-.-.--"; break;
          case '&': punctuationCode = ".-..."; break;
          case '@': punctuationCode = ".--.-."; break;
          case ')': punctuationCode = "-.--.-"; break;
          case '(': punctuationCode = "-.--."; break;
          case '/': punctuationCode = "-..-."; break;
          case '\'': punctuationCode = ".----."; break;  // apostrophe character must be escaped with a \ character
        }
        if (punctuationCode) {
          build_morsecode_pattern(punctuationCode, morsecodePattern, patternLength, MORSECODE_MAX_PATTERN_SIZE);
        }
      }
    }

    // Build the End-of-message pattern
    if (SEGMENT.check3) {
      build_morsecode_pattern(".-.-.", morsecodePattern, patternLength, MORSECODE_MAX_PATTERN_SIZE);
    }

    for (int x = 0; x < 7; x++) {   // 10 spaces after the last pattern (3 after the last character and now 7 more)
      if (patternLength >= MORSECODE_MAX_PATTERN_SIZE) break;
      morsecodePattern[patternLength++] = false;
    }

    SEGENV.aux1 = patternLength; // Store pattern length
  }

  // Update offset to make the morse code scroll
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed)*3;
  uint32_t it = strip.now / cycleTime;
  if (SEGENV.step != it) {
    SEGENV.aux0++; // Increment scroll offset
    SEGENV.step = it;
  }

  int patternLength = SEGENV.aux1;

  // Clear background
  SEGMENT.fill(BLACK);

  // Draw the scrolling pattern
  int offset = SEGENV.aux0 % patternLength;

  for (int i = 0; i < SEGLEN; i++) {
    int patternIndex = (offset + i) % patternLength;
    if (morsecodePattern[patternIndex]) {
      if (SEGMENT.check1)
        SEGMENT.setPixelColor(i, SEGMENT.color_wheel(SEGENV.aux0 + i));
      else
        SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_MORSECODE[] PROGMEM = "Morse Code@Speed,,,,,Color mode,Punctuation,EndOfMessage;;!;1;sx=128,o1=1,o2=1";



/////////////////////
//  UserMod Class  //
/////////////////////

class UserFxUsermod : public Usermod {
 private:
 public:
  void setup() override {
    strip.addEffect(255, &mode_diffusionfire, _data_FX_MODE_DIFFUSIONFIRE);
    strip.addEffect(255, &mode_morsecode, _data_FX_MODE_MORSECODE);

    ////////////////////////////////////////
    //  add your effect function(s) here  //
    ////////////////////////////////////////

    // use id=255 for all custom user FX (the final id is assigned when adding the effect)

    // strip.addEffect(255, &mode_your_effect, _data_FX_MODE_YOUR_EFFECT);
    // strip.addEffect(255, &mode_your_effect2, _data_FX_MODE_YOUR_EFFECT2);
    // strip.addEffect(255, &mode_your_effect3, _data_FX_MODE_YOUR_EFFECT3);
  }
  void loop() override {} // nothing to do in the loop
  uint16_t getId() override { return USERMOD_ID_USER_FX; }
};

static UserFxUsermod user_fx;
REGISTER_USERMOD(user_fx);
