#include "wled.h"
#include "fcn_declare.h"
#include "const.h"


//helper to get int value at a position in string
int getNumVal(const String &req, uint16_t pos)
{
  return req.substring(pos+3).toInt();
}


//helper to get int value with in/decrementing support via ~ syntax
void parseNumber(const char* str, byte &val, byte minv, byte maxv)
{
  if (str == nullptr || str[0] == '\0') return;
  if (str[0] == 'r') {val = hw_random8(minv,maxv?maxv:255); return;} // maxv for random cannot be 0
  bool wrap = false;
  if (str[0] == 'w' && strlen(str) > 1) {str++; wrap = true;}
  if (str[0] == '~') {
    int out = atoi(str +1);
    if (out == 0) {
      if (str[1] == '0') return;
      if (str[1] == '-') {
        val = (int)(val -1) < (int)minv ? maxv : min((int)maxv,(val -1)); //-1, wrap around
      } else {
        val = (int)(val +1) > (int)maxv ? minv : max((int)minv,(val +1)); //+1, wrap around
      }
    } else {
      if (wrap && val == maxv && out > 0) out = minv;
      else if (wrap && val == minv && out < 0) out = maxv;
      else {
        out += val;
        if (out > maxv) out = maxv;
        if (out < minv) out = minv;
      }
      val = out;
    }
    return;
  } else if (minv == maxv && minv == 0) { // limits "unset" i.e. both 0
    byte p1 = atoi(str);
    const char* str2 = strchr(str,'~'); // min/max range (for preset cycle, e.g. "1~5~")
    if (str2) {
      byte p2 = atoi(++str2);           // skip ~
      if (p2 > 0) {
        while (isdigit(*(++str2)));     // skip digits
        parseNumber(str2, val, p1, p2);
        return;
      }
    }
  }
  val = atoi(str);
}

//getVal supports inc/decrementing and random ("X~Y(r|~[w][-][Z])" form)
bool getVal(JsonVariant elem, byte &val, byte vmin, byte vmax) {
  if (elem.is<int>()) {
		if (elem < 0) return false; //ignore e.g. {"ps":-1}
    val = elem;
    return true;
  } else if (elem.is<const char*>()) {
    const char* str = elem;
    size_t len = strnlen(str, 14);
    if (len == 0 || len > 12) return false;
    // fix for #3605 & #4346
    // ignore vmin and vmax and use as specified in API
    if (len > 3 && (strchr(str,'r') || strchr(str,'~') != strrchr(str,'~'))) vmax = vmin = 0; // we have "X~Y(r|~[w][-][Z])" form
    // end fix
    parseNumber(str, val, vmin, vmax);
    return true;
  }
  return false; //key does not exist
}


bool getBoolVal(const JsonVariant &elem, bool dflt) {
  if (elem.is<const char*>() && elem.as<const char*>()[0] == 't') {
    return !dflt;
  } else {
    return elem | dflt;
  }
}


bool updateVal(const char* req, const char* key, byte &val, byte minv, byte maxv)
{
  const char *v = strstr(req, key);
  if (v) v += strlen(key);
  else return false;
  parseNumber(v, val, minv, maxv);
  return true;
}

static size_t printSetFormInput(Print& settingsScript, const char* key, const char* selector, int value) {
  return settingsScript.printf_P(PSTR("d.Sf.%s.%s=%d;"), key, selector, value);
}

size_t printSetFormCheckbox(Print& settingsScript, const char* key, int val) {
  return printSetFormInput(settingsScript, key, PSTR("checked"), val);
}
size_t printSetFormValue(Print& settingsScript, const char* key, int val) {
  return printSetFormInput(settingsScript, key, PSTR("value"), val);
}
size_t printSetFormIndex(Print& settingsScript, const char* key, int index) {
  return printSetFormInput(settingsScript, key, PSTR("selectedIndex"), index);
}

size_t printSetFormValue(Print& settingsScript, const char* key, const char* val) {
  return settingsScript.printf_P(PSTR("d.Sf.%s.value=\"%s\";"),key,val);
}

size_t printSetClassElementHTML(Print& settingsScript, const char* key, const int index, const char* val) {
  return settingsScript.printf_P(PSTR("d.getElementsByClassName(\"%s\")[%d].innerHTML=\"%s\";"), key, index, val);
}



