/*-------------------------------------------------------------------------
WLEDpixelBus - Lightweight LED driver library for WLED

by @dedehai, 2026

Features:
- Runtime LED timing configuration
- Double-buffered DMA with interrupt-driven refilling (4-step cadence)
- Support for ESP32, ESP32-S2, ESP32-S3, ESP32-C3
- RMT, I2S parallel, and LCD parallel output methods
- RGBW uint32_t pixel buffer format (WLED native)
- Separate CCT (WW/CW) buffer support
- IDF v4.x compatible

-------------------------------------------------------------------------*/

#include "WLEDpixelBus.h"

#ifdef WLEDPB_SPI_SUPPORT
#undef FLAG_ATTR
#define FLAG_ATTR(TYPE)
#include "hal/spi_ll.h"
#include "driver/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
#endif

namespace WLEDpixelBus {

//==============================================================================
// Color Encoder Implementation
//==============================================================================

void ColorEncoder::encode(uint32_t pixel, const CctPixel* cct, uint8_t* out) const {
    uint8_t r = getR(pixel);
    uint8_t g = getG(pixel);
    uint8_t b = getB(pixel);
    uint8_t w = getW(pixel);

    // For CCT strips, use CCT data instead of W channel
    uint8_t ww = cct ? cct->ww : w;
    uint8_t cw = cct ? cct->cw : 0;

    switch (_order) {
        // RGB variants
        case ColorOrder::RGB: out[0]=r; out[1]=g; out[2]=b; break;
        case ColorOrder::GRB: out[0]=g; out[1]=r; out[2]=b; break;
        case ColorOrder::BRG: out[0]=b; out[1]=r; out[2]=g; break;
        case ColorOrder::RBG: out[0]=r; out[1]=b; out[2]=g; break;
        case ColorOrder::GBR: out[0]=g; out[1]=b; out[2]=r; break;
        case ColorOrder::BGR: out[0]=b; out[1]=g; out[2]=r; break;

        // RGBW variants
        case ColorOrder::RGBW: out[0]=r; out[1]=g; out[2]=b; out[3]=w; break;
        case ColorOrder::GRBW: out[0]=g; out[1]=r; out[2]=b; out[3]=w; break;
        case ColorOrder::BRGW: out[0]=b; out[1]=r; out[2]=g; out[3]=w; break;
        case ColorOrder::RBGW: out[0]=r; out[1]=b; out[2]=g; out[3]=w; break;
        case ColorOrder::GBRW: out[0]=g; out[1]=b; out[2]=r; out[3]=w; break;
        case ColorOrder::BGRW: out[0]=b; out[1]=g; out[2]=r; out[3]=w; break;
        case ColorOrder::WRGB: out[0]=w; out[1]=r; out[2]=g; out[3]=b; break;
        case ColorOrder::WGRB: out[0]=w; out[1]=g; out[2]=r; out[3]=b; break;
        case ColorOrder::WBRG: out[0]=w; out[1]=b; out[2]=r; out[3]=g; break;
        case ColorOrder::WRBG: out[0]=w; out[1]=r; out[2]=b; out[3]=g; break;
        case ColorOrder::WGBR: out[0]=w; out[1]=g; out[2]=b; out[3]=r; break;
        case ColorOrder::WBGR: out[0]=w; out[1]=b; out[2]=g; out[3]=r; break;

        default: out[0]=g; out[1]=r; out[2]=b; break;
    }
}

//==============================================================================
// RMT Bus Implementation
//==============================================================================

// Context for RMT translate callback - must be in DRAM for IRAM ISR access
static DRAM_ATTR struct {
    uint32_t bit0;
    uint32_t bit1;
    uint16_t resetDuration;
} s_rmtCtx;

// Static auto-channel counter for RmtBus
uint8_t RmtBus::s_nextAutoChannel = 0;
uint8_t RmtBus::s_activeChannelMask = 0;

RmtBus::RmtBus(int8_t pin, const LedTiming& timing, ColorOrder order, int8_t channel)
    : _pin(pin)
    , _channel(channel)
    , _timing(timing)
    , _order(order)
    , _inverted(false)
    , _initialized(false)
    , _rmtChannel(RMT_CHANNEL_0)
    , _rmtBit0(0)
    , _rmtBit1(0)
    , _rmtResetTicks(0)
    , _encodeBuffer(nullptr)
    , _encodeBufferSize(0)
{
}

RmtBus::~RmtBus() {
    end();
}

void RmtBus::updateRmtTiming() {
    // RMT clock:  80MHz with div=2 -> 40MHz -> 25ns per tick
    const uint8_t clockDiv = 2;
    const float tickNs = 25.0f;

    auto nsToTicks = [tickNs](uint16_t ns) -> uint16_t {
        uint16_t ticks = (uint16_t)((ns + tickNs / 2) / tickNs);
        return ticks > 0 ? ticks : 1;
    };

    uint16_t t0h = nsToTicks(_timing.t0h_ns);
    uint16_t t0l = nsToTicks(_timing.t0l_ns);
    uint16_t t1h = nsToTicks(_timing.t1h_ns);
    uint16_t t1l = nsToTicks(_timing.t1l_ns);

    rmt_item32_t bit0, bit1;

    if (_inverted) {
        bit0.level0 = 0; bit0.duration0 = t0h;
        bit0.level1 = 1; bit0.duration1 = t0l;
        bit1.level0 = 0; bit1.duration0 = t1h;
        bit1.level1 = 1; bit1.duration1 = t1l;
    } else {
        bit0.level0 = 1; bit0.duration0 = t0h;
        bit0.level1 = 0; bit0.duration1 = t0l;
        bit1.level0 = 1; bit1.duration0 = t1h;
        bit1.level1 = 0; bit1.duration1 = t1l;
    }

    _rmtBit0 = bit0.val;
    _rmtBit1 = bit1.val;
    _rmtResetTicks = nsToTicks(_timing.reset_us * 1000);
}

bool RmtBus::begin() {
    if (_initialized) return true;

    // Auto-select channel if needed
    if (_channel < 0) {
        // Use static counter for auto-allocation (fallback if caller didn't specify)
        if (s_nextAutoChannel >= getRmtMaxChannels()) {
            return false;
        }
        _channel = s_nextAutoChannel++;
    }

    if (_channel >= (int8_t)getRmtMaxChannels()) {
        Serial.printf("[WPB] RMT channel %d >= max %u, FAIL\n", _channel, getRmtMaxChannels());
        return false;
    }
    _rmtChannel = (rmt_channel_t)_channel;

    updateRmtTiming();

    rmt_config_t config = {};
    
    config.rmt_mode = RMT_MODE_TX;
    config.channel = _rmtChannel;
    config.gpio_num = (gpio_num_t)_pin;
    config.mem_block_num = 1;
    config.clk_div = 2;  // 40MHz

    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    //config.tx_config.carrier_freq_hz = 38000;
    //config.tx_config.carrier_duty_percent = 33;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = _inverted ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW;


    esp_err_t err = rmt_config(&config);
    if (err != ESP_OK) {
        return false;
    }

    // Prioritize RMT over I2S/SPI DMA interrupts (which use LEVEL1) to prevent starvation.
    // Try LEVEL3 first, fallback to LEVEL2, then LEVEL1.
    int flags = ESP_INTR_FLAG_IRAM;
#ifdef ESP_INTR_FLAG_LEVEL3
    err = rmt_driver_install(_rmtChannel, 0, flags | ESP_INTR_FLAG_LEVEL3);
    if (err != ESP_OK)
#endif
    {
#ifdef ESP_INTR_FLAG_LEVEL2
        err = rmt_driver_install(_rmtChannel, 0, flags | ESP_INTR_FLAG_LEVEL2);
        if (err != ESP_OK)
#endif
        {
            err = rmt_driver_install(_rmtChannel, 0, flags | ESP_INTR_FLAG_LEVEL1);
        }
    }
    if (err != ESP_OK) {
        return false;
    }

    err = rmt_translator_init(_rmtChannel, translateCB);
    if (err != ESP_OK) {
        rmt_driver_uninstall(_rmtChannel);
        return false;
    }

    _initialized = true;
    s_activeChannelMask |= (1 << _channel);
    return true;
}

void RmtBus::end() {
    if (!_initialized) return;

    s_activeChannelMask &= ~(1 << _channel);
    rmt_driver_uninstall(_rmtChannel);

    if (_encodeBuffer) {
        free(_encodeBuffer);
        _encodeBuffer = nullptr;
        _encodeBufferSize = 0;
    }

    _initialized = false;
}

bool RmtBus::allocateBuffer(uint16_t numPixels) {
    size_t needed = numPixels * getChannelCount(_order);
    if (_encodeBuffer && _encodeBufferSize >= needed) {
        return true;
    }

    if (_encodeBuffer) {
        free(_encodeBuffer);
    }

    _encodeBuffer = (uint8_t*)malloc(needed);
    if (!_encodeBuffer) {
        _encodeBufferSize = 0;
        return false;
    }
    _encodeBufferSize = needed;
    return true;
}

bool RmtBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!pixels) {
        pixels = _pixelData;
        numPixels = _numPixels;
        cct = _cctData;
    }
    if (numPixels == 0) numPixels = _numPixels;
    if (!cct) cct = _cctData;

    if (!_initialized || !pixels || numPixels == 0) {
      return false;
    }

    // Wait for previous transmission on THIS channel to complete
    rmt_wait_tx_done((rmt_channel_t)_rmtChannel, portMAX_DELAY);

    if (!allocateBuffer(numPixels)) return false;

    // Encode pixels to byte stream
    ColorEncoder encoder(_order);
    uint8_t* dst = _encodeBuffer;
    uint8_t numCh = encoder.getNumChannels();

    for (uint16_t i = 0; i < numPixels; i++) {
        encoder.encode(pixels[i], cct ?  &cct[i] : nullptr, dst);
        dst += numCh;
    }

    // Update context for ISR
    s_rmtCtx.bit0 = _rmtBit0;
    s_rmtCtx.bit1 = _rmtBit1;
    s_rmtCtx.resetDuration = _rmtResetTicks;

    // Start transmission
    size_t dataLen = numPixels * numCh;
    esp_err_t err = rmt_write_sample(_rmtChannel, _encodeBuffer, dataLen, false);

    return err == ESP_OK;
}

bool RmtBus::canShow() const {
    if (!_initialized) return true;
    // Use rmt_wait_tx_done with 0 timeout to check if TX is done (matching NeoPixelBus)
    return (ESP_OK == rmt_wait_tx_done(_rmtChannel, 0));
}

void RmtBus::waitComplete() {
    if (_initialized) {
        rmt_wait_tx_done(_rmtChannel, portMAX_DELAY);
    }
}

void RmtBus::setTiming(const LedTiming& timing) {
    _timing = timing;
    if (_initialized) {
        updateRmtTiming();
    }
}

void RmtBus::setColorOrder(ColorOrder order) {
    _order = order;
}

