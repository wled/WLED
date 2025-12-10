#include "wled.h"
#include "fcn_declare.h"
#include "const.h"
#ifdef ESP8266
#include "user_interface.h" // for bootloop detection
#include <Hash.h>            // for SHA1 on ESP8266
#else
#include <Update.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
  #include "esp32/rtc.h"    // for bootloop detection
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(3, 3, 0)
  #include "soc/rtc.h"
#endif
#include "mbedtls/sha1.h"   // for SHA1 on ESP32
#include "esp_efuse.h"
#endif


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

  if (src == JSON_palette_names && mode > 255-customPalettes.size()) {
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

// PSRAM compile time checks to provide info for misconfigured env
#if defined(BOARD_HAS_PSRAM)
  #if defined(IDF_TARGET_ESP32C3) || defined(ESP8266)
    #error "ESP32-C3 and ESP8266 with PSRAM is not supported, please remove BOARD_HAS_PSRAM definition"
  #else
  #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3) // PSRAM fix only needed for classic esp32
    // BOARD_HAS_PSRAM also means that compiler flag "-mfix-esp32-psram-cache-issue" has to be used for old "rev.1" esp32
    #warning "BOARD_HAS_PSRAM defined, make sure to use -mfix-esp32-psram-cache-issue to prevent issues on rev.1 ESP32 boards \
              see https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html#esp32-rev-v1-0"
  #endif
  #endif
#else
  #if !defined(IDF_TARGET_ESP32C3) && !defined(ESP8266)
    #pragma message("BOARD_HAS_PSRAM not defined, not using PSRAM.")
  #endif
#endif

// memory allocation functions with minimum free heap size check
#ifdef ESP8266
static void *validateFreeHeap(void *buffer) {
  // make sure there is enough free heap left if buffer was allocated in DRAM region, free it if not
  // note: ESP826 needs very little contiguous heap for webserver, checking total free heap works better
  if (getFreeHeapSize() < MIN_HEAP_SIZE) {
    free(buffer);
    return nullptr;
  }
  return buffer;
}

void *d_malloc(size_t size) {
  // note: using "if (getContiguousFreeHeap() > MIN_HEAP_SIZE + size)" did perform worse in tests with regards to keeping heap healthy and UI working
  void *buffer = malloc(size);
  return validateFreeHeap(buffer);
}

void *d_calloc(size_t count, size_t size) {
  void *buffer = calloc(count, size);
  return validateFreeHeap(buffer);
}

// realloc with malloc fallback, note: on ESPS8266 there is no safe way to ensure MIN_HEAP_SIZE during realloc()s, free buffer and allocate new one
void *d_realloc_malloc(void *ptr, size_t size) {
  //void *buffer = realloc(ptr, size);
  //buffer = validateFreeHeap(buffer);
  //if (buffer) return buffer; // realloc successful
  //d_free(ptr); // free old buffer if realloc failed (or min heap was exceeded)
  //return d_malloc(size); // fallback to malloc
  free(ptr);
  return d_malloc(size);
}
#else
static void *validateFreeHeap(void *buffer) {
  // make sure there is enough free heap left if buffer was allocated in DRAM region, free it if not
  // TODO: between allocate and free, heap can run low (async web access), only IDF V5 allows for a pre-allocation-check of all free blocks
  if ((uintptr_t)buffer > SOC_DRAM_LOW && (uintptr_t)buffer < SOC_DRAM_HIGH && getContiguousFreeHeap() < MIN_HEAP_SIZE) {
    free(buffer);
    return nullptr;
  }
  return buffer;
}

void *d_malloc(size_t size) {
  void *buffer;
  #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
  // the newer ESP32 variants have byte-accessible fast RTC memory that can be used as heap, access speed is on-par with DRAM
  // the system does prefer normal DRAM until full, since free RTC memory is ~7.5k only, its below the minimum heap threshold and needs to be allocated explicitly
  // use RTC RAM for small allocations to improve fragmentation or if DRAM is running low
  if (size < 256 || getContiguousFreeHeap() < 2*MIN_HEAP_SIZE + size)
    buffer = heap_caps_malloc_prefer(size, 2, MALLOC_CAP_RTCRAM, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  else
  #endif
  buffer = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // allocate in any available heap memory
  buffer = validateFreeHeap(buffer); // make sure there is enough free heap left
  #ifdef BOARD_HAS_PSRAM
  if (!buffer)
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); // DRAM failed, use PSRAM if available
  #endif
  return buffer;
}

void *d_calloc(size_t count, size_t size) {
  void *buffer = d_malloc(count * size);
  if (buffer) memset(buffer, 0, count * size); // clear allocated buffer
  return buffer;
}

// realloc with malloc fallback, original buffer is freed if realloc fails but not copied!
void *d_realloc_malloc(void *ptr, size_t size) {
  void *buffer = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  buffer = validateFreeHeap(buffer);
  if (buffer) return buffer; // realloc successful
  d_free(ptr); // free old buffer if realloc failed (or min heap was exceeded)
  return d_malloc(size); // fallback to malloc
}

#ifdef BOARD_HAS_PSRAM
// p_xalloc: prefer PSRAM, use DRAM as fallback
void *p_malloc(size_t size) {
  void *buffer = heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return validateFreeHeap(buffer);
}

void *p_calloc(size_t count, size_t size) {
  void *buffer = p_malloc(count * size);
  if (buffer) memset(buffer, 0, count * size); // clear allocated buffer
  return buffer;
}

// realloc with malloc fallback, original buffer is freed if realloc fails but not copied!
void *p_realloc_malloc(void *ptr, size_t size) {
  void *buffer = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (buffer) return buffer; // realloc successful
  p_free(ptr); // free old buffer if realloc failed
  return p_malloc(size); // fallback to malloc
}
#endif
#endif

// allocation function for buffers like pixel-buffers and segment data
// optimises the use of memory types to balance speed and heap availability, always favours DRAM if possible
// if multiple conflicting types are defined, the lowest bits of "type" take priority (see fcn_declare.h for types)
void *allocate_buffer(size_t size, uint32_t type) {
  void *buffer = nullptr;
  #ifdef CONFIG_IDF_TARGET_ESP32
  // only classic ESP32 has "32bit accessible only" aka IRAM type. Using it frees up normal DRAM for other purposes
  // this memory region is used for IRAM_ATTR functions, whatever is left is unused and can be used for pixel buffers
  // prefer this type over PSRAM as it is slightly faster, except for _pixels where it is on-par as PSRAM-caching does a good job for mostly sequential access
  if (type & BFRALLOC_NOBYTEACCESS) {
    // prefer 32bit region, then PSRAM, fallback to any heap. Note: if adding "INTERNAL"-flag this wont work
    buffer = heap_caps_malloc_prefer(size, 3, MALLOC_CAP_32BIT, MALLOC_CAP_SPIRAM, MALLOC_CAP_8BIT);
    buffer = validateFreeHeap(buffer);
  }
  else
  #endif
  #if !defined(BOARD_HAS_PSRAM)
  buffer = d_malloc(size);
  #else
  if (type & BFRALLOC_PREFER_DRAM) {
    if (getContiguousFreeHeap() < 3*(MIN_HEAP_SIZE/2) + size && size > PSRAM_THRESHOLD)
      buffer = p_malloc(size); // prefer PSRAM for large allocations & when DRAM is low
    else
      buffer = d_malloc(size); // allocate in DRAM if enough free heap is available, PSRAM as fallback
  }
  else if (type & BFRALLOC_ENFORCE_DRAM)
    buffer = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // use DRAM only, otherwise return nullptr
  else if (type & BFRALLOC_PREFER_PSRAM) {
    // if DRAM is plenty, prefer it over PSRAM for speed, reserve enough DRAM for segment data: if MAX_SEGMENT_DATA is exceeded, always uses PSRAM
    if (getContiguousFreeHeap() > 4*MIN_HEAP_SIZE + size + ((uint32_t)(MAX_SEGMENT_DATA - Segment::getUsedSegmentData())))
      buffer = d_malloc(size);
    else
      buffer = p_malloc(size); // prefer PSRAM
  }
  else if (type & BFRALLOC_ENFORCE_PSRAM)
    buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); // use PSRAM only, otherwise return nullptr
  buffer = validateFreeHeap(buffer);
  #endif
  if (buffer && (type & BFRALLOC_CLEAR))
    memset(buffer, 0, size); // clear allocated buffer
  /*
  #if !defined(ESP8266) && defined(WLED_DEBUG)
  if (buffer) {
    DEBUG_PRINTF_P(PSTR("*Buffer allocated: size:%d, address:%p"), size, (uintptr_t)buffer);
    if ((uintptr_t)buffer > SOC_DRAM_LOW && (uintptr_t)buffer < SOC_DRAM_HIGH)
      DEBUG_PRINTLN(F(" in DRAM"));
    #ifndef CONFIG_IDF_TARGET_ESP32C3
    else if ((uintptr_t)buffer > SOC_EXTRAM_DATA_LOW && (uintptr_t)buffer < SOC_EXTRAM_DATA_HIGH)
      DEBUG_PRINTLN(F(" in PSRAM"));
    #endif
    #ifdef CONFIG_IDF_TARGET_ESP32
    else if ((uintptr_t)buffer > SOC_IRAM_LOW && (uintptr_t)buffer < SOC_IRAM_HIGH)
      DEBUG_PRINTLN(F(" in IRAM"));   // only used on ESP32 (MALLOC_CAP_32BIT)
    #else
    else if ((uintptr_t)buffer > SOC_RTC_DRAM_LOW && (uintptr_t)buffer < SOC_RTC_DRAM_HIGH)
      DEBUG_PRINTLN(F(" in RTCRAM")); // not available on ESP32
    #endif
    else
      DEBUG_PRINTLN(F(" in ???")); // unknown (check soc.h for other memory regions)
  } else
    DEBUG_PRINTF_P(PSTR("Buffer allocation failed: size:%d\n"), size);
  #endif 
  */
  return buffer;
}