void prepareHostname(char* hostname)
{
  sprintf_P(hostname, PSTR("wled-%*s"), 6, escapedMac.c_str() + 6);
  const char *pC = serverDescription;
  unsigned pos = 5;          // keep "wled-"
  while (*pC && pos < 24) { // while !null and not over length
    if (isalnum(*pC)) {     // if the current char is alpha-numeric append it to the hostname
      hostname[pos] = *pC;
      pos++;
    } else if (*pC == ' ' || *pC == '_' || *pC == '-' || *pC == '+' || *pC == '!' || *pC == '?' || *pC == '*') {
      hostname[pos] = '-';
      pos++;
    }
    // else do nothing - no leading hyphens and do not include hyphens for all other characters.
    pC++;
  }
  //last character must not be hyphen
  if (pos > 5) {
    while (pos > 4 && hostname[pos -1] == '-') pos--;
    hostname[pos] = '\0'; // terminate string (leave at least "wled")
  }
}


bool isAsterisksOnly(const char* str, byte maxLen)
{
  for (unsigned i = 0; i < maxLen; i++) {
    if (str[i] == 0) break;
    if (str[i] != '*') return false;
  }
  //at this point the password contains asterisks only
  return (str[0] != 0); //false on empty string
}


//threading/network callback details: https://github.com/wled-dev/WLED/pull/2336#discussion_r762276994
bool requestJSONBufferLock(uint8_t moduleID)
{
  if (pDoc == nullptr) {
    DEBUG_PRINTLN(F("ERROR: JSON buffer not allocated!"));
    return false;
  }

#if defined(ARDUINO_ARCH_ESP32)
  // Use a recursive mutex type in case our task is the one holding the JSON buffer.
  // This can happen during large JSON web transactions.  In this case, we continue immediately
  // and then will return out below if the lock is still held.
  if (xSemaphoreTakeRecursive(jsonBufferLockMutex, 250) == pdFALSE) return false;  // timed out waiting
#elif defined(ARDUINO_ARCH_ESP8266)
  // If we're in system context, delay() won't return control to the user context, so there's
  // no point in waiting.
  if (can_yield()) {
    unsigned long now = millis();
    while (jsonBufferLock && (millis()-now < 250)) delay(1); // wait for fraction for buffer lock
  }
#else
  #error Unsupported task framework - fix requestJSONBufferLock
#endif  
  // If the lock is still held - by us, or by another task
  if (jsonBufferLock) {
    DEBUG_PRINTF_P(PSTR("ERROR: Locking JSON buffer (%d) failed! (still locked by %d)\n"), moduleID, jsonBufferLock);
#ifdef ARDUINO_ARCH_ESP32
    xSemaphoreGiveRecursive(jsonBufferLockMutex);
#endif
    return false;
  }

  jsonBufferLock = moduleID ? moduleID : 255;
  DEBUG_PRINTF_P(PSTR("JSON buffer locked. (%d)\n"), jsonBufferLock);
  pDoc->clear();
  return true;
}


void releaseJSONBufferLock()
{
  DEBUG_PRINTF_P(PSTR("JSON buffer released. (%d)\n"), jsonBufferLock);
  jsonBufferLock = 0;
#ifdef ARDUINO_ARCH_ESP32
  xSemaphoreGiveRecursive(jsonBufferLockMutex);
#endif  
}


