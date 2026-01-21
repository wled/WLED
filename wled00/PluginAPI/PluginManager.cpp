/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include <algorithm>
#include <map>
#include "wled.h"
#include "PluginManager.h"

//--------------------------------------------------------------------------------------------------

namespace
{

  static const char *unknownName = "[???]";
  static const char *notAvailable = "[n/a]";

#ifdef PLUGINMGR_SORT_UI_INFO_BY_NAME
  class InfoDataBuilder
  {
    using DataMap = std::map<const char *, String>;

  public:
    void add(const char *key, const String &value)
    {
      auto &entry = _dict[key];
      if (!entry.isEmpty())
        entry += "<br>";
      entry += value;
    }

    using const_iterator = DataMap::const_iterator;
    const_iterator begin() const { return _dict.begin(); }
    const_iterator end() const { return _dict.end(); }

  private:
    DataMap _dict;
  };
#endif // PLUGINMGR_SORT_UI_INFO_BY_NAME

  bool isOutputPin(PinType pinType)
  {
    switch (pinType)
    {
    // case PinType::Digital_in:
    case PinType::Digital_out:
    // case PinType::Analog_in:
    case PinType::PWM_out:
    case PinType::I2C_scl:
    case PinType::I2C_sda:
    case PinType::SPI_sclk:
    case PinType::SPI_mosi:
    // case PinType::SPI_miso:
    case PinType::OneWire:
      return true;
    default:
      return false;
    }
  }

}

//--------------------------------------------------------------------------------------------------

const char *getPinName(PinType pinType) // only needed for allocations at PinManager
{
  switch (pinType)
  {
  case PinType::Digital_in:
    return "Digital in";
  case PinType::Digital_out:
    return "Digital out";
  case PinType::Analog_in:
    return "Analog in";
  case PinType::PWM_out:
    return "PWM out";
  case PinType::I2C_scl:
    return "I2C SCL";
  case PinType::I2C_sda:
    return "I2C SDA";
  case PinType::SPI_sclk:
    return "SPI SCLK";
  case PinType::SPI_mosi:
    return "SPI MOSI";
  case PinType::SPI_miso:
    return "SPI MISO";
  case PinType::OneWire:
    return "OneWire";
  default:
    return unknownName;
  }
}

//--------------------------------------------------------------------------------------------------

// ----- PinUser handling -----

bool PluginManager::registerPinUser(PinUser &user, uint8_t pinCount, PinConfig *pinConfig, const char *pluginName)
{
  unregisterPinUser(user); // always remove previous registration

  auto begin = pinConfig;
  auto end = begin + pinCount;
  for (auto itr = begin; itr != end; ++itr)
  {
    if (itr->pinName == nullptr)
      itr->pinName = getPinName(itr->pinType);
    if (!itr->isPinValid())
    {
      // this is just a demo example how auto-pin-assignment could look like
      // (no interaction with PinManger in that case)
      static uint8_t counter = 199;
      itr->pinNr = ++counter;
      // otherwise:
      // return rollbackPinRegistration(user, pinCount, pinConfig);
    }
    else if (!PinManager::allocatePin(itr->pinNr, isOutputPin(itr->pinType), PinOwner::PluginMgr))
      return rollbackPinRegistration(user, pinCount, pinConfig);
    _pinUserConfigs.emplace_back(&user, *itr);
  }

  _pinUsers.emplace_back(&user, pluginName);
  return true;
}

bool PluginManager::rollbackPinRegistration(PinUser &user, uint8_t pinCount, PinConfig *pinConfig)
{
  unregisterPinUser(user);
  auto begin = pinConfig;
  auto end = begin + pinCount;
  for (auto itr = begin; itr != end; ++itr)
    itr->invalidatePin();
  return false;
}

