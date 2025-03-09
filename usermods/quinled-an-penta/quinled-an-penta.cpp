#include "U8g2lib.h"
#include "SHT85.h"
#include "Wire.h"
#include "wled.h"

class QuinLEDAnPentaUsermod : public Usermod
{
  private:
    bool enabled = false;
    bool firstRunDone = false;
    bool initDone = false;
    U8G2 *oledDisplay = nullptr;
    SHT *sht30TempHumidSensor;

    // Network info vars
    bool networkHasChanged = false;
    bool lastKnownNetworkConnected;
    IPAddress lastKnownIp;
    bool lastKnownWiFiConnected;
    String lastKnownSsid;
    bool lastKnownApActive;
    char *lastKnownApSsid;
    char *lastKnownApPass;
    byte lastKnownApChannel;
    int lastKnownEthType;
    bool lastKnownEthLinkUp;

    // Brightness / LEDC vars
    byte lastKnownBri = 0;
    int8_t currentBussesNumPins[5] = {0, 0, 0, 0, 0};
    int8_t currentLedPins[5] = {0, 0, 0, 0, 0};
    uint8_t currentLedcReads[5] = {0, 0, 0, 0, 0};
    uint8_t lastKnownLedcReads[5] = {0, 0, 0, 0, 0};

    // OLED vars
    bool oledEnabled = false;
    bool oledInitDone = false;
    bool oledUseProgressBars = false;
    bool oledFlipScreen = false;
    bool oledFixBuggedScreen = false;
    byte oledMaxPage = 3;
    byte oledCurrentPage = 3; // Start with the network page to help identifying the IP
    byte oledSecondsPerPage = 10;
    unsigned long oledLogoDrawn = 0;
    unsigned long oledLastTimeUpdated = 0;
    unsigned long oledLastTimePageChange = 0;
    unsigned long oledLastTimeFixBuggedScreen = 0;

    // SHT30 vars
    bool shtEnabled = false;
    bool shtInitDone = false;
    bool shtReadDataSuccess = false;
    byte shtI2cAddress = 0x44;
    unsigned long shtLastTimeUpdated = 0;
    bool shtDataRequested = false;
    float shtCurrentTemp = 0;
    float shtLastKnownTemp = 0;
    float shtCurrentHumidity = 0;
    float shtLastKnownHumidity = 0;

    // Pin/IO vars
    const int8_t anPentaLEDPins[5] = {14, 13, 12, 4, 2};
    int8_t oledSpiClk = 15;
    int8_t oledSpiData = 16;
    int8_t oledSpiCs = 27;
    int8_t oledSpiDc = 32;
    int8_t oledSpiRst = 33;
    int8_t shtSda = 1;
    int8_t shtScl = 3;


    bool isAnPentaLedPin(int8_t pin)
    {
      for(int8_t i = 0; i <= 4; i++)
      {
        if(anPentaLEDPins[i] == pin)
          return true;
      }
      return false;
    }

    void getCurrentUsedLedPins()
    {
      for (int8_t lp = 0; lp <= 4; lp++) currentLedPins[lp] = 0;
      byte numBusses = BusManager::getNumBusses();
      byte numUsedPins = 0;

      for (int8_t b = 0; b < numBusses; b++) {
        Bus* curBus = BusManager::getBus(b);
        if (curBus != nullptr) {
          uint8_t pins[5] = {0, 0, 0, 0, 0};
          currentBussesNumPins[b] = curBus->getPins(pins);
          for (int8_t p = 0; p < currentBussesNumPins[b]; p++) {
            if (isAnPentaLedPin(pins[p])) {
              currentLedPins[numUsedPins] = pins[p];
              numUsedPins++;
            }
          }
        }
      }
    }

