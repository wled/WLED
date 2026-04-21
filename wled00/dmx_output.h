/**
 * Unified DMX Output class for all platforms.
 *
 * On ESP8266: wraps DMXESPSerial (DMX via Serial1)
 * On ESP32:   uses own functions
 */

#ifndef DMX_OUTPUT_H
#define DMX_OUTPUT_H

#if defined(ESP8266)
#include "src/dependencies/dmx/ESPDMX.h"
#endif

#define DMX_CHANNEL_TOP 512
#define DMX_CHANNELS DMX_CHANNEL_TOP + 1

#define DMXSPEED       250000
#define DMXFORMAT      SERIAL_8N2
#define BREAKSPEED     83000
#define BREAKFORMAT    SERIAL_8N1


class DMXOutput {
  public:
    ~DMXOutput();
    bool init(uint8_t outputPin, uint8_t updateRate = 44, int8_t uartNo = -1);
    void write(uint16_t channel, uint8_t value);
    void writeBytes(uint16_t channelStart, uint8_t values[], uint16_t len);
    uint8_t read(uint16_t channel);
    bool readBytes(uint16_t channelStart, uint8_t values[], uint16_t len);
    bool update();
    bool handleDMXOutput();
    unsigned long getLastDmxOut();
    bool busy();
    void setUpdateRate(uint8_t updateRate);
    int timeToNextUpdate();
  private:
    #if defined(ESP8266)
    DMXESPSerial _dmx;
    #else
    HardwareSerial* _dmxSerial;
    uint8_t _outputPin;     // DMX TX pin
    uint8_t _dmxData[DMX_CHANNELS] = {0};
    unsigned int _halTxBufSize;
    #endif
    uint8_t _updateRate;
    unsigned long _lastDmxOutMillis = 0;
};

#endif // DMX_OUTPUT_H