void IRAM_ATTR RmtBus::translateCB(const void* src, rmt_item32_t* dest,
                                    size_t src_size, size_t wanted_num,
                                    size_t* translated_size, size_t* item_num) {
    if (src == nullptr || dest == nullptr) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }

    const uint8_t* psrc = (const uint8_t*)src;
    rmt_item32_t* pdest = dest;
    size_t size = 0;
    size_t num = 0;

    uint32_t bit0 = s_rmtCtx.bit0;
    uint32_t bit1 = s_rmtCtx.bit1;
    uint16_t resetDuration = s_rmtCtx.resetDuration;

    // Each byte produces 8 RMT items
    for (;;)
    {
        uint8_t data = *psrc;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            pdest->val = (data & 0x80) ? bit1 : bit0;
            pdest++;
            data <<= 1;
        }
        num += 8;
        size++;

        // If this is the last byte, extend the last bit's LOW duration
        // to include the full reset signal length
        if (size >= src_size)
        {
            pdest--;
            pdest->duration1 = resetDuration;
            break;
        }

        if (num >= wanted_num)
        {
            break;
        }

        psrc++;
    }

    *translated_size = size;
    *item_num = num;
}

//==============================================================================
// I2S Bus Implementation
//==============================================================================

#ifdef WLEDPB_I2S_SUPPORT

I2sBusContext* I2sBusContext::_instances[WLEDPB_I2S_BUS_COUNT] = {nullptr};
uint8_t I2sBusContext::_refCount[WLEDPB_I2S_BUS_COUNT] = {0};

I2sBusContext* I2sBusContext::get(uint8_t busNum) {
    if (busNum >= WLEDPB_I2S_BUS_COUNT) return nullptr;

    if (_instances[busNum] == nullptr) {
        _instances[busNum] = new I2sBusContext(busNum);
    }
    _refCount[busNum]++;
    return _instances[busNum];
}

void I2sBusContext::release(uint8_t busNum) {
    if (busNum >= WLEDPB_I2S_BUS_COUNT) return;
    if (_refCount[busNum] == 0) return;

    _refCount[busNum]--;
    if (_refCount[busNum] == 0 && _instances[busNum]) {
        delete _instances[busNum];
        _instances[busNum] = nullptr;
    }
}

I2sBusContext::I2sBusContext(uint8_t busNum)
    : _busNum(busNum)
    , _state(DriverState::Idle)
    , _initialized(false)
    , _bufferSize(0)
    , _activeBuffer(0)
    , _timing{0, 0, 0, 0, 0}
    , _clockDiv(1)
    , _isrHandle(nullptr)
    , _channelCount(0)
    , _channelMask(0)
    , _stagedMask(0)
    , _maxDataLen(0)
{
#if defined(WLEDPB_ESP32)
    _i2sDev = (busNum == 0) ? &I2S0 : &I2S1;
#else
    _i2sDev = &I2S0;
#endif

    for (int i = 0; i < WLEDPB_I2S_MAX_CHANNELS; i++) {
        _channels[i] = {nullptr, -1, nullptr, 0, 0, false};
    }

    _dmaDesc[0] = _dmaDesc[1] = nullptr;
    _dmaBuffer[0] = _dmaBuffer[1] = nullptr;
}

I2sBusContext:: ~I2sBusContext() {
    deinit();
}

bool I2sBusContext::init(const LedTiming& timing, size_t bufferSize) {
    if (_initialized) return true;

    _timing = timing;
    _bufferSize = (bufferSize + 3) & ~3;  // align to 4 bytes

    // Allocate DMA buffers (4-byte aligned for DMA)
    for (int i = 0; i < 2; i++) {
        _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
        if (!_dmaBuffer[i]) {
            log_e("I2S DMA buffer alloc failed");
            deinit();
            return false;
        }
        memset(_dmaBuffer[i], 0, _bufferSize);

        _dmaDesc[i] = (lldesc_t*)heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
        if (!_dmaDesc[i]) {
            log_e("I2S DMA desc alloc failed");
            deinit();
            return false;
        }
    }

    // Setup DMA descriptors - circular chain for gapless ping-pong
    for (int i = 0; i < 2; i++) {
        _dmaDesc[i]->size = _bufferSize;
        _dmaDesc[i]->length = _bufferSize;
        _dmaDesc[i]->buf = _dmaBuffer[i];
        _dmaDesc[i]->eof = 1;  // Generate interrupt on completion
        _dmaDesc[i]->sosf = 0;
        _dmaDesc[i]->owner = 1;
    }
    _dmaDesc[0]->qe.stqe_next = _dmaDesc[1];
    _dmaDesc[1]->qe.stqe_next = _dmaDesc[0];

    // Enable I2S peripheral
#if defined(WLEDPB_ESP32)
    periph_module_enable(_busNum == 0 ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE);
#else
    periph_module_enable(PERIPH_I2S0_MODULE);
#endif

    // Stop any existing transmission
    _i2sDev->out_link.stop = 1;
    _i2sDev->conf.tx_start = 0;
    _i2sDev->int_ena.val = 0;
    _i2sDev->int_clr.val = 0xFFFFFFFF;
    _i2sDev->fifo_conf.dscr_en = 0;

    // Reset I2S
    _i2sDev->conf.tx_reset = 1;
    _i2sDev->conf.tx_reset = 0;
    _i2sDev->conf.rx_reset = 1;
    _i2sDev->conf.rx_reset = 0;

    // Reset DMA
    _i2sDev->lc_conf.in_rst = 1;
    _i2sDev->lc_conf.in_rst = 0;
    _i2sDev->lc_conf.out_rst = 1;
    _i2sDev->lc_conf.out_rst = 0;
    _i2sDev->lc_conf.ahbm_rst = 1;
    _i2sDev->lc_conf.ahbm_rst = 0;
    _i2sDev->lc_conf.ahbm_fifo_rst = 1;
    _i2sDev->lc_conf.ahbm_fifo_rst = 0;

    // Reset FIFO
    _i2sDev->conf.tx_fifo_reset = 1;
    _i2sDev->conf.tx_fifo_reset = 0;
    _i2sDev->conf.rx_fifo_reset = 1;
    _i2sDev->conf.rx_fifo_reset = 0;

    // Configure for 8-bit parallel LCD mode (matching NeoPixelBus)
    _i2sDev->conf2.val = 0;
    _i2sDev->conf2.lcd_en = 1;
    _i2sDev->conf2.lcd_tx_wrx2_en = 1;  // Required for 8-bit parallel output
    _i2sDev->conf2.lcd_tx_sdx2_en = 0;

    // DMA config
    _i2sDev->lc_conf.val = 0;
    _i2sDev->lc_conf.out_eof_mode = 1;

    // Disable PDM
#if defined(WLEDPB_ESP32)
    _i2sDev->pdm_conf.tx_pdm_en = 0;
    _i2sDev->pdm_conf.pcm2pdm_conv_en = 0;
#endif

    // FIFO configuration
    _i2sDev->fifo_conf.val = 0;
    _i2sDev->fifo_conf.tx_fifo_mod_force_en = 1;
    _i2sDev->fifo_conf.tx_fifo_mod = 1;  // 16-bit single channel
    _i2sDev->fifo_conf.tx_data_num = 32;  // FIFO threshold

    // PCM bypass
    _i2sDev->conf1.val = 0;
    _i2sDev->conf1.tx_stop_en = 0;
    _i2sDev->conf1.tx_pcm_bypass = 1;

    // Channel config
    _i2sDev->conf_chan.val = 0;
    _i2sDev->conf_chan.tx_chan_mod = 1;  // Right channel

    // I2S conf
    _i2sDev->conf.val = 0;
    _i2sDev->conf.tx_msb_shift = 0;  // No shift in parallel mode
    _i2sDev->conf.tx_right_first = 1;

    // Clear timing register
    _i2sDev->timing.val = 0;

    // Calculate clock divider for 4-step cadence (matching NeoPixelBus)
    // bck_div_num must be >= 2 on ESP32 hardware (NeoPixelBus uses 4)
    // step_time = clkm_div * bck_div / base_clock_MHz * 1000 ns
    // clkm_div = step_time_ns * base_clock_MHz / (bck_div * 1000)
    const uint8_t bckDiv = 4;  // must be >= 2, NeoPixelBus uses 4
    uint32_t bitPeriodNs = timing.bitPeriod();

#if defined(WLEDPB_ESP32)
    const double baseClockMhz = 160.0;
#else
    const double baseClockMhz = 80.0;
#endif

    // NeoPixelBus formula: clkmdiv = nsBitSendTime / bytesPerSample / dmaBitPerDataBit / bck / 1000 * baseClkMhz
    // For parallel 8-bit, bytesPerSample=1, dmaBitPerDataBit=4
    double clkmdiv = (double)bitPeriodNs / 1.0 / 4.0 / (double)bckDiv / 1000.0 * baseClockMhz;
    if (clkmdiv < 2.0) clkmdiv = 2.0;
    if (clkmdiv > 255.0) clkmdiv = 255.0;

    uint8_t clkmInteger = (uint8_t)clkmdiv;
    double clkmFraction = clkmdiv - clkmInteger;

    // Convert fraction to divB/divA (fraction = divB/divA)
    uint8_t divB = 0;
    uint8_t divA = 0;
    if (clkmFraction > 0.001) {
        divA = 63;  // use max denominator for best precision
        divB = (uint8_t)(clkmFraction * 63.0 + 0.5);
        if (divB >= divA) divB = divA - 1;
    }

    _clockDiv = clkmInteger;

    Serial.printf("[I2S] Clock: bitPeriod=%uns, clkm_div=%u+%u/%u, bck_div=%u\n",
                  bitPeriodNs, clkmInteger, divB, divA, bckDiv);
    double actualStepNs = (double)clkmInteger * bckDiv / baseClockMhz * 1000.0;
    if (divA > 0) actualStepNs = ((double)clkmInteger + (double)divB / divA) * bckDiv / baseClockMhz * 1000.0;
    Serial.printf("[I2S] Step time: %.1fns (target: %.1fns), bit period: %.1fns\n",
                  actualStepNs, (double)bitPeriodNs / 4.0, actualStepNs * 4.0);

    // Set clock (with fractional divider for accurate timing)
    _i2sDev->clkm_conf.val = 0;
    _i2sDev->clkm_conf.clk_en = 1;
    _i2sDev->clkm_conf.clkm_div_a = divA;
    _i2sDev->clkm_conf.clkm_div_b = divB;
    _i2sDev->clkm_conf.clkm_div_num = clkmInteger;

    // Sample rate - bck must be >= 2 (NeoPixelBus uses 4)
    _i2sDev->sample_rate_conf.val = 0;
    _i2sDev->sample_rate_conf.tx_bck_div_num = bckDiv;
    _i2sDev->sample_rate_conf.tx_bits_mod = 8;

    // Final reset before ISR install
    _i2sDev->lc_conf.in_rst = 1; _i2sDev->lc_conf.out_rst = 1;
    _i2sDev->lc_conf.ahbm_rst = 1; _i2sDev->lc_conf.ahbm_fifo_rst = 1;
    _i2sDev->lc_conf.in_rst = 0; _i2sDev->lc_conf.out_rst = 0;
    _i2sDev->lc_conf.ahbm_rst = 0; _i2sDev->lc_conf.ahbm_fifo_rst = 0;
    _i2sDev->conf.tx_reset = 1; _i2sDev->conf.tx_fifo_reset = 1;
    _i2sDev->conf.tx_reset = 0; _i2sDev->conf.tx_fifo_reset = 0;

    // Install ISR
    int intSource;
#if defined(WLEDPB_ESP32)
    intSource = (_busNum == 0) ? ETS_I2S0_INTR_SOURCE : ETS_I2S1_INTR_SOURCE;
#else
    intSource = ETS_I2S0_INTR_SOURCE;
#endif

    esp_err_t err = esp_intr_alloc(intSource, 
                                    ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1,
                                    dmaISR, this, &_isrHandle);
    if (err != ESP_OK) {
        log_e("I2S ISR alloc failed: %d", err);
        deinit();
        return false;
    }

    _initialized = true;
    Serial.printf("[I2S] Init complete: bus=%u, bufSize=%u\n", _busNum, _bufferSize);
    return true;
}