    void getCurrentLedcValues()
    {
      byte numBusses = BusManager::getNumBusses();
      byte numLedc = 0;

      for (int8_t b = 0; b < numBusses; b++) {
        Bus* curBus = BusManager::getBus(b);
        if (curBus != nullptr) {
          uint32_t curPixColor = curBus->getPixelColor(0);
          uint8_t _data[5] = {255, 255, 255, 255, 255};
          _data[3] = curPixColor >> 24;
          _data[0] = curPixColor >> 16;
          _data[1] = curPixColor >> 8;
          _data[2] = curPixColor;

          for (uint8_t i = 0; i < currentBussesNumPins[b]; i++) {
            currentLedcReads[numLedc] = (_data[i] * bri) / 255;
            numLedc++;
          }
        }
      }
    }


    void initOledDisplay()
    {
      PinManagerPinType pins[5] = { { oledSpiClk, true }, { oledSpiData, true }, { oledSpiCs, true }, { oledSpiDc, true }, { oledSpiRst, true } };
      if (!PinManager::allocateMultiplePins(pins, 5, PinOwner::UM_QuinLEDAnPenta)) {
        DEBUG_PRINTF("[%s] OLED pin allocation failed!\n", _name);
        oledEnabled = oledInitDone = false;
        return;
      }

      oledDisplay = (U8G2 *) new U8G2_SSD1306_128X64_NONAME_2_4W_SW_SPI(U8G2_R0, oledSpiClk, oledSpiData, oledSpiCs, oledSpiDc, oledSpiRst);
      if (oledDisplay == nullptr) {
        DEBUG_PRINTF("[%s] OLED init failed!\n", _name);
        oledEnabled = oledInitDone = false;
        return;
      }

      oledDisplay->begin();
      oledDisplay->setBusClock(40 * 1000 * 1000);
      oledDisplay->setContrast(10);
      oledDisplay->setPowerSave(0);
      oledDisplay->setFont(u8g2_font_6x10_tf);
      oledDisplay->setFlipMode(oledFlipScreen);

      oledDisplay->firstPage();
      do {
        oledDisplay->drawXBMP(0, 16, 128, 36, quinLedLogo);
      } while (oledDisplay->nextPage());
      oledLogoDrawn = millis();

      oledInitDone = true;
    }

    void cleanupOledDisplay()
    {
      if (oledInitDone) {
        oledDisplay->clear();
      }

      PinManager::deallocatePin(oledSpiClk, PinOwner::UM_QuinLEDAnPenta);
      PinManager::deallocatePin(oledSpiData, PinOwner::UM_QuinLEDAnPenta);
      PinManager::deallocatePin(oledSpiCs, PinOwner::UM_QuinLEDAnPenta);
      PinManager::deallocatePin(oledSpiDc, PinOwner::UM_QuinLEDAnPenta);
      PinManager::deallocatePin(oledSpiRst, PinOwner::UM_QuinLEDAnPenta);

      delete oledDisplay;

      oledEnabled = false;
      oledInitDone = false;
    }

    bool isOledReady()
    {
      return oledEnabled && oledInitDone;
    }

    void initSht30TempHumiditySensor()
    {
      PinManagerPinType pins[2] = { { shtSda, true }, { shtScl, true } };
      if (!PinManager::allocateMultiplePins(pins, 2, PinOwner::UM_QuinLEDAnPenta)) {
        DEBUG_PRINTF("[%s] SHT30 pin allocation failed!\n", _name);
        shtEnabled = shtInitDone = false;
        return;
      }

      TwoWire *wire = new TwoWire(1);
      wire->setClock(400000);

      sht30TempHumidSensor = (SHT *) new SHT30();
      sht30TempHumidSensor->begin(shtI2cAddress, wire);
      // The SHT lib calls wire.begin() again without the SDA and SCL pins... So call it again here...
      wire->begin(shtSda, shtScl);
      if (sht30TempHumidSensor->readStatus() == 0xFFFF) {
        DEBUG_PRINTF("[%s] SHT30 init failed!\n", _name);
        shtEnabled = shtInitDone = false;
        return;
      }

      shtInitDone = true;
    }

    void cleanupSht30TempHumiditySensor()
    {
      if (shtInitDone) {
        sht30TempHumidSensor->reset();
      }

      PinManager::deallocatePin(shtSda, PinOwner::UM_QuinLEDAnPenta);
      PinManager::deallocatePin(shtScl, PinOwner::UM_QuinLEDAnPenta);

      delete sht30TempHumidSensor;

      shtEnabled = false;
      shtInitDone = false;
    }

