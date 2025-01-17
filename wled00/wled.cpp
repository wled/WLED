#define WLED_DEFINE_GLOBAL_VARS //only in one source file, wled.cpp!
#include "wled.h"
#include "wled_ethernet.h"
#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_DISABLE_BROWNOUT_DET)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

extern "C" void usePWMFixedNMI();

/*
 * Main WLED class implementation. Mostly initialization and connection logic
 */

WLED::WLED()
{
}

// turns all LEDs off and restarts ESP
void WLED::reset()
{
  briT = 0;
  #ifdef WLED_ENABLE_WEBSOCKETS
  ws.closeAll(1012);
  #endif
  unsigned long dly = millis();
  while (millis() - dly < 450) {
    yield();        // enough time to send response to client
  }
  applyBri();
  DEBUG_PRINTLN(F("WLED RESET"));
  ESP.restart();
}

void WLED::loop()
{
  static uint32_t      lastHeap = UINT32_MAX;
  static unsigned long heapTime = 0;
#ifdef WLED_DEBUG
  static unsigned long lastRun = 0;
  unsigned long        loopMillis = millis();
  size_t               loopDelay = loopMillis - lastRun;
  if (lastRun == 0) loopDelay=0; // startup - don't have valid data from last run.
  if (loopDelay > 4) DEBUG_PRINTF_P(PSTR("Loop delayed more than %ums.\n"), loopDelay);
  static unsigned long maxLoopMillis = 0;
  static size_t        avgLoopMillis = 0;
  static unsigned long maxUsermodMillis = 0;
  static size_t        avgUsermodMillis = 0;
  static unsigned long maxStripMillis = 0;
  static size_t        avgStripMillis = 0;
  unsigned long        stripMillis;
#endif

  handleTime();
  #ifndef WLED_DISABLE_INFRARED
  handleIR();        // 2nd call to function needed for ESP32 to return valid results -- should be good for ESP8266, too
  #endif
  handleConnection();
  #ifdef WLED_ENABLE_ADALIGHT
  handleSerial();
  #endif
  handleImprovWifiScan();
  handleNotifications();
  handleTransitions();
  #ifdef WLED_ENABLE_DMX
  handleDMX();
  #endif

  #ifdef WLED_DEBUG
  unsigned long usermodMillis = millis();
  #endif
  userLoop();
  UsermodManager::loop();
  #ifdef WLED_DEBUG
  usermodMillis = millis() - usermodMillis;
  avgUsermodMillis += usermodMillis;
  if (usermodMillis > maxUsermodMillis) maxUsermodMillis = usermodMillis;
  #endif

  yield();
  handleIO();
  #ifndef WLED_DISABLE_INFRARED
  handleIR();
  #endif
  #ifndef WLED_DISABLE_ESPNOW
  handleRemote();
  #endif
  #ifndef WLED_DISABLE_ALEXA
  handleAlexa();
  #endif

  if (doCloseFile) {
    closeFile();
    yield();
  }

  #ifdef WLED_DEBUG
  stripMillis = millis();
  #endif
  if (!realtimeMode || realtimeOverride || (realtimeMode && useMainSegmentOnly))  // block stuff if WARLS/Adalight is enabled
  {
    if (apActive) dnsServer.processNextRequest();
    #ifndef WLED_DISABLE_OTA
    if (Network.isConnected() && aOtaEnabled && !otaLock && correctPIN) ArduinoOTA.handle();
    #endif
    handleNightlight();
    yield();

    #ifndef WLED_DISABLE_HUESYNC
    handleHue();
    yield();
    #endif

    if (!presetNeedsSaving()) {
      handlePlaylist();
      yield();
    }
    handlePresets();
    yield();

    if (!offMode || strip.isOffRefreshRequired() || strip.needsUpdate())
      strip.service();
    #ifdef ESP8266
    else if (!noWifiSleep)
      delay(1); //required to make sure ESP enters modem sleep (see #1184)
    #endif
  }
  #ifdef WLED_DEBUG
  stripMillis = millis() - stripMillis;
  avgStripMillis += stripMillis;
  if (stripMillis > maxStripMillis) maxStripMillis = stripMillis;
  #endif

  yield();
#ifdef ESP8266
  MDNS.update();
#endif

  //millis() rolls over every 50 days
  if (lastMqttReconnectAttempt > millis()) {
    rolloverMillis++;
    lastMqttReconnectAttempt = 0;
    ntpLastSyncTime = NTP_NEVER;  // force new NTP query
    strip.restartRuntime();
  }
  if (millis() - lastMqttReconnectAttempt > 30000 || lastMqttReconnectAttempt == 0) { // lastMqttReconnectAttempt==0 forces immediate broadcast
    lastMqttReconnectAttempt = millis();
    #ifndef WLED_DISABLE_MQTT
    initMqtt();
    #endif
    yield();
    // refresh WLED nodes list
    refreshNodeList();
    if (nodeBroadcastEnabled) sendSysInfoUDP();
    yield();
  }

  // 15min PIN time-out
  if (strlen(settingsPIN)>0 && correctPIN && millis() - lastEditTime > PIN_TIMEOUT) {
    correctPIN = false;
    createEditHandler(false);
  }

  // reconnect WiFi to clear stale allocations if heap gets too low
  if (millis() - heapTime > 15000) {
    uint32_t heap = ESP.getFreeHeap();
    if (heap < MIN_HEAP_SIZE && lastHeap < MIN_HEAP_SIZE) {
      DEBUG_PRINTF_P(PSTR("Heap too low! %u\n"), heap);
      forceReconnect = true;
      strip.resetSegments(); // remove all but one segments from memory
    } else if (heap < MIN_HEAP_SIZE) {
      DEBUG_PRINTLN(F("Heap low, purging segments."));
      strip.purgeSegments();
    }
    lastHeap = heap;
    heapTime = millis();
  }

  //LED settings need to be saved, re-init busses
  //This code block causes severe FPS drop on ESP32 with the original "if (busConfigs[0] != nullptr)" conditional. Investigate!
  if (doInit & INIT_2D) {
    doInit &= ~INIT_2D;
    strip.setUpMatrix(); // will check limits
    strip.makeAutoSegments(true);
    strip.deserializeMap();
  }
  if (doInit & INIT_BUS) {
    doInit &= ~INIT_BUS;
    DEBUG_PRINTLN(F("Re-init busses."));
    bool aligned = strip.checkSegmentAlignment(); //see if old segments match old bus(ses)
    BusManager::removeAll();
    strip.finalizeInit(); // will create buses and also load default ledmap if present
    BusManager::setBrightness(bri); // fix re-initialised bus' brightness #4005
    if (aligned) strip.makeAutoSegments();
    else strip.fixInvalidSegments();
    doSerializeConfig = true;
  }
  if (loadLedmap >= 0) {
    strip.deserializeMap(loadLedmap);
    loadLedmap = -1;
  }
  yield();
  if (doSerializeConfig) serializeConfig();

  yield();
  handleWs();
#if defined(STATUSLED)
  handleStatusLED();
#endif

  toki.resetTick();

#if WLED_WATCHDOG_TIMEOUT > 0
  // we finished our mainloop, reset the watchdog timer
  static unsigned long lastWDTFeed = 0;
  if (!strip.isUpdating() || millis() - lastWDTFeed > (WLED_WATCHDOG_TIMEOUT*500)) {
  #ifdef ARDUINO_ARCH_ESP32
    esp_task_wdt_reset();
  #else
    ESP.wdtFeed();
  #endif
    lastWDTFeed = millis();
  }
#endif

  if (doReboot && (!doInit || !doSerializeConfig)) // if busses have to be inited & saved, wait until next iteration
    reset();

// DEBUG serial logging (every 30s)
#ifdef WLED_DEBUG
  unsigned long now = millis();
  loopMillis = now - loopMillis;
  if (loopMillis > 30) {
    DEBUG_PRINTF_P(PSTR(" Loop took %lums.\n"),     loopMillis);
    DEBUG_PRINTF_P(PSTR(" Usermods took %lums.\n"), usermodMillis);
    DEBUG_PRINTF_P(PSTR(" Strip took %lums.\n"),    stripMillis);
  }
  avgLoopMillis += loopMillis;
  if (loopMillis > maxLoopMillis) maxLoopMillis = loopMillis;
  if (WiFi.status() != lastWifiState) wifiStateChangedTime = now;
  lastWifiState = WiFi.status();
  // WL_IDLE_STATUS      = 0
  // WL_NO_SSID_AVAIL    = 1
  // WL_SCAN_COMPLETED   = 2
  // WL_CONNECTED        = 3
  // WL_CONNECT_FAILED   = 4
  // WL_CONNECTION_LOST  = 5
  // WL_DISCONNECTED     = 6
  if (now - debugTime > 29999) {
    DEBUG_PRINTLN(F("---DEBUG INFO---"));
    DEBUG_PRINTF_P(PSTR("Runtime: %lus\n"),  now/1000);
    DEBUG_PRINTF_P(PSTR("Unix time: %u,%03u\n"), toki.getTime().sec, toki.getTime().ms);
    DEBUG_PRINTF_P(PSTR("Free heap: %u\n"), ESP.getFreeHeap());
    #if defined(ARDUINO_ARCH_ESP32)
    if (psramFound()) {
      DEBUG_PRINTF_P(PSTR("PSRAM: %dkB/%dkB\n"), ESP.getFreePsram()/1024, ESP.getPsramSize()/1024);
      if (!psramSafe) DEBUG_PRINTLN(F("Not using PSRAM."));
    }
    DEBUG_PRINTF_P(PSTR("TX power: %d/%d\n"), WiFi.getTxPower(), txPower);
    #endif
    DEBUG_PRINTF_P(PSTR("Wifi state: %d (connected %d (%d), channel %d, BSSID %s, mode %d, scan %d) @ %lus -> %lus\n"), lastWifiState, (int)apClients, (int)Network.isConnected(), WiFi.channel(), WiFi.BSSIDstr().c_str(), (int)WiFi.getMode(), (int)WiFi.scanComplete(), lastReconnectAttempt/1000, wifiStateChangedTime/1000);
    #ifndef WLED_DISABLE_ESPNOW
    DEBUG_PRINTF_P(PSTR("ESP-NOW state: %u\n"),        statusESPNow);
    #endif
    DEBUG_PRINTF_P(PSTR("NTP last sync: %lus\n"),      ntpLastSyncTime/1000);
    DEBUG_PRINTF_P(PSTR("Client IP: %u.%u.%u.%u\n"),   Network.localIP()[0], Network.localIP()[1], Network.localIP()[2], Network.localIP()[3]);
    if (loops > 0) { // avoid division by zero
      DEBUG_PRINTF_P(PSTR("Loops/sec: %u\n"),          loops / 30);
      DEBUG_PRINTF_P(PSTR(" Loop time[ms]: %u/%lu\n"), avgLoopMillis/loops,    maxLoopMillis);
      DEBUG_PRINTF_P(PSTR(" UM time[ms]: %u/%lu\n"),   avgUsermodMillis/loops, maxUsermodMillis);
      DEBUG_PRINTF_P(PSTR(" Strip time[ms]:%u/%lu\n"), avgStripMillis/loops,   maxStripMillis);
    }
    #ifdef WLED_DEBUG_FX
    strip.printSize();
    #endif
    loops = 0;
    maxLoopMillis = 0;
    maxUsermodMillis = 0;
    maxStripMillis = 0;
    avgLoopMillis = 0;
    avgUsermodMillis = 0;
    avgStripMillis = 0;
    debugTime = now;
  }
  loops++;
  lastRun = now;
#endif        // WLED_DEBUG
}

