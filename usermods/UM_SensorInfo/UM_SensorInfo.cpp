/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"

extern uint16_t mode_static(void);

//--------------------------------------------------------------------------------------------------

namespace
{

  void drawLine(int pos, int length, uint32_t color)
  {
    const int end = pos + length;
    while (pos < end)
      SEGMENT.setPixelColor(pos++, color);
  }

  class SensorChannelVisualizer
  {
  public:
    const uint8_t offset = 3;
    const uint8_t size = 16;
    const uint8_t space = 3;

    void showInfo(Sensor &sensor)
    {
      _startPos = offset + _sensorCounter * (size + space);

      showReadyState(sensor);
      showChannelInfo(sensor);

      ++_sensorCounter;
    }

  private:
    void showReadyState(Sensor &sensor)
    {
      drawLine(_startPos, size, sensor.isSensorReady() ? 0x002200 : 0x220000);
    }

    void showChannelInfo(Sensor &sensor)
    {
      int pos = _startPos;
      const uint8_t channelCount = sensor.channelCount();
      for (uint8_t ch = 0; ch < MIN(channelCount, size / 2); ++ch)
      {
        SensorChannelProxy channel = sensor.getChannel(ch);
        const uint32_t color = getChannelColor(channel);
        SEGMENT.setPixelColor(pos, color);
        pos += 2;
      }
    }

    static uint32_t getChannelColor(SensorChannelProxy &channel)
    {
      return channel.isReady() ? 0x008844 : 0x880044;
    }

  private:
    uint8_t _sensorCounter = 0;
    int _startPos;
  };

}

//--------------------------------------------------------------------------------------------------

