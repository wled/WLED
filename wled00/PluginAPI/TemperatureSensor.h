/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

//--------------------------------------------------------------------------------------------------

/// Interface of a sensor that can provide temperature readings.
class TemperatureSensor
{
public:
  /// Returns \c false when the sensor is not ready.
  bool isReady() { return _isTemperatureValid; }

  /// Get the temperature, with the unit according to the global setting of \a useFahrenheit()
  float temperature() { return useFahrenheit() ? temperatureF() : temperatureC(); }

  /// Get the temperature in °C
  float temperatureC() { return do_getTemperatureC(); }

  /// Get the temperature in °F
  float temperatureF() { return c2f(do_getTemperatureC()); }

  /** Determines the unit for \c temperature() - default is °C
   * @see PluginManager::setUseFahrenheit()
   */
  static bool useFahrenheit() { return _useFahrenheit; }

  /// Small helper to convert °C into °F
  static float c2f(float degC) { return degC * 9.0f / 5.0f + 32.0f; }

protected:
  /// Get the plugin's temperature reading in °C
  virtual float do_getTemperatureC() = 0;

  /// The plugin must set this to \c true after the sensor has been initialized.
  bool _isTemperatureValid = false;

private:
  friend class PluginManager;
  static bool _useFahrenheit;
};

//--------------------------------------------------------------------------------------------------