void I2sBusContext::deinit() {
    if (_i2sDev) {
        _i2sDev->int_ena.val = 0;      // Disable interrupts first
        _i2sDev->conf.tx_start = 0;
        _i2sDev->out_link.start = 0;
    }
    _state = DriverState::Idle;

    if (_isrHandle) {
        esp_intr_free(_isrHandle);
        _isrHandle = nullptr;
    }

    for (int i = 0; i < 2; i++) {
        if (_dmaBuffer[i]) {
            heap_caps_free(_dmaBuffer[i]);
            _dmaBuffer[i] = nullptr;
        }
        if (_dmaDesc[i]) {
            heap_caps_free(_dmaDesc[i]);
            _dmaDesc[i] = nullptr;
        }
    }

#if defined(WLEDPB_ESP32)
    periph_module_disable(_busNum == 0 ? PERIPH_I2S0_MODULE :  PERIPH_I2S1_MODULE);
#else
    periph_module_disable(PERIPH_I2S0_MODULE);
#endif

    _initialized = false;
}

int8_t I2sBusContext::registerChannel(int8_t pin, I2sBus* bus) {
    // Find free slot
    int8_t idx = -1;
    for (int i = 0; i < WLEDPB_I2S_MAX_CHANNELS; i++) {
        if (! _channels[i].active) {
            idx = i;
            break;
        }
    }

    if (idx < 0) return -1;

    _channels[idx].bus = bus;
    _channels[idx].pin = pin;
    _channels[idx].active = true;
    _channelCount++;
    _channelMask |= (1 << idx);

    // Configure GPIO
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);

    // Route I2S output to GPIO
    int sigIdx;
#if defined(WLEDPB_ESP32)
    sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT0_IDX : I2S1O_DATA_OUT0_IDX;
#else
    sigIdx = I2S0O_DATA_OUT0_IDX;
#endif
    sigIdx += idx;

    gpio_matrix_out(pin, sigIdx, false, false);

    return idx;
}

void I2sBusContext::unregisterChannel(int8_t channelIdx) {
    if (channelIdx < 0 || channelIdx >= WLEDPB_I2S_MAX_CHANNELS) return;
    if (!_channels[channelIdx].active) return;

    if (_channels[channelIdx].pin >= 0) {
        gpio_reset_pin((gpio_num_t)_channels[channelIdx].pin);
    }

    _channels[channelIdx] = {nullptr, -1, nullptr, 0, 0, false};
    _channelCount--;
    _channelMask &= ~(1 << channelIdx);
}

void I2sBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
    if (channelIdx < 0 || channelIdx >= WLEDPB_I2S_MAX_CHANNELS) return;

    _channels[channelIdx].srcData = data;
    _channels[channelIdx].srcLen = len;
    _channels[channelIdx].srcPos = 0;

    if (len > _maxDataLen) {
        _maxDataLen = len;
    }

    // Safety: If this channel was already staged, it means we somehow missed triggering startTransmit()
    if (_stagedMask & (1 << channelIdx)) {
        _stagedMask = 0; 
    }
    _stagedMask |= (1 << channelIdx);
}

void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen) {
    // 4-step cadence encoding for parallel output
    // Each source bit becomes 4 DMA bytes (one bit per channel in each byte)
    // Desired output: [HIGH][data][data][LOW] for each bit
    //
    // ESP32 I2S LCD mode with lcd_tx_wrx2_en=1 swaps half-words within 32-bit values:
    //   Memory [b0,b1,b2,b3] outputs as [b2,b3,b0,b1]
    //   (NeoPixelBus documents this as "bytes within the words are swapped")
    //   So we write: p[0]=step2, p[1]=step3, p[2]=step0, p[3]=step1
    //
    // Buffer is always filled completely (zeros = LOW = reset signal)

    memset(dest, 0, destLen);
    size_t pos = 0;

    // Process each source byte position across all channels
    while (pos + 32 <= destLen) {  // 8 bits * 4 steps = 32 bytes per source byte
        bool hasData = false;

        for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
            if (!_channels[ch].active) continue;
            if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;

            hasData = true;
            uint8_t srcByte = _channels[ch].srcData[_channels[ch].srcPos];
            uint8_t chMask = (1 << ch);

            uint8_t* p = dest + pos;
            for (int bit = 7; bit >= 0; bit--) {
                uint8_t dataVal = (srcByte >> bit) & 1;

                // Half-word swapped: memory layout [step2, step3, step0, step1]
                // Step 0 (HIGH) -> p[2]
                p[2] |= chMask;

                // Step 1 (data) -> p[3]
                if (dataVal) p[3] |= chMask;

                // Step 2 (data) -> p[0]
                if (dataVal) p[0] |= chMask;

                // Step 3 (LOW)  -> p[1] (already 0)

                p += 4;
            }
        }

        if (!hasData) break;

        // Advance all channel positions
        for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
            if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
                _channels[ch].srcPos++;
            }
        }

        pos += 32;
    }
    // Rest of buffer remains zero (reset signal) from memset
}

void I2sBusContext::fillBuffer(uint8_t bufIdx) {
    encode4Step(_dmaBuffer[bufIdx], _bufferSize);
    // desc->length stays at _bufferSize (set in init, never changes)
}

bool I2sBusContext::startTransmit() {
    if (_state != DriverState::Idle) return false;
    if (_channelCount == 0) return false;

    // Only start transmission if ALL active channels have populated data
    if (_stagedMask != _channelMask) return false;
    _stagedMask = 0; // Reset for next frame

    _maxDataLen = 0;
    for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
        if (_channels[ch].active) {
            _channels[ch].srcPos = 0;
            if (_channels[ch].srcLen > _maxDataLen) {
                _maxDataLen = _channels[ch].srcLen;
            }
        }
    }

    // Fill both buffers initially
    fillBuffer(0);
    fillBuffer(1);

    // Restore DMA descriptor ownership (DMA clears owner to 0 after processing)
    _dmaDesc[0]->owner = 1;
    _dmaDesc[1]->owner = 1;

    _activeBuffer = 0;
    _state = DriverState::Sending;

    // Reset DMA and FIFO before starting
    _i2sDev->lc_conf.in_rst = 1; _i2sDev->lc_conf.out_rst = 1;
    _i2sDev->lc_conf.ahbm_rst = 1; _i2sDev->lc_conf.ahbm_fifo_rst = 1;
    _i2sDev->lc_conf.in_rst = 0; _i2sDev->lc_conf.out_rst = 0;
    _i2sDev->lc_conf.ahbm_rst = 0; _i2sDev->lc_conf.ahbm_fifo_rst = 0;
    _i2sDev->conf.tx_reset = 1; _i2sDev->conf.tx_fifo_reset = 1;
    _i2sDev->conf.tx_reset = 0; _i2sDev->conf.tx_fifo_reset = 0;

    // Clear and enable interrupts
    _i2sDev->int_clr.val = 0xFFFFFFFF;
    _i2sDev->int_ena.out_eof = 1;

    // Enable DMA and start
    _i2sDev->fifo_conf.dscr_en = 1;
    _i2sDev->out_link.start = 0;
    _i2sDev->out_link.addr = (uint32_t)_dmaDesc[0];
    _i2sDev->out_link.start = 1;
    _i2sDev->conf.tx_start = 1;

    return true;
}

static volatile uint32_t s_i2sIsrCount = 0;
static volatile uint32_t s_i2sIsrSending = 0;
static volatile uint32_t s_i2sIsrReset = 0;
static volatile uint32_t s_i2sIsrIdle = 0;

void IRAM_ATTR I2sBusContext::dmaISR(void* arg) {
    I2sBusContext* ctx = (I2sBusContext*)arg;
    i2s_dev_t* dev = ctx->_i2sDev;

    uint32_t status = dev->int_st.val;
    dev->int_clr.val = status;

    if (!(status & I2S_OUT_EOF_INT_ST)) return;

    s_i2sIsrCount++;

    // The completed buffer just finished playing; DMA is now on the other buffer
    uint8_t completedBuf = ctx->_activeBuffer;
    ctx->_activeBuffer ^= 1;

    if (ctx->_state == DriverState::Sending) {
        s_i2sIsrSending++;
        // Encode next chunk into the completed buffer
        // encode4Step always fills the full buffer (zeros for any remainder)
        ctx->encode4Step(ctx->_dmaBuffer[completedBuf], ctx->_bufferSize);

        // Check if all source data has been consumed
        bool moreData = false;
        for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
            if (ctx->_channels[ch].active &&
                ctx->_channels[ch].srcPos < ctx->_channels[ch].srcLen) {
                moreData = true;
                break;
            }
        }
        if (!moreData) {
            // Last data chunk was just encoded into completedBuf.
            // DMA is currently playing the OTHER buffer. We need to wait
            // for that to finish so the last-data buffer gets played.
            ctx->_state = DriverState::SendingLast;
        }

        // Restore DMA ownership so hardware can replay this buffer
        ctx->_dmaDesc[completedBuf]->owner = 1;
    } else if (ctx->_state == DriverState::SendingLast) {
        // The other buffer just finished. DMA is now playing the last-data buffer.
        // Fill completed buffer with zeros (reset signal) so it plays after.
        memset(ctx->_dmaBuffer[completedBuf], 0, ctx->_bufferSize);
        ctx->_dmaDesc[completedBuf]->owner = 1;
        s_i2sIsrReset++;
        ctx->_state = DriverState::WaitingReset;
    } else {
        // WaitingReset - last data played, zero buffer sent as reset. Stop DMA.
        s_i2sIsrIdle++;
        dev->int_ena.out_eof = 0;
        dev->conf.tx_start = 0;
        dev->out_link.start = 0;
        ctx->_state = DriverState::Idle;
    }
}

