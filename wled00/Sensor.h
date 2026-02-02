/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

#include <array>

//--------------------------------------------------------------------------------------------------

class SensorValueArray;  // TODO(feature) Not implemented yet!
class SensorValueStruct; // TODO(feature) Not implemented yet!

class SensorValueVisitor;
class SensorValueArrayVisitor;  // TODO(feature) Not implemented yet!
class SensorValueStructVisitor; // TODO(feature) Not implemented yet!
class SensorChannelVisitor;

enum class SensorValueType : uint8_t
{
  Bool = 0,
  Float,
  Int32,
  UInt32,
  Array,
  Struct,
  Whatever
};

class SensorValue
{
public:
  SensorValue(bool val) : _bool{val}, _type{SensorValueType::Bool} {}
  SensorValue(float val) : _float{val}, _type{SensorValueType::Float} {}
  SensorValue(int32_t val) : _int32{val}, _type{SensorValueType::Int32} {}
  SensorValue(uint32_t val) : _uint32{val}, _type{SensorValueType::UInt32} {}
  explicit SensorValue(const SensorValueArray *val) : _array{val}, _type{SensorValueType::Array} {}
  explicit SensorValue(const SensorValueStruct *val) : _struct{val}, _type{SensorValueType::Struct} {}
  explicit SensorValue(const void *val = nullptr) : _whatever{val}, _type{SensorValueType::Whatever} {}

  bool as_bool() const { return _type == SensorValueType::Bool ? _bool : false; }
  float as_float() const { return _type == SensorValueType::Float ? _float : 0.0f; }
  int32_t as_int32() const { return _type == SensorValueType::Int32 ? _int32 : 0; }
  uint32_t as_uint32() const { return _type == SensorValueType::UInt32 ? _uint32 : 0U; }
  const SensorValueArray *as_array() const { return _type == SensorValueType::Array ? _array : nullptr; }
  const SensorValueStruct *as_struct() const { return _type == SensorValueType::Struct ? _struct : nullptr; }
  const void *as_whatever() const { return _type == SensorValueType::Whatever ? _whatever : nullptr; }

  SensorValueType type() const { return _type; }

  void accept(SensorValueVisitor &visitor) const;

  operator bool() const { return as_bool(); }
  operator float() const { return as_float(); }
  operator int32_t() const { return as_int32(); }
  operator uint32_t() const { return as_uint32(); }
  explicit operator const SensorValueArray *() const { return as_array(); }
  explicit operator const SensorValueStruct *() const { return as_struct(); }
  explicit operator const void *() const { return as_whatever(); }

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
    const void *_whatever;
  };

  SensorValueType _type;
};

// The physical (or theoretical/virtual) quantity of the readings from a sensor channel.
struct SensorQuantity
{
  const char *const name;
  const char *const unit;

  // common physical measurements (just for convenience and consistency)
  static SensorQuantity Temperature() { return {"Temperature", "°C"}; }
  static SensorQuantity Humidity() { return {"Humidity", "%rel"}; }
  static SensorQuantity AirPressure() { return {"Air Pressure", "hPa"}; }

  static SensorQuantity Voltage() { return {"Voltage", "V"}; }
  static SensorQuantity Current() { return {"Current", "A"}; }
  static SensorQuantity Power() { return {"Power", "W"}; }
  static SensorQuantity Energy() { return {"Energy", "kWh"}; }

  static SensorQuantity Angle() { return {"Angle", "°"}; }

  static SensorQuantity Percent() { return {"Percent", "%"}; }
};

struct SensorChannelProps
{
  SensorChannelProps(const char *channelName_,
                     SensorQuantity quantity_,
                     SensorValue rangeMin_,
                     SensorValue rangeMax_)
      : channelName{channelName_}, quantity{quantity_}, rangeMin{rangeMin_}, rangeMax{rangeMax_} {}

