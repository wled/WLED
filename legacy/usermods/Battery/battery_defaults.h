#ifndef UMBDefaults_h
#define UMBDefaults_h

#include "wled.h"

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

// The initial delay before the first battery voltage reading after power-on.
// This allows the voltage to stabilize before readings are taken, improving accuracy of initial reading.
#ifndef USERMOD_BATTERY_INITIAL_DELAY
  #define USERMOD_BATTERY_INITIAL_DELAY 10000 // (milliseconds)
#endif

// the frequency to check the battery, 30 sec
#ifndef USERMOD_BATTERY_MEASUREMENT_INTERVAL
  #define USERMOD_BATTERY_MEASUREMENT_INTERVAL 30000
#endif


/* Default Battery Type
 * 0 = unkown
 * 1 = Lipo
 * 2 = Lion
 */
#ifndef USERMOD_BATTERY_DEFAULT_TYPE
  #define USERMOD_BATTERY_DEFAULT_TYPE 0
#endif
/*
 *
 *  Unkown 'Battery' defaults
 *
 */
#ifndef USERMOD_BATTERY_UNKOWN_MIN_VOLTAGE
  // Extra save defaults
  #define USERMOD_BATTERY_UNKOWN_MIN_VOLTAGE 3.3f
#endif
#ifndef USERMOD_BATTERY_UNKOWN_MAX_VOLTAGE
  #define USERMOD_BATTERY_UNKOWN_MAX_VOLTAGE 4.2f
#endif

/*
 *
 *  Lithium polymer (Li-Po) defaults
 *
 */
#ifndef USERMOD_BATTERY_LIPO_MIN_VOLTAGE
  // LiPo "1S" Batteries should not be dischared below 3V !!
  #define USERMOD_BATTERY_LIPO_MIN_VOLTAGE 3.2f
#endif
#ifndef USERMOD_BATTERY_LIPO_MAX_VOLTAGE
  #define USERMOD_BATTERY_LIPO_MAX_VOLTAGE 4.2f
#endif

/*
 *
 *  Lithium-ion (Li-Ion) defaults
 *
 */
#ifndef USERMOD_BATTERY_LION_MIN_VOLTAGE
  // default for 18650 battery
  #define USERMOD_BATTERY_LION_MIN_VOLTAGE 2.6f
#endif
#ifndef USERMOD_BATTERY_LION_MAX_VOLTAGE
  #define USERMOD_BATTERY_LION_MAX_VOLTAGE 4.2f
#endif

// the default ratio for the voltage divider
#ifndef USERMOD_BATTERY_VOLTAGE_MULTIPLIER
  #ifdef ARDUINO_ARCH_ESP32
    #define USERMOD_BATTERY_VOLTAGE_MULTIPLIER 2.0f
  #else //ESP8266 boards
    #define USERMOD_BATTERY_VOLTAGE_MULTIPLIER 4.2f
  #endif
#endif

#ifndef USERMOD_BATTERY_AVERAGING_ALPHA
  #define USERMOD_BATTERY_AVERAGING_ALPHA 0.1f
#endif

// offset or calibration value to fine tune the calculated voltage
#ifndef USERMOD_BATTERY_CALIBRATION
  #define USERMOD_BATTERY_CALIBRATION 0
#endif

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

// battery types
typedef enum
{
  unknown=0,
  lipo=1,
  lion=2
} batteryType;

// used for initial configuration after boot 
typedef struct bconfig_t 
{
  batteryType type;
  float minVoltage;
  float maxVoltage;
  float voltage;          // current voltage
  int8_t level;           // current level
  float calibration;      // offset or calibration value to fine tune the calculated voltage
  float voltageMultiplier;
} batteryConfig;

#endif