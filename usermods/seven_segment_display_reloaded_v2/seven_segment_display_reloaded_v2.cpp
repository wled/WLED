#ifdef USERMOD_SN_PHOTORESISTOR
  #include "SN_Photoresistor.h"
#endif
#ifdef USERMOD_BH1750
  #include "BH1750_v2.h"
#endif

#include "seven_segment_display_reloaded_v2.h"

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

  void UsermodSSDR::_overlaySevenSegmentDraw() {
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
          timeVar = year(localTime) % 100;  // Fix: Get last two digits of year
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

  void UsermodSSDR::_setColons() {
    if ( umSSDRColonblink ) {
      if ( second(localTime) % 2 == 0 ) {
        _showElements(&umSSDRColons, 0, 1, 0);
      }
    } else {
      _showElements(&umSSDRColons, 0, 1, 0);
    }
  }

  void UsermodSSDR::_showElements(String *map, int timevar, bool isColon, bool removeZero) {
    if ((map != nullptr) && (*map != nullptr) && !(*map).equals("")) {
      int length = (timevar == 0) ? 1 : log10(timevar) + 1;
      bool addZero = false;
      if (length == 1) {
        length = 2;
        addZero = true;
      }
      int timeArr[length];

      if (addZero) {
        if (removeZero) {
          timeArr[0] = timevar;
          timeArr[1] = 10;  // blank digit
        } else {
          timeArr[0] = timevar;
          timeArr[1] = 0;
        }
      } else {
		  int count = 0;
		  while (timevar) {
			timeArr[count] = timevar % 10;
			timevar /= 10;
			count++;
		  }
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
            if (countDigit < length) {  // Prevent array out of bounds
              _setLeds(_checkForNumber(count, index, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
            }
            count = 0;
            range = false;
            countDigit++;
            countSegments = 0;
            break;
          case ';':
            if (countDigit < length) {  // Prevent array out of bounds
              _setLeds(_checkForNumber(count, index, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
            }
            count = 0;
            range = false;
            countSegments++;
            break;
          case ',':
            if (countDigit < length) {  // Prevent array out of bounds
              _setLeds(_checkForNumber(count, index, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
            }
            count = 0;
            range = false;
            break;
          default:
            count++;
            break;
        }
      }
      
      // Process final segment if any
      if (count > 0 && countDigit < length) {
        _setLeds(_checkForNumber(count, colonsLen, map), lastSeenLedNr, range, countSegments, timeArr[countDigit], isColon);
      }
    }
  }

  void UsermodSSDR::_setLeds(int lednr, int lastSeenLedNr, bool range, int countSegments, int number, bool colon) {
    if (!umSSDRMask || (lednr < 0) || (lednr >= umSSDRLength)) return;  // prevent array bounds violation

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

  void UsermodSSDR::_setMaskToLeds() {
    if (!umSSDRMask) return;  // Safety check
    
    for(int i = 0; i < umSSDRLength; i++) {  // Changed <= to < to prevent buffer overflow
      if ((!umSSDRInverted && !umSSDRMask[i]) || (umSSDRInverted && umSSDRMask[i])) {
        strip.setPixelColor(i, 0x000000);
      }
    }
  }

  void UsermodSSDR::_setAllFalse() {
    if (!umSSDRMask) return;  // Safety check
    
    for(int i = 0; i < umSSDRLength; i++) {  // Changed <= to < to prevent buffer overflow
      umSSDRMask[i] = false;
    }
  }

  int UsermodSSDR::_checkForNumber(int count, int index, String *map) {
    if (count <= 0 || map == nullptr || index < count) return 0;  // Added safety checks
    
    String number = (*map).substring(index - count, index);
    return number.toInt();
  }

  void UsermodSSDR::_publishMQTTint_P(const char *subTopic, int value)
  {
    #ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      if(mqtt == NULL) return;
        
      char buffer[64];
      char valBuffer[12];
      int result = snprintf_P(buffer, sizeof(buffer), PSTR("%s/%S/%S"), mqttDeviceTopic, _str_name, subTopic);
      if (result < 0 || result >= sizeof(buffer)) return;  // Buffer overflow check
      
      snprintf_P(valBuffer, sizeof(valBuffer), PSTR("%d"), value);
      mqtt->publish(buffer, 2, true, valBuffer);
    }
    #endif
  }

  void UsermodSSDR::_publishMQTTstr_P(const char *subTopic, String Value)
  {
    #ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      if(mqtt == NULL) return;
      char buffer[64];
      int result = snprintf_P(buffer, sizeof(buffer), PSTR("%s/%S/%S"), mqttDeviceTopic, _str_name, subTopic);
      if (result < 0 || result >= sizeof(buffer)) return;  // Buffer overflow check
      
      mqtt->publish(buffer, 2, true, Value.c_str(), Value.length());
    }
    #endif
  }

  bool UsermodSSDR::_cmpIntSetting_P(char *topic, char *payload, const char *setting, void *value)
  {
    if (strcmp_P(topic, setting) == 0)
    {
      *((int *)value) = strtol(payload, nullptr, 10);  // Changed NULL to nullptr
      _publishMQTTint_P(setting, *((int *)value));
      return true;
    }
    return false;
  }

  bool UsermodSSDR::_handleSetting(char *topic, char *payload) {
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

  void UsermodSSDR::_updateMQTT()
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

  void UsermodSSDR::_addJSONObject(JsonObject& root) {
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

  void UsermodSSDR::setup() {
    umSSDRLength = strip.getLengthTotal();
	  
    // Fixed memory allocation to prevent leaks
    if (umSSDRMask != nullptr) {
      free(umSSDRMask);  // Free previous allocation if exists
    }
    umSSDRMask = (bool*) calloc(umSSDRLength, sizeof(bool));  // Use calloc to initialize to 0
	  
    if (umSSDRMask == nullptr) {
      DEBUG_PRINTLN(F("Failed to allocate memory for SSDR mask"));
      return;  // Early return on memory allocation failure
    }
    _setAllFalse();

    #ifdef USERMOD_SN_PHOTORESISTOR
      ptr = (Usermod_SN_Photoresistor*) UsermodManager::lookup(USERMOD_ID_SN_PHOTORESISTOR);
    #endif
    #ifdef USERMOD_BH1750
      bh1750 = (Usermod_BH1750*) UsermodManager::lookup(USERMOD_ID_BH1750);
    #endif
    DEBUG_PRINTLN(F("SSDR setup done"));
  }

  // Clean up any allocated memory in the destructor
  UsermodSSDR::~UsermodSSDR() {
    if (umSSDRMask != nullptr) {
      free(umSSDRMask);
      umSSDRMask = nullptr;
    }
  }

  /*
  * loop() is called continuously. Here you can check for events, read sensors, etc.
  */
  void UsermodSSDR::loop() {
    if (!umSSDRDisplayTime || strip.isUpdating() || umSSDRMask == nullptr) {
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
          // Constrain lux values within defined range
          float constrainedLux = constrain(lux, umSSDRLuxMin, umSSDRLuxMax);
          
          if (!umSSDRInvertAutoBrightness) {
            brightness = map(constrainedLux, umSSDRLuxMin, umSSDRLuxMax, umSSDRBrightnessMin, umSSDRBrightnessMax);
          } else {
            brightness = map(constrainedLux, umSSDRLuxMin, umSSDRLuxMax, umSSDRBrightnessMax, umSSDRBrightnessMin);
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

  void UsermodSSDR::handleOverlayDraw() {
    if (umSSDRDisplayTime && !disableUmLedControl && umSSDRMask != nullptr) {
      _overlaySevenSegmentDraw();
    }
  }

  /*
  * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
  * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
  * Below it is shown how this could be used for e.g. a light sensor
  */
  void UsermodSSDR::addToJsonInfo(JsonObject& root) {
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
  void UsermodSSDR::addToJsonState(JsonObject& root) {
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
  void UsermodSSDR::readFromJsonState(JsonObject& root) {
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

  void UsermodSSDR::onMqttConnect(bool sessionPresent) {
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

  bool UsermodSSDR::onMqttMessage(char *topic, char *payload) {
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

  void UsermodSSDR::addToConfig(JsonObject &root) {
     _addJSONObject(root);
	 
     _updateMQTT();
  }

  bool UsermodSSDR::readFromConfig(JsonObject &root) {
    JsonObject top = root[FPSTR(_str_name)];

    if (top.isNull()) {
      DEBUG_PRINT(FPSTR(_str_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    umSSDRDisplayTime			= (top[FPSTR(_str_timeEnabled)] | umSSDRDisplayTime);
    umSSDREnableLDR				= (top[FPSTR(_str_ldrEnabled)] | umSSDREnableLDR);
    umSSDRInverted				= (top[FPSTR(_str_inverted)] | umSSDRInverted);
    umSSDRColonblink			= (top[FPSTR(_str_colonblink)] | umSSDRColonblink);
    umSSDRLeadingZero			= (top[FPSTR(_str_leadingZero)] | umSSDRLeadingZero);
    umSSDRDisplayMask			= top[FPSTR(_str_displayMask)] | umSSDRDisplayMask;
    umSSDRHours					= top[FPSTR(_str_hours)] | umSSDRHours;
    umSSDRMinutes				= top[FPSTR(_str_minutes)] | umSSDRMinutes;
    umSSDRSeconds				= top[FPSTR(_str_seconds)] | umSSDRSeconds;
    umSSDRColons				= top[FPSTR(_str_colons)] | umSSDRColons;
    umSSDRLight					= top[FPSTR(_str_light)] | umSSDRLight;
    umSSDRDays					= top[FPSTR(_str_days)] | umSSDRDays;
    umSSDRMonths				= top[FPSTR(_str_months)] | umSSDRMonths;
    umSSDRYears					= top[FPSTR(_str_years)] | umSSDRYears;
    umSSDRBrightnessMin			= top[FPSTR(_str_minBrightness)] | umSSDRBrightnessMin;
    umSSDRBrightnessMax			= top[FPSTR(_str_maxBrightness)] | umSSDRBrightnessMax;
    umSSDRLuxMin				= top[FPSTR(_str_luxMin)] | umSSDRLuxMin;
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
  uint16_t UsermodSSDR::getId() {
    return USERMOD_ID_SSDR;
  }

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