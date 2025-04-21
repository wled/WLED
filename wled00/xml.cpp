#include "wled.h"
#include "wled_ethernet.h"

/*
 * Sending XML status files to client
 */

//build XML response to HTTP /win API request
void XML_response(Print& dest)
{
  dest.printf_P(PSTR("<?xml version=\"1.0\" ?><vs><ac>%d</ac>"), (nightlightActive && nightlightMode > NL_MODE_SET) ? briT : bri);
  for (int i = 0; i < 3; i++)
  {
   dest.printf_P(PSTR("<cl>%d</cl>"), colPri[i]);
  }
  for (int i = 0; i < 3; i++)
  {
    dest.printf_P(PSTR("<cs>%d</cs>"), colSec[i]);
  }
  dest.printf_P(PSTR("<ns>%d</ns><nr>%d</nr><nl>%d</nl><nf>%d</nf><nd>%d</nd><nt>%d</nt><fx>%d</fx><sx>%d</sx><ix>%d</ix><fp>%d</fp><wv>%d</wv><ws>%d</ws><ps>%d</ps><cy>%d</cy><ds>%s%s</ds><ss>%d</ss></vs>"),
    notifyDirect, receiveGroups!=0, nightlightActive, nightlightMode > NL_MODE_SET, nightlightDelayMins,
    nightlightTargetBri, effectCurrent, effectSpeed, effectIntensity, effectPalette,
    strip.hasWhiteChannel() ? colPri[3] : -1, colSec[3], currentPreset, currentPlaylist >= 0,
    serverDescription, realtimeMode ? PSTR(" (live)") : "",
    strip.getFirstSelectedSegId()
  );
}

static void extractPin(Print& settingsScript, const JsonObject &obj, const char *key) {
  if (obj[key].is<JsonArray>()) {
    JsonArray pins = obj[key].as<JsonArray>();
    for (JsonVariant pv : pins) {
      if (pv.as<int>() > -1) { settingsScript.print(","); settingsScript.print(pv.as<int>()); }
    }
  } else {
    if (obj[key].as<int>() > -1) { settingsScript.print(","); settingsScript.print(obj[key].as<int>()); }
  }
}

// print used pins by scanning JsonObject (1 level deep)
static void fillUMPins(Print& settingsScript, const JsonObject &mods)
{
  for (JsonPair kv : mods) {
    // kv.key() is usermod name or subobject key
    // kv.value() is object itself
    JsonObject obj = kv.value();
    if (!obj.isNull()) {
      // element is an JsonObject
      if (!obj["pin"].isNull()) {
        extractPin(settingsScript, obj, "pin");
      } else {
        // scan keys (just one level deep as is possible with usermods)
        for (JsonPair so : obj) {
          const char *key = so.key().c_str();
          if (strstr(key, "pin")) {
            // we found a key containing "pin" substring
            if (strlen(strstr(key, "pin")) == 3) {
              // and it is at the end, we found another pin
              extractPin(settingsScript, obj, key);
              continue;
            }
          }
          if (!obj[so.key()].is<JsonObject>()) continue;
          JsonObject subObj = obj[so.key()];
          if (!subObj["pin"].isNull()) {
            // get pins from subobject
            extractPin(settingsScript, subObj, "pin");
          }
        }
      }
    }
  }
}

