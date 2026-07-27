/*-------------------------------------------------------------------------
WLEDpixelBus - Lightweight LED driver library for WLED

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna):
NeoPixelBus served me well as a reference to proper hardware initialisation
as well as figuring out the more exotic digital LED types

Features:
- Runtime LED timing configuration
- Support for ESP32, ESP32-S2, ESP32-S3, ESP32-C3 and ESP8266
- RMT, I2S parallel, LCD parallel, SPI parallel, BitBang parallel
- 2-Pin LED support (hardware SPI and BitBang)
- RGBW uint32_t pixel buffer format (WLED native)
- Support for up to 6 color channels, 8bit or 16bit
- Supports output inversion (ESP32 only) and individual color channel inversion
- Supports hardware LED brightness for improved color resolution if available

Currently based on IDF v4.x API functions and low-level HAL

-------------------------------------------------------------------------*/

/*
TODO List
- size DMA buffers such that the buffer completes one LED
- ESP32 C61 has no RMT and no PARLIO, either bit bang or check if parallel SPI can be used (need hardware first)
- need to check if the features "custom bus start indices" and "global color override" work (color override probably does not work)
- in BB bus: if sending gets cancelled due to interrupt, send it again. i.e. need a loop to resend up to 5 times or so
- the LCD bus works but can crash due to Core  1 panic'ed (Cache disabled but cached memory region accessed). something is not put in IRAM -> should be fixed, needs testing
- I2S/LCD requires more stress-testing to ensure glitch-free outputs, for 16-parallel maybe even 4 buffers are needed under heavy load
- the DMA buffer count and size need to be checked agains memory usage formulas, they may be incorrect (we could do "worst case" in the UI as it wont matter much for a small amount of LEDs)
- I2S driver now has safe watchdog in case interrupts are missed. check if parallel spi driver and parlio driver can suffer from the same race condition (nullpointer / end of transfer overwritten or missed)
- LOOPTEST_CYCLES in BB bus need fine-tuning
- color orders of some strips may differ from NPB implementation, needs checking and reordering to match legacy behaviour or users will complain
- move gamma down to bus level, see pending PR
- brightness scaling for special buses with LED current (or 16bit) may need fine-tuning
- SPI 2-wire bus types need testing on all platforms (tested working in the past but not recently)
- SPI 2-wire types do not support signal inversion
- ESP32_DATA_IDLE_HIGH flag is a hack to fix bad hardware design and uses the legacy RMT driver, we should drop support for that
- UI: restrict custom timing inputs in hw-parallel buses to only allow changing t0h and calculate from that (to represent hardware limits), reset time is free to choose (check its properly limited to not cause overflows)
- parallel SPI driver: use more modern GMDA channel reservation to get rid of hard ceded GDMA_CHANNEL, see todo in parallelSPI.h bus
- parallel SPI driver: the reset pulse handling needs some improvement, currently SPI_RESET_BITS is fixed
- PARLIO bus needs an in depth review to check for any AI slop
- use chip capabilities instead of individual target ifdefs in buswrapper
- ESP8266 UART and I2S buses show() return false instead of waiting which is inconsistent
*/


#include "WLEDpixelBus.h"

namespace WLEDpixelBus {

// LED Timing Lookup
LedTiming getProtocol(uint8_t wledType) {
  uint8_t idx = getTimingIndex(wledType);
#ifdef ESP8266
  return LedTiming(
    (uint16_t)pgm_read_word(&s_ledTimings[idx].t0h_ns),
    (uint16_t)pgm_read_word(&s_ledTimings[idx].t0l_ns),
    (uint16_t)pgm_read_word(&s_ledTimings[idx].t1h_ns),
    (uint16_t)pgm_read_word(&s_ledTimings[idx].t1l_ns),
    (uint32_t)pgm_read_dword(&s_ledTimings[idx].reset_us)
  );
#else
  return s_ledTimings[idx];
#endif
}

//==============================================================================
// Color Encoder Implementation
//==============================================================================

ColorEncoder::ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType)
{
  _invertMask = 0;
  memset(_channelMap, 0, sizeof(_channelMap));

  // --- Standard types: fast path via _idxR/_idxG/_idxB/_idxW/_idxCW ---

  uint8_t flags = 0;

  // Decode RGB wire positions from color order lower nibble (see ColorOrder enum)
  uint8_t rPos, gPos, bPos;
  switch (co & 0x0F) {
    case ORDER_RGB: rPos=0; gPos=1; bPos=2; break;
    case ORDER_BRG: rPos=1; gPos=2; bPos=0; break;
    case ORDER_RBG: rPos=0; gPos=2; bPos=1; break;
    case ORDER_BGR: rPos=2; gPos=1; bPos=0; break;
    case ORDER_GBR: rPos=2; gPos=0; bPos=1; break;
    default:        rPos=1; gPos=0; bPos=2; break; // ORDER_GRB
  }
  _idxR = rPos; _idxG = gPos; _idxB = bPos;
  _idxW = 3; _idxCW = 4; // defaults, overridden below as needed

  // White/CCT channels: numChannels from bus type, W-swap from upper nibble
  if (numChannels == CHANNELS_RGBW) {
    // LED-type-specific native wire order: TM1814 and TM1815 send W first.
    // Shift RGB indices up by 1 and place W at index 0 before applying user wSwap.
    if (ledType == TYPE_TM1814 || ledType == TYPE_TM1815) {
      _idxR++; _idxG++; _idxB++;
      _idxW = 0;
    }
    const uint8_t wSwap = co >> 4;
    if (wSwap == WSWAP_B) { std::swap(_idxW, _idxB); } // swap W & B
    if (wSwap == WSWAP_G) { std::swap(_idxW, _idxG); } // swap W & G
    if (wSwap == WSWAP_R) { std::swap(_idxW, _idxR); } // swap W & R
  } else if (numChannels >= CHANNELS_CCT) {
    const uint8_t wSwap = co >> 4;
    if (wSwap == WSWAP_WWCW) { std::swap(_idxW, _idxCW); } // swap WW & CW
  }

  // 16-bit chips: two wire bytes per logical channel; lower nibble stores wire bytes.
  if (ledType == TYPE_UCS8903 || ledType == TYPE_UCS8904 || ledType == TYPE_SM16825) {
    flags |= NCHF_16BIT;
    numChannels *= 2; // 16bit buses use two bytes per color channel
  }
  _pixelFormat  = numChannels | flags;

}

