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

/// Specific datatype that is contained in a SensorValue.
enum class SensorValueType : uint8_t
{
  invalid = 0, //< SensorValue is empty.
  Bool,
  Float,
  Int32,
  UInt32,
  Array,
  Struct,
  Whatever
};

/// Generic datatype that is delivered by a SensorChannel.
class SensorValue
{
public:
  /// Default constructor creates an invalid SensorValue.
  SensorValue() : _type{SensorValueType::invalid} {}

  SensorValue(bool val) : _bool{val}, _type{SensorValueType::Bool} {}
  SensorValue(float val) : _float{val}, _type{SensorValueType::Float} {}
  SensorValue(int32_t val) : _int32{val}, _type{SensorValueType::Int32} {}
  SensorValue(uint32_t val) : _uint32{val}, _type{SensorValueType::UInt32} {}
  explicit SensorValue(const SensorValueArray *val) : _array{val}, _type{SensorValueType::Array} {}
  explicit SensorValue(const SensorValueStruct *val) : _struct{val}, _type{SensorValueType::Struct} {}
  explicit SensorValue(const void *val) : _whatever{val}, _type{SensorValueType::Whatever} {}

  /** Check if the SensorValue is valid.
   * Converting an invalid SensorValue to a specific type results in undefined behaviour.
   */
  bool isValid() const { return _type != SensorValueType::invalid; }

  SensorValueType type() const { return _type; }

  void accept(SensorValueVisitor &visitor) const;

  bool as_bool() const { return _type == SensorValueType::Bool ? _bool : false; }
  float as_float() const { return _type == SensorValueType::Float ? _float : 0.0f; }
  int32_t as_int32() const { return _type == SensorValueType::Int32 ? _int32 : 0; }
  uint32_t as_uint32() const { return _type == SensorValueType::UInt32 ? _uint32 : 0U; }
  const SensorValueArray *as_array() const { return _type == SensorValueType::Array ? _array : nullptr; }
  const SensorValueStruct *as_struct() const { return _type == SensorValueType::Struct ? _struct : nullptr; }
  const void *as_whatever() const { return _type == SensorValueType::Whatever ? _whatever : nullptr; }

  explicit operator bool() const { return as_bool(); }
  explicit operator float() const { return as_float(); }
  explicit operator int32_t() const { return as_int32(); }
  explicit operator uint32_t() const { return as_uint32(); }
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

/// The physical (or theoretical/virtual) quantity of the readings from a sensor channel.
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

/// Properties of a SensorChannel.
struct SensorChannelProps
{
  SensorChannelProps(const char *channelName_,
                     SensorQuantity quantity_,
                     SensorValue rangeMin_,
                     SensorValue rangeMax_)
      : channelName{channelName_}, quantity{quantity_}, rangeMin{rangeMin_}, rangeMax{rangeMax_} {}

  const char *const channelName; //< The channel's name.
  const SensorQuantity quantity; //< The quantity of the channel's readings.
  const SensorValue rangeMin;    //< The readings' (typical) minimum range of operation.
  const SensorValue rangeMax;    //< The readings' (typical) maximum range of operation.
};

/// Helper array for multiple SensorChannelProps.
template <size_t CHANNEL_COUNT>
using SensorChannelPropsArray = std::array<SensorChannelProps, CHANNEL_COUNT>;

//--------------------------------------------------------------------------------------------------
class SensorChannelProxy;

/// Interface to be implemented by all sensors.
class Sensor
{
public:
  /// Get the sensor's name.
  const char *name() { return _sensorName; }

  /// Get the number of provided sensor channels.
  uint8_t channelCount() { return _channelCount; }

  /// Check if the sensor is online and ready to be used.
  bool isSensorReady() { return do_isSensorReady(); }

  /** Get a proxy object that's representing one specific sensor channel.
   * channelIndex >= channelCount() results in undefined behaviour.
   */
  SensorChannelProxy getChannel(uint8_t channelIndex);

  /** Check if a specific sensor channel is ready to deliver data.
   * channelIndex >= channelCount() results in undefined behaviour.
   */
  bool isChannelReady(uint8_t channelIndex) { return do_isSensorReady() ? do_isSensorChannelReady(channelIndex) : false; }

