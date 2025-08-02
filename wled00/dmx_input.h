#pragma once
#include <cstdint>
#include <esp_dmx.h>
#include <atomic>
#include <mutex>

/*
 * Support for DMX/RDM input via serial (e.g. max485) on ESP32
 * ESP32 Library from:
 * https://github.com/someweisguy/esp_dmx
 */
class DMXInput
{
public:
  void init(uint8_t rxPin, uint8_t txPin, uint8_t enPin, uint8_t inputPortNum);
  void update();

  /**disable dmx receiver (do this before disabling the cache)*/
  void disable();
  void enable();

  /// True if dmx is currently connected
  bool isConnected() const { return connected; }

private:
  /// @return true if rdm identify is active
  bool isIdentifyOn() const;

  /**
   * Checks if the global dmx config has changed and updates the changes in rdm
   */
  void checkAndUpdateConfig();

  /// overrides everything and turns on all leds
  void turnOnAllLeds();

  /// installs the dmx driver
  /// @return false on fail
  bool installDriver();

  /// is called by the dmx receive task regularly to receive new dmx data
  void updateInternal();

  // is invoked whenver the dmx start address is changed via rdm
  friend void rdmAddressChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                                  void *context);

  // is invoked whenever the personality is changed via rdm
  friend void rdmPersonalityChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                                      void *context);

  /// The internal dmx task.
  /// This is the main loop of the dmx receiver. It never returns.
  friend void dmxReceiverTask(void * context);

  uint8_t inputPortNum = 255; 
  uint8_t rxPin = 255;
  uint8_t txPin = 255;
  uint8_t enPin = 255;

  /// is written to by the dmx receive task.
  byte dmxdata[DMX_PACKET_SIZE]; 
  /// True once the dmx input has been initialized successfully
  bool initialized = false; // true once init finished successfully
  /// True if dmx is currently connected
  std::atomic<bool> connected{false};
  std::atomic<bool> identify{false};
  /// Timestamp of the last time a dmx frame was received
  unsigned long lastUpdate = 0;

  /// Taskhandle of the dmx task that is running in the background 
  TaskHandle_t task;
  /// Guards access to dmxData
  std::mutex dmxDataLock;
  
};
