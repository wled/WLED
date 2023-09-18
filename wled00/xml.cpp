#include "wled.h"
#include "wled_ethernet.h"

/*
 * Sending XML status files to client
 */

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

void appendGPIOinfo() {
  char nS[8];

  oappend(SET_F("d.um_p=[-1")); // has to have 1 element
  if (i2c_sda > -1 && i2c_scl > -1) {
    oappend(","); oappend(itoa(i2c_sda,nS,10));
    oappend(","); oappend(itoa(i2c_scl,nS,10));
  }
  if (spi_mosi > -1 && spi_sclk > -1) {
    oappend(","); oappend(itoa(spi_mosi,nS,10));
    oappend(","); oappend(itoa(spi_sclk,nS,10));
  }
  // usermod pin reservations will become unnecessary when settings pages will read cfg.json directly
  if (requestJSONBufferLock(6)) {
    // if we can't allocate JSON buffer ignore usermod pins
    JsonObject mods = doc.createNestedObject(F("um"));
    usermods.addToConfig(mods);
    if (!mods.isNull()) fillUMPins(mods);
    releaseJSONBufferLock();
  }
  oappend(SET_F("];"));

//WLEDMM: use isPinOK instead of hardcoded pins
  char rsvd[140] = "d.rsvd=[";
  size_t rsLen = strlen(rsvd);
  char ro_gpio[140] = "d.ro_gpio=[";
  size_t roLen = strlen(ro_gpio);
  char pinString[10];
  for(int pinNr = 0; pinNr < WLED_NUM_PINS; pinNr++) { // 49 = highest PIN on ESP32-S3
    if(!pinManager.isPinOk(pinNr, false)) {
      sprintf(pinString, "%s%d", strlen(rsvd)==rsLen?"":",", pinNr);
      strcat(rsvd, pinString);
    }
    else {
      //if ((!pinManager.isPinAllocated(pinNr)) && (pinManager.getPinSpecialText(pinNr).length() == 0)) continue;      // un-comment to hide no-name,unused GPIO pins
      bool is_inOut = pinManager.isPinOk(pinNr, true);
      if (!is_inOut) {
        sprintf(pinString, "%s%d", strlen(ro_gpio)==roLen?"":",", pinNr);
        strcat(ro_gpio, pinString);
      }
    }
  }

  // add reserved and usermod pins as d.um_p array
  oappend(rsvd);

  #ifdef WLED_ENABLE_DMX
  oappend(SET_F(",2")); // DMX hardcoded pin
  #endif

  #ifdef WLED_DEBUG
    #if defined(WLED_DEBUG_HOST)
      if (!netDebugEnabled) oappend(SET_F(",")); oappend(itoa(hardwareTX,nS,10));// debug output (TX) pin
    #else
      oappend(SET_F(",")); oappend(itoa(hardwareTX,nS,10));// debug output (TX) pin
    #endif
  #endif

  //Note: Using pin 3 (RX) disables Adalight / Serial JSON

  #if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM)
    #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    if (psramFound()) oappend(SET_F(",16,17")); // GPIO16 & GPIO17 reserved for SPI RAM on ESP32 (not on S2, S3 or C3)
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    if (psramFound()) oappend(SET_F(",33,34,35,36,37")); // in use for "octal" PSRAM or "octal" FLASH -seems that octal PSRAM is very common on S3.
    #endif
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

  // add info for read-only GPIO
  //WLEDMM: use isPinOK instead of hardcoded pins
  oappend(ro_gpio);
  oappend(SET_F("];"));

  // add info about max. # of pins
  oappend(SET_F("d.max_gpio="));
  #if defined(CONFIG_IDF_TARGET_ESP32S2)
  oappendi(46);
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
  oappendi(48);
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
  oappendi(21);
  #elif defined(ESP32)
  oappendi(39);
  #else //8266
  oappendi(NUM_DIGITAL_PINS); //WLEDMM include pin 17 for Analog
  #endif
  oappend(SET_F(";"));

  char dt_pins[64] = { '\0' }; // fix warning: output 45 bytes into a destination of size 30
  #if defined(ESP8266) && !defined(ARDUINO_ESP8266_ESP01)
  snprintf(dt_pins, 64, "d.dt_pins=[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d];", D0, D1, D2, D3, D4, D5, D6, D7, D8, hardwareRX, hardwareTX);
  #else
  snprintf(dt_pins, 64, "d.dt_pins=[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d];", PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, PM_NO_PIN, hardwareRX, hardwareTX);
  #endif
  oappend(dt_pins);

  char a_pins[64]  = { '\0' }; // fix warning: output 45 bytes into a destination of size 30
  snprintf(a_pins, 64, "d.a_pins=[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d];", pinManager.getADCPin(PM_ADC1, 0), pinManager.getADCPin(PM_ADC1, 1), pinManager.getADCPin(PM_ADC1, 2), pinManager.getADCPin(PM_ADC1, 3), pinManager.getADCPin(PM_ADC1, 4), pinManager.getADCPin(PM_ADC1, 5), pinManager.getADCPin(PM_ADC1, 6), pinManager.getADCPin(PM_ADC1, 7), pinManager.getADCPin(PM_ADC1, 8), pinManager.getADCPin(PM_ADC1, 9), pinManager.getADCPin(PM_ADC1, 10));
  oappend(a_pins);
}