// I2sBus implementation

I2sBus::I2sBus(int8_t pin, const LedTiming& timing, ColorOrder order,
               uint8_t busNum, size_t bufferSize)
    : _pin(pin)
    , _busNum(busNum)
    , _bufferSize(bufferSize)
    , _timing(timing)
    , _order(order)
    , _initialized(false)
    , _channelIdx(-1)
    , _ctx(nullptr)
    , _encodeBuffer(nullptr)
    , _encodeBufferSize(0)
{
}

I2sBus::~I2sBus() {
    end();
}

bool I2sBus::begin() {
    if (_initialized) return true;

    _ctx = I2sBusContext::get(_busNum);
    if (!_ctx) return false;

    if (!_ctx->init(_timing, _bufferSize)) {
        I2sBusContext::release(_busNum);
        _ctx = nullptr;
        return false;
    }

    _channelIdx = _ctx->registerChannel(_pin, this);
    if (_channelIdx < 0) {
        Serial.printf("[I2S] registerChannel failed for pin %d\n", _pin);
        I2sBusContext::release(_busNum);
        _ctx = nullptr;
        return false;
    }

    _initialized = true;
    Serial.printf("[I2S] I2sBus::begin() OK: pin=%d, bus=%u, channel=%d\n", _pin, _busNum, _channelIdx);
    return true;
}

void I2sBus::end() {
    if (!_initialized) return;

    if (_ctx) {
        // Wait for any active transmission to complete before cleanup
        while (!_ctx->isIdle()) vTaskDelay(1);
        _ctx->unregisterChannel(_channelIdx);
        I2sBusContext::release(_busNum);
        _ctx = nullptr;
    }

    if (_encodeBuffer) {
        heap_caps_free(_encodeBuffer);
        _encodeBuffer = nullptr;
        _encodeBufferSize = 0;
    }

    _initialized = false;
}

bool I2sBus::allocateBuffer(uint16_t numPixels) {
    size_t needed = numPixels * getChannelCount(_order);
    if (_encodeBuffer && _encodeBufferSize >= needed) {
        return true;
    }

    if (_encodeBuffer) {
        heap_caps_free(_encodeBuffer);
    }

    _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_DMA);
    if (!_encodeBuffer) {
        _encodeBufferSize = 0;
        return false;
    }
    _encodeBufferSize = needed;
    return true;
}

bool I2sBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    // Always use internal pixel buffer (WLED always calls show() without args)
    if (!_initialized || !_ctx || !_pixelData || _numPixels == 0) return false;

    // Wait for previous transmission to complete
    while (!_ctx->isIdle()) {
        vTaskDelay(1);
    }

    if (!allocateBuffer(_numPixels)) return false;

    // Encode pixels to byte stream
    ColorEncoder encoder(_order);
    uint8_t* dst = _encodeBuffer;
    uint8_t numCh = encoder.getNumChannels();

    for (uint16_t i = 0; i < _numPixels; i++) {
        encoder.encode(_pixelData[i], _cctData ? &_cctData[i] : nullptr, dst);
        dst += numCh;
    }

    // Set data for our channel and start
    _ctx->setChannelData(_channelIdx, _encodeBuffer, _numPixels * numCh);
    return _ctx->startTransmit();
}

bool I2sBus::canShow() const {
    if (!_ctx) return true;
    return _ctx->isIdle();
}

void I2sBus::waitComplete() {
    while (_ctx && !_ctx->isIdle()) {
        vTaskDelay(1);
    }
}

void I2sBus::setColorOrder(ColorOrder order) {
    _order = order;
}

#endif // WLEDPB_I2S_SUPPORT

//==============================================================================
// LCD Bus Implementation (ESP32-S3)
//==============================================================================
/*-------------------------------------------------------------------------
WLEDpixelBus - LCD Implementation with Continuous DMA (Glitch-Free)

Key design:
- DMA runs continuously in circular mode (buf0 -> buf1 -> buf0 -> ...)
- Buffers are filled with data or zeros (for reset period)
- Stop only after reset period completes on buffer boundary
- No DMA reconfiguration during transmission
-------------------------------------------------------------------------*/

#ifdef WLEDPB_LCD_SUPPORT

#include "esp_private/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
#include "hal/dma_types.h"
#include "hal/gpio_hal.h"
#include "hal/lcd_ll.h"
#include "soc/lcd_cam_struct.h"
#include "soc/gpio_sig_map.h"

// ============================================
// Configuration
// ============================================

#ifndef WLEDPB_LCD_DMA_BUFFER_SIZE
#define WLEDPB_LCD_DMA_BUFFER_SIZE 512
#endif

#ifndef WLEDPB_LCD_CADENCE_STEPS
#define WLEDPB_LCD_CADENCE_STEPS 4
#endif

#ifndef WLEDPB_LCD_DEBUG
#define WLEDPB_LCD_DEBUG 1
#endif

#if WLEDPB_LCD_DEBUG
    #define LCD_LOG(fmt, ...) Serial.printf("[LCD] " fmt "\n", ##__VA_ARGS__)
#else
    #define LCD_LOG(fmt, ...)
#endif

static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE >= 64, "DMA buffer too small");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE <= 4092, "DMA buffer too large");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE % 4 == 0, "DMA buffer must be multiple of 4");
static_assert(WLEDPB_LCD_CADENCE_STEPS == 3 || WLEDPB_LCD_CADENCE_STEPS == 4, "Cadence must be 3 or 4");


// Debug counters
static volatile uint32_t s_isrCount = 0;
static volatile uint32_t s_dataFills = 0;
static volatile uint32_t s_zeroFills = 0;

LcdBusContext* LcdBusContext::_instance = nullptr;
uint8_t LcdBusContext::_refCount = 0;

LcdBusContext* LcdBusContext::get() {
    if (_instance == nullptr) {
        _instance = new LcdBusContext();
    }
    _refCount++;
    return _instance;
}

void LcdBusContext::release() {
    if (_refCount == 0) return;
    _refCount--;
    if (_refCount == 0 && _instance) {
        delete _instance;
        _instance = nullptr;
    }
}

LcdBusContext::LcdBusContext()
    : _state(DriverState::Idle)
    , _txState(TxState::Idle)
    , _initialized(false)
    , _use16Bit(false)
    , _dmaChannel(nullptr)
    , _timing{0, 0, 0, 0, 0}
    , _channelCount(0)
    , _channelMask(0)
    , _stagedMask(0)
    , _maxDataLen(0)
    , _activeBuffer(0)
    , _resetBytesRemaining(0)
    , _resetBytesPerBuffer(0)
{
    for (int i = 0; i < WLEDPB_LCD_MAX_CHANNELS; i++) {
        _channels[i] = {nullptr, -1, nullptr, 0, 0, false};
    }
    for (int i = 0; i < 2; i++) {
        _dmaDesc[i] = nullptr;
        _dmaBuffer[i] = nullptr;
    }
}

LcdBusContext::~LcdBusContext() {
    deinit();
}

