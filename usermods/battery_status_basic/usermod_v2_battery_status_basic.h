#pragma once

#include "wled.h"




// pin defaults
// for the esp32 it is best to use the ADC1: GPIO32 - GPIO39
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html
#ifndef USERMOD_BATTERY_MEASUREMENT_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define USERMOD_BATTERY_MEASUREMENT_PIN 32
  #else //ESP8266 boards
    #define USERMOD_BATTERY_MEASUREMENT_PIN A0
  #endif
#endif

// esp32 has a 12bit adc resolution
// esp8266 only 10bit
#ifndef USERMOD_BATTERY_ADC_PRECISION
  #ifdef ARDUINO_ARCH_ESP32
    // 12 bits
    #define USERMOD_BATTERY_ADC_PRECISION 4095.0f
  #else
    // 10 bits
    #define USERMOD_BATTERY_ADC_PRECISION 1024.0f
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
  #define USERMOD_BATTERY_MIN_VOLTAGE 2.6f
#endif

#ifndef USERMOD_BATTERY_MAX_VOLTAGE
  #define USERMOD_BATTERY_MAX_VOLTAGE 4.2f
#endif

class UsermodBatteryBasic : public Usermod 
{
  private:
    // battery pin can be defined in my_config.h
    int8_t batteryPin = USERMOD_BATTERY_MEASUREMENT_PIN;
    // how often to read the battery voltage
    unsigned long readingInterval = USERMOD_BATTERY_MEASUREMENT_INTERVAL;
    unsigned long nextReadTime = 0;
    unsigned long lastReadTime = 0;
    // battery min. voltage
    float minBatteryVoltage = USERMOD_BATTERY_MIN_VOLTAGE;
    // battery max. voltage
    float maxBatteryVoltage = USERMOD_BATTERY_MAX_VOLTAGE;
    // 0 - 1024 for esp8266 (10-bit resolution)
    // 0 - 4095 for esp32 (Default is 12-bit resolution)
    float adcPrecision = USERMOD_BATTERY_ADC_PRECISION;
    // raw analog reading 
    float rawValue = 0.0;
    // calculated voltage            
    float voltage = 0.0;
    // mapped battery level based on voltage
    long batteryLevel = 0;
    bool initDone = false;
    bool initializing = true;


    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _readInterval[];
    

