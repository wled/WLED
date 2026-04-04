#pragma once

#include "WLEDpixelBus.h"
#include <SPI.h>

namespace WLEDpixelBus {

//==============================================================================
// SPI Bus (Hardware and Software SPI for 2-wire LEDs like APA102, WS2801)
//==============================================================================

class SpiBus : public PixelBus {
public:
  /**
   * Create SPI bus
   * @param dataPin MOSI pin
   * @param clockPin SCLK pin
   * @param timing LED timing parameters (used mostly for bit rate/clock speed)
   * @param order Color order
   * @param useHardwareSpi True to use hardware SPI peripheral (faster), false for bit-bang
   */
  SpiBus(int8_t dataPin, int8_t clockPin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, bool useHardwareSpi = true);
  ~SpiBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return _useHardware ? "HW_SPI" : "SW_SPI"; }

  bool setPixel(uint16_t pos, uint32_t c, uint8_t ww, uint8_t cw) override;
  uint32_t getPixelColor(uint16_t pix) const override;

  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(uint8_t co);

private:
  int8_t _dataPin;
  int8_t _clockPin;
  LedTiming _timing;
  ColorEncoder _encoder;
  bool _useHardware;
  bool _initialized;

  void sendByte(uint8_t d);
  void sendStartFrame();
  void sendEndFrame();
};

} // namespace WLEDpixelBus