bool LcdBusContext::init(const LedTiming& timing, size_t bufferSize, bool use16Bit) {
    if (_initialized) {
        LCD_LOG("Already initialized");
        return true;
    }

    LCD_LOG("Initializing LCD bus context (continuous DMA mode)");
    LCD_LOG("  DMA buffer size: %u bytes x2", WLEDPB_LCD_DMA_BUFFER_SIZE);
    LCD_LOG("  Cadence:  %d-step", WLEDPB_LCD_CADENCE_STEPS);

    _timing = timing;
    _use16Bit = use16Bit;

    // Calculate reset bytes needed
    // Reset time in us -> bytes at our clock rate
    // Clock period = bitPeriod / cadenceSteps
    // Bytes per us = 1000 / clockPeriodNs
    uint32_t bitPeriodNs = timing.bitPeriod();
    uint32_t clockPeriodNs = bitPeriodNs / WLEDPB_LCD_CADENCE_STEPS;
    uint32_t bytesPerUs = 1000 / clockPeriodNs;
    _resetBytesPerBuffer = timing.reset_us * bytesPerUs;
    
    // Round up to buffer size for clean stopping
    if (_resetBytesPerBuffer < WLEDPB_LCD_DMA_BUFFER_SIZE) {
        _resetBytesPerBuffer = WLEDPB_LCD_DMA_BUFFER_SIZE;
    }
    
    LCD_LOG("  Reset time: %u us = %u bytes minimum", timing.reset_us, _resetBytesPerBuffer);

    // Allocate double DMA buffers
    for (int i = 0; i < 2; i++) {
        _dmaBuffer[i] = (uint8_t*)heap_caps_malloc(WLEDPB_LCD_DMA_BUFFER_SIZE, MALLOC_CAP_DMA);
        if (!_dmaBuffer[i]) {
            LCD_LOG("ERROR: DMA buffer %d alloc failed", i);
            deinit();
            return false;
        }
        memset(_dmaBuffer[i], 0, WLEDPB_LCD_DMA_BUFFER_SIZE);

        _dmaDesc[i] = (dma_descriptor_t*)heap_caps_malloc(sizeof(dma_descriptor_t), MALLOC_CAP_DMA);
        if (!_dmaDesc[i]) {
            LCD_LOG("ERROR: DMA desc %d alloc failed", i);
            deinit();
            return false;
        }
        memset(_dmaDesc[i], 0, sizeof(dma_descriptor_t));
    }

    // Setup DMA descriptors in CIRCULAR mode (never changes during operation!)
    // buf0 -> buf1 -> buf0 -> buf1 -> ...
    for (int i = 0; i < 2; i++) {
        _dmaDesc[i]->dw0.size = WLEDPB_LCD_DMA_BUFFER_SIZE;
        _dmaDesc[i]->dw0.length = WLEDPB_LCD_DMA_BUFFER_SIZE;
        _dmaDesc[i]->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        _dmaDesc[i]->dw0.suc_eof = 1;  // Always trigger EOF for ISR
        _dmaDesc[i]->buffer = _dmaBuffer[i];
        _dmaDesc[i]->next = _dmaDesc[i ^ 1];  // Point to other buffer (circular)
    }

    LCD_LOG("  DMA descriptors configured in circular mode");

    // Enable LCD_CAM peripheral
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Reset LCD
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(100);
    LCD_CAM.lcd_user.lcd_reset = 0;
    esp_rom_delay_us(100);

    // Calculate clock divider
    double clkm_div = (double)bitPeriodNs / WLEDPB_LCD_CADENCE_STEPS / 1000.0 * 240.0;
    
    LCD_LOG("  Bit period: %u ns, clock div: %.2f", bitPeriodNs, clkm_div);

    if (clkm_div > LCD_LL_CLK_FRAC_DIV_N_MAX || clkm_div < 2.0) {
        LCD_LOG("ERROR: Invalid clock divider");
        deinit();
        return false;
    }

    uint8_t clkm_div_int = (uint8_t)clkm_div;
    double clkm_frac = clkm_div - clkm_div_int;
    
    uint8_t divB = 0;
    uint8_t divA = 0;
    if (clkm_frac > 0.001) {
        divA = 63;
        divB = (uint8_t)(clkm_frac * 63.0 + 0.5);
        if (divB >= divA) divB = divA - 1;
    }

    // Configure LCD clock
    LCD_CAM.lcd_clock.clk_en = 1;
    LCD_CAM.lcd_clock.lcd_clk_sel = 2;
    LCD_CAM.lcd_clock.lcd_clkm_div_a = divA;
    LCD_CAM.lcd_clock.lcd_clkm_div_b = divB;
    LCD_CAM.lcd_clock.lcd_clkm_div_num = clkm_div_int;
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;

    // Configure frame format
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0;
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;
    LCD_CAM.lcd_data_dout_mode.val = 0;
    LCD_CAM.lcd_user.lcd_always_out_en = 1;
    LCD_CAM.lcd_user.lcd_8bits_order = 0;
    LCD_CAM.lcd_user.lcd_bit_order = 0;
    LCD_CAM.lcd_user.lcd_2byte_en = use16Bit ?  1 : 0;
    LCD_CAM.lcd_user.lcd_dummy = 1;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;
    LCD_CAM.lcd_user.lcd_cmd = 0;

    // Allocate GDMA channel
    gdma_channel_alloc_config_t dma_chan_config = {
        .sibling_chan = NULL,
        .direction = GDMA_CHANNEL_DIRECTION_TX,
        .flags = {.reserve_sibling = 0}
    };
    
    esp_err_t err = gdma_new_channel(&dma_chan_config, &_dmaChannel);
    if (err != ESP_OK) {
        LCD_LOG("ERROR:  GDMA alloc failed:  %d", err);
        deinit();
        return false;
    }

    err = gdma_connect(_dmaChannel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    if (err != ESP_OK) {
        LCD_LOG("ERROR: GDMA connect failed: %d", err);
        deinit();
        return false;
    }

    gdma_strategy_config_t strategy_config = {
        .owner_check = false,
        .auto_update_desc = false
    };
    gdma_apply_strategy(_dmaChannel, &strategy_config);

    // Register DMA callback
    gdma_tx_event_callbacks_t tx_cbs = {.on_trans_eof = dmaCallback};
    gdma_register_tx_event_callbacks(_dmaChannel, &tx_cbs, this);

    _initialized = true;
    LCD_LOG("LCD bus initialized (continuous circular DMA)");
    
    return true;
}

void LcdBusContext::deinit() {
    LCD_LOG("Deinitializing LCD");
    
    // Stop transmission
    LCD_CAM.lcd_user.lcd_start = 0;
    _state = DriverState::Idle;
    _txState = TxState::Idle;

    if (_dmaChannel) {
        gdma_stop(_dmaChannel);
        gdma_disconnect(_dmaChannel);
        gdma_del_channel(_dmaChannel);
        _dmaChannel = nullptr;
    }

    for (int i = 0; i < 2; i++) {
        if (_dmaBuffer[i]) {
            heap_caps_free(_dmaBuffer[i]);
            _dmaBuffer[i] = nullptr;
        }
        if (_dmaDesc[i]) {
            heap_caps_free(_dmaDesc[i]);
            _dmaDesc[i] = nullptr;
        }
    }

    if (_initialized) {
        periph_module_disable(PERIPH_LCD_CAM_MODULE);
    }
    
    _initialized = false;
}

int8_t LcdBusContext:: registerChannel(int8_t pin, LcdBus* bus) {
    LCD_LOG("Registering channel:  pin=%d", pin);
    
    int8_t idx = -1;
    for (int i = 0; i < WLEDPB_LCD_MAX_CHANNELS; i++) {
        if (! _channels[i].active) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        LCD_LOG("ERROR: No free channels");
        return -1;
    }

    _channels[idx].bus = bus;
    _channels[idx].pin = pin;
    _channels[idx].active = true;
    _channelCount++;
    _channelMask |= (1 << idx);

    uint8_t muxIdx = LCD_DATA_OUT0_IDX + idx;
    esp_rom_gpio_connect_out_signal(pin, muxIdx, false, false);
    gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
    gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_3);

    LCD_LOG("  Channel %d registered:  pin=%d", idx, pin);
    return idx;
}

void LcdBusContext::unregisterChannel(int8_t channelIdx) {
    if (channelIdx < 0 || channelIdx >= WLEDPB_LCD_MAX_CHANNELS) return;
    if (!_channels[channelIdx].active) return;

    if (_channels[channelIdx].pin >= 0) {
        gpio_matrix_out(_channels[channelIdx].pin, SIG_GPIO_OUT_IDX, false, false);
        pinMode(_channels[channelIdx].pin, INPUT);
    }

    _channels[channelIdx] = {nullptr, -1, nullptr, 0, 0, false};
    _channelCount--;
    _channelMask &= ~(1 << channelIdx);
}

void LcdBusContext:: setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
    if (channelIdx < 0 || channelIdx >= WLEDPB_LCD_MAX_CHANNELS) return;

    _channels[channelIdx].srcData = data;
    _channels[channelIdx].srcLen = len;
    _channels[channelIdx].srcPos = 0;

    if (len > _maxDataLen) {
        _maxDataLen = len;
    }

    if (_stagedMask & (1 << channelIdx)) {
        _stagedMask = 0; 
    }
    _stagedMask |= (1 << channelIdx);
}

size_t IRAM_ATTR LcdBusContext:: encodeIntoBuffer(uint8_t* dest, size_t destLen) {
    // Encode pixel data from all channels
    // Returns number of bytes written (0 if no more data)
    
    size_t written = 0;
    const size_t bytesPerSourceByte = 8 * WLEDPB_LCD_CADENCE_STEPS;
    
    // Clear buffer (important for OR operations and zero-fill)
    memset(dest, 0, destLen);
    
    while (written + bytesPerSourceByte <= destLen) {
        // Check if any channel has data
        bool hasData = false;
        for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
            if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
                hasData = true;
                break;
            }
        }
        
        if (! hasData) break;
        
        // Encode one byte from each active channel
        for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
            if (! _channels[ch].active) continue;
            if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;
            
            uint8_t srcByte = _channels[ch].srcData[_channels[ch].srcPos];
            uint8_t chMask = (1 << ch);
            uint8_t* p = dest + written;
            
            for (int bit = 7; bit >= 0; bit--) {
                uint8_t dataVal = (srcByte >> bit) & 1;
                
#if WLEDPB_LCD_CADENCE_STEPS == 4
                p[0] |= chMask;
                if (dataVal) {
                    p[1] |= chMask;
                    p[2] |= chMask;
                }
                p += 4;
#else
                p[0] |= chMask;
                if (dataVal) {
                    p[1] |= chMask;
                }
                p += 3;
#endif
            }
        }
        
        // Advance all channels
        for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
            if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
                _channels[ch].srcPos++;
            }
        }
        
        written += bytesPerSourceByte;
    }
    
    return written;
}

bool IRAM_ATTR LcdBusContext::hasMoreData() const {
    for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
        if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
            return true;
        }
    }
    return false;
}

void IRAM_ATTR LcdBusContext::fillBufferForState(uint8_t bufIdx) {
    // Fill buffer based on current transmission state
    // This is called from ISR context! 
    
    switch (_txState) {
        case TxState::SendingData:  {
            size_t encoded = encodeIntoBuffer(_dmaBuffer[bufIdx], WLEDPB_LCD_DMA_BUFFER_SIZE);
            
            if (encoded > 0) {
                s_dataFills++;
                // Still have data, continue
                if (! hasMoreData()) {
                    // This was the last data, switch to reset phase
                    _txState = TxState::SendingReset;
                    _resetBytesRemaining = _resetBytesPerBuffer;
                }
            } else {
                // No data encoded, switch to reset
                _txState = TxState::SendingReset;
                _resetBytesRemaining = _resetBytesPerBuffer;
                s_zeroFills++;
                // Buffer already zeroed by encodeIntoBuffer
            }
            break;
        }
        
        case TxState:: SendingReset: {
            // Fill with zeros for reset period
            memset(_dmaBuffer[bufIdx], 0, WLEDPB_LCD_DMA_BUFFER_SIZE);
            s_zeroFills++;
            
            if (_resetBytesRemaining <= WLEDPB_LCD_DMA_BUFFER_SIZE) {
                // This is the last reset buffer, stop after it completes
                _txState = TxState::StopPending;
                _resetBytesRemaining = 0;
            } else {
                _resetBytesRemaining -= WLEDPB_LCD_DMA_BUFFER_SIZE;
            }
            break;
        }
        
        case TxState::StopPending: {
            // Fill with zeros but we'll stop after this
            memset(_dmaBuffer[bufIdx], 0, WLEDPB_LCD_DMA_BUFFER_SIZE);
            s_zeroFills++;
            break;
        }
        
        default: 
            break;
    }
}

bool LcdBusContext::startTransmit() {
    if (_stagedMask != _channelMask) {
        return false; // wait for all channels (from multiple buses)
    }
    
    _stagedMask = 0; // ready for next frame once we transmit

    if (_state != DriverState::Idle) {
        LCD_LOG("ERROR:  Not idle");
        return false;
    }
    
    if (_channelCount == 0) {
        LCD_LOG("ERROR: No channels");
        return false;
    }

    // Reset channel positions
    _maxDataLen = 0;
    for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
        if (_channels[ch].active) {
            _channels[ch].srcPos = 0;
            if (_channels[ch].srcLen > _maxDataLen) {
                _maxDataLen = _channels[ch].srcLen;
            }
        }
    }

    if (_maxDataLen == 0) {
        LCD_LOG("ERROR: No data");
        return false;
    }

    // Reset debug counters
    s_isrCount = 0;
    s_dataFills = 0;
    s_zeroFills = 0;

    LCD_LOG("Starting transmission:  %u bytes", _maxDataLen);

    // Set initial state
    _txState = TxState::SendingData;
    _activeBuffer = 0;
    _resetBytesRemaining = 0;

    // Pre-fill both buffers with data
    // Buffer 0: First chunk
    size_t len0 = encodeIntoBuffer(_dmaBuffer[0], WLEDPB_LCD_DMA_BUFFER_SIZE);
    s_dataFills++;
    
    if (! hasMoreData()) {
        // All data fits in buffer 0, prepare for reset
        _txState = TxState::SendingReset;
        _resetBytesRemaining = _resetBytesPerBuffer;
    }
    
    // Buffer 1: Second chunk or start of reset
    fillBufferForState(1);

    _state = DriverState::Sending;

    // Start DMA (circular mode - descriptors already linked)
    // DO NOT call gdma_reset here - just start fresh
    gdma_reset(_dmaChannel);
    
    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;

    gdma_start(_dmaChannel, (intptr_t)_dmaDesc[0]);
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_start = 1;

    LCD_LOG("  DMA started in circular mode");

    return true;
}

