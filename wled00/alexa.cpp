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
  if (alexaEnabled && WLED_CONNECTED)
  {
    if (espalexaDevice == nullptr) //only init once
    {
      espalexaDevice = new EspalexaDevice(alexaInvocationName, onAlexaChange, EspalexaDeviceType::extendedcolor);
      espalexa.addDevice(espalexaDevice);
      espalexa.begin(&server);
    } else {
      espalexaDevice->setName(alexaInvocationName);
    }
  }
}

void handleAlexa()
{
  if (!alexaEnabled || !WLED_CONNECTED) return;
  espalexa.loop();
}

void onAlexaChange(EspalexaDevice* dev)
{
  EspalexaDeviceProperty m = espalexaDevice->getLastChangedProperty();
  
  if (m == EspalexaDeviceProperty::on)
  {
    if (!macroAlexaOn)
    {
      if (bri == 0)
      {
        bri = briLast;
        stateUpdated(CALL_MODE_ALEXA);
      }
    } else {
      applyPreset(macroAlexaOn, CALL_MODE_ALEXA);
      if (bri == 0) espalexaDevice->setValue(briLast); //stop Alexa from complaining if macroAlexaOn does not actually turn on
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
    } else {
      applyPreset(macroAlexaOff, CALL_MODE_ALEXA);
      if (bri != 0) espalexaDevice->setValue(0); //stop Alexa from complaining if macroAlexaOff does not actually turn off
    }
  } else if (m == EspalexaDeviceProperty::bri)
  {
    bri = espalexaDevice->getValue();
    stateUpdated(CALL_MODE_ALEXA);
  } else //color
  {
    if (espalexaDevice->getColorMode() == EspalexaColorMode::ct) //shade of white
    {
      byte rgbw[4];
      uint16_t ct = espalexaDevice->getCt();
			if (!ct) return;
			uint16_t k = 1000000 / ct; //mireds to kelvin
			
			if (strip.hasCCTBus()) {
				strip.setCCT(k);
				rgbw[0]= 0; rgbw[1]= 0; rgbw[2]= 0; rgbw[3]= 255;
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
      strip.setColor(0, rgbw[0], rgbw[1], rgbw[2], rgbw[3]);
    } else {
      uint32_t color = espalexaDevice->getRGB();
      strip.setColor(0, color);
    }
    stateUpdated(CALL_MODE_ALEXA);
  }
}


#else
 void alexaInit(){}
 void handleAlexa(){}
#endif