#if WLED_WATCHDOG_TIMEOUT > 0
void WLED::enableWatchdog() {
  #ifdef ARDUINO_ARCH_ESP32
  esp_err_t watchdog = esp_task_wdt_init(WLED_WATCHDOG_TIMEOUT, true);
  DEBUG_PRINT(F("Watchdog enabled: "));
  if (watchdog == ESP_OK) {
    DEBUG_PRINTLN(F("OK"));
  } else {
    DEBUG_PRINTLN(watchdog);
    return;
  }
  esp_task_wdt_add(NULL);
  #else
  ESP.wdtEnable(WLED_WATCHDOG_TIMEOUT * 1000);
  #endif
}

void WLED::disableWatchdog() {
  DEBUG_PRINTLN(F("Watchdog: disabled"));
  #ifdef ARDUINO_ARCH_ESP32
  esp_task_wdt_delete(NULL);
  #else
  ESP.wdtDisable();
  #endif
}
#endif

void WLED::setup()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_DISABLE_BROWNOUT_DET)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detection
#endif

#ifdef ARDUINO_ARCH_ESP32
  pinMode(hardwareRX, INPUT_PULLDOWN); delay(1);        // suppress noise in case RX pin is floating (at low noise energy) - see issue #3128
#endif
#ifdef WLED_BOOTUPDELAY
  delay(WLED_BOOTUPDELAY); // delay to let voltage stabilize, helps with boot issues on some setups