IRAM_ATTR bool LcdBusContext:: dmaCallback(gdma_channel_handle_t dma_chan,
                                          gdma_event_data_t* event_data,
                                          void* user_data) {
    LcdBusContext* ctx = (LcdBusContext*)user_data;
    
    s_isrCount++;
    
    // The buffer that just completed is the active one
    // We need to refill it while the other buffer is being sent
    uint8_t completedBuf = ctx->_activeBuffer;
    ctx->_activeBuffer ^= 1;  // Switch to other buffer
    
    if (ctx->_txState == TxState::StopPending) {
        // Stop transmission cleanly
        LCD_CAM.lcd_user.lcd_start = 0;
        ctx->_state = DriverState::Idle;
        ctx->_txState = TxState::Idle;
        return true;
    }
    
    // Refill the completed buffer for next round
    ctx->fillBufferForState(completedBuf);
    
    // Ensure DMA descriptor is ready (owner = DMA)
    ctx->_dmaDesc[completedBuf]->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    
    return true;
}

void LcdBusContext::printDebugStats() {
    LCD_LOG("Stats: ISR=%u, dataFills=%u, zeroFills=%u", 
            s_isrCount, s_dataFills, s_zeroFills);
}

// ============================================
// LcdBus Implementation
// ============================================

LcdBus::LcdBus(int8_t pin, const LedTiming& timing, ColorOrder order,
               size_t bufferSize, bool use16Bit)
    : _pin(pin)
    , _bufferSize(bufferSize)
    , _use16Bit(use16Bit)
    , _timing(timing)
    , _order(order)
    , _initialized(false)
    , _channelIdx(-1)
    , _ctx(nullptr)
    , _encodeBuffer(nullptr)
    , _encodeBufferSize(0)
    , _encodedLen(0)
{
}

LcdBus::~LcdBus() {
    end();
}

bool LcdBus::begin() {
    if (_initialized) return true;

    LCD_LOG("LcdBus::begin() pin=%d", _pin);

    _ctx = LcdBusContext::get();
    if (!_ctx) return false;

    if (!_ctx->init(_timing, _bufferSize, _use16Bit)) {
        LcdBusContext::release();
        _ctx = nullptr;
        return false;
    }

    _channelIdx = _ctx->registerChannel(_pin, this);
    if (_channelIdx < 0) {
        LcdBusContext::release();
        _ctx = nullptr;
        return false;
    }

    _initialized = true;
    LCD_LOG("LcdBus ready:  channel=%d", _channelIdx);
    return true;
}

void LcdBus::end() {
    if (!_initialized) return;

    if (_ctx) {
        _ctx->unregisterChannel(_channelIdx);
        LcdBusContext::release();
        _ctx = nullptr;
    }

    if (_encodeBuffer) {
        free(_encodeBuffer);
        _encodeBuffer = nullptr;
        _encodeBufferSize = 0;
    }

    _initialized = false;
}

bool LcdBus::allocateBuffer(uint16_t numPixels) {
    size_t needed = numPixels * getChannelCount(_order);
    if (_encodeBuffer && _encodeBufferSize >= needed) {
        return true;
    }

    if (_encodeBuffer) free(_encodeBuffer);

    _encodeBuffer = (uint8_t*)malloc(needed);
    if (!_encodeBuffer) {
        _encodeBufferSize = 0;
        return false;
    }
    _encodeBufferSize = needed;
    return true;
}

bool LcdBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!_initialized || !_ctx || !pixels || numPixels == 0) {
        return false;
    }

    // Wait for idle
    uint32_t timeout = 1000;  // Longer timeout for large strips
    uint32_t start = millis();
    while (! _ctx->isIdle()) {
        if (millis() - start > timeout) {
            LCD_LOG("ERROR:  Timeout waiting for idle");
            _ctx->forceIdle();
            break;
        }
        delay(1);
    }

    if (!allocateBuffer(numPixels)) return false;

    // Encode pixels to byte stream
    ColorEncoder encoder(_order);
    uint8_t* dst = _encodeBuffer;
    uint8_t numCh = encoder.getNumChannels();

    for (uint16_t i = 0; i < numPixels; i++) {
        encoder.encode(pixels[i], cct ?  &cct[i] : nullptr, dst);
        dst += numCh;
    }

    _encodedLen = numPixels * numCh;

    // Set data for our channel
    _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodedLen);

    // Start transmission
    return _ctx->startTransmit();
}

bool LcdBus::canShow() const {
    if (!_ctx) return true;
    return _ctx->isIdle();
}

void LcdBus::waitComplete() {
    if (!_ctx) return;
    
    uint32_t start = millis();
    while (!_ctx->isIdle()) {
        if (millis() - start > 1000) {
            LCD_LOG("waitComplete timeout");
            _ctx->printDebugStats();
            _ctx->forceIdle();
            return;
        }
        delay(1);
    }
    
#if WLEDPB_LCD_DEBUG
    _ctx->printDebugStats();
#endif
}

void LcdBus::setColorOrder(ColorOrder order) {
    _order = order;
}

#endif // WLEDPB_LCD_SUPPORT

//==============================================================================
// SPI Parallel Bus Implementation (ESP32-C3)
//==============================================================================

#ifdef WLEDPB_SPI_SUPPORT

// low level functions available in IDF V5 (not available in IDF V4, this is for future proofint)
static inline void spi_ll_apply_config(spi_dev_t *hw)
{
    hw->cmd.update = 1;
    while (hw->cmd.update);    //waiting config applied
}

static inline void spi_ll_user_start(spi_dev_t *hw)
{
    hw->cmd.usr = 1;
}


// Pin assignments for SPI2 quad mode on C3
// SPI2 signals: FSPID (MOSI/D0), FSPIQ (MISO/D1), FSPIWP (D2), FSPIHD (D3)
static const int SPI_SIGNAL_INDICES[] = { FSPID_OUT_IDX, FSPIQ_OUT_IDX, FSPIWP_OUT_IDX, FSPIHD_OUT_IDX };

// Encoding patterns for SPI quad mode (4-step cadence, LSB first)
// Each lane is one bit position in a nibble, one byte = two clock cycles, 2 bytes = one 4-step bit
static constexpr uint16_t SPI_ZERO_BIT = 0x0001;  // output: [1,0,0,0] = 25% high
static constexpr uint16_t SPI_ONE_BIT  = 0x0111;  // output: [1,1,1,0] = 75% high

SpiBusContext* SpiBusContext::_instance = nullptr;
uint8_t SpiBusContext::_refCount = 0;

SpiBusContext* SpiBusContext::get() {
    if (_instance == nullptr) {
        _instance = new SpiBusContext();
    }
    _refCount++;
    return _instance;
}

void SpiBusContext::release() {
    if (_refCount == 0) return;
    _refCount--;
    if (_refCount == 0 && _instance) {
        delete _instance;
        _instance = nullptr;
    }
}

SpiBusContext::SpiBusContext()
    : _sending(false)
    , _initialized(false)
    , _hasStarted(false)
    , _currentBuffer(0)
    , _descCount(0)
    , _isrHandle(nullptr)
    , _spiIsrHandle(nullptr)
    , _hw(&GPSPI2)
    , _channelCount(0)
    , _framePos(0)
    , _numBytes(0)
    , _lastTransmitMs(0)
    , _stagedMask(0)
    , _channelMask(0)
{
    _dmaBuffer[0] = _dmaBuffer[1] = nullptr;
    for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
        _channels[i] = {nullptr, -1, nullptr, 0, false};
    }
}

SpiBusContext::~SpiBusContext() {
    deinit();
}

bool SpiBusContext::isSpiDone() {
    if (!_hasStarted) return true;  // no transfer ever started

    // We are logically done once all real data is encoded
    if (!_sending) {
        return true;
    }

    // Fail-safe watchdog: if stuck for > 250ms, recover it
    if (millis() - _lastTransmitMs > 250) {
        forceIdle();
        return true;
    }
    
    return false;
}

void SpiBusContext::forceIdle() {
    _sending = false;
    _stagedMask = 0;

    // Stop DMA immediately
    gdma_dev_t* dma = &GDMA;
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);

    if (_hw) {
        _hw->cmd.usr = 0;
        _hw->dma_int_clr.val = 0xFFFFFFFF;
    }
}

void SpiBusContext::dumpDebug() {
    Serial.printf("\n--- SPI GDMA DEBUG ---\n");
    Serial.printf("_sending=%d, _framePos=%u, _numBytes=%u, _hasStarted=%d\n", _sending, _framePos, _numBytes, _hasStarted);
    Serial.printf("GDMA INTR ST=0x%08X, ENA=0x%08X\n", GDMA.intr[WLEDPB_SPI_GDMA_CHANNEL].st.val, GDMA.intr[WLEDPB_SPI_GDMA_CHANNEL].ena.val);
    if (_hw) {
        Serial.printf("SPI cmd.usr=%d, dma_int_raw=0x%08X\n", _hw->cmd.usr, _hw->dma_int_raw.val);
    }
    Serial.printf("Descriptors:\n");
    Serial.printf("  Desc0: owner=%d length=%d eof=%d addr=0x%p next=0x%p\n", _dmaDesc[0].owner, _dmaDesc[0].length, _dmaDesc[0].eof, &_dmaDesc[0], _dmaDesc[0].qe.stqe_next);
    Serial.printf("  Desc1: owner=%d length=%d eof=%d addr=0x%p next=0x%p\n", _dmaDesc[1].owner, _dmaDesc[1].length, _dmaDesc[1].eof, &_dmaDesc[1], _dmaDesc[1].qe.stqe_next);
    Serial.printf("----------------------\n");
}

void IRAM_ATTR SpiBusContext::encodeSpiChunk(uint8_t bufIdx) {
    uint8_t* dst = _dmaBuffer[bufIdx];
    memset(dst, 0x00, WLEDPB_SPI_DMA_BUFFER_SIZE);

    if (!_sending) {
        return;
    }

    size_t maxSrcThisChunk = WLEDPB_SPI_DMA_BUFFER_SIZE / 16;  // 16 DMA bytes per source byte
    size_t srcBytesLeft = (_framePos < _numBytes) ? (_numBytes - _framePos) : 0;
    size_t srcThisChunk = (srcBytesLeft < maxSrcThisChunk) ? srcBytesLeft : maxSrcThisChunk;

    if (srcThisChunk == 0) {
        _sending = false;
        _framePos = 0;
        return;
    }

    for (uint8_t lane = 0; lane < WLEDPB_SPI_MAX_CHANNELS; lane++) {
        if (!_channels[lane].active || !_channels[lane].srcData) continue;

        size_t srcLen = _channels[lane].srcLen;
        if (_framePos >= srcLen) continue; // Past the end of this lane's data, leave buffer 0 (low/no pulse)

        size_t validBytes = srcLen - _framePos;
        if (validBytes > srcThisChunk) validBytes = srcThisChunk;

        const uint16_t zerobit = SPI_ZERO_BIT << lane;
        const uint16_t onebit  = SPI_ONE_BIT << lane;
        const uint8_t* src = _channels[lane].srcData;
        uint16_t* pOut = reinterpret_cast<uint16_t*>(dst);

        for (size_t i = 0; i < validBytes; i++) {
            uint8_t v = src[_framePos + i];
            *pOut++ |= (v & 0x80) ? onebit : zerobit;
            *pOut++ |= (v & 0x40) ? onebit : zerobit;
            *pOut++ |= (v & 0x20) ? onebit : zerobit;
            *pOut++ |= (v & 0x10) ? onebit : zerobit;
            *pOut++ |= (v & 0x08) ? onebit : zerobit;
            *pOut++ |= (v & 0x04) ? onebit : zerobit;
            *pOut++ |= (v & 0x02) ? onebit : zerobit;
            *pOut++ |= (v & 0x01) ? onebit : zerobit;
        }
    }
    _framePos += srcThisChunk;
}

