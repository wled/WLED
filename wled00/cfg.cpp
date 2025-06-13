#include "wled.h"
#include "wled_ethernet.h"

/*
 * Serializes and parses the cfg.json and wsec.json settings files, stored in internal FS.
 * The structure of the JSON is not to be considered an official API and may change without notice.
 */

#ifndef PIXEL_COUNTS
  #define PIXEL_COUNTS DEFAULT_LED_COUNT
#endif

#ifndef DATA_PINS
  #define DATA_PINS DEFAULT_LED_PIN
#endif

#ifndef LED_TYPES
  #define LED_TYPES DEFAULT_LED_TYPE
#endif

#ifndef DEFAULT_LED_COLOR_ORDER
  #define DEFAULT_LED_COLOR_ORDER COL_ORDER_GRB  //default to GRB
#endif

static constexpr unsigned sumPinsRequired(const unsigned* current, size_t count) {
  return (count > 0) ? (Bus::getNumberOfPins(*current) + sumPinsRequired(current+1,count-1)) : 0;
}

static constexpr bool validatePinsAndTypes(const unsigned* types, unsigned numTypes, unsigned numPins ) {
  // Pins provided < pins required -> always invalid
  // Pins provided = pins required -> always valid
  // Pins provided > pins required -> valid if excess pins are a product of last type pins since it will be repeated
  return (sumPinsRequired(types, numTypes) > numPins) ? false :
          (numPins - sumPinsRequired(types, numTypes)) % Bus::getNumberOfPins(types[numTypes-1]) == 0;
}


//simple macro for ArduinoJSON's or syntax
#define CJSON(a,b) a = b | a

void getStringFromJson(char* dest, const char* src, size_t len) {
  if (src != nullptr) strlcpy(dest, src, len);
}