    void cleanup()
    {
      if (isOledReady()) {
        cleanupOledDisplay();
      }

      if (isShtReady()) {
        cleanupSht30TempHumiditySensor();
      }

      enabled = false;
    }

    bool oledCheckForNetworkChanges()
    {
      if (lastKnownNetworkConnected != Network.isConnected() || lastKnownIp != Network.localIP()
          || lastKnownWiFiConnected != WiFi.isConnected() || lastKnownSsid != WiFi.SSID()
          || lastKnownApActive != apActive || lastKnownApSsid != apSSID || lastKnownApPass != apPass || lastKnownApChannel != apChannel) {
        lastKnownNetworkConnected = Network.isConnected();
        lastKnownIp = Network.localIP();
        lastKnownWiFiConnected = WiFi.isConnected();
        lastKnownSsid = WiFi.SSID();
        lastKnownApActive = apActive;
        lastKnownApSsid = apSSID;
        lastKnownApPass = apPass;
        lastKnownApChannel = apChannel;

        return networkHasChanged = true;
      }
      #ifdef WLED_USE_ETHERNET
      if (lastKnownEthType != ethernetType || lastKnownEthLinkUp != ETH.linkUp()) {
        lastKnownEthType = ethernetType;
        lastKnownEthLinkUp = ETH.linkUp();

        return networkHasChanged = true;
      }
      #endif

      return networkHasChanged = false;
    }

    byte oledGetNextPage()
    {
      return oledCurrentPage + 1 <= oledMaxPage ? oledCurrentPage + 1 : 1;
    }

    void oledShowPage(byte page, bool updateLastTimePageChange = false)
    {
      oledCurrentPage = page;
      updateOledDisplay();
      oledLastTimeUpdated = millis();
      if (updateLastTimePageChange) oledLastTimePageChange = oledLastTimeUpdated;
    }

