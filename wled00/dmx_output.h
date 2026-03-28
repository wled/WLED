#ifndef DMX_OUTPUT_H
#define DMX_OUTPUT_H

/**
 * Unified DMX Output class for all platforms.
 *
 * On ESP8266: wraps DMXESPSerial (bit-banged DMX via Serial1)
 * On ESP32:   uses the esp_dmx library
 */

#if defined(ESP8266)
#include "src/dependencies/dmx/ESPDMX.h"

class DMXOutput
{
public:
    void init(uint8_t outputPin);
    void write(int channel, uint8_t value);
    void update();
private:
    DMXESPSerial _dmx;
};

#else // ESP32

#include <esp_dmx.h>

class DMXOutput
{
public:
    void init(uint8_t outputPin);
    void write(int channel, uint8_t value);
    void update();
private:
    byte dmxdata[DMX_PACKET_SIZE];
    /* The ESP32 has either 2 or 3 UART ports.
       Port 0 is typically used for Serial Monitor,
       so we use port 1. */
    dmx_port_t dmxPort = 1;
};

#endif // ESP8266 / ESP32

#endif // DMX_OUTPUT_H
