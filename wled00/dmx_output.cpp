#include "wled.h"
#include "dmx_output.h"
/*
 * Support for DMX output via serial (e.g. MAX485).
 * ESP8266 Library from:
 * https://github.com/Rickgg/ESP-Dmx
 * ESP32 Library from:
 * https://github.com/someweisguy/esp_dmx
 */

#ifdef WLED_ENABLE_DMX

void handleDMXOutput()
{
  // don't act, when in DMX Proxy mode
  if (e131ProxyUniverse != 0) return;

  uint8_t brightness = strip.getBrightness();

  bool calc_brightness = true;

   // check if no shutter channel is set
   for (unsigned i = 0; i < DMXChannels; i++)
   {
     if (DMXFixtureMap[i] == 5) calc_brightness = false;
   }

  uint16_t len = strip.getLengthTotal();
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

  dmx.update();        // update the DMX bus
}

void initDMXOutput(int outputPin) {
  if (outputPin < 1) return;
  const bool pinAllocated = PinManager::allocatePin(outputPin, true, PinOwner::DMX_OUTPUT);
  if (!pinAllocated) {
    DEBUG_PRINTF("DMXOutput: Error: Failed to allocate pin for DMX_OUTPUT. Pin already in use:\n");
    DEBUG_PRINTF("In use by: %s\n", PinManager::getPinOwner(outputPin));
    return;
  }
  dmx.init(outputPin);        // set output pin and initialize DMX output
}

#if !defined(ESP8266)
void DMXOutput::init(uint8_t outputPin) {
  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_driver_install(dmxPort, &config, DMX_INTR_FLAGS_DEFAULT);
  dmx_set_pin(dmxPort, outputPin, -1, -1);
}
void DMXOutput::write(uint8_t channel, uint8_t value) {
  dmxdata[channel] = value;
}
void DMXOutput::update() {
  dmx_write(dmxPort, dmxdata, DMX_PACKET_SIZE);
  dmx_send(dmxPort, DMX_PACKET_SIZE);
}
#endif


#else
void initDMXOutput(){}
void handleDMXOutput() {}
#endif