#endif
  Serial.begin(115200);
#if !ARDUINO_USB_CDC_ON_BOOT
  Serial.setTimeout(50);  // this causes troubles on new MCUs that have a "virtual" USB Serial (HWCDC)
#endif
#if (defined(WLED_DEBUG) || defined(WLED_DEBUG_FX) || defined(WLED_DEBUG_FS) || defined(WLED_DEBUG_BUS) || defined(WLED_DEBUG_PINMANAGER) || defined(WLED_DEBUG_USERMODS)) && defined(ARDUINO_ARCH_ESP32) && (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || ARDUINO_USB_CDC_ON_BOOT)
  delay(2500);  // allow CDC USB serial to initialise
#endif
#if !(defined(WLED_DEBUG) || defined(WLED_DEBUG_FX) || defined(WLED_DEBUG_FS) || defined(WLED_DEBUG_BUS) || defined(WLED_DEBUG_PINMANAGER) || defined(WLED_DEBUG_USERMODS)) && defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DEBUG_HOST) && ARDUINO_USB_CDC_ON_BOOT
  Serial.setDebugOutput(false); // switch off kernel messages when using USBCDC
#endif
  DEBUG_PRINTLN();
  DEBUG_PRINTF_P(PSTR("---WLED %s %u INIT---\n"), versionString, build);
  DEBUG_PRINTLN();
#ifdef ARDUINO_ARCH_ESP32
  DEBUG_PRINTF_P(PSTR("esp32 %s\n"), ESP.getSdkVersion());
  #if defined(ESP_ARDUINO_VERSION)
    DEBUG_PRINTF_P(PSTR("arduino-esp32 v%d.%d.%d\n"), int(ESP_ARDUINO_VERSION_MAJOR), int(ESP_ARDUINO_VERSION_MINOR), int(ESP_ARDUINO_VERSION_PATCH));  // available since v2.0.0
  #else
    DEBUG_PRINTLN(F("arduino-esp32 v1.0.x\n"));  // we can't say in more detail.
  #endif

  DEBUG_PRINTF_P(PSTR("CPU:   %s rev.%d, %d core(s), %d MHz.\n"), ESP.getChipModel(), (int)ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  DEBUG_PRINTF_P(PSTR("FLASH: %d MB, Mode %d "), (ESP.getFlashChipSize()/1024)/1024, (int)ESP.getFlashChipMode());
  #ifdef WLED_DEBUG
  switch (ESP.getFlashChipMode()) {
    // missing: Octal modes
    case FM_QIO:  DEBUG_PRINT(F("(QIO)")); break;
    case FM_QOUT: DEBUG_PRINT(F("(QOUT)"));break;
    case FM_DIO:  DEBUG_PRINT(F("(DIO)")); break;
    case FM_DOUT: DEBUG_PRINT(F("(DOUT)"));break;
    #if defined(CONFIG_IDF_TARGET_ESP32S3) && CONFIG_ESPTOOLPY_FLASHMODE_OPI
    case FM_FAST_READ: DEBUG_PRINT(F("(OPI)")); break;
    #else
    case FM_FAST_READ: DEBUG_PRINT(F("(fast_read)")); break;
    #endif
    case FM_SLOW_READ: DEBUG_PRINT(F("(slow_read)")); break;
    default: break;
  }
  #endif
  DEBUG_PRINTF_P(PSTR(", speed %u MHz.\n"), ESP.getFlashChipSpeed()/1000000);

#else
  DEBUG_PRINTF_P(PSTR("esp8266 @ %u MHz.\nCore: %s\n"), ESP.getCpuFreqMHz(), ESP.getCoreVersion());
  DEBUG_PRINTF_P(PSTR("FLASH: %u MB\n"), (ESP.getFlashChipSize()/1024)/1024);
#endif
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());

#if defined(ARDUINO_ARCH_ESP32)
  // BOARD_HAS_PSRAM also means that a compiler flag "-mfix-esp32-psram-cache-issue" was used and so PSRAM is safe to use on rev.1 ESP32
  #if !defined(BOARD_HAS_PSRAM) && !(defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3))
  if (psramFound() && ESP.getChipRevision() < 3) psramSafe = false;
  if (!psramSafe) DEBUG_PRINTLN(F("Not using PSRAM."));
  #endif
  pDoc = new PSRAMDynamicJsonDocument((psramSafe && psramFound() ? 2 : 1)*JSON_BUFFER_SIZE);
  DEBUG_PRINTF_P(PSTR("JSON buffer allocated: %u\n"), (psramSafe && psramFound() ? 2 : 1)*JSON_BUFFER_SIZE);
  // if the above fails requestJsonBufferLock() will always return false preventing crashes
  if (psramFound()) {
    DEBUG_PRINTF_P(PSTR("PSRAM: %dkB/%dkB\n"), ESP.getFreePsram()/1024, ESP.getPsramSize()/1024);
  }
#endif

#ifdef ESP8266
  usePWMFixedNMI(); // link the NMI fix
#endif

#if (defined(WLED_DEBUG) || defined(WLED_DEBUG_FX) || defined(WLED_DEBUG_FS) || defined(WLED_DEBUG_BUS) || defined(WLED_DEBUG_PINMANAGER) || defined(WLED_DEBUG_USERMODS)) && !defined(WLED_DEBUG_HOST)
  PinManager::allocatePin(hardwareTX, true, PinOwner::DebugOut); // TX (GPIO1 on ESP32) reserved for debug output