// bootloop detection and handling
// checks if the ESP reboots multiple times due to a crash or watchdog timeout
// if a bootloop is detected: restore settings from backup, then reset settings, then switch boot image (and repeat)

#define BOOTLOOP_INTERVAL_MILLIS 120000  // time limit between crashes: 120 seconds (2 minutes)
#define BOOTLOOP_THRESHOLD       5     // number of consecutive crashes to trigger bootloop detection
#define BOOTLOOP_ACTION_RESTORE  0     // default action: restore config from /bkp.cfg.json
#define BOOTLOOP_ACTION_RESET    1     // if restore does not work, reset config (rename /cfg.json to /rst.cfg.json)
#define BOOTLOOP_ACTION_OTA      2     // swap the boot partition
#define BOOTLOOP_ACTION_DUMP     3     // nothing seems to help, dump files to serial and reboot (until hardware reset)

// Platform-agnostic abstraction
enum class ResetReason {
  Power,
  Software,
  Crash,
  Brownout
};

#ifdef ESP8266
// Place variables in RTC memory via references, since RTC memory is not exposed via the linker in the Non-OS SDK
// Use an offset of 32 as there's some hints that the first 128 bytes of "user" memory are used by the OTA system
// Ref: https://github.com/esp8266/Arduino/blob/78d0d0aceacc1553f45ad8154592b0af22d1eede/cores/esp8266/Esp.cpp#L168
static volatile uint32_t& bl_last_boottime = *(RTC_USER_MEM + 32);
static volatile uint32_t& bl_crashcounter = *(RTC_USER_MEM + 33);
static volatile uint32_t& bl_actiontracker = *(RTC_USER_MEM + 34);

