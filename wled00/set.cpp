#include "wled.h"

/*
 * Receives client input
 */

//called upon POST settings form submit
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage)
{
  if (subPage == SUBPAGE_PINREQ)
  {
    checkSettingsPIN(request->arg(F("PIN")).c_str());
    return;
  }

  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec 7: DMX 8: usermods 9: N/A 10: 2D
  if (subPage < 1 || subPage > 10 || !correctPIN) return;

  //WIFI SETTINGS
  if (subPage == SUBPAGE_WIFI)
  {
    unsigned cnt = 0;
    for (size_t n = 0; n < WLED_MAX_WIFI_COUNT; n++) {
      char cs[4] = "CS"; cs[2] = 48+n; cs[3] = 0; //client SSID
      char pw[4] = "PW"; pw[2] = 48+n; pw[3] = 0; //client password
      char bs[4] = "BS"; bs[2] = 48+n; bs[3] = 0; //BSSID
      char ip[5] = "IP"; ip[2] = 48+n; ip[4] = 0; //IP address
      char gw[5] = "GW"; gw[2] = 48+n; gw[4] = 0; //GW address
      char sn[5] = "SN"; sn[2] = 48+n; sn[4] = 0; //subnet mask
      if (request->hasArg(cs)) {
        if (n >= multiWiFi.size()) multiWiFi.emplace_back(); // expand vector by one
        char oldSSID[33]; strcpy(oldSSID, multiWiFi[n].clientSSID);
        char oldPass[65]; strcpy(oldPass, multiWiFi[n].clientPass);

        strlcpy(multiWiFi[n].clientSSID, request->arg(cs).c_str(), 33);
        if (strlen(oldSSID) == 0 || !strncmp(multiWiFi[n].clientSSID, oldSSID, 32)) {
          forceReconnect = true;
        }
        if (!isAsterisksOnly(request->arg(pw).c_str(), 65)) {
          strlcpy(multiWiFi[n].clientPass, request->arg(pw).c_str(), 65);
          forceReconnect = true;
        }
        fillStr2MAC(multiWiFi[n].bssid, request->arg(bs).c_str());
        for (size_t i = 0; i < 4; i++) {
          ip[3] = 48+i;
          gw[3] = 48+i;
          sn[3] = 48+i;
          multiWiFi[n].staticIP[i] = request->arg(ip).toInt();
          multiWiFi[n].staticGW[i] = request->arg(gw).toInt();
          multiWiFi[n].staticSN[i] = request->arg(sn).toInt();
        }
        cnt++;
      }
    }
    // remove unused
    if (cnt < multiWiFi.size()) {
      cnt = multiWiFi.size() - cnt;
      while (cnt--) multiWiFi.pop_back();
      multiWiFi.shrink_to_fit(); // release memory
    }

    if (request->hasArg(F("D0"))) {
      dnsAddress = IPAddress(request->arg(F("D0")).toInt(),request->arg(F("D1")).toInt(),request->arg(F("D2")).toInt(),request->arg(F("D3")).toInt());
    }

    strlcpy(cmDNS, request->arg(F("CM")).c_str(), 33);

    apBehavior = request->arg(F("AB")).toInt();
    char oldSSID[33]; strcpy(oldSSID, apSSID);
    strlcpy(apSSID, request->arg(F("AS")).c_str(), 33);
    if (!strcmp(oldSSID, apSSID) && apActive) forceReconnect = true;
    apHide = request->hasArg(F("AH"));
    int passlen = request->arg(F("AP")).length();
    if (passlen == 0 || (passlen > 7 && !isAsterisksOnly(request->arg(F("AP")).c_str(), 65))) {
      strlcpy(apPass, request->arg(F("AP")).c_str(), 65);
      forceReconnect = true;
    }
    int t = request->arg(F("AC")).toInt();
    if (t != apChannel) forceReconnect = true;
    if (t > 0 && t < 14) apChannel = t;

    #ifdef ARDUINO_ARCH_ESP32
    int tx = request->arg(F("TX")).toInt();
    txPower = min(max(tx, (int)WIFI_POWER_2dBm), (int)WIFI_POWER_19_5dBm);
    #endif

    force802_3g = request->hasArg(F("FG"));
    noWifiSleep = request->hasArg(F("WS"));

    #ifndef WLED_DISABLE_ESPNOW
    bool oldESPNow = enableESPNow;
    enableESPNow = request->hasArg(F("RE"));
    if (oldESPNow != enableESPNow) forceReconnect = true;
    linked_remotes.clear();  // clear old remotes
    for (size_t n = 0; n < 10; n++) {
      char rm[5];
      snprintf(rm, sizeof(rm), "RM%d", n); // "RM0" to "RM9"
      if (request->hasArg(rm)) {
        const String& arg = request->arg(rm);
        if (arg.isEmpty()) continue;
        std::array<char, 13> mac{};
        strlcpy(mac.data(), request->arg(rm).c_str(), 13);
        strlwr(mac.data());
        if (mac[0] != '\0') {
          linked_remotes.emplace_back(mac);
        }
      }
    }
    #endif

    #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
    ethernetType = request->arg(F("ETH")).toInt();
    initEthernet();
    #endif
  }

  //LED SETTINGS
  if (subPage == SUBPAGE_LEDS)
  {
    int t = 0;

    if (rlyPin>=0 && PinManager::isPinAllocated(rlyPin, PinOwner::Relay)) {
       PinManager::deallocatePin(rlyPin, PinOwner::Relay);
    }
    #ifndef WLED_DISABLE_INFRARED
    if (irPin>=0 && PinManager::isPinAllocated(irPin, PinOwner::IR)) {
      deInitIR();
      PinManager::deallocatePin(irPin, PinOwner::IR);
    }
    #endif
    for (unsigned s=0; s<WLED_MAX_BUTTONS; s++) {
      if (btnPin[s]>=0 && PinManager::isPinAllocated(btnPin[s], PinOwner::Button)) {
        PinManager::deallocatePin(btnPin[s], PinOwner::Button);
        #ifdef SOC_TOUCH_VERSION_2 // ESP32 S2 and S3 have a function to check touch state, detach interrupt
        if (digitalPinToTouchChannel(btnPin[s]) >= 0) // if touch capable pin
          touchDetachInterrupt(btnPin[s]);            // if not assigned previously, this will do nothing
        #endif
      }
    }

    unsigned colorOrder, type, skip, awmode, channelSwap, maPerLed;
    unsigned length, start, maMax;
    uint8_t pins[5] = {255, 255, 255, 255, 255};

    // this will set global ABL max current used when per-port ABL is not used
    unsigned ablMilliampsMax = request->arg(F("MA")).toInt();
    BusManager::setMilliampsMax(ablMilliampsMax);

    strip.autoSegments = request->hasArg(F("MS"));
    strip.correctWB = request->hasArg(F("CCT"));
    strip.cctFromRgb = request->hasArg(F("CR"));
    cctICused = request->hasArg(F("IC"));
    uint8_t cctBlending = request->arg(F("CB")).toInt();
    Bus::setCCTBlend(cctBlending);
    Bus::setGlobalAWMode(request->arg(F("AW")).toInt());
    strip.setTargetFps(request->arg(F("FR")).toInt());
    #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    useParallelI2S = request->hasArg(F("PR"));
    #endif

    bool busesChanged = false;
    for (int s = 0; s < 36; s++) { // theoretical limit is 36 : "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      int offset = s < 10 ? '0' : 'A' - 10;
      char lp[4] = "L0"; lp[2] = offset+s; lp[3] = 0; //ascii 0-9 //strip data pin
      char lc[4] = "LC"; lc[2] = offset+s; lc[3] = 0; //strip length
      char co[4] = "CO"; co[2] = offset+s; co[3] = 0; //strip color order
      char lt[4] = "LT"; lt[2] = offset+s; lt[3] = 0; //strip type
      char ls[4] = "LS"; ls[2] = offset+s; ls[3] = 0; //strip start LED
      char cv[4] = "CV"; cv[2] = offset+s; cv[3] = 0; //strip reverse
      char sl[4] = "SL"; sl[2] = offset+s; sl[3] = 0; //skip first N LEDs
      char rf[4] = "RF"; rf[2] = offset+s; rf[3] = 0; //refresh required
      char aw[4] = "AW"; aw[2] = offset+s; aw[3] = 0; //auto white mode
      char wo[4] = "WO"; wo[2] = offset+s; wo[3] = 0; //channel swap
      char sp[4] = "SP"; sp[2] = offset+s; sp[3] = 0; //bus clock speed (DotStar & PWM)
      char la[4] = "LA"; la[2] = offset+s; la[3] = 0; //LED mA
      char ma[4] = "MA"; ma[2] = offset+s; ma[3] = 0; //max mA
      if (!request->hasArg(lp)) {
        DEBUG_PRINTF_P(PSTR("# of buses: %d\n"), s+1);
        break;
      }
      for (int i = 0; i < 5; i++) {
        lp[1] = '0'+i;
        if (!request->hasArg(lp)) break;
        pins[i] = (request->arg(lp).length() > 0) ? request->arg(lp).toInt() : 255;
      }
      type = request->arg(lt).toInt();
      skip = request->arg(sl).toInt();
      colorOrder = request->arg(co).toInt();
      start = (request->hasArg(ls)) ? request->arg(ls).toInt() : t;
      if (request->hasArg(lc) && request->arg(lc).toInt() > 0) {
        t += length = request->arg(lc).toInt();
      } else {
        break;  // no parameter
      }
      awmode = request->arg(aw).toInt();
      uint16_t freq = request->arg(sp).toInt();
      if (Bus::isPWM(type)) {
        switch (freq) {
          case 0 : freq = WLED_PWM_FREQ/2;    break;
          case 1 : freq = WLED_PWM_FREQ*2/3;  break;
          default:
          case 2 : freq = WLED_PWM_FREQ;      break;
          case 3 : freq = WLED_PWM_FREQ*2;    break;
          case 4 : freq = WLED_PWM_FREQ*10/3; break; // uint16_t max (19531 * 3.333)
        }
      } else if (Bus::is2Pin(type)) {
        switch (freq) {
          default:
          case 0 : freq =  1000; break;
          case 1 : freq =  2000; break;
          case 2 : freq =  5000; break;
          case 3 : freq = 10000; break;
          case 4 : freq = 20000; break;
        }
      } else {
        freq = 0;
      }
      channelSwap = Bus::hasWhite(type) ? request->arg(wo).toInt() : 0;
      if (Bus::isOnOff(type) || Bus::isPWM(type) || Bus::isVirtual(type)) { // analog and virtual
        maPerLed = 0;
        maMax = 0;
      } else {
        maPerLed = request->arg(la).toInt();
        maMax = request->arg(ma).toInt() * request->hasArg(F("PPL")); // if PP-ABL is disabled maMax (per bus) must be 0
      }
      type |= request->hasArg(rf) << 7; // off refresh override
      // actual finalization is done in WLED::loop() (removing old busses and adding new)
      // this may happen even before this loop is finished so we do "doInitBusses" after the loop
      busConfigs.emplace_back(type, pins, start, length, colorOrder | (channelSwap<<4), request->hasArg(cv), skip, awmode, freq, maPerLed, maMax);
      busesChanged = true;
    }
    //doInitBusses = busesChanged; // we will do that below to ensure all input data is processed

    // we will not bother with pre-allocating ColorOrderMappings vector
    BusManager::getColorOrderMap().reset();
    for (int s = 0; s < WLED_MAX_COLOR_ORDER_MAPPINGS; s++) {
      int offset = s < 10 ? '0' : 'A' - 10;
      char xs[4] = "XS"; xs[2] = offset+s; xs[3] = 0; //start LED
      char xc[4] = "XC"; xc[2] = offset+s; xc[3] = 0; //strip length
      char xo[4] = "XO"; xo[2] = offset+s; xo[3] = 0; //color order
      char xw[4] = "XW"; xw[2] = offset+s; xw[3] = 0; //W swap
      if (request->hasArg(xs)) {
        start = request->arg(xs).toInt();
        length = request->arg(xc).toInt();
        colorOrder = request->arg(xo).toInt() & 0x0F;
        colorOrder |= (request->arg(xw).toInt() & 0x0F) << 4; // add W swap information
        if (!BusManager::getColorOrderMap().add(start, length, colorOrder)) break;
      }
    }

    // update other pins
    #ifndef WLED_DISABLE_INFRARED
    int hw_ir_pin = request->arg(F("IR")).toInt();
    if (PinManager::allocatePin(hw_ir_pin,false, PinOwner::IR)) {
      irPin = hw_ir_pin;
    } else {
      irPin = -1;
    }
    irEnabled = request->arg(F("IT")).toInt();
    initIR();
    #endif
    irApplyToAllSelected = !request->hasArg(F("MSO"));

    int hw_rly_pin = request->arg(F("RL")).toInt();
    if (PinManager::allocatePin(hw_rly_pin,true, PinOwner::Relay)) {
      rlyPin = hw_rly_pin;
    } else {
      rlyPin = -1;
    }
    rlyMde = (bool)request->hasArg(F("RM"));
    rlyOpenDrain = (bool)request->hasArg(F("RO"));

    disablePullUp = (bool)request->hasArg(F("IP"));
    touchThreshold = request->arg(F("TT")).toInt();
    for (int i = 0; i < WLED_MAX_BUTTONS; i++) {
      int offset = i < 10 ? '0' : 'A' - 10;
      char bt[4] = "BT"; bt[2] = offset+i; bt[3] = 0; // button pin (use A,B,C,... if WLED_MAX_BUTTONS>10)
      char be[4] = "BE"; be[2] = offset+i; be[3] = 0; // button type (use A,B,C,... if WLED_MAX_BUTTONS>10)
      int hw_btn_pin = request->arg(bt).toInt();
      if (hw_btn_pin >= 0 && PinManager::allocatePin(hw_btn_pin,false,PinOwner::Button)) {
        btnPin[i] = hw_btn_pin;
        buttonType[i] = request->arg(be).toInt();
      #ifdef ARDUINO_ARCH_ESP32
        // ESP32 only: check that button pin is a valid gpio
        if ((buttonType[i] == BTN_TYPE_ANALOG) || (buttonType[i] == BTN_TYPE_ANALOG_INVERTED))
        {
          if (digitalPinToAnalogChannel(btnPin[i]) < 0) {
            // not an ADC analog pin
            DEBUG_PRINTF_P(PSTR("PIN ALLOC error: GPIO%d for analog button #%d is not an analog pin!\n"), btnPin[i], i);
            btnPin[i] = -1;
            PinManager::deallocatePin(hw_btn_pin,PinOwner::Button);
          } else {
            analogReadResolution(12); // see #4040
          }
        }
        else if ((buttonType[i] == BTN_TYPE_TOUCH || buttonType[i] == BTN_TYPE_TOUCH_SWITCH))
        {
          if (digitalPinToTouchChannel(btnPin[i]) < 0)
          {
            // not a touch pin
            DEBUG_PRINTF_P(PSTR("PIN ALLOC error: GPIO%d for touch button #%d is not an touch pin!\n"), btnPin[i], i);
            btnPin[i] = -1;
            PinManager::deallocatePin(hw_btn_pin,PinOwner::Button);
          }          
          #ifdef SOC_TOUCH_VERSION_2 // ESP32 S2 and S3 have a fucntion to check touch state but need to attach an interrupt to do so
          else                    
          {
            touchAttachInterrupt(btnPin[i], touchButtonISR, touchThreshold << 4); // threshold on Touch V2 is much higher (1500 is a value given by Espressif example, I measured changes of over 5000)
          }
          #endif          
        }
        else
      #endif
        {
          if (disablePullUp) {
            pinMode(btnPin[i], INPUT);
          } else {
            #ifdef ESP32
            pinMode(btnPin[i], buttonType[i]==BTN_TYPE_PUSH_ACT_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
            #else
            pinMode(btnPin[i], INPUT_PULLUP);
            #endif
          }
        }
      } else {
        btnPin[i] = -1;
        buttonType[i] = BTN_TYPE_NONE;
      }
    }

    briS = request->arg(F("CA")).toInt();

    turnOnAtBoot = request->hasArg(F("BO"));
    t = request->arg(F("BP")).toInt();
    if (t <= 250) bootPreset = t;
    gammaCorrectBri = request->hasArg(F("GB"));
    gammaCorrectCol = request->hasArg(F("GC"));
    gammaCorrectVal = request->arg(F("GV")).toFloat();
    if (gammaCorrectVal <= 1.0f || gammaCorrectVal > 3) {
      gammaCorrectVal = 1.0f; // no gamma correction
      gammaCorrectBri = false;
      gammaCorrectCol = false;
    }
    NeoGammaWLEDMethod::calcGammaTable(gammaCorrectVal); // fill look-up tables

    t = request->arg(F("TD")).toInt();
    if (t >= 0) transitionDelayDefault = t;
    t = request->arg(F("TP")).toInt();
    randomPaletteChangeTime = MIN(255,MAX(1,t));
    useHarmonicRandomPalette = request->hasArg(F("TH"));
    useRainbowWheel = request->hasArg(F("RW"));

    nightlightTargetBri = request->arg(F("TB")).toInt();
    t = request->arg(F("TL")).toInt();
    if (t > 0) nightlightDelayMinsDefault = t;
    nightlightDelayMins = nightlightDelayMinsDefault;
    nightlightMode = request->arg(F("TW")).toInt();

    t = request->arg(F("PB")).toInt();
    if (t >= 0 && t < 4) paletteBlend = t;
    t = request->arg(F("BF")).toInt();
    if (t > 0) briMultiplier = t;

    doInitBusses = busesChanged;
  }

  //UI
  if (subPage == SUBPAGE_UI)
  {
    strlcpy(serverDescription, request->arg(F("DS")).c_str(), 33);
    //syncToggleReceive = request->hasArg(F("ST"));
    simplifiedUI = request->hasArg(F("SU"));
    DEBUG_PRINTLN(F("Enumerating ledmaps"));
    enumerateLedmaps();
    DEBUG_PRINTLN(F("Loading custom palettes"));
    loadCustomPalettes(); // (re)load all custom palettes
  }

  //SYNC
  if (subPage == SUBPAGE_SYNC)
  {
    int t = request->arg(F("UP")).toInt();
    if (t > 0) udpPort = t;
    t = request->arg(F("U2")).toInt();
    if (t > 0) udpPort2 = t;

    #ifndef WLED_DISABLE_ESPNOW
    useESPNowSync = request->hasArg(F("EN"));
    #endif

    syncGroups = request->arg(F("GS")).toInt();
    receiveGroups = request->arg(F("GR")).toInt();

    receiveNotificationBrightness = request->hasArg(F("RB"));
    receiveNotificationColor = request->hasArg(F("RC"));
    receiveNotificationEffects = request->hasArg(F("RX"));
    receiveNotificationPalette = request->hasArg(F("RP"));
    receiveSegmentOptions = request->hasArg(F("SO"));
    receiveSegmentBounds = request->hasArg(F("SG"));
    sendNotifications = request->hasArg(F("SS"));
    notifyDirect = request->hasArg(F("SD"));
    notifyButton = request->hasArg(F("SB"));
    notifyAlexa = request->hasArg(F("SA"));
    notifyHue = request->hasArg(F("SH"));

    t = request->arg(F("UR")).toInt();
    if ((t>=0) && (t<30)) udpNumRetries = t;


    nodeListEnabled = request->hasArg(F("NL"));
    if (!nodeListEnabled) Nodes.clear();
    nodeBroadcastEnabled = request->hasArg(F("NB"));

    receiveDirect = request->hasArg(F("RD")); // UDP realtime
    useMainSegmentOnly = request->hasArg(F("MO"));
    realtimeRespectLedMaps = request->hasArg(F("RLM"));
    e131SkipOutOfSequence = request->hasArg(F("ES"));
    e131Multicast = request->hasArg(F("EM"));
    t = request->arg(F("EP")).toInt();
    if (t > 0) e131Port = t;
    t = request->arg(F("EU")).toInt();
    if (t >= 0  && t <= 63999) e131Universe = t;
    t = request->arg(F("DA")).toInt();
    if (t >= 0  && t <= 510) DMXAddress = t;
    t = request->arg(F("XX")).toInt();
    if (t >= 0  && t <= 150) DMXSegmentSpacing = t;
    t = request->arg(F("PY")).toInt();
    if (t >= 0  && t <= 200) e131Priority = t;
    t = request->arg(F("DM")).toInt();
    if (t >= DMX_MODE_DISABLED && t <= DMX_MODE_PRESET) DMXMode = t;
    t = request->arg(F("ET")).toInt();
    if (t > 99  && t <= 65000) realtimeTimeoutMs = t;
    arlsForceMaxBri = request->hasArg(F("FB"));
    arlsDisableGammaCorrection = request->hasArg(F("RG"));
    t = request->arg(F("WO")).toInt();
    if (t >= -255  && t <= 255) arlsOffset = t;

#ifdef WLED_ENABLE_DMX_INPUT
    dmxInputTransmitPin = request->arg(F("IDMT")).toInt();
    dmxInputReceivePin = request->arg(F("IDMR")).toInt();
    dmxInputEnablePin = request->arg(F("IDME")).toInt();
    dmxInputPort = request->arg(F("IDMP")).toInt();
    if(dmxInputPort <= 0 || dmxInputPort > 2) dmxInputPort = 2;
#endif

    #ifndef WLED_DISABLE_ALEXA
    alexaEnabled = request->hasArg(F("AL"));
    strlcpy(alexaInvocationName, request->arg(F("AI")).c_str(), 33);
    t = request->arg(F("AP")).toInt();
    if (t >= 0 && t <= 9) alexaNumPresets = t;
    #endif

    #ifndef WLED_DISABLE_MQTT
    mqttEnabled = request->hasArg(F("MQ"));
    strlcpy(mqttServer, request->arg(F("MS")).c_str(), MQTT_MAX_SERVER_LEN+1);
    t = request->arg(F("MQPORT")).toInt();
    if (t > 0) mqttPort = t;
    strlcpy(mqttUser, request->arg(F("MQUSER")).c_str(), 41);
    if (!isAsterisksOnly(request->arg(F("MQPASS")).c_str(), 41)) strlcpy(mqttPass, request->arg(F("MQPASS")).c_str(), 65);
    strlcpy(mqttClientID, request->arg(F("MQCID")).c_str(), 41);
    strlcpy(mqttDeviceTopic, request->arg(F("MD")).c_str(), MQTT_MAX_TOPIC_LEN+1);
    strlcpy(mqttGroupTopic, request->arg(F("MG")).c_str(), MQTT_MAX_TOPIC_LEN+1);
    buttonPublishMqtt = request->hasArg(F("BM"));
    retainMqttMsg = request->hasArg(F("RT"));
    #endif

    #ifndef WLED_DISABLE_HUESYNC
    for (int i=0;i<4;i++){
      String a = "H"+String(i);
      hueIP[i] = request->arg(a).toInt();
    }

    t = request->arg(F("HL")).toInt();
    if (t > 0) huePollLightId = t;

    t = request->arg(F("HI")).toInt();
    if (t > 50) huePollIntervalMs = t;

    hueApplyOnOff = request->hasArg(F("HO"));
    hueApplyBri = request->hasArg(F("HB"));
    hueApplyColor = request->hasArg(F("HC"));
    huePollingEnabled = request->hasArg(F("HP"));
    hueStoreAllowed = true;
    reconnectHue();
    #endif

    t = request->arg(F("BD")).toInt();
    if (t >= 96 && t <= 15000) serialBaud = t;
    updateBaudRate(serialBaud *100);
  }

  //TIME
  if (subPage == SUBPAGE_TIME)
  {
    ntpEnabled = request->hasArg(F("NT"));
    strlcpy(ntpServerName, request->arg(F("NS")).c_str(), 33);
    useAMPM = !request->hasArg(F("CF"));
    currentTimezone = request->arg(F("TZ")).toInt();
    utcOffsetSecs = request->arg(F("UO")).toInt();

    //start ntp if not already connected
    if (ntpEnabled && WLED_CONNECTED && !ntpConnected) ntpConnected = ntpUdp.begin(ntpLocalPort);
    ntpLastSyncTime = NTP_NEVER; // force new NTP query

    longitude = request->arg(F("LN")).toFloat();
    latitude = request->arg(F("LT")).toFloat();
    // force a sunrise/sunset re-calculation
    calculateSunriseAndSunset();

    overlayCurrent = request->hasArg(F("OL")) ? 1 : 0;

    overlayMin = request->arg(F("O1")).toInt();
    overlayMax = request->arg(F("O2")).toInt();
    analogClock12pixel = request->arg(F("OM")).toInt();
    analogClock5MinuteMarks = request->hasArg(F("O5"));
    analogClockSecondsTrail = request->hasArg(F("OS"));
    analogClockSolidBlack = request->hasArg(F("OB"));

    countdownMode = request->hasArg(F("CE"));
    countdownYear = request->arg(F("CY")).toInt();
    countdownMonth = request->arg(F("CI")).toInt();
    countdownDay = request->arg(F("CD")).toInt();
    countdownHour = request->arg(F("CH")).toInt();
    countdownMin = request->arg(F("CM")).toInt();
    countdownSec = request->arg(F("CS")).toInt();
    setCountdown();

    macroAlexaOn = request->arg(F("A0")).toInt();
    macroAlexaOff = request->arg(F("A1")).toInt();
    macroCountdown = request->arg(F("MC")).toInt();
    macroNl = request->arg(F("MN")).toInt();
    for (unsigned i=0; i<WLED_MAX_BUTTONS; i++) {
      char mp[4] = "MP"; mp[2] = (i<10?48:55)+i; mp[3] = 0; // short
      char ml[4] = "ML"; ml[2] = (i<10?48:55)+i; ml[3] = 0; // long
      char md[4] = "MD"; md[2] = (i<10?48:55)+i; md[3] = 0; // double
      //if (!request->hasArg(mp)) break;
      macroButton[i] = request->arg(mp).toInt();      // these will default to 0 if not present
      macroLongPress[i] = request->arg(ml).toInt();
      macroDoublePress[i] = request->arg(md).toInt();
    }

    char k[3]; k[2] = 0;
    for (int i = 0; i<10; i++) {
      k[1] = i+48;//ascii 0,1,2,3,...
      k[0] = 'H'; //timer hours
      timerHours[i] = request->arg(k).toInt();
      k[0] = 'N'; //minutes
      timerMinutes[i] = request->arg(k).toInt();
      k[0] = 'T'; //macros
      timerMacro[i] = request->arg(k).toInt();
      k[0] = 'W'; //weekdays
      timerWeekday[i] = request->arg(k).toInt();
      if (i<8) {
        k[0] = 'M'; //start month
        timerMonth[i] = request->arg(k).toInt() & 0x0F;
        timerMonth[i] <<= 4;
        k[0] = 'P'; //end month
        timerMonth[i] += (request->arg(k).toInt() & 0x0F);
        k[0] = 'D'; //start day
        timerDay[i] = request->arg(k).toInt();
        k[0] = 'E'; //end day
        timerDayEnd[i] = request->arg(k).toInt();
      }
    }
  }

  //SECURITY
  if (subPage == SUBPAGE_SEC)
  {
    if (request->hasArg(F("RS"))) //complete factory reset
    {
      WLED_FS.format();
      #ifdef WLED_ADD_EEPROM_SUPPORT
      clearEEPROM();
      #endif
      serveMessage(request, 200, F("All Settings erased."), F("Connect to WLED-AP to setup again"),255);
      doReboot = true; // may reboot immediately on dual-core system (race condition) which is desireable in this case
    }

    if (request->hasArg(F("PIN"))) {
      const char *pin = request->arg(F("PIN")).c_str();
      unsigned pinLen = strlen(pin);
      if (pinLen == 4 || pinLen == 0) {
        unsigned numZeros = 0;
        for (unsigned i = 0; i < pinLen; i++) numZeros += (pin[i] == '0');
        if (numZeros < pinLen || pinLen == 0) { // ignore 0000 input (placeholder)
          strlcpy(settingsPIN, pin, 5);
        }
        settingsPIN[4] = 0;
      }
    }

    bool pwdCorrect = !otaLock; //always allow access if ota not locked
    if (request->hasArg(F("OP")))
    {
      if (otaLock && strcmp(otaPass,request->arg(F("OP")).c_str()) == 0)
      {
        // brute force protection: do not unlock even if correct if last save was less than 3 seconds ago
        if (millis() - lastEditTime > PIN_RETRY_COOLDOWN) pwdCorrect = true;
      }
      if (!otaLock && request->arg(F("OP")).length() > 0)
      {
        strlcpy(otaPass,request->arg(F("OP")).c_str(), 33); // set new OTA password
      }
    }

    if (pwdCorrect) //allow changes if correct pwd or no ota active
    {
      otaLock = request->hasArg(F("NO"));
      wifiLock = request->hasArg(F("OW"));
      #ifndef WLED_DISABLE_OTA
      aOtaEnabled = request->hasArg(F("AO"));
      #endif
      //createEditHandler(correctPIN && !otaLock);
      otaSameSubnet = request->hasArg(F("SU"));
    }
  }

  #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
  if (subPage == SUBPAGE_DMX)
  {
    int t = request->arg(F("PU")).toInt();
    if (t >= 0  && t <= 63999) e131ProxyUniverse = t;

    t = request->arg(F("CN")).toInt();
    if (t>0 && t<16) {
      DMXChannels = t;
    }
    t = request->arg(F("CS")).toInt();
    if (t>0 && t<513) {
      DMXStart = t;
    }
    t = request->arg(F("CG")).toInt();
    if (t>0 && t<513) {
      DMXGap = t;
    }
    t = request->arg(F("SL")).toInt();
    if (t>=0 && t < MAX_LEDS) {
      DMXStartLED = t;
    }
    for (int i=0; i<15; i++) {
      String argname = "CH" + String((i+1));
      t = request->arg(argname).toInt();
      DMXFixtureMap[i] = t;
    }
  }
  #endif

  //USERMODS
  if (subPage == SUBPAGE_UM)
  {
    if (!requestJSONBufferLock(5)) {
      request->deferResponse();
      return;
    }

    // global I2C & SPI pins
    int8_t hw_sda_pin  = !request->arg(F("SDA")).length() ? -1 : (int)request->arg(F("SDA")).toInt();
    int8_t hw_scl_pin  = !request->arg(F("SCL")).length() ? -1 : (int)request->arg(F("SCL")).toInt();
    if (i2c_sda != hw_sda_pin || i2c_scl != hw_scl_pin) {
      // only if pins changed
      uint8_t old_i2c[2] = { static_cast<uint8_t>(i2c_scl), static_cast<uint8_t>(i2c_sda) };
      PinManager::deallocateMultiplePins(old_i2c, 2, PinOwner::HW_I2C); // just in case deallocation of old pins

      PinManagerPinType i2c[2] = { { hw_sda_pin, true }, { hw_scl_pin, true } };
      if (hw_sda_pin >= 0 && hw_scl_pin >= 0 && PinManager::allocateMultiplePins(i2c, 2, PinOwner::HW_I2C)) {
        i2c_sda = hw_sda_pin;
        i2c_scl = hw_scl_pin;
        // no bus re-initialisation as usermods do not get any notification
        //Wire.begin(i2c_sda, i2c_scl);
      } else {
        // there is no Wire.end()
        DEBUG_PRINTLN(F("Could not allocate I2C pins."));
        i2c_sda = -1;
        i2c_scl = -1;
      }
    }
    int8_t hw_mosi_pin = !request->arg(F("MOSI")).length() ? -1 : (int)request->arg(F("MOSI")).toInt();
    int8_t hw_miso_pin = !request->arg(F("MISO")).length() ? -1 : (int)request->arg(F("MISO")).toInt();
    int8_t hw_sclk_pin = !request->arg(F("SCLK")).length() ? -1 : (int)request->arg(F("SCLK")).toInt();
    #ifdef ESP8266
    // cannot change pins on ESP8266
    if (hw_mosi_pin >= 0 && hw_mosi_pin != HW_PIN_DATASPI)  hw_mosi_pin = HW_PIN_DATASPI;
    if (hw_miso_pin >= 0 && hw_miso_pin != HW_PIN_MISOSPI)  hw_mosi_pin = HW_PIN_MISOSPI;
    if (hw_sclk_pin >= 0 && hw_sclk_pin != HW_PIN_CLOCKSPI) hw_sclk_pin = HW_PIN_CLOCKSPI;
    #endif
    if (spi_mosi != hw_mosi_pin || spi_miso != hw_miso_pin || spi_sclk != hw_sclk_pin) {
      // only if pins changed
      uint8_t old_spi[3] = { static_cast<uint8_t>(spi_mosi), static_cast<uint8_t>(spi_miso), static_cast<uint8_t>(spi_sclk) };
      PinManager::deallocateMultiplePins(old_spi, 3, PinOwner::HW_SPI); // just in case deallocation of old pins
      PinManagerPinType spi[3] = { { hw_mosi_pin, true }, { hw_miso_pin, true }, { hw_sclk_pin, true } };
      if (hw_mosi_pin >= 0 && hw_sclk_pin >= 0 && PinManager::allocateMultiplePins(spi, 3, PinOwner::HW_SPI)) {
        spi_mosi = hw_mosi_pin;
        spi_miso = hw_miso_pin;
        spi_sclk = hw_sclk_pin;
        // no bus re-initialisation as usermods do not get any notification
        //SPI.end();
        #ifdef ESP32
        //SPI.begin(spi_sclk, spi_miso, spi_mosi);
        #else
        //SPI.begin();
        #endif
      } else {
        //SPI.end();
        DEBUG_PRINTLN(F("Could not allocate SPI pins."));
        spi_mosi = -1;
        spi_miso = -1;
        spi_sclk = -1;
      }
    }

    JsonObject um = pDoc->createNestedObject("um");

    size_t args = request->args();
    unsigned j=0;
    for (size_t i=0; i<args; i++) {
      String name = request->argName(i);
      String value = request->arg(i);

      // POST request parameters are combined as <usermodname>_<usermodparameter>
      int umNameEnd = name.indexOf(":");
      if (umNameEnd<1) continue;  // parameter does not contain ":" or on 1st place -> wrong

      JsonObject mod = um[name.substring(0,umNameEnd)]; // get a usermod JSON object
      if (mod.isNull()) {
        mod = um.createNestedObject(name.substring(0,umNameEnd)); // if it does not exist create it
      }
      DEBUG_PRINT(name.substring(0,umNameEnd));
      DEBUG_PRINT(":");
      name = name.substring(umNameEnd+1); // remove mod name from string

      // if the resulting name still contains ":" this means nested object
      JsonObject subObj;
      int umSubObj = name.indexOf(":");
      DEBUG_PRINTF_P(PSTR("(%d):"),umSubObj);
      if (umSubObj>0) {
        subObj = mod[name.substring(0,umSubObj)];
        if (subObj.isNull())
          subObj = mod.createNestedObject(name.substring(0,umSubObj));
        name = name.substring(umSubObj+1); // remove nested object name from string
      } else {
        subObj = mod;
      }
      DEBUG_PRINT(name);

      // check if parameters represent array
      if (name.endsWith("[]")) {
        name.replace("[]","");
        value.replace(",",".");      // just in case conversion
        if (!subObj[name].is<JsonArray>()) {
          JsonArray ar = subObj.createNestedArray(name);
          if (value.indexOf(".") >= 0) ar.add(value.toFloat());  // we do have a float
          else                         ar.add(value.toInt());    // we may have an int
          j=0;
        } else {
          if (value.indexOf(".") >= 0) subObj[name].add(value.toFloat());  // we do have a float
          else                         subObj[name].add(value.toInt());    // we may have an int
          j++;
        }
        DEBUG_PRINTF_P(PSTR("[%d] = %s\n"), j, value.c_str());
      } else {
        // we are using a hidden field with the same name as our parameter (!before the actual parameter!)
        // to describe the type of parameter (text,float,int), for boolean parameters the first field contains "off"
        // so checkboxes have one or two fields (first is always "false", existence of second depends on checkmark and may be "true")
        if (subObj[name].isNull()) {
          // the first occurrence of the field describes the parameter type (used in next loop)
          if (value == "false") subObj[name] = false; // checkboxes may have only one field
          else                  subObj[name] = value;
        } else {
          String type = subObj[name].as<String>();  // get previously stored value as a type
          if (subObj[name].is<bool>())   subObj[name] = true;   // checkbox/boolean
          else if (type == "number") {
            value.replace(",",".");      // just in case conversion
            if (value.indexOf(".") >= 0) subObj[name] = value.toFloat();  // we do have a float
            else                         subObj[name] = value.toInt();    // we may have an int
          } else if (type == "int")      subObj[name] = value.toInt();
          else                           subObj[name] = value;  // text fields
        }
        DEBUG_PRINTF_P(PSTR(" = %s\n"), value.c_str());
      }
    }
    UsermodManager::readFromConfig(um);  // force change of usermod parameters
    DEBUG_PRINTLN(F("Done re-init UsermodManager::"));
    releaseJSONBufferLock();
  }

  #ifndef WLED_DISABLE_2D
  //2D panels
  if (subPage == SUBPAGE_2D)
  {
    strip.isMatrix = request->arg(F("SOMP")).toInt();
    strip.panel.clear();
    if (strip.isMatrix) {
      unsigned panels = constrain(request->arg(F("MPC")).toInt(), 1, WLED_MAX_PANELS);
      strip.panel.reserve(panels); // pre-allocate memory
      for (unsigned i=0; i<panels; i++) {
        WS2812FX::Panel p;
        char pO[8] = { '\0' };
        snprintf_P(pO, 7, PSTR("P%d"), i);       // WLED_MAX_PANELS is less than 100 so pO will always only be 4 characters or less
        pO[7] = '\0';
        unsigned l = strlen(pO);
        // create P0B, P1B, ..., P63B, etc for other PxxX
        pO[l] = 'B'; if (!request->hasArg(pO)) break;
        pO[l] = 'B'; p.bottomStart = request->arg(pO).toInt();
        pO[l] = 'R'; p.rightStart  = request->arg(pO).toInt();
        pO[l] = 'V'; p.vertical    = request->arg(pO).toInt();
        pO[l] = 'S'; p.serpentine  = request->hasArg(pO);
        pO[l] = 'X'; p.xOffset     = request->arg(pO).toInt();
        pO[l] = 'Y'; p.yOffset     = request->arg(pO).toInt();
        pO[l] = 'W'; p.width       = request->arg(pO).toInt();
        pO[l] = 'H'; p.height      = request->arg(pO).toInt();
        strip.panel.push_back(p);
      }
    }
    strip.panel.shrink_to_fit();  // release unused memory
    strip.deserializeMap(); // (re)load default ledmap (will also setUpMatrix() if ledmap does not exist)
    strip.makeAutoSegments(true); // force re-creation of segments
  }
  #endif

  lastEditTime = millis();
  // do not save if factory reset or LED settings (which are saved after LED re-init)
  configNeedsWrite = subPage != SUBPAGE_LEDS && !(subPage == SUBPAGE_SEC && doReboot);
  if (subPage == SUBPAGE_UM) doReboot = request->hasArg(F("RBT")); // prevent race condition on dual core system (set reboot here, after configNeedsWrite has been set)
  #ifndef WLED_DISABLE_ALEXA
  if (subPage == SUBPAGE_SYNC) alexaInit();
  #endif
}


//HTTP API request parser
bool handleSet(AsyncWebServerRequest *request, const String& req, bool apply)
{
  if (!(req.indexOf("win") >= 0)) return false;

  int pos = 0;
  DEBUG_PRINTF_P(PSTR("API req: %s\n"), req.c_str());

  //segment select (sets main segment)
  pos = req.indexOf(F("SM="));
  if (pos > 0 && !realtimeMode) {
    strip.setMainSegmentId(getNumVal(req, pos));
  }

  byte selectedSeg = strip.getFirstSelectedSegId();

  bool singleSegment = false;

  pos = req.indexOf(F("SS="));
  if (pos > 0) {
    unsigned t = getNumVal(req, pos);
    if (t < strip.getSegmentsNum()) {
      selectedSeg = t;
      singleSegment = true;
    }
  }

  Segment& selseg = strip.getSegment(selectedSeg);
  pos = req.indexOf(F("SV=")); //segment selected
  if (pos > 0) {
    unsigned t = getNumVal(req, pos);
    if (t == 2) for (unsigned i = 0; i < strip.getSegmentsNum(); i++) strip.getSegment(i).selected = false; // unselect other segments
    selseg.selected = t;
  }

  // temporary values, write directly to segments, globals are updated by setValuesFromFirstSelectedSeg()
  uint32_t col0    = selseg.colors[0];
  uint32_t col1    = selseg.colors[1];
  uint32_t col2    = selseg.colors[2];
  byte colIn[4]    = {R(col0), G(col0), B(col0), W(col0)};
  byte colInSec[4] = {R(col1), G(col1), B(col1), W(col1)};
  byte effectIn    = selseg.mode;
  byte speedIn     = selseg.speed;
  byte intensityIn = selseg.intensity;
  byte paletteIn   = selseg.palette;
  byte custom1In   = selseg.custom1;
  byte custom2In   = selseg.custom2;
  byte custom3In   = selseg.custom3;
  byte check1In    = selseg.check1;
  byte check2In    = selseg.check2;
  byte check3In    = selseg.check3;
  uint16_t startI  = selseg.start;
  uint16_t stopI   = selseg.stop;
  uint16_t startY  = selseg.startY;
  uint16_t stopY   = selseg.stopY;
  uint8_t  grpI    = selseg.grouping;
  uint16_t spcI    = selseg.spacing;
  pos = req.indexOf(F("&S=")); //segment start
  if (pos > 0) {
    startI = std::abs(getNumVal(req, pos));
  }
  pos = req.indexOf(F("S2=")); //segment stop
  if (pos > 0) {
    stopI = std::abs(getNumVal(req, pos));
  }
  pos = req.indexOf(F("GP=")); //segment grouping
  if (pos > 0) {
    grpI = std::max(1,getNumVal(req, pos));
  }
  pos = req.indexOf(F("SP=")); //segment spacing
  if (pos > 0) {
    spcI = std::max(0,getNumVal(req, pos));
  }
  strip.suspend(); // must suspend strip operations before changing geometry
  selseg.setGeometry(startI, stopI, grpI, spcI, UINT16_MAX, startY, stopY, selseg.map1D2D);
  strip.resume();

  pos = req.indexOf(F("RV=")); //Segment reverse
  if (pos > 0) selseg.reverse = req.charAt(pos+3) != '0';

  pos = req.indexOf(F("MI=")); //Segment mirror
  if (pos > 0) selseg.mirror = req.charAt(pos+3) != '0';

  pos = req.indexOf(F("SB=")); //Segment brightness/opacity
  if (pos > 0) {
    byte segbri = getNumVal(req, pos);
    selseg.setOption(SEG_OPTION_ON, segbri); // use transition
    if (segbri) {
      selseg.setOpacity(segbri);
    }
  }

  pos = req.indexOf(F("SW=")); //segment power
  if (pos > 0) {
    switch (getNumVal(req, pos)) {
      case 0:  selseg.setOption(SEG_OPTION_ON, false);      break; // use transition
      case 1:  selseg.setOption(SEG_OPTION_ON, true);       break; // use transition
      default: selseg.setOption(SEG_OPTION_ON, !selseg.on); break; // use transition
    }
  }

  pos = req.indexOf(F("PS=")); //saves current in preset
  if (pos > 0) savePreset(getNumVal(req, pos));

  pos = req.indexOf(F("P1=")); //sets first preset for cycle
  if (pos > 0) presetCycMin = getNumVal(req, pos);

  pos = req.indexOf(F("P2=")); //sets last preset for cycle
  if (pos > 0) presetCycMax = getNumVal(req, pos);

  //apply preset
  if (updateVal(req.c_str(), "PL=", presetCycCurr, presetCycMin, presetCycMax)) {
    applyPreset(presetCycCurr);
  }

  pos = req.indexOf(F("NP")); //advances to next preset in a playlist
  if (pos > 0) doAdvancePlaylist = true;
  
  //set brightness
  updateVal(req.c_str(), "&A=", bri);

  bool col0Changed = false, col1Changed = false, col2Changed = false;
  //set colors
  col0Changed |= updateVal(req.c_str(), "&R=", colIn[0]);
  col0Changed |= updateVal(req.c_str(), "&G=", colIn[1]);
  col0Changed |= updateVal(req.c_str(), "&B=", colIn[2]);
  col0Changed |= updateVal(req.c_str(), "&W=", colIn[3]);

  col1Changed |= updateVal(req.c_str(), "R2=", colInSec[0]);
  col1Changed |= updateVal(req.c_str(), "G2=", colInSec[1]);
  col1Changed |= updateVal(req.c_str(), "B2=", colInSec[2]);
  col1Changed |= updateVal(req.c_str(), "W2=", colInSec[3]);

  #ifdef WLED_ENABLE_LOXONE
  //lox parser
  pos = req.indexOf(F("LX=")); // Lox primary color
  if (pos > 0) {
    int lxValue = getNumVal(req, pos);
    if (parseLx(lxValue, colIn)) {
      bri = 255;
      nightlightActive = false; //always disable nightlight when toggling
      col0Changed = true;
    }
  }
  pos = req.indexOf(F("LY=")); // Lox secondary color
  if (pos > 0) {
    int lxValue = getNumVal(req, pos);
    if(parseLx(lxValue, colInSec)) {
      bri = 255;
      nightlightActive = false; //always disable nightlight when toggling
      col1Changed = true;
    }
  }
  #endif

  //set hue
  pos = req.indexOf(F("HU="));
  if (pos > 0) {
    uint16_t temphue = getNumVal(req, pos);
    byte tempsat = 255;
    pos = req.indexOf(F("SA="));
    if (pos > 0) {
      tempsat = getNumVal(req, pos);
    }
    byte sec = req.indexOf(F("H2"));
    colorHStoRGB(temphue, tempsat, (sec>0) ? colInSec : colIn);
    col0Changed |= (!sec); col1Changed |= sec;
  }

  //set white spectrum (kelvin)
  pos = req.indexOf(F("&K="));
  if (pos > 0) {
    byte sec = req.indexOf(F("K2"));
    colorKtoRGB(getNumVal(req, pos), (sec>0) ? colInSec : colIn);
    col0Changed |= (!sec); col1Changed |= sec;
  }

  //set color from HEX or 32bit DEC
  pos = req.indexOf(F("CL="));
  if (pos > 0) {
    colorFromDecOrHexString(colIn, (char*)req.substring(pos + 3).c_str());
    col0Changed = true;
  }
  pos = req.indexOf(F("C2="));
  if (pos > 0) {
    colorFromDecOrHexString(colInSec, (char*)req.substring(pos + 3).c_str());
    col1Changed = true;
  }
  pos = req.indexOf(F("C3="));
  if (pos > 0) {
    byte tmpCol[4];
    colorFromDecOrHexString(tmpCol, (char*)req.substring(pos + 3).c_str());
    col2 = RGBW32(tmpCol[0], tmpCol[1], tmpCol[2], tmpCol[3]);
    selseg.setColor(2, col2); // defined above (SS= or main)
    col2Changed = true;
  }

  //set to random hue SR=0->1st SR=1->2nd
  pos = req.indexOf(F("SR"));
  if (pos > 0) {
    byte sec = getNumVal(req, pos);
    setRandomColor(sec? colInSec : colIn);
    col0Changed |= (!sec); col1Changed |= sec;
  }

  // apply colors to selected segment, and all selected segments if applicable
  if (col0Changed) {
    col0 = RGBW32(colIn[0], colIn[1], colIn[2], colIn[3]);
    selseg.setColor(0, col0);
  }

  if (col1Changed) {
    col1 = RGBW32(colInSec[0], colInSec[1], colInSec[2], colInSec[3]);
    selseg.setColor(1, col1);
  }

  //swap 2nd & 1st
  pos = req.indexOf(F("SC"));
  if (pos > 0) {
    std::swap(col0,col1);
    col0Changed = col1Changed = true;
  }

  bool fxModeChanged = false, speedChanged = false, intensityChanged = false, paletteChanged = false;
  bool custom1Changed = false, custom2Changed = false, custom3Changed = false, check1Changed = false, check2Changed = false, check3Changed = false;
  // set effect parameters
  if (updateVal(req.c_str(), "FX=", effectIn, 0, strip.getModeCount()-1)) {
    if (request != nullptr) unloadPlaylist(); // unload playlist if changing FX using web request
    fxModeChanged = true;
  }
  speedChanged     = updateVal(req.c_str(), "SX=", speedIn);
  intensityChanged = updateVal(req.c_str(), "IX=", intensityIn);
  paletteChanged   = updateVal(req.c_str(), "FP=", paletteIn, 0, getPaletteCount()-1);
  custom1Changed   = updateVal(req.c_str(), "X1=", custom1In);
  custom2Changed   = updateVal(req.c_str(), "X2=", custom2In);
  custom3Changed   = updateVal(req.c_str(), "X3=", custom3In);
  check1Changed    = updateVal(req.c_str(), "M1=", check1In);
  check2Changed    = updateVal(req.c_str(), "M2=", check2In);
  check3Changed    = updateVal(req.c_str(), "M3=", check3In);

  stateChanged |= (fxModeChanged || speedChanged || intensityChanged || paletteChanged || custom1Changed || custom2Changed || custom3Changed || check1Changed || check2Changed || check3Changed);

  // apply to main and all selected segments to prevent #1618.
  for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
    Segment& seg = strip.getSegment(i);
    if (i != selectedSeg && (singleSegment || !seg.isActive() || !seg.isSelected())) continue; // skip non main segments if not applying to all
    if (fxModeChanged)    seg.setMode(effectIn, req.indexOf(F("FXD="))>0);  // apply defaults if FXD= is specified
    if (speedChanged)     seg.speed     = speedIn;
    if (intensityChanged) seg.intensity = intensityIn;
    if (paletteChanged)   seg.setPalette(paletteIn);
    if (col0Changed)      seg.setColor(0, col0);
    if (col1Changed)      seg.setColor(1, col1);
    if (col2Changed)      seg.setColor(2, col2);
    if (custom1Changed)   seg.custom1   = custom1In;
    if (custom2Changed)   seg.custom2   = custom2In;
    if (custom3Changed)   seg.custom3   = custom3In;
    if (check1Changed)    seg.check1    = (bool)check1In;
    if (check2Changed)    seg.check2    = (bool)check2In;
    if (check3Changed)    seg.check3    = (bool)check3In;
  }

  //set advanced overlay
  pos = req.indexOf(F("OL="));
  if (pos > 0) {
    overlayCurrent = getNumVal(req, pos);
  }

  //apply macro (deprecated, added for compatibility with pre-0.11 automations)
  pos = req.indexOf(F("&M="));
  if (pos > 0) {
    applyPreset(getNumVal(req, pos) + 16);
  }

  //toggle send UDP direct notifications
  pos = req.indexOf(F("SN="));
  if (pos > 0) notifyDirect = (req.charAt(pos+3) != '0');

  //toggle receive UDP direct notifications
  pos = req.indexOf(F("RN="));
  if (pos > 0) receiveGroups = (req.charAt(pos+3) != '0') ? receiveGroups | 1 : receiveGroups & 0xFE;

  //receive live data via UDP/Hyperion
  pos = req.indexOf(F("RD="));
  if (pos > 0) receiveDirect = (req.charAt(pos+3) != '0');

  //main toggle on/off (parse before nightlight, #1214)
  pos = req.indexOf(F("&T="));
  if (pos > 0) {
    nightlightActive = false; //always disable nightlight when toggling
    switch (getNumVal(req, pos))
    {
      case 0: if (bri != 0){briLast = bri; bri = 0;} break; //off, only if it was previously on
      case 1: if (bri == 0) bri = briLast; break; //on, only if it was previously off
      default: toggleOnOff(); //toggle
    }
  }

  //toggle nightlight mode
  bool aNlDef = false;
  if (req.indexOf(F("&ND")) > 0) aNlDef = true;
  pos = req.indexOf(F("NL="));
  if (pos > 0)
  {
    if (req.charAt(pos+3) == '0')
    {
      nightlightActive = false;
    } else {
      nightlightActive = true;
      if (!aNlDef) nightlightDelayMins = getNumVal(req, pos);
      else         nightlightDelayMins = nightlightDelayMinsDefault;
      nightlightStartTime = millis();
    }
  } else if (aNlDef)
  {
    nightlightActive = true;
    nightlightDelayMins = nightlightDelayMinsDefault;
    nightlightStartTime = millis();
  }

  //set nightlight target brightness
  pos = req.indexOf(F("NT="));
  if (pos > 0) {
    nightlightTargetBri = getNumVal(req, pos);
    nightlightActiveOld = false; //re-init
  }

  //toggle nightlight fade
  pos = req.indexOf(F("NF="));
  if (pos > 0)
  {
    nightlightMode = getNumVal(req, pos);

    nightlightActiveOld = false; //re-init
  }
  if (nightlightMode > NL_MODE_SUN) nightlightMode = NL_MODE_SUN;

  pos = req.indexOf(F("TT="));
  if (pos > 0) transitionDelay = getNumVal(req, pos);
  strip.setTransition(transitionDelay);

  //set time (unix timestamp)
  pos = req.indexOf(F("ST="));
  if (pos > 0) {
    setTimeFromAPI(getNumVal(req, pos));
  }

  //set countdown goal (unix timestamp)
  pos = req.indexOf(F("CT="));
  if (pos > 0) {
    countdownTime = getNumVal(req, pos);
    if (countdownTime - toki.second() > 0) countdownOverTriggered = false;
  }

  pos = req.indexOf(F("LO="));
  if (pos > 0) {
    realtimeOverride = getNumVal(req, pos);
    if (realtimeOverride > 2) realtimeOverride = REALTIME_OVERRIDE_ALWAYS;
    if (realtimeMode && useMainSegmentOnly) {
      strip.getMainSegment().freeze = !realtimeOverride;
      realtimeOverride = REALTIME_OVERRIDE_NONE;  // ignore request for override if using main segment only
    }
  }

  pos = req.indexOf(F("RB"));
  if (pos > 0) doReboot = true;

  // clock mode, 0: normal, 1: countdown
  pos = req.indexOf(F("NM="));
  if (pos > 0) countdownMode = (req.charAt(pos+3) != '0');

  pos = req.indexOf(F("U0=")); //user var 0
  if (pos > 0) {
    userVar0 = getNumVal(req, pos);
  }

  pos = req.indexOf(F("U1=")); //user var 1
  if (pos > 0) {
    userVar1 = getNumVal(req, pos);
  }
  // you can add more if you need

  // global colPri[], effectCurrent, ... are updated in stateChanged()
  if (!apply) return true; // when called by JSON API, do not call colorUpdated() here

  pos = req.indexOf(F("&NN")); //do not send UDP notifications this time
  stateUpdated((pos > 0) ? CALL_MODE_NO_NOTIFY : CALL_MODE_DIRECT_CHANGE);

  // internal call, does not send XML response
  pos = req.indexOf(F("IN"));
  if ((request != nullptr) && (pos < 1)) {
    auto response = request->beginResponseStream("text/xml");
    XML_response(*response);
    request->send(response);
  }

  return true;
}