  /** Read a value from a specific sensor channel.
   * channelIndex >= channelCount() results in undefined behaviour.
   * Reading a value while isChannelReady() returns false results in undefined behaviour.
   */
  SensorValue getChannelValue(uint8_t channelIndex) { return do_getSensorChannelValue(channelIndex); }

  /** Get the properties of a specific sensor channel.
   * channelIndex >= channelCount() results in undefined behaviour.
   */
  const SensorChannelProps &getChannelProps(uint8_t channelIndex) { return do_getSensorChannelProperties(channelIndex); }

  /** Accept the given \a visitor for a specific sensor channel.
   * channelIndex >= channelCount() results in undefined behaviour.
   */
  void accept(uint8_t channelIndex, SensorChannelVisitor &visitor);

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

/// A proxy object that is representing one specific sensor channel.
class SensorChannelProxy
{
public:
  SensorChannelProxy(Sensor &parentSensor, const uint8_t channelIndex)
      : _parent{parentSensor}, _index{channelIndex} {}

  /// Get the channel's name.
  const char *name() { return getProps().channelName; }

  /// Check if the channel is ready to deliver data.
  bool isReady() { return _parent.isChannelReady(_index); }

  /** Read a value from the channel.
   * Reading a value while isReady() returns false results in undefined behaviour.
   */
  SensorValue getValue() { return _parent.getChannelValue(_index); }

  /// Get the channel's properties.
  const SensorChannelProps &getProps() { return _parent.getChannelProps(_index); }

  /// Get the channel's corresponding origin sensor.
  Sensor &getRealSensor() { return _parent; }

  /// Get the channel's corresponding index at the origin sensor.
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

/// A cursor to iterate over all available sensors (provided by UsermodManager).
class SensorCursor
{
public:
  using UmIterator = Usermod *const *;
  SensorCursor(UmIterator umBegin, UmIterator umEnd) : _umBegin{umBegin}, _umEnd{umEnd} { reset(); }

  /// Check if the cursor has currently selected a valid sensor instance.
  bool isValid() const { return _sensor != nullptr; }

  /** Get the currently selected sensor instance.
   * Getting the sensor while isValid() returns false results in undefined behaviour.
   */
  Sensor &get() { return *_sensor; }
  Sensor &operator*() { return *_sensor; }
  Sensor *operator->() { return _sensor; }

  /** Select the next sensor.
   * @return Same as \c isValid()
   */
  bool next();

  /// Jump back to the first sensor (if any).
  void reset();

private:
  UmIterator _umBegin;
  UmIterator _umEnd;
  UmIterator _umIter = nullptr;
  Sensor *_sensor = nullptr;
  uint8_t _sensorIndex = 0;
};

Sensor *findSensorByName(SensorCursor allSensors, const char *sensorName);

//--------------------------------------------------------------------------------------------------

/// Base class for cursors that iterate over specific channels of all sensors.
class SensorChannelCursor
{
public:
  /// Check if the cursor has currently selected a valid sensor channel instance.
  bool isValid();

  /** Get the currently selected sensor channel instance.
   * Getting the channel while isValid() returns false results in undefined behaviour.
   */
  SensorChannelProxy get() { return {*_sensorCursor, _channelIndex}; }

  /** Select the next channel.
   * @return Same as \c isValid()
   */
  bool next();

  /// Jump back to the first channel (if any).
  void reset();

protected:
  ~SensorChannelCursor() = default;
  explicit SensorChannelCursor(SensorCursor allSensors) : _sensorCursor{allSensors} { reset(); }

