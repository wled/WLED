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

#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
#include "WLEDpixelBus_RMT.h"
#include "WLEDpixelBus_I2S.h"
#include "WLEDpixelBus_LCD.h"
#include "WLEDpixelBus_ParallelSpi.h"
#endif

#if defined(WLEDPB_ESP8266)
#include "WLEDpixelBus_ESP8266.h"
#endif

namespace WLEDpixelBus {

//==============================================================================
// Color Encoder Implementation
//==============================================================================

ColorEncoder::ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType)
{
  _invertMask = 0;
  memset(_channelMap, 0, sizeof(_channelMap));

  // --- Standard types: fast path via _idxR/_idxG/_idxB/_idxW/_idxCW ---

  uint8_t flags = 0;

  // Decode RGB wire positions from color order lower nibble (0=GRB,1=RGB,2=BRG,3=RBG,4=BGR,5=GBR)
  uint8_t rPos, gPos, bPos;
  switch (co & 0x0F) {
    case 1:  rPos=0; gPos=1; bPos=2; break; // RGB
    case 2:  rPos=1; gPos=2; bPos=0; break; // BRG
    case 3:  rPos=0; gPos=2; bPos=1; break; // RBG
    case 4:  rPos=2; gPos=1; bPos=0; break; // BGR
    case 5:  rPos=2; gPos=0; bPos=1; break; // GBR
    default: rPos=1; gPos=0; bPos=2; break; // GRB (case 0)
  }
  _idxR = rPos; _idxG = gPos; _idxB = bPos;
  _idxW = 3; _idxCW = 4; // defaults, overridden below as needed

  // White/CCT channels: numChannels from bus type, W-swap from upper nibble
  if (numChannels == 4) {
    // LED-type-specific native wire order: TM1814 and TM1815 send W first.
    // Shift RGB indices up by 1 and place W at index 0 before applying user wSwap.
    if (ledType == TYPE_TM1814 || ledType == TYPE_TM1815) {
      _idxR++; _idxG++; _idxB++;
      _idxW = 0;
    }
    const uint8_t wSwap = co >> 4;
    if (wSwap == 1) { std::swap(_idxW, _idxB); } // swap W & B
    if (wSwap == 2) { std::swap(_idxW, _idxG); } // swap W & G
    if (wSwap == 3) { std::swap(_idxW, _idxR); } // swap W & R
  } else if (numChannels >= 5) {
    const uint8_t wSwap = co >> 4;
    if (wSwap == 4) { std::swap(_idxW, _idxCW); } // swap WW & CW
  }

  // 16-bit chips: two wire bytes per logical channel; lower nibble stores wire bytes.
  if (ledType == TYPE_UCS8903 || ledType == TYPE_UCS8904 || ledType == TYPE_SM16825) {
    flags |= NCHF_16BIT;
    numChannels *= 2; // 16bit buses use two bytes per color channel
  }
  _pixelFormat  = numChannels | flags;

}

// Custom channel map constructor for TYPE_CUSTOM_BUS.
// channelMap[i]: 0=Unused, 1=R, 2=G, 3=B, 4=W, 5=WW, 6=CW
ColorEncoder::ColorEncoder(const uint8_t channelMap[6], uint8_t numChannels, uint8_t invertMask, bool is16bit)
{
  memcpy(_channelMap, channelMap, 6);
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
        case 1: val = getR(c); break;
        case 2: val = getG(c); break;
        case 3: val = getB(c); break;
        case 4: val = getW(c); break;
        case 5: val = cct.ww;  break;
        case 6: val = cct.cw;  break;
        default: val = 0;      break; // Unused
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
      case 6:  encodeRGB16(c, out, bri);       break;
      case 8:  encodeRGBW16(c, out, bri);      break;
      default: encodeCCT16(c, cct, out, bri);  break; // 10
    }
  } else {
    switch (logCh) {
      case 3: encodeRGB(c, out);        break;
      case 4: encodeRGBW(c, out);       break;
      default: encodeCCT(c, cct, out);  break; // 5
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
        case 1: r  = val; break;
        case 2: g  = val; break;
        case 3: b_ = val; break;
        case 4: case 5: case 6: // W, WW, CW → map to W (lossy)
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
    if (logCh >= 8) w = readU16Hi(in, _idxW);   // 8=RGBW16, 10=CCT16
  } else {
    r = in[_idxR]; g = in[_idxG]; b = in[_idxB];
    if (logCh >= 4) w = in[_idxW];              // 4=RGBW, 5=CCT
  }
  if (flags & NCHF_INVERT) {
    if (_invertMask & (1u << _idxR)) r ^= 0xFF;
    if (_invertMask & (1u << _idxG)) g ^= 0xFF;
    if (_invertMask & (1u << _idxB)) b ^= 0xFF;
    const bool hasW = (flags & NCHF_16BIT) ? (logCh >= 8) : (logCh >= 4);
    if (hasW && (_invertMask & (1u << _idxW))) w ^= 0xFF;
  }
  return makeColor(r, g, b, w);
}


//==============================================================================
// Bus Factory Implementation
//==============================================================================

PixelBus* createBus(BusDriver driver, int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, size_t bufferSize, int8_t channel, uint8_t ledType) {

  PixelBus* bus = nullptr;

  switch (driver) {
#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
    case BusDriver::RMT:
      bus = new RmtBus(pin, timing, colorOrder, numChannels, channel, ledType);
      break;

#ifdef WLEDPB_I2S_SUPPORT
        case BusDriver::I2S:
#if defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32C3)
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 0, bufferSize, ledType);
#else
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 1, bufferSize, ledType);
#endif
      break;
#endif

#ifdef WLEDPB_LCD_SUPPORT
    case BusDriver::LCD:
      bus = new LcdBus(pin, timing, colorOrder, numChannels, bufferSize, false, ledType);
      break;
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
    case BusDriver::SPI:
      bus = new ParallelSpiBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#endif

    case BusDriver::BitBang:
      bus = new BitBangBus(pin, timing, colorOrder, numChannels, ledType);
      break;

#elif defined(WLEDPB_ESP8266)
    case BusDriver::UART:
      bus = new Esp8266UartBus(pin, timing, colorOrder, numChannels, ledType);
      break;
    case BusDriver::DMA:
      bus = new Esp8266DmaBus(pin, timing, colorOrder, numChannels, ledType);
      break;
    case BusDriver::BitBang:
      bus = new Esp8266BitBangBus(pin, timing, colorOrder, numChannels, ledType);
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
        bus->setPrefixLen(8); // 8-byte C1+C2 current-config prefix; updated per-frame in BusDigital::setBrightness()
        bus->setInverted(true);
        break;
      case TYPE_TM1914:
        bus->setPrefixLen(6); // 6-byte mode-setting prefix; written once in BusDigital::begin()
        bus->setInverted(true);
        break;
      case TYPE_SM16825:
        bus->setSuffixLen(4); // 4-byte per-frame configuration suffix; defaults written by allocateEncodeBuffer()
        break;
      //case TYPE_CUSTOM_BUS:
        //bus->setInverted(invert);  // TODO: need a parameter to set inverted for custom bus, UI already supports it but driver does not
      default:
        break;
    }
  }

  return bus;
} // createBus

} // namespace WLEDpixelBus

