/*-------------------------------------------------------------------------
WLEDpixelBus - Lightweight LED driver library for WLED

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna):
NeoPixelBus served me well as a reference to proper hardware initialisation
as well as figuring out the more exotic digital LED types

Features:
- Runtime LED timing configuration
- Support for ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-C5 and ESP8266 (maybe other ESP32 flavours work too)
- RMT, I2S parallel, LCD parallel, SPI parallel, BitBang parallel, PARLIO parallel
- 2-Pin LED support (hardware SPI and BitBang)
- RGBW uint32_t pixel buffer format (WLED native)
- Support for up to 6 color channels, 8bit or 16bit
- Supports output inversion (ESP32 only) and individual color channel inversion
- Supports hardware LED brightness for improved color resolution if available

Tested working on IDF V4 and V5 API functions and low-level HAL

-------------------------------------------------------------------------*/

/*
TODO List
- need to verify custom bus start indices and color-order overrides on hardware (both should now work)
- ESP32 C61 has no RMT and no PARLIO, either bit bang or check if parallel SPI can be used
- I2S/LCD requires more stress-testing to ensure glitch-free outputs, for 16-parallel maybe even 4 buffers are needed under heavy load
- the DMA buffer count and size need to be checked agains memory usage formulas, they may be incorrect (we could do "worst case" in the UI as it wont matter much for a small amount of LEDs)
- stress-test all parallel drivers for missed interrupts, descriptor races, and watchdog recovery
- LOOPTEST_CYCLES in BB bus need fine-tuning
- color orders of some strips may differ from NPB implementation, needs checking and reordering to match legacy behaviour or users will complain (see legacy bus_wrapper where this is defined in neoXXXtype)
- move gamma down to bus level, see pending PR
- brightness scaling for special buses with LED current (or 16bit) may need fine-tuning
- SPI 2-wire bus types need testing on all platforms (tested working in the past but not recently)
- SPI 2-wire types do not support signal inversion (_inverted flag)
- SPI-2-wire Optimisation: C3 (maybe also C5, C6?) can output much faster if using CPU pin groups i.e. direct CPU to GPIO access, would speed up SPI BB outputs
- ESP32_DATA_IDLE_HIGH flag is a hack to fix bad hardware design and uses the legacy RMT driver, we should drop support for that
- parallel SPI driver: the reset pulse is now derived from the LED timing (calc4StepResetDmaBytes); needs testing on hardware
- PARLIO bus needs further target-hardware testing
- use chip capabilities instead of individual target ifdefs in buswrapper
- SM16825_SUFFIX should be made dynamic to increase color range at lower brightness (scale with global brightness)
- The config rework is unfinished: with the updated workflow the goal is to fully drop "custom" bus handling and tread every bus the same (UI needs rework to do that) -> done now, needs testing and refinement
- RMT driver has no canShow() timeout yet, could adopt the "ensureIdle()" approach from I2S/PARLIO
- the bus capability definitions in const.h should probably be moved to driver level or use driver level constants instead of "overriding" low level hardware capabilities
- LPD6803 and LPD8806 encoding is done on RGB only, need to add support for more color channels if such stripes exist
- PARLIO uses software channel inversion, can we use GPIO inversion like in other drivers? code for testing is in place but commented out, see hwInit() in WLEDpixelBus_PARLIO.cpp
- ESP8266 DMA output uses a slow path to scale and reset buffers, needs optimisation
- the "non global buffer" path is not optimised, check if this path should not color-encode on the fly so reading buffer back is faster, then do an encode pass before sending
- I2S bus and PARLIO use identical 4-step encoding, should use a common function. I2S 16-parallel out still is only used by I2S -> move to WLEDpixelBus.h
- The 4-step helper functions in WLEDpixelBus_Timings.h should also be moved to WLEDpixelBus.h for common use just like the encoding functions
- WLEDpixelBus_features.h now only contains current steps - should also be moved into WLEDpixelBus.h and removed
- ColorEncoder has two overloaded versions, the non mapped one is not really used anymore as mapping is done by legacy shims - should be removed after checking if SPI bus is handled correctly if doing so
*/


#include "WLEDpixelBus.h"
#include "../../const.h" // needed for color orders