// extracts effect mode (or palette) name from names serialized string
// caller must provide large enough buffer for name (including SR extensions)!
uint8_t extractModeName(uint8_t mode, const char *src, char *dest, uint8_t maxLen)
{
  if (src == JSON_mode_names || src == nullptr) {
    if (mode < strip.getModeCount()) {
      char lineBuffer[256];
      //strcpy_P(lineBuffer, (const char*)pgm_read_dword(&(WS2812FX::_modeData[mode])));
      strncpy_P(lineBuffer, strip.getModeData(mode), sizeof(lineBuffer)/sizeof(char)-1);
      lineBuffer[sizeof(lineBuffer)/sizeof(char)-1] = '\0'; // terminate string
      size_t len = strlen(lineBuffer);
      size_t j = 0;
      for (; j < maxLen && j < len; j++) {
        if (lineBuffer[j] == '\0' || lineBuffer[j] == '@') break;
        dest[j] = lineBuffer[j];
      }
      dest[j] = 0; // terminate string
      return strlen(dest);
    } else return 0;
  }

  if (src == JSON_palette_names && mode > (GRADIENT_PALETTE_COUNT + 13)) {
    snprintf_P(dest, maxLen, PSTR("~ Custom %d ~"), 255-mode);
    dest[maxLen-1] = '\0';
    return strlen(dest);
  }

  unsigned qComma = 0;
  bool insideQuotes = false;
  unsigned printedChars = 0;
  char singleJsonSymbol;
  size_t len = strlen_P(src);

  // Find the mode name in JSON
  for (size_t i = 0; i < len; i++) {
    singleJsonSymbol = pgm_read_byte_near(src + i);
    if (singleJsonSymbol == '\0') break;
    if (singleJsonSymbol == '@' && insideQuotes && qComma == mode) break; //stop when SR extension encountered
    switch (singleJsonSymbol) {
      case '"':
        insideQuotes = !insideQuotes;
        break;
      case '[':
      case ']':
        break;
      case ',':
        if (!insideQuotes) qComma++;
      default:
        if (!insideQuotes || (qComma != mode)) break;
        dest[printedChars++] = singleJsonSymbol;
    }
    if ((qComma > mode) || (printedChars >= maxLen)) break;
  }
  dest[printedChars] = '\0';
  return strlen(dest);
}


// extracts effect slider data (1st group after @)
uint8_t extractModeSlider(uint8_t mode, uint8_t slider, char *dest, uint8_t maxLen, uint8_t *var)
{
  dest[0] = '\0'; // start by clearing buffer

  if (mode < strip.getModeCount()) {
    String lineBuffer = FPSTR(strip.getModeData(mode));
    if (lineBuffer.length() > 0) {
      int start = lineBuffer.indexOf('@');   // String::indexOf() returns an int, not an unsigned; -1 means "not found"
      int stop  = lineBuffer.indexOf(';', start);
      if (start>0 && stop>0) {
        String names = lineBuffer.substring(start, stop); // include @
        int nameBegin = 1, nameEnd, nameDefault;
        if (slider < 10) {
          for (size_t i=0; i<=slider; i++) {
            const char *tmpstr;
            dest[0] = '\0'; //clear dest buffer
            if (nameBegin <= 0) break; // there are no more names
            nameEnd = names.indexOf(',', nameBegin);
            if (i == slider) {
              nameDefault = names.indexOf('=', nameBegin); // find default value
              if (nameDefault > 0 && var && ((nameEnd>0 && nameDefault<nameEnd) || nameEnd<0)) {
                *var = (uint8_t)atoi(names.substring(nameDefault+1).c_str());
              }
              if (names.charAt(nameBegin) == '!') {
                switch (slider) {
                  case  0: tmpstr = PSTR("FX Speed");     break;
                  case  1: tmpstr = PSTR("FX Intensity"); break;
                  case  2: tmpstr = PSTR("FX Custom 1");  break;
                  case  3: tmpstr = PSTR("FX Custom 2");  break;
                  case  4: tmpstr = PSTR("FX Custom 3");  break;
                  default: tmpstr = PSTR("FX Custom");    break;
                }
                strncpy_P(dest, tmpstr, maxLen); // copy the name into buffer (replacing previous)
                dest[maxLen-1] = '\0';
              } else {
                if (nameEnd<0) tmpstr = names.substring(nameBegin).c_str(); // did not find ",", last name?
                else           tmpstr = names.substring(nameBegin, nameEnd).c_str();
                strlcpy(dest, tmpstr, maxLen); // copy the name into buffer (replacing previous)
              }
            }
            nameBegin = nameEnd+1; // next name (if "," is not found it will be 0)
          } // next slider
        } else if (slider == 255) {
          // palette
          strlcpy(dest, "pal", maxLen);
          names = lineBuffer.substring(stop+1); // stop has index of color slot names
          nameBegin = names.indexOf(';'); // look for palette
          if (nameBegin >= 0) {
            nameEnd = names.indexOf(';', nameBegin+1);
            if (!isdigit(names[nameBegin+1])) nameBegin = names.indexOf('=', nameBegin+1); // look for default value
            if (nameEnd >= 0 && nameBegin > nameEnd) nameBegin = -1;
            if (nameBegin >= 0 && var) {
              *var = (uint8_t)atoi(names.substring(nameBegin+1).c_str());
            }
          }
        }
        // we have slider name (including default value) in the dest buffer
        for (size_t i=0; i<strlen(dest); i++) if (dest[i]=='=') { dest[i]='\0'; break; } // truncate default value

      } else {
        // defaults to just speed and intensity since there is no slider data
        switch (slider) {
          case 0:  strncpy_P(dest, PSTR("FX Speed"), maxLen); break;
          case 1:  strncpy_P(dest, PSTR("FX Intensity"), maxLen); break;
        }
        dest[maxLen] = '\0'; // strncpy does not necessarily null terminate string
      }
    }
    return strlen(dest);
  }
  return 0;
}