// SPI interrupt ISR: handles both trans_done (bit counter expired) and
// outfifo_empty_err (FIFO underrun from ISR latency, e.g. WiFi traffic).
// On trans_done: just reload bit length and restart. Re-enable outfifo_empty_err if it was disabled.
// On outfifo_empty_err: reset FIFO, reset DMA, restart. DISABLE the outfifo_empty_err interrupt
// to prevent an infinite ISR loop (the error re-fires immediately if WiFi is still hogging CPU).
// trans_done will re-enable it on the next successful cycle.
void IRAM_ATTR SpiBusContext::spiISR(void* arg) {
    SpiBusContext* ctx = (SpiBusContext*)arg;
    uint32_t raw = ctx->_hw->dma_int_raw.val;

    if (raw & 0x02) {
        // outfifo_empty_err (bit 1): SPI FIFO starved because DMA couldn't keep up.
        // Temporarily disable this interrupt to prevent infinite ISR loop / WDT crash.
        ctx->_hw->dma_int_ena.outfifo_empty_err = 0;
        ctx->_hw->dma_int_clr.val = raw;  // Clear all SPI interrupt flags
        ctx->_hw->cmd.usr = 0;  // Stop SPI

        spi_ll_dma_tx_fifo_reset(ctx->_hw);
        spi_ll_outfifo_empty_clr(ctx->_hw);

        // Reset and restart DMA from current descriptor
        gdma_dev_t* dma = &GDMA;
        gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
        ctx->_dmaDesc[0].owner = 1;
        ctx->_dmaDesc[1].owner = 1;
        gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, 0);
        gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&ctx->_dmaDesc[ctx->_currentBuffer]);
        gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;

        // Restart SPI with max buffer-aligned bit count (31 * 1024 * 8 = 253952)
        // to minimize trans_done restart gaps
        spi_ll_clear_int_stat(ctx->_hw);
        spi_ll_set_mosi_bitlen(ctx->_hw, 253952);
        ctx->_hw->cmd.usr = 1;
        return;
    }

    if (raw & 0x1000) {
        // trans_done (bit 12): SPI finished counting its bits. Restart immediately.
        ctx->_hw->dma_int_clr.trans_done = 1;
        spi_ll_set_mosi_bitlen(ctx->_hw, 253952);
        ctx->_hw->cmd.usr = 1;
        // Re-enable outfifo_empty_err now that we've had a successful cycle
        ctx->_hw->dma_int_ena.outfifo_empty_err = 1;
    }
}

void IRAM_ATTR SpiBusContext::gdmaISR(void* arg) {
    SpiBusContext* ctx = (SpiBusContext*)arg;
    gdma_dev_t* dma = &GDMA;

    if (dma->intr[WLEDPB_SPI_GDMA_CHANNEL].st.out_eof) {
        uint8_t completedBuf = ctx->_currentBuffer;
        ctx->encodeSpiChunk(completedBuf);

        // CRITICAL: Give ownership of the descriptor back to DMA so it can keep feeding SPI
        ctx->_dmaDesc[completedBuf].size = WLEDPB_SPI_DMA_BUFFER_SIZE;
        ctx->_dmaDesc[completedBuf].length = WLEDPB_SPI_DMA_BUFFER_SIZE;
        ctx->_dmaDesc[completedBuf].eof = 1;
        ctx->_dmaDesc[completedBuf].owner = 1;
        ctx->_currentBuffer = completedBuf ? 0 : 1;
    }
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.val = dma->intr[WLEDPB_SPI_GDMA_CHANNEL].st.val;
}

bool SpiBusContext::init(const LedTiming& timing) {
    if (_initialized) return true;

    Serial.printf("[SPI] Initializing SPI parallel bus context\n");

    // Allocate DMA buffers
    for (int i = 0; i < 2; i++) {
        _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, WLEDPB_SPI_DMA_BUFFER_SIZE,
                                                           MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!_dmaBuffer[i]) {
            Serial.printf("[SPI] DMA buffer %d alloc failed\n", i);
            deinit();
            return false;
        }
        memset(_dmaBuffer[i], 0, WLEDPB_SPI_DMA_BUFFER_SIZE);
    }

    // Setup DMA descriptors - circular linked list
    for (int i = 0; i < 2; i++) {
        _dmaDesc[i].size = WLEDPB_SPI_DMA_BUFFER_SIZE;
        _dmaDesc[i].length = WLEDPB_SPI_DMA_BUFFER_SIZE;
        _dmaDesc[i].owner = 1;
        _dmaDesc[i].sosf = 0;
        _dmaDesc[i].eof = 1;
        _dmaDesc[i].buf = _dmaBuffer[i];
    }
    _dmaDesc[0].qe.stqe_next = &_dmaDesc[1];
    _dmaDesc[1].qe.stqe_next = &_dmaDesc[0];

    // Enable peripheral clocks and force-reset (SPI2 + DMA)
    // periph_module_enable() uses ref counting and may be a no-op if the
    // peripheral was already enabled. Explicit reset ensures clean state.
    periph_module_enable(PERIPH_SPI2_MODULE);
    periph_module_reset(PERIPH_SPI2_MODULE);
    periph_module_enable(PERIPH_GDMA_MODULE);
    periph_module_reset(PERIPH_GDMA_MODULE);

    // Configure SPI2 master
    spi_ll_master_init(_hw);
    spi_ll_master_set_mode(_hw, 0);
    spi_ll_set_tx_lsbfirst(_hw, true);

    _hw->user.usr_command = 0;
    _hw->user.usr_addr = 0;
    _hw->user.usr_dummy = 0;
    _hw->user.usr_miso = 0;
    _hw->user.usr_mosi = 1;

    // To prevent D2 and D3 from idling high, explicitly clear idle output polarities
    _hw->ctrl.q_pol = 0;
    _hw->ctrl.d_pol = 0;
    _hw->ctrl.hold_pol = 0;
    _hw->ctrl.wp_pol = 0;

    // To prevent WP (D2) and HD (D3) from idling high, clear quad mode in ctrl and user registers?
    // Let's first clear the command/address phases to fix the initial corrupted 12 zeroes.
    
    spi_line_mode_t linemode = {};
    linemode.data_lines = 4;  // quad mode
    spi_ll_master_set_line_mode(_hw, linemode);

    // Clock: target ~2.6MHz for ~390ns per step, matching user's tested config
    // 4 steps per bit → ~1560ns per bit (within WS2812 tolerance)
    // Clock: 4 steps per bit → 4 SPI clock cycles per bit period.
    // targetFreq = 4 / (bitPeriod_ns * 1e-9) = 4,000,000,000 / bitPeriod_ns
    uint32_t bitPeriodNs = timing.bitPeriod();
    uint32_t targetFreq = 4000000000UL / bitPeriodNs;
    if (targetFreq < 2000000) targetFreq = 2000000;
    if (targetFreq > 5000000) targetFreq = 5000000;

    spi_ll_master_set_clock(_hw, 80000000, targetFreq, 128);

    Serial.printf("[SPI] Bit period: %uns, target SPI freq: %uHz\n", bitPeriodNs, targetFreq);

    // Route SPI clock to a dummy pin (needed for DMA to work)
    // GPIO11 is VDD_SPI on C3 but routing CLK to it doesn't affect power.
    // We don't set it as OUTPUT manually to allow the matrix to take full control.
    pinMatrixOutAttach(11, FSPICLK_OUT_IDX, false, false);

    // Matching working reference order: set_mosi_bitlen BEFORE enable_mosi
    // Use a default value; will be overwritten in startTransmit with actual frame size
    spi_ll_set_mosi_bitlen(_hw, 16384);
    spi_ll_enable_mosi(_hw, true);

    spi_ll_dma_tx_enable(_hw, true);
    spi_ll_dma_tx_fifo_reset(_hw);
    spi_ll_outfifo_empty_clr(_hw);
    spi_ll_apply_config(_hw);
    
    Serial.printf("[SPI] init: hw->cmd.val after reset=0x%08X\n", _hw->cmd.val);

    // Configure GDMA (matching working reference: reset, connect, set desc addr, start)
    gdma_dev_t* dma = &GDMA;
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
    gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, 0);
    gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);
    gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);

    // Enable EOF interrupt, then install ISR (matching working reference order)
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;

    esp_err_t err = esp_intr_alloc(ETS_DMA_CH0_INTR_SOURCE,
                                    ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL2,
                                    gdmaISR, this, &_isrHandle);
    if (err != ESP_OK) {
        Serial.printf("[SPI] GDMA ISR alloc failed: %d\n", err);
        deinit();
        return false;
    }

    // Install SPI ISR for trans_done and outfifo_empty_err recovery.
    // trans_done: SPI finished its bit counter, restart immediately.
    // outfifo_empty_err: FIFO underrun (e.g. WiFi ISR delayed DMA), must reset and recover.
    _hw->dma_int_clr.val = 0xFFFFFFFF;  // Clear all flags
    _hw->dma_int_ena.trans_done = 1;
    _hw->dma_int_ena.outfifo_empty_err = 1;
    err = esp_intr_alloc(ETS_SPI2_INTR_SOURCE,
                         ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL2,
                         spiISR, this, &_spiIsrHandle);
    if (err != ESP_OK) {
        Serial.printf("[SPI] SPI ISR alloc failed: %d\n", err);
        deinit();
        return false;
    }

    _initialized = true;
    Serial.printf("[SPI] Init complete\n");
    return true;
}

void SpiBusContext::deinit() {
    Serial.printf("[SPI] deinit() starting\n");
    _sending = false;

    // Stop SPI and DMA before freeing resources
    if (_hw) {
        _hw->cmd.usr = 0;  // Stop SPI transfer
    }

    gdma_dev_t* dma = &GDMA;
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;  // Disable interrupt
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);

    if (_hw) {
        _hw->dma_int_ena.trans_done = 0;  // Disable SPI trans_done interrupt
    }

    if (_spiIsrHandle) {
        esp_intr_free(_spiIsrHandle);
        _spiIsrHandle = nullptr;
    }

    if (_isrHandle) {
        esp_intr_free(_isrHandle);
        _isrHandle = nullptr;
    }

    for (int i = 0; i < 2; i++) {
        if (_dmaBuffer[i]) {
            heap_caps_free(_dmaBuffer[i]);
            _dmaBuffer[i] = nullptr;
        }
    }

    periph_module_disable(PERIPH_SPI2_MODULE);
    periph_module_disable(PERIPH_GDMA_MODULE);
    _initialized = false;
}