uint16_t mode_SensorInfo()
{
  SEGMENT.clear();

  SensorChannelVisualizer visualizer;

  SensorList sensorList = UsermodManager::getSensors();
  for (auto cursor = sensorList.getAllSensors();
       cursor.isValid();
       cursor.next())
  {
    visualizer.showInfo(cursor.get());
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_SENSOR_INFO[] PROGMEM = "1 Sensor Info";

//--------------------------------------------------------------------------------------------------

uint16_t mode_NumberDumper()
{
  SEGMENT.clear();

  SensorList sensorList = UsermodManager::getSensors();
  uint8_t hue = 0;
  for (auto cursor = sensorList.getSensorChannelsByType(SensorValueType::Float); cursor.isValid(); cursor.next())
  {
    auto channel = cursor.get();
    const bool isReady = channel.isReady();
    if (!isReady)
      continue;

    const auto &props = channel.getProps();
    const float sensorValue = channel.getValue();
    const float sensorValueMax = channel.getProps().rangeMax;
    // TODO(feature) Also take care for negative ranges, and map accordingly.
    if (sensorValueMax > 0.0f)
    {
      const int pos = sensorValue * (SEGLEN - 1) / sensorValueMax;
      uint32_t color;
      hsv2rgb(CHSV32(hue, 255, 255), color);
      SEGMENT.setPixelColor(pos, color);
      hue += 74;
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_NUMBER_DUMPER[] PROGMEM = "2 Numbers";

//--------------------------------------------------------------------------------------------------

uint16_t mode_SEF_all()
{
  SensorList sensorList = UsermodManager::getSensors();
  Sensor *sensor = sensorList.findSensorByName("SEF");
  if (!sensor || !sensor->isSensorReady())
    return mode_static();

  SEGMENT.clear();
  SEGMENT.fill(0x080800);

  uint8_t hue = 0;
  for (uint8_t channelIndex = 0; channelIndex < sensor->channelCount(); ++channelIndex)
  {
    if (!sensor->isChannelReady(channelIndex))
      continue;

    const auto &props = sensor->getChannelProps(channelIndex);
    const float sensorValue = sensor->getChannelValue(channelIndex);
    const float sensorValueMax = sensor->getChannelProps(channelIndex).rangeMax;
    // TODO(feature) Also take care for negative ranges, and map accordingly.
    if (sensorValueMax > 0.0f)
    {
      const int pos = sensorValue * (SEGLEN - 1) / sensorValueMax;
      uint32_t color;
      hsv2rgb(CHSV32(hue, 255, 255), color);
      SEGMENT.setPixelColor(pos, color);
      hue += 74;
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_SEF_ALL[] PROGMEM = "3 SEF all";

//--------------------------------------------------------------------------------------------------

class FluctuationChannels final : public SensorChannelCursor
{
public:
  explicit FluctuationChannels(SensorList &sensorList) : SensorChannelCursor{sensorList.getAllSensors()} {}

private:
  bool matches(const SensorChannelProps &props) override { return strcmp(props.quantity.name, "offset") == 0; }
};

uint16_t mode_Fluctuations()
{
  SEGMENT.clear();
  SEGMENT.fill(0x080800);

  SensorList sensorList = UsermodManager::getSensors();
  uint8_t hue = 0;
  for (FluctuationChannels cursor{sensorList}; cursor.isValid(); cursor.next())
  {
    auto channel = cursor.get();
    if (!channel.isReady())
      continue;

    const auto &props = channel.getProps();
    const float sensorValue = channel.getValue();
    const float sensorValueMax = channel.getProps().rangeMax;
    // TODO(feature) Also take care for negative ranges, and map accordingly.
    if (sensorValueMax > 0.0f)
    {
      const int pos = sensorValue * (SEGLEN - 1) / sensorValueMax;
      uint32_t color;
      hsv2rgb(CHSV32(hue, 255, 255), color);
      SEGMENT.setPixelColor(pos, color);
      hue += 74;
    }
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_FLUCTUATIONS[] PROGMEM = "4 Fluct only";

//--------------------------------------------------------------------------------------------------

class UM_SensorInfo : public Usermod
{
  void setup() override
  {
    strip.addEffect(255, &mode_SensorInfo, _data_FX_MODE_SENSOR_INFO);
    strip.addEffect(255, &mode_NumberDumper, _data_FX_MODE_NUMBER_DUMPER);
    strip.addEffect(255, &mode_SEF_all, _data_FX_MODE_SEF_ALL);
    strip.addEffect(255, &mode_Fluctuations, _data_FX_MODE_FLUCTUATIONS);
  }

  void loop() override {}

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    int sensorIndex = 0;
    SensorList sensorList = UsermodManager::getSensors();
    for (auto cursor = sensorList.getAllSensors(); cursor.isValid(); cursor.next())
    {
      Sensor &sensor = cursor.get();
      const bool isSensorReady = sensor.isSensorReady();
      const int channelCount = sensor.channelCount();

      String sensorName;
      sensorName += sensorIndex;
      sensorName += "._ ";
      sensorName += sensor.name();
      String sensorChannels;
      if (!isSensorReady)
        sensorChannels += "[OFFLINE] - ";
      sensorChannels += channelCount;
      sensorChannels += " channel";
      if (channelCount > 1)
        sensorChannels += 's';
      user.createNestedArray(sensorName).add(sensorChannels);

      for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
      {
        SensorChannelProxy channel = sensor.getChannel(channelIndex);
        const SensorChannelProps &channelProps = channel.getProps();
        String key;
        key += sensorIndex;
        key += ".";
        key += channelIndex;
        key += " ";
        key += channel.name();

        const bool isChannelReady = channel.isReady();
        String val;
        val += channelProps.quantity.name;
        val += "<br>";
        if (isChannelReady)
        {
          const SensorValue sensorValue = channel.getValue();
          // TODO(feature) Also take care for other datatypes (via visitor).
          val += sensorValue.as_float();
        }
        else
        {
          val += "[n/a]";
        }
        val += " ";
        val += channelProps.quantity.unit;

        user.createNestedArray(key).add(val);
      }

      ++sensorIndex;
    }
  }
};

//--------------------------------------------------------------------------------------------------

static UM_SensorInfo um_SensorInfo;
REGISTER_USERMOD(um_SensorInfo);