// channelMap[i]: ChannelSource value (CH_UNUSED/CH_R/CH_G/CH_B/CH_W/CH_WW/CH_CW)
ColorEncoder::ColorEncoder(const uint8_t channelMap[MAX_CUSTOM_CHANNELS], uint8_t numChannels, uint8_t invertMask, bool is16bit)
{
  memcpy(_channelMap, channelMap, MAX_CUSTOM_CHANNELS);
  _invertMask = invertMask;
  _idxR = _idxG = _idxB = _idxW = _idxCW = 0; // unused in custom path
  uint8_t flags = NCHF_CUSTOM;
  if (is16bit) {
    flags |= NCHF_16BIT;
    numChannels *= 2; // 2 wire bytes per logical channel in 16-bit mode
  }
  if (invertMask) flags |= NCHF_INVERT; // set invert flag so encodeGeneric is always called for custom
  _pixelFormat = numChannels | flags;
}

// Generic encoder for non-fast-path cases: NCHF_INVERT, NCHF_CUSTOM,
// and any 16-bit + invert combination.
void ColorEncoder::encodeGeneric(uint32_t c, const CctPixel& cct, uint8_t* out, uint8_t bri) const {
  const uint8_t flags = _pixelFormat & 0xF0;
  const uint8_t logCh = _pixelFormat & 0x0F;

  if (flags & NCHF_CUSTOM) {
    // Custom channel map: each wire byte is assigned a color source from _channelMap
    // logCh is wire bytes (numChannels * 2 for 16-bit, numChannels otherwise)
    const bool b16 = (flags & NCHF_16BIT) != 0;
    const uint8_t numLogi = b16 ? logCh / 2 : logCh;
    for (uint8_t i = 0; i < numLogi; i++) {
      uint8_t val;
      switch (_channelMap[i]) {
        case CH_R:  val = getR(c); break;
        case CH_G:  val = getG(c); break;
        case CH_B:  val = getB(c); break;
        case CH_W:  val = getW(c); break;
        case CH_WW: val = cct.ww;  break;
        case CH_CW: val = cct.cw;  break;
        default:    val = 0;      break; // CH_UNUSED
      }
      if (_invertMask & (1u << i)) val ^= 0xFF;
      if (b16) {
        const uint16_t v16 = (uint16_t)val * bri;
        out[i*2]   = v16 >> 8;
        out[i*2+1] = v16 & 0xFF;
      } else {
        out[i] = val;
      }
    }
    return;
  }

  // NCHF_INVERT, optionally combined with NCHF_16BIT
  // logCh is wire bytes: 6/8/10 for 16-bit, 3/4/5 for 8-bit
  if (flags & NCHF_16BIT) {
    switch (logCh) {
      case WB_RGB16:  encodeRGB16(c, out, bri);       break;
      case WB_RGBW16: encodeRGBW16(c, out, bri);      break;
      default:        encodeCCT16(c, cct, out, bri);  break; // WB_CCT16
    }
  } else {
    switch (logCh) {
      case WB_RGB:  encodeRGB(c, out);        break;
      case WB_RGBW: encodeRGBW(c, out);       break;
      default:      encodeCCT(c, cct, out);  break; // WB_CCT
    }
  }
  // Apply invert mask; logCh already equals wire bytes
  for (uint8_t i = 0; i < logCh; i++) {
    if (_invertMask & (1u << i)) out[i] ^= 0xFF;
  }
}