    // custom map function
    // https://forum.arduino.cc/t/floating-point-using-map-function/348113/2
    double mapf(double x, double in_min, double in_max, double out_min, double out_max) 
    {
      return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    float truncate(float val, byte dec) 
    {
      float x = val * pow(10, dec);
      float y = round(x);
      float z = x - y;
      if ((int)z == 5)
      {
          y++;
      }
      x = y / pow(10, dec);
      return x;
    }



  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() 
    {
      #ifdef ARDUINO_ARCH_ESP32
        DEBUG_PRINTLN(F("Allocating battery pin..."));
        if (batteryPin >= 0 && pinManager.allocatePin(batteryPin, false)) 
        {
          DEBUG_PRINTLN(F("Battery pin allocation succeeded."));
        } else {
          if (batteryPin >= 0) DEBUG_PRINTLN(F("Battery pin allocation failed."));
          batteryPin = -1;  // allocation failed
        }
      #else //ESP8266 boards have only one analog input pin A0

        pinMode(batteryPin, INPUT);
      #endif

      nextReadTime = millis() + readingInterval;
      lastReadTime = millis();

      initDone = true;
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() 
    {
      //Serial.println("Connected to WiFi!");
    }


    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     * 
     */
    void loop() 
    {
      if(strip.isUpdating()) return;

      // check the battery level every USERMOD_BATTERY_MEASUREMENT_INTERVAL (ms)
      if (millis() < nextReadTime) return;


      nextReadTime = millis() + readingInterval;
      lastReadTime = millis();
      initializing = false;

      // read battery raw input
      rawValue = analogRead(batteryPin);

      // calculate the voltage     
      voltage = (rawValue / adcPrecision) * maxBatteryVoltage ;
      // check if voltage is within specified voltage range
      voltage = voltage<minBatteryVoltage||voltage>maxBatteryVoltage?-1.0f:voltage;

      // translate battery voltage into percentage
      /*
        the standard "map" function doesn't work
        https://www.arduino.cc/reference/en/language/functions/math/map/  notes and warnings at the bottom
      */
      batteryLevel = mapf(voltage, minBatteryVoltage, maxBatteryVoltage, 0, 100);


      // SmartHome stuff
      if (WLED_MQTT_CONNECTED) {
        char subuf[64];
        strcpy(subuf, mqttDeviceTopic);
        strcat_P(subuf, PSTR("/voltage"));
        mqtt->publish(subuf, 0, false, String(voltage).c_str());
      }

    }


    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root)
    {
      
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      // info modal display names
      JsonArray batteryPercentage = user.createNestedArray("Battery level");
      JsonArray batteryVoltage = user.createNestedArray("Battery voltage");

      if (initializing) {
        batteryPercentage.add((nextReadTime - millis()) / 1000);
        batteryPercentage.add(" sec");
        batteryVoltage.add((nextReadTime - millis()) / 1000);
        batteryVoltage.add(" sec");
        return;
      }

      if(batteryLevel < 0) {
        batteryPercentage.add(F("invalid"));
      } else {
        batteryPercentage.add(batteryLevel);
      }
      batteryPercentage.add(F(" %"));

      if(voltage < 0) {
        batteryVoltage.add(F("invalid"));
      } else {
        batteryVoltage.add(truncate(voltage, 2));
      }
      batteryVoltage.add(F(" V"));
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    /*
    void addToJsonState(JsonObject& root)
    {

    }
    */


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    /*
    void readFromJsonState(JsonObject& root)
    {
    }
    */


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
    void addToConfig(JsonObject& root)
    {
      // created JSON object: 
      /*
      {
        "Battery-Level": {
          "pin": "A0",              <--- only when using esp32 boards
          "minBatteryVoltage": 2.6, 
          "maxBatteryVoltage": 4.2,
          "read-interval-ms": 30000  
        }
      }
      */ 
      JsonObject battery = root.createNestedObject(FPSTR(_name)); // usermodname
      #ifdef ARDUINO_ARCH_ESP32
        battery["pin"] = batteryPin;                              // usermodparam
      #endif
      battery["minBatteryVoltage"] = minBatteryVoltage;           // usermodparam
      battery["maxBatteryVoltage"] = maxBatteryVoltage;           // usermodparam
      battery[FPSTR(_readInterval)] = readingInterval;

      DEBUG_PRINTLN(F("Battery config saved."));
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
    bool readFromConfig(JsonObject& root)
    {
      // looking for JSON object: 
      /*
      {
        "BatteryLevel": {
          "pin": "A0",              <--- only when using esp32 boards
          "minBatteryVoltage": 2.6, 
          "maxBatteryVoltage": 4.2,
          "read-interval-ms": 30000  
        }
      }
      */
      #ifdef ARDUINO_ARCH_ESP32
        int8_t newBatteryPin = batteryPin;
      #endif

      JsonObject battery = root[FPSTR(_name)];
      if (battery.isNull()) 
      {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      #ifdef ARDUINO_ARCH_ESP32
        newBatteryPin     = battery["pin"] | newBatteryPin;
      #endif
      minBatteryVoltage   = battery["minBatteryVoltage"] | minBatteryVoltage;
      //minBatteryVoltage = min(12.0f, (int)readingInterval);
      maxBatteryVoltage   = battery["maxBatteryVoltage"] | maxBatteryVoltage;
      //maxBatteryVoltage = min(14.4f, max(3.3f,(int)readingInterval));
      readingInterval     = battery["read-interval-ms"] | readingInterval;
      readingInterval     = max(3000, (int)readingInterval); // minimum repetition is >5000ms (5s)

      DEBUG_PRINT(FPSTR(_name));

      #ifdef ARDUINO_ARCH_ESP32
        if (!initDone) 
        {
          // first run: reading from cfg.json
          newBatteryPin = batteryPin;
          DEBUG_PRINTLN(F(" config loaded."));
        } 
        else 
        {
          DEBUG_PRINTLN(F(" config (re)loaded."));

          // changing paramters from settings page
          if (newBatteryPin != batteryPin) 
          {
            // deallocate pin
            pinManager.deallocatePin(batteryPin);
            batteryPin = newBatteryPin;
            // initialise
            setup();
          }
        }
      #endif

      return !battery[FPSTR(_readInterval)].isNull();
    }

   
    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_BATTERY_STATUS_BASIC;
    }
};

// strings to reduce flash memory usage (used more than twice)
const char UsermodBatteryBasic::_name[]         PROGMEM = "Battery-level";
const char UsermodBatteryBasic::_readInterval[] PROGMEM = "read-interval-ms";