int8_t SpiBusContext::registerChannel(int8_t pin, ParallelSpiBus* bus) {
    int8_t idx = -1;
    for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
        if (!_channels[i].active) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;

    _channels[idx].bus = bus;
    _channels[idx].pin = pin;
    _channels[idx].active = true;
    _channelCount++;
    _channelMask |= (1 << idx);

    // Route SPI data signal to GPIO
    pinMode(pin, OUTPUT);
    pinMatrixOutAttach(pin, SPI_SIGNAL_INDICES[idx], false, false);

    Serial.printf("[SPI] Registered channel %d on pin %d (signal=%d)\n", idx, pin, SPI_SIGNAL_INDICES[idx]);
    return idx;
}

void SpiBusContext::unregisterChannel(int8_t channelIdx) {
    if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
    if (!_channels[channelIdx].active) return;

    if (_channels[channelIdx].pin >= 0) {
        gpio_reset_pin((gpio_num_t)_channels[channelIdx].pin);
    }

    _channels[channelIdx] = {nullptr, -1, nullptr, 0, false};
    _channelCount--;
    _channelMask &= ~(1 << channelIdx);
}

void SpiBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
    if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
    _channels[channelIdx].srcData = data;
    _channels[channelIdx].srcLen = len;
    
    // Mark this channel as staged
    _stagedMask |= (1 << channelIdx);
    
    if (len > _numBytes) _numBytes = len;
}

void SpiBusContext::resetAndStart() {
    // Matching working reference loop restart order exactly:
    // 1. Wait for any previous SPI to finish
    //    (caller should have ensured this, but belt-and-suspenders)
    // 2. Reset SPI FIFO
    spi_ll_dma_tx_fifo_reset(_hw);
    spi_ll_outfifo_empty_clr(_hw);
    spi_ll_apply_config(_hw);

    // 3. Restore DMA descriptor ownership (DMA clears owner after processing)
    _dmaDesc[0].owner = 1;
    _dmaDesc[1].owner = 1;

    // 4. Reset and restart GDMA (matching working reference order)
    gdma_dev_t* dma = &GDMA;
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
    gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, 0);
    gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);
    gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);

    // Re-enable EOF interrupt after channel reset (reset may clear enable bits)
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;
}

bool SpiBusContext::startTransmit() {
    if (_sending) return false;
    if (_channelCount == 0) return false;

    // Only start transmission if ALL active channels have staged data
    if (_stagedMask != _channelMask) return false;
    _stagedMask = 0; // Reset for next frame

    _lastTransmitMs = millis();
    size_t newBytes = 0;
    for (int ch = 0; ch < WLEDPB_SPI_MAX_CHANNELS; ch++) {
        if (_channels[ch].active && _channels[ch].srcLen > newBytes) {
            newBytes = _channels[ch].srcLen;
        }
    }

    if (!_hasStarted) {
        _framePos = 0;
        _numBytes = newBytes;

        // Set totalBits to max buffer-aligned value (31 * 1024 * 8 = 253952 bits).
        // This gives ~97ms of continuous output before trans_done fires and restarts.
        // If FIFO underruns (WiFi traffic etc), outfifo_empty_err fires and recovers.
        uint32_t totalBits = 253952;

        spi_ll_set_mosi_bitlen(_hw, totalBits);

        // Clear the trans_done flag BEFORE starting new transfer.
        // spi_ll_usr_is_done() checks dma_int_raw.trans_done which is sticky —
        // it stays set after the previous transfer until explicitly cleared.
        spi_ll_clear_int_stat(_hw);

        // Set state and encode buffers FIRST before starting DMA
        _sending = true;
        _currentBuffer = 0;
        encodeSpiChunk(0);
        encodeSpiChunk(1);

        // Reset SPI + DMA first (matching working reference loop order)
        resetAndStart();

        _hasStarted = true;

        // Start SPI transfer LAST (matching working reference: spi_ll_user_start is the final step)
        spi_ll_user_start(_hw);
    } else {
        // Infinite send active!
        // The hardware is already running, churning out zeros via the background ISR loop.
        // All we need to do is reset the pointers and flip _sending = true. The ISR 
        // will naturally pick this up and seamlessly transition from sending zeros to legitimate frame data!
        
        gdma_dev_t* dma = &GDMA;
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0; // Temporarily pause ISR to prevent race condition

        _framePos = 0;
        _numBytes = newBytes;
        _sending = true; // Engage new frame

        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1; // Unpause ISR
    }

    return true;
}

// SpiBus implementation

ParallelSpiBus::ParallelSpiBus(int8_t pin, const LedTiming& timing, ColorOrder order)
    : _pin(pin)
    , _timing(timing)
    , _order(order)
    , _initialized(false)
    , _channelIdx(-1)
    , _ctx(nullptr)
    , _encodeBuffer(nullptr)
    , _encodeBufferSize(0)
{
}

ParallelSpiBus::~ParallelSpiBus() {
    end();
}

bool ParallelSpiBus::begin() {
    if (_initialized) return true;

    _ctx = SpiBusContext::get();
    if (!_ctx) return false;

    if (!_ctx->init(_timing)) {
        SpiBusContext::release();
        _ctx = nullptr;
        return false;
    }

    _channelIdx = _ctx->registerChannel(_pin, this);
    if (_channelIdx < 0) {
        Serial.printf("[SPI] registerChannel failed for pin %d\n", _pin);
        SpiBusContext::release();
        _ctx = nullptr;
        return false;
    }

    _initialized = true;
    Serial.printf("[SPI] ParallelSpiBus::begin() OK: pin=%d, channel=%d\n", _pin, _channelIdx);
    return true;
}

void ParallelSpiBus::end() {
    Serial.printf("[SPI] ParallelSpiBus::end() pin=%d\n", _pin);
    if (!_initialized) return;

    if (_ctx) {
        uint32_t startWait = millis();
        while (!_ctx->isIdle()) {
            if (millis() - startWait > 500) {
                Serial.printf("[SPI] end() timeout waiting for idle, forcing...\n");
                break;
            }
            vTaskDelay(1);
        }
        _ctx->unregisterChannel(_channelIdx);
        SpiBusContext::release();
        _ctx = nullptr;
    }

    if (_encodeBuffer) {
        heap_caps_free(_encodeBuffer);
        _encodeBuffer = nullptr;
        _encodeBufferSize = 0;
    }

    _initialized = false;
}

bool ParallelSpiBus::allocateBuffer(uint16_t numPixels) {
    size_t needed = numPixels * getChannelCount(_order);
    if (_encodeBuffer && _encodeBufferSize >= needed) return true;

    if (_encodeBuffer) heap_caps_free(_encodeBuffer);

    _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_INTERNAL);
    if (!_encodeBuffer) {
        _encodeBufferSize = 0;
        return false;
    }
    _encodeBufferSize = needed;
    return true;
}

bool ParallelSpiBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!_initialized || !_ctx || !_pixelData || _numPixels == 0) return false;

    // Wait for previous transmission
    uint32_t startWait = millis();
    while (!_ctx->isIdle()) {
        if (millis() - startWait > 500) {
            Serial.printf("[SPI] show() timeout waiting for idle\n");
            _ctx->dumpDebug();
            // Force idle to prevent endless freeze
            _ctx->forceIdle();
            return false;
        }
        vTaskDelay(1);
    }

    // Wait for SPI to finish any remaining transfer
    startWait = millis();
    while (!_ctx->isSpiDone()) {
        if (millis() - startWait > 500) {
            Serial.printf("[SPI] show() timeout waiting for SpiDone\n");
            _ctx->dumpDebug();
            _ctx->forceIdle();
            return false;
        }
        vTaskDelay(1);
    }

    if (!allocateBuffer(_numPixels)) return false;

    ColorEncoder encoder(_order);
    uint8_t* dst = _encodeBuffer;
    uint8_t numCh = encoder.getNumChannels();

    for (uint16_t i = 0; i < _numPixels; i++) {
        encoder.encode(_pixelData[i], _cctData ? &_cctData[i] : nullptr, dst);
        dst += numCh;
    }

    _ctx->setChannelData(_channelIdx, _encodeBuffer, _numPixels * numCh);

    return _ctx->startTransmit();
}

bool ParallelSpiBus::canShow() const {
    if (!_ctx) return true;
    return _ctx->isIdle() && _ctx->isSpiDone();
}

void ParallelSpiBus::waitComplete() {
    while (_ctx && !_ctx->isIdle()) {
        vTaskDelay(1);
    }
}

void ParallelSpiBus::setColorOrder(ColorOrder order) {
    _order = order;
}

void ParallelSpiBus::setPixelColor(uint16_t pix, uint32_t c, const CctPixel* cp) {
    IBus::setPixelColor(pix, c, cp);
}

#endif // WLEDPB_SPI_SUPPORT

//==============================================================================
// Bus Factory Implementation
//==============================================================================

IBus* createBus(BusType type, int8_t pin, const LedTiming& timing,
                ColorOrder order, size_t bufferSize, int8_t channel) {
    
    Serial.printf("[WPB] createBus type=%u pin=%d bufSize=%u ch=%d\n", (unsigned)type, pin, bufferSize, channel);
    if (type == BusType::Auto) {
        type = getRecommendedBusType();
        Serial.printf("[WPB] Auto resolved to type=%u\n", (unsigned)type);
    }

    IBus* bus = nullptr;

    switch (type) {
        case BusType::RMT:
            bus = new RmtBus(pin, timing, order, channel);
            break;

#ifdef WLEDPB_I2S_SUPPORT
        case BusType::I2S:
            bus = new I2sBus(pin, timing, order, 1, bufferSize);
            break;
#endif

#ifdef WLEDPB_LCD_SUPPORT
        case BusType::LCD:
            bus = new LcdBus(pin, timing, order, bufferSize);
            break;
#endif

#ifdef WLEDPB_SPI_SUPPORT
        case BusType::SPI:
            bus = new ParallelSpiBus(pin, timing, order);
            break;
#endif

        default:
            return nullptr;
    }

    return bus;
}

BusType getRecommendedBusType() {
#if defined(WLEDPB_ESP32S3)
    return BusType::LCD;  // S3 has best LCD support
#elif defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2)
    return BusType::I2S;  // Original and S2 have I2S parallel
#elif defined(WLEDPB_SPI_SUPPORT)
    return BusType::SPI;  // C3 uses SPI quad mode
#else
    return BusType::RMT;  // Fallback to RMT
#endif
}

} // namespace WLEDpixelBus

