#include "wled.h"
#include "driver/rtc_io.h"

#ifdef ESP8266
#error The "Deep Sleep" usermod does not support ESP8266
#endif

#ifndef DEEPSLEEP_WAKEUPPIN
#define DEEPSLEEP_WAKEUPPIN 0
#endif
#ifndef DEEPSLEEP_WAKEWHENHIGH
#define DEEPSLEEP_WAKEWHENHIGH 0
#endif
#ifndef DEEPSLEEP_DISABLEPULL
#define DEEPSLEEP_DISABLEPULL 1
#endif
#ifndef DEEPSLEEP_WAKEUPINTERVAL
#define DEEPSLEEP_WAKEUPINTERVAL 0
#endif
#ifndef DEEPSLEEP_DELAY
#define DEEPSLEEP_DELAY 1
#endif

RTC_DATA_ATTR bool powerup = true; // variable in RTC data persists on a reboot

class DeepSleepUsermod : public Usermod {

  private:

    bool enabled = true;
    bool initDone = false;
    uint8_t wakeupPin = DEEPSLEEP_WAKEUPPIN;
    uint8_t wakeWhenHigh = DEEPSLEEP_WAKEWHENHIGH; // wake up when pin goes high if 1, triggers on low if 0
    bool noPull = true; // use pullup/pulldown resistor
    int wakeupAfter = DEEPSLEEP_WAKEUPINTERVAL; // in seconds, <=0: button only
    int sleepDelay = DEEPSLEEP_DELAY; // in seconds, 0 = immediate
    int delaycounter = 5; // delay deep sleep at bootup until preset settings are applied
    uint32_t lastLoopTime = 0;
    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];

    bool pin_is_valid(uint8_t wakePin) {
    #ifdef CONFIG_IDF_TARGET_ESP32 //ESP32: GPIOs 0,2,4, 12-15, 25-39 can be used for wake-up
      if (wakePin == 0 || wakePin == 2 || wakePin == 4 || (wakePin >= 12 && wakePin <= 15) || (wakePin >= 25 && wakePin <= 27) || (wakePin >= 32 && wakePin <= 39)) {
          return true;
      }
    #endif
    #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) //ESP32 S3 & S3: GPIOs 0-21 can be used for wake-up
      if (wakePin <= 21) {
          return true;
      }
    #endif
    #ifdef CONFIG_IDF_TARGET_ESP32C3 // ESP32 C3: GPIOs 0-5 can be used for wake-up
      if (wakePin <= 5) {
          return true;
      }
    #endif
      DEBUG_PRINTLN(F("Error: unsupported deep sleep wake-up pin"));
      return false;
    }

  public:

    inline void enable(bool enable) { enabled = enable; } // Enable/Disable the usermod
    inline bool isEnabled() { return enabled; } //Get usermod enabled/disabled state

    // setup is called at boot (or in this case after every exit of sleep mode)
    void setup() {
      //TODO: if the de-init of RTC pins is required to do it could be done here
      //rtc_gpio_deinit(wakeupPin);
      initDone = true;
    }

    void loop() {
      if (!enabled || !offMode) { // disabled or LEDs are on
        lastLoopTime = 0; // reset timer
        return;
      }

      if (sleepDelay > 0) {
        if(lastLoopTime == 0) lastLoopTime = millis(); // initialize
        if (millis() - lastLoopTime < sleepDelay * 1000) {
            return; // wait until delay is over
        }
      }

      if(powerup == false && delaycounter) { // delay sleep in case a preset is being loaded and turnOnAtBoot is disabled (handleIO() does enable offMode temporarily in this case)
        delaycounter--;
        if(delaycounter == 2 && offMode) { // force turn on, no matter the settings (device is bricked if user set sleepDelay=0, no bootup preset and turnOnAtBoot=false)
          if (briS == 0) bri = 10; // turn on at low brightness
          else bri = briS;
          strip.setBrightness(bri); // needed to make handleIO() not turn off LEDs (really? does not help in bootup preset)
          offMode = false;
          applyPresetWithFallback(0, CALL_MODE_INIT, FX_MODE_STATIC, 0); // try to apply preset 0, fallback to static
          if (rlyPin >= 0) {
            digitalWrite(rlyPin, (rlyMde ? HIGH : LOW)); // turn relay on TODO: this should be done by wled, what function to call?
          }
        }
        return;
      }

      DEBUG_PRINTLN(F("DeepSleep UM: entering deep sleep..."));
      powerup = false; // turn leds on in all subsequent bootups (overrides Turn LEDs on after power up/reset' at reboot)
      if(!pin_is_valid(wakeupPin)) return;
      esp_err_t halerror = ESP_OK;
      pinMode(wakeupPin, INPUT); // make sure GPIO is input with pullup/pulldown disabled
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); //disable all wake-up sources (just in case)

      if(wakeupAfter)
      esp_sleep_enable_timer_wakeup((uint64_t)wakeupAfter * (uint64_t)1e6); //sleep for x seconds

    #if defined(CONFIG_IDF_TARGET_ESP32C3) // ESP32 C3
    if(noPull)
      gpio_sleep_set_pull_mode((gpio_num_t)wakeupPin, GPIO_FLOATING);
    else { // enable pullup/pulldown resistor
      if(wakeWhenHigh)
        gpio_sleep_set_pull_mode((gpio_num_t)wakeupPin, GPIO_PULLDOWN_ONLY);
      else
        gpio_sleep_set_pull_mode((gpio_num_t)wakeupPin, GPIO_PULLUP_ONLY);
    }
    if(wakeWhenHigh)
      halerror = esp_deep_sleep_enable_gpio_wakeup(1<<wakeupPin, ESP_GPIO_WAKEUP_GPIO_HIGH);
    else
      halerror = esp_deep_sleep_enable_gpio_wakeup(1<<wakeupPin, ESP_GPIO_WAKEUP_GPIO_LOW);
    #else // ESP32, S2, S3
    gpio_pulldown_dis((gpio_num_t)wakeupPin); // disable internal pull resistors for GPIO use
    gpio_pullup_dis((gpio_num_t)wakeupPin);
    if(noPull) {
      rtc_gpio_pullup_dis((gpio_num_t)wakeupPin);
      rtc_gpio_pulldown_dis((gpio_num_t)wakeupPin);
    }
    else { // enable pullup/pulldown resistor for RTC use
      if(wakeWhenHigh)
        rtc_gpio_pulldown_en((gpio_num_t)wakeupPin);
      else
        rtc_gpio_pullup_en((gpio_num_t)wakeupPin);
    }
    if(wakeWhenHigh)
      halerror = esp_sleep_enable_ext0_wakeup((gpio_num_t)wakeupPin, HIGH); // only RTC pins can be used
    else
      halerror = esp_sleep_enable_ext0_wakeup((gpio_num_t)wakeupPin, LOW);
  #endif

      delay(1); // wait for pin to be ready
      if(halerror == ESP_OK) esp_deep_sleep_start(); // go into deep sleep
      else DEBUG_PRINTLN(F("sleep failed"));
    }

    //void connected() {} //unused, this is called every time the WiFi is (re)connected

