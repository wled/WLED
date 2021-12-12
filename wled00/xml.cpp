#include "wled.h"
#include "wled_ethernet.h"

/*
 * Sending XML status files to client
 */

//macro to convert F to const
#define SET_F(x)  (const char*)F(x)

//build XML response to HTTP /win API request
void XML_response(AsyncWebServerRequest *request, char* dest)
{
  char sbuf[(dest == nullptr)?1024:1]; //allocate local buffer if none passed
  obuf = (dest == nullptr)? sbuf:dest;

  olen = 0;
  oappend(SET_F("<?xml version=\"1.0\" ?><vs><ac>"));
  oappendi((nightlightActive && nightlightMode > NL_MODE_SET) ? briT : bri);
  oappend(SET_F("</ac>"));

  for (int i = 0; i < 3; i++)
  {
   oappend("<cl>");
   oappendi(col[i]);
   oappend("</cl>");
  }
  for (int i = 0; i < 3; i++)
  {
   oappend("<cs>");
   oappendi(colSec[i]);
   oappend("</cs>");
  }
  oappend(SET_F("<ns>"));
  oappendi(notifyDirect);
  oappend(SET_F("</ns><nr>"));
  oappendi(receiveNotifications);
  oappend(SET_F("</nr><nl>"));
  oappendi(nightlightActive);
  oappend(SET_F("</nl><nf>"));
  oappendi(nightlightMode > NL_MODE_SET);
  oappend(SET_F("</nf><nd>"));
  oappendi(nightlightDelayMins);
  oappend(SET_F("</nd><nt>"));
  oappendi(nightlightTargetBri);
  oappend(SET_F("</nt><fx>"));
  oappendi(effectCurrent);
  oappend(SET_F("</fx><sx>"));
  oappendi(effectSpeed);
  oappend(SET_F("</sx><ix>"));
  oappendi(effectIntensity);
  oappend(SET_F("</ix><fp>"));
  oappendi(effectPalette);
  oappend(SET_F("</fp><wv>"));
  if (strip.hasWhiteChannel()) {
   oappendi(col[3]);
  } else {
   oappend("-1");
  }
  oappend(SET_F("</wv><ws>"));
  oappendi(colSec[3]);
  oappend(SET_F("</ws><ps>"));
  oappendi(currentPreset);
  oappend(SET_F("</ps><cy>"));
  oappendi(currentPlaylist >= 0);
  oappend(SET_F("</cy><ds>"));
  oappend(serverDescription);
  if (realtimeMode)
  {
    oappend(SET_F(" (live)"));
  }
  oappend(SET_F("</ds><ss>"));
  oappendi(strip.getFirstSelectedSegId());
  oappend(SET_F("</ss></vs>"));
  if (request != nullptr) request->send(200, "text/xml", obuf);
}

//Deprecated, use of /json/state and presets recommended instead
void URL_response(AsyncWebServerRequest *request)
{
  char sbuf[256];
  char s2buf[100];
  obuf = s2buf;
  olen = 0;

  char s[16];
  oappend(SET_F("http://"));
  IPAddress localIP = Network.localIP();
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
  oappend(s);

  oappend(SET_F("/win&A="));
  oappendi(bri);
  oappend(SET_F("&CL=h"));
  for (int i = 0; i < 3; i++)
  {
   sprintf(s,"%02X", col[i]);
   oappend(s); 
  }
  oappend(SET_F("&C2=h"));
  for (int i = 0; i < 3; i++)
  {
   sprintf(s,"%02X", colSec[i]);
   oappend(s);
  }
  oappend(SET_F("&FX="));
  oappendi(effectCurrent);
  oappend(SET_F("&SX="));
  oappendi(effectSpeed);
  oappend(SET_F("&IX="));
  oappendi(effectIntensity);
  oappend(SET_F("&FP="));
  oappendi(effectPalette);

  obuf = sbuf;
  olen = 0;

  oappend(SET_F("<html><body><a href=\""));
  oappend(s2buf);
  oappend(SET_F("\" target=\"_blank\">"));
  oappend(s2buf);  
  oappend(SET_F("</a></body></html>"));

  if (request != nullptr) request->send(200, "text/html", obuf);
}

