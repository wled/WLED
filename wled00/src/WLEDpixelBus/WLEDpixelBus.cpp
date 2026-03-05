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

// Thread-local context for RMT translate callback
static struct {
    uint32_t bit0;
    uint32_t bit1;
} s_rmtCtx;

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
        _channel = 0;
    }

#if defined(WLEDPB_ESP32)
    if (_channel > 7) return false;
#else
    if (_channel > 3) return false;
#endif
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
        log_e("RMT config failed: %d", err);
        return false;
    }


    err = rmt_driver_install(_rmtChannel, 0, ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK) {
        log_e("RMT driver install failed: %d", err);
        return false;
    }

    err = rmt_translator_init(_rmtChannel, translateCB);
    if (err != ESP_OK) {
        log_e("RMT translator init failed:  %d", err);
        rmt_driver_uninstall(_rmtChannel);
        return false;
    }

    _initialized = true;
    return true;
}

void RmtBus::end() {
    if (!_initialized) return;

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
    if (!_initialized || !pixels || numPixels == 0) return false;

    // Wait for previous transmission
    rmt_wait_tx_done(_rmtChannel, portMAX_DELAY);

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

    // Start transmission
    size_t dataLen = numPixels * numCh;
    esp_err_t err = rmt_write_sample(_rmtChannel, _encodeBuffer, dataLen, true);

    return err == ESP_OK;
}

bool RmtBus::canShow() const {
    if (!_initialized) return true;

    rmt_channel_status_result_t status;
    rmt_get_channel_status(&status);
    return status.status[_rmtChannel] == RMT_CHANNEL_IDLE;
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

    while (size < src_size && num < wanted_num) {
        uint8_t byte = *psrc++;
        size++;

        for (int i = 7; i >= 0 && num < wanted_num; i--) {
            pdest->val = (byte & (1 << i)) ? bit1 : bit0;
            pdest++;
            num++;
        }
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
    _bufferSize = bufferSize;

    // Allocate DMA buffers
    for (int i = 0; i < 2; i++) {
        _dmaBuffer[i] = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_DMA);
        if (!_dmaBuffer[i]) {
            log_e("I2S DMA buffer alloc failed");
            deinit();
            return false;
        }
        memset(_dmaBuffer[i], 0, bufferSize);

        _dmaDesc[i] = (lldesc_t*)heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
        if (!_dmaDesc[i]) {
            log_e("I2S DMA desc alloc failed");
            deinit();
            return false;
        }
    }

    // Setup DMA descriptors - linked list for ping-pong
    for (int i = 0; i < 2; i++) {
        _dmaDesc[i]->size = bufferSize;
        _dmaDesc[i]->length = bufferSize;
        _dmaDesc[i]->buf = _dmaBuffer[i];
        _dmaDesc[i]->eof = 1;  // Generate interrupt on completion
        _dmaDesc[i]->sosf = 0;
        _dmaDesc[i]->owner = 1;
        _dmaDesc[i]->qe.stqe_next = nullptr;  // Set dynamically
    }

    // Enable I2S peripheral
#if defined(WLEDPB_ESP32)
    periph_module_enable(_busNum == 0 ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE);
#else
    periph_module_enable(PERIPH_I2S0_MODULE);
#endif

    // Reset I2S
    _i2sDev->conf.tx_reset = 1;
    _i2sDev->conf.tx_reset = 0;
    _i2sDev->conf.rx_reset = 1;
    _i2sDev->conf.rx_reset = 0;
    _i2sDev->conf.tx_fifo_reset = 1;
    _i2sDev->conf.tx_fifo_reset = 0;

    // Configure for parallel LCD mode
    _i2sDev->conf2.lcd_en = 1;
    _i2sDev->conf2.lcd_tx_wrx2_en = 0;
    _i2sDev->conf2.lcd_tx_sdx2_en = 0;

    // Calculate clock divider for 4-step cadence
    // Bit period / 4 = step time
    uint32_t bitPeriodNs = timing.bitPeriod();
    uint32_t stepPeriodNs = bitPeriodNs / 4;

#if defined(WLEDPB_ESP32)
    uint32_t baseClock = 160000000;
#else
    uint32_t baseClock = 80000000;
#endif

    _clockDiv = (baseClock / 1000000) * stepPeriodNs / 1000;
    if (_clockDiv < 2) _clockDiv = 2;
    if (_clockDiv > 255) _clockDiv = 255;

    // Set clock
    _i2sDev->clkm_conf.clkm_div_a = 0;
    _i2sDev->clkm_conf.clkm_div_b = 0;
    _i2sDev->clkm_conf.clkm_div_num = _clockDiv;
    _i2sDev->clkm_conf.clk_en = 1;

    // Sample configuration
    _i2sDev->fifo_conf.tx_fifo_mod = 1;  // 8-bit parallel
    _i2sDev->fifo_conf.tx_fifo_mod_force_en = 1;
    _i2sDev->sample_rate_conf.tx_bck_div_num = 1;
    _i2sDev->sample_rate_conf.tx_bits_mod = 8;

    // Channel config
    _i2sDev->conf_chan.tx_chan_mod = 1;
    _i2sDev->conf.tx_right_first = 1;
    _i2sDev->conf.tx_msb_right = 0;
    _i2sDev->conf.tx_mono = 1;

    // Enable DMA
    _i2sDev->fifo_conf.dscr_en = 1;
    _i2sDev->lc_conf.out_data_burst_en = 1;
    _i2sDev->lc_conf.outdscr_burst_en = 1;

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
    return true;
}

