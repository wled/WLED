#include "WLEDpixelBus.h"
#ifdef WLEDPB_SPI_SUPPORT
#include "WLEDpixelBus_ParallelSpi.h"

#undef FLAG_ATTR
#define FLAG_ATTR(TYPE)
#include "hal/spi_ll.h"
#include "driver/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"

namespace WLEDpixelBus {

//==============================================================================
// SPI Parallel Bus Implementation (ESP32-C3)
//==============================================================================


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

// SPI ISR: handles trans_done (restart SPI bit counter) and outfifo_empty_err
// (FIFO underrun recovery). outfifo_empty_err is temporarily disabled after
// handling to prevent infinite ISR loops; trans_done re-enables it.
void IRAM_ATTR SpiBusContext::spiISR(void* arg) {
    SpiBusContext* ctx = (SpiBusContext*)arg;
    uint32_t raw = ctx->_hw->dma_int_raw.val;

    if (raw & 0x02) {
        // outfifo_empty_err: FIFO starved, disable to prevent ISR loop
        ctx->_hw->dma_int_ena.outfifo_empty_err = 0;
        ctx->_hw->dma_int_clr.val = raw;  // Clear all SPI interrupt flags
        ctx->_hw->cmd.usr = 0;  // Stop SPI

        spi_ll_dma_tx_fifo_reset(ctx->_hw);
        spi_ll_outfifo_empty_clr(ctx->_hw);

        gdma_dev_t* dma = &GDMA;
        gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
        ctx->_dmaDesc[0].owner = 1;
        ctx->_dmaDesc[1].owner = 1;
        gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, 0);
        gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&ctx->_dmaDesc[ctx->_currentBuffer]);
        gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;

        spi_ll_clear_int_stat(ctx->_hw);
        spi_ll_set_mosi_bitlen(ctx->_hw, 253952);
        ctx->_hw->cmd.usr = 1;
        return;
    }

    if (raw & 0x1000) {
        ctx->_hw->dma_int_clr.trans_done = 1;
        spi_ll_set_mosi_bitlen(ctx->_hw, 253952);
        ctx->_hw->cmd.usr = 1;
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

    // Clear idle output polarities for D2/D3
    _hw->ctrl.q_pol = 0;
    _hw->ctrl.d_pol = 0;
    _hw->ctrl.hold_pol = 0;
    _hw->ctrl.wp_pol = 0;

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

    // Route SPI clock to a dummy pin (needed for DMA to work)
    pinMatrixOutAttach(11, FSPICLK_OUT_IDX, false, false);

    spi_ll_set_mosi_bitlen(_hw, 16384);
    spi_ll_enable_mosi(_hw, true);

    spi_ll_dma_tx_enable(_hw, true);
    spi_ll_dma_tx_fifo_reset(_hw);
    spi_ll_outfifo_empty_clr(_hw);
    spi_ll_apply_config(_hw);

    // Configure GDMA
    gdma_dev_t* dma = &GDMA;
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
    gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, 0);
    gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);
    gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);

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

    // Install SPI ISR for trans_done and outfifo_empty_err recovery
    _hw->dma_int_clr.val = 0xFFFFFFFF;
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
    return true;
}

void SpiBusContext::deinit() {
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
    spi_ll_dma_tx_fifo_reset(_hw);
    spi_ll_outfifo_empty_clr(_hw);
    spi_ll_apply_config(_hw);

    _dmaDesc[0].owner = 1;
    _dmaDesc[1].owner = 1;

    gdma_dev_t* dma = &GDMA;
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
    gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, 0);
    gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);
    gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);

    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;
}

bool SpiBusContext::startTransmit() {
    if (_sending) return false;
    if (_channelCount == 0) return false;

    if (_stagedMask != _channelMask) return false;
    _stagedMask = 0;

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

        // Max buffer-aligned bit count: continuous output for ~97ms before
        // trans_done restarts. outfifo_empty_err handles FIFO underruns.
        spi_ll_set_mosi_bitlen(_hw, 253952);
        spi_ll_clear_int_stat(_hw);

        _sending = true;
        _currentBuffer = 0;
        encodeSpiChunk(0);
        encodeSpiChunk(1);

        resetAndStart();
        _hasStarted = true;
        spi_ll_user_start(_hw);
    } else {
        // Hardware already running - just update pointers, ISR picks up new data
        gdma_dev_t* dma = &GDMA;
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
        _framePos = 0;
        _numBytes = newBytes;
        _sending = true;
        dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
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
    return true;
}

void ParallelSpiBus::end() {
    if (!_initialized) return;

    if (_ctx) {
        uint32_t startWait = millis();
        while (!_ctx->isIdle()) {
            if (millis() - startWait > 500) {
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
            _ctx->forceIdle();
            return false;
        }
        vTaskDelay(1);
    }

    // Wait for SPI to finish any remaining transfer
    startWait = millis();
    while (!_ctx->isSpiDone()) {
        if (millis() - startWait > 500) {
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


} // namespace WLEDpixelBus
#endif // WLEDPB_SPI_SUPPORT