void PluginManager::unregisterPinUser(PinUser &user)
{
  for (const auto &entry : _pinUserConfigs)
  {
    const auto &pinConfig = entry.second;
    PinManager::deallocatePin(pinConfig.pinNr, PinOwner::PluginMgr);
  }

  auto pred1 = [&user](const PinUserConfigs::value_type &entry)
  { return entry.first == &user; };
  _pinUserConfigs.erase(
      std::remove_if(_pinUserConfigs.begin(), _pinUserConfigs.end(), pred1), _pinUserConfigs.end());

  auto pred2 = [&user](const PinUsers::value_type &entry)
  { return entry.first == &user; };
  _pinUsers.erase(
      std::remove_if(_pinUsers.begin(), _pinUsers.end(), pred2), _pinUsers.end());
}

// ----- TemperatureSensor handling -----

void PluginManager::registerTemperatureSensor(TemperatureSensor &sensor, const char *pluginName)
{
  unregisterTemperatureSensor(sensor);
  _temperatureSensors.emplace_back(&sensor, pluginName);
}

void PluginManager::unregisterTemperatureSensor(TemperatureSensor &sensor)
{
  // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom
  auto pred = [&sensor](const TemperatureSensors::value_type &entry)
  { return entry.first == &sensor; };
  _temperatureSensors.erase(
      std::remove_if(_temperatureSensors.begin(), _temperatureSensors.end(), pred), _temperatureSensors.end());
}

// ----- HumiditySensor handling -----

void PluginManager::registerHumiditySensor(HumiditySensor &sensor, const char *pluginName)
{
  unregisterHumiditySensor(sensor);
  _humiditySensors.emplace_back(&sensor, pluginName);
}

void PluginManager::unregisterHumiditySensor(HumiditySensor &sensor)
{
  auto pred = [&sensor](const HumiditySensors::value_type &entry)
  { return entry.first == &sensor; };
  _humiditySensors.erase(
      std::remove_if(_humiditySensors.begin(), _humiditySensors.end(), pred), _humiditySensors.end());
}

// ----- name handling -----

const char *PluginManager::getSensorName(const TemperatureSensor *sensor) const
{
  auto pred = [sensor](const TemperatureSensors::value_type &entry)
  { return entry.first == sensor; };
  const auto itr = std::find_if(_temperatureSensors.begin(), _temperatureSensors.end(), pred);
  return itr == _temperatureSensors.end() ? unknownName : itr->second;
}

const char *PluginManager::getSensorName(const HumiditySensor *sensor) const
{
  auto pred = [sensor](const HumiditySensors::value_type &entry)
  { return entry.first == sensor; };
  const auto itr = std::find_if(_humiditySensors.begin(), _humiditySensors.end(), pred);
  return itr == _humiditySensors.end() ? unknownName : itr->second;
}

const char *PluginManager::getPluginName(const PinUser *user) const
{
  auto pred = [user](const PinUsers::value_type &entry)
  { return entry.first == user; };
  const auto itr = std::find_if(_pinUsers.begin(), _pinUsers.end(), pred);
  return itr == _pinUsers.end() ? unknownName : itr->second;
}

// ----- UI interaction -----

#ifndef PLUGINMGR_DISABLE_UI

void PluginManager::addToJsonInfo(JsonObject &root, bool advanced)
{
  JsonObject user = root["u"];
  if (user.isNull())
    user = root.createNestedObject("u");

  addUiInfo_plugins(user);
#ifdef PLUGINMGR_SORT_UI_INFO_BY_NAME
  addUiInfo(user, advanced);
#else
  addUiInfo_basic(user);
  if (advanced)
    addUiInfo_advanced(user);
#endif // PLUGINMGR_SORT_UI_INFO_BY_NAME
}

#ifdef PLUGINMGR_SORT_UI_INFO_BY_NAME