bool deserializeConfig(JsonObject doc, bool fromFS) {
  bool needsSave = false;
  //int rev_major = doc["rev"][0]; // 1
  //int rev_minor = doc["rev"][1]; // 0

  //long vid = doc[F("vid")]; // 2010020

#ifdef WLED_USE_ETHERNET
  JsonObject ethernet = doc[F("eth")];
  CJSON(ethernetType, ethernet["type"]);
  // NOTE: Ethernet configuration takes priority over other use of pins
  initEthernet();
#endif

  JsonObject id = doc["id"];
  getStringFromJson(cmDNS, id[F("mdns")], 33);
  getStringFromJson(serverDescription, id[F("name")], 33);
#ifndef WLED_DISABLE_ALEXA
  getStringFromJson(alexaInvocationName, id[F("inv")], 33);
#endif
  CJSON(simplifiedUI, id[F("sui")]);

  JsonObject nw = doc["nw"];
#ifndef WLED_DISABLE_ESPNOW
  CJSON(enableESPNow, nw[F("espnow")]);
  linked_remotes.clear();
  JsonVariant lrem = nw[F("linked_remote")];
  if (!lrem.isNull()) {
     if (lrem.is<JsonArray>()) {
      for (size_t i = 0; i < lrem.size(); i++) {
        std::array<char, 13> entry{};
        getStringFromJson(entry.data(), lrem[i], 13);
        entry[12] = '\0';
        linked_remotes.emplace_back(entry);
      }
    }
    else { // legacy support for single MAC address in config
      std::array<char, 13> entry{};
      getStringFromJson(entry.data(), lrem, 13);
      entry[12] = '\0';
      linked_remotes.emplace_back(entry);
    }
  }
#endif

  size_t n = 0;
  JsonArray nw_ins = nw["ins"];
  if (!nw_ins.isNull()) {
    // as password are stored separately in wsec.json when reading configuration vector resize happens there, but for dynamic config we need to resize if necessary
    if (nw_ins.size() > 1 && nw_ins.size() > multiWiFi.size()) multiWiFi.resize(nw_ins.size()); // resize constructs objects while resizing
    for (JsonObject wifi : nw_ins) {
      JsonArray ip = wifi["ip"];
      JsonArray gw = wifi["gw"];
      JsonArray sn = wifi["sn"];
      char ssid[33] = "";
      char pass[65] = "";
      char bssid[13] = "";
      IPAddress nIP = (uint32_t)0U, nGW = (uint32_t)0U, nSN = (uint32_t)0x00FFFFFF; // little endian
      getStringFromJson(ssid, wifi[F("ssid")], 33);
      getStringFromJson(pass, wifi["psk"], 65); // password is not normally present but if it is, use it
      getStringFromJson(bssid, wifi[F("bssid")], 13);
      for (size_t i = 0; i < 4; i++) {
        CJSON(nIP[i], ip[i]);
        CJSON(nGW[i], gw[i]);
        CJSON(nSN[i], sn[i]);
      }
      if (strlen(ssid) > 0) strlcpy(multiWiFi[n].clientSSID, ssid, 33); // this will keep old SSID intact if not present in JSON
      if (strlen(pass) > 0) strlcpy(multiWiFi[n].clientPass, pass, 65); // this will keep old password intact if not present in JSON
      if (strlen(bssid) > 0) fillStr2MAC(multiWiFi[n].bssid, bssid);
      multiWiFi[n].staticIP = nIP;
      multiWiFi[n].staticGW = nGW;
      multiWiFi[n].staticSN = nSN;
      if (++n >= WLED_MAX_WIFI_COUNT) break;
    }
  }

  JsonArray dns = nw[F("dns")];
  if (!dns.isNull()) {
    for (size_t i = 0; i < 4; i++) {
      CJSON(dnsAddress[i], dns[i]);
    }
  }

  JsonObject ap = doc["ap"];
  getStringFromJson(apSSID, ap[F("ssid")], 33);
  getStringFromJson(apPass, ap["psk"] , 65); //normally not present due to security
  //int ap_pskl = ap[F("pskl")];
  CJSON(apChannel, ap[F("chan")]);
  if (apChannel > 13 || apChannel < 1) apChannel = 1;
  CJSON(apHide, ap[F("hide")]);
  if (apHide > 1) apHide = 1;
  CJSON(apBehavior, ap[F("behav")]);
  /*
  JsonArray ap_ip = ap["ip"];
  for (unsigned i = 0; i < 4; i++) {
    apIP[i] = ap_ip;
  }
  */

  JsonObject wifi = doc[F("wifi")];
  noWifiSleep = !(wifi[F("sleep")] | !noWifiSleep); // inverted
  //noWifiSleep = !noWifiSleep;
  CJSON(force802_3g, wifi[F("phy")]); //force phy mode g?
#ifdef ARDUINO_ARCH_ESP32
  CJSON(txPower, wifi[F("txpwr")]);
  txPower = min(max((int)txPower, (int)WIFI_POWER_2dBm), (int)WIFI_POWER_19_5dBm);
#endif

  JsonObject hw = doc[F("hw")];

  // initialize LED pins and lengths prior to other HW (except for ethernet)
  JsonObject hw_led = hw["led"];

  uint16_t total = hw_led[F("total")] | strip.getLengthTotal();
  uint16_t ablMilliampsMax = hw_led[F("maxpwr")] | BusManager::ablMilliampsMax();
  BusManager::setMilliampsMax(ablMilliampsMax);
  Bus::setGlobalAWMode(hw_led[F("rgbwm")] | AW_GLOBAL_DISABLED);
  CJSON(strip.correctWB, hw_led["cct"]);
  CJSON(strip.cctFromRgb, hw_led[F("cr")]);
  CJSON(cctICused, hw_led[F("ic")]);
  uint8_t cctBlending = hw_led[F("cb")] | Bus::getCCTBlend();
  Bus::setCCTBlend(cctBlending);
  strip.setTargetFps(hw_led["fps"]); //NOP if 0, default 42 FPS
  #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  CJSON(useParallelI2S, hw_led[F("prl")]);
  #endif

  #ifndef WLED_DISABLE_2D
  // 2D Matrix Settings
  JsonObject matrix = hw_led[F("matrix")];
  if (!matrix.isNull()) {
    strip.isMatrix = true;
    unsigned numPanels = matrix[F("mpc")] | 1;
    numPanels = constrain(numPanels, 1, WLED_MAX_PANELS);
    strip.panel.clear();
    JsonArray panels = matrix[F("panels")];
    unsigned s = 0;
    if (!panels.isNull()) {
      strip.panel.reserve(numPanels);  // pre-allocate default 8x8 panels
      for (JsonObject pnl : panels) {
        WS2812FX::Panel p;
        CJSON(p.bottomStart, pnl["b"]);
        CJSON(p.rightStart,  pnl["r"]);
        CJSON(p.vertical,    pnl["v"]);
        CJSON(p.serpentine,  pnl["s"]);
        CJSON(p.xOffset,     pnl["x"]);
        CJSON(p.yOffset,     pnl["y"]);
        CJSON(p.height,      pnl["h"]);
        CJSON(p.width,       pnl["w"]);
        strip.panel.push_back(p);
        if (++s >= numPanels) break; // max panels reached
      }
    }
    strip.panel.shrink_to_fit();  // release unused memory (just in case)
    // cannot call strip.deserializeLedmap()/strip.setUpMatrix() here due to already locked JSON buffer
    //if (!fromFS) doInit2D = true; // if called at boot (fromFS==true), WLED::beginStrip() will take care of setting up matrix
  }
  #endif

  DEBUG_PRINTF_P(PSTR("Heap before buses: %d\n"), ESP.getFreeHeap());
  JsonArray ins = hw_led["ins"];
  if (!ins.isNull()) {
    int s = 0;  // bus iterator
    for (JsonObject elm : ins) {
      if (s >= WLED_MAX_BUSSES) break; // only counts physical buses
      uint8_t pins[5] = {255, 255, 255, 255, 255};
      JsonArray pinArr = elm["pin"];
      if (pinArr.size() == 0) continue;
      //pins[0] = pinArr[0];
      unsigned i = 0;
      for (int p : pinArr) {
        pins[i++] = p;
        if (i>4) break;
      }
      uint16_t length = elm["len"] | 1;
      uint8_t colorOrder = (int)elm[F("order")]; // contains white channel swap option in upper nibble
      uint8_t skipFirst = elm[F("skip")];
      uint16_t start = elm["start"] | 0;
      if (length==0 || start + length > MAX_LEDS) continue; // zero length or we reached max. number of LEDs, just stop
      uint8_t ledType = elm["type"] | TYPE_WS2812_RGB;
      bool reversed = elm["rev"];
      bool refresh = elm["ref"] | false;
      uint16_t freqkHz = elm[F("freq")] | 0;  // will be in kHz for DotStar and Hz for PWM
      uint8_t AWmode = elm[F("rgbwm")] | RGBW_MODE_MANUAL_ONLY;
      uint8_t maPerLed = elm[F("ledma")] | LED_MILLIAMPS_DEFAULT;
      uint16_t maMax = elm[F("maxpwr")] | (ablMilliampsMax * length) / total; // rough (incorrect?) per strip ABL calculation when no config exists
      // To disable brightness limiter we either set output max current to 0 or single LED current to 0 (we choose output max current)
      if (Bus::isPWM(ledType) || Bus::isOnOff(ledType) || Bus::isVirtual(ledType)) { // analog and virtual
        maPerLed = 0;
        maMax = 0;
      }
      ledType |= refresh << 7; // hack bit 7 to indicate strip requires off refresh

      busConfigs.emplace_back(ledType, pins, start, length, colorOrder, reversed, skipFirst, AWmode, freqkHz, maPerLed, maMax);
      doInitBusses = true;  // finalization done in beginStrip()
      if (!Bus::isVirtual(ledType)) s++; // have as many virtual buses as you want
    }
  } else if (fromFS) {
    //if busses failed to load, add default (fresh install, FS issue, ...)
    BusManager::removeAll();
    busConfigs.clear();

    DEBUG_PRINTLN(F("No busses, init default"));
    constexpr unsigned defDataTypes[] = {LED_TYPES};
    constexpr unsigned defDataPins[] = {DATA_PINS};
    constexpr unsigned defCounts[] = {PIXEL_COUNTS};
    constexpr unsigned defNumTypes = (sizeof(defDataTypes) / sizeof(defDataTypes[0]));
    constexpr unsigned defNumPins = (sizeof(defDataPins) / sizeof(defDataPins[0]));
    constexpr unsigned defNumCounts = (sizeof(defCounts) / sizeof(defCounts[0]));

    static_assert(validatePinsAndTypes(defDataTypes, defNumTypes, defNumPins),
                  "The default pin list defined in DATA_PINS does not match the pin requirements for the default buses defined in LED_TYPES");

    unsigned mem = 0;
    unsigned pinsIndex = 0;
    unsigned digitalCount = 0;
    for (unsigned i = 0; i < WLED_MAX_BUSSES; i++) {
      uint8_t defPin[OUTPUT_MAX_PINS];
      // if we have less types than requested outputs and they do not align, use last known type to set current type
      unsigned dataType = defDataTypes[(i < defNumTypes) ? i : defNumTypes -1];
      unsigned busPins = Bus::getNumberOfPins(dataType);

      // if we need more pins than available all outputs have been configured
      if (pinsIndex + busPins > defNumPins) break;

      // Assign all pins first so we can check for conflicts on this bus
      for (unsigned j = 0; j < busPins && j < OUTPUT_MAX_PINS; j++) defPin[j] = defDataPins[pinsIndex + j];

      for (unsigned j = 0; j < busPins && j < OUTPUT_MAX_PINS; j++) {
        bool validPin = true;
        // When booting without config (1st boot) we need to make sure GPIOs defined for LED output don't clash with hardware
        // i.e. DEBUG (GPIO1), DMX (2), SPI RAM/FLASH (16&17 on ESP32-WROVER/PICO), read/only pins, etc.
        // Pin should not be already allocated, read/only or defined for current bus
        while (PinManager::isPinAllocated(defPin[j]) || !PinManager::isPinOk(defPin[j],true)) {
          if (validPin) {
            DEBUG_PRINTLN(F("Some of the provided pins cannot be used to configure this LED output."));
            defPin[j] = 1; // start with GPIO1 and work upwards
            validPin = false;
          } else if (defPin[j] < WLED_NUM_PINS) {
            defPin[j]++;
          } else {
            DEBUG_PRINTLN(F("No available pins left! Can't configure output."));
            break;
          }
          // is the newly assigned pin already defined or used previously?
          // try next in line until there are no clashes or we run out of pins
          bool clash;
          do {
            clash = false;
            // check for conflicts on current bus
            for (const auto &pin : defPin) {
              if (&pin != &defPin[j] && pin == defPin[j]) {
                clash = true;
                break;
              }
            }
            // We already have a clash on current bus, no point checking next buses
            if (!clash) {
              // check for conflicts in defined pins
              for (const auto &pin : defDataPins) {
                if (pin == defPin[j]) {
                  clash = true;
                  break;
                }
              }
            }
            if (clash) defPin[j]++;
            if (defPin[j] >= WLED_NUM_PINS) break;
          } while (clash);
        }
      }
      pinsIndex += busPins;

      // if we have less counts than pins and they do not align, use last known count to set current count
      unsigned count = defCounts[(i < defNumCounts) ? i : defNumCounts -1];
      unsigned start = 0;
      // analog always has length 1
      if (Bus::isPWM(dataType) || Bus::isOnOff(dataType)) count = 1;
      BusConfig defCfg = BusConfig(dataType, defPin, start, count, DEFAULT_LED_COLOR_ORDER, false, 0, RGBW_MODE_MANUAL_ONLY, 0);
      mem += defCfg.memUsage(Bus::isDigital(dataType) && !Bus::is2Pin(dataType) ? digitalCount++ : 0);
      if (mem > MAX_LED_MEMORY) {
        DEBUG_PRINTF_P(PSTR("Out of LED memory! Bus %d (%d) #%u not created."), (int)dataType, (int)count, digitalCount);
        break;
      }
      busConfigs.push_back(defCfg); // use push_back for simplification as we needed defCfg to calculate memory usage
      doInitBusses = true;  // finalization done in beginStrip()
    }
    DEBUG_PRINTF_P(PSTR("LED buffer size: %uB/%uB\n"), mem, BusManager::memUsage());
  }
  if (hw_led["rev"] && BusManager::getNumBusses()) BusManager::getBus(0)->setReversed(true); //set 0.11 global reversed setting for first bus

  // read color order map configuration
  JsonArray hw_com = hw[F("com")];
  if (!hw_com.isNull()) {
    BusManager::getColorOrderMap().reserve(std::min(hw_com.size(), (size_t)WLED_MAX_COLOR_ORDER_MAPPINGS));
    for (JsonObject entry : hw_com) {
      uint16_t start = entry["start"] | 0;
      uint16_t len = entry["len"] | 0;
      uint8_t colorOrder = (int)entry[F("order")];
      if (!BusManager::getColorOrderMap().add(start, len, colorOrder)) break;
    }
  }

  // read multiple button configuration
  JsonObject btn_obj = hw["btn"];
  CJSON(touchThreshold, btn_obj[F("tt")]);
  bool pull = btn_obj[F("pull")] | (!disablePullUp); // if true, pullup is enabled
  disablePullUp = !pull;
  JsonArray hw_btn_ins = btn_obj["ins"];
  if (!hw_btn_ins.isNull()) {
    // deallocate existing button pins
    for (unsigned b = 0; b < WLED_MAX_BUTTONS; b++) PinManager::deallocatePin(btnPin[b], PinOwner::Button); // does nothing if trying to deallocate a pin with PinOwner != Button
    unsigned s = 0;
    for (JsonObject btn : hw_btn_ins) {
      CJSON(buttonType[s], btn["type"]);
      int8_t pin = btn["pin"][0] | -1;
      if (pin > -1 && PinManager::allocatePin(pin, false, PinOwner::Button)) {
        btnPin[s] = pin;
      #ifdef ARDUINO_ARCH_ESP32
        // ESP32 only: check that analog button pin is a valid ADC gpio
        if ((buttonType[s] == BTN_TYPE_ANALOG) || (buttonType[s] == BTN_TYPE_ANALOG_INVERTED)) {
          if (digitalPinToAnalogChannel(btnPin[s]) < 0) {
            // not an ADC analog pin
            DEBUG_PRINTF_P(PSTR("PIN ALLOC error: GPIO%d for analog button #%d is not an analog pin!\n"), btnPin[s], s);
            btnPin[s] = -1;
            PinManager::deallocatePin(pin,PinOwner::Button);
          } else {
            analogReadResolution(12); // see #4040
          }
        }
        else if ((buttonType[s] == BTN_TYPE_TOUCH || buttonType[s] == BTN_TYPE_TOUCH_SWITCH))
        {
          if (digitalPinToTouchChannel(btnPin[s]) < 0) {
            // not a touch pin
            DEBUG_PRINTF_P(PSTR("PIN ALLOC error: GPIO%d for touch button #%d is not a touch pin!\n"), btnPin[s], s);
            btnPin[s] = -1;
            PinManager::deallocatePin(pin,PinOwner::Button);
          }          
          //if touch pin, enable the touch interrupt on ESP32 S2 & S3
          #ifdef SOC_TOUCH_VERSION_2    // ESP32 S2 and S3 have a function to check touch state but need to attach an interrupt to do so
          else
          {
            touchAttachInterrupt(btnPin[s], touchButtonISR, touchThreshold << 4); // threshold on Touch V2 is much higher (1500 is a value given by Espressif example, I measured changes of over 5000)
          }
          #endif
        }
        else
      #endif
        {
          if (disablePullUp) {
            pinMode(btnPin[s], INPUT);
          } else {
            #ifdef ESP32
            pinMode(btnPin[s], buttonType[s]==BTN_TYPE_PUSH_ACT_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
            #else
            pinMode(btnPin[s], INPUT_PULLUP);
            #endif
          }
        }
      } else {
        btnPin[s] = -1;
      }
      JsonArray hw_btn_ins_0_macros = btn["macros"];
      CJSON(macroButton[s], hw_btn_ins_0_macros[0]);
      CJSON(macroLongPress[s],hw_btn_ins_0_macros[1]);
      CJSON(macroDoublePress[s], hw_btn_ins_0_macros[2]);
      if (++s >= WLED_MAX_BUTTONS) break; // max buttons reached
    }
    // clear remaining buttons
    for (; s<WLED_MAX_BUTTONS; s++) {
      btnPin[s]           = -1;
      buttonType[s]       = BTN_TYPE_NONE;
      macroButton[s]      = 0;
      macroLongPress[s]   = 0;
      macroDoublePress[s] = 0;
    }
  } else if (fromFS) {
    // new install/missing configuration (button 0 has defaults)
    // relies upon only being called once with fromFS == true, which is currently true.
    for (size_t s = 0; s < WLED_MAX_BUTTONS; s++) {
      if (buttonType[s] == BTN_TYPE_NONE || btnPin[s] < 0 || !PinManager::allocatePin(btnPin[s], false, PinOwner::Button)) {
        btnPin[s]     = -1;
        buttonType[s] = BTN_TYPE_NONE;
      }
      if (btnPin[s] >= 0) {
        if (disablePullUp) {
          pinMode(btnPin[s], INPUT);
        } else {
          #ifdef ESP32
          pinMode(btnPin[s], buttonType[s]==BTN_TYPE_PUSH_ACT_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
          #else
          pinMode(btnPin[s], INPUT_PULLUP);
          #endif
        }
      }
      macroButton[s]      = 0;
      macroLongPress[s]   = 0;
      macroDoublePress[s] = 0;
    }
  }

  CJSON(buttonPublishMqtt,btn_obj["mqtt"]);

  #ifndef WLED_DISABLE_INFRARED
  int hw_ir_pin = hw["ir"]["pin"] | -2; // 4
  if (hw_ir_pin > -2) {
    PinManager::deallocatePin(irPin, PinOwner::IR);
    if (PinManager::allocatePin(hw_ir_pin, false, PinOwner::IR)) {
      irPin = hw_ir_pin;
    } else {
      irPin = -1;
    }
  }
  CJSON(irEnabled, hw["ir"]["type"]);
  #endif
  CJSON(irApplyToAllSelected, hw["ir"]["sel"]);

  JsonObject relay = hw[F("relay")];

  rlyOpenDrain  = relay[F("odrain")] | rlyOpenDrain;
  int hw_relay_pin = relay["pin"] | -2;
  if (hw_relay_pin > -2) {
    PinManager::deallocatePin(rlyPin, PinOwner::Relay);
    if (PinManager::allocatePin(hw_relay_pin,true, PinOwner::Relay)) {
      rlyPin = hw_relay_pin;
      pinMode(rlyPin, rlyOpenDrain ? OUTPUT_OPEN_DRAIN : OUTPUT);
    } else {
      rlyPin = -1;
    }
  }
  if (relay.containsKey("rev")) {
    rlyMde = !relay["rev"];
  }

  CJSON(serialBaud, hw[F("baud")]);
  if (serialBaud < 96 || serialBaud > 15000) serialBaud = 1152;
  updateBaudRate(serialBaud *100);

  JsonArray hw_if_i2c = hw[F("if")][F("i2c-pin")];
  CJSON(i2c_sda, hw_if_i2c[0]);
  CJSON(i2c_scl, hw_if_i2c[1]);
  PinManagerPinType i2c[2] = { { i2c_sda, true }, { i2c_scl, true } };
  if (i2c_scl >= 0 && i2c_sda >= 0 && PinManager::allocateMultiplePins(i2c, 2, PinOwner::HW_I2C)) {
    #ifdef ESP32
    if (!Wire.setPins(i2c_sda, i2c_scl)) { i2c_scl = i2c_sda = -1; } // this will fail if Wire is initialised (Wire.begin() called prior)
    else Wire.begin();
    #else
    Wire.begin(i2c_sda, i2c_scl);
    #endif
  } else {
    i2c_sda = -1;
    i2c_scl = -1;
  }
  JsonArray hw_if_spi = hw[F("if")][F("spi-pin")];
  CJSON(spi_mosi, hw_if_spi[0]);
  CJSON(spi_sclk, hw_if_spi[1]);
  CJSON(spi_miso, hw_if_spi[2]);
  PinManagerPinType spi[3] = { { spi_mosi, true }, { spi_miso, true }, { spi_sclk, true } };
  if (spi_mosi >= 0 && spi_sclk >= 0 && PinManager::allocateMultiplePins(spi, 3, PinOwner::HW_SPI)) {
    #ifdef ESP32
    SPI.begin(spi_sclk, spi_miso, spi_mosi);  // SPI global uses VSPI on ESP32 and FSPI on C3, S3
    #else
    SPI.begin();
    #endif
  } else {
    spi_mosi = -1;
    spi_miso = -1;
    spi_sclk = -1;
  }

  //int hw_status_pin = hw[F("status")]["pin"]; // -1

  JsonObject light = doc[F("light")];
  CJSON(briMultiplier, light[F("scale-bri")]);
  CJSON(paletteBlend, light[F("pal-mode")]);
  CJSON(strip.autoSegments, light[F("aseg")]);
  CJSON(useRainbowWheel, light[F("rw")]);

  CJSON(gammaCorrectVal, light["gc"]["val"]); // default 2.2
  float light_gc_bri = light["gc"]["bri"];
  float light_gc_col = light["gc"]["col"];
  if (light_gc_bri > 1.0f) gammaCorrectBri = true;
  else                     gammaCorrectBri = false;
  if (light_gc_col > 1.0f) gammaCorrectCol = true;
  else                     gammaCorrectCol = false;
  if (gammaCorrectVal <= 1.0f || gammaCorrectVal > 3) {
    gammaCorrectVal = 1.0f; // no gamma correction
    gammaCorrectBri = false;
    gammaCorrectCol = false;
  }
  NeoGammaWLEDMethod::calcGammaTable(gammaCorrectVal); // fill look-up tables

  JsonObject light_tr = light["tr"];
  int tdd = light_tr["dur"] | -1;
  if (tdd >= 0) transitionDelay = transitionDelayDefault = tdd * 100;
  strip.setTransition(transitionDelayDefault);
  CJSON(randomPaletteChangeTime, light_tr[F("rpc")]);
  CJSON(useHarmonicRandomPalette, light_tr[F("hrp")]);

  JsonObject light_nl = light["nl"];
  CJSON(nightlightMode, light_nl["mode"]);
  byte prev = nightlightDelayMinsDefault;
  CJSON(nightlightDelayMinsDefault, light_nl["dur"]);
  if (nightlightDelayMinsDefault != prev) nightlightDelayMins = nightlightDelayMinsDefault;

  CJSON(nightlightTargetBri, light_nl[F("tbri")]);
  CJSON(macroNl, light_nl["macro"]);

  JsonObject def = doc["def"];
  CJSON(bootPreset, def["ps"]);
  CJSON(turnOnAtBoot, def["on"]); // true
  CJSON(briS, def["bri"]); // 128

  JsonObject interfaces = doc["if"];

  JsonObject if_sync = interfaces["sync"];
  CJSON(udpPort, if_sync[F("port0")]); // 21324
  CJSON(udpPort2, if_sync[F("port1")]); // 65506

#ifndef WLED_DISABLE_ESPNOW
  CJSON(useESPNowSync, if_sync[F("espnow")]);
#endif

  JsonObject if_sync_recv = if_sync[F("recv")];
  CJSON(receiveNotificationBrightness, if_sync_recv["bri"]);
  CJSON(receiveNotificationColor, if_sync_recv["col"]);
  CJSON(receiveNotificationEffects, if_sync_recv["fx"]);
  CJSON(receiveNotificationPalette, if_sync_recv["pal"]);
  CJSON(receiveGroups, if_sync_recv["grp"]);
  CJSON(receiveSegmentOptions, if_sync_recv["seg"]);
  CJSON(receiveSegmentBounds, if_sync_recv["sb"]);

  JsonObject if_sync_send = if_sync[F("send")];
  CJSON(sendNotifications, if_sync_send["en"]);
  sendNotificationsRT = sendNotifications;
  CJSON(notifyDirect, if_sync_send[F("dir")]);
  CJSON(notifyButton, if_sync_send["btn"]);
  CJSON(notifyAlexa, if_sync_send["va"]);
  CJSON(notifyHue, if_sync_send["hue"]);
  CJSON(syncGroups, if_sync_send["grp"]);
  if (if_sync_send[F("twice")]) udpNumRetries = 1; // import setting from 0.13 and earlier
  CJSON(udpNumRetries, if_sync_send["ret"]);

  JsonObject if_nodes = interfaces["nodes"];
  CJSON(nodeListEnabled, if_nodes[F("list")]);
  CJSON(nodeBroadcastEnabled, if_nodes[F("bcast")]);

  JsonObject if_live = interfaces["live"];
  CJSON(receiveDirect, if_live["en"]);  // UDP/Hyperion realtime
  CJSON(useMainSegmentOnly, if_live[F("mso")]);
  CJSON(realtimeRespectLedMaps, if_live[F("rlm")]);
  CJSON(e131Port, if_live["port"]); // 5568
  if (e131Port == DDP_DEFAULT_PORT) e131Port = E131_DEFAULT_PORT; // prevent double DDP port allocation
  CJSON(e131Multicast, if_live[F("mc")]);

  JsonObject if_live_dmx = if_live["dmx"];
  CJSON(e131Universe, if_live_dmx[F("uni")]);
  CJSON(e131SkipOutOfSequence, if_live_dmx[F("seqskip")]);
  CJSON(DMXAddress, if_live_dmx[F("addr")]);
  if (!DMXAddress || DMXAddress > 510) DMXAddress = 1;
  CJSON(DMXSegmentSpacing, if_live_dmx[F("dss")]);
  if (DMXSegmentSpacing > 150) DMXSegmentSpacing = 0;
  CJSON(e131Priority, if_live_dmx[F("e131prio")]);
  if (e131Priority > 200) e131Priority = 200;
  CJSON(DMXMode, if_live_dmx["mode"]);

  tdd = if_live[F("timeout")] | -1;
  if (tdd >= 0) realtimeTimeoutMs = tdd * 100;

  #ifdef WLED_ENABLE_DMX_INPUT
    CJSON(dmxInputTransmitPin, if_live_dmx[F("inputRxPin")]);
    CJSON(dmxInputReceivePin, if_live_dmx[F("inputTxPin")]);
    CJSON(dmxInputEnablePin, if_live_dmx[F("inputEnablePin")]);
    CJSON(dmxInputPort, if_live_dmx[F("dmxInputPort")]);
  #endif

  CJSON(arlsForceMaxBri, if_live[F("maxbri")]);
  CJSON(arlsDisableGammaCorrection, if_live[F("no-gc")]); // false
  CJSON(arlsOffset, if_live[F("offset")]); // 0

#ifndef WLED_DISABLE_ALEXA
  CJSON(alexaEnabled, interfaces["va"][F("alexa")]); // false
  CJSON(macroAlexaOn, interfaces["va"]["macros"][0]);
  CJSON(macroAlexaOff, interfaces["va"]["macros"][1]);
  CJSON(alexaNumPresets, interfaces["va"]["p"]);
#endif

#ifndef WLED_DISABLE_MQTT
  JsonObject if_mqtt = interfaces["mqtt"];
  CJSON(mqttEnabled, if_mqtt["en"]);
  getStringFromJson(mqttServer, if_mqtt[F("broker")], MQTT_MAX_SERVER_LEN+1);
  CJSON(mqttPort, if_mqtt["port"]); // 1883
  getStringFromJson(mqttUser, if_mqtt[F("user")], 41);
  getStringFromJson(mqttPass, if_mqtt["psk"], 65); //normally not present due to security
  getStringFromJson(mqttClientID, if_mqtt[F("cid")], 41);

  getStringFromJson(mqttDeviceTopic, if_mqtt[F("topics")][F("device")], MQTT_MAX_TOPIC_LEN+1); // "wled/test"
  getStringFromJson(mqttGroupTopic, if_mqtt[F("topics")][F("group")], MQTT_MAX_TOPIC_LEN+1); // ""
  CJSON(retainMqttMsg, if_mqtt[F("rtn")]);
#endif

#ifndef WLED_DISABLE_HUESYNC
  JsonObject if_hue = interfaces["hue"];
  CJSON(huePollingEnabled, if_hue["en"]);
  CJSON(huePollLightId, if_hue["id"]);
  tdd = if_hue[F("iv")] | -1;
  if (tdd >= 2) huePollIntervalMs = tdd * 100;

  JsonObject if_hue_recv = if_hue["recv"];
  CJSON(hueApplyOnOff, if_hue_recv["on"]);
  CJSON(hueApplyBri, if_hue_recv["bri"]);
  CJSON(hueApplyColor, if_hue_recv["col"]);

  JsonArray if_hue_ip = if_hue["ip"];

  for (unsigned i = 0; i < 4; i++)
    CJSON(hueIP[i], if_hue_ip[i]);
#endif

  JsonObject if_ntp = interfaces[F("ntp")];
  CJSON(ntpEnabled, if_ntp["en"]);
  getStringFromJson(ntpServerName, if_ntp[F("host")], 33); // "1.wled.pool.ntp.org"
  CJSON(currentTimezone, if_ntp[F("tz")]);
  CJSON(utcOffsetSecs, if_ntp[F("offset")]);
  CJSON(useAMPM, if_ntp[F("ampm")]);
  CJSON(longitude, if_ntp[F("ln")]);
  CJSON(latitude, if_ntp[F("lt")]);

  JsonObject ol = doc[F("ol")];
  CJSON(overlayCurrent ,ol[F("clock")]); // 0
  CJSON(countdownMode, ol[F("cntdwn")]);

  CJSON(overlayMin, ol["min"]);
  CJSON(overlayMax, ol[F("max")]);
  CJSON(analogClock12pixel, ol[F("o12pix")]);
  CJSON(analogClock5MinuteMarks, ol[F("o5m")]);
  CJSON(analogClockSecondsTrail, ol[F("osec")]);
  CJSON(analogClockSolidBlack, ol[F("osb")]);

  //timed macro rules
  JsonObject tm = doc[F("timers")];
  JsonObject cntdwn = tm[F("cntdwn")];
  JsonArray cntdwn_goal = cntdwn[F("goal")];
  CJSON(countdownYear,  cntdwn_goal[0]);
  CJSON(countdownMonth, cntdwn_goal[1]);
  CJSON(countdownDay,   cntdwn_goal[2]);
  CJSON(countdownHour,  cntdwn_goal[3]);
  CJSON(countdownMin,   cntdwn_goal[4]);
  CJSON(countdownSec,   cntdwn_goal[5]);
  CJSON(macroCountdown, cntdwn["macro"]);
  setCountdown();

  JsonArray timers = tm["ins"];
  uint8_t it = 0;
  for (JsonObject timer : timers) {
    if (it > 9) break;
    if (it<8 && timer[F("hour")]==255) it=8;  // hour==255 -> sunrise/sunset
    CJSON(timerHours[it], timer[F("hour")]);
    CJSON(timerMinutes[it], timer["min"]);
    CJSON(timerMacro[it], timer["macro"]);

    byte dowPrev = timerWeekday[it];
    //note: act is currently only 0 or 1.
    //the reason we are not using bool is that the on-disk type in 0.11.0 was already int
    int actPrev = timerWeekday[it] & 0x01;
    CJSON(timerWeekday[it], timer[F("dow")]);
    if (timerWeekday[it] != dowPrev) { //present in JSON
      timerWeekday[it] <<= 1; //add active bit
      int act = timer["en"] | actPrev;
      if (act) timerWeekday[it]++;
    }
    if (it<8) {
      JsonObject start = timer["start"];
      byte startm = start["mon"];
      if (startm) timerMonth[it] = (startm << 4);
      CJSON(timerDay[it], start["day"]);
      JsonObject end = timer["end"];
      CJSON(timerDayEnd[it], end["day"]);
      byte endm = end["mon"];
      if (startm) timerMonth[it] += endm & 0x0F;
      if (!(timerMonth[it] & 0x0F)) timerMonth[it] += 12; //default end month to 12
    }
    it++;
  }

  JsonObject ota = doc["ota"];
  const char* pwd = ota["psk"]; //normally not present due to security

  bool pwdCorrect = !otaLock; //always allow access if ota not locked
  if (pwd != nullptr && strncmp(otaPass, pwd, 33) == 0) pwdCorrect = true;

  if (pwdCorrect) { //only accept these values from cfg.json if ota is unlocked (else from wsec.json)
    CJSON(otaLock, ota[F("lock")]);
    CJSON(wifiLock, ota[F("lock-wifi")]);
    #ifndef WLED_DISABLE_OTA
    CJSON(aOtaEnabled, ota[F("aota")]);
    #endif
    getStringFromJson(otaPass, pwd, 33); //normally not present due to security
  }

  #ifdef WLED_ENABLE_DMX
  JsonObject dmx = doc["dmx"];
  CJSON(DMXChannels, dmx[F("chan")]);
  CJSON(DMXGap,dmx[F("gap")]);
  CJSON(DMXStart, dmx["start"]);
  CJSON(DMXStartLED,dmx[F("start-led")]);

  JsonArray dmx_fixmap = dmx[F("fixmap")];
  for (int i = 0; i < dmx_fixmap.size(); i++) {
    if (i > 14) break;
    CJSON(DMXFixtureMap[i],dmx_fixmap[i]);
  }

  CJSON(e131ProxyUniverse, dmx[F("e131proxy")]);
  #endif

  DEBUG_PRINTLN(F("Starting usermod config."));
  JsonObject usermods_settings = doc["um"];
  if (!usermods_settings.isNull()) {
    needsSave = !UsermodManager::readFromConfig(usermods_settings);
  }

  if (fromFS) return needsSave;
  // if from /json/cfg
  doReboot = doc[F("rb")] | doReboot;
  if (doInitBusses) return false; // no save needed, will do after bus init in wled.cpp loop
  return (doc["sv"] | true);
}


static const char s_cfg_json[] PROGMEM = "/cfg.json";

void deserializeConfigFromFS() {
  [[maybe_unused]] bool success = deserializeConfigSec();
  #ifdef WLED_ADD_EEPROM_SUPPORT
  if (!success) { //if file does not exist, try reading from EEPROM
    deEEPSettings();
  }
  #endif

  if (!requestJSONBufferLock(1)) return;

  DEBUG_PRINTLN(F("Reading settings from /cfg.json..."));

  success = readObjectFromFile(s_cfg_json, nullptr, pDoc);

  // NOTE: This routine deserializes *and* applies the configuration
  //       Therefore, must also initialize ethernet from this function
  JsonObject root = pDoc->as<JsonObject>();
  bool needsSave = deserializeConfig(root, true);
  releaseJSONBufferLock();

  if (needsSave) serializeConfigToFS(); // usermods required new parameters
}

void serializeConfigToFS() {
  serializeConfigSec();

  DEBUG_PRINTLN(F("Writing settings to /cfg.json..."));

  if (!requestJSONBufferLock(2)) return;

  JsonObject root = pDoc->to<JsonObject>();

  serializeConfig(root);

  File f = WLED_FS.open(FPSTR(s_cfg_json), "w");
  if (f) serializeJson(root, f);
  f.close();
  releaseJSONBufferLock();

  configNeedsWrite = false;
}

void serializeConfig(JsonObject root) {
  JsonArray rev = root.createNestedArray("rev");
  rev.add(1); //major settings revision
  rev.add(0); //minor settings revision

  root[F("vid")] = VERSION;

  JsonObject id = root.createNestedObject("id");
  id[F("mdns")] = cmDNS;
  id[F("name")] = serverDescription;
#ifndef WLED_DISABLE_ALEXA
  id[F("inv")] = alexaInvocationName;
#endif
  id[F("sui")] = simplifiedUI;

  JsonObject nw = root.createNestedObject("nw");
#ifndef WLED_DISABLE_ESPNOW
  nw[F("espnow")] = enableESPNow;
  JsonArray lrem = nw.createNestedArray(F("linked_remote"));
  for (size_t i = 0; i < linked_remotes.size(); i++) {
    lrem.add(linked_remotes[i].data());
  }
#endif

  JsonArray nw_ins = nw.createNestedArray("ins");
  for (size_t n = 0; n < multiWiFi.size(); n++) {
    JsonObject wifi = nw_ins.createNestedObject();
    wifi[F("ssid")] = multiWiFi[n].clientSSID;
    wifi[F("pskl")] = strlen(multiWiFi[n].clientPass);
    char bssid[13];
    fillMAC2Str(bssid, multiWiFi[n].bssid);
    wifi[F("bssid")] = bssid;
    JsonArray wifi_ip = wifi.createNestedArray("ip");
    JsonArray wifi_gw = wifi.createNestedArray("gw");
    JsonArray wifi_sn = wifi.createNestedArray("sn");
    for (size_t i = 0; i < 4; i++) {
      wifi_ip.add(multiWiFi[n].staticIP[i]);
      wifi_gw.add(multiWiFi[n].staticGW[i]);
      wifi_sn.add(multiWiFi[n].staticSN[i]);
    }
  }

  JsonArray dns = nw.createNestedArray(F("dns"));
  for (size_t i = 0; i < 4; i++) {
    dns.add(dnsAddress[i]);
  }

  JsonObject ap = root.createNestedObject("ap");
  ap[F("ssid")] = apSSID;
  ap[F("pskl")] = strlen(apPass);
  ap[F("chan")] = apChannel;
  ap[F("hide")] = apHide;
  ap[F("behav")] = apBehavior;

  JsonArray ap_ip = ap.createNestedArray("ip");
  ap_ip.add(4);
  ap_ip.add(3);
  ap_ip.add(2);
  ap_ip.add(1);

  JsonObject wifi = root.createNestedObject(F("wifi"));
  wifi[F("sleep")] = !noWifiSleep;
  wifi[F("phy")] = force802_3g;
#ifdef ARDUINO_ARCH_ESP32
  wifi[F("txpwr")] = txPower;
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  JsonObject ethernet = root.createNestedObject("eth");
  ethernet["type"] = ethernetType;
  if (ethernetType != WLED_ETH_NONE && ethernetType < WLED_NUM_ETH_TYPES) {
    JsonArray pins = ethernet.createNestedArray("pin");
    for (unsigned p=0; p<WLED_ETH_RSVD_PINS_COUNT; p++) pins.add(esp32_nonconfigurable_ethernet_pins[p].pin);
    if (ethernetBoards[ethernetType].eth_power>=0)     pins.add(ethernetBoards[ethernetType].eth_power);
    if (ethernetBoards[ethernetType].eth_mdc>=0)       pins.add(ethernetBoards[ethernetType].eth_mdc);
    if (ethernetBoards[ethernetType].eth_mdio>=0)      pins.add(ethernetBoards[ethernetType].eth_mdio);
    switch (ethernetBoards[ethernetType].eth_clk_mode) {
      case ETH_CLOCK_GPIO0_IN:
      case ETH_CLOCK_GPIO0_OUT:
        pins.add(0);
        break;
      case ETH_CLOCK_GPIO16_OUT:
        pins.add(16);
        break;
      case ETH_CLOCK_GPIO17_OUT:
        pins.add(17);
        break;
    }
  }
#endif

  JsonObject hw = root.createNestedObject(F("hw"));

  JsonObject hw_led = hw.createNestedObject("led");
  hw_led[F("total")] = strip.getLengthTotal(); //provided for compatibility on downgrade and per-output ABL
  hw_led[F("maxpwr")] = BusManager::ablMilliampsMax();
//  hw_led[F("ledma")] = 0; // no longer used
  hw_led["cct"] = strip.correctWB;
  hw_led[F("cr")] = strip.cctFromRgb;
  hw_led[F("ic")] = cctICused;
  hw_led[F("cb")] = Bus::getCCTBlend();
  hw_led["fps"] = strip.getTargetFps();
  hw_led[F("rgbwm")] = Bus::getGlobalAWMode(); // global auto white mode override
  #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  hw_led[F("prl")] = BusManager::hasParallelOutput();
  #endif

  #ifndef WLED_DISABLE_2D
  // 2D Matrix Settings
  if (strip.isMatrix) {
    JsonObject matrix = hw_led.createNestedObject(F("matrix"));
    matrix[F("mpc")] = strip.panel.size();
    JsonArray panels = matrix.createNestedArray(F("panels"));
    for (size_t i = 0; i < strip.panel.size(); i++) {
      JsonObject pnl = panels.createNestedObject();
      pnl["b"] = strip.panel[i].bottomStart;
      pnl["r"] = strip.panel[i].rightStart;
      pnl["v"] = strip.panel[i].vertical;
      pnl["s"] = strip.panel[i].serpentine;
      pnl["x"] = strip.panel[i].xOffset;
      pnl["y"] = strip.panel[i].yOffset;
      pnl["h"] = strip.panel[i].height;
      pnl["w"] = strip.panel[i].width;
    }
  }
  #endif

  JsonArray hw_led_ins = hw_led.createNestedArray("ins");

  for (size_t s = 0; s < BusManager::getNumBusses(); s++) {
    DEBUG_PRINTF_P(PSTR("Cfg: Saving bus #%u\n"), s);
    const Bus *bus = BusManager::getBus(s);
    if (!bus || !bus->isOk()) break;
    DEBUG_PRINTF_P(PSTR("  (%d-%d, type:%d, CO:%d, rev:%d, skip:%d, AW:%d kHz:%d, mA:%d/%d)\n"),
      (int)bus->getStart(), (int)(bus->getStart()+bus->getLength()),
      (int)(bus->getType() & 0x7F),
      (int)bus->getColorOrder(),
      (int)bus->isReversed(),
      (int)bus->skippedLeds(),
      (int)bus->getAutoWhiteMode(),
      (int)bus->getFrequency(),
      (int)bus->getLEDCurrent(), (int)bus->getMaxCurrent()
    );
    JsonObject ins = hw_led_ins.createNestedObject();
    ins["start"] = bus->getStart();
    ins["len"]   = bus->getLength();
    JsonArray ins_pin = ins.createNestedArray("pin");
    uint8_t pins[5];
    uint8_t nPins = bus->getPins(pins);
    for (int i = 0; i < nPins; i++) ins_pin.add(pins[i]);
    ins[F("order")]  = bus->getColorOrder();
    ins["rev"]       = bus->isReversed();
    ins[F("skip")]   = bus->skippedLeds();
    ins["type"]      = bus->getType() & 0x7F;
    ins["ref"]       = bus->isOffRefreshRequired();
    ins[F("rgbwm")]  = bus->getAutoWhiteMode();
    ins[F("freq")]   = bus->getFrequency();
    ins[F("maxpwr")] = bus->getMaxCurrent();
    ins[F("ledma")]  = bus->getLEDCurrent();
  }

  JsonArray hw_com = hw.createNestedArray(F("com"));
  const ColorOrderMap& com = BusManager::getColorOrderMap();
  for (size_t s = 0; s < com.count(); s++) {
    const ColorOrderMapEntry *entry = com.get(s);
    if (!entry || !entry->len) break;
    JsonObject co = hw_com.createNestedObject();
    co["start"] = entry->start;
    co["len"] = entry->len;
    co[F("order")] = entry->colorOrder;
  }

  // button(s)
  JsonObject hw_btn = hw.createNestedObject("btn");
  hw_btn["max"] = WLED_MAX_BUTTONS; // just information about max number of buttons (not actually used)
  hw_btn[F("pull")] = !disablePullUp;
  JsonArray hw_btn_ins = hw_btn.createNestedArray("ins");

  // configuration for all buttons
  for (int i = 0; i < WLED_MAX_BUTTONS; i++) {
    JsonObject hw_btn_ins_0 = hw_btn_ins.createNestedObject();
    hw_btn_ins_0["type"] = buttonType[i];
    JsonArray hw_btn_ins_0_pin = hw_btn_ins_0.createNestedArray("pin");
    hw_btn_ins_0_pin.add(btnPin[i]);
    JsonArray hw_btn_ins_0_macros = hw_btn_ins_0.createNestedArray("macros");
    hw_btn_ins_0_macros.add(macroButton[i]);
    hw_btn_ins_0_macros.add(macroLongPress[i]);
    hw_btn_ins_0_macros.add(macroDoublePress[i]);
  }

  hw_btn[F("tt")] = touchThreshold;
  hw_btn["mqtt"] = buttonPublishMqtt;

  JsonObject hw_ir = hw.createNestedObject("ir");
  #ifndef WLED_DISABLE_INFRARED
  hw_ir["pin"] = irPin;
  hw_ir["type"] = irEnabled;  // the byte 'irEnabled' does contain the IR-Remote Type ( 0=disabled )
  #endif
  hw_ir["sel"] = irApplyToAllSelected;

  JsonObject hw_relay = hw.createNestedObject(F("relay"));
  hw_relay["pin"] = rlyPin;
  hw_relay["rev"] = !rlyMde;
  hw_relay[F("odrain")] = rlyOpenDrain;

  hw[F("baud")] = serialBaud;

  JsonObject hw_if = hw.createNestedObject(F("if"));
  JsonArray hw_if_i2c = hw_if.createNestedArray("i2c-pin");
  hw_if_i2c.add(i2c_sda);
  hw_if_i2c.add(i2c_scl);
  JsonArray hw_if_spi = hw_if.createNestedArray("spi-pin");
  hw_if_spi.add(spi_mosi);
  hw_if_spi.add(spi_sclk);
  hw_if_spi.add(spi_miso);

  //JsonObject hw_status = hw.createNestedObject("status");
  //hw_status["pin"] = -1;

  JsonObject light = root.createNestedObject(F("light"));
  light[F("scale-bri")] = briMultiplier;
  light[F("pal-mode")] = paletteBlend;
  light[F("aseg")] = strip.autoSegments;
  light[F("rw")] = useRainbowWheel;

  JsonObject light_gc = light.createNestedObject("gc");
  light_gc["bri"] = (gammaCorrectBri) ? gammaCorrectVal : 1.0f;  // keep compatibility
  light_gc["col"] = (gammaCorrectCol) ? gammaCorrectVal : 1.0f;  // keep compatibility
  light_gc["val"] = gammaCorrectVal;

  JsonObject light_tr = light.createNestedObject("tr");
  light_tr["dur"] = transitionDelayDefault / 100;
  light_tr[F("rpc")] = randomPaletteChangeTime;
  light_tr[F("hrp")] = useHarmonicRandomPalette;

  JsonObject light_nl = light.createNestedObject("nl");
  light_nl["mode"] = nightlightMode;
  light_nl["dur"] = nightlightDelayMinsDefault;
  light_nl[F("tbri")] = nightlightTargetBri;
  light_nl["macro"] = macroNl;

  JsonObject def = root.createNestedObject("def");
  def["ps"] = bootPreset;
  def["on"] = turnOnAtBoot;
  def["bri"] = briS;

  JsonObject interfaces = root.createNestedObject("if");

  JsonObject if_sync = interfaces.createNestedObject("sync");
  if_sync[F("port0")] = udpPort;
  if_sync[F("port1")] = udpPort2;

#ifndef WLED_DISABLE_ESPNOW
  if_sync[F("espnow")] = useESPNowSync;
#endif

  JsonObject if_sync_recv = if_sync.createNestedObject(F("recv"));
  if_sync_recv["bri"] = receiveNotificationBrightness;
  if_sync_recv["col"] = receiveNotificationColor;
  if_sync_recv["fx"]  = receiveNotificationEffects;
  if_sync_recv["pal"] = receiveNotificationPalette;
  if_sync_recv["grp"] = receiveGroups;
  if_sync_recv["seg"] = receiveSegmentOptions;
  if_sync_recv["sb"]  = receiveSegmentBounds;

  JsonObject if_sync_send = if_sync.createNestedObject(F("send"));
  if_sync_send["en"] = sendNotifications;
  if_sync_send[F("dir")] = notifyDirect;
  if_sync_send["btn"] = notifyButton;
  if_sync_send["va"] = notifyAlexa;
  if_sync_send["hue"] = notifyHue;
  if_sync_send["grp"] = syncGroups;
  if_sync_send["ret"] = udpNumRetries;

  JsonObject if_nodes = interfaces.createNestedObject("nodes");
  if_nodes[F("list")] = nodeListEnabled;
  if_nodes[F("bcast")] = nodeBroadcastEnabled;

  JsonObject if_live = interfaces.createNestedObject("live");
  if_live["en"] = receiveDirect; // UDP/Hyperion realtime
  if_live[F("mso")] = useMainSegmentOnly;
  if_live[F("rlm")] = realtimeRespectLedMaps;
  if_live["port"] = e131Port;
  if_live[F("mc")] = e131Multicast;

  JsonObject if_live_dmx = if_live.createNestedObject("dmx");
  if_live_dmx[F("uni")] = e131Universe;
  if_live_dmx[F("seqskip")] = e131SkipOutOfSequence;
  if_live_dmx[F("e131prio")] = e131Priority;
  if_live_dmx[F("addr")] = DMXAddress;
  if_live_dmx[F("dss")] = DMXSegmentSpacing;
  if_live_dmx["mode"] = DMXMode;
  #ifdef WLED_ENABLE_DMX_INPUT
    if_live_dmx[F("inputRxPin")] = dmxInputTransmitPin;
    if_live_dmx[F("inputTxPin")] = dmxInputReceivePin;
    if_live_dmx[F("inputEnablePin")] = dmxInputEnablePin;
    if_live_dmx[F("dmxInputPort")] = dmxInputPort;
  #endif

  if_live[F("timeout")] = realtimeTimeoutMs / 100;
  if_live[F("maxbri")] = arlsForceMaxBri;
  if_live[F("no-gc")] = arlsDisableGammaCorrection;
  if_live[F("offset")] = arlsOffset;

#ifndef WLED_DISABLE_ALEXA
  JsonObject if_va = interfaces.createNestedObject("va");
  if_va[F("alexa")] = alexaEnabled;

  JsonArray if_va_macros = if_va.createNestedArray("macros");
  if_va_macros.add(macroAlexaOn);
  if_va_macros.add(macroAlexaOff);

  if_va["p"] = alexaNumPresets;
#endif

#ifndef WLED_DISABLE_MQTT
  JsonObject if_mqtt = interfaces.createNestedObject("mqtt");
  if_mqtt["en"] = mqttEnabled;
  if_mqtt[F("broker")] = mqttServer;
  if_mqtt["port"] = mqttPort;
  if_mqtt[F("user")] = mqttUser;
  if_mqtt[F("pskl")] = strlen(mqttPass);
  if_mqtt[F("cid")] = mqttClientID;
  if_mqtt[F("rtn")] = retainMqttMsg;

  JsonObject if_mqtt_topics = if_mqtt.createNestedObject(F("topics"));
  if_mqtt_topics[F("device")] = mqttDeviceTopic;
  if_mqtt_topics[F("group")] = mqttGroupTopic;
#endif

#ifndef WLED_DISABLE_HUESYNC
  JsonObject if_hue = interfaces.createNestedObject("hue");
  if_hue["en"] = huePollingEnabled;
  if_hue["id"] = huePollLightId;
  if_hue[F("iv")] = huePollIntervalMs / 100;

  JsonObject if_hue_recv = if_hue.createNestedObject(F("recv"));
  if_hue_recv["on"] = hueApplyOnOff;
  if_hue_recv["bri"] = hueApplyBri;
  if_hue_recv["col"] = hueApplyColor;

  JsonArray if_hue_ip = if_hue.createNestedArray("ip");
  for (unsigned i = 0; i < 4; i++) {
    if_hue_ip.add(hueIP[i]);
  }
#endif

  JsonObject if_ntp = interfaces.createNestedObject("ntp");
  if_ntp["en"] = ntpEnabled;
  if_ntp[F("host")] = ntpServerName;
  if_ntp[F("tz")] = currentTimezone;
  if_ntp[F("offset")] = utcOffsetSecs;
  if_ntp[F("ampm")] = useAMPM;
  if_ntp[F("ln")] = longitude;
  if_ntp[F("lt")] = latitude;

  JsonObject ol = root.createNestedObject("ol");
  ol[F("clock")] = overlayCurrent;
  ol[F("cntdwn")] = countdownMode;

  ol["min"] = overlayMin;
  ol[F("max")] = overlayMax;
  ol[F("o12pix")] = analogClock12pixel;
  ol[F("o5m")] = analogClock5MinuteMarks;
  ol[F("osec")] = analogClockSecondsTrail;
  ol[F("osb")] = analogClockSolidBlack;

  JsonObject timers = root.createNestedObject(F("timers"));

  JsonObject cntdwn = timers.createNestedObject(F("cntdwn"));
  JsonArray goal = cntdwn.createNestedArray(F("goal"));
  goal.add(countdownYear); goal.add(countdownMonth); goal.add(countdownDay);
  goal.add(countdownHour); goal.add(countdownMin); goal.add(countdownSec);
  cntdwn["macro"] = macroCountdown;

  JsonArray timers_ins = timers.createNestedArray("ins");

  for (unsigned i = 0; i < 10; i++) {
    if (timerMacro[i] == 0 && timerHours[i] == 0 && timerMinutes[i] == 0) continue; // sunrise/sunset get saved always (timerHours=255)
    JsonObject timers_ins0 = timers_ins.createNestedObject();
    timers_ins0["en"] = (timerWeekday[i] & 0x01);
    timers_ins0[F("hour")] = timerHours[i];
    timers_ins0["min"] = timerMinutes[i];
    timers_ins0["macro"] = timerMacro[i];
    timers_ins0[F("dow")] = timerWeekday[i] >> 1;
    if (i<8) {
      JsonObject start = timers_ins0.createNestedObject("start");
      start["mon"] = (timerMonth[i] >> 4) & 0xF;
      start["day"] = timerDay[i];
      JsonObject end = timers_ins0.createNestedObject("end");
      end["mon"] = timerMonth[i] & 0xF;
      end["day"] = timerDayEnd[i];
    }
  }

  JsonObject ota = root.createNestedObject("ota");
  ota[F("lock")] = otaLock;
  ota[F("lock-wifi")] = wifiLock;
  ota[F("pskl")] = strlen(otaPass);
  #ifndef WLED_DISABLE_OTA
  ota[F("aota")] = aOtaEnabled;
  #endif

  #ifdef WLED_ENABLE_DMX
  JsonObject dmx = root.createNestedObject("dmx");
  dmx[F("chan")] = DMXChannels;
  dmx[F("gap")] = DMXGap;
  dmx["start"] = DMXStart;
  dmx[F("start-led")] = DMXStartLED;

  JsonArray dmx_fixmap = dmx.createNestedArray(F("fixmap"));
  for (unsigned i = 0; i < 15; i++) {
    dmx_fixmap.add(DMXFixtureMap[i]);
  }

  dmx[F("e131proxy")] = e131ProxyUniverse;
  #endif

  JsonObject usermods_settings = root.createNestedObject("um");
  UsermodManager::addToConfig(usermods_settings);
}


static const char s_wsec_json[] PROGMEM = "/wsec.json";

//settings in /wsec.json, not accessible via webserver, for passwords and tokens
bool deserializeConfigSec() {
  DEBUG_PRINTLN(F("Reading settings from /wsec.json..."));

  if (!requestJSONBufferLock(3)) return false;

  bool success = readObjectFromFile(s_wsec_json, nullptr, pDoc);
  if (!success) {
    releaseJSONBufferLock();
    return false;
  }

  JsonObject root = pDoc->as<JsonObject>();

  size_t n = 0;
  JsonArray nw_ins = root["nw"]["ins"];
  if (!nw_ins.isNull()) {
    if (nw_ins.size() > 1 && nw_ins.size() > multiWiFi.size()) multiWiFi.resize(nw_ins.size()); // resize constructs objects while resizing
    for (JsonObject wifi : nw_ins) {
      char pw[65] = "";
      getStringFromJson(pw, wifi["psk"], 65);
      strlcpy(multiWiFi[n].clientPass, pw, 65);
      if (++n >= WLED_MAX_WIFI_COUNT) break;
    }
  }

  JsonObject ap = root["ap"];
  getStringFromJson(apPass, ap["psk"] , 65);

  [[maybe_unused]] JsonObject interfaces = root["if"];

#ifndef WLED_DISABLE_MQTT
  JsonObject if_mqtt = interfaces["mqtt"];
  getStringFromJson(mqttPass, if_mqtt["psk"], 65);
#endif

#ifndef WLED_DISABLE_HUESYNC
  getStringFromJson(hueApiKey, interfaces["hue"][F("key")], 47);
#endif

  getStringFromJson(settingsPIN, root["pin"], 5);
  correctPIN = !strlen(settingsPIN);

  JsonObject ota = root["ota"];
  getStringFromJson(otaPass, ota[F("pwd")], 33);
  CJSON(otaLock, ota[F("lock")]);
  CJSON(wifiLock, ota[F("lock-wifi")]);
  #ifndef WLED_DISABLE_OTA
  CJSON(aOtaEnabled, ota[F("aota")]);
  #endif

  releaseJSONBufferLock();
  return true;
}

void serializeConfigSec() {
  DEBUG_PRINTLN(F("Writing settings to /wsec.json..."));

  if (!requestJSONBufferLock(4)) return;

  JsonObject root = pDoc->to<JsonObject>();

  JsonObject nw = root.createNestedObject("nw");

  JsonArray nw_ins = nw.createNestedArray("ins");
  for (size_t i = 0; i < multiWiFi.size(); i++) {
    JsonObject wifi = nw_ins.createNestedObject();
    wifi[F("psk")] = multiWiFi[i].clientPass;
  }

  JsonObject ap = root.createNestedObject("ap");
  ap["psk"] = apPass;

  [[maybe_unused]] JsonObject interfaces = root.createNestedObject("if");
#ifndef WLED_DISABLE_MQTT
  JsonObject if_mqtt = interfaces.createNestedObject("mqtt");
  if_mqtt["psk"] = mqttPass;
#endif
#ifndef WLED_DISABLE_HUESYNC
  JsonObject if_hue = interfaces.createNestedObject("hue");
  if_hue[F("key")] = hueApiKey;
#endif

  root["pin"] = settingsPIN;

  JsonObject ota = root.createNestedObject("ota");
  ota[F("pwd")] = otaPass;
  ota[F("lock")] = otaLock;
  ota[F("lock-wifi")] = wifiLock;
  #ifndef WLED_DISABLE_OTA
  ota[F("aota")] = aOtaEnabled;
  #endif

  File f = WLED_FS.open(FPSTR(s_wsec_json), "w");
  if (f) serializeJson(root, f);
  f.close();
  releaseJSONBufferLock();
}