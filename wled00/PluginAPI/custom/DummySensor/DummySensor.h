/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

#include "PluginAPI/PluginMacros.h"

//--------------------------------------------------------------------------------------------------

/** This is a plugin API that is very specific only for the DummySensor usermod.
 * Other usermods or effects can directly interact with the usermod through this API.
 * They can determine at runtinme if the usermod is included in the WLED binary or not.
 */
class DummySensor
{
public:
  virtual void enableTemperatureSensor() = 0;
  virtual void disableTemperatureSensor() = 0;
  virtual bool isTemperatureSensorEnabled() = 0;

  virtual void enableHumiditySensor() = 0;
  virtual void disableHumiditySensor() = 0;
  virtual bool isHumiditySensorEnabled() = 0;
};

DECLARE_PLUGIN_API(DummySensor);

//--------------------------------------------------------------------------------------------------
