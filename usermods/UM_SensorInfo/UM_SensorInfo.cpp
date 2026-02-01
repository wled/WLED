/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"

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
    const uint8_t size = 15;
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
      drawLine(_startPos, size, sensor.isReady() ? 0x002200 : 0x220000);
    }

    void showChannelInfo(Sensor &sensor)
    {
      int pos = _startPos;
      const uint8_t channelCount = sensor.channelCount();
      for (uint8_t ch = 0; ch < MIN(channelCount, size / 3); ++ch)
      {
        SensorChannelProxy channel = sensor.channel(ch);
        const auto color = getChannelColor(channel);
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

  auto sensorList = UsermodManager::getSensors();
  for (auto cursor = sensorList.getAllSensors();
       cursor.isValid();
       cursor.next())
  {
    visualizer.showInfo(cursor.get());
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_SENSOR_INFO[] PROGMEM = "! SensorInfo";

//--------------------------------------------------------------------------------------------------

#if (0)

uint16_t mode_NumberDumper()
{
  SEGMENT.clear();

#if (0)
  auto sensorList = UsermodManager::getSensors();
  int counter = 0;
  for (auto cursor = sensorList.getSensorChannelsByType(SensorValueType::Float);
       cursor.isValid();
       cursor.next())
  {
    auto sensor = cursor.get();
    const bool isReady = sensor.isReady();
    SEGMENT.setPixelColor(counter * 10 + 5, isReady ? 0x008800 : 0x880000);

    if (isReady)
      continue;
    const float sensorValue = sensor.getValue();
    const float sensorValueMax = sensor.getProps().rangeMax;
    if (sensorValueMax > 0.0f)
    {
      const int pos = sensorValue * (SEGLEN - 1) / sensorValueMax;
      SEGMENT.setPixelColor(pos, 0x000088);
      // SEGMENT.setPixelColor(pos, SEGCOLOR(0));
      // SEGMENT.setPixelColor(pos, fast_color_scale(SEGCOLOR(0), 64));
    }

    ++counter;
  }
#endif

#if (1)
  auto sensorList = UsermodManager::getSensors();
  int counter = 0;
  for (auto cursor = sensorList.getAllSensors();
       cursor.isValid();
       cursor.next())
  {
    const int basePos = 5 + 25 * counter++;

    auto &sensor = cursor.get();
    const bool isReady = sensor.isReady();
    SEGMENT.setPixelColor(basePos, isReady ? 0x004400 : 0x440000);

    int baseOffset = 2;
    const uint8_t channelCount = sensor.channelCount();
    for (uint8_t ch = 0; ch < channelCount; ++ch)
    {
      SEGMENT.setPixelColor(basePos + baseOffset + ch * baseOffset, 0x440044);
    }
  }
#endif

  return FRAMETIME;
}
static const char _data_FX_MODE_EX_NUMBER_DUMPER[] PROGMEM = "! SensorNumberDumper";
// static const char _data_FX_MODE_EX_NUMBER_DUMPER[] PROGMEM = "Ex: NumberDumper@,,,,,,Humidity Info,Temperature Info;!;;o2=1,o3=1";

#endif

//--------------------------------------------------------------------------------------------------

class UM_SensorInfo : public Usermod
{
  void setup() override
  {
    strip.addEffect(255, &mode_SensorInfo, _data_FX_MODE_SENSOR_INFO);
    // strip.addEffect(255, &mode_NumberDumper, _data_FX_MODE_EX_NUMBER_DUMPER);
  }

  void loop() override {}

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    int sensorIndex = 0;
    auto sensorList = UsermodManager::getSensors();
    for (auto cursor = sensorList.getAllSensors(); cursor.isValid(); cursor.next())
    {
      Sensor &sensor = cursor.get();
      const bool isSensorReady = sensor.isReady();
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
        SensorChannelProxy channel = sensor.channel(channelIndex);
        const SensorChannelProps &channelProps = channel.getProps(channelIndex);
        String key;
        key += sensorIndex;
        key += ".";
        key += channelIndex;
        key += " ";
        key += channelProps.channelName;

        const bool isChannelReady = sensor.isReady(channelIndex);
        String val;
        if (channelProps.channelNameShort[0] != '\0')
        {
          val += channelProps.channelNameShort;
          val += " = ";
        }
        if (isChannelReady)
        {
          const SensorValue sensorValue = sensor.getValue(channelIndex);
          val += sensorValue.as_float();
        }
        else
        {
          val += "[n/a]";
        }
        val += " ";
        val += channelProps.unitString;

        user.createNestedArray(key).add(val);
      }

      ++sensorIndex;
    }
  }
};

//--------------------------------------------------------------------------------------------------

static UM_SensorInfo um_SensorInfo;
REGISTER_USERMOD(um_SensorInfo);