// extracts mode parameter defaults from last section of mode data (e.g. "Juggle@!,Trail;!,!,;!;012;sx=16,ix=240")
int16_t extractModeDefaults(uint8_t mode, const char *segVar)
{
  if (mode < strip.getModeCount()) {
    char lineBuffer[256];
    strncpy_P(lineBuffer, strip.getModeData(mode), sizeof(lineBuffer)/sizeof(char)-1);
    lineBuffer[sizeof(lineBuffer)/sizeof(char)-1] = '\0'; // terminate string
    if (lineBuffer[0] != 0) {
      char* startPtr = strrchr(lineBuffer, ';'); // last ";" in FX data
      if (!startPtr) return -1;

      char* stopPtr = strstr(startPtr, segVar);
      if (!stopPtr) return -1;

      stopPtr += strlen(segVar) +1; // skip "="
      return atoi(stopPtr);
    }
  }
  return -1;
}


void checkSettingsPIN(const char* pin) {
  if (!pin) return;
  if (!correctPIN && millis() - lastEditTime < PIN_RETRY_COOLDOWN) return; // guard against PIN brute force
  bool correctBefore = correctPIN;
  correctPIN = (strlen(settingsPIN) == 0 || strncmp(settingsPIN, pin, 4) == 0);
  if (correctBefore != correctPIN) createEditHandler(correctPIN);
  lastEditTime = millis();
}


uint16_t crc16(const unsigned char* data_p, size_t length) {
  uint8_t x;
  uint16_t crc = 0xFFFF;
  if (!length) return 0x1D0F;
  while (length--) {
    x = crc >> 8 ^ *data_p++;
    x ^= x>>4;
    crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
  }
  return crc;
}

