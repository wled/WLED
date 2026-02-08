/******************************************************************************
 * EspDmxOutput.cpp
 *
 * DMX output via `esp_dmx` on ESP32.
 * Keeps the minimal API WLED uses: initWrite(), write(), update().
 ******************************************************************************/

/* ----- LIBRARIES ----- */
#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2)

#include "EspDmxOutput.h"
#include <esp_dmx.h>

#define dmxMaxChannel 512
#define defaultMax 32

// Some new MCUs (-S2, -C3) don't have HardwareSerial(2)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
  #if SOC_UART_NUM < 3
  #error DMX output is not possible on your MCU, as it does not have HardwareSerial(2)
  #endif
#endif

static constexpr dmx_port_t dmxPort = DMX_NUM_2;

// Pin defaults. Change these if needed for your hardware.
static const int txPin = 2;                  // transmit DMX data over this pin (default is GPIO2)
static const int rxPin = DMX_PIN_NO_CHANGE;  // RX unused for DMX output
static const int rtsPin = DMX_PIN_NO_CHANGE; // RS485 DE/RE pin (UART RTS). Set to a GPIO to control transceiver direction.

// DMX value array and size. Entry 0 holds start code, so we need 512+1 elements.
static uint8_t dmxData[DMX_PACKET_SIZE] = {0};
// Maximum slot count configured by initWrite() (includes start code slot 0).
static size_t maxSize = 0;
// Current transmit size (includes start code slot 0). This grows as channels are written.
static size_t txSize = 1;
static bool dmxInstalled = false;

void EspDmxOutput::initWrite(int chanQuant) {
  if (chanQuant > dmxMaxChannel || chanQuant <= 0) chanQuant = defaultMax;
  maxSize = static_cast<size_t>(chanQuant) + 1; // +1 for start code
  txSize = 1;                                   // start with just start code

  // Configure the driver if needed.
  if (!dmx_driver_is_installed(dmxPort)) {
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmxInstalled = dmx_driver_install(dmxPort, &config, DMX_INTR_FLAGS_DEFAULT);
  } else {
    dmxInstalled = true;
  }

  if (dmxInstalled) {
    dmx_set_pin(dmxPort, txPin, rxPin, rtsPin);

    // Ensure NULL start code (slot 0).
    dmxData[0] = DMX_SC;
    dmx_write_slot(dmxPort, 0, DMX_SC);
  }
}

void EspDmxOutput::write(int Channel, uint8_t value) {
  if (!dmxInstalled) return;

  // Allow slot 0 writes, but start code is enforced in update().
  if (Channel < 0) Channel = 0;
  if (Channel > dmxMaxChannel) Channel = dmxMaxChannel;

  const size_t neededSize = static_cast<size_t>(Channel) + 1;
  if (neededSize > txSize) txSize = neededSize;
  if (maxSize > 0 && txSize > maxSize) txSize = maxSize;
  if (txSize > DMX_PACKET_SIZE) txSize = DMX_PACKET_SIZE;

  dmxData[Channel] = value;
  dmx_write_slot(dmxPort, static_cast<size_t>(Channel), value);
}

void EspDmxOutput::update() {
  if (!dmxInstalled) return;

  // Always send the break signal first
  dmx_write_slot(dmxPort, 0, DMX_SC);

  // Send the frame currently staged in the driver buffer.
  dmx_send(dmxPort, txSize);
}

#endif
#endif