void PluginManager::addUiInfo(JsonObject &user, bool advanced)
{
  InfoDataBuilder info;

  for (const auto &entry : _temperatureSensors)
  {
    auto &sensor = *entry.first;
    String val;
    val += "Temperature = ";
    if (sensor.isReady())
    {
      val += sensor.temperature();
      val += sensor.useFahrenheit() ? " °F" : " °C";
    }
    else
    {
      val += notAvailable;
    }
    info.add(entry.second, val);
  }

  for (const auto &entry : _humiditySensors)
  {
    auto &sensor = *entry.first;
    String val;
    val += "Humidity = ";
    if (sensor.isReady())
    {
      val += sensor.humidity();
      val += " %rel";
    }
    else
    {
      val += notAvailable;
    }
    info.add(entry.second, val);
  }

  if (advanced)
  {
    for (const auto &entry : _pinUserConfigs)
    {
      const PinConfig &config = entry.second;
      String val;
      val += config.pinName;
      val += " = Pin ";
      val += config.pinNr;
      info.add(getPluginName(entry.first), val);
    }
  }

  for (const auto &line : info)
    user.createNestedArray(line.first).add(line.second);
}

#else // PLUGINMGR_SORT_UI_INFO_BY_NAME

void PluginManager::addUiInfo_basic(JsonObject &user)
{
  int counter = 0;
  for (const auto &entry : _temperatureSensors)
  {
#if (0)
    TemperatureSensor &sensor = *entry.first;
    String key;
    key += "Temp. ";
    key += ++counter;
    key += " = ";
    if (sensor.isReady())
    {
      val += sensor.temperature();
      val += sensor.useFahrenheit() ? " °F" : " °C";
    }
    else
    {
      val += notAvailable;
    }
    user.createNestedArray(key).add(entry.second);
#else
    TemperatureSensor &sensor = *entry.first;
    String key;
    key += "Temp. ";
    key += ++counter;
    key += ": ";
    key += entry.second;
    String val;
    if (sensor.isReady())
    {
      val += sensor.temperature();
      val += sensor.useFahrenheit() ? " °F" : " °C";
    }
    else
    {
      val += notAvailable;
    }
    user.createNestedArray(key).add(val);
#endif
  }

  counter = 0;
  for (const auto &entry : _humiditySensors)
  {
#if (0)
    HumiditySensor &sensor = *entry.first;
    String key;
    key += "Hum. ";
    key += ++counter;
    key += " = ";
    if (sensor.isReady())
    {
      val += sensor.humidity();
      val += " %rel";
    }
    else
    {
      val += notAvailable;
    }
    user.createNestedArray(key).add(entry.second);
#else
    HumiditySensor &sensor = *entry.first;
    String key;
    key += "Hum. ";
    key += ++counter;
    key += ": ";
    key += entry.second;
    String val;
    if (sensor.isReady())
    {
      val += sensor.humidity();
      val += " %rel";
    }
    else
    {
      val += notAvailable;
    }
    user.createNestedArray(key).add(val);
#endif
  }
}

void PluginManager::addUiInfo_advanced(JsonObject &user)
{
  for (const auto &entry : _pinUserConfigs)
  {
#if (0)
    const PinConfig &config = entry.second;
    String key;
    key += "GPIO ";
    key += config.pinNr;
    key += " = ";
    key += config.pinName;
    user.createNestedArray(key).add(getPluginName(entry.first));
#else
    const PinConfig &config = entry.second;
    String key;
    key += "GPIO ";
    key += config.pinNr;
    key += ": ";
    key += getPluginName(entry.first);
    user.createNestedArray(key).add(config.pinName);
#endif
  }
}

#endif // PLUGINMGR_SORT_UI_INFO_BY_NAME

void PluginManager::addUiInfo_plugins(JsonObject &user)
{
  // JsonArray line = user.createNestedArray("<hr>Hello");
  // line.add("<hr> from ");
  // line.add("<hr>Plugins!");
}

#endif // PLUGINMGR_DISABLE_UI

//--------------------------------------------------------------------------------------------------

bool TemperatureSensor::_useFahrenheit = false;

PluginManager pluginManager;

//--------------------------------------------------------------------------------------------------