// fastled beatsin: 1:1 replacements to remove the use of fastled sin16()
// Generates a 16-bit sine wave at a given BPM that oscillates within a given range. see fastled for details.
uint16_t beatsin88_t(accum88 beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset)
{
    uint16_t beat = beat88( beats_per_minute_88, timebase);
    uint16_t beatsin (sin16_t( beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

// Generates a 16-bit sine wave at a given BPM that oscillates within a given range. see fastled for details.
uint16_t beatsin16_t(accum88 beats_per_minute, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset)
{
    uint16_t beat = beat16( beats_per_minute, timebase);
    uint16_t beatsin = (sin16_t( beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

// Generates an 8-bit sine wave at a given BPM that oscillates within a given range. see fastled for details.
uint8_t beatsin8_t(accum88 beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t timebase, uint8_t phase_offset)
{
    uint8_t beat = beat8( beats_per_minute, timebase);
    uint8_t beatsin = sin8_t( beat + phase_offset);
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8( beatsin, rangewidth);
    uint8_t result = lowest + scaledbeat;
    return result;
}

///////////////////////////////////////////////////////////////////////////////
// Begin simulateSound (to enable audio enhanced effects to display something)
///////////////////////////////////////////////////////////////////////////////
// Currently 4 types defined, to be fine tuned and new types added
// (only 2 used as stored in 1 bit in segment options, consider switching to a single global simulation type)
typedef enum UM_SoundSimulations {
  UMS_BeatSin = 0,
  UMS_WeWillRockYou,
  UMS_10_13,
  UMS_14_3
} um_soundSimulations_t;

um_data_t* simulateSound(uint8_t simulationId)
{
  static uint8_t samplePeak;
  static float   FFT_MajorPeak;
  static uint8_t maxVol;
  static uint8_t binNum;

  static float    volumeSmth;
  static uint16_t volumeRaw;
  static float    my_magnitude;

  //arrays
  uint8_t *fftResult;

  static um_data_t* um_data = nullptr;

  if (!um_data) {
    //claim storage for arrays
    fftResult = (uint8_t *)malloc(sizeof(uint8_t) * 16);

    // initialize um_data pointer structure
    // NOTE!!!
    // This may change as AudioReactive usermod may change
    um_data = new um_data_t;
    um_data->u_size = 8;
    um_data->u_type = new um_types_t[um_data->u_size];
    um_data->u_data = new void*[um_data->u_size];
    um_data->u_data[0] = &volumeSmth;
    um_data->u_data[1] = &volumeRaw;
    um_data->u_data[2] = fftResult;
    um_data->u_data[3] = &samplePeak;
    um_data->u_data[4] = &FFT_MajorPeak;
    um_data->u_data[5] = &my_magnitude;
    um_data->u_data[6] = &maxVol;
    um_data->u_data[7] = &binNum;
  } else {
    // get arrays from um_data
    fftResult =  (uint8_t*)um_data->u_data[2];
  }

  uint32_t ms = millis();

  switch (simulationId) {
    default:
    case UMS_BeatSin:
      for (int i = 0; i<16; i++)
        fftResult[i] = beatsin8_t(120 / (i+1), 0, 255);
        // fftResult[i] = (beatsin8_t(120, 0, 255) + (256/16 * i)) % 256;
      volumeSmth = fftResult[8];
      break;
    case UMS_WeWillRockYou:
      if (ms%2000 < 200) {
        volumeSmth = hw_random8();
        for (int i = 0; i<5; i++)
          fftResult[i] = hw_random8();
      }
      else if (ms%2000 < 400) {
        volumeSmth = 0;
        for (int i = 0; i<16; i++)
          fftResult[i] = 0;
      }
      else if (ms%2000 < 600) {
        volumeSmth = hw_random8();
        for (int i = 5; i<11; i++)
          fftResult[i] = hw_random8();
      }
      else if (ms%2000 < 800) {
        volumeSmth = 0;
        for (int i = 0; i<16; i++)
          fftResult[i] = 0;
      }
      else if (ms%2000 < 1000) {
        volumeSmth = hw_random8();
        for (int i = 11; i<16; i++)
          fftResult[i] = hw_random8();
      }
      else {
        volumeSmth = 0;
        for (int i = 0; i<16; i++)
          fftResult[i] = 0;
      }
      break;
    case UMS_10_13:
      for (int i = 0; i<16; i++)
        fftResult[i] = perlin8(beatsin8_t(90 / (i+1), 0, 200)*15 + (ms>>10), ms>>3);
      volumeSmth = fftResult[8];
      break;
    case UMS_14_3:
      for (int i = 0; i<16; i++)
        fftResult[i] = perlin8(beatsin8_t(120 / (i+1), 10, 30)*10 + (ms>>14), ms>>3);
      volumeSmth = fftResult[8];
      break;
  }

  samplePeak    = hw_random8() > 250;
  FFT_MajorPeak = 21 + (volumeSmth*volumeSmth) / 8.0f; // walk thru full range of 21hz...8200hz
  maxVol        = 31;  // this gets feedback fro UI
  binNum        = 8;   // this gets feedback fro UI
  volumeRaw = volumeSmth;
  my_magnitude = 10000.0f / 8.0f; //no idea if 10000 is a good value for FFT_Magnitude ???
  if (volumeSmth < 1 ) my_magnitude = 0.001f;             // noise gate closed - mute

  return um_data;
}

static const char s_ledmap_tmpl[] PROGMEM = "ledmap%d.json";
// enumerate all ledmapX.json files on FS and extract ledmap names if existing
void enumerateLedmaps() {
  StaticJsonDocument<64> filter;
  filter["n"] = true;
  ledMaps = 1;
  for (size_t i=1; i<WLED_MAX_LEDMAPS; i++) {
    char fileName[33] = "/";
    sprintf_P(fileName+1, s_ledmap_tmpl, i);
    bool isFile = WLED_FS.exists(fileName);

    #ifndef ESP8266
    if (ledmapNames[i-1]) { //clear old name
      free(ledmapNames[i-1]);
      ledmapNames[i-1] = nullptr;
    }
    #endif

    if (isFile) {
      ledMaps |= 1 << i;

      #ifndef ESP8266
      if (requestJSONBufferLock(21)) {
        if (readObjectFromFile(fileName, nullptr, pDoc, &filter)) {
          size_t len = 0;
          JsonObject root = pDoc->as<JsonObject>();
          if (!root["n"].isNull()) {
            // name field exists
            const char *name = root["n"].as<const char*>();
            if (name != nullptr) len = strlen(name);
            if (len > 0 && len < 33) {
              ledmapNames[i-1] = static_cast<char*>(malloc(len+1));
              if (ledmapNames[i-1]) strlcpy(ledmapNames[i-1], name, 33);
            }
          }
          if (!ledmapNames[i-1]) {
            char tmp[33];
            snprintf_P(tmp, 32, s_ledmap_tmpl, i);
            len = strlen(tmp);
            ledmapNames[i-1] = static_cast<char*>(malloc(len+1));
            if (ledmapNames[i-1]) strlcpy(ledmapNames[i-1], tmp, 33);
          }
        }
        releaseJSONBufferLock();
      }
      #endif
    }

  }
}

/*
 * Returns a new, random color wheel index with a minimum distance of 42 from pos.
 */
uint8_t get_random_wheel_index(uint8_t pos) {
  uint8_t r = 0, x = 0, y = 0, d = 0;
  while (d < 42) {
    r = hw_random8();
    x = abs(pos - r);
    y = 255 - x;
    d = MIN(x, y);
  }
  return r;
}

// float version of map()
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

uint32_t hashInt(uint32_t s) {
  // borrowed from https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
  s = ((s >> 16) ^ s) * 0x45d9f3b;
  s = ((s >> 16) ^ s) * 0x45d9f3b;
  return (s >> 16) ^ s;
}

// 32 bit random number generator, inlining uses more code, use hw_random16() if speed is critical (see fcn_declare.h)
uint32_t hw_random(uint32_t upperlimit) {
  uint32_t rnd = hw_random();
  uint64_t scaled = uint64_t(rnd) * uint64_t(upperlimit);
  return scaled >> 32;
}

int32_t hw_random(int32_t lowerlimit, int32_t upperlimit) {
  if(lowerlimit >= upperlimit) {
    return lowerlimit;
  }
  uint32_t diff = upperlimit - lowerlimit;
  return hw_random(diff) + lowerlimit;
}

#ifndef ESP8266
void *p_malloc(size_t size) {
  int caps1 = MALLOC_CAP_SPIRAM  | MALLOC_CAP_8BIT;
  int caps2 = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
  if (psramSafe) {
    if (heap_caps_get_free_size(caps2) > 3*MIN_HEAP_SIZE && size < 512) std::swap(caps1, caps2);  // use DRAM for small alloactions & when heap is plenty
    return heap_caps_malloc_prefer(size, 2, caps1, caps2); // otherwise prefer PSRAM if it exists
  }
  return heap_caps_malloc(size, caps2);
}

void *p_realloc(void *ptr, size_t size) {
  int caps1 = MALLOC_CAP_SPIRAM  | MALLOC_CAP_8BIT;
  int caps2 = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
  if (psramSafe) {
    if (heap_caps_get_free_size(caps2) > 3*MIN_HEAP_SIZE && size < 512) std::swap(caps1, caps2);  // use DRAM for small alloactions & when heap is plenty
    return heap_caps_realloc_prefer(ptr, size, 2, caps1, caps2); // otherwise prefer PSRAM if it exists
  }
  return heap_caps_realloc(ptr, size, caps2);
}

void *p_calloc(size_t count, size_t size) {
  int caps1 = MALLOC_CAP_SPIRAM  | MALLOC_CAP_8BIT;
  int caps2 = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
  if (psramSafe) {
    if (heap_caps_get_free_size(caps2) > 3*MIN_HEAP_SIZE && size < 512) std::swap(caps1, caps2);  // use DRAM for small alloactions & when heap is plenty
    return heap_caps_calloc_prefer(count, size, 2, caps1, caps2); // otherwise prefer PSRAM if it exists
  }
  return heap_caps_calloc(count, size, caps2);
}

void *d_malloc(size_t size) {
  int caps1 = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
  int caps2 = MALLOC_CAP_SPIRAM  | MALLOC_CAP_8BIT;
  if (psramSafe) {
    if (size > MIN_HEAP_SIZE) std::swap(caps1, caps2);  // prefer PSRAM for large alloactions
    return heap_caps_malloc_prefer(size, 2, caps1, caps2); // otherwise prefer DRAM
  }
  return heap_caps_malloc(size, caps1);
}

void *d_realloc(void *ptr, size_t size) {
  int caps1 = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
  int caps2 = MALLOC_CAP_SPIRAM  | MALLOC_CAP_8BIT;
  if (psramSafe) {
    if (size > MIN_HEAP_SIZE) std::swap(caps1, caps2);  // prefer PSRAM for large alloactions
    return heap_caps_realloc_prefer(ptr, size, 2, caps1, caps2); // otherwise prefer DRAM
  }
  return heap_caps_realloc(ptr, size, caps1);
}

void *d_calloc(size_t count, size_t size) {
  int caps1 = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
  int caps2 = MALLOC_CAP_SPIRAM  | MALLOC_CAP_8BIT;
  if (psramSafe) {
    if (size > MIN_HEAP_SIZE) std::swap(caps1, caps2);  // prefer PSRAM for large alloactions
    return heap_caps_calloc_prefer(count, size, 2, caps1, caps2); // otherwise prefer DRAM
  }
  return heap_caps_calloc(count, size, caps1);
}
#endif

/*
 * Fixed point integer based Perlin noise functions by @dedehai
 * Note: optimized for speed and to mimic fastled inoise functions, not for accuracy or best randomness
 */
#define PERLIN_SHIFT 1

// calculate gradient for corner from hash value
static inline __attribute__((always_inline)) int32_t hashToGradient(uint32_t h) {
  // using more steps yields more "detailed" perlin noise but looks less like the original fastled version (adjust PERLIN_SHIFT to compensate, also changes range and needs proper adustment)
  // return (h & 0xFF) - 128; // use PERLIN_SHIFT 7
  // return (h & 0x0F) - 8; // use PERLIN_SHIFT 3
  // return (h & 0x07) - 4; // use PERLIN_SHIFT 2
  return (h & 0x03) - 2; // use PERLIN_SHIFT 1 -> closest to original fastled version
}

// Gradient functions for 1D, 2D and 3D Perlin noise  note: forcing inline produces smaller code and makes it 3x faster!
static inline __attribute__((always_inline)) int32_t gradient1D(uint32_t x0, int32_t dx) {
  uint32_t h = x0 * 0x27D4EB2D;
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  h ^= h >> 7;
  return (hashToGradient(h) * dx) >> PERLIN_SHIFT;
}

static inline __attribute__((always_inline)) int32_t gradient2D(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy) {
  uint32_t h = (x0 * 0x27D4EB2D) ^ (y0 * 0xB5297A4D);
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  return (hashToGradient(h) * dx + hashToGradient(h>>PERLIN_SHIFT) * dy) >> (1 + PERLIN_SHIFT);
}

static inline __attribute__((always_inline)) int32_t gradient3D(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy, uint32_t z0, int32_t dz) {
  // fast and good entropy hash from corner coordinates
  uint32_t h = (x0 * 0x27D4EB2D) ^ (y0 * 0xB5297A4D) ^ (z0 * 0x1B56C4E9);
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  return ((hashToGradient(h) * dx + hashToGradient(h>>(1+PERLIN_SHIFT)) * dy + hashToGradient(h>>(1 + 2*PERLIN_SHIFT)) * dz) * 85) >> (8 + PERLIN_SHIFT); // scale to 16bit, x*85 >> 8 = x/3
}

// fast cubic smoothstep: t*(3 - 2t²), optimized for fixed point, scaled to avoid overflows
static uint32_t smoothstep(const uint32_t t) {
  uint32_t t_squared = (t * t) >> 16;
  uint32_t factor = (3 << 16) - ((t << 1));
  return (t_squared * factor) >> 18; // scale to avoid overflows and give best resolution
}

// simple linear interpolation for fixed-point values, scaled for perlin noise use
static inline int32_t lerpPerlin(int32_t a, int32_t b, int32_t t) {
    return a + (((b - a) * t) >> 14); // match scaling with smoothstep to yield 16.16bit values
}

// 1D Perlin noise function that returns a value in range of -24691 to 24689
int32_t perlin1D_raw(uint32_t x, bool is16bit) {
  // integer and fractional part coordinates
  int32_t x0 = x >> 16;
  int32_t x1 = x0 + 1;
  if(is16bit) x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF

  int32_t dx0 = x & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  // gradient values for the two corners
  int32_t g0 = gradient1D(x0, dx0);
  int32_t g1 = gradient1D(x1, dx1);
  // interpolate and smooth function
  int32_t tx = smoothstep(dx0);
  int32_t noise = lerpPerlin(g0, g1, tx);
  return noise;
}

// 2D Perlin noise function that returns a value in range of -20633 to 20629
int32_t perlin2D_raw(uint32_t x, uint32_t y, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;

  if(is16bit) {
    x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF
    y1 = y1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;

  int32_t g00 = gradient2D(x0, dx0, y0, dy0);
  int32_t g10 = gradient2D(x1, dx1, y0, dy0);
  int32_t g01 = gradient2D(x0, dx0, y1, dy1);
  int32_t g11 = gradient2D(x1, dx1, y1, dy1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);

  int32_t nx0 = lerpPerlin(g00, g10, tx);
  int32_t nx1 = lerpPerlin(g01, g11, tx);

  int32_t noise = lerpPerlin(nx0, nx1, ty);
  return noise;
}

// 3D Perlin noise function that returns a value in range of -16788 to 16381
int32_t perlin3D_raw(uint32_t x, uint32_t y, uint32_t z, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t z0 = z >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;
  int32_t z1 = z0 + 1;

  if(is16bit) {
    x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF
    y1 = y1 & 0xFF;
    z1 = z1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dz0 = z & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;
  int32_t dz1 = dz0 - 0x10000;

  int32_t g000 = gradient3D(x0, dx0, y0, dy0, z0, dz0);
  int32_t g001 = gradient3D(x0, dx0, y0, dy0, z1, dz1);
  int32_t g010 = gradient3D(x0, dx0, y1, dy1, z0, dz0);
  int32_t g011 = gradient3D(x0, dx0, y1, dy1, z1, dz1);
  int32_t g100 = gradient3D(x1, dx1, y0, dy0, z0, dz0);
  int32_t g101 = gradient3D(x1, dx1, y0, dy0, z1, dz1);
  int32_t g110 = gradient3D(x1, dx1, y1, dy1, z0, dz0);
  int32_t g111 = gradient3D(x1, dx1, y1, dy1, z1, dz1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);
  uint32_t tz = smoothstep(dz0);

  int32_t nx0 = lerpPerlin(g000, g100, tx);
  int32_t nx1 = lerpPerlin(g010, g110, tx);
  int32_t nx2 = lerpPerlin(g001, g101, tx);
  int32_t nx3 = lerpPerlin(g011, g111, tx);
  int32_t ny0 = lerpPerlin(nx0, nx1, ty);
  int32_t ny1 = lerpPerlin(nx2, nx3, ty);

  int32_t noise = lerpPerlin(ny0, ny1, tz);
  return noise;
}

// scaling functions for fastled replacement
uint16_t perlin16(uint32_t x) {
  return ((perlin1D_raw(x) * 1159) >> 10) + 32803; //scale to 16bit and offset (fastled range: about 4838 to 60766)
}

uint16_t perlin16(uint32_t x, uint32_t y) {
 return ((perlin2D_raw(x, y) * 1537) >> 10) + 32725; //scale to 16bit and offset (fastled range: about 1748 to 63697)
}

uint16_t perlin16(uint32_t x, uint32_t y, uint32_t z) {
  return ((perlin3D_raw(x, y, z) * 1731) >> 10) + 33147; //scale to 16bit and offset (fastled range: about 4766 to 60840)
}

uint8_t perlin8(uint16_t x) {
  return (((perlin1D_raw((uint32_t)x << 8, true) * 1353) >> 10) + 32769) >> 8; //scale to 16 bit, offset, then scale to 8bit
}

uint8_t perlin8(uint16_t x, uint16_t y) {
  return (((perlin2D_raw((uint32_t)x << 8, (uint32_t)y << 8, true) * 1620) >> 10) + 32771) >> 8; //scale to 16 bit, offset, then scale to 8bit
}

uint8_t perlin8(uint16_t x, uint16_t y, uint16_t z) {
  return (((perlin3D_raw((uint32_t)x << 8, (uint32_t)y << 8, (uint32_t)z << 8, true) * 2015) >> 10) + 33168) >> 8; //scale to 16 bit, offset, then scale to 8bit
}