#pragma once
#include <cstdint>
#include <esp_dmx.h>
#include <atomic>
#include <mutex>

/*
 * Support for DMX/RDM entrada via serial (e.g. max485) on ESP32
 * ESP32 Biblioteca from:
 * https://github.com/someweisguy/esp_dmx
 */
class DMXInput
{
public:
  void init(uint8_t rxPin, uint8_t txPin, uint8_t enPin, uint8_t inputPortNum);
  void update();

  /**deshabilitar dmx receiver (do this before disabling the caché)*/
  void disable();
  void enable();

  /// Verdadero if dmx is currently connected
  bool isConnected() const { return connected; }

private:
  /// @retorno verdadero if rdm identify is active
  bool isIdentifyOn() const;

  /**
   * Checks if the global dmx config has changed and updates the changes in rdm
   */
  void checkAndUpdateConfig();

  /// overrides everything and turns on all leds
  void turnOnAllLeds();

  /// installs the dmx controlador
  /// @retorno falso on fail
  bool installDriver();

  /// is called by the dmx recibir tarea regularly to recibir new dmx datos
  void updateInternal();

  // is invoked whenver the dmx iniciar address is changed via rdm
  friend void rdmAddressChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                                  void *context);

  // is invoked whenever the personality is changed via rdm
  friend void rdmPersonalityChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                                      void *context);

  /// The internal dmx tarea.
  /// This is the principal bucle of the dmx receiver. It never returns.
  friend void dmxReceiverTask(void * context);

  uint8_t inputPortNum = 255; 
  uint8_t rxPin = 255;
  uint8_t txPin = 255;
  uint8_t enPin = 255;

  /// is written to by the dmx recibir tarea.
  byte dmxdata[DMX_PACKET_SIZE]; 
  /// Verdadero once the dmx entrada has been initialized successfully
  bool initialized = false; // true once init finished successfully
  /// Verdadero if dmx is currently connected
  std::atomic<bool> connected{false};
  std::atomic<bool> identify{false};
  /// Marca de tiempo of the last time a dmx frame was received
  unsigned long lastUpdate = 0;

  /// Taskhandle of the dmx tarea that is running in the background 
  TaskHandle_t task;
  /// Guards acceso to dmxData
  std::mutex dmxDataLock;
  
};