static inline ResetReason rebootReason() {
  uint32_t resetReason = system_get_rst_info()->reason;
  if (resetReason == REASON_EXCEPTION_RST
      || resetReason == REASON_WDT_RST
      || resetReason == REASON_SOFT_WDT_RST)
      return ResetReason::Crash;
  if (resetReason == REASON_SOFT_RESTART)
    return ResetReason::Software;
  return ResetReason::Power;
}

static inline uint32_t getRtcMillis() { return system_get_rtc_time() / 160; };  // rtc ticks ~160000Hz

#else
// variables in RTC_NOINIT memory persist between reboots (but not on hardware reset)
RTC_NOINIT_ATTR static uint32_t bl_last_boottime;
RTC_NOINIT_ATTR static uint32_t bl_crashcounter;
RTC_NOINIT_ATTR static uint32_t bl_actiontracker;

static inline ResetReason rebootReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_BROWNOUT) return ResetReason::Brownout;
  if (reason == ESP_RST_SW) return ResetReason::Software;
  if (reason == ESP_RST_PANIC || reason == ESP_RST_WDT || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT) return ResetReason::Crash;
  return ResetReason::Power;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
static inline uint32_t getRtcMillis() { return esp_rtc_get_time_us() / 1000; }
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(3, 3, 0)
static inline uint32_t getRtcMillis() { return rtc_time_slowclk_to_us(rtc_time_get(), rtc_clk_slow_freq_get_hz()) / 1000; }
#endif

