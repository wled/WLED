#include "WLEDpixelBus_ESP8266.h"

#ifdef WLEDPB_ESP8266

#include <Arduino.h>
#include <HardwareSerial.h>
#include <uart_register.h>
#include <eagle_soc.h>
#include <i2s_reg.h>
#include <slc_register.h>
#include <user_interface.h>

#ifdef ARDUINO_ESP8266_MAJOR
#include <core_esp8266_i2s.h>
#else
#include <i2s.h>
#endif

namespace WLEDpixelBus {

//==============================================================================
// ESP8266 UART Bus
//==============================================================================

Esp8266UartBus::Esp8266UartBus(int8_t pin, const LedTiming& timing, ColorOrder order)
    : _pin(pin), _timing(timing), _order(order), _initialized(false), _encodeLut(nullptr), _encodeBuffer(nullptr), _encodeBufferSize(0) {}

Esp8266UartBus::~Esp8266UartBus() {
    end();
    if (_encodeLut) free(_encodeLut);
    if (_encodeBuffer) free(_encodeBuffer);
}

void Esp8266UartBus::buildLut() {
    if (!_encodeLut) _encodeLut = (uint8_t*)malloc(256 * 4);
    if (!_encodeLut) return;
    const uint8_t uartData[4] = {0b110111, 0b000111, 0b110100, 0b000100};
    for (int i=0; i<256; i++) {
        _encodeLut[i*4 + 0] = uartData[(i >> 6) & 0x03];
        _encodeLut[i*4 + 1] = uartData[(i >> 4) & 0x03];
        _encodeLut[i*4 + 2] = uartData[(i >> 2) & 0x03];
        _encodeLut[i*4 + 3] = uartData[i & 0x03];
    }
}

bool Esp8266UartBus::allocateBuffer(size_t encodedDataLen) {
    if (_encodeBufferSize >= encodedDataLen) return true;
    if (_encodeBuffer) free(_encodeBuffer);
    _encodeBuffer = (uint8_t*)malloc(encodedDataLen);
    if (!_encodeBuffer) return false;
    _encodeBufferSize = encodedDataLen;
    return true;
}

bool Esp8266UartBus::begin() {
    if (_initialized) return true;
    if (_pin != 1 && _pin != 2) return false; 
    buildLut();
    updateUartTiming();
    _initialized = true;
    return true;
}

void Esp8266UartBus::end() {
    if (!_initialized) return;
    if (_pin == 2) Serial1.end();
    else Serial.end();
    _initialized = false;
}

void Esp8266UartBus::updateUartTiming() {
    uint32_t periodNs = _timing.bitPeriod(); 
    if (periodNs < 200) periodNs = 1250;
    uint32_t baud = 4000000000ULL / periodNs;
    
    if (_pin == 2) {
        Serial1.begin(baud, SERIAL_6N1, SERIAL_TX_ONLY);
        USC0(1) |= (1 << UCTXI); // set inverted logic
    } else {
        Serial.begin(baud, SERIAL_6N1, SERIAL_TX_ONLY);
        USC0(0) |= (1 << UCTXI); // set inverted logic
    }
}

bool Esp8266UartBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!pixels) { pixels = _pixelData; numPixels = _numPixels; cct = _cctData; }
    if (!_initialized || !pixels || numPixels == 0) return false;

    size_t bpp = (_order >= ColorOrder::RGBWC) ? 5 : ((_order >= ColorOrder::RGBW) ? 4 : 3);
    size_t outLen = numPixels * bpp * 4;
    if (!allocateBuffer(outLen)) return false;

    ColorEncoder encoder(_order);
    uint8_t* out = _encodeBuffer;
    uint8_t pData[5];

    for (size_t i = 0; i < numPixels; i++) {
        encoder.encode(pixels[i], cct ? &cct[i] : nullptr, pData);
        for (size_t b = 0; b < bpp; b++) {
            uint8_t v = pData[b];
            *out++ = _encodeLut[v * 4 + 0];
            *out++ = _encodeLut[v * 4 + 1];
            *out++ = _encodeLut[v * 4 + 2];
            *out++ = _encodeLut[v * 4 + 3];
        }
    }

    uint8_t uartNum = (_pin == 2) ? 1 : 0;
    out = _encodeBuffer;
    size_t len = outLen;
    
    // Busy wait UART filling to ensure stable rendering (OS interrupts intact but tight timing loop)
    while(len > 0) {
        if (((USS(uartNum) >> USTXC) & 0xff) < 127) {
            USF(uartNum) = *out++;
            len--;
        }
    }
    return true;
}