#endif
#ifdef WLED_ENABLE_DMX //reserve GPIO2 as hardcoded DMX pin
  PinManager::allocatePin(2, true, PinOwner::DMX);
#endif

  DEBUG_PRINTLN(F("Registering usermods ..."));
  registerUsermods();

  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());

  bool fsinit = false;
  DEBUGFS_PRINTLN(F("Mount FS"));
#ifdef ARDUINO_ARCH_ESP32
  fsinit = WLED_FS.begin(true);
#else
  fsinit = WLED_FS.begin();
#endif
  if (!fsinit) {
    DEBUGFS_PRINTLN(F("FS failed!"));
    errorFlag = ERR_FS_BEGIN;
  }
#ifdef WLED_ADD_EEPROM_SUPPORT
  else deEEP();
#else
  initPresetsFile();
#endif
  updateFSInfo();

  // generate module IDs must be done before AP setup
  escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();

  WLED_SET_AP_SSID(); // otherwise it is empty on first boot until config is saved
  multiWiFi.push_back(WiFiConfig(CLIENT_SSID,CLIENT_PASS)); // initialise vector with default WiFi

  DEBUG_PRINTLN(F("Reading config"));
  deserializeConfigFromFS();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());

#if defined(STATUSLED) && STATUSLED>=0
  if (!PinManager::isPinAllocated(STATUSLED)) {
    // NOTE: Special case: The status LED should *NOT* be allocated.
    //       See comments in handleStatusLed().
    pinMode(STATUSLED, OUTPUT);
  }
#endif

  DEBUG_PRINTLN(F("Initializing strip"));
  beginStrip();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());

  DEBUG_PRINTLN(F("Usermods setup"));
  userSetup();
  UsermodManager::setup();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());

  DEBUG_PRINTLN(F("Initializing WiFi"));
  WiFi.persistent(false);
  //WiFi.enableLongRange(true);
  WiFi.onEvent(WiFiEvent);
#if defined(ARDUINO_ARCH_ESP32) && ESP_IDF_VERSION_MAJOR==4
  WiFi.useStaticBuffers(true);    // use preallocated buffers (for speed)
#endif
#ifdef ESP8266
  WiFi.setPhyMode(force802_3g ? WIFI_PHY_MODE_11G : WIFI_PHY_MODE_11N);
#endif
  if (isWiFiConfigured()) {
    showWelcomePage = false;
    WiFi.setAutoReconnect(true);  // use automatic reconnect functionality
    WiFi.mode(WIFI_MODE_STA);     // enable SSID scanning
    findWiFi(true);               // start scanning for available WiFi-s
  } else {
    showWelcomePage = true;
    WiFi.mode(WIFI_MODE_AP);      // WiFi is not configured so we'll most likely open an AP
  }

  // all GPIOs are allocated at this point
  serialCanRX = !PinManager::isPinAllocated(hardwareRX); // Serial RX pin (GPIO 3 on ESP32 and ESP8266)
  serialCanTX = !PinManager::isPinAllocated(hardwareTX) || PinManager::getPinOwner(hardwareTX) == PinOwner::DebugOut; // Serial TX pin (GPIO 1 on ESP32 and ESP8266)

  #ifdef WLED_ENABLE_ADALIGHT
  //Serial RX (Adalight, Improv, Serial JSON) only possible if GPIO3 unused
  //Serial TX (Debug, Improv, Serial JSON) only possible if GPIO1 unused
  if (serialCanRX && serialCanTX) {
    Serial.println(F("Ada"));
  }
  #endif

  // fill in unique mdns default
  if (strcmp(cmDNS, DEFAULT_MDNS_NAME) == 0) sprintf_P(cmDNS, PSTR("wled-%*s"), 6, escapedMac.c_str() + 6);
#ifndef WLED_DISABLE_MQTT
  if (mqttDeviceTopic[0] == 0) sprintf_P(mqttDeviceTopic, PSTR("wled/%*s"), 6, escapedMac.c_str() + 6);
  if (mqttClientID[0] == 0)    sprintf_P(mqttClientID, PSTR("WLED-%*s"), 6, escapedMac.c_str() + 6);
#endif

#ifndef WLED_DISABLE_OTA
  if (aOtaEnabled) {
    ArduinoOTA.onStart([]() {
      #ifdef ESP8266
      wifi_set_sleep_type(NONE_SLEEP_T);
      #endif
      #if WLED_WATCHDOG_TIMEOUT > 0
      WLED::instance().disableWatchdog();
      #endif
      DEBUG_PRINTLN(F("Start ArduinoOTA"));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      #if WLED_WATCHDOG_TIMEOUT > 0
      // reenable watchdog on failed update
      WLED::instance().enableWatchdog();
      #endif
    });
    if (strlen(cmDNS) > 0)
      ArduinoOTA.setHostname(cmDNS);
  }
#endif
#ifdef WLED_ENABLE_DMX
  initDMX();
#endif

#ifdef WLED_ENABLE_ADALIGHT
  if (serialCanRX && Serial.available() > 0 && Serial.peek() == 'I') handleImprovPacket();
#endif

  // HTTP server page init
  DEBUG_PRINTLN(F("initServer"));
  initServer();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());

#ifndef WLED_DISABLE_INFRARED
  // init IR
  DEBUG_PRINTLN(F("initIR"));
  initIR();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());
#endif

  // Seed FastLED random functions with an esp random value, which already works properly at this point.
  const uint32_t seed32 = hw_random();
  random16_set_seed((uint16_t)seed32);

  #if WLED_WATCHDOG_TIMEOUT > 0
  enableWatchdog();
  #endif

  #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_DISABLE_BROWNOUT_DET)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //enable brownout detector
  #endif
}

