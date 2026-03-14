/*
 * EspDmxOutput.h
 *
 * WLED DMX output implementation for ESP32 using the `esp_dmx` library.
 */
#pragma once

#include <inttypes.h>

class EspDmxOutput {
public:
  void initWrite(int maxChan);
  void write(int channel, uint8_t value);
  void update();
};


