/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"
#include "Sensor.h"

//--------------------------------------------------------------------------------------------------

void SensorCursor::reset()
{
  _sensorIndex = 0;
  for (_umIter = _umBegin; _umIter < _umEnd; ++_umIter)
  {
    Usermod *um = *_umIter;
    if (um->getSensorCount())
    {
      _sensor = um->getSensor(0);
      if (_sensor && _sensor->channelCount())
        return;
    }
  }
  _sensor = nullptr;
}

bool SensorCursor::next()
{
  if (isValid())
  {
    // try next sensor of current usermod
    Usermod *um = *_umIter;
    if (++_sensorIndex < um->getSensorCount())
    {
      _sensor = um->getSensor(_sensorIndex);
      if (_sensor && _sensor->channelCount())
        return true;
    }
    // try next usermod
    _sensorIndex = 0;
    while (++_umIter < _umEnd)
    {
      um = *_umIter;
      if (um->getSensorCount())
      {
        _sensor = um->getSensor(0);
        if (_sensor && _sensor->channelCount())
          return true;
      }
    }
    // no more sensors found
    _sensor = nullptr;
  }
  return false;
}

Sensor *findSensorByName(SensorCursor allSensors, const char *sensorName)
{
  while (allSensors.isValid())
  {
    if (strcmp(allSensors->name(), sensorName) == 0)
      return &allSensors.get();
    allSensors.next();
  }
  return nullptr;
}

//--------------------------------------------------------------------------------------------------

bool SensorChannelCursor::isValid()
{
  if (!_sensorCursor.isValid())
    return false;
  if (matches(_sensorCursor->getChannelProps(_channelIndex)))
    return true;
  return next();
}

void SensorChannelCursor::reset()
{
  _sensorCursor.reset();
  _channelIndex = 0;
}

bool SensorChannelCursor::next()
{
  while (_sensorCursor.isValid())
  {
    if (++_channelIndex < _sensorCursor->channelCount())
      if (matches(_sensorCursor->getChannelProps(_channelIndex)))
        return true;

    _channelIndex = 0;
    if (_sensorCursor.next())
      if (matches(_sensorCursor->getChannelProps(_channelIndex)))
        return true;
  }
  return false;
}

//--------------------------------------------------------------------------------------------------

void SensorValue::accept(SensorValueVisitor &visitor) const
{
  switch (_type)
  {
  case SensorValueType::Bool:
    visitor.visit(_bool);
    break;
  case SensorValueType::Float:
    visitor.visit(_float);
    break;
  case SensorValueType::Int32:
    visitor.visit(_int32);
    break;
  case SensorValueType::UInt32:
    visitor.visit(_uint32);
    break;
  case SensorValueType::Array:
    visitor.visit(*_array);
    break;
  case SensorValueType::Struct:
    visitor.visit(*_struct);
    break;
  case SensorValueType::Whatever:
    visitor.visit(_whatever);
    break;
  }
}

void Sensor::accept(uint8_t channelIndex, SensorChannelVisitor &visitor)
{
  if (!isSensorReady())
    return;

  const auto &val = getChannelValue(channelIndex);
  const auto &props = getChannelProps(channelIndex);
  switch (val.type())
  {
  case SensorValueType::Bool:
    visitor.visit(val._bool, props);
    break;
  case SensorValueType::Float:
    visitor.visit(val._float, props);
    break;
  case SensorValueType::Int32:
    visitor.visit(val._int32, props);
    break;
  case SensorValueType::UInt32:
    visitor.visit(val._uint32, props);
    break;
  case SensorValueType::Array:
    visitor.visit(*val._array, props);
    break;
  case SensorValueType::Struct:
    visitor.visit(*val._struct, props);
    break;
  case SensorValueType::Whatever:
    visitor.visit(val._whatever, props);
    break;
  }
}

//--------------------------------------------------------------------------------------------------