//append a numeric setting to string buffer
void sappend(char stype, const char* key, int val)
{
  char ds[] = "d.Sf.";

  switch(stype)
  {
    case 'c': //checkbox
      oappend(ds);
      oappend(key);
      oappend(".checked=");
      oappendi(val);
      oappend(";");
      break;
    case 'v': //numeric
      oappend(ds);
      oappend(key);
      oappend(".value=");
      oappendi(val);
      oappend(";");
      break;
    case 'i': //selectedIndex
      oappend(ds);
      oappend(key);
      oappend(SET_F(".selectedIndex="));
      oappendi(val);
      oappend(";");
      break;
  }
}

//append a string setting to buffer
void sappends(char stype, const char* key, char* val)
{
  switch(stype)
  {
    case 's': { //string (we can interpret val as char*)
      oappend("d.Sf.");
      oappend(key);
      oappend(".value=\"");
      //convert "%" to "%%" to make EspAsyncWebServer happy
      char buf[130];
      uint8_t len = strlen(val) +1;
      uint8_t s = 0;
      for (uint8_t i = 0; i < len; i++) {
        buf[i+s] = val[i];
        if (val[i] == '%') {
          s++; buf[i+s] = '%';
        }
      }

      oappend(buf);
      oappend("\";");
      break; }
    case 'm': //message
      oappend(SET_F("d.getElementsByClassName"));
      oappend(key);
      oappend(SET_F(".innerHTML=\""));
      oappend(val);
      oappend("\";");
      break;
  }
}

void extractPin(JsonObject &obj, const char *key) {
  if (obj[key].is<JsonArray>()) {
    JsonArray pins = obj[key].as<JsonArray>();
    for (JsonVariant pv : pins) {
      if (pv.as<int>() > -1) { oappend(","); oappendi(pv.as<int>()); }
    }
  } else {
    if (obj[key].as<int>() > -1) { oappend(","); oappendi(obj[key].as<int>()); }
  }
}

// oappend used pins by scanning JsonObject (1 level deep)
void fillUMPins(JsonObject &mods)
{
  for (JsonPair kv : mods) {
    // kv.key() is usermod name or subobject key
    // kv.value() is object itself
    JsonObject obj = kv.value();
    if (!obj.isNull()) {
      // element is an JsonObject
      if (!obj["pin"].isNull()) {
        extractPin(obj, "pin");
      } else {
        // scan keys (just one level deep as is possible with usermods)
        for (JsonPair so : obj) {
          const char *key = so.key().c_str();
          if (strstr(key, "pin")) {
            // we found a key containing "pin" substring
            if (strlen(strstr(key, "pin")) == 3) {
              // and it is at the end, we found another pin
              extractPin(obj, key);
              continue;
            }
          }
          if (!obj[so.key()].is<JsonObject>()) continue;
          JsonObject subObj = obj[so.key()];
          if (!subObj["pin"].isNull()) {
            // get pins from subobject
            extractPin(subObj, "pin");
          }
        }
      }
    }
  }
}


