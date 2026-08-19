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

// index of the first segment device in the Espalexa device list (0 = no segment devices)
static unsigned alexaSegmentDeviceStart = 0;

// map a segment device offset to an actual segment index, skipping inactive segments
// returns -1 if the offset is out of range
static int mapSegDevToSegIndex(unsigned segDevIdx)
{
  unsigned activeCount = 0;
  for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
    if (!strip.getSegment(i).isActive()) continue;
    if (activeCount == segDevIdx) return i;
    activeCount++;
  }
  return -1;
}

void alexaInit()
{
  if (!alexaEnabled || !WLED_CONNECTED) return;

  espalexa.removeAllDevices();
  alexaSegmentDeviceStart = 0;

  // the original configured device for on/off or macros (added first, i.e. index 0)
  espalexaDevice = new EspalexaDevice(alexaInvocationName, onAlexaChange, EspalexaDeviceType::extendedcolor);
  espalexa.addDevice(espalexaDevice);

  // up to 9 devices (added second, third, ... i.e. index 1 to 9) serve for switching on up to nine presets (preset IDs 1 to 9 in WLED),
  // names are identical as the preset names, switching off can be done by switching off any of them
  if (alexaNumPresets) {
    String name = "";
    for (unsigned presetIndex = 1; presetIndex <= alexaNumPresets; presetIndex++)
    {
      if (!getPresetName(presetIndex, name)) break; // no more presets
      EspalexaDevice* dev = new EspalexaDevice(name.c_str(), onAlexaChange, EspalexaDeviceType::extendedcolor);
      espalexa.addDevice(dev);
    }
  }

  // segment devices are added after the main device and preset devices
  // device IDs: 0 = main, 1..N = presets, (N+1).. = segments
  if (alexaExposeSegments) {
    alexaSegmentDeviceStart = espalexa.getDeviceCount(); // first segment device index
    for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive()) continue;
      String segName(seg.name ? seg.name : "");
      if (!segName.length()) segName = String(F("Segment ")) + String(i);
      EspalexaDevice* dev = new EspalexaDevice(segName.c_str(), onAlexaChange, EspalexaDeviceType::extendedcolor);
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
  unsigned devId = dev->getId();

  // determine if this is a segment device
  bool isSegmentDevice = alexaExposeSegments && alexaSegmentDeviceStart > 0 && devId >= alexaSegmentDeviceStart;

  if (isSegmentDevice)
  {
    int segIdx = mapSegDevToSegIndex(devId - alexaSegmentDeviceStart);
    if (segIdx < 0) return; // segment was deactivated after discovery
    Segment& seg = strip.getSegment(segIdx);

    if (m == EspalexaDeviceProperty::on)
    {
      seg.setOption(SEG_OPTION_ON, true);
      if (!seg.opacity) seg.setOpacity(255); // ensure segment is visible when turned on
      stateUpdated(CALL_MODE_ALEXA);
    }
    else if (m == EspalexaDeviceProperty::off)
    {
      seg.setOption(SEG_OPTION_ON, false);
      dev->setValue(0);
      stateUpdated(CALL_MODE_ALEXA);
    }
    else if (m == EspalexaDeviceProperty::bri)
    {
      seg.setOption(SEG_OPTION_ON, true);
      seg.setOpacity(dev->getValue());
      stateUpdated(CALL_MODE_ALEXA);
    }
    else // color
    {
      if (dev->getColorMode() == EspalexaColorMode::ct)
      {
        byte rgbw[4];
        uint16_t ct = dev->getCt();
        if (!ct) return;
        uint16_t k = 1000000 / ct;
        if (seg.isCCT()) {
          seg.setCCT(k);
          if (seg.hasWhite()) {
            rgbw[0] = 0; rgbw[1] = 0; rgbw[2] = 0; rgbw[3] = 255;
          } else {
            rgbw[0] = 255; rgbw[1] = 255; rgbw[2] = 255; rgbw[3] = 0;
            dev->setValue(255);
          }
        } else if (seg.hasWhite()) {
          switch (ct) {
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
        seg.setColor(0, RGBW32(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
      } else {
        uint32_t color = dev->getRGB();
        seg.setColor(0, color);
      }
      stateUpdated(CALL_MODE_ALEXA);
    }
    return;
  }

  // boundary for preset device range (excludes segment devices)
  unsigned presetEnd = alexaSegmentDeviceStart ? alexaSegmentDeviceStart : espalexa.getDeviceCount();

  // original behavior: main device and preset devices
  if (m == EspalexaDeviceProperty::on)
  {
    if (devId == 0) // Device 0 is for on/off or macros
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
      for (unsigned i = 1; i < presetEnd; i++)
      {
        if (i == devId) continue;
        espalexa.getDevice(i)->setValue(0); // turn off other presets
      }

      applyPreset(devId, CALL_MODE_ALEXA); // in alexaInit() preset 1 device was added second (index 1), preset 2 third (index 2) etc.
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
    // set main and preset devices to off, but not segment devices
    for (unsigned i = 0; i < presetEnd; i++)
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
      strip.getMainSegment().setColor(0, RGBW32(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
    } else {
      uint32_t color = dev->getRGB();
      strip.getMainSegment().setColor(0, color);
    }
    stateUpdated(CALL_MODE_ALEXA);
  }
}


#else
 void alexaInit(){}
 void handleAlexa(){}
#endif
