/*-------------------------------------------------------------------------
WLEDpixelBus - SPI 2-pin clocked LEDs driver implementation

written by Damian Schneider @dedehai 2026

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#include <SPI.h>

namespace WLEDpixelBus {

// SPI Bus: Hardware and Software SPI for 2-wire LEDs like APA102, WS2801
class SpiBus : public PixelBus {
public:
  SpiBus(int8_t dataPin, int8_t clockPin, uint16_t frequencykHz, uint8_t colorOrder, uint8_t numChannels, bool useHardware = true, uint8_t ledType = 0);
  ~SpiBus() override;

  bool begin() override;
  void end() override;

  bool show() override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return _useHardware ? "HW_SPI" : "SW_SPI"; }
#endif
  void setApa102HwBri(uint8_t v) override { _apa102HwBri = v & 0x1F; } // Set the APA102 per-pixel 5-bit hardware brightness step (0–31).

private:
  uint32_t _clockHz;          // SPI clock in Hz, derived from frequencykHz by the constructor
  uint8_t  _apa102HwBri = 31; // APA102 5-bit hardware brightness step (0–31); 31 = max (default)
  int8_t   _dataPin;
  int8_t   _clockPin;
  bool     _useHardware;
  bool     _initialized;

  // Pre-computed GPIO bitmasks for fast bit-bang output (set in begin())
#if defined(ARDUINO_ARCH_ESP32)
  bool     _dataHigh;  // true when data pin >= 32 (use GPIO1 register)  TODO: this is only for ESP32's with a high data bank (ESP32, S3, maybe C6,C5)
  bool     _clkHigh;   // true when clock pin >= 32 (use GPIO1 register)
  uint32_t _dataMask;  // GPIO set/clear mask for data pin
  uint32_t _clkMask;   // GPIO set/clear mask for clock pin
#elif defined(ARDUINO_ARCH_ESP8266)
  uint32_t _dataMask;
  uint32_t _clkMask;
#endif

  inline void bbSetData(bool high) const;
  inline void bbSetClk(bool high) const;

  void sendByte(uint8_t d);
  void sendStartFrame(uint16_t numPixels);
  void sendEndFrame(uint16_t numPixels);
};

} // namespace WLEDpixelBus

