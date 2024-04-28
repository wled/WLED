#pragma once

#include "wled.h"
#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif


#include <dht_nonblocking.h>

// USERMOD_DHT_DHTTYPE:
//   11   // DHT 11
//   21   // DHT 21
//   22   // DHT 22  (AM2302), AM2321 *** default
#ifndef USERMOD_DHT_DHTTYPE
#define USERMOD_DHT_DHTTYPE 22
#endif

#if USERMOD_DHT_DHTTYPE == 11
#define DHTTYPE DHT_TYPE_11
#elif USERMOD_DHT_DHTTYPE == 21
#define DHTTYPE DHT_TYPE_21
#elif USERMOD_DHT_DHTTYPE == 22
#define DHTTYPE DHT_TYPE_22
#endif

// Connect pin 1 (on the left) of the sensor to +5V
//   NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
//   to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
//   NOTE: Pin defaults below are for QuinLed Dig-Uno's Q2 on the board
// Connect pin 4 (on the right) of the sensor to GROUND
//   NOTE: If using a bare sensor (AM*), Connect a 10K resistor from pin 2
//   (data) to pin 1 (power) of the sensor. DHT* boards have the pullup already

#ifdef USERMOD_DHT_PIN
#define DHTPIN USERMOD_DHT_PIN
#else
#ifdef ARDUINO_ARCH_ESP32
#define DHTPIN 21
#else //ESP8266 boards
#define DHTPIN 4
#endif
#endif

// the frequency to check sensor, 1 minute
#ifndef USERMOD_DHT_MEASUREMENT_INTERVAL
#define USERMOD_DHT_MEASUREMENT_INTERVAL 60000
#endif

// how many seconds after boot to take first measurement, 90 seconds
// 90 gives enough time to OTA update firmware if this crashes
#ifndef USERMOD_DHT_FIRST_MEASUREMENT_AT
#define USERMOD_DHT_FIRST_MEASUREMENT_AT 90000
#endif

// from COOLDOWN_TIME in dht_nonblocking.cpp
#define DHT_TIMEOUT_TIME  10000

DHT_nonblocking dht_sensor(DHTPIN, DHTTYPE);

class UsermodDHT : public Usermod {
  private:
    unsigned long nextReadTime = 0;
    unsigned long lastReadTime = 0;
    float humidity, temperature = 0;
    bool initializing = true;
    bool disabled = false;
    #ifdef USERMOD_DHT_MQTT
    char dhtMqttTopic[64];
    size_t dhtMqttTopicLen;
    #endif
    #ifdef USERMOD_DHT_STATS
    unsigned long nextResetStatsTime = 0;
    uint16_t updates = 0;
    uint16_t clean_updates = 0;
    uint16_t errors = 0;
    unsigned long maxDelay = 0;
    unsigned long currentIteration = 0;
    unsigned long maxIteration = 0;
    #endif

  public:
    void setup() {
      nextReadTime = millis() + USERMOD_DHT_FIRST_MEASUREMENT_AT;
      lastReadTime = millis();
      #ifdef USERMOD_DHT_MQTT
      sprintf(dhtMqttTopic, "%s/dht", mqttDeviceTopic);
      dhtMqttTopicLen = strlen(dhtMqttTopic);
      #endif
      #ifdef USERMOD_DHT_STATS
      nextResetStatsTime = millis() + 60*60*1000;
      #endif
    }

