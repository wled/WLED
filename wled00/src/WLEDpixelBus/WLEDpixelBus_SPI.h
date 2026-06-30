/*-------------------------------------------------------------------------
WLEDpixelBus - SPI 2-pin clocked LEDs driver implementation

supports ESP32, S3, S2 and C3 both hardware SPI and BitBanged output
supports hardware brightness on APA102 for improved color resolution

written by Damian Schneider @dedehai 2026

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#include <SPI.h>

namespace WLEDpixelBus {

//==========================================================================
// SPI Bus: Hardware and Software SPI for 2-wire LEDs like APA102, WS2801
//==========================================================================

class SpiBus : public PixelBus {
public:
  /**
   * Create SPI bus
   * @param dataPin  MOSI pin
   * @param clockPin SCLK pin
   * @param timing   LED timing (bitPeriod() determines SPI clock, clamped to 1–20 MHz)
   * @param colorOrder Color order
   * @param numChannels Number of color channels
   * @param useHardwareSpi True = hardware SPI peripheral, false = GPIO bit-bang
   * @param ledType  WLED LED type constant (TYPE_APA102, TYPE_WS2801, …)
   */
  SpiBus(int8_t dataPin, int8_t clockPin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, bool useHardwareSpi = true, uint8_t ledType = 0);
  ~SpiBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return _useHardware ? "HW_SPI" : "SW_SPI"; }
#endif
  void setColorOrder(uint8_t co);

  /**
   * Set the APA102 per-pixel 5-bit hardware brightness step (0–31).
   * Stored and sent in the brightness byte: 0xE0 | step.
   * Overrides the PixelBus no-op base implementation.
   */
  void setApa102HwBri(uint8_t v) override { _apa102HwBri = v & 0x1F; }

private:
  uint8_t  _apa102HwBri = 31; // APA102 5-bit hardware brightness step (0–31); 31 = max (default)
  int8_t   _dataPin;
  int8_t   _clockPin;
  LedTiming _timing;
  bool     _useHardware;
  bool     _initialized;
  uint32_t _clockHz;   // SPI clock in Hz, derived from timing at begin() time
  // TODO: need _inverted flag here as well? output inversion is currently not supported on SPI type bus

  // Pre-computed GPIO bitmasks for fast bit-bang output (set in begin())
#if defined(ARDUINO_ARCH_ESP32)
  uint32_t _dataMask;  // GPIO set/clear mask for data pin
  uint32_t _clkMask;   // GPIO set/clear mask for clock pin
  bool     _dataHigh;  // true when data pin >= 32 (use GPIO1 register)  TODO: this is only for ESP32
  bool     _clkHigh;   // true when clock pin >= 32 (use GPIO1 register)
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