//get values for settings form in javascript
void getSettingsJS(AsyncWebServerRequest* request, byte subPage, char* dest) //WLEDMM add request
{
  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec
  DEBUG_PRINT(F("settings resp"));
  DEBUG_PRINTLN(subPage);
  obuf = dest;
  olen = 0;

  if (subPage <0 || subPage >10) return;

  if (subPage == 0)
  {
  #ifndef WLED_DISABLE_2D // include only if 2D is compiled in
    oappend(PSTR("gId('2dbtn').style.display='';"));
  #endif
  #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
    oappend(PSTR("gId('dmxbtn').style.display='';"));
  #endif
  }

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

    #ifndef WLED_DISABLE_ESPNOW
    sappend('c',SET_F("RE"),enable_espnow_remote);
    sappends('s',SET_F("RMAC"),linked_remote);
    #else
    //hide remote settings if not compiled
    oappend(SET_F("document.getElementById('remd').style.display='none';"));
    #endif

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

    #ifndef WLED_DISABLE_ESPNOW
    if (last_signal_src[0] != 0) //Have seen an ESP-NOW Remote
    {
      sappends('m',SET_F("(\"rlid\")[0]"),last_signal_src);
    } else if (!enable_espnow_remote)
    {
      sappends('m',SET_F("(\"rlid\")[0]"),(char*)F("(Enable remote to listen)"));
    } else 
    {
      sappends('m',SET_F("(\"rlid\")[0]"),(char*)F("None"));
    }
    #endif
  }

  if (subPage == 2)
  {
    char nS[32];

    appendGPIOinfo();

    // set limits
    oappend(SET_F("bLimits("));
    oappend(itoa(WLED_MAX_BUSSES,nS,10));  oappend(",");
    oappend(itoa(WLED_MIN_VIRTUAL_BUSSES,nS,10));  oappend(",");
    oappend(itoa(MAX_LEDS_PER_BUS,nS,10)); oappend(",");
    oappend(itoa(MAX_LED_MEMORY,nS,10));   oappend(",");
    oappend(itoa(MAX_LEDS,nS,10));
    oappend(SET_F(");"));

    sappend('c',SET_F("MS"),autoSegments);
    sappend('c',SET_F("CCT"),correctWB);
    sappend('c',SET_F("CR"),cctFromRgb);
    sappend('v',SET_F("CB"),strip.cctBlending);
    sappend('v',SET_F("FR"),strip.getTargetFps());
    sappend('v',SET_F("AW"),Bus::getGlobalAWMode());
    sappend('c',SET_F("LD"),strip.useLedsArray);

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
      char aw[4] = "AW"; aw[2] = 48+s; aw[3] = 0; //auto white mode
      char wo[4] = "WO"; wo[2] = 48+s; wo[3] = 0; //swap channels
      char sp[4] = "SP"; sp[2] = 48+s; sp[3] = 0; //bus clock speed
      oappend(SET_F("addLEDs(1);"));
      uint8_t pins[5];
      uint8_t nPins = bus->getPins(pins);
      for (uint8_t i = 0; i < nPins; i++) {
        lp[1] = 48+i;
        if (pinManager.isPinOk(pins[i]) || bus->getType()>=TYPE_NET_DDP_RGB) sappend('v',lp,pins[i]);
      }
      sappend('v',lc,bus->getLength());
      sappend('v',lt,bus->getType());
      sappend('v',co,bus->getColorOrder() & 0x0F);
      sappend('v',ls,bus->getStart());
      sappend('c',cv,bus->reversed);
      sappend('v',sl,bus->skippedLeds());
      sappend('c',rf,bus->isOffRefreshRequired());
      sappend('v',aw,bus->getAutoWhiteMode());
      sappend('v',wo,bus->getColorOrder() >> 4);
      uint16_t speed = bus->getFrequency();
      if (bus->getType() > TYPE_ONOFF && bus->getType() < 48) {
        switch (speed) {
          case WLED_PWM_FREQ/3 : speed = 0; break;
          case WLED_PWM_FREQ/2 : speed = 1; break;
          default:
          case WLED_PWM_FREQ   : speed = 2; break;
          case WLED_PWM_FREQ*2 : speed = 3; break;
          case WLED_PWM_FREQ*3 : speed = 4; break;
        }
      } else {
        switch (speed) {
          case  1000 : speed = 0; break;
          case  2000 : speed = 1; break;
          default:
          case  5000 : speed = 2; break;
          case 10000 : speed = 3; break;
          case 20000 : speed = 4; break;
        }
      }
      sappend('v',sp,speed);
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

    sappend('c',SET_F("GB"),gammaCorrectBri);
    sappend('c',SET_F("GC"),gammaCorrectCol);
    sappend('c',SET_F("GCP"),gammaCorrectPreview);   // WLEDMM
    dtostrf(gammaCorrectVal,3,1,nS); sappends('s',SET_F("GV"),nS);
    sappend('c',SET_F("TF"),fadeTransition);
    sappend('v',SET_F("TD"),transitionDelayDefault);
    sappend('c',SET_F("PF"),strip.paletteFade);
    sappend('v',SET_F("TP"),randomPaletteChangeTime);
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
    sappend('c',SET_F("IP"),disablePullUp);
    sappend('v',SET_F("TT"),touchThreshold);
    sappend('v',SET_F("IR"),irPin);
    sappend('v',SET_F("IT"),irEnabled);
    sappend('c',SET_F("MSO"),!irApplyToAllSelected);
    #if !defined(WLED_DISABLE_INFRARED)
    oappend(SET_F("hideNoIR();"));  // WLEDMM hide "not compiled in" message
    #endif
  }

  if (subPage == 3)
  {
    sappends('s',SET_F("DS"),serverDescription);
    sappend('c',SET_F("ST"),syncToggleReceive);
  #ifdef WLED_ENABLE_SIMPLE_UI
    sappend('c',SET_F("SU"),simplifiedUI);
  #else
    oappend(SET_F("toggle('Simple');"));    // hide Simplified UI settings
  #endif
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
    sappend('v',SET_F("UR"),udpNumRetries);

    sappend('c',SET_F("NL"),nodeListEnabled);
    sappend('c',SET_F("NB"),nodeBroadcastEnabled);

    sappend('c',SET_F("RD"),receiveDirect);
    sappend('c',SET_F("MO"),useMainSegmentOnly);
    sappend('v',SET_F("EP"),e131Port);
    sappend('c',SET_F("ES"),e131SkipOutOfSequence);
    sappend('c',SET_F("EM"),e131Multicast);
    sappend('v',SET_F("EU"),e131Universe);
#ifdef WLED_ENABLE_DMX
    oappend(SET_F("hideNoDMX();"));  // WLEDMM hide "not compiled in" message    
#endif    
#ifndef WLED_ENABLE_DMX_INPUT
    oappend(SET_F("hideDMXInput();"));  // WLEDMM hide "dmx input" settings
#else
    oappend(SET_F("hideNoDMXInput();"));  // WLEDMM hide "not compiled in" message
    sappend('v',SET_F("DMT"),dmxTransmitPin);
    sappend('v',SET_F("DMR"),dmxReceivePin);
    sappend('v',SET_F("DME"),dmxEnablePin);
#endif    
    sappend('v',SET_F("DA"),DMXAddress);
    sappend('v',SET_F("XX"),DMXSegmentSpacing);
    sappend('v',SET_F("PY"),e131Priority);
    sappend('v',SET_F("DM"),DMXMode);
    sappend('v',SET_F("ET"),realtimeTimeoutMs);
    sappend('c',SET_F("FB"),arlsForceMaxBri);
    sappend('c',SET_F("RG"),arlsDisableGammaCorrection);
    sappend('v',SET_F("WO"),arlsOffset);
    sappend('c',SET_F("AL"),alexaEnabled);
    sappends('s',SET_F("AI"),alexaInvocationName);
    sappend('c',SET_F("SA"),notifyAlexa);
    sappend('v',SET_F("AP"),alexaNumPresets);
    #ifdef WLED_DISABLE_ALEXA
    oappend(SET_F("toggle('Alexa');"));  // hide Alexa settings
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
    sappend('c',SET_F("RT"),retainMqttMsg);
    #else
    oappend(SET_F("toggle('MQTT');"));    // hide MQTT settings
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
      case HUE_ERROR_INACTIVE     : strcpy_P(hueErrorString,PSTR("Inactive"));                break;
      case HUE_ERROR_ACTIVE       : strcpy_P(hueErrorString,PSTR("Active"));                  break;
      case HUE_ERROR_UNAUTHORIZED : strcpy_P(hueErrorString,PSTR("Unauthorized"));            break;
      case HUE_ERROR_LIGHTID      : strcpy_P(hueErrorString,PSTR("Invalid light ID"));        break;
      case HUE_ERROR_PUSHLINK     : strcpy_P(hueErrorString,PSTR("Link button not pressed")); break;
      case HUE_ERROR_JSON_PARSING : strcpy_P(hueErrorString,PSTR("JSON parsing error"));      break;
      case HUE_ERROR_TIMEOUT      : strcpy_P(hueErrorString,PSTR("Timeout"));                 break;
      default: sprintf_P(hueErrorString,PSTR("Bridge Error %i"),hueError);
    }

    sappends('m',SET_F("(\"sip\")[0]"),hueErrorString);
    #else
    oappend(SET_F("toggle('Hue');"));    // hide Hue Sync settings
    #endif

    //WLEDMM: add netdebug variables
    #ifdef WLED_DEBUG_HOST
      sappend('v',SET_F("N0"),netDebugPrintIP[0]);
      sappend('v',SET_F("N1"),netDebugPrintIP[1]);
      sappend('v',SET_F("N2"),netDebugPrintIP[2]);
      sappend('v',SET_F("N3"),netDebugPrintIP[3]);
      sappend('v',SET_F("NP"),netDebugPrintPort);
    #endif
    
    sappend('v',SET_F("BD"),serialBaud);