  const char *const channelName;
  const SensorQuantity quantity;
  const SensorValue rangeMin;
  const SensorValue rangeMax;
};

template <size_t CHANNEL_COUNT>
using SensorChannelPropsArray = std::array<SensorChannelProps, CHANNEL_COUNT>;

//--------------------------------------------------------------------------------------------------
class SensorChannelProxy;

class Sensor
{
public:
  const char *name() { return _sensorName; }

  uint8_t channelCount() { return _channelCount; }

  bool isSensorReady() { return do_isSensorReady(); }

  SensorChannelProxy getChannel(uint8_t channelIndex);

  bool isChannelReady(uint8_t channelIndex) { return do_isSensorReady() ? do_isSensorChannelReady(channelIndex) : false; }

  SensorValue getChannelValue(uint8_t channelIndex) { return do_getSensorChannelValue(channelIndex); }

  const SensorChannelProps &getChannelProps(uint8_t channelIndex) { return do_getSensorChannelProperties(channelIndex); }

  void accept(uint8_t channelIndex, SensorChannelVisitor &visitor);
  void accept(SensorChannelVisitor &visitor) { accept(0, visitor); }

protected:
  Sensor(const char *sensorName, uint8_t channelCount)
      : _sensorName{sensorName}, _channelCount{channelCount} {}

  virtual bool do_isSensorReady() = 0;
  virtual bool do_isSensorChannelReady(uint8_t channelIndex) { return true; };
  virtual SensorValue do_getSensorChannelValue(uint8_t channelIndex) = 0;
  virtual const SensorChannelProps &do_getSensorChannelProperties(uint8_t channelIndex) = 0;

private:
  const char *_sensorName;
  const uint8_t _channelCount;
};

class SensorChannelProxy
{
public:
  SensorChannelProxy(Sensor &realSensor, const uint8_t channelIndex)
      : _parent{realSensor}, _index{channelIndex} {}

  const char *name() { return getProps().channelName; }

  bool isReady() { return _parent.isChannelReady(_index); }

  SensorValue getValue() { return _parent.getChannelValue(_index); }

  const SensorChannelProps &getProps() { return _parent.getChannelProps(_index); }

  Sensor &getRealSensor() { return _parent; }
  uint8_t getRealChannelIndex() { return _index; }

private:
  Sensor &_parent;
  const uint8_t _index;
};

inline SensorChannelProxy Sensor::getChannel(uint8_t channelIndex) { return SensorChannelProxy{*this, channelIndex}; }

//--------------------------------------------------------------------------------------------------

class SensorValueVisitor
{
public:
  virtual void visit(bool val) {}
  virtual void visit(float val) {}
  virtual void visit(int32_t val) {}
  virtual void visit(uint32_t val) {}
  virtual void visit(const SensorValueArray &val) {}
  virtual void visit(const SensorValueStruct &val) {}
  virtual void visit(const void *val) {}
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
  virtual void visit(const void *val, const SensorChannelProps &props) {}
};

//--------------------------------------------------------------------------------------------------
class Usermod;

// A cursor to iterate over all available sensors.
class SensorCursor
{
public:
  using UmIterator = Usermod *const *;
  SensorCursor(UmIterator umBegin, UmIterator umEnd) : _umBegin{umBegin}, _umEnd{umEnd} { reset(); }

  bool isValid() const { return _sensor != nullptr; }
  Sensor &get() { return *_sensor; }
  Sensor &operator*() { return *_sensor; }
  Sensor *operator->() { return _sensor; }
  bool next();
  void reset();

private:
  UmIterator _umBegin;
  UmIterator _umEnd;
  UmIterator _umIter = nullptr;
  Sensor *_sensor = nullptr;
  uint8_t _sensorIndex = 0;
};

