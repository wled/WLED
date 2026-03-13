#pragma once

#include "WLEDpixelBus.h"

namespace WLEDpixelBus {

//==============================================================================
// RMT Bus - Works on all ESP32 variants
//==============================================================================

#include "driver/rmt.h"

class RmtBus : public IBus {
public:
    /**
     * Create RMT bus
     * @param pin GPIO pin
     * @param timing LED timing
     * @param order Color order
     * @param channel RMT channel (-1 for auto)
     */
    RmtBus(int8_t pin, const LedTiming& timing, ColorOrder order,
           int8_t channel = -1);
    ~RmtBus() override;

    bool begin() override;
    void end() override;

    bool show(const uint32_t* pixels, uint16_t numPixels,
              const CctPixel* cct = nullptr) override;
    bool canShow() const override;
    void waitComplete() override;
    const char* getType() const override { return "RMT"; }

    // Configuration
    void setInverted(bool inv) { _inverted = inv; }
    void setTiming(const LedTiming& timing);
    void setColorOrder(ColorOrder order);

    // Reset the auto-allocation counter (call before re-creating buses)
    static void resetAutoChannel() { s_nextAutoChannel = 0; }

private:
    int8_t _pin;
    int8_t _channel;
    LedTiming _timing;
    ColorOrder _order;
    bool _inverted;
    bool _initialized;
    
    rmt_channel_t _rmtChannel;
    uint32_t _rmtBit0;
    uint32_t _rmtBit1;
    uint16_t _rmtResetTicks;

    // Encode buffer
    uint8_t* _encodeBuffer;
    size_t _encodeBufferSize;

    static uint8_t s_nextAutoChannel;  // auto-allocation counter
    static uint8_t s_activeChannelMask; // bitmask of initialized channels

    void updateRmtTiming();
    bool allocateBuffer(uint16_t numPixels);

    // Static translate callback
    static void IRAM_ATTR translateCB(const void* src, rmt_item32_t* dest,
                                       size_t src_size, size_t wanted_num,
                                       size_t* translated_size, size_t* item_num);
};

} // namespace WLEDpixelBus
