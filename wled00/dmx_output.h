#ifndef DMX_OUTPUT_H
#define DMX_OUTPUT_H

/**
 * Unified DMX Output class for all platforms.
 *
 * On ESP8266: wraps DMXESPSerial (bit-banged DMX via Serial1)
 * On ESP32:   uses the esp_dmx library
 */

#define DMX_CHANNEL_TOP 512

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
    EspDmxOutput _dmx;
};

#endif // ESP8266 / ESP32

#endif // DMX_OUTPUT_H