// Base class for cursors to iterate over specific channels of all sensors.
class SensorChannelCursor
{
public:
  bool isValid();
  SensorChannelProxy get() { return {*_sensorCursor, _channelIndex}; }
  bool next();
  void reset();

protected:
  ~SensorChannelCursor() = default;
  explicit SensorChannelCursor(SensorCursor allSensors)
      : _sensorCursor{allSensors} { reset(); }

  virtual bool matches(const SensorChannelProps &channelProps) = 0;

private:
  SensorCursor _sensorCursor;
  uint8_t _channelIndex = 0;
};

// A cursor to iterate over all available channels of all sensors.
class AllSensorChannels final : public SensorChannelCursor
{
public:
  explicit AllSensorChannels(SensorCursor allSensors)
      : SensorChannelCursor{allSensors} {}

private:
  bool matches(const SensorChannelProps &) override { return true; }
};

// A cursor to iterate over all available channels with a specific quantity.
class SensorChannelsByQuantity final : public SensorChannelCursor
{
public:
  SensorChannelsByQuantity(SensorCursor allSensors, const char *quantityName)
      : SensorChannelCursor{allSensors}, _quantityName{quantityName} {}

  SensorChannelsByQuantity(SensorCursor allSensors, SensorQuantity quantity)
      : SensorChannelsByQuantity{allSensors, quantity.name} {}

private:
  bool matches(const SensorChannelProps &props) override { return strcmp(props.quantity.name, _quantityName) == 0; }
  const char *const _quantityName;
};

// A cursor to iterate over all available channels with a specific ValueType.
class SensorChannelsByType final : public SensorChannelCursor
{
public:
  SensorChannelsByType(SensorCursor allSensors, SensorValueType valueType)
      : SensorChannelCursor{allSensors}, _type{valueType} {}

private:
  bool matches(const SensorChannelProps &props) override { return props.rangeMin.type() == _type; }
  const SensorValueType _type;
};

// A cursor to iterate over all available channels with a specific name.
class SensorChannelsByName final : public SensorChannelCursor
{
public:
  SensorChannelsByName(SensorCursor allSensors, const char *channelName)
      : SensorChannelCursor{allSensors}, _name{channelName} {}

private:
  bool matches(const SensorChannelProps &props) override { return strcmp(props.channelName, _name) == 0; }
  const char *const _name;
};

class SensorList
{
public:
  SensorList(SensorCursor::UmIterator umBegin, SensorCursor::UmIterator umEnd)
      : _umBegin{umBegin}, _umEnd{umEnd} {}

  SensorCursor getAllSensors() { return SensorCursor{_umBegin, _umEnd}; }

  Sensor *findSensorByName(const char *sensorName)
  {
    for (auto cursor = getAllSensors(); cursor.isValid(); cursor.next())
      if (strcmp(cursor->name(), sensorName) == 0)
        return &cursor.get();
    return nullptr;
  }

  AllSensorChannels getAllSensorChannels() { return AllSensorChannels{getAllSensors()}; }

  SensorChannelsByQuantity getSensorChannelsByQuantity(const char *quantityName) { return {getAllSensors(), quantityName}; }
  SensorChannelsByQuantity getSensorChannelsByQuantity(SensorQuantity quantity) { return {getAllSensors(), quantity}; }

  SensorChannelsByType getSensorChannelsByType(SensorValueType valueType) { return {getAllSensors(), valueType}; }

  SensorChannelsByName getSensorChannelsByName(const char *channelName) { return {getAllSensors(), channelName}; }

private:
  SensorCursor::UmIterator _umBegin;
  SensorCursor::UmIterator _umEnd;
};

//--------------------------------------------------------------------------------------------------

class EasySensorBase : public Sensor
{
public:
  void suspendSensor() { _isSensorReady = false; }
  void suspendChannel(uint8_t channelIndex) { _channelReadyFlags &= ~(1U << channelIndex); }

