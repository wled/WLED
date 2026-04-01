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
  SpiBus(int8_t dataPin, int8_t clockPin, const LedTiming& timing, ColorOrder order, bool useHardwareSpi = true);
  ~SpiBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return _useHardware ? "HW_SPI" : "SW_SPI"; }

  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(ColorOrder order) { _order = order; }

private:
  int8_t _dataPin;
  int8_t _clockPin;
  LedTiming _timing;
  ColorOrder _order;
  bool _useHardware;
  bool _initialized;

  void sendByte(uint8_t d);
  void sendStartFrame();
  void sendEndFrame();
};

} // namespace WLEDpixelBus

