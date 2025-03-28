#include "wled.h"

#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

class UsermodSSDR : public Usermod {

//#define REFRESHTIME 497

private:
  //Runtime variables.
  unsigned long umSSDRLastRefresh = 0;
  unsigned long umSSDRResfreshTime = 3000;
  uint16_t umSSDRLength = 0;

  // Configurable settings for the SSDR Usermod
  // Enabled usermod
  #ifndef umSSDR_ENABLED
	  #define umSSDR_ENABLED false
  #endif

  #ifndef umSSDR_ENABLE_AUTO_BRIGHTNESS
	  #define umSSDR_ENABLE_AUTO_BRIGHTNESS false
  #endif

  #ifndef umSSDR_BRIGHTNESS_MIN
	  #define umSSDR_BRIGHTNESS_MIN 0
  #endif

  #ifndef umSSDR_BRIGHTNESS_MAX
	  #define umSSDR_BRIGHTNESS_MAX 255
  #endif

  #ifndef umSSDR_INVERT_AUTO_BRIGHTNESS
	  #define umSSDR_INVERT_AUTO_BRIGHTNESS false
  #endif

  #ifndef umSSDR_LUX_MIN
	  #define umSSDR_LUX_MIN 0
  #endif

  #ifndef umSSDR_LUX_MAX
	  #define umSSDR_LUX_MAX 1000
  #endif

  #ifndef umSSDR_INVERTED
	  #define umSSDR_INVERTED false
  #endif

  #ifndef umSSDR_COLONBLINK
	  #define umSSDR_COLONBLINK true
  #endif

  #ifndef umSSDR_LEADING_ZERO
	  #define umSSDR_LEADING_ZERO false
  #endif

  // Display mask for physical layout
  /*//  H - 00-23 hours
    //  h - 01-12 hours
    //  k - 01-24 hours
    //  m - 00-59 minutes
    //  s - 00-59 seconds
    //  d - 01-31 day of month
    //  M - 01-12 month
    //  y - 21 last two positions of year
    //  Y - 2021 year
    //  L - Light LED
    //  This option defines a separate LED (or set of LEDs) that can be controlled independently 
    //  from the other clock display LEDs. It can be switched on or off to provide additional 
    //  lighting for the display or serve as an ambient light source, without affecting the 
    //  clock's visual display of time and date.
    //  : for a colon
    */
  
  #ifndef umSSDR_DISPLAY_MASK
    #define umSSDR_DISPLAY_MASK "H:m"
  #endif

  #ifndef umSSDR_HOURS
    #define umSSDR_HOURS ""
  #endif

  #ifndef umSSDR_MINUTES
    #define umSSDR_MINUTES ""
  #endif

  #ifndef umSSDR_SECONDS
    #define umSSDR_SECONDS ""
  #endif

  #ifndef umSSDR_COLONS
    #define umSSDR_COLONS ""
  #endif

  #ifndef umSSDR_LIGHT
    #define umSSDR_LIGHT ""
  #endif

  #ifndef umSSDR_DAYS
    #define umSSDR_DAYS ""
  #endif

  #ifndef umSSDR_MONTHS
    #define umSSDR_MONTHS ""
  #endif

  #ifndef umSSDR_YEARS
    #define umSSDR_YEARS ""
  #endif
  
  #ifndef umSSDR_NUMBERS
    /* Segment order, seen from the front:
          <  A  >
        /\       /\
        F        B
        \/       \/
          <  G  >
        /\       /\
        E        C
        \/       \/
          <  D  >
    */
    uint8_t umSSDRNumbers[11][7] = {
        //  A    B    C    D    E    F    G
        {   1,   1,   1,   1,   1,   1,   0 },  // 0
        {   0,   1,   1,   0,   0,   0,   0 },  // 1
        {   1,   1,   0,   1,   1,   0,   1 },  // 2
        {   1,   1,   1,   1,   0,   0,   1 },  // 3
        {   0,   1,   1,   0,   0,   1,   1 },  // 4
        {   1,   0,   1,   1,   0,   1,   1 },  // 5
        {   1,   0,   1,   1,   1,   1,   1 },  // 6
        {   1,   1,   1,   0,   0,   0,   0 },  // 7
        {   1,   1,   1,   1,   1,   1,   1 },  // 8
        {   1,   1,   1,   1,   0,   1,   1 },  // 9
        {   0,   0,   0,   0,   0,   0,   0 }   // blank
    };
  #else
	  uint8_t umSSDRNumbers[11][10] = umSSDR_NUMBERS;
  #endif

