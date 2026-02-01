/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"

//--------------------------------------------------------------------------------------------------

/** Dummy usermod implementation that simulates random sensor readings.
 */
class UM_SensorDummy : public Usermod, public Sensor
{
public:
  UM_SensorDummy() : Sensor{"SEF", 4} {}

  // ----- usermod functions -----

  void setup() override {}

  void loop() override
  {
    const auto now = millis();
    if (now < _nextUpdateTime)
      return;
    readWeatherStation();
    _nextUpdateTime = now + 20; // 50 sensor updates per second
  }

  uint8_t getSensorCount() override { return 2; }

  Sensor *getSensor(uint8_t index) override
  {
    if (index != 0)
      return this;
    return &_sensorArray;
  }

  bool do_isSensorReady() override { return true; }
  SensorValue do_getSensorChannelValue(uint8_t channelIndex) override { return readSEF(channelIndex); }
  const SensorChannelProps &do_getSensorChannelProperties(uint8_t channelIndex) override { return _localSensorProps[channelIndex]; }

  // ----- internal processing functions -----

  void readWeatherStation()
  {
    _sensorArray.set(0, 1012.34f);
    _sensorArray.set(1, readTemperature());
    _sensorArray.set(2, readHumidity());
#if (0) // Battery is empty :-(
    _sensorArray.set(3, readTemperature() - 3.0f);
    _sensorArray.set(4, readHumidity() - 5.0f);
#endif
  }

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

  float readSEF(uint8_t index)
  {
    if (index >= 3)
    {
      const int32_t raw = abs(beat16(20) - 0x8000);
      return raw / 32767.0f * 100.0f;
    }
    const int32_t raw = beatsin16_t(40, 0, 0xFFFF, 0, (index * 0xFFFF) / 3);
    return 90.0f + raw / 65535.0f * 90.0f;
  }

  // ----- member variables -----

  uint32_t _nextUpdateTime = 0;

  const SensorChannelPropsArray<4> _localSensorProps =
      {{makeChannelProps_Float("deltaX", {"offset", "°rad"}, 0.0f, 360.0f),
        makeChannelProps_Float("deltaY", {"offset", "°rad"}, 0.0f, 360.0f),
        makeChannelProps_Float("deltaZ", {"offset", "°rad"}, 0.0f, 360.0f),
        makeChannelProps_Float("deltaT", {"jitter", "µs"}, -1000.0f, 1000.0f)}};

  EasySensorArray<5> _sensorArray{"Weather Station",
                                  {{SensorChannelProps{"Barometer",
                                                       SensorQuantity::AirPressure(), 950.0f, 1050.0f},
                                    makeChannelProps_Temperature("Indoor Temp."),
                                    makeChannelProps_Humidity("Indoor Hum."),
                                    makeChannelProps_Temperature("Outdoor Temp."),
                                    makeChannelProps_Humidity("Outdoor Hum.")}}};
};

//--------------------------------------------------------------------------------------------------

static UM_SensorDummy um_SensorDummy;
REGISTER_USERMOD(um_SensorDummy);
