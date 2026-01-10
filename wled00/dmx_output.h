//
// Created by will on 1/10/26.
//

#ifndef DMX_OUTPUT_H
#define DMX_OUTPUT_H

#if defined(ESP8266)
#include "src/dependencies/dmx/ESPDMX.h"
DMXESPSerial dmx;
#else
#include <esp_dmx.h>
/**
 * Support for DMX Output via serial (e.g. max485) on ESP32
 * ESP32 Library from:
 * https://github.com/someweisguy/esp_dmx
 */
class DMXOutput
{
public:
    void init(uint8_t txPin);
    void write(uint8_t channel, uint8_t value);
    void update();
private:
    byte dmxdata[DMX_PACKET_SIZE];
    /* Next, lets decide which DMX port to use. The ESP32 has either 2 or 3 ports.
Port 0 is typically used to transmit serial data back to your Serial Monitor,
so we shouldn't use that port. Lets use port 1! */
    dmx_port_t dmxPort = 1;
};

DMXOutput dmx;
#endif


#endif //DMX_OUTPUT_H
