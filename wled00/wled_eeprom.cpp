#ifdef WLED_ADD_EEPROM_SUPPORT
#include <EEPROM.h>
#include "wled.h"

#if defined(WLED_ENABLE_MQTT) && MQTT_MAX_TOPIC_LEN < 32
#error "MQTT topics length < 32 is not supported by the EEPROM module!"
#endif

/*
 * DEPRECATED, do not use for new settings
 * Only used to restore config from pre-0.11 installations using the deEEP() methods
 *
 * Methods to handle saving and loading to non-volatile memory
 * EEPROM Map: https://github.com/wled-dev/WLED/wiki/EEPROM-Map
 */

//eeprom Version code, enables default settings instead of 0 init on update
#define EEPVER 22
#define EEPSIZE 2560  //Maximum is 4096
//0 -> old version, default
//1 -> 0.4p 1711272 and up
//2 -> 0.4p 1711302 and up
//3 -> 0.4  1712121 and up
//4 -> 0.5.0 and up
//5 -> 0.5.1 and up
//6 -> 0.6.0 and up
//7 -> 0.7.1 and up
//8 -> 0.8.0-a and up
//9 -> 0.8.0
//10-> 0.8.2
//11-> 0.8.5-dev #mqttauth @TimothyBrown
//12-> 0.8.7-dev
//13-> 0.9.0-dev
//14-> 0.9.0-b1
//15-> 0.9.0-b3
//16-> 0.9.1
//17-> 0.9.1-dmx
//18-> 0.9.1-e131
//19-> 0.9.1n
//20-> 0.9.1p
//21-> 0.10.1p
//22-> 2009260

/*
 * Erase all (pre 0.11) configuration data on factory reset
 */