void appendGPIOinfo(Print& settingsScript) {
  settingsScript.print(F("d.um_p=[-1")); // has to have 1 element
  if (i2c_sda > -1 && i2c_scl > -1) {
    settingsScript.printf_P(PSTR(",%d,%d"), i2c_sda, i2c_scl);
  }
  if (spi_mosi > -1 && spi_sclk > -1) {
    settingsScript.printf_P(PSTR(",%d,%d"), spi_mosi, spi_sclk);
  }
  // usermod pin reservations will become unnecessary when settings pages will read cfg.json directly
  if (requestJSONBufferLock(6)) {
    // if we can't allocate JSON buffer ignore usermod pins
    JsonObject mods = pDoc->createNestedObject("um");
    UsermodManager::addToConfig(mods);
    if (!mods.isNull()) fillUMPins(settingsScript, mods);
    releaseJSONBufferLock();
  }
  settingsScript.print(F("];"));

  // add reserved (unusable) pins
  bool firstPin = true;
  settingsScript.print(F("d.rsvd=["));
  for (unsigned i = 0; i < WLED_NUM_PINS; i++) {
    if (!PinManager::isPinOk(i, false)) {  // include readonly pins
      if (!firstPin) settingsScript.print(',');
      settingsScript.print(i);
      firstPin = false;
    }
  }
  #ifdef WLED_ENABLE_DMX
  if (!firstPin) settingsScript.print(',');
  settingsScript.print(2); // DMX hardcoded pin
  firstPin = false;
  #endif
  #if defined(WLED_DEBUG) && !defined(WLED_DEBUG_HOST)
  if (!firstPin) settingsScript.print(',');
  settingsScript.print(hardwareTX); // debug output (TX) pin
  firstPin = false;
  #endif
  #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  if (ethernetType != WLED_ETH_NONE && ethernetType < WLED_NUM_ETH_TYPES) {
    if (!firstPin) settingsScript.print(',');
    for (unsigned p=0; p<WLED_ETH_RSVD_PINS_COUNT; p++) { settingsScript.printf("%d,",esp32_nonconfigurable_ethernet_pins[p].pin); }
    if (ethernetBoards[ethernetType].eth_power >= 0)    { settingsScript.printf("%d,",ethernetBoards[ethernetType].eth_power); }
    if (ethernetBoards[ethernetType].eth_mdc >= 0)      { settingsScript.printf("%d,",ethernetBoards[ethernetType].eth_mdc); }
    if (ethernetBoards[ethernetType].eth_mdio >= 0)     { settingsScript.printf("%d,",ethernetBoards[ethernetType].eth_mdio); }
    switch (ethernetBoards[ethernetType].eth_clk_mode)  {
      case ETH_CLOCK_GPIO0_IN:
      case ETH_CLOCK_GPIO0_OUT:
        settingsScript.print(0);
        break;
      case ETH_CLOCK_GPIO16_OUT:
        settingsScript.print(16);
        break;
      case ETH_CLOCK_GPIO17_OUT:
        settingsScript.print(17);
        break;
    }
  }
  #endif
  settingsScript.print(F("];")); // rsvd

  // add info for read-only GPIO
  settingsScript.print(F("d.ro_gpio=["));
  firstPin = true;
  for (unsigned i = 0; i < WLED_NUM_PINS; i++) {
    if (PinManager::isReadOnlyPin(i)) {
      // No comma before the first pin
      if (!firstPin) settingsScript.print(',');
      settingsScript.print(i);
      firstPin = false;
    }
  }
  settingsScript.print(F("];"));

  // add info about max. # of pins
  settingsScript.printf_P(PSTR("d.max_gpio=%d;"),WLED_NUM_PINS);
}