    /*
     * Page 1: Overall brightness and LED outputs
     * Page 2: General info like temp, humidity and others
     * Page 3: Network info
     */
    void updateOledDisplay()
    {
      if (!isOledReady()) return;

      oledDisplay->firstPage();
      do {
        oledDisplay->setFont(u8g2_font_chroma48medium8_8r);
        oledDisplay->drawStr(0, 8, serverDescription);
        oledDisplay->drawHLine(0, 13, 127);
        oledDisplay->setFont(u8g2_font_6x10_tf);

        byte charPerRow = 21;
        byte oledRow = 23;
        switch (oledCurrentPage) {
          // LED Outputs
          case 1:
          {
            char charCurrentBrightness[charPerRow+1] = "Brightness:";
            if (oledUseProgressBars) {
              oledDisplay->drawStr(0, oledRow, charCurrentBrightness);
              // There is no method to draw a filled box with rounded corners. So draw the rounded frame first, then fill that frame accordingly to LED percentage
              oledDisplay->drawRFrame(68, oledRow - 6, 60, 7, 2);
              oledDisplay->drawBox(69, oledRow - 5, int(round(58*getPercentageForBrightness(bri)) / 100), 5);
            }
            else {
              sprintf(charCurrentBrightness, "%s %d%%", charCurrentBrightness, getPercentageForBrightness(bri));
              oledDisplay->drawStr(0, oledRow, charCurrentBrightness);
            }
            oledRow += 8;

            byte drawnLines = 0;
            for (int8_t app = 0; app <= 4; app++) {
              for (int8_t clp = 0; clp <= 4; clp++) {
                if (anPentaLEDPins[app] == currentLedPins[clp]) {
                  char charCurrentLedcReads[17];
                  sprintf(charCurrentLedcReads, "LED %d:", app+1);
                  if (oledUseProgressBars) {
                    oledDisplay->drawStr(0, oledRow+(drawnLines*8), charCurrentLedcReads);
                    oledDisplay->drawRFrame(38, oledRow - 6 + (drawnLines * 8), 90, 7, 2);
                    oledDisplay->drawBox(39, oledRow - 5 + (drawnLines * 8), int(round(88*getPercentageForBrightness(currentLedcReads[clp])) / 100), 5);
                  }
                  else {
                    sprintf(charCurrentLedcReads, "%s %d%%", charCurrentLedcReads, getPercentageForBrightness(currentLedcReads[clp]));
                    oledDisplay->drawStr(0, oledRow+(drawnLines*8), charCurrentLedcReads);
                  }

                  drawnLines++;
                }
              }
            }
            break;
          }

          // Various info
          case 2:
          {
            if (isShtReady() && shtReadDataSuccess) {
              char charShtCurrentTemp[charPerRow+4]; // Reserve 3 more bytes than usual as we gonna have one UTF8 char which can be up to 4 bytes.
              sprintf(charShtCurrentTemp, "Temperature: %.02f°C", shtCurrentTemp);
              char charShtCurrentHumidity[charPerRow+1];
              sprintf(charShtCurrentHumidity, "Humidity: %.02f RH", shtCurrentHumidity);

              oledDisplay->drawUTF8(0, oledRow, charShtCurrentTemp);
              oledDisplay->drawStr(0, oledRow + 10, charShtCurrentHumidity);
              oledRow += 20;
            }

            if (mqttEnabled && mqttServer[0] != 0) {
              char charMqttStatus[charPerRow+1];
              sprintf(charMqttStatus, "MQTT: %s", (WLED_MQTT_CONNECTED ? "Connected" : "Disconnected"));
              oledDisplay->drawStr(0, oledRow, charMqttStatus);
              oledRow += 10;
            }

            // Always draw these two on the bottom
            char charUptime[charPerRow+1];
            sprintf(charUptime, "Uptime: %ds", int(millis()/1000 + rolloverMillis*4294967)); // From json.cpp
            oledDisplay->drawStr(0, 53, charUptime);

            char charWledVersion[charPerRow+1];
            sprintf(charWledVersion, "WLED v%s", versionString);
            oledDisplay->drawStr(0, 63, charWledVersion);
            break;
          }

          // Network Info
          case 3:
            #ifdef WLED_USE_ETHERNET
              if (lastKnownEthType == WLED_ETH_NONE) {
                oledDisplay->drawStr(0, oledRow, "Ethernet: No board selected");
                oledRow += 10;
              }
              else if (!lastKnownEthLinkUp) {
                oledDisplay->drawStr(0, oledRow, "Ethernet: Link Down");
                oledRow += 10;
              }
            #endif

            if (lastKnownNetworkConnected) {
              #ifdef WLED_USE_ETHERNET
                if (lastKnownEthLinkUp) {
                  oledDisplay->drawStr(0, oledRow, "Ethernet: Link Up");
                  oledRow += 10;
                }
                else
              #endif
              // Wi-Fi can be active with ETH being connected, but we don't mind...
              if (lastKnownWiFiConnected) {
                #ifdef WLED_USE_ETHERNET
                  if (!lastKnownEthLinkUp) {
                #endif

                oledDisplay->drawStr(0, oledRow, "Wi-Fi: Connected");
                char currentSsidChar[lastKnownSsid.length() + 1];
                lastKnownSsid.toCharArray(currentSsidChar, lastKnownSsid.length() + 1);
                char charCurrentSsid[50];
                sprintf(charCurrentSsid, "SSID: %s", currentSsidChar);
                oledDisplay->drawStr(0, oledRow + 10, charCurrentSsid);
                oledRow += 20;

                #ifdef WLED_USE_ETHERNET
                  }
                #endif
              }

              String currentIpStr = lastKnownIp.toString();
              char currentIpChar[currentIpStr.length() + 1];
              currentIpStr.toCharArray(currentIpChar, currentIpStr.length() + 1);
              char charCurrentIp[30];
              sprintf(charCurrentIp, "IP: %s", currentIpChar);
              oledDisplay->drawStr(0, oledRow, charCurrentIp);
            }
            // If WLED AP is active. Theoretically, it can even be active with ETH being connected, but we don't mind...
            else if (lastKnownApActive) {
              char charCurrentApStatus[charPerRow+1];
              sprintf(charCurrentApStatus, "WLED AP: %s (Ch: %d)", (lastKnownApActive ? "On" : "Off"), lastKnownApChannel);
              oledDisplay->drawStr(0, oledRow, charCurrentApStatus);

              char charCurrentApSsid[charPerRow+1];
              sprintf(charCurrentApSsid, "SSID: %s", lastKnownApSsid);
              oledDisplay->drawStr(0, oledRow + 10, charCurrentApSsid);

              char charCurrentApPass[charPerRow+1];
              sprintf(charCurrentApPass, "PW: %s", lastKnownApPass);
              oledDisplay->drawStr(0, oledRow + 20, charCurrentApPass);

              // IP is hardcoded / no var exists in WLED at the time this mod was coded, so also hardcode it here
              oledDisplay->drawStr(0, oledRow + 30, "IP: 4.3.2.1");
            }

            break;
        }
      } while (oledDisplay->nextPage());
    }

