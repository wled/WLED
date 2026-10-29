/*-------------------------------------------------------------------------

WLEDpixelBus - RMT output driver implementation

written by Damian Schneider @dedehai 2026

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#include "RmtHIDriver.h"  // high interrupt priority driver (defines WLEDPB_USE_RMTHI on supported targets)

namespace WLEDpixelBus {

// RMT Bus: uses ESP32 RMT peripheral for LED timing

class RmtBus : public PixelBus {
public:
  RmtBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType = 0);
  ~RmtBus() override;

  bool begin() override;
  void end() override;

  bool show() override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "RMT"; }
#endif

  static void setExpectedChannelCount(uint8_t expected) { expectedChannels = (expected > 0) ? expected : 1; }
  static void resetChannelTracking() {
    allocatedCount = 0;
    currentChannelIndex = 0;
    usedBlocks = 0;
  }

private:
  int8_t _pin;
  int8_t _channel;
  bool _initialized;
  bool _driverInstalled;
  LedTiming _timing;

  // _encodeBuffer and _encodeBufferSize are in PixelBus base
  static uint8_t expectedChannels; // TODO: make none static? would save a few bytes of ram but use more heap
  static uint8_t allocatedCount;
  static uint8_t currentChannelIndex;
  static uint8_t usedBlocks;
};

} // namespace WLEDpixelBus