void bootloopCheckOTA() { bl_actiontracker = BOOTLOOP_ACTION_OTA; } // swap boot image if bootloop is detected instead of restoring config

#endif

// detect bootloop by checking the reset reason and the time since last boot
static bool detectBootLoop() {
  uint32_t rtctime = getRtcMillis();
  bool result = false;

  switch(rebootReason()) {
    case ResetReason::Power:
      bl_actiontracker = BOOTLOOP_ACTION_RESTORE; // init action tracker if not an intentional reboot (e.g. from OTA or bootloop handler)
      // fall through
    case ResetReason::Software:
      // no crash detected, reset counter
      bl_crashcounter = 0;
      break;

    case ResetReason::Crash:
    {
      DEBUG_PRINTLN(F("crash detected!"));
      uint32_t rebootinterval = rtctime - bl_last_boottime;
      if (rebootinterval < BOOTLOOP_INTERVAL_MILLIS) {
        bl_crashcounter++;
        if (bl_crashcounter >= BOOTLOOP_THRESHOLD) {
          DEBUG_PRINTLN(F("!BOOTLOOP DETECTED!"));
          bl_crashcounter = 0;  
          if(bl_actiontracker > BOOTLOOP_ACTION_DUMP) bl_actiontracker = BOOTLOOP_ACTION_RESTORE; // reset action tracker if out of bounds
          result = true;
        }
      } else {
        // Reset counter on long intervals to track only consecutive short-interval crashes
        bl_crashcounter = 0;
        // TODO: crash reporting goes here
      }
      break;
    }

    case ResetReason::Brownout:
      // crash due to brownout can't be detected unless using flash memory to store bootloop variables
      DEBUG_PRINTLN(F("brownout detected"));
      //restoreConfig(); // TODO: blindly restoring config if brownout detected is a bad idea, need a better way (if at all)
      break;
  }

  bl_last_boottime = rtctime; // store current runtime for next reboot

  return result;
}

void handleBootLoop() {
  DEBUG_PRINTF_P(PSTR("checking for bootloop: time %d, counter %d, action %d\n"), bl_last_boottime, bl_crashcounter, bl_actiontracker);
  if (!detectBootLoop()) return; // no bootloop detected

  switch(bl_actiontracker) {
    case BOOTLOOP_ACTION_RESTORE:
      restoreConfig();
      ++bl_actiontracker;
      break;
    case BOOTLOOP_ACTION_RESET:
      resetConfig();
      ++bl_actiontracker;
      break;
    case BOOTLOOP_ACTION_OTA:
#ifndef ESP8266
      if(Update.canRollBack()) {
        DEBUG_PRINTLN(F("Swapping boot partition..."));
        Update.rollBack(); // swap boot partition
      }
      ++bl_actiontracker;
      break;
#else
      // fall through
#endif
    case BOOTLOOP_ACTION_DUMP:
      dumpFilesToSerial();
      break;
  }

  ESP.restart(); // restart cleanly and don't wait for another crash
}

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