    bool isShtReady()
    {
      return shtEnabled && shtInitDone;
    }


  public:
    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _oledEnabled[];
    static const char _oledUseProgressBars[];
    static const char _oledFlipScreen[];
    static const char _oledSecondsPerPage[];
    static const char _oledFixBuggedScreen[];
    static const char _shtEnabled[];
    static const unsigned char quinLedLogo[];


    static int8_t getPercentageForBrightness(byte brightness)
    {
      return int(((float)brightness / (float)255) * 100);
    }


    /*
      * setup() is called once at boot. WiFi is not yet connected at this point.
      * You can use it to initialize variables, sensors or similar.
      */
    void setup()
    {
      if (enabled) {
        lastKnownBri = bri;

        if (oledEnabled) {
          initOledDisplay();
        }

        if (shtEnabled) {
          initSht30TempHumiditySensor();
        }

        getCurrentUsedLedPins();

        initDone = true;
      }

      firstRunDone = true;
    }

    /*
      * loop() is called continuously. Here you can check for events, read sensors, etc.
      *
      * Tips:
      * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
      *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
      *
      * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
      *    Instead, use a timer check as shown here.
      */
    void loop()
    {
      if (!enabled || !initDone || strip.isUpdating()) return;

      if (isShtReady()) {
        if (millis() - shtLastTimeUpdated > 30000 && !shtDataRequested) {
          sht30TempHumidSensor->requestData();
          shtDataRequested = true;

          shtLastTimeUpdated = millis();
        }

        if (shtDataRequested) {
          if (sht30TempHumidSensor->dataReady()) {
            if (sht30TempHumidSensor->readData()) {
              shtCurrentTemp = sht30TempHumidSensor->getTemperature();
              shtCurrentHumidity = sht30TempHumidSensor->getHumidity();
              shtReadDataSuccess = true;
            }
            else {
              shtReadDataSuccess = false;
            }

            shtDataRequested = false;
          }
        }
      }

      if (isOledReady() && millis() - oledLogoDrawn > 3000) {
        // Check for changes on the current page and update the OLED if a change is detected
        if (millis() - oledLastTimeUpdated > 150) {
          // If there was a network change, force page 3 (network page)
          if (oledCheckForNetworkChanges()) {
            oledCurrentPage = 3;
          }
          // Only redraw a page if there was a change for that page
          switch (oledCurrentPage) {
            case 1:
              lastKnownBri = bri;
              // Probably causes lag to always do ledcRead(), so rather re-do the math, 'cause we can't easily get it...
              getCurrentLedcValues();

              if (bri != lastKnownBri || lastKnownLedcReads[0] != currentLedcReads[0] || lastKnownLedcReads[1] != currentLedcReads[1] || lastKnownLedcReads[2] != currentLedcReads[2]
                  || lastKnownLedcReads[3] != currentLedcReads[3] || lastKnownLedcReads[4] != currentLedcReads[4]) {
                lastKnownLedcReads[0] = currentLedcReads[0]; lastKnownLedcReads[1] = currentLedcReads[1]; lastKnownLedcReads[2] = currentLedcReads[2]; lastKnownLedcReads[3] = currentLedcReads[3]; lastKnownLedcReads[4] = currentLedcReads[4];

                oledShowPage(1);
              }
              break;

            case 2:
              if (shtLastKnownTemp != shtCurrentTemp || shtLastKnownHumidity != shtCurrentHumidity) {
                shtLastKnownTemp = shtCurrentTemp;
                shtLastKnownHumidity = shtCurrentHumidity;

                oledShowPage(2);
              }
              break;

            case 3:
              if (networkHasChanged) {
                networkHasChanged = false;

                oledShowPage(3, true);
              }
              break;
          }
        }
        // Cycle through OLED pages
        if (millis() - oledLastTimePageChange > oledSecondsPerPage * 1000) {
          // Periodically fixing a "bugged out" OLED. More details in the ReadMe
          if (oledFixBuggedScreen && millis() - oledLastTimeFixBuggedScreen > 60000) {
            oledDisplay->begin();
            oledLastTimeFixBuggedScreen = millis();
          }
          oledShowPage(oledGetNextPage(), true);
        }
      }
    }

