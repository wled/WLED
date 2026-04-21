#ifdef WLED_ENABLE_DMX_OUTPUT

#include "wled.h"
#include "dmx_output.h"
#ifdef ESP8266
#include "uart.cpp"
#else
#include "hal/uart_ll.h"
#endif

bool DMXOutput::init(uint8_t outputPin, uint8_t updateRate, int8_t uartNo) {

  // If already initialized, users have to call end() first. We won't do it for them.
  if(_uartNo > 0)
    return false;

  #ifdef ESP8266
  if(uartNo == -1) uartNo = 1;
  if((uartNo != 1) || (outputPin != 2)) {
    DEBUG_PRINTF_P(PSTR("DMXOutput: Can only run with UART1, TX pin 2 on ESP8266."));
    return false;
  }
  #else //not ESP8266
  #if SOC_UART_NUM <= 1
  #error DMX output is not possible on your MCU, as it does not have HardwareSerial(1)
  #endif

  if(uartNo == -1) {
    uartNo = SOC_UART_NUM - 1;    // use last UART as default
  }
  if(uartNo == 0) {
    DEBUG_PRINTF_P(PSTR("DMXOutput: Error: Cannot run on chips with <=1 hardware UART, or with UART0."));
    return false;
  }
  #endif //ESP8266 or ESP32

  if(outputPin < 1) return false;
  const bool pinAllocated = PinManager::allocatePin(outputPin, true, PinOwner::DMX_OUTPUT);
  if(!pinAllocated) {
    DEBUG_PRINTF_P(PSTR("DMXOutput: Error: Failed to allocate pin %d for DMX output\n"), outputPin);
    return false;
  }
  DEBUG_PRINTF_P(PSTR("DMXOutput: init: pin %d\n"), outputPin);

  digitalWrite(outputPin, 1);
  pinMode(outputPin, OUTPUT);
  _outputPin = outputPin;
  _updateRate = updateRate;
  _dmxSerial = new HardwareSerial(uartNo);
  _uartNo = uartNo;

  #ifdef ESP8266
  // Sadly no TX buffer. But at least still a TX FIFO.
  _dmxSerial->begin(DMXSPEED, DMXFORMAT, SERIAL_TX_ONLY, outputPin);
  #else
  // DMX_CHANNELS + SOC_UART_FIFO_LEN is the minimum that leads to full async operation. Don't ask me why.
  _dmxSerial->setTxBufferSize(DMX_CHANNELS + SOC_UART_FIFO_LEN);  //641 = 1ms, 600 = 6ms, 514 = 10ms, 513-SOC_UART_FIFO_LEN=385 = 10ms, 185 = 17ms
  _dmxSerial->begin(DMXSPEED, DMXFORMAT, -1, outputPin);
  #endif

  return true;
}

void DMXOutput::end() {
  if(_uartNo >= 0) {
    #ifdef ESP8266
    _dmxSerial->end();
    #endif
    delete _dmxSerial;    // end() is implied in delete
    pinMode(_outputPin, INPUT);
    _uartNo = -1;
  }
}

DMXOutput::~DMXOutput() {
  end();
}

void DMXOutput::write(uint16_t channel, uint8_t value) {
  if(channel > DMX_CHANNEL_TOP) return; // out of bounds
  _dmxData[channel] = value;
}

void DMXOutput::writeBytes(uint16_t channelStart, uint8_t values[], uint16_t len) {
  if(channelStart == 0) return;     // channel 0 is no valid start channel, because it is special function
  for(int i = 0; i < len; i++) {
    if(channelStart + i > DMX_CHANNEL_TOP) break;   // finish when we reached the DMX channel 512
    write(channelStart + i, values[i]);
  }
}

uint8_t DMXOutput::read(uint16_t channel) {
  if(channel > DMX_CHANNEL_TOP) return 0;   // out of bounds
  return _dmxData[channel];
}

bool DMXOutput::readBytes(uint16_t channelStart, uint8_t values[], uint16_t len) {
  if(channelStart + len > DMX_CHANNELS) return false;   // out of bounds

  memcpy(values, &_dmxData[channelStart], len);
  return true;
}