void I2sBusContext::deinit() {
    if (_i2sDev) {
        _i2sDev->conf.tx_start = 0;
        _i2sDev->out_link.start = 0;
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
}

void I2sBusContext::encode4Step(uint8_t* dest, size_t destLen, size_t* bytesWritten) {
    // 4-step cadence encoding for parallel output
    // Each source bit becomes 4 DMA bytes (one bit per channel in each byte)
    // Pattern: [1][data][data][0] for each bit
    //
    // For multiple channels, we interleave bits into each byte position

    size_t written = 0;
    memset(dest, 0, destLen);

    // Process each source byte position across all channels
    while (written + 32 <= destLen) {  // 8 bits * 4 steps = 32 bytes per source byte
        bool hasData = false;

        for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
            if (! _channels[ch].active) continue;
            if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;

            hasData = true;
            uint8_t srcByte = _channels[ch].srcData[_channels[ch].srcPos];
            uint8_t chMask = (1 << ch);

            uint8_t* p = dest + written;
            for (int bit = 7; bit >= 0; bit--) {
                uint8_t dataVal = (srcByte >> bit) & 1;

                // Step 0: Always HIGH (start of bit)
                p[0] |= chMask;

                // Step 1: HIGH for '1', LOW for '0'
                if (dataVal) p[1] |= chMask;

                // Step 2: HIGH for '1', LOW for '0'
                if (dataVal) p[2] |= chMask;

                // Step 3: Always LOW (already 0)

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

        written += 32;
    }

    *bytesWritten = written;
}

void I2sBusContext:: fillBuffer(uint8_t bufIdx) {
    size_t written;
    encode4Step(_dmaBuffer[bufIdx], _bufferSize, &written);
    _dmaDesc[bufIdx]->length = written;
}

bool I2sBusContext::startTransmit() {
    if (_state != DriverState::Idle) return false;
    if (_channelCount == 0) return false;

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

    // Check if we need second buffer
    bool moreData = false;
    for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
        if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
            moreData = true;
            break;
        }
    }

    // Setup DMA chain
    if (moreData) {
        _dmaDesc[0]->qe.stqe_next = _dmaDesc[1];
        _dmaDesc[1]->qe.stqe_next = nullptr;  // Will be set in ISR if more data
    } else {
        _dmaDesc[0]->qe.stqe_next = _dmaDesc[1]->length > 0 ? _dmaDesc[1] : nullptr;
        _dmaDesc[1]->qe.stqe_next = nullptr;
    }

    _activeBuffer = 0;
    _state = DriverState::Sending;

    // Reset FIFO
    _i2sDev->conf.tx_fifo_reset = 1;
    _i2sDev->conf.tx_fifo_reset = 0;

    // Clear interrupts
    _i2sDev->int_clr.val = 0xFFFFFFFF;

    // Enable EOF interrupt
    _i2sDev->int_ena.out_eof = 1;

    // Set DMA start address
    _i2sDev->out_link.addr = (uint32_t)_dmaDesc[0];

    // Start transmission
    _i2sDev->out_link.start = 1;
    _i2sDev->conf.tx_start = 1;

    return true;
}

