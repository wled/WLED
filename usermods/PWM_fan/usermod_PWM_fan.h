#pragma once

#ifndef USERMOD_DALLASTEMPERATURE
#error The "PWM fan" usermod requires "Dallas Temeprature" usermod to function properly.
#endif

#include "wled.h"

// PWM & tacho code curtesy of @KlausMu
// https://github.com/KlausMu/esp32-fan-controller/tree/main/src
// adapted for WLED usermod by @blazoncek

#ifndef TACHO_PIN
  #define TACHO_PIN -1
#endif

#ifndef PWM_PIN
  #define PWM_PIN -1
#endif

// tacho counter
static volatile unsigned long counter_rpm = 0;
// Interrupt counting every rotation of the fan
// https://desire.giesecke.tk/index.php/2018/01/30/change-global-variables-from-isr/
static void IRAM_ATTR rpm_fan() {
  counter_rpm++;
}


class PWMFanUsermod : public Usermod {

  private:

    bool initDone = false;
    bool enabled = true;
    unsigned long msLastTachoMeasurement = 0;
    uint16_t last_rpm = 0;
    #ifdef ARDUINO_ARCH_ESP32
    uint8_t pwmChannel = 255;
    #endif

    #ifdef USERMOD_DALLASTEMPERATURE
    UsermodTemperature* tempUM;
    #endif

    // configurable parameters
    int8_t  tachoPin          = TACHO_PIN;
    int8_t  pwmPin            = PWM_PIN;
    uint8_t tachoUpdateSec    = 30;
    float   targetTemperature = 25.0;
    uint8_t minPWMValuePct    = 50;
    uint8_t numberOfInterrupsInOneSingleRotation = 2;     // Number of interrupts ESP32 sees on tacho signal on a single fan rotation. All the fans I've seen trigger two interrups.

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _tachoPin[];
    static const char _pwmPin[];
    static const char _temperature[];
    static const char _tachoUpdateSec[];
    static const char _minPWMValuePct[];
    static const char _IRQperRotation[];

    void initTacho(void) {
      if (tachoPin < 0 || !pinManager.allocatePin(tachoPin, false, PinOwner::UM_Unspecified)){
        tachoPin = -1;
        return;
      }
      pinMode(tachoPin, INPUT);
      digitalWrite(tachoPin, HIGH);
      attachInterrupt(digitalPinToInterrupt(tachoPin), rpm_fan, FALLING);
      DEBUG_PRINTLN(F("Tacho sucessfully initialized."));
    }

    void deinitTacho(void) {
      if (tachoPin < 0) return;
      detachInterrupt(digitalPinToInterrupt(tachoPin));
      pinManager.deallocatePin(tachoPin, PinOwner::UM_Unspecified);
      tachoPin = -1;
    }

    void updateTacho(void) {
      if (tachoPin < 0) return;

      // start of tacho measurement
      // detach interrupt while calculating rpm
      detachInterrupt(digitalPinToInterrupt(tachoPin)); 
      // calculate rpm
      last_rpm = (counter_rpm * 60) / numberOfInterrupsInOneSingleRotation;
      last_rpm /= tachoUpdateSec;
      // reset counter
      counter_rpm = 0; 
      // store milliseconds when tacho was measured the last time
      msLastTachoMeasurement = millis();
      // attach interrupt again
      attachInterrupt(digitalPinToInterrupt(tachoPin), rpm_fan, FALLING);
    }

    // https://randomnerdtutorials.com/esp32-pwm-arduino-ide/
    void initPWMfan(void) {
      if (pwmPin < 0 || !pinManager.allocatePin(pwmPin, true, PinOwner::UM_Unspecified)) {
        pwmPin = -1;
        return;
      }

      #ifdef ESP8266
      analogWriteRange(255);
      analogWriteFreq(WLED_PWM_FREQ);
      #else
      pwmChannel = pinManager.allocateLedc(1);
      if (pwmChannel == 255) { //no more free LEDC channels
        deinitPWMfan(); return;
      }
      // configure LED PWM functionalitites
      ledcSetup(pwmChannel, 25000, 8);
      // attach the channel to the GPIO to be controlled
      ledcAttachPin(pwmPin, pwmChannel);
      #endif
      DEBUG_PRINTLN(F("Fan PWM sucessfully initialized."));
    }

    void deinitPWMfan(void) {
      if (pwmPin < 0) return;

      pinManager.deallocatePin(pwmPin, PinOwner::UM_Unspecified);
      #ifdef ARDUINO_ARCH_ESP32
      pinManager.deallocateLedc(pwmChannel, 1);
      #endif
      pwmPin = -1;
    }

    void updateFanSpeed(uint8_t pwmValue){
      if (pwmPin < 0) return;

      #ifdef ESP8266
      analogWrite(pwmPin, pwmValue);
      #else
      ledcWrite(pwmChannel, pwmValue);
      #endif
    }

    float getActualTemperature(void) {
      #ifdef USERMOD_DALLASTEMPERATURE
      if (tempUM != nullptr)
        return tempUM->getTemperatureC();
      #endif
      return -127.0f;
    }

