/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

#include <vector>

#include "PinUser.h"
#include "HumiditySensor.h"
#include "TemperatureSensor.h"

// set this via PlatformIO to disable PluginManager's UI info entries for demo & debugging
// #define PLUGINMGR_DISABLE_UI

// set this via PlatformIO for sorting PluginManager's UI info entries by plugin name
// #define PLUGINMGR_SORT_UI_INFO_BY_NAME

//--------------------------------------------------------------------------------------------------

/** The central component that orchestrates all plugins.
 * @note All strings (names) that are provided from the plugins are only pointer-copied.
 * They must remain valid as long as the plugin is registered!
 */
class PluginManager
{
public:
  // no copy & move
  PluginManager(const PluginManager &) = delete;
  PluginManager &operator=(const PluginManager &) = delete;
  PluginManager() = default;

  // ----- PinUser handling -----

  /** Try to register the desired pins of a plugin.
   * @note Use this method also for updating the pin configuration (even if already registered).
   */
  bool registerPinUser(PinUser &user, uint8_t pinCount, PinConfig *pinConfig, const char *pluginName);

  /// Convenience method for registering one single pin.
  bool registerPinUser(PinUser &user, PinConfig &pinConfig, const char *pluginName)
  {
    return registerPinUser(user, 1, &pinConfig, pluginName);
  }

  /// Convenience method for registering an array of pins.
  template <size_t NUM_PINS>
  bool registerPinUser(PinUser &user, std::array<PinConfig, NUM_PINS> &pinConfig, const char *pluginName)
  {
    return registerPinUser(user, NUM_PINS, pinConfig.data(), pluginName);
  }

  /// Revoke all of a plugin's registered pins.
  void unregisterPinUser(PinUser &user);

  // ----- Sensor handling -----

  // Register a plugin as a specific sensor.
  void registerTemperatureSensor(TemperatureSensor &sensor, const char *pluginName);
  void registerHumiditySensor(HumiditySensor &sensor, const char *pluginName);

  // Revoke a plugin's registration as specific sensor.
  void unregisterTemperatureSensor(TemperatureSensor &sensor);
  void unregisterHumiditySensor(HumiditySensor &sensor);

  // Get a registered sensor, or nullptr if there is none.
  TemperatureSensor *getTemperatureSensor(uint8_t index = 0) { return index < _temperatureSensors.size() ? _temperatureSensors[index].first : nullptr; }
  HumiditySensor *getHumiditySensor(uint8_t index = 0) { return index < _humiditySensors.size() ? _humiditySensors[index].first : nullptr; }

  size_t getTemperatureSensorCount() const { return _temperatureSensors.size(); }
  size_t getHumiditySensorCount() const { return _humiditySensors.size(); }

  const char *getSensorName(const TemperatureSensor *sensor) const;
  const char *getSensorName(const HumiditySensor *sensor) const;

  // ----- WLED internal stuff -----

  /** Globally set the unit for \c TemperatureSensor::temperature() - default is °C
   * @note This setting should only be configured via UI, and not via usermod or effect.
   */
  static void setUseFahrenheit(bool enabled) { TemperatureSensor::_useFahrenheit = enabled; }

#ifndef PLUGINMGR_DISABLE_UI
  void addToJsonInfo(JsonObject &root, bool advanced = true);
#else
  void addToJsonInfo(JsonObject &root, bool advanced = true) {}
#endif

private:
  bool rollbackPinRegistration(PinUser &user, uint8_t pinCount, PinConfig *pinConfig);
#ifndef PLUGINMGR_DISABLE_UI
#ifdef PLUGINMGR_SORT_UI_INFO_BY_NAME
  void addUiInfo(JsonObject &root, bool advanced);
#else
  void addUiInfo_basic(JsonObject &user);
  void addUiInfo_advanced(JsonObject &user);
#endif // PLUGINMGR_SORT_UI_INFO_BY_NAME
  void addUiInfo_plugins(JsonObject &user);
#endif // PLUGINMGR_DISABLE_UI
  const char *getPluginName(const PinUser *user) const;

  // TODO(optimization) To save precious DRAM, use std::pmr::vector with a memory resource that
  // allocates PSRAM instead. Unfortunately, that is a C++17 feature...
  using PinUsers = std::vector<std::pair<PinUser *, const char *>>;
  using PinUserConfigs = std::vector<std::pair<PinUser *, PinConfig>>; // yes, we store a _copy_ of the config!
  PinUsers _pinUsers;
  PinUserConfigs _pinUserConfigs;

  using TemperatureSensors = std::vector<std::pair<TemperatureSensor *, const char *>>;
  TemperatureSensors _temperatureSensors;

  using HumiditySensors = std::vector<std::pair<HumiditySensor *, const char *>>;
  HumiditySensors _humiditySensors;
};

/// The global PluginManager instance.
extern PluginManager pluginManager;

//--------------------------------------------------------------------------------------------------
