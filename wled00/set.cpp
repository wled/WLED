#include "wled.h"

/*
 * Receives client input
 */
bool isAsterisksOnly(const char* str, byte maxLen)
{
  for (byte i = 0; i < maxLen; i++) {
    if (str[i] == 0) break;
    if (str[i] != '*') return false;
  }
  //at this point the password contains asterisks only
  return (str[0] != 0); //false on empty string
}


//called upon POST settings form submit
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage)
{

  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec 7: DMX 8: usermods
  if (subPage <1 || subPage >8) return;

  //WIFI SETTINGS
  if (subPage == 1)
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

    noWifiSleep = request->hasArg(F("WS"));

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
  if (subPage == 2)
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

    uint8_t colorOrder, type, skip;
    uint16_t length, start;
    uint8_t pins[5] = {255, 255, 255, 255, 255};

    autoSegments = request->hasArg(F("MS"));
    correctWB = request->hasArg(F("CCT"));
    cctFromRgb = request->hasArg(F("CR"));
    strip.cctBlending = request->arg(F("CB")).toInt();
    Bus::setCCTBlend(strip.cctBlending);
    strip.autoWhiteMode = (request->arg(F("AW")).toInt());
    Bus::setAutoWhiteMode(strip.autoWhiteMode);
    strip.setTargetFps(request->arg(F("FR")).toInt());

    bool busesChanged = false;
    for (uint8_t s = 0; s < WLED_MAX_BUSSES; s++) {
      char lp[4] = "L0"; lp[2] = 48+s; lp[3] = 0; //ascii 0-9 //strip data pin
      char lc[4] = "LC"; lc[2] = 48+s; lc[3] = 0; //strip length
      char co[4] = "CO"; co[2] = 48+s; co[3] = 0; //strip color order
      char lt[4] = "LT"; lt[2] = 48+s; lt[3] = 0; //strip type
      char ls[4] = "LS"; ls[2] = 48+s; ls[3] = 0; //strip start LED
      char cv[4] = "CV"; cv[2] = 48+s; cv[3] = 0; //strip reverse
      char sl[4] = "SL"; sl[2] = 48+s; sl[3] = 0; //skip first N LEDs
      char rf[4] = "RF"; rf[2] = 48+s; rf[3] = 0; //refresh required
      if (!request->hasArg(lp)) {
        DEBUG_PRINTLN(F("No data.")); break;
      }
      for (uint8_t i = 0; i < 5; i++) {
        lp[1] = 48+i;
        if (!request->hasArg(lp)) break;
        pins[i] = (request->arg(lp).length() > 0) ? request->arg(lp).toInt() : 255;
      }
      type = request->arg(lt).toInt();
      type |= request->hasArg(rf) << 7; // off refresh override
      skip = request->arg(sl).toInt();
      colorOrder = request->arg(co).toInt();
      start = (request->hasArg(ls)) ? request->arg(ls).toInt() : t;
      if (request->hasArg(lc) && request->arg(lc).toInt() > 0) {
        t += length = request->arg(lc).toInt();
      } else {
        break;  // no parameter
      }

      // actual finalization is done in WLED::loop() (removing old busses and adding new)
      if (busConfigs[s] != nullptr) delete busConfigs[s];
      busConfigs[s] = new BusConfig(type, pins, start, length, colorOrder, request->hasArg(cv), skip);
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

    // upate other pins
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

    for (uint8_t i=0; i<WLED_MAX_BUTTONS; i++) {
      char bt[4] = "BT"; bt[2] = (i<10?48:55)+i; bt[3] = 0; // button pin (use A,B,C,... if WLED_MAX_BUTTONS>10)
      char be[4] = "BE"; be[2] = (i<10?48:55)+i; be[3] = 0; // button type (use A,B,C,... if WLED_MAX_BUTTONS>10)
      int hw_btn_pin = request->arg(bt).toInt();
      if (pinManager.allocatePin(hw_btn_pin,false,PinOwner::Button)) {
        btnPin[i] = hw_btn_pin;
        pinMode(btnPin[i], INPUT_PULLUP);
        buttonType[i] = request->arg(be).toInt();
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
    strip.gammaCorrectBri = request->hasArg(F("GB"));
    strip.gammaCorrectCol = request->hasArg(F("GC"));

    fadeTransition = request->hasArg(F("TF"));
    t = request->arg(F("TD")).toInt();
    if (t >= 0) transitionDelay = t;
    transitionDelayDefault = t;
    strip.paletteFade = request->hasArg(F("PF"));

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
  if (subPage == 3)
  {
    strlcpy(serverDescription, request->arg(F("DS")).c_str(), 33);
    syncToggleReceive = request->hasArg(F("ST"));
  }

  //SYNC
  if (subPage == 4)
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
    notifyTwice = request->hasArg(F("S2"));

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
    t = request->arg(F("DM")).toInt();
    if (t >= DMX_MODE_DISABLED && t <= DMX_MODE_MULTIPLE_RGBW) DMXMode = t;
    t = request->arg(F("ET")).toInt();
    if (t > 99  && t <= 65000) realtimeTimeoutMs = t;
    arlsForceMaxBri = request->hasArg(F("FB"));
    arlsDisableGammaCorrection = request->hasArg(F("RG"));
    t = request->arg(F("WO")).toInt();
    if (t >= -255  && t <= 255) arlsOffset = t;

    alexaEnabled = request->hasArg(F("AL"));
    strlcpy(alexaInvocationName, request->arg(F("AI")).c_str(), 33);

    #ifndef WLED_DISABLE_BLYNK
    strlcpy(blynkHost, request->arg("BH").c_str(), 33);
    t = request->arg(F("BP")).toInt();
    if (t > 0) blynkPort = t;

    if (request->hasArg("BK") && !request->arg("BK").equals(F("Hidden"))) {
      strlcpy(blynkApiKey, request->arg("BK").c_str(), 36); initBlynk(blynkApiKey, blynkHost, blynkPort);
    }
    #endif

    #ifdef WLED_ENABLE_MQTT
    mqttEnabled = request->hasArg(F("MQ"));
    strlcpy(mqttServer, request->arg(F("MS")).c_str(), 33);
    t = request->arg(F("MQPORT")).toInt();
    if (t > 0) mqttPort = t;
    strlcpy(mqttUser, request->arg(F("MQUSER")).c_str(), 41);
    if (!isAsterisksOnly(request->arg(F("MQPASS")).c_str(), 41)) strlcpy(mqttPass, request->arg(F("MQPASS")).c_str(), 65);
    strlcpy(mqttClientID, request->arg(F("MQCID")).c_str(), 41);
    strlcpy(mqttDeviceTopic, request->arg(F("MD")).c_str(), 33);
    strlcpy(mqttGroupTopic, request->arg(F("MG")).c_str(), 33);
    buttonPublishMqtt = request->hasArg(F("BM"));
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
  if (subPage == 5)
  {
    ntpEnabled = request->hasArg(F("NT"));
    strlcpy(ntpServerName, request->arg(F("NS")).c_str(), 33);
    useAMPM = !request->hasArg(F("CF"));
    currentTimezone = request->arg(F("TZ")).toInt();
    utcOffsetSecs = request->arg(F("UO")).toInt();

    //start ntp if not already connected
    if (ntpEnabled && WLED_CONNECTED && !ntpConnected) ntpConnected = ntpUdp.begin(ntpLocalPort);
    ntpLastSyncTime = 0; // force new NTP query

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
  if (subPage == 6)
  {
    if (request->hasArg(F("RS"))) //complete factory reset
    {
      WLED_FS.format();
      clearEEPROM();
      serveMessage(request, 200, F("All Settings erased."), F("Connect to WLED-AP to setup again"),255);
      doReboot = true;
    }

    bool pwdCorrect = !otaLock; //always allow access if ota not locked
    if (request->hasArg(F("OP")))
    {
      if (otaLock && strcmp(otaPass,request->arg(F("OP")).c_str()) == 0)
      {
        pwdCorrect = true;
      }
      if (!otaLock && request->arg(F("OP")).length() > 0)
      {
        strlcpy(otaPass,request->arg(F("OP")).c_str(), 33);
      }
    }

    if (pwdCorrect) //allow changes if correct pwd or no ota active
    {
      otaLock = request->hasArg(F("NO"));
      wifiLock = request->hasArg(F("OW"));
      aOtaEnabled = request->hasArg(F("AO"));
    }
  }
  #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
  if (subPage == 7)
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
  if (subPage == 8)
  {
    #ifdef WLED_USE_DYNAMIC_JSON
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    #else
    if (!requestJSONBufferLock(5)) return;
    #endif

    JsonObject um = doc.createNestedObject("um");

    size_t args = request->args();
    uint16_t j=0;
    for (size_t i=0; i<args; i++) {
      String name = request->argName(i);
      String value = request->arg(i);

      // POST request parameters are combined as <usermodname>_<usermodparameter>
      int umNameEnd = name.indexOf(":");
      if (umNameEnd<1) break;  // parameter does not contain ":" or on 1st place -> wrong

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
        // to describe the type of parameter (text,float,int), for boolean patameters the first field contains "off"
        // so checkboxes have one or two fields (first is always "false", existence of second depends on checkmark and may be "true")
        if (subObj[name].isNull()) {
          // the first occurence of the field describes the parameter type (used in next loop)
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

    releaseJSONBufferLock();
  }
  
  if (subPage != 2 && (subPage != 6 || !doReboot)) serializeConfig(); //do not save if factory reset or LED settings (which are saved after LED re-init)
  if (subPage == 4) alexaInit();
}



//helper to get int value at a position in string
int getNumVal(const String* req, uint16_t pos)
{
  return req->substring(pos+3).toInt();
}


//helper to get int value with in/decrementing support via ~ syntax
void parseNumber(const char* str, byte* val, byte minv, byte maxv)
{
  if (str == nullptr || str[0] == '\0') return;
  if (str[0] == 'r') {*val = random8(minv,maxv); return;}
  bool wrap = false;
  if (str[0] == 'w' && strlen(str) > 1) {str++; wrap = true;}
  if (str[0] == '~') {
    int out = atoi(str +1);
    if (out == 0)
    {
      if (str[1] == '0') return;
      if (str[1] == '-')
      {
        *val = (int)(*val -1) < (int)minv ? maxv : min((int)maxv,(*val -1)); //-1, wrap around
      } else {
        *val = (int)(*val +1) > (int)maxv ? minv : max((int)minv,(*val +1)); //+1, wrap around
      }
    } else {
      if (wrap && *val == maxv && out > 0) out = minv;
      else if (wrap && *val == minv && out < 0) out = maxv;
      else { 
        out += *val;
        if (out > maxv) out = maxv;
        if (out < minv) out = minv;
      }
      *val = out;
    }
  } else
  {
    byte p1 = atoi(str);
    const char* str2 = strchr(str,'~'); //min/max range (for preset cycle, e.g. "1~5~")
    if (str2) {
      byte p2 = atoi(str2+1);
      presetCycMin = p1; presetCycMax = p2;
      while (isdigit((str2+1)[0])) str2++;
      parseNumber(str2+1, val, p1, p2);
    } else {
      *val = p1;
    }
  }
}


bool updateVal(const String* req, const char* key, byte* val, byte minv, byte maxv)
{
  int pos = req->indexOf(key);
  if (pos < 1) return false;
  if (req->length() < (unsigned int)(pos + 4)) return false;
  parseNumber(req->c_str() + pos +3, val, minv, maxv);
  return true;
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
  if (pos > 0) {
    strip.setMainSegmentId(getNumVal(&req, pos));
  }

  byte selectedSeg = strip.getFirstSelectedSegId();

  bool singleSegment = false;

  pos = req.indexOf(F("SS="));
  if (pos > 0) {
    byte t = getNumVal(&req, pos);
    if (t < strip.getMaxSegments()) {
      selectedSeg = t;
      singleSegment = true;
    }
  }

  WS2812FX::Segment& selseg = strip.getSegment(selectedSeg);
  pos = req.indexOf(F("SV=")); //segment selected
  if (pos > 0) {
    byte t = getNumVal(&req, pos);
    if (t == 2) for (uint8_t i = 0; i < strip.getMaxSegments(); i++) strip.getSegment(i).setOption(SEG_OPTION_SELECTED, 0); // unselect other segments
    selseg.setOption(SEG_OPTION_SELECTED, t);
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

  uint16_t startI = selseg.start;
  uint16_t stopI  = selseg.stop;
  uint8_t  grpI   = selseg.grouping;
  uint16_t spcI   = selseg.spacing;
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
  strip.setSegment(selectedSeg, startI, stopI, grpI, spcI);

  pos = req.indexOf(F("RV=")); //Segment reverse
  if (pos > 0) selseg.setOption(SEG_OPTION_REVERSED, req.charAt(pos+3) != '0');

  pos = req.indexOf(F("MI=")); //Segment mirror
  if (pos > 0) selseg.setOption(SEG_OPTION_MIRROR, req.charAt(pos+3) != '0');

  pos = req.indexOf(F("SB=")); //Segment brightness/opacity
  if (pos > 0) {
    byte segbri = getNumVal(&req, pos);
    selseg.setOption(SEG_OPTION_ON, segbri, selectedSeg);
    if (segbri) {
      selseg.setOpacity(segbri, selectedSeg);
    }
  }

  pos = req.indexOf(F("SW=")); //segment power
  if (pos > 0) {
    switch (getNumVal(&req, pos)) {
      case 0: selseg.setOption(SEG_OPTION_ON, false); break;
      case 1: selseg.setOption(SEG_OPTION_ON, true); break;
      default: selseg.setOption(SEG_OPTION_ON, !selseg.getOption(SEG_OPTION_ON)); break;
    }
  }

  pos = req.indexOf(F("PS=")); //saves current in preset
  if (pos > 0) savePreset(getNumVal(&req, pos));

  pos = req.indexOf(F("P1=")); //sets first preset for cycle
  if (pos > 0) presetCycMin = getNumVal(&req, pos);

  pos = req.indexOf(F("P2=")); //sets last preset for cycle
  if (pos > 0) presetCycMax = getNumVal(&req, pos);

  //apply preset
  if (updateVal(&req, "PL=", &presetCycCurr, presetCycMin, presetCycMax)) {
		unloadPlaylist();
    applyPreset(presetCycCurr);
  }

  //set brightness
  updateVal(&req, "&A=", &bri);

  bool col0Changed = false, col1Changed = false;
  //set colors
  col0Changed |= updateVal(&req, "&R=", &colIn[0]);
  col0Changed |= updateVal(&req, "&G=", &colIn[1]);
  col0Changed |= updateVal(&req, "&B=", &colIn[2]);
  col0Changed |= updateVal(&req, "&W=", &colIn[3]);

  col1Changed |= updateVal(&req, "R2=", &colInSec[0]);
  col1Changed |= updateVal(&req, "G2=", &colInSec[1]);
  col1Changed |= updateVal(&req, "B2=", &colInSec[2]);
  col1Changed |= updateVal(&req, "W2=", &colInSec[3]);

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
    selseg.setColor(2, col2, selectedSeg); // defined above (SS= or main)
    stateChanged = true;
    if (!singleSegment) strip.setColor(2, col2);
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
    stateChanged = true;
    uint32_t colIn0 = RGBW32(colIn[0], colIn[1], colIn[2], colIn[3]);
    selseg.setColor(0, colIn0, selectedSeg);
    if (!singleSegment) strip.setColor(0, colIn0);
  }

  if (col1Changed) {
    stateChanged = true;
    uint32_t colIn1 = RGBW32(colInSec[0], colInSec[1], colInSec[2], colInSec[3]);
    selseg.setColor(1, colIn1, selectedSeg);
    if (!singleSegment) strip.setColor(1, colIn1);
  }

  bool fxModeChanged = false, speedChanged = false, intensityChanged = false, paletteChanged = false;
  // set effect parameters
  if (updateVal(&req, "FX=", &effectIn, 0, strip.getModeCount()-1)) {
    if (request != nullptr) unloadPlaylist(); // unload playlist if changing FX using web request
    fxModeChanged = true;
  }
  speedChanged     = updateVal(&req, "SX=", &speedIn);
  intensityChanged = updateVal(&req, "IX=", &intensityIn);
  paletteChanged   = updateVal(&req, "FP=", &paletteIn, 0, strip.getPaletteCount()-1);
  
  stateChanged |= (fxModeChanged || speedChanged || intensityChanged || paletteChanged);

  // apply to main and all selected segments to prevent #1618.
  for (uint8_t i = 0; i < strip.getMaxSegments(); i++) {
    WS2812FX::Segment& seg = strip.getSegment(i);
    if (i != selectedSeg && (singleSegment || !seg.isActive() || !seg.isSelected())) continue; // skip non main segments if not applying to all
    if (fxModeChanged)    strip.setMode(i, effectIn);
    if (speedChanged)     seg.speed     = speedIn;
    if (intensityChanged) seg.intensity = intensityIn;
    if (paletteChanged)   seg.palette   = paletteIn;
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
      nightlightStartTime = millis();
    }
  } else if (aNlDef)
  {
    nightlightActive = true;
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
      strip.getMainSegment().setOption(SEG_OPTION_FREEZE, !realtimeOverride, strip.getMainSegmentId());
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