  void set(uint8_t channelIndex, SensorValue val)
  {
    if (channelIndex >= channelCount())
      channelIndex = 0;
    _channelValues[channelIndex] = val;
    _channelReadyFlags |= (1U << channelIndex);
    _isSensorReady = true;
  }

protected:
  ~EasySensorBase() = default;
  EasySensorBase(const char *sensorName, uint8_t channelCount,
                 SensorValue *channelValues, const SensorChannelProps *channelProps)
      : Sensor{sensorName, channelCount},
        _channelProps{channelProps}, _channelValues{channelValues} {}

private:
  bool do_isSensorReady() final { return _isSensorReady; }

  bool do_isSensorChannelReady(uint8_t channelIndex) final
  {
    if (channelIndex >= channelCount())
      channelIndex = 0;
    return _channelReadyFlags & (1U << channelIndex);
  };

  SensorValue do_getSensorChannelValue(uint8_t channelIndex) final
  {
    if (channelIndex >= channelCount())
      channelIndex = 0;
    return _channelValues[channelIndex];
  }

  const SensorChannelProps &do_getSensorChannelProperties(uint8_t channelIndex) final
  {
    if (channelIndex >= channelCount())
      channelIndex = 0;
    return _channelProps[channelIndex];
  }

private:
  const SensorChannelProps *_channelProps;
  SensorValue *_channelValues;
  uint32_t _channelReadyFlags = 0;
  bool _isSensorReady = false;
};

template <size_t CHANNEL_COUNT>
class EasySensorArray : public EasySensorBase
{
public:
  static_assert(CHANNEL_COUNT <= 32, "EasySensorArray supports max. 32 sensor channels");

  EasySensorArray(const char *sensorName, const SensorChannelPropsArray<CHANNEL_COUNT> &channelProps)
      : EasySensorBase{sensorName, CHANNEL_COUNT, _channelValues.data(), _channelProps.data()},
        _channelProps{channelProps} {}

private:
  const SensorChannelPropsArray<CHANNEL_COUNT> _channelProps;
  std::array<SensorValue, CHANNEL_COUNT> _channelValues;
};

class EasySensor : public EasySensorBase
{
public:
  void suspendChannel(uint8_t channelIndex) = delete;
  void set(uint8_t channelIndex, SensorValue val) = delete;

  EasySensor(const char *sensorName, const SensorChannelProps &channelProps)
      : EasySensorBase{sensorName, 1, &_val, &_props}, _props{channelProps} {}

  void set(SensorValue val) { EasySensorBase::set(0, val); }
  void operator=(SensorValue val) { set(val); }

private:
  const SensorChannelProps _props;
  SensorValue _val;
};

//--------------------------------------------------------------------------------------------------

inline SensorChannelProps makeChannelProps_Bool(const char *channelName,
                                                const char *quantityName = nullptr)
{
  return {channelName, {quantityName ? quantityName : channelName, ""}, false, true};
}

inline SensorChannelProps makeChannelProps_Float(const char *channelName,
                                                 const SensorQuantity &channelQuantity,
                                                 float rangeMin,
                                                 float rangeMax)
{
  return {channelName, channelQuantity, rangeMin, rangeMax};
}

inline SensorChannelProps makeChannelProps_Temperature(const char *channelName,
                                                       float rangeMin = 0.0f,
                                                       float rangeMax = 40.0f)
{
  const auto quantity = SensorQuantity::Temperature();
  return {channelName ? channelName : quantity.name, quantity, rangeMin, rangeMax};
}

inline SensorChannelProps makeChannelProps_Temperature(float rangeMin = 0.0f,
                                                       float rangeMax = 40.0f)
{
  return makeChannelProps_Temperature(nullptr, rangeMin, rangeMax);
}

inline SensorChannelProps makeChannelProps_Humidity(const char *channelName = nullptr)
{
  const auto quantity = SensorQuantity::Humidity();
  return {channelName ? channelName : quantity.name, quantity, 0.0f, 100.0f};
}

//--------------------------------------------------------------------------------------------------
