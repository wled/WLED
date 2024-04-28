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
    strlcpy(clientSSID,request->arg(F("CS")).c_str(), 33);

    if (!isAsterisksOnly(request->arg(F("CP")).c_str(), 65)) strlcpy(clientPass, request->arg(F("CP")).c_str(), 65);

    strlcpy(cmDNS, request->arg(F("CM")).c_str(), 33);

    apBehavior = request->arg(F("AB")).toInt();
    strlcpy(apSSID, request->arg(F("AS")).c_str(), 33);
    apHide = request->hasArg(F("AH"));
    int passlen = request->arg(F("AP")).length();
    if (passlen == 0 || (passlen > 7 && !isAsterisksOnly(request->arg(F("AP")).c_str(), 65))) strlcpy(apPass, request->arg(F("AP")).c_str(), 65);
    int t = request->arg(F("AC")).toInt(); if (t > 0 && t < 14) apChannel = t;

    force802_3g = request->hasArg(F("FG"));
    noWifiSleep = request->hasArg(F("WS"));

    #ifndef WLED_DISABLE_ESPNOW
    enable_espnow_remote = request->hasArg(F("RE"));
    strlcpy(linked_remote,request->arg(F("RMAC")).c_str(), 13);

    //Normalize MAC format to lowercase
    strlcpy(linked_remote,strlwr(linked_remote), 13);
    #endif

    #ifdef WLED_USE_ETHERNET
    ethernetType = request->arg(F("ETH")).toInt();
    WLED::instance().initEthernet();
    #endif

    char k[3]; k[2] = 0;
    for (int i = 0; i<4; i++)
    {
      k[1] = i+48;//ascii 0,1,2,3

      k[0] = 'I'; //static IP
      staticIP[i] = request->arg(k).toInt();

      k[0] = 'G'; //gateway
      staticGateway[i] = request->arg(k).toInt();

      k[0] = 'S'; //subnet
      staticSubnet[i] = request->arg(k).toInt();
    }
  }

  //LED SETTINGS
  if (subPage == SUBPAGE_LEDS)
  {
    int t = 0;

    if (rlyPin>=0 && pinManager.isPinAllocated(rlyPin, PinOwner::Relay)) {
       pinManager.deallocatePin(rlyPin, PinOwner::Relay);
    }
    if (irPin>=0 && pinManager.isPinAllocated(irPin, PinOwner::IR)) {
       pinManager.deallocatePin(irPin, PinOwner::IR);
    }
    for (uint8_t s=0; s<WLED_MAX_BUTTONS; s++) {
      if (btnPin[s]>=0 && pinManager.isPinAllocated(btnPin[s], PinOwner::Button)) {
        pinManager.deallocatePin(btnPin[s], PinOwner::Button);
      }
    }

    uint8_t colorOrder, type, skip, awmode, channelSwap;
    uint16_t length, start;
    uint8_t pins[5] = {255, 255, 255, 255, 255};

    autoSegments = request->hasArg(F("MS"));
    correctWB = request->hasArg(F("CCT"));
    cctFromRgb = request->hasArg(F("CR"));
    strip.cctBlending = request->arg(F("CB")).toInt();
    Bus::setCCTBlend(strip.cctBlending);
    Bus::setGlobalAWMode(request->arg(F("AW")).toInt());
    strip.setTargetFps(request->arg(F("FR")).toInt());
    useGlobalLedBuffer = request->hasArg(F("LD"));

    bool busesChanged = false;
    for (uint8_t s = 0; s < WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES; s++) {
      char lp[4] = "L0"; lp[2] = 48+s; lp[3] = 0; //ascii 0-9 //strip data pin
      char lc[4] = "LC"; lc[2] = 48+s; lc[3] = 0; //strip length
      char co[4] = "CO"; co[2] = 48+s; co[3] = 0; //strip color order
      char lt[4] = "LT"; lt[2] = 48+s; lt[3] = 0; //strip type
      char ls[4] = "LS"; ls[2] = 48+s; ls[3] = 0; //strip start LED
      char cv[4] = "CV"; cv[2] = 48+s; cv[3] = 0; //strip reverse
      char sl[4] = "SL"; sl[2] = 48+s; sl[3] = 0; //skip first N LEDs
      char rf[4] = "RF"; rf[2] = 48+s; rf[3] = 0; //refresh required
      char aw[4] = "AW"; aw[2] = 48+s; aw[3] = 0; //auto white mode
      char wo[4] = "WO"; wo[2] = 48+s; wo[3] = 0; //channel swap
      char sp[4] = "SP"; sp[2] = 48+s; sp[3] = 0; //bus clock speed (DotStar & PWM)
      if (!request->hasArg(lp)) {
        DEBUG_PRINT(F("No data for "));
        DEBUG_PRINTLN(s);
        break;
      }
      for (uint8_t i = 0; i < 5; i++) {
        lp[1] = 48+i;
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
      uint16_t freqHz = request->arg(sp).toInt();
      if (type > TYPE_ONOFF && type < 49) {
        switch (freqHz) {
          case 0 : freqHz = WLED_PWM_FREQ/3; break;
          case 1 : freqHz = WLED_PWM_FREQ/2; break;
          default:
          case 2 : freqHz = WLED_PWM_FREQ;   break;
          case 3 : freqHz = WLED_PWM_FREQ*2; break;
          case 4 : freqHz = WLED_PWM_FREQ*3; break;
        }
      } else if (type > 48 && type < 64) {
        switch (freqHz) {
          default:
          case 0 : freqHz =  1000; break;
          case 1 : freqHz =  2000; break;
          case 2 : freqHz =  5000; break;
          case 3 : freqHz = 10000; break;
          case 4 : freqHz = 20000; break;
        }
      } else {
        freqHz = 0;
      }
      channelSwap = Bus::hasWhite(type) ? request->arg(wo).toInt() : 0;
      type |= request->hasArg(rf) << 7; // off refresh override
      // actual finalization is done in WLED::loop() (removing old busses and adding new)
      // this may happen even before this loop is finished so we do "doInitBusses" after the loop
      if (busConfigs[s] != nullptr) delete busConfigs[s];
      busConfigs[s] = new BusConfig(type, pins, start, length, colorOrder | (channelSwap<<4), request->hasArg(cv), skip, awmode, freqHz, useGlobalLedBuffer);
      busesChanged = true;
    }
    //doInitBusses = busesChanged; // we will do that below to ensure all input data is processed

    ColorOrderMap com = {};
    for (uint8_t s = 0; s < WLED_MAX_COLOR_ORDER_MAPPINGS; s++) {
      char xs[4] = "XS"; xs[2] = 48+s; xs[3] = 0; //start LED
      char xc[4] = "XC"; xc[2] = 48+s; xc[3] = 0; //strip length
      char xo[4] = "XO"; xo[2] = 48+s; xo[3] = 0; //color order
      if (request->hasArg(xs)) {
        start = request->arg(xs).toInt();
        length = request->arg(xc).toInt();
        colorOrder = request->arg(xo).toInt();
        com.add(start, length, colorOrder);
      }
    }
    busses.updateColorOrderMap(com);

    // update other pins
    int hw_ir_pin = request->arg(F("IR")).toInt();
    if (pinManager.allocatePin(hw_ir_pin,false, PinOwner::IR)) {
      irPin = hw_ir_pin;
    } else {
      irPin = -1;
    }
    irEnabled = request->arg(F("IT")).toInt();
    irApplyToAllSelected = !request->hasArg(F("MSO"));

    int hw_rly_pin = request->arg(F("RL")).toInt();
    if (pinManager.allocatePin(hw_rly_pin,true, PinOwner::Relay)) {
      rlyPin = hw_rly_pin;
    } else {
      rlyPin = -1;
    }
    rlyMde = (bool)request->hasArg(F("RM"));

    disablePullUp = (bool)request->hasArg(F("IP"));
    for (uint8_t i=0; i<WLED_MAX_BUTTONS; i++) {
      char bt[4] = "BT"; bt[2] = (i<10?48:55)+i; bt[3] = 0; // button pin (use A,B,C,... if WLED_MAX_BUTTONS>10)
      char be[4] = "BE"; be[2] = (i<10?48:55)+i; be[3] = 0; // button type (use A,B,C,... if WLED_MAX_BUTTONS>10)
      int hw_btn_pin = request->arg(bt).toInt();
      if (pinManager.allocatePin(hw_btn_pin,false,PinOwner::Button)) {
        btnPin[i] = hw_btn_pin;
        buttonType[i] = request->arg(be).toInt();
      #ifdef ARDUINO_ARCH_ESP32
        // ESP32 only: check that analog button pin is a valid ADC gpio
        if (((buttonType[i] == BTN_TYPE_ANALOG) || (buttonType[i] == BTN_TYPE_ANALOG_INVERTED)) && (digitalPinToAnalogChannel(btnPin[i]) < 0))
        {
          // not an ADC analog pin
          if (btnPin[i] >= 0) DEBUG_PRINTF("PIN ALLOC error: GPIO%d for analog button #%d is not an analog pin!\n", btnPin[i], i);
          btnPin[i] = -1;
          pinManager.deallocatePin(hw_btn_pin,PinOwner::Button);
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
    touchThreshold = request->arg(F("TT")).toInt();

    strip.ablMilliampsMax = request->arg(F("MA")).toInt();
    strip.milliampsPerLed = request->arg(F("LA")).toInt();

    briS = request->arg(F("CA")).toInt();

    turnOnAtBoot = request->hasArg(F("BO"));
    t = request->arg(F("BP")).toInt();
    if (t <= 250) bootPreset = t;
    gammaCorrectBri = request->hasArg(F("GB"));
    gammaCorrectCol = request->hasArg(F("GC"));
    gammaCorrectVal = request->arg(F("GV")).toFloat();
    if (gammaCorrectVal > 1.0f && gammaCorrectVal <= 3)
      NeoGammaWLEDMethod::calcGammaTable(gammaCorrectVal);
    else {
      gammaCorrectVal = 1.0f; // no gamma correction
      gammaCorrectBri = false;
      gammaCorrectCol = false;
    }

    fadeTransition = request->hasArg(F("TF"));
    modeBlending = request->hasArg(F("EB"));
    t = request->arg(F("TD")).toInt();
    if (t >= 0) transitionDelayDefault = t;
    strip.paletteFade = request->hasArg(F("PF"));
    t = request->arg(F("TP")).toInt();
    randomPaletteChangeTime = MIN(255,MAX(1,t));

    nightlightTargetBri = request->arg(F("TB")).toInt();
    t = request->arg(F("TL")).toInt();
    if (t > 0) nightlightDelayMinsDefault = t;
    nightlightDelayMins = nightlightDelayMinsDefault;
    nightlightMode = request->arg(F("TW")).toInt();

    t = request->arg(F("PB")).toInt();
    if (t >= 0 && t < 4) strip.paletteBlend = t;
    t = request->arg(F("BF")).toInt();
    if (t > 0) briMultiplier = t;

    doInitBusses = busesChanged;
  }

  //UI
  if (subPage == SUBPAGE_UI)
  {
    strlcpy(serverDescription, request->arg(F("DS")).c_str(), 33);
    syncToggleReceive = request->hasArg(F("ST"));
  #ifdef WLED_ENABLE_SIMPLE_UI
    if (simplifiedUI ^ request->hasArg(F("SU"))) {
      // UI selection changed, invalidate browser cache
      cacheInvalidate++;
    }
    simplifiedUI = request->hasArg(F("SU"));
  #endif
    DEBUG_PRINTLN(F("Enumerating ledmaps"));
    enumerateLedmaps();
    DEBUG_PRINTLN(F("Loading custom palettes"));
    strip.loadCustomPalettes(); // (re)load all custom palettes
  }

  //SYNC
  if (subPage == SUBPAGE_SYNC)
  {
    int t = request->arg(F("UP")).toInt();
    if (t > 0) udpPort = t;
    t = request->arg(F("U2")).toInt();
    if (t > 0) udpPort2 = t;

    syncGroups = request->arg(F("GS")).toInt();
    receiveGroups = request->arg(F("GR")).toInt();

    receiveNotificationBrightness = request->hasArg(F("RB"));
    receiveNotificationColor = request->hasArg(F("RC"));
    receiveNotificationEffects = request->hasArg(F("RX"));
    receiveSegmentOptions = request->hasArg(F("SO"));
    receiveSegmentBounds = request->hasArg(F("SG"));
    receiveNotifications = (receiveNotificationBrightness || receiveNotificationColor || receiveNotificationEffects || receiveSegmentOptions);
    notifyDirectDefault = request->hasArg(F("SD"));
    notifyDirect = notifyDirectDefault;
    notifyButton = request->hasArg(F("SB"));
    notifyAlexa = request->hasArg(F("SA"));
    notifyHue = request->hasArg(F("SH"));
    notifyMacro = request->hasArg(F("SM"));

    t = request->arg(F("UR")).toInt();
    if ((t>=0) && (t<30)) udpNumRetries = t;


    nodeListEnabled = request->hasArg(F("NL"));
    if (!nodeListEnabled) Nodes.clear();
    nodeBroadcastEnabled = request->hasArg(F("NB"));

    receiveDirect = request->hasArg(F("RD"));
    useMainSegmentOnly = request->hasArg(F("MO"));
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

    alexaEnabled = request->hasArg(F("AL"));
    strlcpy(alexaInvocationName, request->arg(F("AI")).c_str(), 33);
    t = request->arg(F("AP")).toInt();
    if (t >= 0 && t <= 9) alexaNumPresets = t;

    #ifdef WLED_ENABLE_MQTT
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
    for (uint8_t i=0; i<WLED_MAX_BUTTONS; i++) {
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
      uint8_t pinLen = strlen(pin);
      if (pinLen == 4 || pinLen == 0) {
        uint8_t numZeros = 0;
        for (uint8_t i = 0; i < pinLen; i++) numZeros += (pin[i] == '0');
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
      aOtaEnabled = request->hasArg(F("AO"));
      //createEditHandler(correctPIN && !otaLock);
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
    if (!requestJSONBufferLock(5)) return;

    // global I2C & SPI pins
    int8_t hw_sda_pin  = !request->arg(F("SDA")).length() ? -1 : (int)request->arg(F("SDA")).toInt();
    int8_t hw_scl_pin  = !request->arg(F("SCL")).length() ? -1 : (int)request->arg(F("SCL")).toInt();
    if (i2c_sda != hw_sda_pin || i2c_scl != hw_scl_pin) {
      // only if pins changed
      uint8_t old_i2c[2] = { static_cast<uint8_t>(i2c_scl), static_cast<uint8_t>(i2c_sda) };
      pinManager.deallocateMultiplePins(old_i2c, 2, PinOwner::HW_I2C); // just in case deallocation of old pins

      PinManagerPinType i2c[2] = { { hw_sda_pin, true }, { hw_scl_pin, true } };
      if (hw_sda_pin >= 0 && hw_scl_pin >= 0 && pinManager.allocateMultiplePins(i2c, 2, PinOwner::HW_I2C)) {
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
      pinManager.deallocateMultiplePins(old_spi, 3, PinOwner::HW_SPI); // just in case deallocation of old pins
      PinManagerPinType spi[3] = { { hw_mosi_pin, true }, { hw_miso_pin, true }, { hw_sclk_pin, true } };
      if (hw_mosi_pin >= 0 && hw_sclk_pin >= 0 && pinManager.allocateMultiplePins(spi, 3, PinOwner::HW_SPI)) {
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

    JsonObject um = doc.createNestedObject("um");

    size_t args = request->args();
    uint16_t j=0;
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
      DEBUG_PRINTF("(%d):",umSubObj);
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
        DEBUG_PRINT("[");
        DEBUG_PRINT(j);
        DEBUG_PRINT("] = ");
        DEBUG_PRINTLN(value);
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
        DEBUG_PRINT(" = ");
        DEBUG_PRINTLN(value);
      }
    }
    usermods.readFromConfig(um);  // force change of usermod parameters
    DEBUG_PRINTLN(F("Done re-init usermods."));
    releaseJSONBufferLock();
  }

  #ifndef WLED_DISABLE_2D
  //2D panels
  if (subPage == SUBPAGE_2D)
  {
    strip.isMatrix = request->arg(F("SOMP")).toInt();
    strip.panel.clear(); // release memory if allocated
    if (strip.isMatrix) {
      strip.panels  = MAX(1,MIN(WLED_MAX_PANELS,request->arg(F("MPC")).toInt()));
      strip.panel.reserve(strip.panels); // pre-allocate memory
      for (uint8_t i=0; i<strip.panels; i++) {
        WS2812FX::Panel p;
        char pO[8] = { '\0' };
        snprintf_P(pO, 7, PSTR("P%d"), i);       // MAX_PANELS is 64 so pO will always only be 4 characters or less
        pO[7] = '\0';
        uint8_t l = strlen(pO);
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
      strip.setUpMatrix(); // will check limits
      strip.makeAutoSegments(true);
      strip.deserializeMap();
    } else {
      Segment::maxWidth  = strip.getLengthTotal();
      Segment::maxHeight = 1;
    }
  }
  #endif

  lastEditTime = millis();
  // do not save if factory reset or LED settings (which are saved after LED re-init)
  doSerializeConfig = subPage != SUBPAGE_LEDS && !(subPage == SUBPAGE_SEC && doReboot);
  if (subPage == SUBPAGE_UM) doReboot = request->hasArg(F("RBT")); // prevent race condition on dual core system (set reboot here, after doSerializeConfig has been set)
  #ifndef WLED_DISABLE_ALEXA
  if (subPage == SUBPAGE_SYNC) alexaInit();
  #endif
}


//HTTP API request parser
bool handleSet(AsyncWebServerRequest *request, const String& req, bool apply)
{
  if (!(req.indexOf("win") >= 0)) return false;

  int pos = 0;
  DEBUG_PRINT(F("API req: "));
  DEBUG_PRINTLN(req);

  //segment select (sets main segment)
  pos = req.indexOf(F("SM="));
  if (pos > 0 && !realtimeMode) {
    strip.setMainSegmentId(getNumVal(&req, pos));
  }

  byte selectedSeg = strip.getFirstSelectedSegId();

  bool singleSegment = false;

  pos = req.indexOf(F("SS="));
  if (pos > 0) {
    byte t = getNumVal(&req, pos);
    if (t < strip.getSegmentsNum()) {
      selectedSeg = t;
      singleSegment = true;
    }
  }

  Segment& selseg = strip.getSegment(selectedSeg);
  pos = req.indexOf(F("SV=")); //segment selected
  if (pos > 0) {
    byte t = getNumVal(&req, pos);
    if (t == 2) for (uint8_t i = 0; i < strip.getSegmentsNum(); i++) strip.getSegment(i).selected = false; // unselect other segments
    selseg.selected = t;
  }

  // temporary values, write directly to segments, globals are updated by setValuesFromFirstSelectedSeg()
  uint32_t col0 = selseg.colors[0];
  uint32_t col1 = selseg.colors[1];
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
    startI = getNumVal(&req, pos);
  }
  pos = req.indexOf(F("S2=")); //segment stop
  if (pos > 0) {
    stopI = getNumVal(&req, pos);
  }
  pos = req.indexOf(F("GP=")); //segment grouping
  if (pos > 0) {
    grpI = getNumVal(&req, pos);
    if (grpI == 0) grpI = 1;
  }
  pos = req.indexOf(F("SP=")); //segment spacing
  if (pos > 0) {
    spcI = getNumVal(&req, pos);
  }
  strip.setSegment(selectedSeg, startI, stopI, grpI, spcI, UINT16_MAX, startY, stopY);

  pos = req.indexOf(F("RV=")); //Segment reverse
  if (pos > 0) selseg.reverse = req.charAt(pos+3) != '0';

  pos = req.indexOf(F("MI=")); //Segment mirror
  if (pos > 0) selseg.mirror = req.charAt(pos+3) != '0';

  pos = req.indexOf(F("SB=")); //Segment brightness/opacity
  if (pos > 0) {
    byte segbri = getNumVal(&req, pos);
    selseg.setOption(SEG_OPTION_ON, segbri); // use transition
    if (segbri) {
      selseg.setOpacity(segbri);
    }
  }

  pos = req.indexOf(F("SW=")); //segment power
  if (pos > 0) {
    switch (getNumVal(&req, pos)) {
      case 0:  selseg.setOption(SEG_OPTION_ON, false);      break; // use transition
      case 1:  selseg.setOption(SEG_OPTION_ON, true);       break; // use transition
      default: selseg.setOption(SEG_OPTION_ON, !selseg.on); break; // use transition
    }
  }

  pos = req.indexOf(F("PS=")); //saves current in preset
  if (pos > 0) savePreset(getNumVal(&req, pos));

  pos = req.indexOf(F("P1=")); //sets first preset for cycle
  if (pos > 0) presetCycMin = getNumVal(&req, pos);

  pos = req.indexOf(F("P2=")); //sets last preset for cycle
  if (pos > 0) presetCycMax = getNumVal(&req, pos);

  //apply preset
  if (updateVal(req.c_str(), "PL=", &presetCycCurr, presetCycMin, presetCycMax)) {
    unloadPlaylist();
    applyPreset(presetCycCurr);
  }

  //set brightness
  updateVal(req.c_str(), "&A=", &bri);

  bool col0Changed = false, col1Changed = false;
  //set colors
  col0Changed |= updateVal(req.c_str(), "&R=", &colIn[0]);
  col0Changed |= updateVal(req.c_str(), "&G=", &colIn[1]);
  col0Changed |= updateVal(req.c_str(), "&B=", &colIn[2]);
  col0Changed |= updateVal(req.c_str(), "&W=", &colIn[3]);

  col1Changed |= updateVal(req.c_str(), "R2=", &colInSec[0]);
  col1Changed |= updateVal(req.c_str(), "G2=", &colInSec[1]);
  col1Changed |= updateVal(req.c_str(), "B2=", &colInSec[2]);
  col1Changed |= updateVal(req.c_str(), "W2=", &colInSec[3]);

  #ifdef WLED_ENABLE_LOXONE
  //lox parser
  pos = req.indexOf(F("LX=")); // Lox primary color
  if (pos > 0) {
    int lxValue = getNumVal(&req, pos);
    if (parseLx(lxValue, colIn)) {
      bri = 255;
      nightlightActive = false; //always disable nightlight when toggling
      col0Changed = true;
    }
  }
  pos = req.indexOf(F("LY=")); // Lox secondary color
  if (pos > 0) {
    int lxValue = getNumVal(&req, pos);
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
    uint16_t temphue = getNumVal(&req, pos);
    byte tempsat = 255;
    pos = req.indexOf(F("SA="));
    if (pos > 0) {
      tempsat = getNumVal(&req, pos);
    }
    byte sec = req.indexOf(F("H2"));
    colorHStoRGB(temphue, tempsat, (sec>0) ? colInSec : colIn);
    col0Changed |= (!sec); col1Changed |= sec;
  }

  //set white spectrum (kelvin)
  pos = req.indexOf(F("&K="));
  if (pos > 0) {
    byte sec = req.indexOf(F("K2"));
    colorKtoRGB(getNumVal(&req, pos), (sec>0) ? colInSec : colIn);
    col0Changed |= (!sec); col1Changed |= sec;
  }

  //set color from HEX or 32bit DEC
  byte tmpCol[4];
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
    colorFromDecOrHexString(tmpCol, (char*)req.substring(pos + 3).c_str());
    uint32_t col2 = RGBW32(tmpCol[0], tmpCol[1], tmpCol[2], tmpCol[3]);
    selseg.setColor(2, col2); // defined above (SS= or main)
    if (!singleSegment) strip.setColor(2, col2); // will set color to all active & selected segments
  }

  //set to random hue SR=0->1st SR=1->2nd
  pos = req.indexOf(F("SR"));
  if (pos > 0) {
    byte sec = getNumVal(&req, pos);
    setRandomColor(sec? colInSec : colIn);
    col0Changed |= (!sec); col1Changed |= sec;
  }

  //swap 2nd & 1st
  pos = req.indexOf(F("SC"));
  if (pos > 0) {
    byte temp;
    for (uint8_t i=0; i<4; i++) {
      temp        = colIn[i];
      colIn[i]    = colInSec[i];
      colInSec[i] = temp;
    }
    col0Changed = col1Changed = true;
  }

  // apply colors to selected segment, and all selected segments if applicable
  if (col0Changed) {
    uint32_t colIn0 = RGBW32(colIn[0], colIn[1], colIn[2], colIn[3]);
    selseg.setColor(0, colIn0);
    if (!singleSegment) strip.setColor(0, colIn0); // will set color to all active & selected segments
  }

  if (col1Changed) {
    uint32_t colIn1 = RGBW32(colInSec[0], colInSec[1], colInSec[2], colInSec[3]);
    selseg.setColor(1, colIn1);
    if (!singleSegment) strip.setColor(1, colIn1); // will set color to all active & selected segments
  }

  bool fxModeChanged = false, speedChanged = false, intensityChanged = false, paletteChanged = false;
  bool custom1Changed = false, custom2Changed = false, custom3Changed = false, check1Changed = false, check2Changed = false, check3Changed = false;
  // set effect parameters
  if (updateVal(req.c_str(), "FX=", &effectIn, 0, strip.getModeCount()-1)) {
    if (request != nullptr) unloadPlaylist(); // unload playlist if changing FX using web request
    fxModeChanged = true;
  }
  speedChanged     = updateVal(req.c_str(), "SX=", &speedIn);
  intensityChanged = updateVal(req.c_str(), "IX=", &intensityIn);
  paletteChanged   = updateVal(req.c_str(), "FP=", &paletteIn, 0, strip.getPaletteCount()-1);
  custom1Changed   = updateVal(req.c_str(), "X1=", &custom1In);
  custom2Changed   = updateVal(req.c_str(), "X2=", &custom2In);
  custom3Changed   = updateVal(req.c_str(), "X3=", &custom3In);
  check1Changed    = updateVal(req.c_str(), "M1=", &check1In);
  check2Changed    = updateVal(req.c_str(), "M2=", &check2In);
  check3Changed    = updateVal(req.c_str(), "M3=", &check3In);

  stateChanged |= (fxModeChanged || speedChanged || intensityChanged || paletteChanged || custom1Changed || custom2Changed || custom3Changed || check1Changed || check2Changed || check3Changed);

  // apply to main and all selected segments to prevent #1618.
  for (uint8_t i = 0; i < strip.getSegmentsNum(); i++) {
    Segment& seg = strip.getSegment(i);
    if (i != selectedSeg && (singleSegment || !seg.isActive() || !seg.isSelected())) continue; // skip non main segments if not applying to all
    if (fxModeChanged)    seg.setMode(effectIn, req.indexOf(F("FXD="))>0);  // apply defaults if FXD= is specified
    if (speedChanged)     seg.speed     = speedIn;
    if (intensityChanged) seg.intensity = intensityIn;
    if (paletteChanged)   seg.setPalette(paletteIn);
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
    overlayCurrent = getNumVal(&req, pos);
  }

  //apply macro (deprecated, added for compatibility with pre-0.11 automations)
  pos = req.indexOf(F("&M="));
  if (pos > 0) {
    applyPreset(getNumVal(&req, pos) + 16);
  }

  //toggle send UDP direct notifications
  pos = req.indexOf(F("SN="));
  if (pos > 0) notifyDirect = (req.charAt(pos+3) != '0');

  //toggle receive UDP direct notifications
  pos = req.indexOf(F("RN="));
  if (pos > 0) receiveNotifications = (req.charAt(pos+3) != '0');

  //receive live data via UDP/Hyperion
  pos = req.indexOf(F("RD="));
  if (pos > 0) receiveDirect = (req.charAt(pos+3) != '0');

  //main toggle on/off (parse before nightlight, #1214)
  pos = req.indexOf(F("&T="));
  if (pos > 0) {
    nightlightActive = false; //always disable nightlight when toggling
    switch (getNumVal(&req, pos))
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
      if (!aNlDef) nightlightDelayMins = getNumVal(&req, pos);
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
    nightlightTargetBri = getNumVal(&req, pos);
    nightlightActiveOld = false; //re-init
  }

  //toggle nightlight fade
  pos = req.indexOf(F("NF="));
  if (pos > 0)
  {
    nightlightMode = getNumVal(&req, pos);

    nightlightActiveOld = false; //re-init
  }
  if (nightlightMode > NL_MODE_SUN) nightlightMode = NL_MODE_SUN;

  pos = req.indexOf(F("TT="));
  if (pos > 0) transitionDelay = getNumVal(&req, pos);
  if (fadeTransition) strip.setTransition(transitionDelay);

  //set time (unix timestamp)
  pos = req.indexOf(F("ST="));
  if (pos > 0) {
    setTimeFromAPI(getNumVal(&req, pos));
  }

  //set countdown goal (unix timestamp)
  pos = req.indexOf(F("CT="));
  if (pos > 0) {
    countdownTime = getNumVal(&req, pos);
    if (countdownTime - toki.second() > 0) countdownOverTriggered = false;
  }

  pos = req.indexOf(F("LO="));
  if (pos > 0) {
    realtimeOverride = getNumVal(&req, pos);
    if (realtimeOverride > 2) realtimeOverride = REALTIME_OVERRIDE_ALWAYS;
    if (realtimeMode && useMainSegmentOnly) {
      strip.getMainSegment().freeze = !realtimeOverride;
    }
  }

  pos = req.indexOf(F("RB"));
  if (pos > 0) doReboot = true;

  // clock mode, 0: normal, 1: countdown
  pos = req.indexOf(F("NM="));
  if (pos > 0) countdownMode = (req.charAt(pos+3) != '0');

  pos = req.indexOf(F("U0=")); //user var 0
  if (pos > 0) {
    userVar0 = getNumVal(&req, pos);
  }

  pos = req.indexOf(F("U1=")); //user var 1
  if (pos > 0) {
    userVar1 = getNumVal(&req, pos);
  }
  // you can add more if you need

  // global col[], effectCurrent, ... are updated in stateChanged()
  if (!apply) return true; // when called by JSON API, do not call colorUpdated() here

  pos = req.indexOf(F("&NN")); //do not send UDP notifications this time
  stateUpdated((pos > 0) ? CALL_MODE_NO_NOTIFY : CALL_MODE_DIRECT_CHANGE);

  // internal call, does not send XML response
  pos = req.indexOf(F("IN"));
  if (pos < 1) XML_response(request);

  return true;
}