bool Esp8266UartBus::canShow() const {
    if (!_initialized) return false;
    uint8_t uartNum = (_pin == 2) ? 1 : 0;
    return (((USS(uartNum) >> USTXC) & 0xff) == 0); // ready when FIFO empty
}

void Esp8266UartBus::waitComplete() {
    while (!canShow()) { yield(); }
}


//==============================================================================
// ESP8266 BitBang Bus
//==============================================================================

Esp8266BitBangBus::Esp8266BitBangBus(int8_t pin, const LedTiming& timing, ColorOrder order)
    : _pin(pin), _timing(timing), _order(order), _initialized(false) {}

Esp8266BitBangBus::~Esp8266BitBangBus() {
    end();
}

bool Esp8266BitBangBus::begin() {
    if (_initialized) return true;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    setTiming(_timing);
    _initialized = true;
    return true;
}

void Esp8266BitBangBus::end() {
    pinMode(_pin, INPUT);
    _initialized = false;
}

bool Esp8266BitBangBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!pixels) { pixels = _pixelData; numPixels = _numPixels; cct = _cctData; }
    if (!_initialized || !pixels || numPixels == 0) return false;

    size_t bpp = (_order >= ColorOrder::RGBWC) ? 5 : ((_order >= ColorOrder::RGBW) ? 4 : 3);
    ColorEncoder encoder(_order);
    uint32_t mask = 1 << _pin;
    uint8_t pData[5];

    os_intr_lock();
    for (size_t i = 0; i < numPixels; i++) {
        encoder.encode(pixels[i], cct ? &cct[i] : nullptr, pData);
        for (size_t b = 0; b < bpp; b++) {
            uint8_t v = pData[b];
            for (int bit = 7; bit >= 0; bit--) {
                uint32_t t = ESP.getCycleCount();
                if (v & (1 << bit)) {
                    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask);
                    while((ESP.getCycleCount() - t) < _t1h);
                    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask);
                    while((ESP.getCycleCount() - t) < (_t1h + _t1l));
                } else {
                    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask);
                    while((ESP.getCycleCount() - t) < _t0h);
                    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask);
                    while((ESP.getCycleCount() - t) < (_t0h + _t0l));
                }
            }
        }
    }
    os_intr_unlock();
    return true;
}

bool Esp8266BitBangBus::canShow() const {
    return _initialized;
}

void Esp8266BitBangBus::waitComplete() {
    // Blocking show, always complete upon return
}


//==============================================================================
// ESP8266 DMA Bus 
//==============================================================================

Esp8266DmaBus::Esp8266DmaBus(int8_t pin, const LedTiming& timing, ColorOrder order)
    : _pin(pin), _timing(timing), _order(order), _initialized(false), _encodeBuffer(nullptr), _encodeBufferSize(0) {}

Esp8266DmaBus::~Esp8266DmaBus() {
    end();
    if (_encodeBuffer) {
        free(_encodeBuffer);
        _encodeBuffer = nullptr;
    }
}

bool Esp8266DmaBus::allocateBuffer(size_t len) {
    if (_encodeBufferSize >= len) return true;
    if (_encodeBuffer) free(_encodeBuffer);
    _encodeBuffer = (uint8_t*)malloc(len);
    if (!_encodeBuffer) return false;
    _encodeBufferSize = len;
    return true;
}

