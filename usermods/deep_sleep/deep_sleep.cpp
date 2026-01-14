#include "wled.h"
#include "driver/rtc_io.h"
#ifndef CONFIG_IDF_TARGET_ESP32C3
  #include "soc/touch_sensor_periph.h"
#endif
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

#ifndef DEEPSLEEP_WAKEUP_TOUCH_PIN
#define DEEPSLEEP_WAKEUP_TOUCH_PIN 1
#endif
RTC_DATA_ATTR bool powerup = true; // this is first boot after power cycle. note: variable in RTC data persists on a reboot
RTC_DATA_ATTR uint8_t wakeupPreset = 0; // preset to apply after deep sleep wakeup (0 = none), set to timer macro preset

class DeepSleepUsermod : public Usermod {

  private:
    bool enabled = false; // do not enable by default
    bool initDone = false;
    uint8_t wakeupPin = DEEPSLEEP_WAKEUPPIN;
    uint8_t wakeWhenHigh = DEEPSLEEP_WAKEWHENHIGH; // wake up when pin goes high if 1, triggers on low if 0
    bool noPull = true; // use pullup/pulldown resistor
    bool enableTouchWakeup = false;
    uint8_t touchPin = DEEPSLEEP_WAKEUP_TOUCH_PIN;
    int wakeupAfter = DEEPSLEEP_WAKEUPINTERVAL; // in seconds, <=0: button only
    bool presetWake = true; // wakeup timer for preset
    int sleepDelay = DEEPSLEEP_DELAY; // in seconds, 0 = immediate
    int delaycounter = 10; // delay deep sleep at bootup until preset settings are applied, force wake up if offmode persists after bootup
    uint32_t lastLoopTime = 0;

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];

    bool pin_is_valid(uint8_t wakePin) {
    #ifdef CONFIG_IDF_TARGET_ESP32 //ESP32: GPIOs 0,2,4, 12-15, 25-39 can be used for wake-up. note: input-only GPIOs 34-39 do not have internal pull resistors
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

    // functions to calculate time difference between now and next scheduled timer event
    int calculateTimeDifference(int hour1, int minute1, int hour2, int minute2) {
      int totalMinutes1 = hour1 * 60 + minute1;
      int totalMinutes2 = hour2 * 60 + minute2;
      if (totalMinutes2 < totalMinutes1) {
        totalMinutes2 += 24 * 60;
      }
      return totalMinutes2 - totalMinutes1;
    }

    int findNextTimerInterval() {
      if (toki.getTimeSource() == TOKI_TS_NONE) {
        DEBUG_PRINTLN("DeepSleep: local time not yet synchronized, skipping timer check.");
        return -1;
      }
      int currentHour = hour(localTime);
      int currentMinute = minute(localTime);
      int currentWeekday = weekdayMondayFirst(); // 1=Monday ... 7=Sunday
      int minDifference = INT_MAX;

      for (uint8_t i = 0; i < 8; i++) {
        // check if timer is enabled and date is in range, also wakes up if no macro is used
        if ((timerWeekday[i] & 0x01) && isTodayInDateRange(((timerMonth[i] >> 4) & 0x0F), timerDay[i], timerMonth[i] & 0x0F, timerDayEnd[i])) {

          // if timer is enabled (bit0 of timerWeekday) and date is in range, check all weekdays it is set for
          for (int dayOffset = 0; dayOffset < 7; dayOffset++) {
            int checkWeekday = ((currentWeekday + dayOffset) % 7); // 1-7, check all weekdays starting from today
            if (checkWeekday == 0) {
              checkWeekday = 7; // sunday is 7 not 0
            }

            int targetHour = timerHours[i];
            int targetMinute = timerMinutes[i];
            if ((timerWeekday[i] >> (checkWeekday)) & 0x01) {
              if (dayOffset == 0 && (targetHour < currentHour || (targetHour == currentHour && targetMinute <= currentMinute)))
                continue; // skip if time has already passed today

              int timeDifference = calculateTimeDifference(currentHour, currentMinute, targetHour + (dayOffset * 24), targetMinute);
              if (timeDifference < minDifference) {
                minDifference = timeDifference;
                wakeupPreset = timerMacro[i];
              }
            }
          }
        }
      }
      return minDifference;
    }

  public:
    inline void enable(bool enable) { enabled = enable; } // Enable/Disable the usermod
    inline bool isEnabled() { return enabled; } //Get usermod enabled/disabled state

    // setup is called at boot (or in this case after every exit of sleep mode)
    void setup() {
      //TODO: if the de-init of RTC pins is required to do it could be done here
      //rtc_gpio_deinit(wakeupPin);
      #ifdef WLED_DEBUG
        DEBUG_PRINTF("sleep wakeup cause: %d\n", esp_sleep_get_wakeup_cause());
      #endif
      if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER)
        wakeupPreset = 0; // not a timed wakeup, don't apply preset
      initDone = true;
    }

    void loop() {
      if (!enabled) return;
      if (!offMode) { // LEDs are on
        lastLoopTime = 0; // reset timer
        if (delaycounter)
          delaycounter--; // decrease delay counter if LEDs are on (they are always turned on after a wake-up, see below)
        else if (wakeupPreset)
          applyPreset(wakeupPreset); // apply preset if set, this ensures macro is applied even if we missed the wake-up time
        return;
      }

      if (sleepDelay > 0) {
        powerup = false; // disable "safety" powerup sleep if delay is set
        if (lastLoopTime == 0)
          lastLoopTime = millis(); // initialize
        if (millis() - lastLoopTime < sleepDelay * 1000)
          return; // wait until delay is over
      } else if (powerup && delaycounter) {
        delaycounter--; // on first boot without sleepDelay set, do not force-turn on
        delay(1000);    // just in case: give user a short ~10s window to turn LEDs on in UI (delaycounter is 10 by default)
        return;
      }
      if (powerup == false && delaycounter) { // delay sleep in case a preset is being loaded and turnOnAtBoot is disabled (handleIO() does enable offMode temporarily in this case)
        delaycounter--;
        if (delaycounter == 1 && offMode) { // force turn on, no matter the settings (device is bricked if user set sleepDelay=0, no bootup preset and turnOnAtBoot=false)
          if (briS == 0) bri = 10; // turn on and set low brightness to avoid automatic turn off
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
      if (!pin_is_valid(wakeupPin)) return;
      esp_err_t halerror = ESP_OK;
      pinMode(wakeupPin, INPUT); // make sure GPIO is input with pullup/pulldown disabled
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); //disable all wake-up sources (just in case)

      uint32_t wakeupAfterSec = 0;
      if (presetWake) {
        int nextInterval = findNextTimerInterval();
        if (nextInterval > 1 && nextInterval < INT_MAX)
          wakeupAfterSec = (nextInterval - 1) * 60; // wakeup before next preset
      }
      if (wakeupAfter > 0) { // user-defined interval
        if (wakeupAfterSec == 0 || (uint32_t)wakeupAfter < wakeupAfterSec) {
          wakeupAfterSec = wakeupAfter;
        }
      }
      if (wakeupAfterSec > 0) {
        esp_sleep_enable_timer_wakeup(wakeupAfterSec * (uint64_t)1e6);
        DEBUG_PRINTF("wakeup after %d seconds\n", wakeupAfterSec);
      }

    #if defined(CONFIG_IDF_TARGET_ESP32C3) // ESP32 C3
      gpio_hold_dis((gpio_num_t)wakeupPin); // disable hold and configure pin
      if (wakeWhenHigh)
        halerror = esp_deep_sleep_enable_gpio_wakeup(1<<wakeupPin, ESP_GPIO_WAKEUP_GPIO_HIGH);
      else
        halerror = esp_deep_sleep_enable_gpio_wakeup(1<<wakeupPin, ESP_GPIO_WAKEUP_GPIO_LOW);
      // note: on C3 calling esp_deep_sleep_enable_gpio_wakeup() automatically enables pullup/pulldown unless we call gpio_hold_en() which overrides that
      gpio_pullup_dis((gpio_num_t)wakeupPin); // disable pull resistors by default
      gpio_pulldown_dis((gpio_num_t)wakeupPin);
      if (!noPull)  {
        if (wakeWhenHigh) {
          gpio_pulldown_en((gpio_num_t)wakeupPin);
        } else {
          gpio_pullup_en((gpio_num_t)wakeupPin);
        }
      }
      gpio_hold_en((gpio_num_t)wakeupPin); // hold the configured GPIO state during deep sleep, overrides the automatic pullup/pulldown, see note above
    #else // ESP32, S2, S3
      rtc_gpio_hold_dis((gpio_num_t)wakeupPin); // disable hold so we can (re)configure pin
      rtc_gpio_init((gpio_num_t)wakeupPin);     // hand the pin over to RTC module
      rtc_gpio_set_direction((gpio_num_t)wakeupPin, RTC_GPIO_MODE_INPUT_ONLY);
      rtc_gpio_pullup_dis((gpio_num_t)wakeupPin); // disable pull resistors by default
      rtc_gpio_pulldown_dis((gpio_num_t)wakeupPin);
      if (!noPull) {
        if (wakeWhenHigh)
          rtc_gpio_pulldown_en((gpio_num_t)wakeupPin);
        else
          rtc_gpio_pullup_en((gpio_num_t)wakeupPin);
      }
      if (wakeWhenHigh)
        halerror = esp_sleep_enable_ext1_wakeup(1ULL << wakeupPin, ESP_EXT1_WAKEUP_ANY_HIGH); // use ext1 as ext0 does not work with touch wakeup
      else
        halerror = esp_sleep_enable_ext1_wakeup(1ULL << wakeupPin, ESP_EXT1_WAKEUP_ALL_LOW);

      if (enableTouchWakeup) {
      #ifdef SOC_TOUCH_VERSION_2 // S2 and S3 use much higher thresholds, see notes in pin_manager
        touchSleepWakeUpEnable(touchPin, touchThreshold  << 4); // ESP32 S2 & S3: lower threshold = more sensitive
      #else
        touchSleepWakeUpEnable(touchPin, touchThreshold); // ESP32: use normal threshold (higher = more sensitive)
      #endif
      }
      delay(1); // wait for pins to be ready
      rtc_gpio_hold_en((gpio_num_t)wakeupPin); // latch and hold the configured GPIO state during deep sleep
    #endif
      WiFi.mode(WIFI_OFF);  // Completely shut down the Wi-Fi module
      if (halerror == ESP_OK) esp_deep_sleep_start(); // go into deep sleep
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
    #ifndef CONFIG_IDF_TARGET_ESP32C3
      top["enableTouchWakeup"] = enableTouchWakeup;
      top["touchPin"] = touchPin;
    #endif
      top["presetWake"] = presetWake;
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
    #ifndef CONFIG_IDF_TARGET_ESP32C3
      configComplete &= getJsonValue(top["enableTouchWakeup"], enableTouchWakeup);
      configComplete &= getJsonValue(top["touchPin"], touchPin, DEEPSLEEP_WAKEUP_TOUCH_PIN);
    #endif
      configComplete &= getJsonValue(top["presetWake"], presetWake);
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
    #ifndef CONFIG_IDF_TARGET_ESP32C3
      // dropdown for touch wakeupPin
      oappend(SET_F("dd=addDropdown('DeepSleep','touchPin');"));
      for (int touchchannel = 0; touchchannel < SOC_TOUCH_SENSOR_NUM; touchchannel++) {
        if (touch_sensor_channel_io_map[touchchannel] >= 0) {
          oappend(SET_F("addOption(dd,'"));
          oappend(String(touch_sensor_channel_io_map[touchchannel]).c_str());
          oappend(SET_F("',"));
          oappend(String(touch_sensor_channel_io_map[touchchannel]).c_str());
          oappend(SET_F(");"));
        }
      }
    #endif

      oappend(SET_F("dd=addDropdown('DeepSleep','wakeWhen');"));
      oappend(SET_F("addOption(dd,'Low',0);"));
      oappend(SET_F("addOption(dd,'High',1);"));

      oappend(SET_F("addInfo('DeepSleep:pull',1,'','-up/down disable: ');")); // first string is suffix, second string is prefix
      oappend(SET_F("addInfo('DeepSleep:wakeAfter',1,'seconds <i>(0 = never)<i>');"));
      oappend(SET_F("addInfo('DeepSleep:presetWake',1,'<i>(wake up before next preset timer)<i>');"));
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