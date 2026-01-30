/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

#include <array>

//--------------------------------------------------------------------------------------------------

class SensorValueArray;
class SensorValueStruct;

class SensorChannelVisitor;
class SensorValueVisitor;
class SensorValueArrayVisitor;
class SensorValueStructVisitor;

enum class SensorValueType : uint8_t
{
  Bool = 0,
  Float,
  Int32,
  UInt32,
  Array,
  Struct
};

class SensorValue
{
public:
  SensorValue(bool val) : _bool{val}, _type{SensorValueType::Bool} {}
  SensorValue(float val) : _float{val}, _type{SensorValueType::Float} {}
  SensorValue(int32_t val) : _int32{val}, _type{SensorValueType::Int32} {}
  SensorValue(uint32_t val) : _uint32{val}, _type{SensorValueType::UInt32} {}
  SensorValue(const SensorValueArray *val) : _array{val}, _type{SensorValueType::Array} {}
  SensorValue(const SensorValueStruct *val) : _struct{val}, _type{SensorValueType::Struct} {}

  bool as_bool() const { return _type == SensorValueType::Bool ? _bool : false; }
  float as_float() const { return _type == SensorValueType::Float ? _float : 0.0f; }
  int32_t as_int32() const { return _type == SensorValueType::Int32 ? _int32 : 0; }
  uint32_t as_uint32() const { return _type == SensorValueType::UInt32 ? _uint32 : 0U; }
  const SensorValueArray *as_array() const { return _type == SensorValueType::Array ? _array : nullptr; }
  const SensorValueStruct *as_struct() const { return _type == SensorValueType::Struct ? _struct : nullptr; }

  SensorValueType type() const { return _type; }

  void accept(SensorValueVisitor &visitor) const;

  operator bool() const { return as_bool(); }
  operator float() const { return as_float(); }
  operator int32_t() const { return as_int32(); }
  operator uint32_t() const { return as_uint32(); }
  operator const SensorValueArray *() const { return as_array(); }
  operator const SensorValueStruct *() const { return as_struct(); }

private:
  friend class Sensor;
  union
  {
    bool _bool;
    float _float;
    int32_t _int32;
    uint32_t _uint32;
    const SensorValueArray *_array;
    const SensorValueStruct *_struct;
  };

  SensorValueType _type;
};

struct SensorChannelProps
{
  SensorChannelProps(const char *channelName_,
                     const char *unitString_,
                     SensorValue rangeMin_,
                     SensorValue rangeMax_,
                     const char *channelNameShort_ = "")
      : channelName{channelName_}, unitString{unitString_}, rangeMin{rangeMin_}, rangeMax{rangeMax_}, channelNameShort{channelNameShort_} {}

  const char *channelName;
  const char *unitString;
  SensorValue rangeMin;
  SensorValue rangeMax;
  const char *channelNameShort;
};

template <size_t CHANNEL_COUNT>
using SensorChannelPropertiesArray = std::array<SensorChannelProps, CHANNEL_COUNT>;

//--------------------------------------------------------------------------------------------------

class Sensor
{
public:
  const char *name() const { return _sensorName; }

  uint8_t channelCount() const { return _channelCount; }

  bool isReady() { return do_isSensorReady(); }

  SensorValue getValue(uint8_t channelIndex = 0) { return do_getSensorValue(channelIndex); }

  const SensorChannelProps &getProperties(uint8_t channelIndex = 0) { return do_getSensorProperties(channelIndex); }

  void accept(uint8_t channelIndex, SensorChannelVisitor &visitor);
  void accept(SensorChannelVisitor &visitor) { accept(0, visitor); }

protected:
  Sensor(const char *sensorName, uint8_t channelCount)
      : _sensorName{sensorName}, _channelCount{channelCount} {}

  virtual bool do_isSensorReady() = 0;
  virtual SensorValue do_getSensorValue(uint8_t channelIndex) = 0;
  virtual const SensorChannelProps &do_getSensorProperties(uint8_t channelIndex) = 0;

private:
  const char *_sensorName;
  const uint8_t _channelCount;
};

class SensorChannelProxy final : public Sensor
{
public:
  SensorChannelProxy(Sensor &realSensor, const uint8_t channelIndex)
      : Sensor{realSensor.name(), 1}, _realSensor{realSensor}, _channelIndex{channelIndex} {}

  Sensor &getRealSensor() { return _realSensor; }
  uint8_t getRealChannelIndex() { return _channelIndex; }

private:
  bool do_isSensorReady() override { return _realSensor.isReady(); }
  SensorValue do_getSensorValue(uint8_t) override { return _realSensor.getValue(_channelIndex); }
  const SensorChannelProps &do_getSensorProperties(uint8_t) override { return _realSensor.getProperties(_channelIndex); }

  Sensor &_realSensor;
  const uint8_t _channelIndex;
};

//--------------------------------------------------------------------------------------------------

class SensorValueVisitor
{
public:
  virtual void visit(bool val) {}
  virtual void visit(float val) {}
  virtual void visit(int32_t val) {}
  virtual void visit(uint32_t val) {}
  virtual void visit(const SensorValueArray *val) {}
  virtual void visit(const SensorValueStruct *val) {}
};

class SensorChannelVisitor
{
public:
  virtual void visit(bool val, const SensorChannelProps &props) {}
  virtual void visit(float val, const SensorChannelProps &props) {}
  virtual void visit(int32_t val, const SensorChannelProps &props) {}
  virtual void visit(uint32_t val, const SensorChannelProps &props) {}
  virtual void visit(const SensorValueArray &val, const SensorChannelProps &props) {}
  virtual void visit(const SensorValueStruct &val, const SensorChannelProps &props) {}
};

//--------------------------------------------------------------------------------------------------

class EasySensor : public Sensor
{
public:
  EasySensor(const char *sensorName, const SensorChannelProps &channelProps)
      : Sensor{sensorName, 1}, _props{channelProps}, _val{channelProps.rangeMin} {}

  void set(SensorValue val)
  {
    if (val.type() == _val.type())
    {
      _val = val;
      _isReady = true;
    }
  }

  void suspend() { _isReady = false; }

  SensorValue get() const { return _val; }

  void operator=(SensorValue val) { set(val); }

private:
  bool do_isSensorReady() override { return _isReady; }
  SensorValue do_getSensorValue(uint8_t) override { return _val; }
  const SensorChannelProps &do_getSensorProperties(uint8_t) override { return _props; }

private:
  const SensorChannelProps _props;
  SensorValue _val;
  bool _isReady = false;
};

//--------------------------------------------------------------------------------------------------

inline SensorChannelProps makeChannelProps_Bool(const char *channelName,
                                                const char *channelNameShort = "",
                                                const char *unitString = "")
{
  return {channelName, unitString, false, true, channelNameShort};
}

inline SensorChannelProps makeChannelProps_Temperature(float rangeMin = 0.0f,
                                                       float rangeMax = 40.0f)
{
  return {"Temperature", "°C", rangeMin, rangeMax, "Temp"};
}

inline SensorChannelProps makeChannelProps_Humidity()
{
  return {"Humidity", "%rel", 0.0f, 100.0f, "Hum"};
}

//--------------------------------------------------------------------------------------------------
