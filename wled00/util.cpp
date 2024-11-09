#include "wled.h"
#include "fcn_declare.h"
#include "const.h"


//helper to get int value at a position in string
int getNumVal(const String* req, uint16_t pos)
{
  return req->substring(pos+3).toInt();
}


//helper to get int value with in/decrementing support via ~ syntax
void parseNumber(const char* str, byte* val, byte minv, byte maxv)
{
  if (str == nullptr || str[0] == '\0') return;
  if (str[0] == 'r') {*val = random8(minv,maxv?maxv:255); return;} // maxv for random cannot be 0
  bool wrap = false;
  if (str[0] == 'w' && strlen(str) > 1) {str++; wrap = true;}
  if (str[0] == '~') {
    int out = atoi(str +1);
    if (out == 0) {
      if (str[1] == '0') return;
      if (str[1] == '-') {
        *val = (int)(*val -1) < (int)minv ? maxv : min((int)maxv,(*val -1)); //-1, wrap around
      } else {
        *val = (int)(*val +1) > (int)maxv ? minv : max((int)minv,(*val +1)); //+1, wrap around
      }
    } else {
      if (wrap && *val == maxv && out > 0) out = minv;
      else if (wrap && *val == minv && out < 0) out = maxv;
      else {
        out += *val;
        if (out > maxv) out = maxv;
        if (out < minv) out = minv;
      }
      *val = out;
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
  *val = atoi(str);
}


bool getVal(JsonVariant elem, byte* val, byte vmin, byte vmax) {
  if (elem.is<int>()) {
		if (elem < 0) return false; //ignore e.g. {"ps":-1}
    *val = elem;
    return true;
  } else if (elem.is<const char*>()) {
    const char* str = elem;
    size_t len = strnlen(str, 12);
    if (len == 0 || len > 10) return false;
    parseNumber(str, val, vmin, vmax);
    return true;
  }
  return false; //key does not exist
}


bool getBoolVal(JsonVariant elem, bool dflt) {
  if (elem.is<const char*>() && elem.as<const char*>()[0] == 't') {
    return !dflt;
  } else {
    return elem | dflt;
  }
}


bool updateVal(const char* req, const char* key, byte* val, byte minv, byte maxv)
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


//threading/network callback details: https://github.com/Aircoookie/WLED/pull/2336#discussion_r762276994
bool requestJSONBufferLock(uint8_t module)
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
    DEBUG_PRINTF_P(PSTR("ERROR: Locking JSON buffer (%d) failed! (still locked by %d)\n"), module, jsonBufferLock);
#ifdef ARDUINO_ARCH_ESP32
    xSemaphoreGiveRecursive(jsonBufferLockMutex);
#endif
    return false;
  }

  jsonBufferLock = module ? module : 255;
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
      unsigned start = lineBuffer.indexOf('@');
      unsigned stop  = lineBuffer.indexOf(';', start);
      if (start>0 && stop>0) {
        String names = lineBuffer.substring(start, stop); // include @
        unsigned nameBegin = 1, nameEnd, nameDefault;
        if (slider < 10) {
          for (size_t i=0; i<=slider; i++) {
            const char *tmpstr;
            dest[0] = '\0'; //clear dest buffer
            if (nameBegin == 0) break; // there are no more names
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


///////////////////////////////////////////////////////////////////////////////
// Begin simulateSound (to enable audio enhanced effects to display something)
///////////////////////////////////////////////////////////////////////////////
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
      for (int i = 0; i<16; i++) fftResult[i] = beatsin8(120 / (i+1), 0, 255);
      volumeSmth = fftResult[8];
      break;
    case UMS_WeWillRockYou:
      if (ms%2000 < 200) {
        volumeSmth = random8(255);
        for (int i = 0; i<5; i++) fftResult[i] = random8(255);
      } else if (ms%2000 < 400) {
        volumeSmth = 0;
        for (int i = 0; i<16; i++) fftResult[i] = 0;
      } else if (ms%2000 < 600) {
        volumeSmth = random8(255);
        for (int i = 5; i<11; i++) fftResult[i] = random8(255);
      } else if (ms%2000 < 800) {
        volumeSmth = 0;
        for (int i = 0; i<16; i++) fftResult[i] = 0;
      } else if (ms%2000 < 1000) {
        volumeSmth = random8(255);
        for (int i = 11; i<16; i++) fftResult[i] = random8(255);
      } else {
        volumeSmth = 0;
        for (int i = 0; i<16; i++) fftResult[i] = 0;
      }
      break;
    case UMS_10_13:
      for (int i = 0; i<16; i++) fftResult[i] = inoise8(beatsin8(90 / (i+1), 0, 200)*15 + (ms>>10), ms>>3);
      volumeSmth = fftResult[8];
      break;
    case UMS_14_3:
      for (int i = 0; i<16; i++) fftResult[i] = inoise8(beatsin8(120 / (i+1), 10, 30)*10 + (ms>>14), ms>>3);
      volumeSmth = fftResult[8];
      break;
  }

  samplePeak    = random8() > 250;
  FFT_MajorPeak = 21.0f + (volumeSmth*volumeSmth) / 8.0f; // walk through full range of 21hz...8200hz
  maxVol        = 31;  // this gets feedback from UI
  binNum        = 8;   // this gets feedback from UI
  volumeRaw     = volumeSmth;
  my_magnitude  = 10000.0f / 8.0f; //no idea if 10000 is a good value for FFT_Magnitude ???
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
      delete[] ledmapNames[i-1];
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
              ledmapNames[i-1] = new char[len+1];
              if (ledmapNames[i-1]) strlcpy(ledmapNames[i-1], name, 33);
            }
          }
          if (!ledmapNames[i-1]) {
            char tmp[33];
            snprintf_P(tmp, 32, s_ledmap_tmpl, i);
            len = strlen(tmp);
            ledmapNames[i-1] = new char[len+1];
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
    r = random8();
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