//get values for settings form in javascript
void getSettingsJS(byte subPage, Print& settingsScript)
{
  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec
  DEBUG_PRINTF_P(PSTR("settings resp %u\n"), (unsigned)subPage);

  if (subPage <0 || subPage >10) return;
  char nS[32];

  if (subPage == SUBPAGE_MENU)
  {
  #ifdef WLED_DISABLE_2D // include only if 2D is not compiled in
    settingsScript.print(F("gId('2dbtn').style.display='none';"));
  #endif
  #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
    settingsScript.print(F("gId('dmxbtn').style.display='';"));
  #endif
  }

  if (subPage == SUBPAGE_WIFI)
  {
    size_t l;
    settingsScript.printf_P(PSTR("resetWiFi(%d);"), WLED_MAX_WIFI_COUNT);
    for (size_t n = 0; n < multiWiFi.size(); n++) {
      l = strlen(multiWiFi[n].clientPass);
      char fpass[l+1]; //fill password field with ***
      fpass[l] = 0;
      memset(fpass,'*',l);
      char bssid[13];
      fillMAC2Str(bssid, multiWiFi[n].bssid);
      settingsScript.printf_P(PSTR("addWiFi(\"%s\",\"%s\",\"%s\",0x%X,0x%X,0x%X);"),
        multiWiFi[n].clientSSID,
        fpass,
        bssid,
        (uint32_t) multiWiFi[n].staticIP, // explicit cast required as this is a struct
        (uint32_t) multiWiFi[n].staticGW,
        (uint32_t) multiWiFi[n].staticSN);
    }

    printSetFormValue(settingsScript,PSTR("D0"),dnsAddress[0]);
    printSetFormValue(settingsScript,PSTR("D1"),dnsAddress[1]);
    printSetFormValue(settingsScript,PSTR("D2"),dnsAddress[2]);
    printSetFormValue(settingsScript,PSTR("D3"),dnsAddress[3]);

    printSetFormValue(settingsScript,PSTR("CM"),cmDNS);
    printSetFormIndex(settingsScript,PSTR("AB"),apBehavior);
    printSetFormValue(settingsScript,PSTR("AS"),apSSID);
    printSetFormCheckbox(settingsScript,PSTR("AH"),apHide);

    l = strlen(apPass);
    char fapass[l+1]; //fill password field with ***
    fapass[l] = 0;
    memset(fapass,'*',l);
    printSetFormValue(settingsScript,PSTR("AP"),fapass);

    printSetFormValue(settingsScript,PSTR("AC"),apChannel);
    #ifdef ARDUINO_ARCH_ESP32
    printSetFormValue(settingsScript,PSTR("TX"),txPower);
    #else
    settingsScript.print(F("gId('tx').style.display='none';"));
    #endif
    printSetFormCheckbox(settingsScript,PSTR("FG"),force802_3g);
    printSetFormCheckbox(settingsScript,PSTR("WS"),noWifiSleep);

    #ifndef WLED_DISABLE_ESPNOW
    printSetFormCheckbox(settingsScript,PSTR("RE"),enableESPNow);
    printSetFormValue(settingsScript,PSTR("RMAC"),linked_remote);
    #else
    //hide remote settings if not compiled
    settingsScript.print(F("toggle('ESPNOW');"));  // hide ESP-NOW setting
    #endif

    #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
    printSetFormValue(settingsScript,PSTR("ETH"),ethernetType);
    #else
    //hide ethernet setting if not compiled in
    settingsScript.print(F("gId('ethd').style.display='none';"));
    #endif

    if (Network.isConnected()) //is connected
    {
      char s[32];
      IPAddress localIP = Network.localIP();
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

      #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
      if (Network.isEthernet()) strcat_P(s ,PSTR(" (Ethernet)"));
      #endif
      printSetClassElementHTML(settingsScript,PSTR("sip"),0,s);
    } else
    {
      printSetClassElementHTML(settingsScript,PSTR("sip"),0,(char*)F("Not connected"));
    }

    if (WiFi.softAPIP()[0] != 0) //is active
    {
      char s[16];
      IPAddress apIP = WiFi.softAPIP();
      sprintf(s, "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
      printSetClassElementHTML(settingsScript,PSTR("sip"),1,s);
    } else
    {
      printSetClassElementHTML(settingsScript,PSTR("sip"),1,(char*)F("Not active"));
    }

    #ifndef WLED_DISABLE_ESPNOW
    if (strlen(last_signal_src) > 0) { //Have seen an ESP-NOW Remote
      printSetClassElementHTML(settingsScript,PSTR("rlid"),0,last_signal_src);
    } else if (!enableESPNow) {
      printSetClassElementHTML(settingsScript,PSTR("rlid"),0,(char*)F("(Enable ESP-NOW to listen)"));
    } else {
      printSetClassElementHTML(settingsScript,PSTR("rlid"),0,(char*)F("None"));
    }
    #endif
  }

  if (subPage == SUBPAGE_LEDS)
  {
    appendGPIOinfo(settingsScript);

    settingsScript.printf_P(PSTR("d.ledTypes=%s;"), BusManager::getLEDTypesJSONString().c_str());

    // set limits
    settingsScript.printf_P(PSTR("bLimits(%d,%d,%d,%d,%d,%d,%d,%d);"),
      WLED_MAX_BUSSES,
      WLED_MIN_VIRTUAL_BUSSES, // irrelevant, but kept to distinguish S2/S3 in UI
      MAX_LEDS_PER_BUS,
      MAX_LED_MEMORY,
      MAX_LEDS,
      WLED_MAX_COLOR_ORDER_MAPPINGS,
      WLED_MAX_DIGITAL_CHANNELS,
      WLED_MAX_ANALOG_CHANNELS
    );

    printSetFormCheckbox(settingsScript,PSTR("MS"),strip.autoSegments);
    printSetFormCheckbox(settingsScript,PSTR("CCT"),strip.correctWB);
    printSetFormCheckbox(settingsScript,PSTR("IC"),cctICused);
    printSetFormCheckbox(settingsScript,PSTR("CR"),strip.cctFromRgb);
    printSetFormValue(settingsScript,PSTR("CB"),Bus::getCCTBlend());
    printSetFormValue(settingsScript,PSTR("FR"),strip.getTargetFps());
    printSetFormValue(settingsScript,PSTR("AW"),Bus::getGlobalAWMode());
    printSetFormCheckbox(settingsScript,PSTR("LD"),useGlobalLedBuffer);
    printSetFormCheckbox(settingsScript,PSTR("PR"),BusManager::hasParallelOutput());  // get it from bus manager not global variable

    unsigned sumMa = 0;
    for (int s = 0; s < BusManager::getNumBusses(); s++) {
      const Bus* bus = BusManager::getBus(s);
      if (!bus || !bus->isOk()) break; // should not happen but for safety
      int offset = s < 10 ? '0' : 'A';
      char lp[4] = "L0"; lp[2] = offset+s; lp[3] = 0; //ascii 0-9 //strip data pin
      char lc[4] = "LC"; lc[2] = offset+s; lc[3] = 0; //strip length
      char co[4] = "CO"; co[2] = offset+s; co[3] = 0; //strip color order
      char lt[4] = "LT"; lt[2] = offset+s; lt[3] = 0; //strip type
      char ls[4] = "LS"; ls[2] = offset+s; ls[3] = 0; //strip start LED
      char cv[4] = "CV"; cv[2] = offset+s; cv[3] = 0; //strip reverse
      char sl[4] = "SL"; sl[2] = offset+s; sl[3] = 0; //skip 1st LED
      char rf[4] = "RF"; rf[2] = offset+s; rf[3] = 0; //off refresh
      char aw[4] = "AW"; aw[2] = offset+s; aw[3] = 0; //auto white mode
      char wo[4] = "WO"; wo[2] = offset+s; wo[3] = 0; //swap channels
      char sp[4] = "SP"; sp[2] = offset+s; sp[3] = 0; //bus clock speed
      char la[4] = "LA"; la[2] = offset+s; la[3] = 0; //LED current
      char ma[4] = "MA"; ma[2] = offset+s; ma[3] = 0; //max per-port PSU current
      settingsScript.print(F("addLEDs(1);"));
      uint8_t pins[5];
      int nPins = bus->getPins(pins);
      for (int i = 0; i < nPins; i++) {
        lp[1] = '0'+i;
        if (PinManager::isPinOk(pins[i]) || bus->isVirtual()) printSetFormValue(settingsScript,lp,pins[i]);
      }
      printSetFormValue(settingsScript,lc,bus->getLength());
      printSetFormValue(settingsScript,lt,bus->getType());
      printSetFormValue(settingsScript,co,bus->getColorOrder() & 0x0F);
      printSetFormValue(settingsScript,ls,bus->getStart());
      printSetFormCheckbox(settingsScript,cv,bus->isReversed());
      printSetFormValue(settingsScript,sl,bus->skippedLeds());
      printSetFormCheckbox(settingsScript,rf,bus->isOffRefreshRequired());
      printSetFormValue(settingsScript,aw,bus->getAutoWhiteMode());
      printSetFormValue(settingsScript,wo,bus->getColorOrder() >> 4);
      unsigned speed = bus->getFrequency();
      if (bus->isPWM()) {
        switch (speed) {
          case WLED_PWM_FREQ/2    : speed = 0; break;
          case WLED_PWM_FREQ*2/3  : speed = 1; break;
          default:
          case WLED_PWM_FREQ      : speed = 2; break;
          case WLED_PWM_FREQ*2    : speed = 3; break;
          case WLED_PWM_FREQ*10/3 : speed = 4; break; // uint16_t max (19531 * 3.333)
        }
      } else if (bus->is2Pin()) {
        switch (speed) {
          case  1000 : speed = 0; break;
          case  2000 : speed = 1; break;
          default:
          case  5000 : speed = 2; break;
          case 10000 : speed = 3; break;
          case 20000 : speed = 4; break;
        }
      }
      printSetFormValue(settingsScript,sp,speed);
      printSetFormValue(settingsScript,la,bus->getLEDCurrent());
      printSetFormValue(settingsScript,ma,bus->getMaxCurrent());
      sumMa += bus->getMaxCurrent();
    }
    printSetFormValue(settingsScript,PSTR("MA"),BusManager::ablMilliampsMax() ? BusManager::ablMilliampsMax() : sumMa);
    printSetFormCheckbox(settingsScript,PSTR("ABL"),BusManager::ablMilliampsMax() || sumMa > 0);
    printSetFormCheckbox(settingsScript,PSTR("PPL"),!BusManager::ablMilliampsMax() && sumMa > 0);

    settingsScript.printf_P(PSTR("resetCOM(%d);"), WLED_MAX_COLOR_ORDER_MAPPINGS);
    const ColorOrderMap& com = BusManager::getColorOrderMap();
    for (int s = 0; s < com.count(); s++) {
      const ColorOrderMapEntry* entry = com.get(s);
      if (!entry || !entry->len) break;
      settingsScript.printf_P(PSTR("addCOM(%d,%d,%d);"), entry->start, entry->len, entry->colorOrder);
    }

    printSetFormValue(settingsScript,PSTR("CA"),briS);

    printSetFormCheckbox(settingsScript,PSTR("BO"),turnOnAtBoot);
    printSetFormValue(settingsScript,PSTR("BP"),bootPreset);

    printSetFormCheckbox(settingsScript,PSTR("GB"),gammaCorrectBri);
    printSetFormCheckbox(settingsScript,PSTR("GC"),gammaCorrectCol);
    dtostrf(gammaCorrectVal,3,1,nS); printSetFormValue(settingsScript,PSTR("GV"),nS);
    printSetFormValue(settingsScript,PSTR("TD"),transitionDelayDefault);
    printSetFormValue(settingsScript,PSTR("TP"),randomPaletteChangeTime);
    printSetFormCheckbox(settingsScript,PSTR("TH"),useHarmonicRandomPalette);
    printSetFormValue(settingsScript,PSTR("BF"),briMultiplier);
    printSetFormValue(settingsScript,PSTR("TB"),nightlightTargetBri);
    printSetFormValue(settingsScript,PSTR("TL"),nightlightDelayMinsDefault);
    printSetFormValue(settingsScript,PSTR("TW"),nightlightMode);
    printSetFormIndex(settingsScript,PSTR("PB"),strip.paletteBlend);
    printSetFormValue(settingsScript,PSTR("RL"),rlyPin);
    printSetFormCheckbox(settingsScript,PSTR("RM"),rlyMde);
    printSetFormCheckbox(settingsScript,PSTR("RO"),rlyOpenDrain);
    for (int i = 0; i < WLED_MAX_BUTTONS; i++) {
      settingsScript.printf_P(PSTR("addBtn(%d,%d,%d);"), i, btnPin[i], buttonType[i]);
    }
    printSetFormCheckbox(settingsScript,PSTR("IP"),disablePullUp);
    printSetFormValue(settingsScript,PSTR("TT"),touchThreshold);
#ifndef WLED_DISABLE_INFRARED
    printSetFormValue(settingsScript,PSTR("IR"),irPin);
    printSetFormValue(settingsScript,PSTR("IT"),irEnabled);
#endif    
    printSetFormCheckbox(settingsScript,PSTR("MSO"),!irApplyToAllSelected);
  }

  if (subPage == SUBPAGE_UI)
  {
    printSetFormValue(settingsScript,PSTR("DS"),serverDescription);
    printSetFormCheckbox(settingsScript,PSTR("SU"),simplifiedUI);
  }

  if (subPage == SUBPAGE_SYNC)
  {
    printSetFormValue(settingsScript,PSTR("UP"),udpPort);
    printSetFormValue(settingsScript,PSTR("U2"),udpPort2);
  #ifndef WLED_DISABLE_ESPNOW
    if (enableESPNow) printSetFormCheckbox(settingsScript,PSTR("EN"),useESPNowSync);
    else              settingsScript.print(F("toggle('ESPNOW');"));  // hide ESP-NOW setting
  #else
    settingsScript.print(F("toggle('ESPNOW');"));  // hide ESP-NOW setting
  #endif
    printSetFormValue(settingsScript,PSTR("GS"),syncGroups);
    printSetFormValue(settingsScript,PSTR("GR"),receiveGroups);

    printSetFormCheckbox(settingsScript,PSTR("RB"),receiveNotificationBrightness);
    printSetFormCheckbox(settingsScript,PSTR("RC"),receiveNotificationColor);
    printSetFormCheckbox(settingsScript,PSTR("RX"),receiveNotificationEffects);
    printSetFormCheckbox(settingsScript,PSTR("RP"),receiveNotificationPalette);
    printSetFormCheckbox(settingsScript,PSTR("SO"),receiveSegmentOptions);
    printSetFormCheckbox(settingsScript,PSTR("SG"),receiveSegmentBounds);
    printSetFormCheckbox(settingsScript,PSTR("SS"),sendNotifications);
    printSetFormCheckbox(settingsScript,PSTR("SD"),notifyDirect);
    printSetFormCheckbox(settingsScript,PSTR("SB"),notifyButton);
    printSetFormCheckbox(settingsScript,PSTR("SH"),notifyHue);
    printSetFormValue(settingsScript,PSTR("UR"),udpNumRetries);

    printSetFormCheckbox(settingsScript,PSTR("NL"),nodeListEnabled);
    printSetFormCheckbox(settingsScript,PSTR("NB"),nodeBroadcastEnabled);

    printSetFormCheckbox(settingsScript,PSTR("RD"),receiveDirect);
    printSetFormCheckbox(settingsScript,PSTR("MO"),useMainSegmentOnly);
    printSetFormCheckbox(settingsScript,PSTR("RLM"),realtimeRespectLedMaps);
    printSetFormValue(settingsScript,PSTR("EP"),e131Port);
    printSetFormCheckbox(settingsScript,PSTR("ES"),e131SkipOutOfSequence);
    printSetFormCheckbox(settingsScript,PSTR("EM"),e131Multicast);
    printSetFormValue(settingsScript,PSTR("EU"),e131Universe);
#ifdef WLED_ENABLE_DMX
    settingsScript.print(SET_F("hideNoDMX();"));  // hide "not compiled in" message    
#endif    
#ifndef WLED_ENABLE_DMX_INPUT
    settingsScript.print(SET_F("hideDMXInput();"));  // hide "dmx input" settings
#else
    settingsScript.print(SET_F("hideNoDMXInput();"));  //hide "not compiled in" message
    printSetFormValue(settingsScript,SET_F("IDMT"),dmxInputTransmitPin);
    printSetFormValue(settingsScript,SET_F("IDMR"),dmxInputReceivePin);
    printSetFormValue(settingsScript,SET_F("IDME"),dmxInputEnablePin);
    printSetFormValue(settingsScript,SET_F("IDMP"),dmxInputPort);
#endif
    printSetFormValue(settingsScript,PSTR("DA"),DMXAddress);
    printSetFormValue(settingsScript,PSTR("XX"),DMXSegmentSpacing);
    printSetFormValue(settingsScript,PSTR("PY"),e131Priority);
    printSetFormValue(settingsScript,PSTR("DM"),DMXMode);
    printSetFormValue(settingsScript,PSTR("ET"),realtimeTimeoutMs);
    printSetFormCheckbox(settingsScript,PSTR("FB"),arlsForceMaxBri);
    printSetFormCheckbox(settingsScript,PSTR("RG"),arlsDisableGammaCorrection);
    printSetFormValue(settingsScript,PSTR("WO"),arlsOffset);
    #ifndef WLED_DISABLE_ALEXA
    printSetFormCheckbox(settingsScript,PSTR("AL"),alexaEnabled);
    printSetFormValue(settingsScript,PSTR("AI"),alexaInvocationName);
    printSetFormCheckbox(settingsScript,PSTR("SA"),notifyAlexa);
    printSetFormValue(settingsScript,PSTR("AP"),alexaNumPresets);
    #else
    settingsScript.print(F("toggle('Alexa');"));  // hide Alexa settings
    #endif

    #ifndef WLED_DISABLE_MQTT
    printSetFormCheckbox(settingsScript,PSTR("MQ"),mqttEnabled);
    printSetFormValue(settingsScript,PSTR("MS"),mqttServer);
    printSetFormValue(settingsScript,PSTR("MQPORT"),mqttPort);
    printSetFormValue(settingsScript,PSTR("MQUSER"),mqttUser);
    byte l = strlen(mqttPass);
    char fpass[l+1]; //fill password field with ***
    fpass[l] = 0;
    memset(fpass,'*',l);
    printSetFormValue(settingsScript,PSTR("MQPASS"),fpass);
    printSetFormValue(settingsScript,PSTR("MQCID"),mqttClientID);
    printSetFormValue(settingsScript,PSTR("MD"),mqttDeviceTopic);
    printSetFormValue(settingsScript,PSTR("MG"),mqttGroupTopic);
    printSetFormCheckbox(settingsScript,PSTR("BM"),buttonPublishMqtt);
    printSetFormCheckbox(settingsScript,PSTR("RT"),retainMqttMsg);
    settingsScript.printf_P(PSTR("d.Sf.MD.maxLength=%d;d.Sf.MG.maxLength=%d;d.Sf.MS.maxLength=%d;"),
                  MQTT_MAX_TOPIC_LEN, MQTT_MAX_TOPIC_LEN, MQTT_MAX_SERVER_LEN);
    #else
    settingsScript.print(F("toggle('MQTT');"));    // hide MQTT settings
    #endif

    #ifndef WLED_DISABLE_HUESYNC
    printSetFormValue(settingsScript,PSTR("H0"),hueIP[0]);
    printSetFormValue(settingsScript,PSTR("H1"),hueIP[1]);
    printSetFormValue(settingsScript,PSTR("H2"),hueIP[2]);
    printSetFormValue(settingsScript,PSTR("H3"),hueIP[3]);
    printSetFormValue(settingsScript,PSTR("HL"),huePollLightId);
    printSetFormValue(settingsScript,PSTR("HI"),huePollIntervalMs);
    printSetFormCheckbox(settingsScript,PSTR("HP"),huePollingEnabled);
    printSetFormCheckbox(settingsScript,PSTR("HO"),hueApplyOnOff);
    printSetFormCheckbox(settingsScript,PSTR("HB"),hueApplyBri);
    printSetFormCheckbox(settingsScript,PSTR("HC"),hueApplyColor);
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

    printSetClassElementHTML(settingsScript,PSTR("sip"),0,hueErrorString);
    #else
    settingsScript.print(F("toggle('Hue');"));    // hide Hue Sync settings
    #endif
    printSetFormValue(settingsScript,PSTR("BD"),serialBaud);
    #ifndef WLED_ENABLE_ADALIGHT
    settingsScript.print(F("toggle('Serial');"));
    #endif
  }

  if (subPage == SUBPAGE_TIME)
  {
    printSetFormCheckbox(settingsScript,PSTR("NT"),ntpEnabled);
    printSetFormValue(settingsScript,PSTR("NS"),ntpServerName);
    printSetFormCheckbox(settingsScript,PSTR("CF"),!useAMPM);
    printSetFormIndex(settingsScript,PSTR("TZ"),currentTimezone);
    printSetFormValue(settingsScript,PSTR("UO"),utcOffsetSecs);
    char tm[32];
    dtostrf(longitude,4,2,tm);
    printSetFormValue(settingsScript,PSTR("LN"),tm);
    dtostrf(latitude,4,2,tm);
    printSetFormValue(settingsScript,PSTR("LT"),tm);
    getTimeString(tm);
    printSetClassElementHTML(settingsScript,PSTR("times"),0,tm);
    if ((int)(longitude*10.0f) || (int)(latitude*10.0f)) {
      sprintf_P(tm, PSTR("Sunrise: %02d:%02d Sunset: %02d:%02d"), hour(sunrise), minute(sunrise), hour(sunset), minute(sunset));
      printSetClassElementHTML(settingsScript,PSTR("times"),1,tm);
    }
    printSetFormCheckbox(settingsScript,PSTR("OL"),overlayCurrent);
    printSetFormValue(settingsScript,PSTR("O1"),overlayMin);
    printSetFormValue(settingsScript,PSTR("O2"),overlayMax);
    printSetFormValue(settingsScript,PSTR("OM"),analogClock12pixel);
    printSetFormCheckbox(settingsScript,PSTR("OS"),analogClockSecondsTrail);
    printSetFormCheckbox(settingsScript,PSTR("O5"),analogClock5MinuteMarks);
    printSetFormCheckbox(settingsScript,PSTR("OB"),analogClockSolidBlack);

    printSetFormCheckbox(settingsScript,PSTR("CE"),countdownMode);
    printSetFormValue(settingsScript,PSTR("CY"),countdownYear);
    printSetFormValue(settingsScript,PSTR("CI"),countdownMonth);
    printSetFormValue(settingsScript,PSTR("CD"),countdownDay);
    printSetFormValue(settingsScript,PSTR("CH"),countdownHour);
    printSetFormValue(settingsScript,PSTR("CM"),countdownMin);
    printSetFormValue(settingsScript,PSTR("CS"),countdownSec);

    printSetFormValue(settingsScript,PSTR("A0"),macroAlexaOn);
    printSetFormValue(settingsScript,PSTR("A1"),macroAlexaOff);
    printSetFormValue(settingsScript,PSTR("MC"),macroCountdown);
    printSetFormValue(settingsScript,PSTR("MN"),macroNl);
    for (unsigned i=0; i<WLED_MAX_BUTTONS; i++) {
      settingsScript.printf_P(PSTR("addRow(%d,%d,%d,%d);"), i, macroButton[i], macroLongPress[i], macroDoublePress[i]);
    }

    char k[4];
    k[2] = 0; //Time macros
    for (int i = 0; i<10; i++)
    {
      k[1] = 48+i; //ascii 0,1,2,3
      if (i<8) { k[0] = 'H'; printSetFormValue(settingsScript,k,timerHours[i]); }
      k[0] = 'N'; printSetFormValue(settingsScript,k,timerMinutes[i]);
      k[0] = 'T'; printSetFormValue(settingsScript,k,timerMacro[i]);
      k[0] = 'W'; printSetFormValue(settingsScript,k,timerWeekday[i]);
      if (i<8) {
        k[0] = 'M'; printSetFormValue(settingsScript,k,(timerMonth[i] >> 4) & 0x0F);
				k[0] = 'P'; printSetFormValue(settingsScript,k,timerMonth[i] & 0x0F);
        k[0] = 'D'; printSetFormValue(settingsScript,k,timerDay[i]);
				k[0] = 'E'; printSetFormValue(settingsScript,k,timerDayEnd[i]);
      }
    }
  }

  if (subPage == SUBPAGE_SEC)
  {
    byte l = strlen(settingsPIN);
    char fpass[l+1]; //fill PIN field with 0000
    fpass[l] = 0;
    memset(fpass,'0',l);
    printSetFormValue(settingsScript,PSTR("PIN"),fpass);
    printSetFormCheckbox(settingsScript,PSTR("NO"),otaLock);
    printSetFormCheckbox(settingsScript,PSTR("OW"),wifiLock);
    printSetFormCheckbox(settingsScript,PSTR("AO"),aOtaEnabled);
    char tmp_buf[128];
    snprintf_P(tmp_buf,sizeof(tmp_buf),PSTR("WLED %s (build %d)"),versionString,VERSION);
    printSetClassElementHTML(settingsScript,PSTR("sip"),0,tmp_buf);
    settingsScript.printf_P(PSTR("sd=\"%s\";"), serverDescription);
  }

  #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
  if (subPage == SUBPAGE_DMX)
  {
    printSetFormValue(settingsScript,PSTR("PU"),e131ProxyUniverse);

    printSetFormValue(settingsScript,PSTR("CN"),DMXChannels);
    printSetFormValue(settingsScript,PSTR("CG"),DMXGap);
    printSetFormValue(settingsScript,PSTR("CS"),DMXStart);
    printSetFormValue(settingsScript,PSTR("SL"),DMXStartLED);

    printSetFormIndex(settingsScript,PSTR("CH1"),DMXFixtureMap[0]);
    printSetFormIndex(settingsScript,PSTR("CH2"),DMXFixtureMap[1]);
    printSetFormIndex(settingsScript,PSTR("CH3"),DMXFixtureMap[2]);
    printSetFormIndex(settingsScript,PSTR("CH4"),DMXFixtureMap[3]);
    printSetFormIndex(settingsScript,PSTR("CH5"),DMXFixtureMap[4]);
    printSetFormIndex(settingsScript,PSTR("CH6"),DMXFixtureMap[5]);
    printSetFormIndex(settingsScript,PSTR("CH7"),DMXFixtureMap[6]);
    printSetFormIndex(settingsScript,PSTR("CH8"),DMXFixtureMap[7]);
    printSetFormIndex(settingsScript,PSTR("CH9"),DMXFixtureMap[8]);
    printSetFormIndex(settingsScript,PSTR("CH10"),DMXFixtureMap[9]);
    printSetFormIndex(settingsScript,PSTR("CH11"),DMXFixtureMap[10]);
    printSetFormIndex(settingsScript,PSTR("CH12"),DMXFixtureMap[11]);
    printSetFormIndex(settingsScript,PSTR("CH13"),DMXFixtureMap[12]);
    printSetFormIndex(settingsScript,PSTR("CH14"),DMXFixtureMap[13]);
    printSetFormIndex(settingsScript,PSTR("CH15"),DMXFixtureMap[14]);
  }
  #endif

  if (subPage == SUBPAGE_UM) //usermods
  {
    appendGPIOinfo(settingsScript);
    settingsScript.printf_P(PSTR("numM=%d;"), UsermodManager::getModCount());
    printSetFormValue(settingsScript,PSTR("SDA"),i2c_sda);
    printSetFormValue(settingsScript,PSTR("SCL"),i2c_scl);
    printSetFormValue(settingsScript,PSTR("MOSI"),spi_mosi);
    printSetFormValue(settingsScript,PSTR("MISO"),spi_miso);
    printSetFormValue(settingsScript,PSTR("SCLK"),spi_sclk);
    settingsScript.printf_P(PSTR("addInfo('SDA','%d');"
                 "addInfo('SCL','%d');"
                 "addInfo('MOSI','%d');"
                 "addInfo('MISO','%d');"
                 "addInfo('SCLK','%d');"),
      HW_PIN_SDA, HW_PIN_SCL, HW_PIN_DATASPI, HW_PIN_MISOSPI, HW_PIN_CLOCKSPI
    );
    UsermodManager::appendConfigData(settingsScript);
  }

  if (subPage == SUBPAGE_UPDATE) // update
  {
    char tmp_buf[128];
    snprintf_P(tmp_buf,sizeof(tmp_buf),PSTR("WLED %s<br>%s<br>(%s build %d)"),
      versionString,
      releaseString,
    #if defined(ARDUINO_ARCH_ESP32)
      ESP.getChipModel(),
    #else
      "esp8266",
    #endif
      VERSION);

    printSetClassElementHTML(settingsScript,PSTR("sip"),0,tmp_buf);
  }

  if (subPage == SUBPAGE_2D) // 2D matrices
  {
    printSetFormValue(settingsScript,PSTR("SOMP"),strip.isMatrix);
    #ifndef WLED_DISABLE_2D
    settingsScript.printf_P(PSTR("maxPanels=%d;resetPanels();"),WLED_MAX_PANELS);
    if (strip.isMatrix) {
      if(strip.panels>0){
        printSetFormValue(settingsScript,PSTR("PW"),strip.panel[0].width); //Set generator Width and Height to first panel size for convenience
        printSetFormValue(settingsScript,PSTR("PH"),strip.panel[0].height);
      }
      printSetFormValue(settingsScript,PSTR("MPC"),strip.panels);
      // panels
      for (unsigned i=0; i<strip.panels; i++) {
        settingsScript.printf_P(PSTR("addPanel(%d);"), i);
        char pO[8] = { '\0' };
        snprintf_P(pO, 7, PSTR("P%d"), i);       // WLED_MAX_PANELS is 18 so pO will always only be 4 characters or less
        pO[7] = '\0';
        unsigned l = strlen(pO);
        // create P0B, P1B, ..., P63B, etc for other PxxX
        pO[l] = 'B'; printSetFormValue(settingsScript,pO,strip.panel[i].bottomStart);
        pO[l] = 'R'; printSetFormValue(settingsScript,pO,strip.panel[i].rightStart);
        pO[l] = 'V'; printSetFormValue(settingsScript,pO,strip.panel[i].vertical);
        pO[l] = 'S'; printSetFormCheckbox(settingsScript,pO,strip.panel[i].serpentine);
        pO[l] = 'X'; printSetFormValue(settingsScript,pO,strip.panel[i].xOffset);
        pO[l] = 'Y'; printSetFormValue(settingsScript,pO,strip.panel[i].yOffset);
        pO[l] = 'W'; printSetFormValue(settingsScript,pO,strip.panel[i].width);
        pO[l] = 'H'; printSetFormValue(settingsScript,pO,strip.panel[i].height);
      }
    }
    #else
    settingsScript.print(F("gId(\"somp\").remove(1);")); // remove 2D option from dropdown
    #endif
  }
}