  bool umSSDRDisplayTime = umSSDR_ENABLED;
  bool umSSDREnableLDR = umSSDR_ENABLE_AUTO_BRIGHTNESS;
  uint16_t umSSDRBrightnessMin = umSSDR_BRIGHTNESS_MIN;
  uint16_t umSSDRBrightnessMax = umSSDR_BRIGHTNESS_MAX;
  bool umSSDRInvertAutoBrightness = umSSDR_INVERT_AUTO_BRIGHTNESS;
  uint16_t umSSDRLuxMin = umSSDR_LUX_MIN;
  uint16_t umSSDRLuxMax = umSSDR_LUX_MAX;
  bool umSSDRInverted = umSSDR_INVERTED;
  bool umSSDRColonblink = umSSDR_COLONBLINK;
  bool umSSDRLeadingZero = umSSDR_LEADING_ZERO;
  String umSSDRDisplayMask = umSSDR_DISPLAY_MASK;
  String umSSDRHours = umSSDR_HOURS;
  String umSSDRMinutes = umSSDR_MINUTES;
  String umSSDRSeconds = umSSDR_SECONDS;
  String umSSDRColons = umSSDR_COLONS;
  String umSSDRLight = umSSDR_LIGHT;
  String umSSDRDays = umSSDR_DAYS;
  String umSSDRMonths = umSSDR_MONTHS;
  String umSSDRYears = umSSDR_YEARS;

  bool* umSSDRMask = 0;
  bool disableUmLedControl = false;
  
  //String to reduce flash memory usage
  static const char _str_name[];
  static const char _str_timeEnabled[];
  static const char _str_ldrEnabled[];
  static const char _str_minBrightness[];
  static const char _str_maxBrightness[];
  static const char _str_luxMin[];
  static const char _str_luxMax[];
  static const char _str_invertAutoBrightness[];
  static const char _str_inverted[];
  static const char _str_colonblink[];
  static const char _str_leadingZero[];
  static const char _str_displayMask[];
  static const char _str_hours[];
  static const char _str_minutes[];
  static const char _str_seconds[];
  static const char _str_colons[];
  static const char _str_light[];
  static const char _str_days[];
  static const char _str_months[];
  static const char _str_years[];

#ifdef USERMOD_SN_PHOTORESISTOR
  Usermod_SN_Photoresistor *ptr;
#else
  void* ptr = nullptr;
#endif
#ifdef USERMOD_BH1750
  Usermod_BH1750* bh1750 = nullptr;
#else
  void* bh1750 = nullptr;
#endif

  void _overlaySevenSegmentDraw() {
    int displayMaskLen = static_cast<int>(umSSDRDisplayMask.length());
    bool colonsDone = false;
    bool lightDone = false;
    _setAllFalse();
    for (int index = 0; index < displayMaskLen; index++) {
      int timeVar = 0;
      switch (umSSDRDisplayMask[index]) {
        case 'h':
          timeVar = hourFormat12(localTime);
          _showElements(&umSSDRHours, timeVar, 0, !umSSDRLeadingZero);
          break;
        case 'H':
          timeVar = hour(localTime);
          _showElements(&umSSDRHours, timeVar, 0, !umSSDRLeadingZero);
          break;
        case 'k':
          timeVar = hour(localTime) + 1;
          _showElements(&umSSDRHours, timeVar, 0, !umSSDRLeadingZero);
          break;
        case 'm':
          timeVar = minute(localTime);
          _showElements(&umSSDRMinutes, timeVar, 0, 0);
          break;
        case 's':
          timeVar = second(localTime);
          _showElements(&umSSDRSeconds, timeVar, 0, 0);
          break;
        case 'd':
          timeVar = day(localTime);
          _showElements(&umSSDRDays, timeVar, 0, 0);
          break;
        case 'M':
          timeVar = month(localTime);
          _showElements(&umSSDRMonths, timeVar, 0, 0);
          break;
        case 'y':
          timeVar = second(localTime);
          _showElements(&umSSDRYears, timeVar, 0, 0);
          break;
        case 'Y':
          timeVar = year(localTime);
          _showElements(&umSSDRYears, timeVar, 0, 0);
          break;
        case ':':
          if (!colonsDone) { // only call _setColons once as all colons are printed when the first colon is found
            _setColons();
            colonsDone = true;
          }
          break;
        case 'L':
          if (!lightDone) { // only call _showElements once
            _showElements(&umSSDRLight, 0, 1, 0);
            lightDone = true;
          }
          break;
      }
    }
    _setMaskToLeds();
  }

