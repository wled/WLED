/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"
#include "PluginAPI/custom/DummySensor/DummySensor.h"

//--------------------------------------------------------------------------------------------------

static constexpr char _name[] = "Dummy-Sensor";

/** Dummy usermod implementation that simulates random sensor readings.
 */
class UM_DummySensor : public Usermod, public DummySensor, public TemperatureSensor, public HumiditySensor
{

  // ----- usermod functions -----

  void setup() override
  {
    // register sensor plugins
    pluginManager.registerTemperatureSensor(*this, _name);
    pluginManager.registerHumiditySensor(*this, _name);
    // this dummy can deliver readings instantly
    _isTemperatureValid = true;
    _isHumidityValid = true;
  }

  void loop() override {}

  // ----- DummySensor (our own) custom plugin functions -----

  void enableTemperatureSensor() override
  {
    if (!_isTemperatureSensorEnabled)
      pluginManager.registerTemperatureSensor(*this, _name);
    _isTemperatureSensorEnabled = true;
  }

  void disableTemperatureSensor() override
  {
    if (_isTemperatureSensorEnabled)
      pluginManager.unregisterTemperatureSensor(*this);
    _isTemperatureSensorEnabled = false;
  }

  bool isTemperatureSensorEnabled() override { return _isTemperatureSensorEnabled; }

  void enableHumiditySensor() override
  {
    if (!_isHumiditySensorEnabled)
      pluginManager.registerHumiditySensor(*this, _name);
    _isHumiditySensorEnabled = true;
  }

  void disableHumiditySensor() override
  {
    if (_isHumiditySensorEnabled)
      pluginManager.unregisterHumiditySensor(*this);
    _isHumiditySensorEnabled = false;
  }

  bool isHumiditySensorEnabled() override { return _isHumiditySensorEnabled; }

  // ----- TemperatureSensor plugin functions -----

  /// @copydoc TemperatureSensor::do_getTemperatureC()
  float do_getTemperatureC() override { return readTemperature(); }

  // ----- HumiditySensor plugin functions -----

  /// @copydoc HumiditySensor::do_getHumidity()
  float do_getHumidity() override { return readHumidity(); }

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

  bool _isTemperatureSensorEnabled = false;
  bool _isHumiditySensorEnabled = false;
};

//--------------------------------------------------------------------------------------------------

static UM_DummySensor um_DummySensor;
REGISTER_USERMOD(um_DummySensor);
DEFINE_PLUGIN_API(DummySensor, um_DummySensor);
