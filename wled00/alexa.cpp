#include "wled.h"

/*
 * Alexa Voice On/Off/Brightness/Color Control. Emulates a Philips Hue bridge to Alexa.
 *
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
#include "src/dependencies/espalexa/EspalexaDevice.h"

#ifndef WLED_DISABLE_ALEXA
void onAlexaChange(EspalexaDevice* dev);

void alexaInit()
{
  if (!alexaEnabled || !WLED_CONNECTED) return;

  espalexa.removeAllDevices();
  // the original configured device for on/off or macros (added first, i.e. index 0)
  espalexaDevice = new EspalexaDevice(alexaInvocationName, onAlexaChange, EspalexaDeviceType::extendedcolor);
  espalexa.addDevice(espalexaDevice);

  // up to 9 devices (added second, third, ... i.e. index 1 to 9) serve for switching on up to nine presets (preset IDs 1 to 9 in WLED),
  // names are identical as the preset names, switching off can be done by switching off any of them
  if (alexaNumPresets) {
    String name = "";
    for (byte presetIndex = 1; presetIndex <= alexaNumPresets; presetIndex++)
    {
      if (!getPresetName(presetIndex, name)) break; // no more presets
      EspalexaDevice* dev = new EspalexaDevice(name.c_str(), onAlexaChange, EspalexaDeviceType::extendedcolor);
      espalexa.addDevice(dev);
    }
  }
  espalexa.begin(&server);
}

void handleAlexa()
{
  if (!alexaEnabled || !WLED_CONNECTED) return;
  espalexa.loop();
}

void onAlexaChange(EspalexaDevice* dev)
{
  EspalexaDeviceProperty m = dev->getLastChangedProperty();

  if (m == EspalexaDeviceProperty::on)
  {
    if (dev->getId() == 0) // Device 0 is for on/off or macros
    {
      if (!macroAlexaOn)
      {
        if (bri == 0)
        {
          bri = briLast;
          stateUpdated(CALL_MODE_ALEXA);
        }
      } else
      {
        applyPreset(macroAlexaOn, CALL_MODE_ALEXA);
        if (bri == 0) dev->setValue(briLast); //stop Alexa from complaining if macroAlexaOn does not actually turn on
      }
    } else // switch-on behavior for preset devices
    {
      // turn off other preset devices
      for (byte i = 1; i < espalexa.getDeviceCount(); i++)
      {
        if (i == dev->getId()) continue;
        espalexa.getDevice(i)->setValue(0); // turn off other presets
      }

      applyPreset(dev->getId(), CALL_MODE_ALEXA); // in alexaInit() preset 1 device was added second (index 1), preset 2 third (index 2) etc.
    }
  } else if (m == EspalexaDeviceProperty::off)
  {
    if (!macroAlexaOff)
    {
      if (bri > 0)
      {
        briLast = bri;
        bri = 0;
        stateUpdated(CALL_MODE_ALEXA);
      }
    } else
    {
      applyPreset(macroAlexaOff, CALL_MODE_ALEXA);
      // below for loop stops Alexa from complaining if macroAlexaOff does not actually turn off
    }
    for (byte i = 0; i < espalexa.getDeviceCount(); i++)
    {
      espalexa.getDevice(i)->setValue(0);
    }
  } else if (m == EspalexaDeviceProperty::bri)
  {
    bri = dev->getValue();
    stateUpdated(CALL_MODE_ALEXA);
  } else //color
  {
    if (dev->getColorMode() == EspalexaColorMode::ct) //shade of white
    {
      byte rgbw[4];
      uint16_t ct = dev->getCt();
      if (!ct) return;
      uint16_t k = 1000000 / ct; //mireds to kelvin

      if (strip.hasCCTBus()) {
        bool hasManualWhite = strip.getActiveSegsLightCapabilities(true) & SEG_CAPABILITY_W;

        strip.setCCT(k);
        if (hasManualWhite) {
          rgbw[0] = 0; rgbw[1] = 0; rgbw[2] = 0; rgbw[3] = 255;
        } else {
          rgbw[0] = 255; rgbw[1] = 255; rgbw[2] = 255; rgbw[3] = 0;
          dev->setValue(255);
        }
      } else if (strip.hasWhiteChannel()) {
        switch (ct) { //these values empirically look good on RGBW
          case 199: rgbw[0]=255; rgbw[1]=255; rgbw[2]=255; rgbw[3]=255; break;
          case 234: rgbw[0]=127; rgbw[1]=127; rgbw[2]=127; rgbw[3]=255; break;
          case 284: rgbw[0]=  0; rgbw[1]=  0; rgbw[2]=  0; rgbw[3]=255; break;
          case 350: rgbw[0]=130; rgbw[1]= 90; rgbw[2]=  0; rgbw[3]=255; break;
          case 383: rgbw[0]=255; rgbw[1]=153; rgbw[2]=  0; rgbw[3]=255; break;
          default : colorKtoRGB(k, rgbw);
        }
      } else {
        colorKtoRGB(k, rgbw);
      }
      strip.setColor(0, RGBW32(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
    } else {
      uint32_t color = dev->getRGB();
      strip.setColor(0, color);
    }
    stateUpdated(CALL_MODE_ALEXA);
  }
}


#else
 void alexaInit(){}
 void handleAlexa(){}
#endif