  void _setColons() {
    if ( umSSDRColonblink ) {
      if ( second(localTime) % 2 == 0 ) {
        _showElements(&umSSDRColons, 0, 1, 0);
      }
    } else {
      _showElements(&umSSDRColons, 0, 1, 0);
    }
  }

  void _showElements(String *map, int timevar, bool isColon, bool removeZero) {
    if ((map != nullptr) && (*map != nullptr) && !(*map).equals("")) {
      int length = String(timevar).length();
      bool addZero = false;
      if (length == 1) {
        length = 2;
        addZero = true;
      }
      int timeArr[length];
      if(addZero) {
        if(removeZero)
          {
            timeArr[0] = timevar;
            timeArr[1] = 10;
          }
        else
        {
          timeArr[0] = timevar;
          timeArr[1] = 0;
        }
      } else {
        int count = 0;
        while (timevar) {
          timeArr[count] = timevar%10;
          timevar /= 10;
          count++;
        };
      }

      int colonsLen = static_cast<int>((*map).length());
      int count = 0;
      int countSegments = 0;
      int countDigit = 0;
      bool range = false;
      int lastSeenLedNr = 0;

      for (int index = 0; index < colonsLen; index++) {
        switch ((*map)[index]) {
          case '-':
            lastSeenLedNr = _checkForNumber(count, index, map);
            count = 0;
            range = true;
            break;
          case ':':
            _setLeds(_checkForNumber(count, index, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
            count = 0;
            range = false;
            countDigit++;
            countSegments = 0;
            break;
          case ';':
            _setLeds(_checkForNumber(count, index, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
            count = 0;
            range = false;
            countSegments++;
            break;
          case ',':
            _setLeds(_checkForNumber(count, index, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
            count = 0;
            range = false;
            break;
          default:
            count++;
            break;
        }
      }
      _setLeds(_checkForNumber(count, colonsLen, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
    }
  }

  void _setLeds(int lednr, int lastSeenLedNr, bool range, int countSegments, int number, bool colon) {
    if ((lednr < 0) || (lednr >= umSSDRLength)) return;                                   // prevent array bounds violation

    if (!(colon && umSSDRColonblink) && ((number < 0) || (countSegments < 0))) return;
    if ((colon && umSSDRColonblink) || umSSDRNumbers[number][countSegments]) {
      
      if (range) {
        for(int i = max(0, lastSeenLedNr); i <= lednr; i++) {
          umSSDRMask[i] = true;
        }
      } else {
        umSSDRMask[lednr] = true;
      }
    }
  }

  void _setMaskToLeds() {
    for(int i = 0; i <= umSSDRLength; i++) {
      if ((!umSSDRInverted && !umSSDRMask[i]) || (umSSDRInverted && umSSDRMask[i])) {
        strip.setPixelColor(i, 0x000000);
      }
    }
  }

  void _setAllFalse() {
    for(int i = 0; i <= umSSDRLength; i++) {
      umSSDRMask[i] = false;
    }
  }

  int _checkForNumber(int count, int index, String *map) {
    String number = (*map).substring(index - count, index);
    return number.toInt();
  }

  void _publishMQTTint_P(const char *subTopic, int value)
  {
    #ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      if(mqtt == NULL) return;
        
      char buffer[64];
      char valBuffer[12];
      sprintf_P(buffer, PSTR("%s/%S/%S"), mqttDeviceTopic, _str_name, subTopic);
      sprintf_P(valBuffer, PSTR("%d"), value);
      mqtt->publish(buffer, 2, true, valBuffer);
    }
    #endif
  }

  void _publishMQTTstr_P(const char *subTopic, String Value)
  {
    #ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      if(mqtt == NULL) return;
      char buffer[64];
      sprintf_P(buffer, PSTR("%s/%S/%S"), mqttDeviceTopic, _str_name, subTopic);
      mqtt->publish(buffer, 2, true, Value.c_str(), Value.length());
    }
    #endif
  }

  bool _cmpIntSetting_P(char *topic, char *payload, const char *setting, void *value)
  {
    if (strcmp_P(topic, setting) == 0)
    {
      *((int *)value) = strtol(payload, NULL, 10);
      _publishMQTTint_P(setting, *((int *)value));
      return true;
    }
    return false;
  }

  bool _handleSetting(char *topic, char *payload) {
    if (_cmpIntSetting_P(topic, payload, _str_timeEnabled, &umSSDRDisplayTime)) {
      return true;
    }
    if (_cmpIntSetting_P(topic, payload, _str_ldrEnabled, &umSSDREnableLDR)) {
      return true;
    }
    if (_cmpIntSetting_P(topic, payload, _str_inverted, &umSSDRInverted)) {
      return true;
    }
    if (_cmpIntSetting_P(topic, payload, _str_colonblink, &umSSDRColonblink)) {
      return true;
    }
    if (_cmpIntSetting_P(topic, payload, _str_leadingZero, &umSSDRLeadingZero)) {
      return true;
    }
    if (strcmp_P(topic, _str_displayMask) == 0) {
      umSSDRDisplayMask = String(payload);
      _publishMQTTstr_P(_str_displayMask, umSSDRDisplayMask);
      return true;
    }
    return false;
  }

  void _updateMQTT()
  {
    _publishMQTTint_P(_str_timeEnabled, umSSDRDisplayTime);
    _publishMQTTint_P(_str_ldrEnabled, umSSDREnableLDR);
    _publishMQTTint_P(_str_minBrightness, umSSDRBrightnessMin);
    _publishMQTTint_P(_str_maxBrightness, umSSDRBrightnessMax);
    _publishMQTTint_P(_str_luxMin, umSSDRLuxMin);
    _publishMQTTint_P(_str_luxMax, umSSDRLuxMax);
    _publishMQTTint_P(_str_invertAutoBrightness, umSSDRInvertAutoBrightness);
    _publishMQTTint_P(_str_inverted, umSSDRInverted);
    _publishMQTTint_P(_str_colonblink, umSSDRColonblink);
    _publishMQTTint_P(_str_leadingZero, umSSDRLeadingZero);

    _publishMQTTstr_P(_str_hours, umSSDRHours);
    _publishMQTTstr_P(_str_minutes, umSSDRMinutes);
    _publishMQTTstr_P(_str_seconds, umSSDRSeconds);
    _publishMQTTstr_P(_str_colons, umSSDRColons);
    _publishMQTTstr_P(_str_light, umSSDRLight);
    _publishMQTTstr_P(_str_days, umSSDRDays);
    _publishMQTTstr_P(_str_months, umSSDRMonths);
    _publishMQTTstr_P(_str_years, umSSDRYears);
    _publishMQTTstr_P(_str_displayMask, umSSDRDisplayMask);
  }

  void _addJSONObject(JsonObject& root) {
    JsonObject ssdrObj = root[FPSTR(_str_name)];
    if (ssdrObj.isNull()) {
      ssdrObj = root.createNestedObject(FPSTR(_str_name));
    }

    ssdrObj[FPSTR(_str_timeEnabled)] = umSSDRDisplayTime;
    ssdrObj[FPSTR(_str_ldrEnabled)] = umSSDREnableLDR;
    ssdrObj[FPSTR(_str_minBrightness)] = umSSDRBrightnessMin;
    ssdrObj[FPSTR(_str_maxBrightness)] = umSSDRBrightnessMax;
    ssdrObj[FPSTR(_str_luxMin)] = umSSDRLuxMin;
    ssdrObj[FPSTR(_str_luxMax)] = umSSDRLuxMax;
    ssdrObj[FPSTR(_str_invertAutoBrightness)] = umSSDRInvertAutoBrightness;
    ssdrObj[FPSTR(_str_inverted)] = umSSDRInverted;
    ssdrObj[FPSTR(_str_colonblink)] = umSSDRColonblink;
    ssdrObj[FPSTR(_str_leadingZero)] = umSSDRLeadingZero;
    ssdrObj[FPSTR(_str_displayMask)] = umSSDRDisplayMask;
    ssdrObj[FPSTR(_str_hours)] = umSSDRHours;
    ssdrObj[FPSTR(_str_minutes)] = umSSDRMinutes;
    ssdrObj[FPSTR(_str_seconds)] = umSSDRSeconds;
    ssdrObj[FPSTR(_str_colons)] = umSSDRColons;
    ssdrObj[FPSTR(_str_light)] = umSSDRLight;
    ssdrObj[FPSTR(_str_days)] = umSSDRDays;
    ssdrObj[FPSTR(_str_months)] = umSSDRMonths;
    ssdrObj[FPSTR(_str_years)] = umSSDRYears;
  }

public:
  //Functions called by WLED

  /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
  void setup() {
    umSSDRLength = strip.getLengthTotal();
    if (umSSDRMask != 0) {
      umSSDRMask = (bool*) realloc(umSSDRMask, umSSDRLength * sizeof(bool));
    } else {
      umSSDRMask = (bool*) malloc(umSSDRLength * sizeof(bool));
    }
    _setAllFalse();

    #ifdef USERMOD_SN_PHOTORESISTOR
      ptr = (Usermod_SN_Photoresistor*) UsermodManager::lookup(USERMOD_ID_SN_PHOTORESISTOR);
    #endif
    #ifdef USERMOD_BH1750
      bh1750 = (Usermod_BH1750*) UsermodManager::lookup(USERMOD_ID_BH1750);
    #endif
    DEBUG_PRINTLN(F("Setup done"));
  }

  /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     */
  void loop() {
    if (!umSSDRDisplayTime || strip.isUpdating()) {
      return;
    }
    #if defined(USERMOD_SN_PHOTORESISTOR) || defined(USERMOD_BH1750)
      if(bri != 0 && umSSDREnableLDR && (millis() - umSSDRLastRefresh > umSSDRResfreshTime) && !disableUmLedControl) {
        float lux = -1; // Initialize lux with an invalid value
	  
        #ifdef USERMOD_SN_PHOTORESISTOR
        if (ptr != nullptr) {
          lux = ptr->getLastLDRValue();
        }
        #endif

        #ifdef USERMOD_BH1750
        if (bh1750 != nullptr) {
          lux = bh1750->getIlluminance();
        }
        #endif

        if (lux >= 0) { // Ensure we got a valid lux value
          uint16_t brightness;
			    if (!umSSDRInvertAutoBrightness) {
            brightness = map(lux, umSSDRLuxMin, umSSDRLuxMax, umSSDRBrightnessMin, umSSDRBrightnessMax);
          } else {
            brightness = map(lux, umSSDRLuxMin, umSSDRLuxMax, umSSDRBrightnessMax, umSSDRBrightnessMin);
          }

          if (bri != brightness) {
            DEBUG_PRINTF("Adjusting brightness based on lux value: %.2f lx, new brightness: %d\n", lux, brightness);
            bri = brightness;
            stateUpdated(1);
          }
        }
        umSSDRLastRefresh = millis();
      }
    #endif
  }

  void handleOverlayDraw() {
    if (umSSDRDisplayTime && !disableUmLedControl) {
      _overlaySevenSegmentDraw();
    }
  }

  //Can be used outside of this usermod
  void disableOutputFunction(bool state) {
    this->disableUmLedControl = state; 
  }

/*
  * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
  * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
  * Below it is shown how this could be used for e.g. a light sensor
  */
  void addToJsonInfo(JsonObject& root) {
    JsonObject user = root[F("u")];
    if (user.isNull()) {
      user = root.createNestedObject(F("u"));
    }
	
    // Create a nested array for energy data
    JsonArray ssr_json_seperator = user.createNestedArray(F("-----------------------------------"));
        
    JsonArray ssr_json = user.createNestedArray(F("Seven segment reloaded:"));
  
    if (!umSSDRDisplayTime) {
      ssr_json.add(F("disabled")); // Indicate that the module is disabled
    } else {
      JsonArray invert = user.createNestedArray("Time inverted");
      invert.add(umSSDRInverted);
      
      JsonArray blink = user.createNestedArray("Blinking colon");
      blink.add(umSSDRColonblink);
      
      JsonArray zero = user.createNestedArray("Show hour leading zero");
      zero.add(umSSDRLeadingZero);
      
      JsonArray ldrEnable = user.createNestedArray(F("Auto Brightness enabled"));
      ldrEnable.add(umSSDREnableLDR);
      
      JsonArray ldrInvert = user.createNestedArray(F("Auto Brightness inverted"));
      ldrInvert.add(umSSDRInvertAutoBrightness);
    }
  }

 /*
  * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
  void addToJsonState(JsonObject& root) {
    JsonObject user = root[F("u")];
    if (user.isNull()) {
      user = root.createNestedObject(F("u"));
    }
    _addJSONObject(user);
  }
  
 /*
  * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
  void readFromJsonState(JsonObject& root) {
    JsonObject user = root[F("u")];
    if (!user.isNull()) {
      JsonObject ssdrObj = user[FPSTR(_str_name)];
      umSSDRDisplayTime = ssdrObj[FPSTR(_str_timeEnabled)] | umSSDRDisplayTime;
      umSSDREnableLDR = ssdrObj[FPSTR(_str_ldrEnabled)] | umSSDREnableLDR;
      umSSDRBrightnessMin = ssdrObj[FPSTR(_str_minBrightness)] | umSSDRBrightnessMin;
      umSSDRBrightnessMax = ssdrObj[FPSTR(_str_maxBrightness)] | umSSDRBrightnessMax;
      umSSDRLuxMin = ssdrObj[FPSTR(_str_luxMin)] | umSSDRLuxMin;
      umSSDRLuxMax = ssdrObj[FPSTR(_str_luxMax)] | umSSDRLuxMax;
      umSSDRInvertAutoBrightness = ssdrObj[FPSTR(_str_invertAutoBrightness)] | umSSDRInvertAutoBrightness;
      umSSDRInverted = ssdrObj[FPSTR(_str_inverted)] | umSSDRInverted;
      umSSDRColonblink = ssdrObj[FPSTR(_str_colonblink)] | umSSDRColonblink;
      umSSDRLeadingZero = ssdrObj[FPSTR(_str_leadingZero)] | umSSDRLeadingZero;
      umSSDRDisplayMask = ssdrObj[FPSTR(_str_displayMask)] | umSSDRDisplayMask;
      umSSDRHours = ssdrObj[FPSTR(_str_hours)] | umSSDRHours;
      umSSDRMinutes = ssdrObj[FPSTR(_str_minutes)] | umSSDRMinutes;
      umSSDRSeconds = ssdrObj[FPSTR(_str_seconds)] | umSSDRSeconds;
      umSSDRColons = ssdrObj[FPSTR(_str_colons)] | umSSDRColons;
      umSSDRLight = ssdrObj[FPSTR(_str_light)] | umSSDRLight;
      umSSDRDays = ssdrObj[FPSTR(_str_days)] | umSSDRDays;
      umSSDRMonths = ssdrObj[FPSTR(_str_months)] | umSSDRMonths;
      umSSDRYears = ssdrObj[FPSTR(_str_years)] | umSSDRYears;
    }
  }

  void onMqttConnect(bool sessionPresent) {
    if (!umSSDRDisplayTime) {
      return;
    }
    #ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      char subBuffer[48];
      if (mqttDeviceTopic[0] != 0)
      {
        _updateMQTT();
        //subscribe for sevenseg messages on the device topic
        sprintf_P(subBuffer, PSTR("%s/%S/+/set"), mqttDeviceTopic, _str_name);
        mqtt->subscribe(subBuffer, 2);
      }

      if (mqttGroupTopic[0] != 0)
      {
        //subscribe for sevenseg messages on the group topic
        sprintf_P(subBuffer, PSTR("%s/%S/+/set"), mqttGroupTopic, _str_name);
        mqtt->subscribe(subBuffer, 2);
      }
    }
    #endif
  }

  bool onMqttMessage(char *topic, char *payload) {
    #ifndef WLED_DISABLE_MQTT
    if (umSSDRDisplayTime) {
      //If topic begins with sevenSeg cut it off, otherwise not our message.
      size_t topicPrefixLen = strlen_P(PSTR("/wledSS/"));
      if (strncmp_P(topic, PSTR("/wledSS/"), topicPrefixLen) == 0) {
        topic += topicPrefixLen;
      } else {
        return false;
      }
      //We only care if the topic ends with /set
      size_t topicLen = strlen(topic);
      if (topicLen > 4 &&
          topic[topicLen - 4] == '/' &&
          topic[topicLen - 3] == 's' &&
          topic[topicLen - 2] == 'e' &&
          topic[topicLen - 1] == 't')
      {
        //Trim /set and handle it
        topic[topicLen - 4] = '\0';
        _handleSetting(topic, payload);
      }
    }
    return true;
    #endif
  }

  void addToConfig(JsonObject &root) {
     _addJSONObject(root);
	 
     _updateMQTT();
  }

  bool readFromConfig(JsonObject &root) {
    JsonObject top = root[FPSTR(_str_name)];

    if (top.isNull()) {
      DEBUG_PRINT(FPSTR(_str_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    umSSDRDisplayTime      		  = (top[FPSTR(_str_timeEnabled)] | umSSDRDisplayTime);
    umSSDREnableLDR        		  = (top[FPSTR(_str_ldrEnabled)] | umSSDREnableLDR);
    umSSDRInverted         		  = (top[FPSTR(_str_inverted)] | umSSDRInverted);
    umSSDRColonblink       		  = (top[FPSTR(_str_colonblink)] | umSSDRColonblink);
    umSSDRLeadingZero      		  = (top[FPSTR(_str_leadingZero)] | umSSDRLeadingZero);
    umSSDRDisplayMask      		  = top[FPSTR(_str_displayMask)] | umSSDRDisplayMask;
    umSSDRHours            		  = top[FPSTR(_str_hours)] | umSSDRHours;
    umSSDRMinutes          		  = top[FPSTR(_str_minutes)] | umSSDRMinutes;
    umSSDRSeconds          		  = top[FPSTR(_str_seconds)] | umSSDRSeconds;
    umSSDRColons           		  = top[FPSTR(_str_colons)] | umSSDRColons;
    umSSDRLight        	  		  = top[FPSTR(_str_light)] | umSSDRLight;
    umSSDRDays             		  = top[FPSTR(_str_days)] | umSSDRDays;
    umSSDRMonths           		  = top[FPSTR(_str_months)] | umSSDRMonths;
    umSSDRYears            		  = top[FPSTR(_str_years)] | umSSDRYears;
    umSSDRBrightnessMin    		  = top[FPSTR(_str_minBrightness)] | umSSDRBrightnessMin;
    umSSDRBrightnessMax    		  = top[FPSTR(_str_maxBrightness)] | umSSDRBrightnessMax;
    umSSDRLuxMin                = top[FPSTR(_str_luxMin)] | umSSDRLuxMin;
    umSSDRLuxMax                = top[FPSTR(_str_luxMax)] | umSSDRLuxMax;
    umSSDRInvertAutoBrightness  = top[FPSTR(_str_invertAutoBrightness)] | umSSDRInvertAutoBrightness;

    DEBUG_PRINT(FPSTR(_str_name));
    DEBUG_PRINTLN(F(" config (re)loaded."));

    return true;
  }
  /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
  uint16_t getId() {
    return USERMOD_ID_SSDR;
  }
};

const char UsermodSSDR::_str_name[]        PROGMEM = "UsermodSSDR";
const char UsermodSSDR::_str_timeEnabled[] PROGMEM = "enabled";
const char UsermodSSDR::_str_inverted[]    PROGMEM = "inverted";
const char UsermodSSDR::_str_colonblink[]  PROGMEM = "Colon-blinking";
const char UsermodSSDR::_str_leadingZero[] PROGMEM = "Leading-Zero";
const char UsermodSSDR::_str_displayMask[] PROGMEM = "Display-Mask";
const char UsermodSSDR::_str_hours[]       PROGMEM = "LED-Numbers-Hours";
const char UsermodSSDR::_str_minutes[]     PROGMEM = "LED-Numbers-Minutes";
const char UsermodSSDR::_str_seconds[]     PROGMEM = "LED-Numbers-Seconds";
const char UsermodSSDR::_str_colons[]      PROGMEM = "LED-Numbers-Colons";
const char UsermodSSDR::_str_light[]  	   PROGMEM = "LED-Numbers-Light";
const char UsermodSSDR::_str_days[]        PROGMEM = "LED-Numbers-Day";
const char UsermodSSDR::_str_months[]      PROGMEM = "LED-Numbers-Month";
const char UsermodSSDR::_str_years[]       PROGMEM = "LED-Numbers-Year";
const char UsermodSSDR::_str_ldrEnabled[]  PROGMEM = "enable-auto-brightness";
const char UsermodSSDR::_str_minBrightness[]  PROGMEM = "auto-brightness-min";
const char UsermodSSDR::_str_maxBrightness[]  PROGMEM = "auto-brightness-max";
const char UsermodSSDR::_str_luxMin[]  PROGMEM = "lux-min";
const char UsermodSSDR::_str_luxMax[]  PROGMEM = "lux-max";
const char UsermodSSDR::_str_invertAutoBrightness[]  PROGMEM = "invert-auto-brightness";

static UsermodSSDR seven_segment_display_reloaded;
REGISTER_USERMOD(seven_segment_display_reloaded);