uint32_t ColorEncoder::decodeGeneric(const uint8_t* in) const {
  const uint8_t flags = _pixelFormat & 0xF0;
  const uint8_t logCh = _pixelFormat & 0x0F;

  if (flags & NCHF_CUSTOM) {
    // Custom channel map decode: reconstruct RGBW from wire bytes
    const bool b16 = (flags & NCHF_16BIT) != 0;
    const uint8_t numLogi = b16 ? logCh / 2 : logCh;
    uint8_t r = 0, g = 0, b_ = 0, w = 0;
    for (uint8_t i = 0; i < numLogi; i++) {
      uint8_t val = b16 ? in[i*2] : in[i]; // take high byte for 16-bit
      if (_invertMask & (1u << i)) val ^= 0xFF;
      switch (_channelMap[i]) {
        case CH_R: r  = val; break;
        case CH_G: g  = val; break;
        case CH_B: b_ = val; break;
        case CH_W: case CH_WW: case CH_CW: // W, WW, CW → map to W (lossy)
          if (val > w) w = val; break;
      }
    }
    return makeColor(r, g, b_, w);
  }

  // NCHF_INVERT, optionally combined with NCHF_16BIT
  // logCh is wire bytes: 6/8/10 for 16-bit, 3/4/5 for 8-bit
  uint8_t r, g, b, w = 0;
  if (flags & NCHF_16BIT) {
    r = readU16Hi(in, _idxR); g = readU16Hi(in, _idxG); b = readU16Hi(in, _idxB);
    if (logCh >= WB_RGBW16) w = readU16Hi(in, _idxW);   // WB_RGBW16=RGBW16, WB_CCT16=CCT16
  } else {
    r = in[_idxR]; g = in[_idxG]; b = in[_idxB];
    if (logCh >= WB_RGBW) w = in[_idxW];              // WB_RGBW=RGBW, WB_CCT=CCT
  }
  if (flags & NCHF_INVERT) {
    if (_invertMask & (1u << _idxR)) r ^= 0xFF;
    if (_invertMask & (1u << _idxG)) g ^= 0xFF;
    if (_invertMask & (1u << _idxB)) b ^= 0xFF;
    const bool hasW = (flags & NCHF_16BIT) ? (logCh >= WB_RGBW16) : (logCh >= WB_RGBW);
    if (hasW && (_invertMask & (1u << _idxW))) w ^= 0xFF;
  }
  return makeColor(r, g, b, w);
}


//==============================================================================
// Bus Factory Implementation
//==============================================================================

PixelBus* createBus(BusDriver driver, int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType, size_t numPixels) {

  PixelBus* bus = nullptr;
  // TODO: need to re-order the colorOrder here to match how NPB is mapping the colors or user configs will be wrong. NPB uses IC datasheet orders (to be confirmed)
  // for example, SM16825 is RGBWY order on the chip.
  switch (driver) {
#if defined(ESP32)
    case BusDriver::RMT:
      bus = new RmtBus(pin, timing, colorOrder, numChannels, ledType);
      break;

#ifdef WLEDPB_I2S_SUPPORT
    case BusDriver::I2S:
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #error C3 hardware does not support parallel I2S output and single channel output is not implemented, use WLEDPB_PARALLEL_SPI_SUPPORT instead
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 0, ledType, numPixels);
#else
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 1, ledType, numPixels);
#endif
      break;
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
    case BusDriver::SPI:
      bus = new ParallelSpiBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#endif
#ifdef WLEDPB_PARLIO_SUPPORT
    case BusDriver::PARLIO:
      // ParlioBus registers itself with the shared ParlioBusContext singleton.
      // numPixels is used for DMA buffer sizing.
      bus = new ParlioBus(pin, timing, colorOrder, numChannels, 0, ledType, numPixels);
      break;
#endif
#if (WLED_MAX_BB_CHANNELS > 0)
    case BusDriver::BitBang:
      bus = new BitBangBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#endif
#elif defined(ESP8266)
    case BusDriver::UART:
      bus = new Esp8266UartBus(pin, timing, colorOrder, numChannels, ledType);
      break;
    case BusDriver::DMA:
      bus = new Esp8266DmaBus(pin, timing, colorOrder, numChannels, ledType);
      break;
    case BusDriver::BitBang:
      bus = new BitBangBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#elif (WLED_MAX_BB_CHANNELS > 0)
    // remaining ESP32 variants (C5/C6/C61/P4): BitBang is the only supported driver so far
    case BusDriver::BitBang:
      bus = new BitBangBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#endif

    default:
      return nullptr;
  }

  // Chip-specific post-creation configuration.
  // Must be done before begin() so allocateEncodeBuffer() reserves the correct space.
  if (bus) {
    switch (ledType) {
      case TYPE_TM1814:
      case TYPE_TM1815:
        bus->setPrefixLen(TM1814_PREFIX_LEN); // C1+C2 current-config prefix; updated per-frame in BusDigital::setBrightness()
        bus->setInverted(true);
        break;
      case TYPE_TM1914:
        bus->setPrefixLen(TM1914_PREFIX_LEN); // mode-setting prefix; written once in BusDigital::begin()
        bus->setInverted(true);
        break;
      case TYPE_SM16825:
        bus->setSuffixLen(SM16825_SUFFIX_LEN); // per-frame configuration suffix; defaults written by allocateEncodeBuffer()
        break;
      default:
        break;
    }
  }

  return bus;
} // createBus

} // namespace WLEDpixelBus

