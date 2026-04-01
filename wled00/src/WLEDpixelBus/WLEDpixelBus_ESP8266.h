#pragma once

#include "WLEDpixelBus.h"

#ifdef WLEDPB_ESP8266

namespace WLEDpixelBus {

//==============================================================================
// ESP8266 UART Bus (Asynchronous via UART1/UART0)
//==============================================================================

class Esp8266UartBus : public PixelBus {
public:
  Esp8266UartBus(int8_t pin, const LedTiming& timing, ColorOrder order);
  ~Esp8266UartBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return "ESP8266_UART"; }

  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(ColorOrder order) { _order = order; }

  static void UartIsr(void* arg);
  static Esp8266UartBus* s_instances[2];

private:
  int8_t _pin;
  LedTiming _timing;
  ColorOrder _order;
  bool _initialized;

  uint8_t* _asyncBuf = nullptr;
  uint8_t* _asyncBufEnd = nullptr;

  void updateUartTiming();
  bool allocateBuffer(size_t encodedDataLen);

  uint8_t* _encodeBuffer = nullptr;
  size_t _encodeBufferSize = 0;
};

//==============================================================================
// ESP8266 DMA Bus (Via I2S)
//==============================================================================

class Esp8266DmaBus : public PixelBus {
public:
  Esp8266DmaBus(int8_t pin, const LedTiming& timing, ColorOrder order);
  ~Esp8266DmaBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return "ESP8266_DMA"; }

  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(ColorOrder order) { _order = order; }

private:
  int8_t _pin; // Only RX pin (GPIO3) supported for I2S DMA on ESP8266
  LedTiming _timing;
  ColorOrder _order;
  bool _initialized;

  void updateI2sTiming();
  bool allocateBuffer(size_t numPixels);

  uint8_t* _encodeBuffer = nullptr;
  size_t _encodeBufferSize = 0;
};

//==============================================================================
// ESP8266 BitBang Bus
//==============================================================================

class Esp8266BitBangBus : public PixelBus {
public:
  Esp8266BitBangBus(int8_t pin, const LedTiming& timing, ColorOrder order);
  ~Esp8266BitBangBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return "ESP8266_BB"; }

  void setTiming(const LedTiming& timing);
  void setColorOrder(ColorOrder order) { _order = order; }

private:
  int8_t _pin;
  LedTiming _timing;
  ColorOrder _order;
  bool _initialized;
  
  // Cycle counts
  uint32_t _t0h, _t0l, _t1h, _t1l;
};

} // namespace WLEDpixelBus

#endif // WLEDPB_ESP8266

