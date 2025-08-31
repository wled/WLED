#include "wled.h"
#include "bq2589x.h"

//Pin defaults for QuinLed Dig-Uno if not overriden
#ifndef BQ2589X_OTG_PIN
  #define BQ2589X_OTG_PIN 10 /* if pin up disable internal voltage converter */
#endif
#ifndef BQ2589X_NCE_PIN
  #define BQ2589X_NCE_PIN 4 /* disable charger */
#endif
#ifndef BQ2589X_INT_PIN
  #define BQ2589X_INT_PIN 2 /* input pit for interrupt by bq */
#endif
#ifndef BQ2589X_SYS_PIN
  #define BQ2589X_SYS_PIN 1 /* input pin for reading sys voltage by ADC of your controller*/
#endif
#ifndef BQ2589X_DEFAULT_REQUEST_INTERVAL
  #define BQ2589X_DEFAULT_REQUEST_INTERVAL 5000
#endif
#ifndef BQ2589X_DEFAULT_BAT_MAX_CC
  #define BQ2589X_DEFAULT_BAT_MAX_CC 2100
#endif
#ifndef BQ2589X_MAX_TEMPERATURE
  #define BQ2589X_MAX_TEMPERATURE 80
#endif


// this function calls when charger generate event 
// you can do it all that you want
static void IRAM_ATTR intPinUp() {
  DEBUG_PRINTLN(F("bq2589x INT pin up."));
}

//class name. Use something descriptive and leave the ": public Usermod" part :)
class Usermod_v2_bq2589x : public Usermod {

  private:
    // Private class members. You can declare variables and functions only accessible to your usermod here
    bq2589x mycharger;
    bq2589x_vbus_type vbus_type;
    bq2589x_part_no   part_no;
    int               bq2589x_revision;
    uint8_t           bat_percentage = 0;               /* % */
    uint32_t          bat_max_charge_current = BQ2589X_DEFAULT_BAT_MAX_CC; /* mA */
    int               bat_temperature_max = BQ2589X_MAX_TEMPERATURE;    /* °C */
    int8_t            otg_pin         = BQ2589X_OTG_PIN;
    int8_t            nce_pin         = BQ2589X_NCE_PIN;
    int8_t            int_pin         = BQ2589X_INT_PIN;
    int8_t            sys_pin         = BQ2589X_SYS_PIN;
    bool              otg_status      = true;
    uint32_t          requestInterval = BQ2589X_DEFAULT_REQUEST_INTERVAL; //ms
    bool enabled  = true;
    bool initPinsDone = false;
    bool initDone = false;
    unsigned long lastTime = 0;

    uint16_t _conversionTable[101]; // table for converting voltage to percentage

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _part_no[];
    static const char _revision[];
    static const char _enabled[];
    static const char _requestInterval[];
    static const char _otgPin[];
    static const char _ncePin[];
    static const char _intPin[];
    static const char _sysPin[];
    static const char _batMinV[];
    static const char _batMaxV[];
    static const char _batMaxCC[];
    static const char _maxTemperature[];