void WLED::beginStrip()
{
  // Initialize NeoPixel Strip and button
  strip.finalizeInit(); // busses created during deserializeConfig() if config existed
  strip.makeAutoSegments();
  strip.setBrightness(0);
  strip.setShowCallback(handleOverlayDraw);
  doInit = 0;

  if (turnOnAtBoot) {
    if (briS > 0) bri = briS;
    else if (bri == 0) bri = 128;
  } else {
    // fix for #3196
    if (bootPreset > 0) {
      // set all segments black (no transition)
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment &seg = strip.getSegment(i);
        if (seg.isActive()) seg.colors[0] = BLACK;
      }
      col[0] = col[1] = col[2] = col[3] = 0;  // needed for colorUpdated()
    }
    briLast = briS; bri = 0;
    strip.fill(BLACK);
    strip.show();
  }
  colorUpdated(CALL_MODE_INIT); // will not send notification but will initiate transition
  if (bootPreset > 0) {
    applyPreset(bootPreset, CALL_MODE_INIT);
  }

  // init relay pin
  if (rlyPin >= 0) {
    pinMode(rlyPin, rlyOpenDrain ? OUTPUT_OPEN_DRAIN : OUTPUT);
    digitalWrite(rlyPin, (rlyMde ? bri : !bri));
  }
}

// stop AP (optionally also stop ESP-NOW)
void WLED::stopAP(bool stopESPNow) {
  DEBUG_PRINTF_P(PSTR("WiFi: Stopping AP. @ %lus\n"), millis()/1000);
#ifndef WLED_DISABLE_ESPNOW
  // we need to stop ESP-NOW as we are stopping AP
  if (stopESPNow && statusESPNow == ESP_NOW_STATE_ON) {
    DEBUG_PRINTLN(F("ESP-NOW stopping on AP stop."));
    quickEspNow.stop();
    statusESPNow = ESP_NOW_STATE_UNINIT;
  }
#endif
  dnsServer.stop();
  WiFi.softAPdisconnect(true); // disengage AP mode on stop
  apActive = false;
}

void WLED::initAP(bool resetAP)
{
  if (apBehavior == AP_BEHAVIOR_BUTTON_ONLY && !resetAP)
    return;

#ifndef WLED_DISABLE_ESPNOW
  if (statusESPNow == ESP_NOW_STATE_ON) {
    DEBUG_PRINTLN(F("ESP-NOW stopping on AP start."));
    quickEspNow.stop();
    statusESPNow = ESP_NOW_STATE_UNINIT;
  }
#endif

  if (resetAP) {
    WLED_SET_AP_SSID();
    strcpy_P(apPass, PSTR(WLED_AP_PASS));
  }
  DEBUG_PRINTF_P(PSTR("WiFi: Opening access point %s @ %lus\n"), apSSID, millis()/1000);
  WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0)); // will also engage WIFI_MODE_AP
  bool success = WiFi.softAP(apSSID, apPass, apChannel, apHide); // WiFi mode can be either WIFI_MODE_AP or WIFI_MODE_APSTA(==WIFI_MODE_STA | WIFI_MODE_AP)

  if (!apActive && success) // start captive portal if AP active
  {
    #ifdef WLED_ENABLE_WEBSOCKETS
    ws.onEvent(wsEvent);
    #endif
    DEBUG_PRINTF_P(PSTR("WiFi: Init AP interfaces @ %lus\n"), millis()/1000);
    server.begin();
    if (udpPort > 0 && udpPort != ntpLocalPort) {
      udpConnected = notifierUdp.begin(udpPort);
    }
    if (udpRgbPort > 0 && udpRgbPort != ntpLocalPort && udpRgbPort != udpPort) {
      udpRgbConnected = rgbUdp.begin(udpRgbPort);
    }
    if (udpPort2 > 0 && udpPort2 != ntpLocalPort && udpPort2 != udpPort && udpPort2 != udpRgbPort) {
      udp2Connected = notifier2Udp.begin(udpPort2);
    }
    e131.begin(false, e131Port, e131Universe, E131_MAX_UNIVERSE_COUNT);
    ddp.begin(false, DDP_DEFAULT_PORT);

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", WiFi.softAPIP());

    #ifdef ARDUINO_ARCH_ESP32
    WiFi.setTxPower(wifi_power_t(txPower));
    #endif

    initESPNow();
  }
  apActive = success;
  DEBUG_PRINTF_P(PSTR("WiFi: AP (%s) %s opened.\n"), apSSID, success ? "" : "NOT");
}

// initConnection() (re)starts connection to configured WiFi/SSIDs
// once the connection is established connected() is called
void WLED::initConnection()
{
  DEBUG_PRINTF_P(PSTR("initConnection() called @ %lus.\n"), millis()/1000);
  WiFi.disconnect(); // close old connections

  lastReconnectAttempt = millis();

  if (isWiFiConfigured()) {
    DEBUG_PRINTF_P(PSTR("WiFi: Connecting to %s... @ %lus\n"), multiWiFi[selectedWiFi].clientSSID, millis()/1000);

    // determine if using DHCP or static IP address, will also engage STA mode if not already
    if (multiWiFi[selectedWiFi].staticIP != 0U && multiWiFi[selectedWiFi].staticGW != 0U) {
      WiFi.config(multiWiFi[selectedWiFi].staticIP, multiWiFi[selectedWiFi].staticGW, multiWiFi[selectedWiFi].staticSN, dnsAddress);
    } else {
      WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0));
    }

    // convert the "serverDescription" into a valid DNS hostname (alphanumeric)
    char hostname[25];
    prepareHostname(hostname);

#ifdef ARDUINO_ARCH_ESP32
    WiFi.setSleep(!noWifiSleep);
    WiFi.setHostname(hostname);
    WiFi.setTxPower(wifi_power_t(txPower));
#else
    wifi_set_sleep_type((noWifiSleep) ? NONE_SLEEP_T : MODEM_SLEEP_T);
    WiFi.hostname(hostname);
#endif
    unsigned i = 0;
    while (i < sizeof(multiWiFi[selectedWiFi].bssid)) if (multiWiFi[selectedWiFi].bssid[i++]) break;
    const uint8_t *bssid = i < sizeof(multiWiFi[selectedWiFi].bssid) ? multiWiFi[selectedWiFi].bssid : nullptr;
    // WiFi mode can be either WIFI_MODE_STA or WIFI_MODE_APSTA(==WIFI_MODE_STA | WIFI_MODE_AP)
    WiFi.begin(multiWiFi[selectedWiFi].clientSSID, multiWiFi[selectedWiFi].clientPass, 0, bssid); // no harm if called multiple times
    // once WiFi is configured and begin() called, ESP will keep connecting to the specified SSID in the background
    // until connection is established or new configuration is submitted or disconnect() is called
  }
}

