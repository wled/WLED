#include "wled.h"
#include <Adafruit_ADS1X15.h>
#include <math.h>

#include "ChannelSettings.h"

using namespace ADS1115;

class ADS1115Usermod : public Usermod {
  public:
    void setup() {
      ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V

      if (!ads.begin()) {
        Serial.println("Failed to initialize ADS");
        return;
      }

      if (!initChannel()) {
        isInitialized = true;
        return;
      }

      startReading();

      isEnabled = true;
      isInitialized = true;
    }

    void loop() {
      if (isEnabled && millis() - lastTime > loopInterval) {
        lastTime = millis();

        // If we don't have new data, skip this iteration.
        if (!ads.conversionComplete()) {
          return;
        }

        updateResult();
        moveToNextChannel();
        startReading();
      }
    }

    void addToJsonInfo(JsonObject& root)
    {
      if (!isEnabled) {
        return;
      }

      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));

      for (uint8_t i = 0; i < channelsCount; i++) {
        ChannelSettings* settingsPtr = &(channelSettings[i]);

        if (!settingsPtr->isEnabled) {
          continue;
        }

        JsonArray lightArr = user.createNestedArray(settingsPtr->name); //name
        float value = round((readings[i] + settingsPtr->offset) * settingsPtr->multiplier, settingsPtr->decimals);
        lightArr.add(value); //value
        lightArr.add(" " + settingsPtr->units); //unit
      }
    }

    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(F("ADC ADS1115"));
      
      for (uint8_t i = 0; i < channelsCount; i++) {
        ChannelSettings* settingsPtr = &(channelSettings[i]);
        JsonObject channel = top.createNestedObject(settingsPtr->settingName);
        channel[F("Enabled")] = settingsPtr->isEnabled;
        channel[F("Name")] = settingsPtr->name;
        channel[F("Units")] = settingsPtr->units;
        channel[F("Multiplier")] = settingsPtr->multiplier;
        channel[F("Offset")] = settingsPtr->offset;
        channel[F("Decimals")] = settingsPtr->decimals;
      }

      top[F("Loop Interval")] = loopInterval;
    }

    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root[F("ADC ADS1115")];

      bool configComplete = !top.isNull();
      bool hasEnabledChannels = false;

      for (uint8_t i = 0; i < channelsCount && configComplete; i++) {
        ChannelSettings* settingsPtr = &(channelSettings[i]);
        JsonObject channel = top[settingsPtr->settingName];

        configComplete &= !channel.isNull();

        configComplete &= getJsonValue(channel[F("Enabled")], settingsPtr->isEnabled);
        configComplete &= getJsonValue(channel[F("Name")], settingsPtr->name);
        configComplete &= getJsonValue(channel[F("Units")], settingsPtr->units);
        configComplete &= getJsonValue(channel[F("Multiplier")], settingsPtr->multiplier);
        configComplete &= getJsonValue(channel[F("Offset")], settingsPtr->offset);
        configComplete &= getJsonValue(channel[F("Decimals")], settingsPtr->decimals);

        hasEnabledChannels |= settingsPtr->isEnabled;
      }

      configComplete &= getJsonValue(top[F("Loop Interval")], loopInterval);

      isEnabled = isInitialized && configComplete && hasEnabledChannels;

      return configComplete;
    }

    uint16_t getId()
    {
      return USERMOD_ID_ADS1115;
    }

  private:
    static const uint8_t channelsCount = 8;

    ChannelSettings channelSettings[channelsCount] = {
      {
        "Differential reading from AIN0 (P) and AIN1 (N)",
        false,
        "Differential AIN0 AIN1",
        "V",
        ADS1X15_REG_CONFIG_MUX_DIFF_0_1,
        1,
        0,
        3
      },
      {
        "Differential reading from AIN0 (P) and AIN3 (N)",
        false,
        "Differential AIN0 AIN3",
        "V",
        ADS1X15_REG_CONFIG_MUX_DIFF_0_3,
        1,
        0,
        3
      },
      {
        "Differential reading from AIN1 (P) and AIN3 (N)",
        false,
        "Differential AIN1 AIN3",
        "V",
        ADS1X15_REG_CONFIG_MUX_DIFF_1_3,
        1,
        0,
        3
      },
      {
        "Differential reading from AIN2 (P) and AIN3 (N)",
        false,
        "Differential AIN2 AIN3",
        "V",
        ADS1X15_REG_CONFIG_MUX_DIFF_2_3,
        1,
        0,
        3
      },
      {
        "Single-ended reading from AIN0",
        false,
        "Single-ended AIN0",
        "V",
        ADS1X15_REG_CONFIG_MUX_SINGLE_0,
        1,
        0,
        3
      },
      {
        "Single-ended reading from AIN1",
        false,
        "Single-ended AIN1",
        "V",
        ADS1X15_REG_CONFIG_MUX_SINGLE_1,
        1,
        0,
        3
      },
      {
        "Single-ended reading from AIN2",
        false,
        "Single-ended AIN2",
        "V",
        ADS1X15_REG_CONFIG_MUX_SINGLE_2,
        1,
        0,
        3
      },
      {
        "Single-ended reading from AIN3",
        false,
        "Single-ended AIN3",
        "V",
        ADS1X15_REG_CONFIG_MUX_SINGLE_3,
        1,
        0,
        3
      },
    };
    float readings[channelsCount] = {0, 0, 0, 0, 0, 0, 0, 0};

    unsigned long loopInterval = 1000;
    unsigned long lastTime = 0;

    Adafruit_ADS1115 ads;
    uint8_t activeChannel;

    bool isEnabled = false;
    bool isInitialized = false;

    static float round(float value, uint8_t decimals) {
      return roundf(value * powf(10, decimals)) / powf(10, decimals);
    }

    bool initChannel() {
      for (uint8_t i = 0; i < channelsCount; i++) {
        if (channelSettings[i].isEnabled) {
          activeChannel = i;
          return true;
        }
      }

      activeChannel = 0;
      return false;
    }

    void moveToNextChannel() {
      uint8_t oldActiveChannel = activeChannel;

      do
      {
        if (++activeChannel >= channelsCount){
          activeChannel = 0;
        }
      }
      while (!channelSettings[activeChannel].isEnabled && oldActiveChannel != activeChannel);
    }

    void startReading() {
      ads.startADCReading(channelSettings[activeChannel].mux, /*continuous=*/false);
    }

    void updateResult() {
        int16_t results = ads.getLastConversionResults();
        readings[activeChannel] = ads.computeVolts(results);
    }
};

static ADS1115Usermod ads1115_v2;
REGISTER_USERMOD(ads1115_v2);