void IRAM_ATTR I2sBusContext::dmaISR(void* arg) {
    I2sBusContext* ctx = (I2sBusContext*)arg;
    i2s_dev_t* dev = ctx->_i2sDev;

    uint32_t status = dev->int_st.val;
    dev->int_clr.val = status;

    if (status & I2S_OUT_EOF_INT_ST) {
        // Current buffer finished, check if more data
        bool moreData = false;
        for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
            if (ctx->_channels[ch].active && 
                ctx->_channels[ch].srcPos < ctx->_channels[ch].srcLen) {
                moreData = true;
                break;
            }
        }

        if (moreData) {
            // Refill the completed buffer
            uint8_t completedBuf = ctx->_activeBuffer;
            ctx->_activeBuffer ^= 1;  // Switch to other buffer

            // Fill the completed buffer with new data
            size_t written;
            ctx->encode4Step(ctx->_dmaBuffer[completedBuf], ctx->_bufferSize, &written);
            ctx->_dmaDesc[completedBuf]->length = written;

            // Link it after current buffer
            ctx->_dmaDesc[ctx->_activeBuffer]->qe.stqe_next = ctx->_dmaDesc[completedBuf];
            ctx->_dmaDesc[completedBuf]->qe.stqe_next = nullptr;
        } else {
            // All data sent, add reset time then go idle
            ctx->_state = DriverState::Idle;
            dev->conf.tx_start = 0;
            dev->out_link.start = 0;
        }
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
    , _encodedLen(0)
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
        I2sBusContext::release(_busNum);
        _ctx = nullptr;
        return false;
    }

    _initialized = true;
    return true;
}

void I2sBus::end() {
    if (!_initialized) return;

    if (_ctx) {
        _ctx->unregisterChannel(_channelIdx);
        I2sBusContext::release(_busNum);
        _ctx = nullptr;
    }

    if (_encodeBuffer) {
        free(_encodeBuffer);
        _encodeBuffer = nullptr;
        _encodeBufferSize = 0;
    }

    _initialized = false;
}

bool I2sBus:: allocateBuffer(uint16_t numPixels) {
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

bool I2sBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
    if (!_initialized || !_ctx || !pixels || numPixels == 0) return false;

    // Wait for previous transmission
    while (! _ctx->isIdle()) {
        vTaskDelay(1);
    }

    if (!allocateBuffer(numPixels)) return false;

    // Encode pixels
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

#include "driver/periph_ctrl.h"
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
}

size_t LcdBusContext:: encodeIntoBuffer(uint8_t* dest, size_t destLen) {
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

bool LcdBusContext::hasMoreData() const {
    for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
        if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
            return true;
        }
    }
    return false;
}

void LcdBusContext::fillBufferForState(uint8_t bufIdx) {
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
// Bus Factory Implementation
//==============================================================================

IBus* createBus(BusType type, int8_t pin, const LedTiming& timing,
                ColorOrder order, size_t bufferSize) {
    
    if (type == BusType::Auto) {
        type = getRecommendedBusType();
    }

    IBus* bus = nullptr;

    switch (type) {
        case BusType::RMT:
            bus = new RmtBus(pin, timing, order);
            break;

#ifdef WLEDPB_I2S_SUPPORT
        case BusType::I2S:
            bus = new I2sBus(pin, timing, order, 0, bufferSize);
            break;
#endif

#ifdef WLEDPB_LCD_SUPPORT
        case BusType::LCD:
            bus = new LcdBus(pin, timing, order, bufferSize);
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
#else
    return BusType::RMT;  // Fallback to RMT
#endif
}

} // namespace WLEDpixelBus