// connected() is called when WiFi connection is established
void WLED::connected()
{
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());
  DEBUG_PRINTLN(F("Init STA interfaces"));

#ifdef WLED_ENABLE_WEBSOCKETS
  ws.onEvent(wsEvent);
#endif

#ifndef WLED_DISABLE_ESPNOW
  if (statusESPNow == ESP_NOW_STATE_ON) {
    DEBUG_PRINTLN(F("ESP-NOW stopping on connect."));
    quickEspNow.stop();
    statusESPNow = ESP_NOW_STATE_UNINIT;
  }
#endif

#ifndef WLED_DISABLE_HUESYNC
  IPAddress ipAddress = Network.localIP();
  if (hueIP[0] == 0) {
    hueIP[0] = ipAddress[0];
    hueIP[1] = ipAddress[1];
    hueIP[2] = ipAddress[2];
  }
#endif

#ifndef WLED_DISABLE_ALEXA
  // init Alexa hue emulation
  if (alexaEnabled)
    alexaInit();
#endif

#ifndef WLED_DISABLE_OTA
  if (aOtaEnabled)
    ArduinoOTA.begin();
#endif

  // Set up mDNS responder:
  if (strlen(cmDNS) > 0) {
    // "end" must be called before "begin" is called a 2nd time
    // see https://github.com/esp8266/Arduino/issues/7213
    MDNS.end();
    MDNS.begin(cmDNS);

    DEBUG_PRINTLN(F("mDNS started"));
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("wled", "tcp", 80);
    MDNS.addServiceTxt("wled", "tcp", "mac", escapedMac.c_str());
  }
  server.begin();

  if (udpPort > 0 && udpPort != ntpLocalPort) {
    udpConnected = notifierUdp.begin(udpPort);
    if (udpConnected && udpRgbPort != udpPort)
      udpRgbConnected = rgbUdp.begin(udpRgbPort);
    if (udpConnected && udpPort2 != udpPort && udpPort2 != udpRgbPort)
      udp2Connected = notifier2Udp.begin(udpPort2);
  }
  if (ntpEnabled)
    ntpConnected = ntpUdp.begin(ntpLocalPort);

  e131.begin(e131Multicast, e131Port, e131Universe, E131_MAX_UNIVERSE_COUNT);
  ddp.begin(false, DDP_DEFAULT_PORT);

#ifndef WLED_DISABLE_HUESYNC
  reconnectHue();
#endif
#ifndef WLED_DISABLE_MQTT
  initMqtt();
#endif
  lastMqttReconnectAttempt = 0; // force immediate update (MQTT & node broadcast)

  initESPNow(!WiFi.isConnected());  // if we are connected using Ethernet force hidden AP mode

  interfacesInited = true;
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());
}

