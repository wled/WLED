#include "wled.h"

namespace ADS1115
{
  struct ChannelSettings {
    const String settingName;
    bool isEnabled;
    String name;
    String units;
    const uint16_t mux;
    float multiplier;
    float offset;
    uint8_t decimals;
  };
}