    void initTable () {
      _conversionTable[0]  = 3200;
      _conversionTable[1]  = 3250; _conversionTable[2]  = 3300; _conversionTable[3]  = 3350; _conversionTable[4]  = 3400; _conversionTable[5]  = 3450;
      _conversionTable[6]  = 3500; _conversionTable[7]  = 3550; _conversionTable[8]  = 3600; _conversionTable[9]  = 3650; _conversionTable[10] = 3700;
      _conversionTable[11] = 3703; _conversionTable[12] = 3706; _conversionTable[13] = 3710; _conversionTable[14] = 3713; _conversionTable[15] = 3716;
      _conversionTable[16] = 3719; _conversionTable[17] = 3723; _conversionTable[18] = 3726; _conversionTable[19] = 3729; _conversionTable[20] = 3732;
      _conversionTable[21] = 3735; _conversionTable[22] = 3739; _conversionTable[23] = 3742; _conversionTable[24] = 3745; _conversionTable[25] = 3748;
      _conversionTable[26] = 3752; _conversionTable[27] = 3755; _conversionTable[28] = 3758; _conversionTable[29] = 3761; _conversionTable[30] = 3765;
      _conversionTable[31] = 3768; _conversionTable[32] = 3771; _conversionTable[33] = 3774; _conversionTable[34] = 3777; _conversionTable[35] = 3781;
      _conversionTable[36] = 3784; _conversionTable[37] = 3787; _conversionTable[38] = 3790; _conversionTable[39] = 3794; _conversionTable[40] = 3797;
      _conversionTable[41] = 3800; _conversionTable[42] = 3805; _conversionTable[43] = 3811; _conversionTable[44] = 3816; _conversionTable[45] = 3821;
      _conversionTable[46] = 3826; _conversionTable[47] = 3832; _conversionTable[48] = 3837; _conversionTable[49] = 3842; _conversionTable[50] = 3847;
      _conversionTable[51] = 3853; _conversionTable[52] = 3858; _conversionTable[53] = 3863; _conversionTable[54] = 3868; _conversionTable[55] = 3874;
      _conversionTable[56] = 3879; _conversionTable[57] = 3884; _conversionTable[58] = 3889; _conversionTable[59] = 3895; _conversionTable[60] = 3900;
      _conversionTable[61] = 3906; _conversionTable[62] = 3911; _conversionTable[63] = 3917; _conversionTable[64] = 3922; _conversionTable[65] = 3928;
      _conversionTable[66] = 3933; _conversionTable[67] = 3939; _conversionTable[68] = 3944; _conversionTable[69] = 3950; _conversionTable[70] = 3956;
      _conversionTable[71] = 3961; _conversionTable[72] = 3967; _conversionTable[73] = 3972; _conversionTable[74] = 3978; _conversionTable[75] = 3983;
      _conversionTable[76] = 3989; _conversionTable[77] = 3994; _conversionTable[78] = 4000; _conversionTable[79] = 4008; _conversionTable[80] = 4015;
      _conversionTable[81] = 4023; _conversionTable[82] = 4031; _conversionTable[83] = 4038; _conversionTable[84] = 4046; _conversionTable[85] = 4054;
      _conversionTable[86] = 4062; _conversionTable[87] = 4069; _conversionTable[88] = 4077; _conversionTable[89] = 4085; _conversionTable[90] = 4092;
      _conversionTable[91] = 4100; _conversionTable[92] = 4111; _conversionTable[93] = 4122; _conversionTable[94] = 4133; _conversionTable[95] = 4144;
      _conversionTable[96] = 4156; _conversionTable[97] = 4167; _conversionTable[98] = 4178; _conversionTable[99] = 4189; _conversionTable[100] = 4200;
    }

    void initPins() {
      if (!enabled) return;
      if (initPinsDone) return;
      PinManagerPinType pins[4] = {
        { otg_pin, true },
        { nce_pin, true },
        { int_pin, false },
        { sys_pin, false }
      };
        
      if (!PinManager::allocateMultiplePins(pins, 4, PinOwner::UM_BQ2589X)) {
          DEBUG_PRINTF("[%s] pin allocation failed!\n", _name);
          initPinsDone = false;
          return;
      }
      attachInterrupt(digitalPinToInterrupt(int_pin), intPinUp, RISING);
      digitalWrite(otg_pin, otg_status ? HIGH : LOW);
      DEBUG_PRINTF("[%s] pin allocation done\n", _name);
      initPinsDone = true;
    }

    void deinitPins() {
      if (!initPinsDone) return;
      detachInterrupt(digitalPinToInterrupt(int_pin));
      PinManager::deallocatePin(otg_pin, PinOwner::UM_BQ2589X);
      PinManager::deallocatePin(nce_pin, PinOwner::UM_BQ2589X);
      PinManager::deallocatePin(int_pin, PinOwner::UM_BQ2589X);
      PinManager::deallocatePin(sys_pin, PinOwner::UM_BQ2589X);
      DEBUG_PRINTF("[%s] pin deallocation done\n", _name);
      initPinsDone = false;
    } 
    void reinitPins() {
      deinitPins();
      initPins();
    }