    void loop() {
      if (disabled) {
        return;
      }
      if (millis() < nextReadTime) {
        return;
      }

      #ifdef USERMOD_DHT_STATS
      if (millis() >= nextResetStatsTime) {
        nextResetStatsTime += 60*60*1000;
        errors = 0;
        updates = 0;
        clean_updates = 0;
      }
      unsigned long dcalc = millis();
      if (currentIteration == 0) {
        currentIteration = millis();
      }
      #endif

      float tempC;
      if (dht_sensor.measure(&tempC, &humidity)) {
        #ifdef USERMOD_DHT_CELSIUS
        temperature = tempC;
        #else
        temperature = tempC * 9 / 5 + 32;
        #endif

        #ifdef USERMOD_DHT_MQTT
        // 10^n where n is number of decimal places to display in mqtt message. Please adjust buff size together with this constant
        #define FLOAT_PREC 100
        if (WLED_MQTT_CONNECTED) {
          char buff[10];

          strcpy(dhtMqttTopic + dhtMqttTopicLen, "/temperature");
          sprintf(buff, "%d.%d", (int)temperature, ((int)(temperature * FLOAT_PREC)) % FLOAT_PREC);
          mqtt->publish(dhtMqttTopic, 0, false, buff);

          sprintf(buff, "%d.%d", (int)humidity, ((int)(humidity * FLOAT_PREC)) % FLOAT_PREC);
          strcpy(dhtMqttTopic + dhtMqttTopicLen, "/humidity");
          mqtt->publish(dhtMqttTopic, 0, false, buff);

          dhtMqttTopic[dhtMqttTopicLen] = '\0';
        }
        #undef FLOAT_PREC
        #endif

        nextReadTime = millis() + USERMOD_DHT_MEASUREMENT_INTERVAL;
        lastReadTime = millis();
        initializing = false;

        #ifdef USERMOD_DHT_STATS
        unsigned long icalc = millis() - currentIteration;
        if (icalc > maxIteration) {
          maxIteration = icalc;
        }
        if (icalc > DHT_TIMEOUT_TIME) {
          errors += icalc/DHT_TIMEOUT_TIME;
        } else {
          clean_updates += 1;
        }
        updates += 1;
        currentIteration = 0;

        #endif
      }

      #ifdef USERMOD_DHT_STATS
      dcalc = millis() - dcalc;
      if (dcalc > maxDelay) {
        maxDelay = dcalc;
      }
      #endif

      if (((millis() - lastReadTime) > 10*USERMOD_DHT_MEASUREMENT_INTERVAL)) {
        disabled = true;
      }
    }

    void addToJsonInfo(JsonObject& root) {
      if (disabled) {
        return;
      }
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray temp = user.createNestedArray("Temperature");
      JsonArray hum = user.createNestedArray("Humidity");

      #ifdef USERMOD_DHT_STATS
      JsonArray next = user.createNestedArray("next");
      if (nextReadTime >= millis()) {
        next.add((nextReadTime - millis()) / 1000);
        next.add(" sec until read");
      } else {
        next.add((millis() - nextReadTime) / 1000);
        next.add(" sec active reading");
      }

      JsonArray last = user.createNestedArray("last");
      last.add((millis() - lastReadTime) / 60000);
      last.add(" min since read");

      JsonArray err = user.createNestedArray("errors");
      err.add(errors);
      err.add(" Errors");

      JsonArray upd = user.createNestedArray("updates");
      upd.add(updates);
      upd.add(" Updates");

      JsonArray cupd = user.createNestedArray("cleanUpdates");
      cupd.add(clean_updates);
      cupd.add(" Updates");

      JsonArray iter = user.createNestedArray("maxIter");
      iter.add(maxIteration);
      iter.add(" ms");

      JsonArray delay = user.createNestedArray("maxDelay");
      delay.add(maxDelay);
      delay.add(" ms");
      #endif

      if (initializing) {
        // if we haven't read the sensor yet, let the user know
        // that we are still waiting for the first measurement
        temp.add((nextReadTime - millis()) / 1000);
        temp.add(" sec until read");
        hum.add((nextReadTime - millis()) / 1000);
        hum.add(" sec until read");
        return;
      }

      hum.add(humidity);
      hum.add("%");

      temp.add(temperature);
      #ifdef USERMOD_DHT_CELSIUS
      temp.add("°C");
      #else
      temp.add("°F");
      #endif
    }

    uint16_t getId()
    {
      return USERMOD_ID_DHT;
    }

};