void clearEEPROM()
{
  EEPROM.begin(EEPSIZE);
  for (int i = 0; i < EEPSIZE; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.end();
}


void readStringFromEEPROM(uint16_t pos, char* str, uint16_t len)
{
  for (int i = 0; i < len; ++i)
  {
    str[i] = EEPROM.read(pos + i);
    if (str[i] == 0) return;
  }
  str[len] = 0; //make sure every string is properly terminated. str must be at least len +1 big.
}

/*
 * Read all configuration from flash
 */
void loadSettingsFromEEPROM()
{
  if (EEPROM.read(233) != 233) //first boot/reset to default
  {
    DEBUG_PRINTLN(F("EEPROM settings invalid, using defaults..."));
    return;
  }
  int lastEEPROMversion = EEPROM.read(377); //last EEPROM version before update


  readStringFromEEPROM(  0, multiWiFi[0].clientSSID, 32);
  readStringFromEEPROM( 32, multiWiFi[0].clientPass, 64);
  readStringFromEEPROM( 96, cmDNS, 32);
  readStringFromEEPROM(128, apSSID, 32);
  readStringFromEEPROM(160, apPass, 64);

  nightlightDelayMinsDefault = EEPROM.read(224);
  nightlightDelayMins = nightlightDelayMinsDefault;
  nightlightMode = EEPROM.read(225);
  notifyDirect = EEPROM.read(226);
  sendNotificationsRT = notifyDirect;

  apChannel = EEPROM.read(227);
  if (apChannel > 13 || apChannel < 1) apChannel = 1;
  apHide = EEPROM.read(228);
  if (apHide > 1) apHide = 1;
  uint16_t length = EEPROM.read(229) + ((EEPROM.read(398) << 8) & 0xFF00); //was ledCount
  if (length > MAX_LEDS || length == 0) length = 30;
  uint8_t pins[5] = {2, 255, 255, 255, 255};
  uint8_t colorOrder = COL_ORDER_GRB;
  if (lastEEPROMversion > 9) colorOrder = EEPROM.read(383);
  if (colorOrder > COL_ORDER_GBR) colorOrder = COL_ORDER_GRB;
  bool skipFirst = EEPROM.read(2204);
  bool reversed = EEPROM.read(252);
  BusConfig bc = BusConfig(EEPROM.read(372) ? TYPE_SK6812_RGBW : TYPE_WS2812_RGB, pins, 0, length, colorOrder, reversed, skipFirst);
  BusManager::add(bc);

  notifyButton = EEPROM.read(230);
  if (EEPROM.read(231)) udpNumRetries = 1;
  buttonType[0] = EEPROM.read(232) ? BTN_TYPE_PUSH : BTN_TYPE_NONE;

  staticIP[0] = EEPROM.read(234);
  staticIP[1] = EEPROM.read(235);
  staticIP[2] = EEPROM.read(236);
  staticIP[3] = EEPROM.read(237);
  staticGateway[0] = EEPROM.read(238);
  staticGateway[1] = EEPROM.read(239);
  staticGateway[2] = EEPROM.read(240);
  staticGateway[3] = EEPROM.read(241);
  staticSubnet[0] = EEPROM.read(242);
  staticSubnet[1] = EEPROM.read(243);
  staticSubnet[2] = EEPROM.read(244);
  staticSubnet[3] = EEPROM.read(245);

  briS = EEPROM.read(249); bri = briS;
  if (!EEPROM.read(369))
  {
    bri = 0; briLast = briS;
  }
  receiveNotificationBrightness = EEPROM.read(250);
  fadeTransition = EEPROM.read(251);
  transitionDelayDefault = EEPROM.read(253) + ((EEPROM.read(254) << 8) & 0xFF00);
  transitionDelay = transitionDelayDefault;
  briMultiplier = EEPROM.read(255);

  readStringFromEEPROM(256, otaPass, 32);

  nightlightTargetBri = EEPROM.read(288);
  otaLock = EEPROM.read(289);
  udpPort = EEPROM.read(290) + ((EEPROM.read(291) << 8) & 0xFF00);

  readStringFromEEPROM(292, serverDescription, 32);

  ntpEnabled = EEPROM.read(327);
  currentTimezone = EEPROM.read(328);
  useAMPM = EEPROM.read(329);
  gammaCorrectBri = EEPROM.read(330);
  gammaCorrectCol = EEPROM.read(331);
  overlayCurrent = EEPROM.read(332);

  alexaEnabled = EEPROM.read(333);

  readStringFromEEPROM(334, alexaInvocationName, 32);

  notifyAlexa = EEPROM.read(366);
  arlsOffset = EEPROM.read(368);
  if (!EEPROM.read(367)) arlsOffset = -arlsOffset;
  turnOnAtBoot = EEPROM.read(369);
  //strip.isRgbw = EEPROM.read(372);
  //374 - strip.paletteFade

  apBehavior = EEPROM.read(376);

  //377 = lastEEPROMversion
  if (lastEEPROMversion > 3) {
    aOtaEnabled = EEPROM.read(390);
    receiveNotificationColor = EEPROM.read(391);
    receiveNotificationEffects = EEPROM.read(392);
  }

  if (lastEEPROMversion > 4) {
    #ifndef WLED_DISABLE_HUESYNC
    huePollingEnabled = EEPROM.read(2048);
    //hueUpdatingEnabled = EEPROM.read(2049);
    for (int i = 2050; i < 2054; ++i)
    {
      hueIP[i-2050] = EEPROM.read(i);
    }

    readStringFromEEPROM(2054, hueApiKey, 46);

    huePollIntervalMs = EEPROM.read(2100) + ((EEPROM.read(2101) << 8) & 0xFF00);
    notifyHue = EEPROM.read(2102);
    hueApplyOnOff = EEPROM.read(2103);
    hueApplyBri = EEPROM.read(2104);
    hueApplyColor = EEPROM.read(2105);
    huePollLightId = EEPROM.read(2106);
    #endif
  }
  if (lastEEPROMversion > 5) {
    overlayMin = EEPROM.read(2150);
    overlayMax = EEPROM.read(2151);
    analogClock12pixel = EEPROM.read(2152);
    analogClock5MinuteMarks = EEPROM.read(2153);
    analogClockSecondsTrail = EEPROM.read(2154);
    countdownMode = EEPROM.read(2155);
    countdownYear = EEPROM.read(2156);
    countdownMonth = EEPROM.read(2157);
    countdownDay = EEPROM.read(2158);
    countdownHour = EEPROM.read(2159);
    countdownMin = EEPROM.read(2160);
    countdownSec = EEPROM.read(2161);
    setCountdown();

    //macroBoot = EEPROM.read(2175);
    macroAlexaOn = EEPROM.read(2176);
    macroAlexaOff = EEPROM.read(2177);
    macroButton[0] = EEPROM.read(2178);
    macroLongPress[0] = EEPROM.read(2179);
    macroCountdown = EEPROM.read(2180);
    macroNl = EEPROM.read(2181);
    macroDoublePress[0] = EEPROM.read(2182);
    if (macroDoublePress[0] > 16) macroDoublePress[0] = 0;
  }

  if (lastEEPROMversion > 6)
  {
    e131Universe = EEPROM.read(2190) + ((EEPROM.read(2191) << 8) & 0xFF00);
    e131Multicast = EEPROM.read(2192);
    realtimeTimeoutMs = EEPROM.read(2193) + ((EEPROM.read(2194) << 8) & 0xFF00);
    arlsForceMaxBri = EEPROM.read(2195);
    arlsDisableGammaCorrection = EEPROM.read(2196);
  }

  if (lastEEPROMversion > 7)
  {
    //strip.paletteFade  = EEPROM.read(374);
    strip.paletteBlend = EEPROM.read(382);

    for (int i = 0; i < 8; ++i)
    {
      timerHours[i]   = EEPROM.read(2260 + i);
      timerMinutes[i] = EEPROM.read(2270 + i);
      timerWeekday[i] = EEPROM.read(2280 + i);
      timerMacro[i]   = EEPROM.read(2290 + i);
      if (timerMacro[i] > 0) timerMacro[i] += 16; //add 16 to work with macro --> preset mapping
      if (timerWeekday[i] == 0) timerWeekday[i] = 255;
      if (timerMacro[i] == 0) timerWeekday[i] = timerWeekday[i] & 0b11111110;
    }
  }

  if (lastEEPROMversion > 8)
  {
    readStringFromEEPROM(2300, mqttServer, 32);
    readStringFromEEPROM(2333, mqttDeviceTopic, 32);
    readStringFromEEPROM(2366, mqttGroupTopic, 32);
  }

  if (lastEEPROMversion > 9)
  {
    //strip.setColorOrder(EEPROM.read(383));
    irEnabled = EEPROM.read(385);
    strip.ablMilliampsMax = EEPROM.read(387) + ((EEPROM.read(388) << 8) & 0xFF00);
  } else if (lastEEPROMversion > 1) //ABL is off by default when updating from version older than 0.8.2
  {
    strip.ablMilliampsMax = 65000;
  } else {
    strip.ablMilliampsMax = ABL_MILLIAMPS_DEFAULT;
  }

  if (lastEEPROMversion > 10)
  {
    readStringFromEEPROM(2399, mqttUser, 40);
    readStringFromEEPROM(2440, mqttPass, 40);
    readStringFromEEPROM(2481, mqttClientID, 40);
    mqttPort = EEPROM.read(2522) + ((EEPROM.read(2523) << 8) & 0xFF00);
  }

  if (lastEEPROMversion > 11)
  {
    strip.milliampsPerLed = EEPROM.read(375);
  } else if (strip.ablMilliampsMax == 65000) //65000 indicates disabled ABL in <0.8.7
  {
    strip.ablMilliampsMax = ABL_MILLIAMPS_DEFAULT;
    strip.milliampsPerLed = 0; //disable ABL
  }
  if (lastEEPROMversion > 12)
  {
    readStringFromEEPROM(990, ntpServerName, 32);
  }
  if (lastEEPROMversion > 13)
  {
    mqttEnabled = EEPROM.read(2299);
    //syncToggleReceive = EEPROM.read(397);
  } else {
    mqttEnabled = true;
    //syncToggleReceive = false;
  }

  if (lastEEPROMversion > 14)
  {
    DMXAddress = EEPROM.read(2197) + ((EEPROM.read(2198) << 8) & 0xFF00);
    DMXMode = EEPROM.read(2199);
  } else {
    DMXAddress = 1;
    DMXMode = DMX_MODE_MULTIPLE_RGB;
  }

  //if (lastEEPROMversion > 15)
  //{
    noWifiSleep = EEPROM.read(370);
  //}

  if (lastEEPROMversion > 17)
  {
    e131SkipOutOfSequence = EEPROM.read(2189);
  } else {
    e131SkipOutOfSequence = true;
  }

  if (lastEEPROMversion > 18)
  {
    e131Port = EEPROM.read(2187) + ((EEPROM.read(2188) << 8) & 0xFF00);
  }

  #ifdef WLED_ENABLE_DMX
  if (lastEEPROMversion > 19)
  {
    e131ProxyUniverse = EEPROM.read(2185) + ((EEPROM.read(2186) << 8) & 0xFF00);
  }
  #endif

  if (lastEEPROMversion > 21) {
    udpPort2 = EEPROM.read(378) + ((EEPROM.read(379) << 8) & 0xFF00);
  }

  receiveDirect = !EEPROM.read(2200);
  //notifyMacro = EEPROM.read(2201);

  //strip.rgbwMode = EEPROM.read(2203);
  //skipFirstLed = EEPROM.read(2204);

  bootPreset = EEPROM.read(389);
  wifiLock = EEPROM.read(393);
  utcOffsetSecs = EEPROM.read(394) + ((EEPROM.read(395) << 8) & 0xFF00);
  if (EEPROM.read(396)) utcOffsetSecs = -utcOffsetSecs; //negative
  //!EEPROM.read(399); was enableSecTransition

  //favorite setting (preset) memory (25 slots/ each 20byte)
  //400 - 899 reserved

  //custom macro memory (16 slots/ each 64byte)
  //1024-2047 reserved

  #ifdef WLED_ENABLE_DMX
  // DMX (2530 - 2549)2535
  DMXChannels = EEPROM.read(2530);
  DMXGap = EEPROM.read(2531) + ((EEPROM.read(2532) << 8) & 0xFF00);
  DMXStart = EEPROM.read(2533) + ((EEPROM.read(2534) << 8) & 0xFF00);

  for (int i=0;i<15;i++) {
    DMXFixtureMap[i] = EEPROM.read(2535+i);
  } //last used: 2549
  DMXStartLED = EEPROM.read(2550);
  #endif

  //Usermod memory
  //2551 - 2559 reserved for Usermods, usable by default
  //2560 - 2943 usable, NOT reserved (need to increase EEPSIZE accordingly, new WLED core features may override this section)
  //2944 - 3071 reserved for Usermods (need to increase EEPSIZE to 3072 in const.h)
}


//provided for increased compatibility with usermods written for v0.10
void applyMacro(byte index) {
  applyPreset(index+16);
}


// De-EEPROM routine, upgrade from previous versions to v0.11
void deEEP() {
  if (WLED_FS.exists(FPSTR(getPresetsFileName()))) return;

  DEBUG_PRINTLN(F("Preset file not found, attempting to load from EEPROM"));
  DEBUGFS_PRINTLN(F("Allocating saving buffer for dEEP"));
  if (!requestJSONBufferLock(8)) return;

  JsonObject sObj = pDoc->to<JsonObject>();
  sObj.createNestedObject("0");

  EEPROM.begin(EEPSIZE);
  if (EEPROM.read(233) == 233) { //valid EEPROM save
    for (uint16_t index = 1; index <= 16; index++) { //copy presets to presets.json
      uint16_t i = 380 + index*20;
      byte ver = EEPROM.read(i);

      if ((index < 16 && ver != 1) || (index == 16 && (ver < 2 || ver > 3))) continue;

      char nbuf[16];
      sprintf(nbuf, "%d", index);

      JsonObject pObj = sObj.createNestedObject(nbuf);

      sprintf_P(nbuf, (char*)F("Preset %d"), index);
      pObj["n"] = nbuf;

      pObj["bri"] = EEPROM.read(i+1);

      if (index < 16) {
        JsonObject segObj = pObj.createNestedObject("seg");

        JsonArray colarr = segObj.createNestedArray("col");

        byte numChannels = (strip.hasWhiteChannel())? 4:3;

        for (uint8_t k = 0; k < 3; k++) //k=0 primary (i+2) k=1 secondary (i+6) k=2 tertiary color (i+12)
        {
          JsonArray colX = colarr.createNestedArray();
          uint16_t memloc = i + 6*k;
          if (k == 0) memloc += 2;

          for (byte j = 0; j < numChannels; j++) colX.add(EEPROM.read(memloc + j));
        }

        segObj["fx"]  = EEPROM.read(i+10);
        segObj["sx"]  = EEPROM.read(i+11);
        segObj["ix"]  = EEPROM.read(i+16);
        segObj["pal"] = EEPROM.read(i+17);
      } else {
        Segment* seg = strip.getSegments();
        memcpy(seg, EEPROM.getDataPtr() +i+2, 240);
        if (ver == 2) { //versions before 2004230 did not have opacity
          for (byte j = 0; j < strip.getMaxSegments(); j++)
          {
            strip.getSegment(j).opacity = 255;
            strip.getSegment(j).setOption(SEG_OPTION_ON, true); // use transistion
          }
        }
        serializeState(pObj, true, false, true);
      }
    }

    for (uint16_t index = 1; index <= 16; index++) { //copy macros to presets.json
      char m[65];
      readStringFromEEPROM(1024+64*(index-1), m, 64);
      if (m[0]) { //macro exists
        char nbuf[16];
        sprintf(nbuf, "%d", index + 16);
        JsonObject pObj = sObj.createNestedObject(nbuf);
        sprintf_P(nbuf, "Z Macro %d", index);
        pObj["n"] = nbuf;
        pObj["win"] = m;
      }
    }
  }

  EEPROM.end();

  File f = WLED_FS.open(FPSTR(getPresetsFileName()), "w");
  if (!f) {
    errorFlag = ERR_FS_GENERAL;
    releaseJSONBufferLock();
    return;
  }
  serializeJson(*pDoc, f);
  f.close();

  releaseJSONBufferLock();

  DEBUG_PRINTLN(F("deEEP complete!"));
}

void deEEPSettings() {
  DEBUG_PRINTLN(F("Restore settings from EEPROM"));
  EEPROM.begin(EEPSIZE);
  loadSettingsFromEEPROM();
  EEPROM.end();
}
#endif
