// pin defaults
// for the esp32 it is best to use the ADC1: GPIO32 - GPIO39
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html
#ifndef USERMOD_BATTERY_MEASUREMENT_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define USERMOD_BATTERY_MEASUREMENT_PIN 35
  #else //ESP8266 boards
    #define USERMOD_BATTERY_MEASUREMENT_PIN A0
  #endif
#endif

// the frequency to check the battery, 30 sec
#ifndef USERMOD_BATTERY_MEASUREMENT_INTERVAL
  #define USERMOD_BATTERY_MEASUREMENT_INTERVAL 30000
#endif

// default for 18650 battery
// https://batterybro.com/blogs/18650-wholesale-battery-reviews/18852515-when-to-recycle-18650-batteries-and-how-to-start-a-collection-center-in-your-vape-shop
// Discharge voltage: 2.5 volt + .1 for personal safety
#ifndef USERMOD_BATTERY_MIN_VOLTAGE
  #ifdef USERMOD_BATTERY_USE_LIPO
    // LiPo "1S" Batteries should not be dischared below 3V !!
    #define USERMOD_BATTERY_MIN_VOLTAGE 3.2f
  #else
    #define USERMOD_BATTERY_MIN_VOLTAGE 2.6f
  #endif
#endif

//the default ratio for the voltage divider
#ifndef USERMOD_BATTERY_VOLTAGE_MULTIPLIER
  #ifdef ARDUINO_ARCH_ESP32
    #define USERMOD_BATTERY_VOLTAGE_MULTIPLIER 2.0f
  #else //ESP8266 boards
    #define USERMOD_BATTERY_VOLTAGE_MULTIPLIER 4.2f
  #endif
#endif

#ifndef USERMOD_BATTERY_MAX_VOLTAGE
  #define USERMOD_BATTERY_MAX_VOLTAGE 4.2f
#endif

// a common capacity for single 18650 battery cells is between 2500 and 3600 mAh
#ifndef USERMOD_BATTERY_TOTAL_CAPACITY
  #define USERMOD_BATTERY_TOTAL_CAPACITY 3100
#endif

// offset or calibration value to fine tune the calculated voltage
#ifndef USERMOD_BATTERY_CALIBRATION
  #define USERMOD_BATTERY_CALIBRATION 0
#endif

// calculate remaining time / the time that is left before the battery runs out of power
// #ifndef USERMOD_BATTERY_CALCULATE_TIME_LEFT_ENABLED
//   #define USERMOD_BATTERY_CALCULATE_TIME_LEFT_ENABLED false
// #endif

// auto-off feature
#ifndef USERMOD_BATTERY_AUTO_OFF_ENABLED
  #define USERMOD_BATTERY_AUTO_OFF_ENABLED true
#endif

#ifndef USERMOD_BATTERY_AUTO_OFF_THRESHOLD
  #define USERMOD_BATTERY_AUTO_OFF_THRESHOLD 10
#endif

// low power indication feature
#ifndef USERMOD_BATTERY_LOW_POWER_INDICATOR_ENABLED
  #define USERMOD_BATTERY_LOW_POWER_INDICATOR_ENABLED true
#endif

#ifndef USERMOD_BATTERY_LOW_POWER_INDICATOR_PRESET
  #define USERMOD_BATTERY_LOW_POWER_INDICATOR_PRESET 0
#endif

#ifndef USERMOD_BATTERY_LOW_POWER_INDICATOR_THRESHOLD
  #define USERMOD_BATTERY_LOW_POWER_INDICATOR_THRESHOLD 20
#endif

#ifndef USERMOD_BATTERY_LOW_POWER_INDICATOR_DURATION
  #define USERMOD_BATTERY_LOW_POWER_INDICATOR_DURATION 5
#endif