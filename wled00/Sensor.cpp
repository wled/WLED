/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "Sensor.h"

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
    visitor.visit(_array);
    break;
  case SensorValueType::Struct:
    visitor.visit(_struct);
    break;
  }
}

void Sensor::accept(uint8_t channelIndex, SensorChannelVisitor &visitor)
{
  if (!isReady())
    return;

  const auto &val = getValue(channelIndex);
  const auto &props = getProperties(channelIndex);
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
  }
}

//--------------------------------------------------------------------------------------------------