    void addToConfig(JsonObject &root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname

      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_oledEnabled)] = oledEnabled;
      top[FPSTR(_oledUseProgressBars)] = oledUseProgressBars;
      top[FPSTR(_oledFlipScreen)] = oledFlipScreen;
      top[FPSTR(_oledSecondsPerPage)] = oledSecondsPerPage;
      top[FPSTR(_oledFixBuggedScreen)] = oledFixBuggedScreen;
      top[FPSTR(_shtEnabled)] = shtEnabled;

      // Update LED pins on config save
      getCurrentUsedLedPins();
    }

    /**
     * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
     *
     * The function should return true if configuration was successfully loaded or false if there was no configuration.
     */
    bool readFromConfig(JsonObject &root)
    {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) {
        DEBUG_PRINTF("[%s] No config found. (Using defaults.)\n", _name);
        return false;
      }

      bool oldEnabled = enabled;
      bool oldOledEnabled = oledEnabled;
      bool oldOledFlipScreen = oledFlipScreen;
      bool oldShtEnabled = shtEnabled;

      getJsonValue(top[FPSTR(_enabled)], enabled);
      getJsonValue(top[FPSTR(_oledEnabled)], oledEnabled);
      getJsonValue(top[FPSTR(_oledUseProgressBars)], oledUseProgressBars);
      getJsonValue(top[FPSTR(_oledFlipScreen)], oledFlipScreen);
      getJsonValue(top[FPSTR(_oledSecondsPerPage)], oledSecondsPerPage);
      getJsonValue(top[FPSTR(_oledFixBuggedScreen)], oledFixBuggedScreen);
      getJsonValue(top[FPSTR(_shtEnabled)], shtEnabled);

      // First run: reading from cfg.json, nothing to do here, will be all done in setup()
      if (!firstRunDone) {
        DEBUG_PRINTF("[%s] First run, nothing to do\n", _name);
      }
      // Check if mod has been en-/disabled
      else if (enabled != oldEnabled) {
        enabled ? setup() : cleanup();
        DEBUG_PRINTF("[%s] Usermod has been en-/disabled\n", _name);
      }
      // Config has been changed, so adopt to changes
      else if (enabled) {
        if (oldOledEnabled != oledEnabled) {
          oledEnabled ? initOledDisplay() : cleanupOledDisplay();
        }
        else if (oledEnabled && oldOledFlipScreen != oledFlipScreen) {
          oledDisplay->clear();
          oledDisplay->setFlipMode(oledFlipScreen);
          oledShowPage(oledCurrentPage);
        }

        if (oldShtEnabled != shtEnabled) {
          shtEnabled ? initSht30TempHumiditySensor() : cleanupSht30TempHumiditySensor();
        }

        DEBUG_PRINTF("[%s] Config (re)loaded\n", _name);
      }

      return true;
    }

    void addToJsonInfo(JsonObject& root)
    {
      if (!enabled && !isShtReady()) {
        return;
      }

      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray jsonTemp = user.createNestedArray("Temperature");
      JsonArray jsonHumidity = user.createNestedArray("Humidity");

      if (shtLastTimeUpdated == 0 || !shtReadDataSuccess) {
        jsonTemp.add(0);
        jsonHumidity.add(0);
        if (shtLastTimeUpdated == 0) {
          jsonTemp.add(" Not read yet");
          jsonHumidity.add(" Not read yet");
        }
        else {
          jsonTemp.add(" Error");
          jsonHumidity.add(" Error");
        }

        return;
      }

      jsonHumidity.add(shtCurrentHumidity);
      jsonHumidity.add(" RH");

      jsonTemp.add(shtCurrentTemp);
      jsonTemp.add(" °C");
    }

    /*
      * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
      * This could be used in the future for the system to determine whether your usermod is installed.
      */
    uint16_t getId()
    {
      return USERMOD_ID_QUINLED_AN_PENTA;
    }
};