#ifdef WLED_ENABLE_LOXONE
    oappend(SET_F("hideNoLOX();"));  // WLEDMM hide "not compiled in" message    
#endif
#ifdef WLED_ENABLE_ADALIGHT
    oappend(SET_F("hideNoADA();"));  // WLEDMM hide "not compiled in" message    
#endif

  }

  if (subPage == 5)
  {
    sappend('c',SET_F("NT"),ntpEnabled);
    sappends('s',SET_F("NS"),ntpServerName);
    sappend('c',SET_F("CF"),!useAMPM);
    sappend('i',SET_F("TZ"),currentTimezone);
    sappend('v',SET_F("UO"),utcOffsetSecs);
    char tm[48];
    dtostrf(longitude,4,2,tm);
    sappends('s',SET_F("LN"),tm);
    dtostrf(latitude,4,2,tm);
    sappends('s',SET_F("LT"),tm);
    getTimeString(tm);
    sappends('m',SET_F("(\"times\")[0]"),tm);
    if ((int)(longitude*10.0f) || (int)(latitude*10.0f)) {
      snprintf_P(tm, sizeof(tm), PSTR("Sunrise: %02d:%02d Sunset: %02d:%02d"), hour(sunrise), minute(sunrise), hour(sunset), minute(sunset));
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
    byte l = strlen(settingsPIN);
    char fpass[l+1]; //fill PIN field with 0000
    fpass[l] = 0;
    memset(fpass,'0',l);
    sappends('s',SET_F("PIN"),fpass);
    sappend('c',SET_F("NO"),otaLock);
    sappend('c',SET_F("OW"),wifiLock);
    sappend('c',SET_F("AO"),aOtaEnabled);
    sappends('m',SET_F("(\"sip\")[0]"),(char*)F("WLEDMM ")); //WLEDMM server message
    olen -= 2; //delete ";
    oappend(versionString);
    oappend(SET_F(" (build "));
    oappendi(VERSION);
    oappend(SET_F(")\";"));
    oappend(SET_F("sd=\""));
    oappend(serverDescription);
    oappend(SET_F("\";"));
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
    appendGPIOinfo();
    if (!request->hasParam("um") ) {
      // oappend(SET_F("numM="));
      // oappendi(usermods.getModCount());
      // oappend(";");
      oappend(SET_F("d.getElementsByName(\"if:SDA:pin\")[1].value=")); oappendi(i2c_sda); oappend(";"); 
      oappend(SET_F("d.getElementsByName(\"if:SCL:pin\")[1].value=")); oappendi(i2c_scl); oappend(";"); 
      oappend(SET_F("d.getElementsByName(\"if:MOSI:pin\")[1].value=")); oappendi(spi_mosi); oappend(";"); 
      oappend(SET_F("d.getElementsByName(\"if:MISO:pin\")[1].value=")); oappendi(spi_miso); oappend(";"); 
      oappend(SET_F("d.getElementsByName(\"if:SCLK:pin\")[1].value=")); oappendi(spi_sclk); oappend(";"); 
      //WLEDMM: add help info showing defaults
      oappend(SET_F("addInfo('if:SDA:pin',0,'', 'SDA');"));
      oappend(SET_F("xOpt('if:SDA:pin',1,' ⍼',")); oappendi(SDA); oappend(");");
    #ifdef HW_PIN_SDA
      oappend(SET_F("xOpt('if:SDA:pin',1,' ⎌',")); oappendi(HW_PIN_SDA); oappend(");");
    #endif
      oappend(SET_F("addInfo('if:SCL:pin',0,'', 'SCL');"));
      oappend(SET_F("xOpt('if:SCL:pin',1,' ⍼',")); oappendi(SCL); oappend(");");
      oappend(SET_F("dRO('if:SCL:pin',1);")); // disable read only pins
    #ifdef HW_PIN_SCL
      oappend(SET_F("xOpt('if:SCL:pin',1,' ⎌',")); oappendi(HW_PIN_SCL); oappend(");"); 
    #endif
      oappend(SET_F("addInfo('if:MOSI:pin',0,'', 'MOSI');"));
      oappend(SET_F("xOpt('if:MOSI:pin',1,' ⍼',")); oappendi(MOSI); oappend(");");
    #ifdef HW_PIN_MOSISPI //WLEDMM renamed from HW_PIN_DATASPI
      oappend(SET_F("xOpt('if:MOSI:pin',1,' ⎌',")); oappendi(HW_PIN_MOSISPI); oappend(");"); 
    #endif
      oappend(SET_F("addInfo('if:MISO:pin',0,'', 'MISO');"));
      oappend(SET_F("xOpt('if:MISO:pin',1,' ⍼',")); oappendi(MISO); oappend(");");
    #ifdef HW_PIN_MISOSPI
      oappend(SET_F("xOpt('if:MISO:pin',1,' ⎌',")); oappendi(HW_PIN_MISOSPI); oappend(");"); 
    #endif
      oappend(SET_F("addInfo('if:SCLK:pin',0,'', 'SCLK');"));
      oappend(SET_F("xOpt('if:SCLK:pin',1,' ⍼',")); oappendi(SCK); oappend(");");
      oappend(SET_F("dRO('if:SCLK:pin',1);")); // disable read only pins
    #ifdef HW_PIN_CLOCKSPI
      oappend(SET_F("xOpt('if:SCLK:pin',1,' ⎌',")); oappendi(HW_PIN_CLOCKSPI); oappend(");"); 
    #endif
    }
    else {
      Usermod *usermod = usermods.lookupName(request->getParam("um")->value().c_str());
      if (usermod) usermod->appendConfigData();
    }

    // oappend(SET_F("console.log('getSettingsJS fix ro pins', d.max_gpio, d.ro_gpio);")); 
    oappend(SET_F("pinPost();")); 
  }

  if (subPage == 9) // update
  {
    //WLEDMM: show bin name
    sappends('m',SET_F("(\"sip\")[0]"),(char*)F("WLEDMM_"));
    olen -= 2; //delete ";
    oappend(versionString);
    oappend(SET_F(" "));
    oappend(releaseString);
    oappend(SET_F(".bin<br>("));
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
    oappend(SET_F("ESP32-C3"));
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    oappend(SET_F("ESP32-S3"));
    #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    oappend(SET_F("ESP32-S2"));
    #elif defined(ARDUINO_ARCH_ESP32)
    oappend(ESP.getChipModel());
    #else
    oappend("esp8266");
    #endif
    oappend(SET_F(" build "));
    oappendi(VERSION);
    oappend(SET_F(")\";"));
  }

  if (subPage == 10) // 2D matrices
  {
    sappend('v',SET_F("SOMP"),strip.isMatrix);
    #ifndef WLED_DISABLE_2D
    oappend(SET_F("maxPanels=")); oappendi(WLED_MAX_PANELS); oappend(SET_F(";"));
    oappend(SET_F("resetPanels();"));
    if (strip.isMatrix) {
      if(strip.panels>0){
        sappend('v',SET_F("PW"),strip.panel[0].width); //Set generator Width and Height to first panel size for convenience
        sappend('v',SET_F("PH"),strip.panel[0].height);
      }
      sappend('v',SET_F("MPC"),strip.panels);

      //WLEDMM: keep storing basic 2d setup
      sappend('v',SET_F("BA"),strip.bOrA); //WLEDMM basic or advanced
      sappend('v',SET_F("MPH"),strip.panelsH);
      sappend('v',SET_F("MPV"),strip.panelsV);
      sappend('v',SET_F("PB"),strip.matrix.bottomStart);
      sappend('v',SET_F("PR"),strip.matrix.rightStart);
      sappend('v',SET_F("PV"),strip.matrix.vertical);
      sappend('c',SET_F("PS"),strip.matrix.serpentine);
      sappend('v',SET_F("PBL"),strip.panelO.bottomStart);
      sappend('v',SET_F("PRL"),strip.panelO.rightStart);
      sappend('v',SET_F("PVL"),strip.panelO.vertical);
      sappend('c',SET_F("PSL"),strip.panelO.serpentine);
      
      //WLEDMM: add Total LEDs
      uint16_t ledCount = 0;
      for (int8_t b = 0; b < busses.getNumBusses(); b++) {
        ledCount+=busses.getBus(b)->getLength();
      }
      sappend('v',SET_F("LC"),ledCount); //WLEDMM

      // panels
      for (uint8_t i=0; i<strip.panels; i++) {
        char n[5];
        oappend(SET_F("addPanel("));
        oappend(itoa(i,n,10));
        oappend(SET_F(");"));
        char pO[8] = { '\0' };
        snprintf_P(pO, 7, PSTR("P%d"), i);       // MAX_PANELS is 64 so pO will always only be 4 characters or less
        pO[7] = '\0';
        uint8_t l = strlen(pO);
        // create P0B, P1B, ..., P63B, etc for other PxxX
        pO[l] = 'B'; sappend('v',pO,strip.panel[i].bottomStart);
        pO[l] = 'R'; sappend('v',pO,strip.panel[i].rightStart);
        pO[l] = 'V'; sappend('v',pO,strip.panel[i].vertical);
        pO[l] = 'S'; sappend('c',pO,strip.panel[i].serpentine);
        pO[l] = 'X'; sappend('v',pO,strip.panel[i].xOffset);
        pO[l] = 'Y'; sappend('v',pO,strip.panel[i].yOffset);
        pO[l] = 'W'; sappend('v',pO,strip.panel[i].width);
        pO[l] = 'H'; sappend('v',pO,strip.panel[i].height);
      }
    }
    #else
    oappend(SET_F("gId(\"somp\").remove(1);")); // remove 2D option from dropdown
    #endif
  }
}