void WLED::handleConnection()
{
  const unsigned long now = millis();
  // if we are connected and interfaces are initialised do nothing
  if (Network.isConnected() && !forceReconnect) {
    if (interfacesInited) sendESPNowHeartBeat();
    else {
      // newly connected
      if (improvActive) {
        if (improvError == 3) sendImprovStateResponse(0x00, true);
        sendImprovStateResponse(0x04);
        if (improvActive > 1) sendImprovIPRPCResult(ImprovRPCType::Command_Wifi);
      }
      connected();
      userConnected();
      UsermodManager::connected();
      // shut down AP
      if (apBehavior != AP_BEHAVIOR_ALWAYS && apActive) {
        stopAP(false); // do not stop ESP-NOW
        DEBUG_PRINTF_P(PSTR("WiFi: AP disabled (connected). @ %lus\n"), now/1000);
      }
    }
    return;
  }

  // at this point we are not connected to WiFi or Ethernet or we are forced to reconnect
  // we will try to connect periodically if WiFi is configured and we will also
  // scan for ESP-NOW master if it is configured and we are in WIFI_MODE_AP mode
  // if WIFI_MODE_STA or WIFI_MODE_APSTA is active we will not atempt any ESP-NOW activity

  const bool wifiConfigured = isWiFiConfigured();
  const int  wifiState = WiFi.status();

  // WL_NO_SHIELD means WiFi is turned off while WL_IDLE_STATUS means we are not trying to connect to SSID (but we may be in AP mode)
  // so need to occasionally check if we can reconnect to restart WiFi
  if (wifiState == WL_NO_SHIELD || (wifiState == WL_IDLE_STATUS && lastReconnectAttempt > 0 && !apClients && !apActive)) {
    // if we haven't heard master & 5 minutes have passes since last reconect
    if (now > WLED_AP_TIMEOUT/2 + heartbeatESPNow && now > WLED_AP_TIMEOUT + lastReconnectAttempt) { // 2.5/5min timeout
      DEBUG_PRINTF_P(PSTR("WiFi: Not initialised %d (%d) @ %lus\n"), (int)wifiState, (int)WiFi.getMode(), now/1000);
      if (wifiConfigured) {
        WiFi.mode(WIFI_MODE_STA);
        if (multiWiFi.size() > 1 || WiFi.scanComplete() == -2) findWiFi(true);
        //lastReconnectAttempt = now + 6000;
      } else if (wifiState == WL_NO_SHIELD) {
        // restart WiF in hidden AP mode
        WiFi.mode(WIFI_MODE_AP);
        WiFi.softAP(apSSID, apPass, apChannel, true);
      }
      return;
    }
  }

  if (wifiConfigured && (forceReconnect || lastReconnectAttempt == 0)) {
    // this is first attempt at connecting to SSID or we were forced to reconnect
    selectedWiFi = findWiFi(); // find strongest WiFi
    if (selectedWiFi == WIFI_SCAN_FAILED) {
      // fallback if scan returned error
      findWiFi(true);
      selectedWiFi = 0;
    } else if (selectedWiFi >= 0) {
      DEBUG_PRINTF_P(PSTR("WiFi: Initial connect or forced reconnect. @ %lus\n"), now/1000);
      initConnection(); // start connecting to preferred/configured WiFi
      forceReconnect = false;
      interfacesInited = false;
#ifndef WLED_DISABLE_ESPNOW
      // if we are slave in ESP-NOW sync we need to postpone active scan for master until temporary AP is closed
      // otherwise just delay it until we connect to WiFi (will be overriden on connect)
      scanESPNow = now + 30000; // postpone ESP-NOW scanning/broadcasting
#endif
    }
    return;
  }

  // ignore connection handling if WiFi is configured and scan still running
  // or within first 2s if WiFi is not configured or AP is always active
  if ((wifiConfigured && multiWiFi.size() > 1 && WiFi.scanComplete() < 0 && (WiFi.getMode() & WIFI_MODE_STA))
    || (now < 2000 && (!wifiConfigured || apBehavior == AP_BEHAVIOR_ALWAYS))) {
    return;
  }

  // send improv failed 6 seconds after second init attempt (24 sec. after provisioning)
  if (improvActive > 2 && now > lastReconnectAttempt + 6000) {
    sendImprovStateResponse(0x03, true);
    improvActive = 2;
  }

  if (!apActive) {
    // WiFi is not configured and soft AP is not yet open
    if (!wifiConfigured && apBehavior != AP_BEHAVIOR_BUTTON_ONLY) {
      DEBUG_PRINTF_P(PSTR("WiFi: Not configured, opening AP! @ %lus\n"), now/1000);
      initAP(); // instantly go to AP mode (WIFI_MODE_AP selected in setup())
      return;
    } else if (apBehavior == AP_BEHAVIOR_ALWAYS || (now > lastReconnectAttempt + 12000 && (!wasConnected || apBehavior == AP_BEHAVIOR_NO_CONN))) {
      // open AP if requested to be permanently open or this is 12s after boot connect attempt or any disconnect (_NO_CONN)
      // i.e. initial connect was unsuccessful (12s is enough to connect to SSID if it exists)
      // !wasConnected means this is after boot and we haven't yet successfully connected to SSID
      DEBUG_PRINTF_P(PSTR("WiFi: Opening AP in STA mode (%d) @ %lus.\n"), (int)(apBehavior == AP_BEHAVIOR_ALWAYS), now/1000);
      WiFi.mode(WIFI_MODE_APSTA);
      initAP();
      return;
    }
  } else {
    //#ifdef ESP8266
    //int apClients = wifi_softap_get_station_num();
    //#else
    //wifi_sta_list_t stationList;
    //esp_wifi_ap_get_sta_list(&stationList);
    //int apClients = stationList.num;
    //#endif
    // disconnect TEMPORARY AP after 5min if no clients are connected
    // if client was connected longer than 10min after boot do not disconnect AP mode
    if (wifiConfigured && apBehavior == AP_BEHAVIOR_TEMPORARY && apClients == 0 && now > WLED_AP_TIMEOUT && now < 2*WLED_AP_TIMEOUT) {
#ifndef WLED_DISABLE_ESPNOW
      if (enableESPNow && useESPNowSync && !sendNotificationsRT) {
        DEBUG_PRINTF_P(PSTR("WiFi: Temporary AP hidden for ESP-NOW @ %lus.\n"), now/1000);
        initESPNow(true);         // enter hidden AP mode (fixed channel)
        scanESPNow = now + 6000;  // "immediately" start scanning for master
      } else
#endif
      {
        DEBUG_PRINTF_P(PSTR("WiFi: Temporary AP disabled @ %lus.\n"), now/1000);
        stopAP(true);             // stop ESP-NOW in AP mode as well
        WiFi.mode(WIFI_MODE_STA);
        initConnection();         // will also start ESP-NOW when connected (if enabled)
      }
      wasConnected = true;        // hack to prevent reopeneing AP
      return;
    }
  }

  const bool isSTAmode = WiFi.getMode() & WIFI_MODE_STA;
  const bool isAPmode  = WiFi.getMode() & WIFI_MODE_AP;
  const bool isESPNowMasterDefined = masterESPNow[0] | masterESPNow[1] | masterESPNow[2] | masterESPNow[3] | masterESPNow[4] | masterESPNow[5];

#ifndef WLED_DISABLE_ESPNOW
  // if we are syncing via ESP-NOW and master has not been heard in a while we shoud retry WiFi
  if (useESPNowSync && !sendNotificationsRT && now > lastReconnectAttempt + 300000 && heartbeatESPNow > 0 && now > heartbeatESPNow + 120000) {
    DEBUG_PRINTF_P(PSTR("WiFi: Timeout conn: %lus, HB: %lus @ %lus.\n"), lastReconnectAttempt/1000, heartbeatESPNow/1000, now/1000);
    if (!isSTAmode) {
      quickEspNow.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_MODE_STA);
    }
    findWiFi(true);
    forceReconnect = true;
    return;
  }
#endif

  // we need to send heartbeat if we are in AP/APSTA mode
  if (isAPmode) sendESPNowHeartBeat();

  // WiFi is configured (with multiple networks); try to reconnect if not connected after 12s (or 300s if clients are connected to AP)
  // this will cycle through all configured SSIDs (findWiFi() will select strongest)
  // ESP usually connects to WiFi within 10s but we should give it a bit of time before attempting another network
  // when a disconnect happens (see onEvent()) the WiFi scan is reinitiated and forced reconnect scheduled
  // if we are ESP-NOW sync slave, connectiong to WiFi must not happen automatically as it will disrupt ESP-NOW channel
  // but we need a way to occasionally reconnect.
  if (isSTAmode && wifiConfigured && (now > lastReconnectAttempt + (apActive ? WLED_AP_TIMEOUT : 12000) && apClients == 0)) {
    // this code is executed if ESP was unsuccessful in connecting to selected SSID. it is repeated every 12s
    // to select different SSID from the list if connects are not successful.
    if (improvActive == 2) improvActive = 3;
#ifndef WLED_DISABLE_ESPNOW
    // we are slave in ESP-NOW sync and we were not able to connect to best SSID (initial connect/forced reconnect)
    // in such case we will not attempt to traverse all configured SSIDs (as findWiFi() does that) but switch to AP mode
    // immediately so ESP-NOW heartbeat scan can commence
    if (enableESPNow && useESPNowSync && !sendNotificationsRT) {
      DEBUG_PRINTF_P(PSTR("WiFi: Last reconnect (%lus) too old, entering ESP-NOW scan. @ %lus.\n"), lastReconnectAttempt/1000, now/1000);
      initESPNow(true);
      scanESPNow = now + 6000;
      lastReconnectAttempt = now;
      return;
    }
#endif
    DEBUG_PRINTF_P(PSTR("WiFi: Last reconnect (%lus) too old @ %lus.\n"), lastReconnectAttempt/1000, now/1000);
    if (++selectedWiFi >= multiWiFi.size()) selectedWiFi = 0; // we couldn't connect, try with another network from the list
    initConnection();                                         // start connecting to selected SSID
    if (selectedWiFi > 0 && selectedWiFi == findWiFi()) lastReconnectAttempt += 120000; // if we selected best SSID then postpone connecting for 2 min (wrapped around/single)
    return;
  }