    void setFanPWMbasedOnTemperature(void) {
      float temp = getActualTemperature();
      float difftemp = temp - targetTemperature;
      // Default to run fan at full speed.
      int newPWMvalue = 255;
      int pwmStep = ((100 - minPWMValuePct) * newPWMvalue) / (7*100);
      int pwmMinimumValue = (minPWMValuePct * newPWMvalue) / 100;

      if ((temp == NAN) || (temp <= 0.0)) {
        DEBUG_PRINTLN(F("WARNING: no temperature value available. Cannot do temperature control. Will set PWM fan to 255."));
      } else if (difftemp <= 0.0) {
        // Temperature is below target temperature. Run fan at minimum speed.
        newPWMvalue = pwmMinimumValue;
      } else if (difftemp <= 0.5) {
        newPWMvalue = pwmMinimumValue + pwmStep;
      } else if (difftemp <= 1.0) {
        newPWMvalue = pwmMinimumValue + 2*pwmStep;
      } else if (difftemp <= 1.5) {
        newPWMvalue = pwmMinimumValue + 3*pwmStep;
      } else if (difftemp <= 2.0) {
        newPWMvalue = pwmMinimumValue + 4*pwmStep;
      } else if (difftemp <= 2.5) {
        newPWMvalue = pwmMinimumValue + 5*pwmStep;
      } else if (difftemp <= 3.0) {
        newPWMvalue = pwmMinimumValue + 6*pwmStep;
      }
      updateFanSpeed(newPWMvalue);
    }

  public:

    // gets called once at boot. Do all initialization that doesn't depend on
    // network here
    void setup() {
      #ifdef USERMOD_DALLASTEMPERATURE   
      // This Usermod requires Temperature usermod
      tempUM = (UsermodTemperature*) usermods.lookup(USERMOD_ID_TEMPERATURE);
      #endif
      initTacho();
      initPWMfan();
      updateFanSpeed((minPWMValuePct * 255) / 100); // inital fan speed
      initDone = true;
    }

    // gets called every time WiFi is (re-)connected. Initialize own network
    // interfaces here
    void connected() {}

    /*
     * Da loop.
     */
    void loop() {
      if (!enabled || strip.isUpdating()) return;

      unsigned long now = millis();
      if ((now - msLastTachoMeasurement) < (tachoUpdateSec * 1000)) return;

      updateTacho();
      setFanPWMbasedOnTemperature();
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root) {
      if (tachoPin < 0) return;
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray data = user.createNestedArray(FPSTR(_name));
      data.add(last_rpm);
      data.add(F("rpm"));
    }

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void addToJsonState(JsonObject& root) {
    //}

    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void readFromJsonState(JsonObject& root) {
    //  if (!initDone) return;  // prevent crash on boot applyPreset()
    //}

    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will also not yet add your setting to one of the settings pages automatically.
     * To make that work you still have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
      top[FPSTR(_enabled)]        = enabled;
      top[FPSTR(_pwmPin)]         = pwmPin;
      top[FPSTR(_tachoPin)]       = tachoPin;
      top[FPSTR(_tachoUpdateSec)] = tachoUpdateSec;
      top[FPSTR(_temperature)]    = targetTemperature;
      top[FPSTR(_minPWMValuePct)] = minPWMValuePct;
      top[FPSTR(_IRQperRotation)] = numberOfInterrupsInOneSingleRotation;
      DEBUG_PRINTLN(F("Autosave config saved."));
    }

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * The function should return true if configuration was successfully loaded or false if there was no configuration.
     */
    bool readFromConfig(JsonObject& root) {
      int8_t newTachoPin = tachoPin;
      int8_t newPwmPin   = pwmPin;

      JsonObject top = root[FPSTR(_name)];
      DEBUG_PRINT(FPSTR(_name));
      if (top.isNull()) {
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      enabled           = top[FPSTR(_enabled)] | enabled;
      newTachoPin       = top[FPSTR(_tachoPin)] | newTachoPin;
      newPwmPin         = top[FPSTR(_pwmPin)] | newPwmPin;
      tachoUpdateSec    = top[FPSTR(_tachoUpdateSec)] | tachoUpdateSec;
      tachoUpdateSec    = (uint8_t) max(1,(int)tachoUpdateSec); // bounds checking
      targetTemperature = top[FPSTR(_temperature)] | targetTemperature;
      minPWMValuePct    = top[FPSTR(_minPWMValuePct)] | minPWMValuePct;
      minPWMValuePct    = (uint8_t) min(100,max(0,(int)minPWMValuePct)); // bounds checking
      numberOfInterrupsInOneSingleRotation = top[FPSTR(_IRQperRotation)] | numberOfInterrupsInOneSingleRotation;
      numberOfInterrupsInOneSingleRotation = (uint8_t) max(1,(int)numberOfInterrupsInOneSingleRotation); // bounds checking

      if (!initDone) {
        // first run: reading from cfg.json
        tachoPin = newTachoPin;
        pwmPin   = newPwmPin;
        DEBUG_PRINTLN(F(" config loaded."));
      } else {
        DEBUG_PRINTLN(F(" config (re)loaded."));
        // changing paramters from settings page
        if (tachoPin != newTachoPin || pwmPin != newPwmPin) {
          DEBUG_PRINTLN(F("Re-init pins."));
          // deallocate pin and release interrupts
          deinitTacho();
          deinitPWMfan();
          tachoPin = newTachoPin;
          pwmPin   = newPwmPin;
          // initialise
          setup();
        }
      }

      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return !top[FPSTR(_IRQperRotation)].isNull();
  }

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() {
        return USERMOD_ID_PWM_FAN;
    }
};

// strings to reduce flash memory usage (used more than twice)
const char PWMFanUsermod::_name[]           PROGMEM = "PWM-fan";
const char PWMFanUsermod::_enabled[]        PROGMEM = "enabled";
const char PWMFanUsermod::_tachoPin[]       PROGMEM = "tacho-pin";
const char PWMFanUsermod::_pwmPin[]         PROGMEM = "PWM-pin";
const char PWMFanUsermod::_temperature[]    PROGMEM = "target-temp-C";
const char PWMFanUsermod::_tachoUpdateSec[] PROGMEM = "tacho-update-s";
const char PWMFanUsermod::_minPWMValuePct[] PROGMEM = "min-PWM-percent";
const char PWMFanUsermod::_IRQperRotation[] PROGMEM = "IRQs-per-rotation";