namespace WLEDpixelBus {

// LED Timing Lookup
LedTiming getLedTiming(uint8_t wledType) {
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

// Color Encoder
//===============

// standard encoder from color order and type
ColorEncoder::ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType)
{
  _invertMask = 0;
  memset(_channelMap, 0, sizeof(_channelMap));

  // Standard types: fast path via _idxR/_idxG/_idxB/_idxW/_idxCW
  uint8_t flags = 0;

  // Decode RGB wire positions from color order lower nibble (see ColorOrder enum)
  uint8_t rPos, gPos, bPos;
  switch (co & 0x0F) {
    case COL_ORDER_RGB: rPos=0; gPos=1; bPos=2; break;
    case COL_ORDER_BRG: rPos=1; gPos=2; bPos=0; break;
    case COL_ORDER_RBG: rPos=0; gPos=2; bPos=1; break;
    case COL_ORDER_BGR: rPos=2; gPos=1; bPos=0; break;
    case COL_ORDER_GBR: rPos=2; gPos=0; bPos=1; break;
    default:        rPos=1; gPos=0; bPos=2; break; // COL_ORDER_GRB
  }
  _idxR = rPos; _idxG = gPos; _idxB = bPos;
  _idxW = 3; _idxCW = 4; // defaults, overridden below as needed

  // White/CCT channels: numChannels from bus type, W-swap from upper nibble
  if (numChannels == CHANNELS_RGBW) {
    if (ledType == TYPE_TM1814 || ledType == TYPE_TM1815) {
      _idxR++; _idxG++; _idxB++; // Shift RGB indices up by 1 and place W at index 0 before applying wSwap
      _idxW = 0; // TM1814 and TM1815 send W first
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
    numChannels *= 2; // 16bit: two bytes per color channel
  }
  _pixelFormat  = numChannels | flags;

}

// custom encoder from channel map, if a standard permutation is detected and no inversion it uses fast path
ColorEncoder::ColorEncoder(const uint8_t channelMap[MAX_CUSTOM_CHANNELS], uint8_t numChannels, uint8_t invertMask, bool is16bit)
{
  if (numChannels > MAX_CUSTOM_CHANNELS) numChannels = MAX_CUSTOM_CHANNELS;
  memcpy(_channelMap, channelMap, MAX_CUSTOM_CHANNELS);
  _invertMask = invertMask;

  // Try to detect a standard permutation usable on the fast inline encode/decode path
  // Requirements: no inversion, each color source appears at most once, channel count {3,4,5}
  bool isPermutation = (invertMask == 0) && (numChannels >= 3) && (numChannels <= 5);
  uint8_t rIdx = 0, gIdx = 1, bIdx = 2, wIdx = 3, cwIdx = 4;
  bool hasR = false, hasG = false, hasB = false, hasW = false, hasCW = false;

  if (isPermutation) {
    for (uint8_t i = 0; i < numChannels; i++) {
      switch (channelMap[i]) {
        case CH_R:    if (hasR)  isPermutation = false; else { hasR  = true; rIdx  = i; } break;
        case CH_G:    if (hasG)  isPermutation = false; else { hasG  = true; gIdx  = i; } break;
        case CH_B:    if (hasB)  isPermutation = false; else { hasB  = true; bIdx  = i; } break;
        case CH_W:    // fallthrough
        case CH_WW:   if (hasW)  isPermutation = false; else { hasW  = true; wIdx  = i; } break;
        case CH_CW:   if (hasCW) isPermutation = false; else { hasCW = true; cwIdx = i; } break;
        default:      isPermutation = false; break; // CH_UNUSED or unknown: not a fast-path permutation
      }
      if (!isPermutation) break;
    }
    // Validate that the present sources match the expected fast-path dispatch case exactly
    if (numChannels == 3) {
      // encodeRGB: exactly R+G+B, no W/CW
      if (!hasR || !hasG || !hasB || hasW || hasCW) isPermutation = false;
    } else if (numChannels == 4) {
      // encodeRGBW: R+G+B+W where W must be CH_W (not CH_WW — encodeRGBW uses W(c))
      if (!hasR || !hasG || !hasB || !hasW || hasCW) isPermutation = false;
      else if (channelMap[wIdx] != CH_W) isPermutation = false;
    } else if (numChannels == 5) {
      // encodeCCT: R+G+B+WW+CW; _idxW stores the WW position, _idxCW the CW position
      if (!hasR || !hasG || !hasB || !hasW || !hasCW) isPermutation = false;
      else if (channelMap[wIdx] != CH_WW) isPermutation = false;
    } else {
      isPermutation = false; // unsupported count for fast path
    }
  }

  uint8_t flags = 0;
  if (is16bit) {
    flags |= NCHF_16BIT;
    numChannels *= 2; // 2 wire bytes per logical channel in 16-bit mode
  }
  if (isPermutation) {
    // Standard permutation: populate fast-path indices, skip NCHF_CUSTOM
    _idxR = rIdx; _idxG = gIdx; _idxB = bIdx; _idxW = wIdx; _idxCW = cwIdx;
    _pixelFormat = numChannels | flags;
  } else {
    // True custom map: use generic encodeGeneric() loop
    _idxR = _idxG = _idxB = _idxW = _idxCW = 0;
    flags |= NCHF_CUSTOM;
    _pixelFormat = numChannels | flags;
  }
}

// Generic encoder for non-fast-path cases: NCHF_CUSTOM maps, and inverted channels (hasInvertedChannels()) in any bit mode
void ColorEncoder::encodeGeneric(uint32_t c, const CctPixel& cct, uint8_t* out, uint8_t bri) const {
  const uint8_t flags     = _pixelFormat & 0xF0;
  const uint8_t wireBytes = _pixelFormat & 0x0F; // total bytes per pixel (numChannels * 2 for 16-bit, numChannels otherwise)
  const bool is16bit = (flags & NCHF_16BIT) != 0; // cache for speed

  if (flags & NCHF_CUSTOM) {
    // Custom channel map: each wire byte is assigned a color source from _channelMap (up to 6 channels supported)
    const uint8_t numLogi = is16bit ? wireBytes / 2 : wireBytes;
    for (uint8_t i = 0; i < numLogi; i++) {
      uint8_t val;
      switch (_channelMap[i]) {
        case CH_R:  val = R(c); break;
        case CH_G:  val = G(c); break;
        case CH_B:  val = B(c); break;
        case CH_W:  val = W(c); break;
        case CH_WW: val = cct.ww;  break;
        case CH_CW: val = cct.cw;  break;
        default:    val = 0;      break; // CH_UNUSED
      }
      if (is16bit) {
        uint16_t v16 = (uint16_t)val * bri; // Note: this can "only" reach 99% as 255×255 = 65025. We do not care.
        if (_invertMask & (1u << i)) v16 ^= 0xFFFF; // invert all bits
        out[i*2]   = v16 >> 8;
        out[i*2+1] = v16 & 0xFF;
      } else {
        if (_invertMask & (1u << i)) val ^= 0xFF;
        out[i] = val; // note: for 8bit colors, scaling happens in BusDigital, only 16bit colors scale during encoding
      }
    }
    return;
  }

  // wire bytes: 6/8/10 for 16-bit, 3/4/5 for 8-bit (standard encoding supports up to 5 channels)
  if (is16bit) {
    switch (wireBytes) {
      case WB_RGB16:  encodeRGB16(c, out, bri);       break;
      case WB_RGBW16: encodeRGBW16(c, out, bri);      break;
      default:        encodeCCT16(c, cct, out, bri);  break; // WB_CCT16
    }
  } else {
    switch (wireBytes) {
      case WB_RGB:  encodeRGB(c, out);        break;
      case WB_RGBW: encodeRGBW(c, out);       break;
      default:      encodeCCT(c, cct, out);  break; // WB_CCT
    }
  }

  if (!hasInvertedChannels()) return;
  // Handle color inversion (per color channel)
  if (is16bit) {
    for (uint8_t i = 0; i < wireBytes / 2; i++) {
      if (_invertMask & (1u << i)) {
        out[i*2] ^= 0xFF;
        out[i*2+1] ^= 0xFF;
      }
    }
  } else {
    for (uint8_t i = 0; i < wireBytes; i++) {
      if (_invertMask & (1u << i)) out[i] ^= 0xFF;
    }
  }
}

uint32_t ColorEncoder::decodeGeneric(const uint8_t* in) const {
  const uint8_t flags     = _pixelFormat & 0xF0;
  const uint8_t wireBytes = _pixelFormat & 0x0F; // total bytes per pixel (numChannels * 2 for 16-bit, numChannels otherwise)
  const bool is16bit = (flags & NCHF_16BIT) != 0; // cache for speed

  if (flags & NCHF_CUSTOM) {
    // Custom channel map decode: reconstruct RGBW from wire bytes
    const uint8_t numLogi = is16bit ? wireBytes / 2 : wireBytes;
    uint8_t r = 0, g = 0, b_ = 0, w = 0;
    for (uint8_t i = 0; i < numLogi; i++) {
      uint8_t val = is16bit ? in[i*2] : in[i]; // take high byte for 16-bit
      if (_invertMask & (1u << i)) val ^= 0xFF;
      switch (_channelMap[i]) {
        case CH_R: r  = val; break;
        case CH_G: g  = val; break;
        case CH_B: b_ = val; break;
        case CH_W: case CH_WW: case CH_CW: // W, WW, CW → map to W (lossy)
          if (val > w) w = val; break;
      }
    }
    return RGBW32(r, g, b_, w);
  }

  uint8_t r, g, b, w = 0;
  if (is16bit) {
    r = readU16Hi(in, _idxR); g = readU16Hi(in, _idxG); b = readU16Hi(in, _idxB);
    if (wireBytes >= WB_RGBW16) w = readU16Hi(in, _idxW); // WB_RGBW16=RGBW16, WB_CCT16=CCT16
  } else {
    r = in[_idxR]; g = in[_idxG]; b = in[_idxB];
    if (wireBytes >= WB_RGBW) w = in[_idxW]; // WB_RGBW=RGBW, WB_CCT=CCT
  }
  if (hasInvertedChannels()) {
    if (_invertMask & (1u << _idxR)) r ^= 0xFF;
    if (_invertMask & (1u << _idxG)) g ^= 0xFF;
    if (_invertMask & (1u << _idxB)) b ^= 0xFF;
    const bool hasW = (is16bit) ? (wireBytes >= WB_RGBW16) : (wireBytes >= WB_RGBW);
    if (hasW && (_invertMask & (1u << _idxW))) w ^= 0xFF;
  }
  return RGBW32(r, g, b, w);
}

// create a 1-wire digital bus (RMT, I2S, ...)
PixelBus* createBus(BusDriver driver, int8_t pin, const BusProperties& properties, uint8_t ledType, size_t numPixels) {
  if (numPixels > UINT16_MAX) return nullptr;
  PixelBus* bus = nullptr;
  const LedTiming& timing = properties.timing;
  switch (driver) {
#if defined(ARDUINO_ARCH_ESP32)
    case BusDriver::RMT:
      bus = new RmtBus(pin, timing, ledType);
      break;

#ifdef WLEDPB_I2S_SUPPORT
    case BusDriver::I2S:
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #error C3 hardware does not support parallel I2S output and single channel output is not implemented, use WLEDPB_PARALLEL_SPI_SUPPORT instead
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
      bus = new I2sBus(pin, timing, 0, ledType);
#else
      bus = new I2sBus(pin, timing, 1, ledType);
#endif
      break;
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
    case BusDriver::SPI:
      bus = new ParallelSpiBus(pin, timing, ledType);
      break;
#endif
#ifdef WLEDPB_PARLIO_SUPPORT
    case BusDriver::PARLIO:
      // ParlioBus registers itself with the shared ParlioBusContext singleton.
      // numPixels is used for DMA buffer sizing.
      bus = new ParlioBus(pin, timing, 0, ledType);
      break;
#endif
#if (WLED_MAX_BB_CHANNELS > 0)
    case BusDriver::BitBang:
      bus = new BitBangBus(pin, timing, ledType);
      break;
#endif
#elif defined(ESP8266)
    case BusDriver::UART:
      bus = new Esp8266UartBus(pin, timing, ledType);
      break;
    case BusDriver::DMA:
      bus = new Esp8266DmaBus(pin, timing, ledType);
      break;
    case BusDriver::BitBang:
      bus = new BitBangBus(pin, timing, ledType);
      break;
#endif

    default:
      return nullptr;
  }

// we have a bus, set the parameters
  if (bus) {
    bus->setNumPixels((uint16_t)numPixels);
    bus->setPrefixLen(properties.prefixLen);
    bus->setPrefixData(properties.prefixData, properties.prefixLen);
    bus->setSuffixLen(properties.suffixLen);
    bus->setSuffixData(properties.suffixData, properties.suffixLen);
    bus->setInverted(properties.invertOutput);
    bus->setEncoder(ColorEncoder(properties.channelMap, properties.numChannels, properties.invertMask, properties.is16bit));
  }

  return bus;
} // createBus

} // namespace WLEDpixelBus