    // any private methods should go here (non-inline method should be defined out of class)
    void publishMqtt(const char* state, bool retain = false); // example for publishing MQTT message
    
    String vbusType() {
      String s = "?";
        switch (mycharger.get_vbus_type()) {
          case 0: s = "NONE";       break;
          case 1: s = "USB_SDP";    break;
          case 2: s = "CDP(1,5A)";  break;
          case 3: s = "DCP(3,25A)"; break;
          case 4: s = "MAXC";       break;
          case 5: s = "UNKNOWN";    break;
          case 6: s = "NONSTAND";   break;
          case 7: s = "OTG";        break;
        }
      return s;
    }

    int calculatePercentage (int voltage) {
      if (voltage <= _conversionTable[0]) return 0;
      if (voltage >= _conversionTable[100]) return 100;
      int i = 1;
      while (i < 101 && voltage > _conversionTable[i]) {
        i++;
      }
      return i;
    }

  public:

    // non WLED related methods, may be used for data exchange between usermods (non-inline methods should be defined out of class)

    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) { enabled = enable; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

    // in such case add the following to another usermod:
    //  in private vars:
    //   #ifdef USERMOD_V2_BQ2589X
    //   Usermod_v2_bq2589x* UM;
    //   #endif
    //  in setup()
    //   #ifdef USERMOD_V2_BQ2589X
    //   UM = (Usermod_v2_bq2589x*) UsermodManager::lookup(USERMOD_ID_BQ2589X);
    //   #endif
    //  somewhere in loop() or other member method
    //   #ifdef USERMOD_V2_BQ2589X
    //   if (UM != nullptr) isExampleEnabled = UM->isEnabled();
    //   if (!isExampleEnabled) UM->enable(true);
    //   #endif
    

    // methods called by WLED (can be inlined as they are called only once but if you call them explicitly define them out of class)

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * readFromConfig() is called prior to setup()
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() override {
      // do your set-up here
      if (enabled) {
        Serial.begin(115200);
        Wire.begin();
        mycharger.begin(&Wire);
        mycharger.detect_device(&part_no, &bq2589x_revision);
        initTable();
        initPins();  
      }
      DEBUG_PRINTLN(F("bq2589x usermod setup done"));
      initDone = true;
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() override {
    }


    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     * 
     * Tips:
     * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
     *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
     * 
     * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
     *    Instead, use a timer check as shown here.
     */
    bool configChanged() {

      if( enabled && (
        requestInterval != BQ2589X_DEFAULT_REQUEST_INTERVAL ||
        bat_max_charge_current != BQ2589X_DEFAULT_BAT_MAX_CC ||
        otg_status != true
      )) {
        return true;
      } else {
        return false;
      }
    }

    void sendUserConfigToBq () {
      if (!enabled || !initDone) return;
      mycharger.adc_start(false);
      mycharger.reset_watchdog_timer();
      
      if (bat_max_charge_current != BQ2589X_DEFAULT_BAT_MAX_CC) {
        DEBUG_PRINT(F("bq2589x battery max charge current set to "));
        DEBUG_PRINT(bat_max_charge_current);
        DEBUG_PRINTLN(F(" mA"));
        mycharger.set_charge_current(bat_max_charge_current);
      }
      if (bat_temperature_max != BQ2589X_MAX_TEMPERATURE) {
        DEBUG_PRINT(F("bq2589x battery max temperature set to "));
        DEBUG_PRINT(bat_temperature_max);
        DEBUG_PRINTLN(F(" °C"));
        if (mycharger.adc_read_temperature() > bat_temperature_max) {
          mycharger.disable_charger();
          mycharger.disable_otg();
        }
      }
      if (!otg_status) {
        DEBUG_PRINTLN(F("bq2589x OTG disabled by I2C command"));
        mycharger.disable_otg();
      }
      mycharger.adc_stop();

    }

    void loop() override {
      static bool timerOn = true;
      // if usermod is disabled or called during strip updating just exit
      // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
      if (!enabled || strip.isUpdating()) return;
      // do your magic here
      if (millis() - lastTime > requestInterval) {
        if (configChanged()) {
          DEBUG_PRINTLN(F("bq2589x usermod config changed, sending to bq2589x"));
          timerOn = false;
          sendUserConfigToBq();
        } else if (!timerOn) {
          DEBUG_PRINTLN(F("bq2589x config set to default, enabling watchdog timer"));
          mycharger.reset_chip();
          timerOn = true;
        }
    }
    lastTime = millis();
  }
    


    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root) override
    {
      // if "u" object does not exist yet wee need to create it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      mycharger.adc_start(false);
      JsonArray infoBq2589xPart = user.createNestedArray(F(_part_no));
      switch(part_no) {
        case BQ25890: infoBq2589xPart.add("BQ25890"); break;
        case BQ25892: infoBq2589xPart.add("BQ25892"); break;
        case BQ25895: infoBq2589xPart.add("BQ25895"); break;
        default: infoBq2589xPart.add("Unknown");   break;
      }
      JsonArray infoBq2589xRev = user.createNestedArray(F(_revision));
      infoBq2589xRev.add(bq2589x_revision);

      JsonArray infoBq2589xLevel = user.createNestedArray(F("Battery level"));
      int voltage = mycharger.adc_read_battery_volt();
      infoBq2589xLevel.add(calculatePercentage(voltage));
      infoBq2589xLevel.add(F(" %"));

      JsonArray infoBq2589xInV = user.createNestedArray(F("Vbus voltage"));
      infoBq2589xInV.add(mycharger.adc_read_vbus_volt());
      infoBq2589xInV.add(F(" mV"));

      JsonArray infoBq2589xVoltage = user.createNestedArray(F("Battery voltage"));
      infoBq2589xVoltage.add(voltage);
      infoBq2589xVoltage.add(F(" mV"));

      JsonArray infoBq2589xCurrent = user.createNestedArray(F("Charge current"));
      infoBq2589xCurrent.add(mycharger.adc_read_charge_current());
      infoBq2589xCurrent.add(F(" mA"));

      JsonArray infoBq2589xTemp = user.createNestedArray(F("Temperature"));
      infoBq2589xTemp.add(mycharger.adc_read_temperature());
      infoBq2589xTemp.add(F(" °C"));

      JsonArray infoBq2589xType = user.createNestedArray(F("Power source"));
      infoBq2589xType.add(vbusType());

      JsonArray infoBq2589xStatus = user.createNestedArray(F("Charging status"));
      switch(mycharger.get_charging_status()) {
        case 0: infoBq2589xStatus.add("Not");       break;
        case 1: infoBq2589xStatus.add("Precharge"); break;
        case 2: infoBq2589xStatus.add("Fast");      break;
        case 3: infoBq2589xStatus.add("Done");      break;
      }

      JsonArray infoBq2589xOtgStatus = user.createNestedArray(F("OTG status"));
      if(otg_status) {
        infoBq2589xOtgStatus.add("On");
      } else {
        infoBq2589xOtgStatus.add("Off");
      }

      mycharger.adc_stop();
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root) override
    {
      if (!initDone || !enabled) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      //usermod["user0"] = userVar0;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root) override
    {
      if (!initDone || !enabled) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_name)];
      
      if(root["on"].is<bool>()) {
        otg_status = root["on"].as<bool>();
      } else if (root["on"].is<const char*>()) {
        if(root["on"].as<const char*>()[0] == 't') otg_status = true;
        else otg_status = false;
      }

      digitalWrite(otg_pin, otg_status ? HIGH : LOW);
      DEBUG_PRINT(F("bq2589x OTG pin set to"));
      DEBUG_PRINTLN(otg_status ? F("ON") : F("OFF"));
      
    }


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
     *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
     *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
     *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
     *     used in the Usermod when reading the value from ArduinoJson.
     * - Pin values can be treated differently from an integer value by using the key name "pin"
     *   - "pin" can contain a single or array of integer values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
     *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root) override
    {
      JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_otgPin)] = otg_pin;  // usermodparam
      top[FPSTR(_ncePin)] = nce_pin;  // usermodparam
      top[FPSTR(_intPin)] = int_pin;  // usermodparam
      top[FPSTR(_sysPin)] = sys_pin;  // usermodparam
      top[FPSTR(_batMaxCC)] = bat_max_charge_current;  // usermodparam
      top[FPSTR(_requestInterval)] = requestInterval;
      top[FPSTR(_maxTemperature)] = bat_temperature_max;
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
     * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
     * 
     * This function is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root) override
    {
      // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)
        JsonObject top = root[FPSTR(_name)];
        if (top.isNull()) {
          DEBUG_PRINTF("[%s] No config found. (Using defaults.)\n", _name);
          return false;
        }
        uint8_t oldPinInt = int_pin;
        uint8_t oldPinNce = nce_pin;
        uint8_t oldPinOtg = otg_pin;
        uint8_t oldPinSys = sys_pin;
        bool    oldEnabled = enabled;
        
        getJsonValue(top[FPSTR(_otgPin)], otg_pin);
        getJsonValue(top[FPSTR(_ncePin)], nce_pin);
        getJsonValue(top[FPSTR(_intPin)], int_pin);
        getJsonValue(top[FPSTR(_sysPin)], sys_pin);
        getJsonValue(top[FPSTR(_batMaxCC)], bat_max_charge_current);
        getJsonValue(top[FPSTR(_requestInterval)], requestInterval);
        getJsonValue(top[FPSTR(_enabled)], enabled);
        getJsonValue(top[FPSTR(_maxTemperature)], bat_temperature_max);

        if (enabled != oldEnabled) {
          deinitPins();
          DEBUG_PRINTF("bq2589 %s\n", enabled ? "enabled" : "disabled");
        }

        if (enabled && (
          oldPinInt != int_pin ||
          oldPinNce != nce_pin ||
          oldPinOtg != otg_pin ||
          oldPinSys != sys_pin)
        ) {
          DEBUG_PRINTF("bq2589x reinit pins\n");
          reinitPins();
        }
      
      return true;
    }


    /*
     * appendConfigData() is called when user enters usermod settings page
     * it may add additional metadata for certain entry fields (adding drop down is possible)
     * be careful not to add too much as oappend() buffer is limited to 3k
     */
    void appendConfigData() override
    {
    }


    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw() override
    {
      //strip.setPixelColor(0, RGBW32(0,0,0,0)) // set the first pixel to black
    }


    /**
     * handleButton() can be used to override default button behaviour. Returning true
     * will prevent button working in a default way.
     * Replicating button.cpp
     */
    bool handleButton(uint8_t b) override {
      yield();
      // ignore certain button types as they may have other consequences
      if (!enabled
       || buttonType[b] == BTN_TYPE_NONE
       || buttonType[b] == BTN_TYPE_RESERVED
       || buttonType[b] == BTN_TYPE_PIR_SENSOR
       || buttonType[b] == BTN_TYPE_ANALOG
       || buttonType[b] == BTN_TYPE_ANALOG_INVERTED) {
        return false;
      }

      bool handled = false;
      // do your button handling here
      return handled;
    }
  