void Esp8266DmaBus::updateI2sTiming() {
    // Setup using Native ESP8266 Core I2S
    uint32_t bitPeriod = _timing.bitPeriod();
    if (bitPeriod == 0) bitPeriod = 1250;
    
    // We map 4 I2S bits to 1 LED bit (4-step cadence).
    // Each LED bit needs 4 I2S periods, thus our desired I2S clock is 4x the LED bit rate.
    // periodNs is time per LED bit. I2S bit clock period = bitPeriod / 4.
    // I2S bits/sec = 1,000,000,000 / (bitPeriod / 4) = 4,000,000,000 / bitPeriod.
    // Using core `i2s_begin` which sets a clock and routing:
    // Actually, `i2s_begin` expects a sample rate for 32-bit (stereo 16+16) frames.
    // Sample rate = (I2S bits/sec) / 32
    uint32_t sampleRate = (4000000000ULL / bitPeriod) / 32;
    i2s_set_rate(sampleRate);
}

bool Esp8266DmaBus::begin() {
    if (_initialized) return true;
    if (_pin != 3) return false; // ESP8266 I2S RX is GPIO3 for NeoPixels
    
    // Begin core I2S subsystem
    i2s_begin(); 
    
    // To make GPIO3 act as I2S Data Out instead of I2S IN, configure registers manually:
    // (This disables I2S BCLK and WS on their default pins so we don't interfere with other hardware)
    pinMode(3, FUNCTION_3); // Set RX to I2S0_DATA
    
    // Disable clock/ws pins on GPIO15/2
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15); 
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2); 

    updateI2sTiming();

    _initialized = true;
    return true;
}

void Esp8266DmaBus::end() {
    if (!_initialized) return;
    i2s_end();
    pinMode(_pin, INPUT);
    _initialized = false;
}

bool Esp8266DmaBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!pixels) { pixels = _pixelData; numPixels = _numPixels; cct = _cctData; }
    if (!_initialized || !pixels || numPixels == 0) return false;

    size_t bpp = (_order >= ColorOrder::RGBWC) ? 5 : ((_order >= ColorOrder::RGBW) ? 4 : 3);
    
    // 4 step cadence per LED bit. 
    // 1 LED bit = 4 I2S bits (half-byte / nibble).
    // 1 LED byte (8 bits) = 32 I2S bits (4 bytes).
    size_t outLen = numPixels * bpp * 4 + 40; // + 40 zero bytes for reset (latch)
    if (!allocateBuffer(outLen)) return false;

    memset(_encodeBuffer, 0, outLen);

    ColorEncoder encoder(_order);
    uint32_t* out32 = (uint32_t*)_encodeBuffer;
    uint8_t pData[5];

    // Using inverted 4-step cadence:
    // Normally:
    // 1 = 0b1110 (0xE)
    // 0 = 0b1000 (0x8)
    for (size_t i = 0; i < numPixels; i++) {
        encoder.encode(pixels[i], cct ? &cct[i] : nullptr, pData);
        for (size_t b = 0; b < bpp; b++) {
            uint32_t i2sData = 0;
            uint8_t v = pData[b];
            for (int bit = 7; bit >= 0; bit--) {
                i2sData <<= 4;
                if (v & (1 << bit)) {
                    i2sData |= 0xE; // 1110
                } else {
                    i2sData |= 0x8; // 1000
                }
            }
            *out32++ = i2sData;
        }
    }

    // Write using Core I2S: It processes full `uint32` buffer natively over I2S
    uint32_t* buf32 = (uint32_t*)_encodeBuffer;
    for (size_t i = 0; i < outLen / 4; i++) {
        i2s_write_sample(buf32[i]);
    }

    return true;
}

bool Esp8266DmaBus::canShow() const {
    return _initialized && !i2s_is_full();
}

void Esp8266DmaBus::waitComplete() {
    // Not strictly supported blocking via I2S, wait for FIFO room
    while (i2s_is_full()) { yield(); }
}

} // namespace WLEDpixelBus

#endif // WLEDPB_ESP8266