  virtual bool matches(const SensorChannelProps &channelProps) = 0;

private:
  SensorCursor _sensorCursor;
  uint8_t _channelIndex = 0;
};

/// A cursor to iterate over all channels of all sensors.
class AllSensorChannels final : public SensorChannelCursor
{
public:
  explicit AllSensorChannels(SensorCursor allSensors)
      : SensorChannelCursor{allSensors} {}

private:
  bool matches(const SensorChannelProps &) override { return true; }
};

/// A cursor to iterate over all channels with a specific quantity.
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

// A cursor to iterate over all channels with a specific ValueType.
class SensorChannelsByType final : public SensorChannelCursor
{
public:
  SensorChannelsByType(SensorCursor allSensors, SensorValueType valueType)
      : SensorChannelCursor{allSensors}, _type{valueType} {}

private:
  bool matches(const SensorChannelProps &props) override { return props.rangeMin.type() == _type; }
  const SensorValueType _type;
};

// A cursor to iterate over all channels with a specific name.
class SensorChannelsByName final : public SensorChannelCursor
{
public:
  SensorChannelsByName(SensorCursor allSensors, const char *channelName)
      : SensorChannelCursor{allSensors}, _name{channelName} {}

private:
  bool matches(const SensorChannelProps &props) override { return strcmp(props.channelName, _name) == 0; }
  const char *const _name;
};

//--------------------------------------------------------------------------------------------------

/** Base class for simple sensor implementations.
 * Supports at most 32 sensor channels.
 * @see EasySensor
 * @see EasySensorArray
 */
class EasySensorBase : public Sensor
{
public:
  /// Put the entire sensor offline (by usermod).
  void suspendSensor() { _isSensorReady = false; }

  /** Put a specific sensor channel offline (by usermod).
   * channelIndex >= channelCount() results in undefined behaviour.
   */
  void suspendChannel(uint8_t channelIndex) { _channelReadyFlags &= ~(1U << channelIndex); }

  /** Store the readings for a specific sensor channel (by usermod).
   * channelIndex >= channelCount() results in undefined behaviour.
   */
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

/** A simple sensor implementation that provides multiple sensor channels.
 * Most of the required sensor housekeeping is provided by this helper.
 * The usermod is ultimately just responsible for:
 * - Initialize this helper (as member variable) with the sensor's properties.
 * - Make this helper available to the UsermodManager.
 * - Periodically read the physical sensor.
 * - Store the readings in this helper.
 */
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

/** A simple sensor implementation that provides one single sensor channel (at index 0).
 * Most of the required sensor housekeeping is provided by this helper.
 * The usermod is ultimately just responsible for:
 * - Initialize this helper (as member variable) with the sensor's properties.
 * - Make this helper available to the UsermodManager.
 * - Periodically read the physical sensor.
 * - Store the readings in this helper.
 */
class EasySensor : public EasySensorBase
{
public:
  void suspendChannel(uint8_t channelIndex) = delete;
  void set(uint8_t channelIndex, SensorValue val) = delete;

  EasySensor(const char *sensorName, const SensorChannelProps &channelProps)
      : EasySensorBase{sensorName, 1, &_val, &_props}, _props{channelProps} {}

  /// Store the the readings for the sensor.
  void set(SensorValue val) { EasySensorBase::set(0, val); }
  void operator=(SensorValue val) { set(val); }

private:
  const SensorChannelProps _props;
  SensorValue _val;
};

//--------------------------------------------------------------------------------------------------

/// Create properties of a channel that delivers generic \c bool readings.
inline SensorChannelProps makeChannelProps_Bool(const char *channelName,
                                                const char *quantityName = nullptr)
{
  return {channelName, {quantityName ? quantityName : channelName, ""}, false, true};
}

/// Create properties of a channel that delivers generic \c float readings.
inline SensorChannelProps makeChannelProps_Float(const char *channelName,
                                                 const SensorQuantity &channelQuantity,
                                                 float rangeMin,
                                                 float rangeMax)
{
  return {channelName, channelQuantity, rangeMin, rangeMax};
}

/// Create properties of a channel that delivers temperature readings.
inline SensorChannelProps makeChannelProps_Temperature(const char *channelName,
                                                       float rangeMin = 0.0f,
                                                       float rangeMax = 40.0f)
{
  const auto quantity = SensorQuantity::Temperature();
  return {channelName ? channelName : quantity.name, quantity, rangeMin, rangeMax};
}

/// Create properties of a channel that delivers temperature readings.
inline SensorChannelProps makeChannelProps_Temperature(float rangeMin = 0.0f,
                                                       float rangeMax = 40.0f)
{
  return makeChannelProps_Temperature(nullptr, rangeMin, rangeMax);
}

/// Create properties of a channel that delivers humidity readings.
inline SensorChannelProps makeChannelProps_Humidity(const char *channelName = nullptr)
{
  const auto quantity = SensorQuantity::Humidity();
  return {channelName ? channelName : quantity.name, quantity, 0.0f, 100.0f};
}

//--------------------------------------------------------------------------------------------------
