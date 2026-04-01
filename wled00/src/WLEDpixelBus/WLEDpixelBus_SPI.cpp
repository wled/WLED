#include "WLEDpixelBus.h"

namespace WLEDpixelBus {

SpiBus::SpiBus(int8_t dataPin, int8_t clockPin, const LedTiming& timing, ColorOrder order, bool useHardwareSpi)
  : _dataPin(dataPin), _clockPin(clockPin), _timing(timing), _order(order), 
    _useHardware(useHardwareSpi), _initialized(false) {
}

SpiBus::~SpiBus() {
  end();
}

bool SpiBus::begin() {
  if (_initialized) return true;
  
  if (_useHardware) {
    SPI.begin();
    // Assuming around 2MHz for now - in reality this would use timing parameters
    SPI.setFrequency(2000000);
    SPI.setDataMode(SPI_MODE0);
  } else {
    pinMode(_dataPin, OUTPUT);
    pinMode(_clockPin, OUTPUT);
    digitalWrite(_clockPin, LOW);
    digitalWrite(_dataPin, LOW);
  }
  
  _initialized = true;
  return true;
}

void SpiBus::end() {
  if (!_initialized) return;
  if (_useHardware) {
    SPI.end();
  } else {
    pinMode(_dataPin, INPUT);
    pinMode(_clockPin, INPUT);
  }
  _initialized = false;
}

void SpiBus::sendByte(uint8_t d) {
  if (_useHardware) {
    SPI.transfer(d);
  } else {
    // Bitbang SPI
    for (uint8_t i = 0; i < 8; i++) {
      if (d & 0x80) {
        digitalWrite(_dataPin, HIGH);
      } else {
        digitalWrite(_dataPin, LOW);
      }
      digitalWrite(_clockPin, HIGH);
      d <<= 1;
      digitalWrite(_clockPin, LOW);
    }
  }
}

void SpiBus::sendStartFrame() {
  for (int i = 0; i < 4; i++) {
    sendByte(0x00);
  }
}

void SpiBus::sendEndFrame() {
  // Basic end frame for APA102
  for (int i = 0; i < 4; i++) {
    sendByte(0xFF);
  }
}

bool SpiBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  if (!pixels) pixels = _pixelData;
  if (numPixels == 0) numPixels = _numPixels;
  if (!_initialized || !pixels || _numPixels == 0) return false;

  // Output generic APA102/DotStar format
  sendStartFrame();

  for (uint16_t i = 0; i < _numPixels; i++) {
    uint32_t c = pixels[i];
    
    // For APA102: Global brightness + RGB (0xFF is max brightness)
    uint8_t r = getR(c);
    uint8_t g = getG(c);
    uint8_t b = getB(c);
    
    // Start byte contains 3 bits of 1 and 5 bits of global brightness (could be dynamic)
    sendByte(0xFF); 
    
    // Output according to ColorOrder
    switch (_order) {
      case ColorOrder::RGB: sendByte(r); sendByte(g); sendByte(b); break;
      case ColorOrder::GRB: sendByte(g); sendByte(r); sendByte(b); break;
      case ColorOrder::BRG: sendByte(b); sendByte(r); sendByte(g); break;
      case ColorOrder::RBG: sendByte(r); sendByte(b); sendByte(g); break;
      case ColorOrder::GBR: sendByte(g); sendByte(b); sendByte(r); break;
      case ColorOrder::BGR: sendByte(b); sendByte(g); sendByte(r); break;
      default: sendByte(b); sendByte(g); sendByte(r); break; // Default APA102 BGR
    }
  }

  sendEndFrame();

  return true;
}

bool SpiBus::canShow() const {
  return true;
}

} // namespace WLEDpixelBus