bool DMXOutput::update() {
  // false if not properly initialized
  if(_uartNo < 0) return false;

  // Rate limiting & only send dmx frame if no other frame is just ongoing i.e. TXbuf is empty
  if((timeToNextUpdate() <= 0) && !busy()) {
    _lastDmxOutMillis = millis();

    // Send DMX break
    // Cannot change UART format while running. End and reinit takes much longer than the additional stopbit here.
    _dmxSerial->updateBaudRate(BREAKSPEED); //change to DMX break settings
    _dmxSerial->write(0);
    _dmxSerial->flush();

    // Send DMX data
    _dmxSerial->updateBaudRate(DMXSPEED);   //change to regular DMX speed
    _dmxSerial->write(_dmxData, DMX_CHANNELS);

    return true;
  }
  return false;
}

bool DMXOutput::busy() {
  if(_uartNo < 0) return true;   // not initialized

  #ifdef ESP8266
  if(uart_tx_fifo_available(_uartNo) == 0) {
    // according to uart.cpp this is buggy and actually we have to wait for one transmission (11 baud) after this
    // indicates TX is free. This makes every call take 45us which seems acceptable.
    delayMicroseconds(11 * 1000000 / DMXSPEED + 1);
    return false;
  } else
    return true;
  #else
  // not busy if tx idle. HardwareSerial.availableForWrite() didn't work reliable.
  return !uart_ll_is_tx_idle(UART_LL_GET_HW(_uartNo));
  #endif
}

unsigned long DMXOutput::getLastDmxOut() {
  return _lastDmxOutMillis;
}

void DMXOutput::setUpdateRate(uint8_t updateRate) {
  _updateRate = updateRate;
}
uint8_t DMXOutput::getUpdateRate() {
  return _updateRate;
}

int DMXOutput::timeToNextUpdate() {
  if(_uartNo < 0) return INT_MAX;   // not initialized
  if(_updateRate == 0) return -1;   // if refresh rate set to 0, refresh rate is max.

  // treat _updateRate as maximum, so round up the refresh delay
  float fdmxFrameTime = 1000.0 / _updateRate;
  int dmxFrameTime = (uint16_t)fdmxFrameTime;
  if(fdmxFrameTime - dmxFrameTime > 0.0) dmxFrameTime += 1;   // if fractional part > 0, add one

  return dmxFrameTime - (millis() - _lastDmxOutMillis);
}

bool DMXOutput::handleDMXOutput() {
  // don't act, when in DMX Proxy mode
  if (e131ProxyUniverse != 0) return false;

  uint8_t brightness = strip.getBrightness();

  bool calc_brightness = true;

   // check if no shutter channel is set
   for (unsigned i = 0; i < DMXChannels; i++) {
     if (DMXFixtureMap[i] == 5) calc_brightness = false;
   }

  uint16_t len = strip.getLengthTotal();
  uint16_t maxLen = (DMX_CHANNELS - DMXStart) / DMXGap;     // maximum LEDs that fit into one physical DMX512 universe
  if (len > maxLen) len = maxLen;

  for (int i = DMXStartLED; i < len; i++) {        // uses the amount of LEDs as fixture count

    uint32_t in = strip.getPixelColor(i);     // get the colors for the individual fixtures as suggested by Aircoookie in issue #462
    byte w = W(in);
    byte r = R(in);
    byte g = G(in);
    byte b = B(in);

    int DMXFixtureStart = DMXStart + (DMXGap * (i - DMXStartLED));
    for (int j = 0; j < DMXChannels; j++) {
      int DMXAddr = DMXFixtureStart + j;
      switch (DMXFixtureMap[j]) {
        case 0:        // Set this channel to 0
          write(DMXAddr, 0);
          break;
        case 1:        // Red
          write(DMXAddr, calc_brightness ? (r * brightness) / 255 : r);
          break;
        case 2:        // Green
          write(DMXAddr, calc_brightness ? (g * brightness) / 255 : g);
          break;
        case 3:        // Blue
          write(DMXAddr, calc_brightness ? (b * brightness) / 255 : b);
          break;
        case 4:        // White
          write(DMXAddr, calc_brightness ? (w * brightness) / 255 : w);
          break;
        case 5:        // Shutter channel. Controls the brightness.
          write(DMXAddr, brightness);
          break;
        case 6:        // Sets this channel to 255. Like 0, but more wholesome.
          write(DMXAddr, 255);
          break;
      }
    }
  }

  return update();        // update the DMX bus, if available
}

#endif