//get values for settings form in javascript
void getSettingsJS(byte subPage, char* dest)
{
  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec
  DEBUG_PRINT(F("settings resp"));
  DEBUG_PRINTLN(subPage);
  obuf = dest;
  olen = 0;

  if (subPage <1 || subPage >8) return;

  if (subPage == 1)
  {
    sappends('s',SET_F("CS"),clientSSID);

    byte l = strlen(clientPass);
    char fpass[l+1]; //fill password field with ***
    fpass[l] = 0;
    memset(fpass,'*',l);
    sappends('s',SET_F("CP"),fpass);

    char k[3]; k[2] = 0; //IP addresses
    for (int i = 0; i<4; i++)
    {
      k[1] = 48+i; //ascii 0,1,2,3
      k[0] = 'I'; sappend('v',k,staticIP[i]);
      k[0] = 'G'; sappend('v',k,staticGateway[i]);
      k[0] = 'S'; sappend('v',k,staticSubnet[i]);
    }

    sappends('s',SET_F("CM"),cmDNS);
    sappend('i',SET_F("AB"),apBehavior);
    sappends('s',SET_F("AS"),apSSID);
    sappend('c',SET_F("AH"),apHide);

    l = strlen(apPass);
    char fapass[l+1]; //fill password field with ***
    fapass[l] = 0;
    memset(fapass,'*',l);
    sappends('s',SET_F("AP"),fapass);

    sappend('v',SET_F("AC"),apChannel);
    sappend('c',SET_F("WS"),noWifiSleep);

    #ifdef WLED_USE_ETHERNET
    sappend('v',SET_F("ETH"),ethernetType);
    #else
    //hide ethernet setting if not compiled in
    oappend(SET_F("document.getElementById('ethd').style.display='none';"));
    #endif

    if (Network.isConnected()) //is connected
    {
      char s[32];
      IPAddress localIP = Network.localIP();
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

      #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
      if (Network.isEthernet()) strcat_P(s ,SET_F(" (Ethernet)"));
      #endif
      sappends('m',SET_F("(\"sip\")[0]"),s);
    } else
    {
      sappends('m',SET_F("(\"sip\")[0]"),(char*)F("Not connected"));
    }

    if (WiFi.softAPIP()[0] != 0) //is active
    {
      char s[16];
      IPAddress apIP = WiFi.softAPIP();
      sprintf(s, "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
      sappends('m',SET_F("(\"sip\")[1]"),s);
    } else
    {
      sappends('m',SET_F("(\"sip\")[1]"),(char*)F("Not active"));
    }
  }

  if (subPage == 2)
  {
    char nS[8];

    // Pin reservations will become unnecessary when settings pages will read cfg.json directly
    // add reserved and usermod pins as d.um_p array
    oappend(SET_F("d.um_p=[6,7,8,9,10,11"));

    { // scope so buffer can be released earlier
    #ifdef WLED_USE_DYNAMIC_JSON
    DynamicJsonDocument doc(3072);
    #else
    if (!requestJSONBufferLock(6)) return;
    #endif

    JsonObject mods = doc.createNestedObject(F("um"));
    usermods.addToConfig(mods);
    if (!mods.isNull()) fillUMPins(mods);
    releaseJSONBufferLock();
    }

    #ifdef WLED_ENABLE_DMX
      oappend(SET_F(",2")); // DMX hardcoded pin
    #endif

    //Note: Using pin 3 (RX) disables Adalight / Serial JSON

    #ifdef WLED_DEBUG
      oappend(SET_F(",1")); // debug output (TX) pin
    #endif

    #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_PSRAM)
      if (psramFound()) oappend(SET_F(",16,17")); // GPIO16 & GPIO17 reserved for SPI RAM
    #endif

    #ifdef WLED_USE_ETHERNET
    if (ethernetType != WLED_ETH_NONE && ethernetType < WLED_NUM_ETH_TYPES) {
      for (uint8_t p=0; p<WLED_ETH_RSVD_PINS_COUNT; p++) { oappend(","); oappend(itoa(esp32_nonconfigurable_ethernet_pins[p].pin,nS,10)); }
      if (ethernetBoards[ethernetType].eth_power>=0)     { oappend(","); oappend(itoa(ethernetBoards[ethernetType].eth_power,nS,10)); }
      if (ethernetBoards[ethernetType].eth_mdc>=0)       { oappend(","); oappend(itoa(ethernetBoards[ethernetType].eth_mdc,nS,10)); }
      if (ethernetBoards[ethernetType].eth_mdio>=0)      { oappend(","); oappend(itoa(ethernetBoards[ethernetType].eth_mdio,nS,10)); }
      switch (ethernetBoards[ethernetType].eth_clk_mode) {
        case ETH_CLOCK_GPIO0_IN:
        case ETH_CLOCK_GPIO0_OUT:
          oappend(SET_F(",0"));
          break;
        case ETH_CLOCK_GPIO16_OUT:
          oappend(SET_F(",16"));
          break;
        case ETH_CLOCK_GPIO17_OUT:
          oappend(SET_F(",17"));
          break;
      }
    }
    #endif

    oappend(SET_F("];"));

    // set limits
    oappend(SET_F("bLimits("));
    oappend(itoa(WLED_MAX_BUSSES,nS,10));  oappend(",");
    oappend(itoa(MAX_LEDS_PER_BUS,nS,10)); oappend(",");
    oappend(itoa(MAX_LED_MEMORY,nS,10)); oappend(",");
    oappend(itoa(MAX_LEDS,nS,10));
    oappend(SET_F(");"));

    sappend('c',SET_F("MS"),autoSegments);
    sappend('c',SET_F("CCT"),correctWB);
    sappend('c',SET_F("CR"),cctFromRgb);
    sappend('v',SET_F("CB"),strip.cctBlending);
    sappend('v',SET_F("FR"),strip.getTargetFps());
    sappend('v',SET_F("AW"),strip.autoWhiteMode);

    for (uint8_t s=0; s < busses.getNumBusses(); s++) {
      Bus* bus = busses.getBus(s);
      if (bus == nullptr) continue;
      char lp[4] = "L0"; lp[2] = 48+s; lp[3] = 0; //ascii 0-9 //strip data pin
      char lc[4] = "LC"; lc[2] = 48+s; lc[3] = 0; //strip length
      char co[4] = "CO"; co[2] = 48+s; co[3] = 0; //strip color order
      char lt[4] = "LT"; lt[2] = 48+s; lt[3] = 0; //strip type
      char ls[4] = "LS"; ls[2] = 48+s; ls[3] = 0; //strip start LED
      char cv[4] = "CV"; cv[2] = 48+s; cv[3] = 0; //strip reverse
      char sl[4] = "SL"; sl[2] = 48+s; sl[3] = 0; //skip 1st LED
      char rf[4] = "RF"; rf[2] = 48+s; rf[3] = 0; //off refresh
      oappend(SET_F("addLEDs(1);"));
      uint8_t pins[5];
      uint8_t nPins = bus->getPins(pins);
      for (uint8_t i = 0; i < nPins; i++) {
        lp[1] = 48+i;
        if (pinManager.isPinOk(pins[i]) || bus->getType()>=TYPE_NET_DDP_RGB) sappend('v',lp,pins[i]);
      }
      sappend('v',lc,bus->getLength());
      sappend('v',lt,bus->getType());
      sappend('v',co,bus->getColorOrder());
      sappend('v',ls,bus->getStart());
      sappend('c',cv,bus->reversed);
      sappend('v',sl,bus->skippedLeds());
      sappend('c',rf,bus->isOffRefreshRequired());
    }
    sappend('v',SET_F("MA"),strip.ablMilliampsMax);
    sappend('v',SET_F("LA"),strip.milliampsPerLed);
    if (strip.currentMilliamps)
    {
      sappends('m',SET_F("(\"pow\")[0]"),(char*)"");
      olen -= 2; //delete ";
      oappendi(strip.currentMilliamps);
      oappend(SET_F("mA\";"));
    }

    oappend(SET_F("resetCOM("));
    oappend(itoa(WLED_MAX_COLOR_ORDER_MAPPINGS,nS,10));
    oappend(SET_F(");"));
    const ColorOrderMap& com = busses.getColorOrderMap();
    for (uint8_t s=0; s < com.count(); s++) {
      const ColorOrderMapEntry* entry = com.get(s);
      if (entry == nullptr) break;
      oappend(SET_F("addCOM("));
      oappend(itoa(entry->start,nS,10));  oappend(",");
      oappend(itoa(entry->len,nS,10));  oappend(",");
      oappend(itoa(entry->colorOrder,nS,10));  oappend(");");
    }

    sappend('v',SET_F("CA"),briS);

    sappend('c',SET_F("BO"),turnOnAtBoot);
    sappend('v',SET_F("BP"),bootPreset);

    sappend('c',SET_F("GB"),strip.gammaCorrectBri);
    sappend('c',SET_F("GC"),strip.gammaCorrectCol);
    sappend('c',SET_F("TF"),fadeTransition);
    sappend('v',SET_F("TD"),transitionDelayDefault);
    sappend('c',SET_F("PF"),strip.paletteFade);
    sappend('v',SET_F("BF"),briMultiplier);
    sappend('v',SET_F("TB"),nightlightTargetBri);
    sappend('v',SET_F("TL"),nightlightDelayMinsDefault);
    sappend('v',SET_F("TW"),nightlightMode);
    sappend('i',SET_F("PB"),strip.paletteBlend);
    sappend('v',SET_F("RL"),rlyPin);
    sappend('c',SET_F("RM"),rlyMde);
    for (uint8_t i=0; i<WLED_MAX_BUTTONS; i++) {
      oappend(SET_F("addBtn("));
      oappend(itoa(i,nS,10));  oappend(",");
      oappend(itoa(btnPin[i],nS,10)); oappend(",");
      oappend(itoa(buttonType[i],nS,10));
      oappend(SET_F(");"));
    }
    sappend('v',SET_F("TT"),touchThreshold);
    sappend('v',SET_F("IR"),irPin);
    sappend('v',SET_F("IT"),irEnabled);
    sappend('c',SET_F("MSO"),!irApplyToAllSelected);
  }

  if (subPage == 3)
  {
    sappends('s',SET_F("DS"),serverDescription);
    sappend('c',SET_F("ST"),syncToggleReceive);
  }

  if (subPage == 4)
  {
    sappend('v',SET_F("UP"),udpPort);
    sappend('v',SET_F("U2"),udpPort2);
    sappend('v',SET_F("GS"),syncGroups);
    sappend('v',SET_F("GR"),receiveGroups);

    sappend('c',SET_F("RB"),receiveNotificationBrightness);
    sappend('c',SET_F("RC"),receiveNotificationColor);
    sappend('c',SET_F("RX"),receiveNotificationEffects);
    sappend('c',SET_F("SO"),receiveSegmentOptions);
    sappend('c',SET_F("SG"),receiveSegmentBounds);
    sappend('c',SET_F("SD"),notifyDirectDefault);
    sappend('c',SET_F("SB"),notifyButton);
    sappend('c',SET_F("SH"),notifyHue);
    sappend('c',SET_F("SM"),notifyMacro);
    sappend('c',SET_F("S2"),notifyTwice);

    sappend('c',SET_F("NL"),nodeListEnabled);
    sappend('c',SET_F("NB"),nodeBroadcastEnabled);

    sappend('c',SET_F("RD"),receiveDirect);
    sappend('c',SET_F("MO"),useMainSegmentOnly);
    sappend('v',SET_F("EP"),e131Port);
    sappend('c',SET_F("ES"),e131SkipOutOfSequence);
    sappend('c',SET_F("EM"),e131Multicast);
    sappend('v',SET_F("EU"),e131Universe);
    sappend('v',SET_F("DA"),DMXAddress);
    sappend('v',SET_F("DM"),DMXMode);
    sappend('v',SET_F("ET"),realtimeTimeoutMs);
    sappend('c',SET_F("FB"),arlsForceMaxBri);
    sappend('c',SET_F("RG"),arlsDisableGammaCorrection);
    sappend('v',SET_F("WO"),arlsOffset);
    sappend('c',SET_F("AL"),alexaEnabled);
    sappends('s',SET_F("AI"),alexaInvocationName);
    sappend('c',SET_F("SA"),notifyAlexa);
    sappends('s',SET_F("BK"),(char*)((blynkEnabled)?SET_F("Hidden"):""));
    #ifndef WLED_DISABLE_BLYNK
    sappends('s',SET_F("BH"),blynkHost);
    sappend('v',SET_F("BP"),blynkPort);
    #endif

    #ifdef WLED_ENABLE_MQTT
    sappend('c',SET_F("MQ"),mqttEnabled);
    sappends('s',SET_F("MS"),mqttServer);
    sappend('v',SET_F("MQPORT"),mqttPort);
    sappends('s',SET_F("MQUSER"),mqttUser);
    byte l = strlen(mqttPass);
    char fpass[l+1]; //fill password field with ***
    fpass[l] = 0;
    memset(fpass,'*',l);
    sappends('s',SET_F("MQPASS"),fpass);
    sappends('s',SET_F("MQCID"),mqttClientID);
    sappends('s',"MD",mqttDeviceTopic);
    sappends('s',SET_F("MG"),mqttGroupTopic);
    sappend('c',SET_F("BM"),buttonPublishMqtt);
    #endif

    #ifndef WLED_DISABLE_HUESYNC
    sappend('v',SET_F("H0"),hueIP[0]);
    sappend('v',SET_F("H1"),hueIP[1]);
    sappend('v',SET_F("H2"),hueIP[2]);
    sappend('v',SET_F("H3"),hueIP[3]);
    sappend('v',SET_F("HL"),huePollLightId);
    sappend('v',SET_F("HI"),huePollIntervalMs);
    sappend('c',SET_F("HP"),huePollingEnabled);
    sappend('c',SET_F("HO"),hueApplyOnOff);
    sappend('c',SET_F("HB"),hueApplyBri);
    sappend('c',SET_F("HC"),hueApplyColor);
    char hueErrorString[25];
    switch (hueError)
    {
      case HUE_ERROR_INACTIVE     : strcpy(hueErrorString,(char*)F("Inactive"));                break;
      case HUE_ERROR_ACTIVE       : strcpy(hueErrorString,(char*)F("Active"));                  break;
      case HUE_ERROR_UNAUTHORIZED : strcpy(hueErrorString,(char*)F("Unauthorized"));            break;
      case HUE_ERROR_LIGHTID      : strcpy(hueErrorString,(char*)F("Invalid light ID"));        break;
      case HUE_ERROR_PUSHLINK     : strcpy(hueErrorString,(char*)F("Link button not pressed")); break;
      case HUE_ERROR_JSON_PARSING : strcpy(hueErrorString,(char*)F("JSON parsing error"));      break;
      case HUE_ERROR_TIMEOUT      : strcpy(hueErrorString,(char*)F("Timeout"));                 break;
      default: sprintf(hueErrorString,(char*)F("Bridge Error %i"),hueError);
    }
    
    sappends('m',SET_F("(\"sip\")[0]"),hueErrorString);
    #endif
    sappend('v',SET_F("BD"),serialBaud);
  }

  if (subPage == 5)
  {
    sappend('c',SET_F("NT"),ntpEnabled);
    sappends('s',SET_F("NS"),ntpServerName);
    sappend('c',SET_F("CF"),!useAMPM);
    sappend('i',SET_F("TZ"),currentTimezone);
    sappend('v',SET_F("UO"),utcOffsetSecs);
    char tm[32];
    dtostrf(longitude,4,2,tm);
    sappends('s',SET_F("LN"),tm);
    dtostrf(latitude,4,2,tm);
    sappends('s',SET_F("LT"),tm);
    getTimeString(tm);
    sappends('m',SET_F("(\"times\")[0]"),tm);
    if ((int)(longitude*10.) || (int)(latitude*10.)) {
      sprintf_P(tm, PSTR("Sunrise: %02d:%02d Sunset: %02d:%02d"), hour(sunrise), minute(sunrise), hour(sunset), minute(sunset));
      sappends('m',SET_F("(\"times\")[1]"),tm);
    }
    sappend('c',SET_F("OL"),overlayCurrent);
    sappend('v',SET_F("O1"),overlayMin);
    sappend('v',SET_F("O2"),overlayMax);
    sappend('v',SET_F("OM"),analogClock12pixel);
    sappend('c',SET_F("OS"),analogClockSecondsTrail);
    sappend('c',SET_F("O5"),analogClock5MinuteMarks);

    sappend('c',SET_F("CE"),countdownMode);
    sappend('v',SET_F("CY"),countdownYear);
    sappend('v',SET_F("CI"),countdownMonth);
    sappend('v',SET_F("CD"),countdownDay);
    sappend('v',SET_F("CH"),countdownHour);
    sappend('v',SET_F("CM"),countdownMin);
    sappend('v',SET_F("CS"),countdownSec);

    sappend('v',SET_F("A0"),macroAlexaOn);
    sappend('v',SET_F("A1"),macroAlexaOff);
    sappend('v',SET_F("MC"),macroCountdown);
    sappend('v',SET_F("MN"),macroNl);
    for (uint8_t i=0; i<WLED_MAX_BUTTONS; i++) {
      oappend(SET_F("addRow("));
      oappend(itoa(i,tm,10));  oappend(",");
      oappend(itoa(macroButton[i],tm,10)); oappend(",");
      oappend(itoa(macroLongPress[i],tm,10)); oappend(",");
      oappend(itoa(macroDoublePress[i],tm,10));
      oappend(SET_F(");"));
    }

    char k[4];
    k[2] = 0; //Time macros
    for (int i = 0; i<10; i++)
    {
      k[1] = 48+i; //ascii 0,1,2,3
      if (i<8) { k[0] = 'H'; sappend('v',k,timerHours[i]); }
      k[0] = 'N'; sappend('v',k,timerMinutes[i]);
      k[0] = 'T'; sappend('v',k,timerMacro[i]);
      k[0] = 'W'; sappend('v',k,timerWeekday[i]);
      if (i<8) {
        k[0] = 'M'; sappend('v',k,(timerMonth[i] >> 4) & 0x0F);
				k[0] = 'P'; sappend('v',k,timerMonth[i] & 0x0F);
        k[0] = 'D'; sappend('v',k,timerDay[i]);
				k[0] = 'E'; sappend('v',k,timerDayEnd[i]);
      }
    }
  }

  if (subPage == 6)
  {
    sappend('c',SET_F("NO"),otaLock);
    sappend('c',SET_F("OW"),wifiLock);
    sappend('c',SET_F("AO"),aOtaEnabled);
    sappends('m',SET_F("(\"sip\")[0]"),(char*)F("WLED "));
    olen -= 2; //delete ";
    oappend(versionString);
    oappend(SET_F(" (build "));
    oappendi(VERSION);
    oappend(SET_F(")\";"));
  }
  
  #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
  if (subPage == 7)
  {
    sappend('v',SET_F("PU"),e131ProxyUniverse);
    
    sappend('v',SET_F("CN"),DMXChannels);
    sappend('v',SET_F("CG"),DMXGap);
    sappend('v',SET_F("CS"),DMXStart);
    sappend('v',SET_F("SL"),DMXStartLED);
    
    sappend('i',SET_F("CH1"),DMXFixtureMap[0]);
    sappend('i',SET_F("CH2"),DMXFixtureMap[1]);
    sappend('i',SET_F("CH3"),DMXFixtureMap[2]);
    sappend('i',SET_F("CH4"),DMXFixtureMap[3]);
    sappend('i',SET_F("CH5"),DMXFixtureMap[4]);
    sappend('i',SET_F("CH6"),DMXFixtureMap[5]);
    sappend('i',SET_F("CH7"),DMXFixtureMap[6]);
    sappend('i',SET_F("CH8"),DMXFixtureMap[7]);
    sappend('i',SET_F("CH9"),DMXFixtureMap[8]);
    sappend('i',SET_F("CH10"),DMXFixtureMap[9]);
    sappend('i',SET_F("CH11"),DMXFixtureMap[10]);
    sappend('i',SET_F("CH12"),DMXFixtureMap[11]);
    sappend('i',SET_F("CH13"),DMXFixtureMap[12]);
    sappend('i',SET_F("CH14"),DMXFixtureMap[13]);
    sappend('i',SET_F("CH15"),DMXFixtureMap[14]);
  }
  #endif

  if (subPage == 8) //usermods
  {
    oappend(SET_F("numM="));
    oappendi(usermods.getModCount());
    oappend(";");
  }

  oappend(SET_F("}</script>"));
}