// strings to reduce flash memory usage (used more than twice)
// Config settings
const char QuinLEDAnPentaUsermod::_name[]                PROGMEM = "QuinLED-An-Penta";
const char QuinLEDAnPentaUsermod::_enabled[]             PROGMEM = "Enabled";
const char QuinLEDAnPentaUsermod::_oledEnabled[]         PROGMEM = "Enable-OLED";
const char QuinLEDAnPentaUsermod::_oledUseProgressBars[] PROGMEM = "OLED-Use-Progress-Bars";
const char QuinLEDAnPentaUsermod::_oledFlipScreen[]      PROGMEM = "OLED-Flip-Screen-180";
const char QuinLEDAnPentaUsermod::_oledSecondsPerPage[]  PROGMEM = "OLED-Seconds-Per-Page";
const char QuinLEDAnPentaUsermod::_oledFixBuggedScreen[] PROGMEM = "OLED-Fix-Bugged-Screen";
const char QuinLEDAnPentaUsermod::_shtEnabled[]          PROGMEM = "Enable-SHT30-Temp-Humidity-Sensor";
// Other strings

const unsigned char QuinLEDAnPentaUsermod::quinLedLogo[] PROGMEM = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9F, 0xFD, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0x03, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x80, 0xFF,
  0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x3F, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x1F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xF0, 0x07, 0xFE, 0xFF, 0xFF, 0x0F, 0xFC,
  0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xFC, 0x0F, 0xFE,
  0xFF, 0xFF, 0x0F, 0xFC, 0xFF, 0xFF, 0xE3, 0xFF, 0xA5, 0xFF, 0xFF, 0xFF,
  0x0F, 0xFC, 0x1F, 0xFE, 0xFF, 0xFF, 0x1F, 0xFC, 0xFF, 0xFF, 0xE1, 0xFF,
  0x00, 0xF0, 0xE3, 0xFF, 0x0F, 0xFE, 0x1F, 0xFE, 0xFF, 0xFF, 0x3F, 0xFF,
  0xFF, 0xFF, 0xE3, 0xFF, 0x00, 0xF0, 0x00, 0xFF, 0x07, 0xFE, 0x1F, 0xFC,
  0xF9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE1, 0xFF, 0x00, 0xF0, 0x00, 0xFE,
  0x07, 0xFF, 0x1F, 0xFC, 0xF0, 0xC7, 0x3F, 0xFF, 0xFF, 0xFF, 0xE3, 0xFF,
  0xF1, 0xFF, 0x00, 0xFC, 0x07, 0xFF, 0x1F, 0xFE, 0xF0, 0xC3, 0x1F, 0xFE,
  0x00, 0xFF, 0xE1, 0xFF, 0xF1, 0xFF, 0x30, 0xF8, 0x07, 0xFF, 0x1F, 0xFE,
  0xF0, 0xC3, 0x1F, 0xFE, 0x00, 0xFC, 0xC3, 0xFF, 0xE1, 0xFF, 0xF0, 0xF0,
  0x03, 0xFF, 0x0F, 0x7E, 0xF0, 0xC3, 0x1F, 0x7E, 0x00, 0xF8, 0xE3, 0xFF,
  0xE1, 0xFF, 0xF1, 0xF1, 0x83, 0xFF, 0x0F, 0x7E, 0xF0, 0xC3, 0x1F, 0x7E,
  0x00, 0xF0, 0xC3, 0xFF, 0xE1, 0xFF, 0xF1, 0xE1, 0x83, 0xFF, 0x0F, 0xFE,
  0xF0, 0xC3, 0x1F, 0xFE, 0xF8, 0xF0, 0xC3, 0xFF, 0xA1, 0xFF, 0xF1, 0xE3,
  0x81, 0xFF, 0x0F, 0x7E, 0xF0, 0xC1, 0x1F, 0x7E, 0xF0, 0xF0, 0xC3, 0xFF,
  0x01, 0xF8, 0xE1, 0xC3, 0x83, 0xFF, 0x0F, 0x7F, 0xF8, 0xC3, 0x1F, 0x7E,
  0xF8, 0xF0, 0xC3, 0xFF, 0x03, 0xF8, 0xE1, 0xC7, 0x81, 0xE4, 0x0F, 0x7F,
  0xF0, 0xC3, 0x1F, 0xFE, 0xF8, 0xF0, 0xC3, 0xFF, 0x01, 0xF8, 0xE3, 0xC7,
  0x01, 0xC0, 0x07, 0x7F, 0xF8, 0xC1, 0x1F, 0x7E, 0xF0, 0xE1, 0xC3, 0xFF,
  0xC3, 0xFD, 0xE1, 0x87, 0x01, 0x00, 0x07, 0x7F, 0xF8, 0xC3, 0x1F, 0x7E,
  0xF8, 0xF0, 0xC3, 0xFF, 0xE3, 0xFF, 0xE3, 0x87, 0x01, 0x00, 0x82, 0x3F,
  0xF8, 0xE1, 0x1F, 0xFE, 0xF8, 0xE1, 0xC3, 0xFF, 0xC3, 0xFF, 0xC3, 0x87,
  0x01, 0x00, 0x80, 0x3F, 0xF8, 0xC1, 0x1F, 0x7E, 0xF0, 0xF1, 0xC3, 0xFF,
  0xC3, 0xFF, 0xC3, 0x87, 0x03, 0x0F, 0x80, 0x3F, 0xF8, 0xE1, 0x0F, 0x7E,
  0xF8, 0xE1, 0x87, 0xFF, 0xC3, 0xFF, 0xC7, 0x87, 0x03, 0x04, 0xC0, 0x7F,
  0xF0, 0xE1, 0x0F, 0xFF, 0xF8, 0xF1, 0x87, 0xFF, 0xC3, 0xFF, 0xC3, 0x87,
  0x07, 0x00, 0xE0, 0x7F, 0x00, 0xE0, 0x1F, 0x7E, 0xF0, 0xE0, 0xC3, 0xFF,
  0xC7, 0xFF, 0x87, 0x87, 0x0F, 0x00, 0xE0, 0x7F, 0x00, 0xE0, 0x0F, 0x7F,
  0xF8, 0xE1, 0x07, 0x80, 0x07, 0xEA, 0x87, 0xC1, 0x0F, 0x00, 0x80, 0xFF,
  0x00, 0xE0, 0x1F, 0x7E, 0xF0, 0xE1, 0x07, 0x00, 0x03, 0x80, 0x07, 0xC0,
  0x7F, 0x00, 0x00, 0xFF, 0x01, 0xE0, 0x1F, 0xFF, 0xF8, 0xE1, 0x07, 0x00,
  0x07, 0x00, 0x07, 0xE0, 0xFF, 0xF7, 0x01, 0xFF, 0x57, 0xF7, 0x9F, 0xFF,
  0xFC, 0xF1, 0x0F, 0x00, 0x07, 0x80, 0x0F, 0xE0, 0xFF, 0xFF, 0x03, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0xBF, 0xFE,
  0xFF, 0xFF, 0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

static QuinLEDAnPentaUsermod quinled_an_penta;
REGISTER_USERMOD(quinled_an_penta);