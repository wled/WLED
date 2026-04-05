#include "WLEDpixelBus.h"

namespace WLEDpixelBus {

SpiBus::SpiBus(int8_t dataPin, int8_t clockPin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, bool useHardwareSpi, uint8_t ledType)
  : _dataPin(dataPin), _clockPin(clockPin), _timing(timing),
    _useHardware(useHardwareSpi), _initialized(false) {
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
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
  if (!allocateEncodeBuffer(_numPixels, _encoder.getNumChannels())) { end(); return false; }
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
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; _encodeBufferSize = 0; }
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

bool SpiBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;

  uint8_t numCh = _encoder.getNumChannels();

  sendStartFrame();
  for (uint16_t i = 0; i < _numPixels; i++) {
    sendByte(0xFF); // APA102 global brightness byte
    const uint8_t* src = _encodeBuffer + i * numCh;
    for (uint8_t ch = 0; ch < numCh; ch++) sendByte(src[ch]);
  }
  sendEndFrame();

  return true;
}

void SpiBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getLogicalChannels(), _ledType);
}

bool SpiBus::canShow() const {
  return true;
}

} // namespace WLEDpixelBus
