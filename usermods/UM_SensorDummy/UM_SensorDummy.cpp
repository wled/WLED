/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"

//--------------------------------------------------------------------------------------------------

/** Dummy usermod implementation that simulates random sensor readings.
 */
class UM_SensorDummy : public Usermod
{

  // ----- usermod functions -----

  void setup() override {}

  void loop() override
  {
    const auto now = millis();
    if (now < _nextUpdateTime)
      return;

    _temperatureSensor = readTemperature();
    _humiditySensor = readHumidity();

    _nextUpdateTime = now + 20; // 50 sensor updates per second
  }

  uint8_t getSensorCount() override { return 2; }

  Sensor *getSensor(uint8_t index) override { return index == 0 ? &_temperatureSensor : &_humiditySensor; }

  // ----- internal processing functions -----

  /// The dummy implementation to simulate temperature values (based on perlin noise).
  float readTemperature()
  {
    const int32_t raw = perlin16(strip.now * 8) - 0x8000;
    // simulate some random temperature around 20°C
    return 20.0f + raw / 65535.0f * 30.0f;
  }

  /// The dummy implementation to simulate humidity values (a sine wave).
  float readHumidity()
  {
    const int32_t raw = beatsin16_t(1);
    // simulate some random humidity between 10% and 90%
    return 10.0f + raw / 65535.0f * 80.0f;
  }

  // ----- member variables -----

  uint32_t _nextUpdateTime = 0;
  EasySensor _temperatureSensor{"Dummy-Temp", makeChannelProps_Temperature()};
  EasySensor _humiditySensor{"Dummy-Hum", makeChannelProps_Humidity()};
};

//--------------------------------------------------------------------------------------------------

static UM_SensorDummy um_SensorDummy;
REGISTER_USERMOD(um_SensorDummy);
