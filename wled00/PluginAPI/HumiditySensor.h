/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

//--------------------------------------------------------------------------------------------------

/// Interface of a sensor that can provide humidity readings.
class HumiditySensor
{
public:
  /// Returns \c false when the sensor is not ready.
  bool isReady() { return _isHumidityValid; }

  /// Get the humidity in % rel.
  float humidity() { return do_getHumidity(); }

protected:
  /// Get the plugin's humidity reading in % rel.
  virtual float do_getHumidity() = 0;

  /// The plugin must set this to \c true after the sensor has been initialized.
  bool _isHumidityValid = false;
};

//--------------------------------------------------------------------------------------------------