void addToConfig(JsonObject& root) override
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      //save these vars persistently whenever settings are saved
      top["gpio"] = wakeupPin;
      top["wakeWhen"] = wakeWhenHigh;
      top["pull"] = noPull;
      top["wakeAfter"] = wakeupAfter;
      top["delaySleep"] = sleepDelay;
    }

    bool readFromConfig(JsonObject& root) override
    {
      // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top["gpio"], wakeupPin, DEEPSLEEP_WAKEUPPIN);
      if (!pin_is_valid(wakeupPin)) {
          wakeupPin = 0; // set to 0 if invalid
          configComplete = false; // Mark config as incomplete if pin is invalid
      }
      configComplete &= getJsonValue(top["wakeWhen"], wakeWhenHigh, DEEPSLEEP_WAKEWHENHIGH); // default to wake on low
      configComplete &= getJsonValue(top["pull"], noPull, DEEPSLEEP_DISABLEPULL); // default to no pullup/pulldown
      configComplete &= getJsonValue(top["wakeAfter"], wakeupAfter, DEEPSLEEP_WAKEUPINTERVAL);
      configComplete &= getJsonValue(top["delaySleep"], sleepDelay, DEEPSLEEP_DELAY);

      return configComplete;
    }

    /*
     * appendConfigData() is called when user enters usermod settings page
     * it may add additional metadata for certain entry fields (adding drop down is possible)
     * be careful not to add too much as oappend() buffer is limited to 3k
     */
    void appendConfigData() override
    {
      // dropdown for wakeupPin
      oappend(SET_F("dd=addDropdown('DeepSleep','gpio');"));
      for (int pin = 0; pin < 40; pin++) { // possible pins are in range 0-39
        if (pin_is_valid(pin)) {
          oappend(SET_F("addOption(dd,'"));
          oappend(String(pin).c_str());
          oappend(SET_F("',"));
          oappend(String(pin).c_str());
          oappend(SET_F(");"));
        }
      }

      oappend(SET_F("dd=addDropdown('DeepSleep','wakeWhen');"));
      oappend(SET_F("addOption(dd,'Low',0);"));
      oappend(SET_F("addOption(dd,'High',1);"));

      oappend(SET_F("addInfo('DeepSleep:pull',1,'','-up/down disable: ');")); // first string is suffix, second string is prefix
      oappend(SET_F("addInfo('DeepSleep:wakeAfter',1,'seconds <i>(0 = never)<i>');"));
      oappend(SET_F("addInfo('DeepSleep:delaySleep',1,'seconds <i>(0 = sleep at powerup)<i>');")); // first string is suffix, second string is prefix
    }

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() {
        return USERMOD_ID_DEEP_SLEEP;
    }

};

// add more strings here to reduce flash memory usage
const char DeepSleepUsermod::_name[]    PROGMEM = "DeepSleep";
const char DeepSleepUsermod::_enabled[] PROGMEM = "enabled";

static DeepSleepUsermod deep_sleep;
REGISTER_USERMOD(deep_sleep);