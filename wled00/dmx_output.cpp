#include "wled.h"
#include "dmx_output.h"
/*
 * Support for DMX output via serial (e.g. MAX485).
 * ESP8266 Library from:
 * https://github.com/Rickgg/ESP-Dmx
 * ESP32 Library from:
 * https://github.com/sparkfun/SparkFunDMX
 */

#ifdef WLED_ENABLE_DMX_OUTPUT

bool handleDMXOutput()
{
  // don't act, when in DMX Proxy mode
  if (e131ProxyUniverse != 0) return 0;

  uint8_t brightness = strip.getBrightness();

  bool calc_brightness = true;

   // check if no shutter channel is set
   for (unsigned i = 0; i < DMXChannels; i++)
   {
     if (DMXFixtureMap[i] == 5) calc_brightness = false;
   }

  uint16_t len = strip.getLengthTotal();
  uint16_t maxLen = (DMX_CHANNEL_TOP - DMXStart) / DMXGap;     // maximum LEDs that fit into one physical DMX512 universe
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
        case 0:        // Set this channel to 0. Good way to tell strobe- and fade-functions to fuck right off.
          dmx.write(DMXAddr, 0);
          break;
        case 1:        // Red
          dmx.write(DMXAddr, calc_brightness ? (r * brightness) / 255 : r);
          break;
        case 2:        // Green
          dmx.write(DMXAddr, calc_brightness ? (g * brightness) / 255 : g);
          break;
        case 3:        // Blue
          dmx.write(DMXAddr, calc_brightness ? (b * brightness) / 255 : b);
          break;
        case 4:        // White
          dmx.write(DMXAddr, calc_brightness ? (w * brightness) / 255 : w);
          break;
        case 5:        // Shutter channel. Controls the brightness.
          dmx.write(DMXAddr, brightness);
          break;
        case 6:        // Sets this channel to 255. Like 0, but more wholesome.
          dmx.write(DMXAddr, 255);
          break;
      }
    }
  }

  #if defined(ESP8266)
  dmx.update();        // update the DMX bus
  return true;
  #else
  return dmx.update();        // update the DMX bus, if available
  #endif
}

void initDMXOutput(int outputPin) {
  if (outputPin < 1) return;
  const bool pinAllocated = PinManager::allocatePin(outputPin, true, PinOwner::DMX);
  if (!pinAllocated) {
    DEBUG_PRINTF_P(PSTR("DMXOutput: Error: Failed to allocate pin %d for DMX output\n"), outputPin);
    return;
  }
  DEBUG_PRINTF_P(PSTR("DMXOutput: init: pin %d\n"), outputPin);
  dmx.init(outputPin);        // set output pin and initialize DMX output
}

void DMXOutput::init(uint8_t outputPin) {
  _dmx.initWrite(outputPin, 512);
}

void DMXOutput::write(int channel, uint8_t value) {
  _dmx.write(channel, value);
}

void DMXOutput::update() {
  _dmx.update();
}
#else
void initDMXOutput(int){}
bool handleDMXOutput() {}
#endif
