#pragma once
#include <cstdint>
#include <esp_dmx.h>
/*
 * Support for DMX/RDM input via serial (e.g. max485) on ESP32
 * ESP32 Library from:
 * https://github.com/sparkfun/SparkFunDMX
 */
class DMXInput
{
public:
  void init(uint8_t rxPin, uint8_t txPin, uint8_t enPin, uint8_t inputPortNum);
  void update();

  /**disable dmx receiver (do this before disabling the cache)*/
  void disable();
  void enable();

private:
  /// @return true if rdm identify is active
  bool isIdentifyOn() const;

  /**
   * Checks if the global dmx config has changed and updates the changes in rdm
   */
  void checkAndUpdateConfig();

  /// overrides everything and turns on all leds
  void turnOnAllLeds();

  dmx_config_t createConfig() const;
  uint8_t inputPortNum = 255; // TODO make this configurable
  /// True once the dmx input has been initialized successfully
  bool initialized = false; // true once init finished successfully
  /// True if dmx is currently connected
  bool connected = false;
  /// Timestamp of the last time a dmx frame was received
  unsigned long lastUpdate = 0;
};