#ifndef WLED_DISABLE_MQTT
    /**
     * handling of MQTT message
     * topic only contains stripped topic (part after /wled/MAC)
     */
    bool onMqttMessage(char* topic, char* payload) override {
      // check if we received a command
      //if (strlen(topic) == 8 && strncmp_P(topic, PSTR("/command"), 8) == 0) {
      //  String action = payload;
      //  if (action == "on") {
      //    enabled = true;
      //    return true;
      //  } else if (action == "off") {
      //    enabled = false;
      //    return true;
      //  } else if (action == "toggle") {
      //    enabled = !enabled;
      //    return true;
      //  }
      ///}
      return false;
    }

    /**
     * onMqttConnect() is called when MQTT connection is established
     */
    void onMqttConnect(bool sessionPresent) override {
      // do any MQTT related initialisation here
      //publishMqtt("I am alive!");
    }
#endif


    /**
     * onStateChanged() is used to detect WLED state change
     * @mode parameter is CALL_MODE_... parameter used for notifications
     */
    void onStateChange(uint8_t mode) override {
      // do something if WLED state changed (color, brightness, effect, preset, etc)
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() override
    {
      return USERMOD_ID_BQ2589X;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!


  // you can add more getters and setters if you need them
  int getBatteryMaxCC() { return bat_max_charge_current; }
  int getBatteryMaxTemp() { return bat_temperature_max; }
  bool getOtgStatus() { return otg_status; }
  int getOtgPin() { return otg_pin; }
  int getNcePin() { return nce_pin; }
  int getIntPin() { return int_pin; }
  int getSysPin() { return sys_pin; }
  uint32_t getRequestInterval() { return requestInterval; }
  void setBatteryMaxCC(int c) { bat_max_charge_current = c; }
  void setBatteryMaxTemp(int t) { bat_temperature_max = t; }
  void setOtgStatus(bool s) { otg_status = s; }
  void setRequestInterval(uint32_t i) { requestInterval = i; }

  int  getBatteryPercentage() {
    mycharger.adc_start(false);
    int voltage = mycharger.adc_read_battery_volt();
    mycharger.adc_stop();
    return calculatePercentage(voltage);
  }

  int getChargerCurrent() {
    mycharger.adc_start(false);
    int temperature = mycharger.adc_read_charge_current();
    mycharger.adc_stop();
    return temperature;
  }
  int getBatteryVoltage() {
    mycharger.adc_start(false);
    int voltage = mycharger.adc_read_battery_volt();
    mycharger.adc_stop();
    return voltage;
  }
  int getInputVoltage() {
    mycharger.adc_start(false);
    int voltage = mycharger.adc_read_vbus_volt();
    mycharger.adc_stop();
    return voltage;
  }
  int getTemperature() {
    mycharger.adc_start(false);
    int temperature = mycharger.adc_read_temperature();
    mycharger.adc_stop();
    return temperature;
  }

};



// implementation of non-inline member methods

void Usermod_v2_bq2589x::publishMqtt(const char* state, bool retain)
{
#ifndef WLED_DISABLE_MQTT
  //Check if MQTT Connected, otherwise it will crash the 8266
  if (WLED_MQTT_CONNECTED) {
    char subuf[64];
    strcpy(subuf, mqttDeviceTopic);
    strcat_P(subuf, PSTR("/bq2589x"));
    mqtt->publish(subuf, 0, retain, state);
  }
#endif
}

// add more strings here to reduce flash memory usage
const char Usermod_v2_bq2589x::_name[]    PROGMEM = "bq2589x";
const char Usermod_v2_bq2589x::_part_no[] PROGMEM = "Part No";
const char Usermod_v2_bq2589x::_revision[] PROGMEM = "Revision";
const char Usermod_v2_bq2589x::_enabled[] PROGMEM = "enabled";
const char Usermod_v2_bq2589x::_requestInterval[] PROGMEM = "request-interval-ms";
const char Usermod_v2_bq2589x::_otgPin[] PROGMEM = "otg-pin";
const char Usermod_v2_bq2589x::_ncePin[] PROGMEM = "!ce-pin";
const char Usermod_v2_bq2589x::_intPin[] PROGMEM = "int-pin";
const char Usermod_v2_bq2589x::_sysPin[] PROGMEM = "sys-pin";
const char Usermod_v2_bq2589x::_batMaxCC[] PROGMEM = "Battery Max Charge Current";
const char Usermod_v2_bq2589x::_maxTemperature[] PROGMEM = "Battery Max Temperature";


static Usermod_v2_bq2589x bq2589x_um;
REGISTER_USERMOD(bq2589x_um);
