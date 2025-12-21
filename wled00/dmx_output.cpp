#include "wled.h"

/*
 * Support for DMX  output via serial (e.g. MAX485).
 * Change the output pin in src/dependencies/ESPDMX.cpp, if needed (ESP8266)
 * Change the output pin in src/dependencies/dmx/EspDmxOutput.cpp, if needed (ESP32/S3)
 * ESP8266 Library from:
 * https://github.com/Rickgg/ESP-Dmx
 * ESP32 Library from:
 * https://github.com/someweisguy/esp_dmx
 */

#ifdef WLED_ENABLE_DMX

#define MAX_DMX_RATE 44 // max DMX update rate in Hz

void handleDMXOutput()
{
  // don't act, when in DMX Proxy mode
  if (e131ProxyUniverse != 0) return;

  // Ensure segments exist before accessing strip data
  if (strip.getSegmentsNum() == 0) return;

  // Rate limiting
  static unsigned long last_dmx_time = 0;
  const unsigned long dmxFrameTime = (1000UL + MAX_DMX_RATE - 1) / MAX_DMX_RATE;
  if (millis() - last_dmx_time < dmxFrameTime) return;

  uint8_t brightness = strip.getBrightness();
  
  // Skip DMX entirely if strip is off
  if (brightness == 0) return;

  last_dmx_time = millis();

  bool calc_brightness = true;

   // check if no shutter channel is set
   for (unsigned i = 0; i < DMXChannels; i++)
   {
     if (DMXFixtureMap[i] == 5) calc_brightness = false;
   }

  uint16_t len = strip.getLengthTotal();
  if (len == 0) return;
  
  // OPTIMIZATION: Only process the LEDs that actually need DMX output
  // Limit to configured number of fixtures instead of processing all LEDs
  uint16_t dmxEndLED = DMXStartLED + DMXNumFixtures;
  if (dmxEndLED > len) dmxEndLED = len;
  
  for (int i = DMXStartLED; i < dmxEndLED; i++) {
    uint32_t in = strip.getPixelColor(i);
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

void initDMXOutput() {
 #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  dmx.init(512);        // initialize with bus length
 #else
  dmx.initWrite(512);  // initialize with bus length
 #endif
}
#else
void initDMXOutput(){}
void handleDMXOutput() {}
#endif