#ifndef WLED_DISABLE_ESPNOW
  // ESP-NOW sync is only relevant if device is not connected to WiFi and it may have AP active or not.
  // if AP is active we will try to find our master by switching AP channels (master is detected in ESP-NOW receive callback)
  // for this to work we must not be in STA mode as otherwise WiFi driver will switch channels on its own while searching for configured SSID
  //
  // so we may have a conflict if we do not want AP (AP_BEHAVIOR_TEMPORARY) and connect to SSID but we also don't want to miss master's heartbeat
  // by unintentionally switching channels
  //
  // the solution is to try to find master while we are in temporary AP mode by switching channels after having enough time to find master on each channel
  // the situation is even more complicated if device initially connected to SSID but subesquently lost connection,
  // in such case AP will not be engaged and we have no clue where master is so we need to disconnect() from SSID do a search
  // for master and if search is unsuccessful reconnect(). the additional complication arises when multiple SSIDs are configured.
  // when they are, we need to findWiFi() every now and then
  //
  // possible situations:
  // - wifi not configured but ESP-NOW syncing enabled
  //   - must be in (temporary?) AP mode (and when timer expires remain in hidden AP mode)
  // - wifi is configured with single SSID
  //   - unconnected at boot
  //     - start temporary AP and listen for master (switching channels every few s)
  //     - if master is found postpone retrying WiFi connection for at least 2 minutes (after last heartbeat)
  //     - if master is not found while in AP mode stop participating in ESP-NOW sync and retry connecting to WiFi
  //   - connected at boot
  //     - when disconnect happens temporarily enter AP mode and stop STA mode
  //     - start searching for master (about 1 minute for 13 channeld) and wait 1 minute between searches
  //     - while waiting for new search try to reconnect
  // - wifi configured with multiple SSIDs
  //   - similar situation as single SSID but additionally perform findWiFi(true) to try to find best SSID
  //
  if (enableESPNow && useESPNowSync && !sendNotificationsRT && isESPNowMasterDefined && apBehavior == AP_BEHAVIOR_TEMPORARY) {
    // WiFi is configured but we are not connected (AP may be open or not). if ESP-NOW is not active,
    // activate it in AP mode and stop connecting to WiFi (lastReconnectAttempt will retry connecting after a while, see above)
    if (now > scanESPNow && (!isAPmode || statusESPNow != ESP_NOW_STATE_ON)) {
      initESPNow(true);
      scanESPNow = now + 6000;  // prevent immediate change of channel
      heartbeatESPNow = 0;
      return;
    }
    if (statusESPNow == ESP_NOW_STATE_ON && isAPmode && !isSTAmode) {
      if (now > scanESPNow) { // are we due for next scan? (will be postponed on each heartbeat)
        // change channel every 4s and wait 60s after roll-over
        // this will take about 52s to scan all channels
        scanESPNow = now + 4000;
        if (++channelESPNow > 13) {
          channelESPNow = 1;
          scanESPNow += 56000;
        }
        if (!quickEspNow.setChannel(channelESPNow)) DEBUG_PRINTLN(F("ESP-NOW Unable to set channel."));
        else {
          DEBUG_PRINTF_P(PSTR("ESP-NOW channel %d set (wifi: %d).\n"), (int)channelESPNow, WiFi.channel());
          // update AP channel to match ESP-NOW channel
          #ifdef ESP8266
          struct softap_config conf;
          wifi_softap_get_config(&conf);
          conf.channel = channelESPNow;
          wifi_softap_set_config_current(&conf);
          #else
          wifi_config_t conf;
          esp_wifi_get_config(WIFI_IF_AP, &conf);
          conf.ap.channel = channelESPNow;
          esp_wifi_set_config(WIFI_IF_AP, &conf);
          #endif
        }
      } else if (WiFi.channel() != channelESPNow) quickEspNow.setChannel(channelESPNow); // sometimes channel will chnage, force it back
    }
  }
#endif
}

// If status LED pin is allocated for other uses, does nothing
// else blink at 1Hz when WLED_CONNECTED is false (no WiFi, ?? no Ethernet ??)
// else blink at 2Hz when MQTT is enabled but not connected
// else turn the status LED off
#if defined(STATUSLED)
void WLED::handleStatusLED()
{
  uint32_t c = 0;

  #if STATUSLED>=0
  if (PinManager::isPinAllocated(STATUSLED)) {
    return; //lower priority if something else uses the same pin
  }
  #endif

  if (Network.isConnected()) {
    c = RGBW32(0,255,0,0);
    ledStatusType = 2;
  } else if (WLED_MQTT_CONNECTED) {
    c = RGBW32(0,128,0,0);
    ledStatusType = 4;
  } else if (apActive) {
    c = RGBW32(0,0,255,0);
    ledStatusType = 1;
  }
  if (ledStatusType) {
    if (millis() - ledStatusLastMillis >= (1000/ledStatusType)) {
      ledStatusLastMillis = millis();
      ledStatusState = !ledStatusState;
      #if STATUSLED>=0
      digitalWrite(STATUSLED, ledStatusState);
      #else
      BusManager::setStatusPixel(ledStatusState ? c : 0);
      #endif
    }
  } else {
    #if STATUSLED>=0
      #ifdef STATUSLEDINVERTED
      digitalWrite(STATUSLED, HIGH);
      #else
      digitalWrite(STATUSLED, LOW);
      #endif
    #else
      BusManager::setStatusPixel(0);
    #endif
  }
}
#endif