// Platform-agnostic SHA1 computation from String input
String computeSHA1(const String& input) {
  #ifdef ESP8266
    return sha1(input); // ESP8266 has built-in sha1() function
  #else
    // ESP32: Compute SHA1 hash using mbedtls
    unsigned char shaResult[20]; // SHA1 produces 20 bytes
    mbedtls_sha1_context ctx;

    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts_ret(&ctx);
    mbedtls_sha1_update_ret(&ctx, (const unsigned char*)input.c_str(), input.length());
    mbedtls_sha1_finish_ret(&ctx, shaResult);
    mbedtls_sha1_free(&ctx);

    // Convert to hexadecimal string
    char hexString[41];
    for (int i = 0; i < 20; i++) {
      sprintf(&hexString[i*2], "%02x", shaResult[i]);
    }
    hexString[40] = '\0';

    return String(hexString);
  #endif
}

#ifdef ESP32
#include "esp_adc_cal.h"
String generateDeviceFingerprint() {
  uint32_t fp[2] = {0, 0}; // create 64 bit fingerprint
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  esp_efuse_mac_get_default((uint8_t*)fp);
  fp[1] ^= ESP.getFlashChipSize();
  fp[0] ^= chip_info.full_revision | (chip_info.model << 16);
  // mix in ADC calibration data:
  esp_adc_cal_characteristics_t ch;
  #if SOC_ADC_MAX_BITWIDTH == 13 // S2 has 13 bit ADC
  constexpr auto myBIT_WIDTH = ADC_WIDTH_BIT_13;
  #else
  constexpr auto myBIT_WIDTH = ADC_WIDTH_BIT_12;
  #endif
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, myBIT_WIDTH, 1100, &ch);
  fp[0] ^= ch.coeff_a;
  fp[1] ^= ch.coeff_b;
  if (ch.low_curve) {
    for (int i = 0; i < 8; i++) {
      fp[0] ^= ch.low_curve[i];
    }
  }
  if (ch.high_curve) {
    for (int i = 0; i < 8; i++) {
      fp[1] ^= ch.high_curve[i];
    }
  }
  char fp_string[17];  // 16 hex chars + null terminator
  sprintf(fp_string, "%08X%08X", fp[1], fp[0]);
  return String(fp_string);
}
#else // ESP8266
String generateDeviceFingerprint() {
  uint32_t fp[2] = {0, 0}; // create 64 bit fingerprint
  WiFi.macAddress((uint8_t*)&fp); // use MAC address as fingerprint base
  fp[0] ^= ESP.getFlashChipId();
  fp[1] ^= ESP.getFlashChipSize() | ESP.getFlashChipVendorId() << 16;
  char fp_string[17];  // 16 hex chars + null terminator
  sprintf(fp_string, "%08X%08X", fp[1], fp[0]);
  return String(fp_string);
}
#endif

// Generate a device ID based on SHA1 hash of MAC address salted with other unique device info
// Returns: original SHA1 + last 2 chars of double-hashed SHA1 (42 chars total)
String getDeviceId() {
  static String cachedDeviceId = "";
  if (cachedDeviceId.length() > 0) return cachedDeviceId;
  // The device string is deterministic as it needs to be consistent for the same device, even after a full flash erase
  // MAC is salted with other consistent device info to avoid rainbow table attacks.
  // If the MAC address is known by malicious actors, they could precompute SHA1 hashes to impersonate devices,
  // but as WLED developers are just looking at statistics and not authenticating devices, this is acceptable.
  // If the usage data was exfiltrated, you could not easily determine the MAC from the device ID without brute forcing SHA1

  String firstHash = computeSHA1(generateDeviceFingerprint());

  // Second hash: SHA1 of the first hash
  String secondHash = computeSHA1(firstHash);

  // Concatenate first hash + last 2 chars of second hash
  cachedDeviceId = firstHash + secondHash.substring(38);

  return cachedDeviceId;
}

