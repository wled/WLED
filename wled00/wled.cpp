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
  if (loopDelay > 2) DEBUG_PRINTF_P(PSTR("Loop delayed more than %ums.\n"), loopDelay);
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
    if (WLED_CONNECTED && aOtaEnabled && !otaLock && correctPIN) ArduinoOTA.handle();
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

  //LED settings have been saved, re-init busses
  //This code block causes severe FPS drop on ESP32 with the original "if (busConfigs[0] != nullptr)" conditional. Investigate!
  if (doInitBusses) {
    doInitBusses = false;
    DEBUG_PRINTLN(F("Re-init busses."));
    bool aligned = strip.checkSegmentAlignment(); //see if old segments match old bus(ses)
    BusManager::removeAll();
    unsigned mem = 0;
    // determine if it is sensible to use parallel I2S outputs on ESP32 (i.e. more than 5 outputs = 1 I2S + 4 RMT)
    bool useParallel = false;
    #if defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_ESP32S2) && !defined(ARDUINO_ARCH_ESP32S3) && !defined(ARDUINO_ARCH_ESP32C3)
    unsigned digitalCount = 0;
    unsigned maxLedsOnBus = 0;
    unsigned maxChannels = 0;
    for (unsigned i = 0; i < WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES; i++) {
      if (busConfigs[i] == nullptr) break;
      if (!Bus::isDigital(busConfigs[i]->type)) continue;
      if (!Bus::is2Pin(busConfigs[i]->type)) {
        digitalCount++;
        unsigned channels = Bus::getNumberOfChannels(busConfigs[i]->type);
        if (busConfigs[i]->count > maxLedsOnBus) maxLedsOnBus = busConfigs[i]->count;
        if (channels > maxChannels) maxChannels  = channels;
      }
    }
    DEBUG_PRINTF_P(PSTR("Maximum LEDs on a bus: %u\nDigital buses: %u\n"), maxLedsOnBus, digitalCount);
    // we may remove 300 LEDs per bus limit when NeoPixelBus is updated beyond 2.9.0
    if (maxLedsOnBus <= 300 && digitalCount > 5) {
      DEBUG_PRINTF_P(PSTR("Switching to parallel I2S."));
      useParallel = true;
      BusManager::useParallelOutput();
      mem = BusManager::memUsage(maxChannels, maxLedsOnBus, 8); // use alternate memory calculation (hse to be used *after* useParallelOutput())
    }
    #endif
    // create buses/outputs
    for (unsigned i = 0; i < WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES; i++) {
      if (busConfigs[i] == nullptr || (!useParallel && i > 10)) break;
      if (useParallel && i < 8) {
        // if for some unexplained reason the above pre-calculation was wrong, update
        unsigned memT = BusManager::memUsage(*busConfigs[i]); // includes x8 memory allocation for parallel I2S
        if (memT > mem) mem = memT; // if we have unequal LED count use the largest
      } else
        mem += BusManager::memUsage(*busConfigs[i]); // includes global buffer
      if (mem <= MAX_LED_MEMORY) BusManager::add(*busConfigs[i]);
      delete busConfigs[i];
      busConfigs[i] = nullptr;
    }
    strip.finalizeInit(); // also loads default ledmap if present
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

  if (doReboot && (!doInitBusses || !doSerializeConfig)) // if busses have to be inited & saved, wait until next iteration
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
    DEBUG_PRINTF_P(PSTR("Wifi state: %d (connected %d, channel %d, mode %d) @ %lus -> %lus\n"), (int)Network.isConnected(), lastWifiState, WiFi.channel(), WiFi.getMode(), lastReconnectAttempt/1000, wifiStateChangedTime/1000);
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
    WiFi.mode(WIFI_MODE_STA);     // enable scanning
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
    DEBUG_PRINTLN(F("Ada"));
    Serial.println(F("Ada"));
  }
  #endif

  // fill in unique mdns default
  if (strcmp(cmDNS, "x") == 0) sprintf_P(cmDNS, PSTR("wled-%*s"), 6, escapedMac.c_str() + 6);
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
#if defined(ARDUINO_ARCH_ESP32)
  const uint32_t seed32 = esp_random();
#elif defined(ARDUINO_ARCH_ESP8266)
  const uint32_t seed32 = RANDOM_REG32;
#else
  const uint32_t seed32 = random(std::numeric_limits<long>::max());
#endif
  random16_set_seed((uint16_t)((seed32 & 0xFFFF) ^ (seed32 >> 16)));

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
  DEBUG_PRINTF_P(PSTR("WiFi: Stopping AP. @ %lus\r\n"), millis()/1000);
#ifndef WLED_DISABLE_ESPNOW
  // we need to stop ESP-NOW as we are stopping AP
  if (stopESPNow && statusESPNow == ESP_NOW_STATE_ON) {
    DEBUG_PRINTLN(F("ESP-NOW stopping on AP stop."));
    quickEspNow.stop();
    statusESPNow = ESP_NOW_STATE_UNINIT;
    scanESPNow = millis() + 8000; // postpone searching for a bit
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
  DEBUG_PRINTF_P(PSTR("WiFi: Opening access point %s @ %lus\r\n"), apSSID, millis()/1000);
  WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0)); // will also engage WIFI_MODE_AP
  WiFi.softAP(apSSID, apPass, apChannel, apHide); // WiFi mode can be either WIFI_MODE_AP or WIFI_MODE_APSTA(==WIFI_MODE_STA | WIFI_MODE_AP)
  #ifdef ARDUINO_ARCH_ESP32
  WiFi.setTxPower(wifi_power_t(txPower));
  #endif

  if (!apActive) // start captive portal if AP active
  {
    #ifdef WLED_ENABLE_WEBSOCKETS
    ws.onEvent(wsEvent);
    #endif
    DEBUG_PRINTF_P(PSTR("WiFi: Init AP interfaces @ %lus\r\n"), millis()/1000);
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

#ifndef WLED_DISABLE_ESPNOW
    if (enableESPNow) {
      // we have opened AP (on desired channel) so we need to initialise ESP-NOW on the same channel
      // it is going to be used as for sending AND receiving (assuming slave device has no WiFi configured)
      // if slave has WiFi configured it needs to set TEMPORARY AP to be able to search for master
      quickEspNow.onDataSent(espNowSentCB);     // see udp.cpp
      quickEspNow.onDataRcvd(espNowReceiveCB);  // see udp.cpp
      DEBUG_PRINTLN(F("ESP-NOW initing in AP mode."));
      #ifdef ESP32
      quickEspNow.setWiFiBandwidth(WIFI_IF_AP, WIFI_BW_HT20); // Only needed for ESP32 in case you need coexistence with ESP8266 in the same network
      #endif //ESP32
      bool espNowOK = quickEspNow.begin(apChannel, WIFI_IF_AP);  // Same channel must be used for both AP and ESP-NOW
      statusESPNow = espNowOK ? ESP_NOW_STATE_ON : ESP_NOW_STATE_ERROR;
      channelESPNow = apChannel;
      DEBUG_PRINTF_P(PSTR("ESP-NOW%sinited in AP mode (wifi: %d/%d).\n"), espNowOK ? " " : " NOT ", WiFi.channel(), (int)apChannel);
    }
#endif
  }
  apActive = true;
}

// initConnection() (re)starts connection to configured WiFi/SSIDs
// once the connection is established initInterfaces() is called
void WLED::initConnection()
{
  DEBUG_PRINTF_P(PSTR("initConnection() called @ %lus.\n"), millis()/1000);
  WiFi.disconnect(true); // close old connections (and also disengage STA mode)

  lastReconnectAttempt = millis();

  if (isWiFiConfigured()) {
    DEBUG_PRINTF_P(PSTR("WiFi: Connecting to %s... @ %lus\r\n"), multiWiFi[selectedWiFi].clientSSID, millis()/1000);

    WiFi.mode(WIFI_MODE_STA); // engage explicit STA mode
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
    WiFi.setTxPower(wifi_power_t(txPower));
    WiFi.setSleep(!noWifiSleep);
    WiFi.setHostname(hostname);
#else
    wifi_set_sleep_type((noWifiSleep) ? NONE_SLEEP_T : MODEM_SLEEP_T);
    WiFi.hostname(hostname);
#endif
    WiFi.begin(multiWiFi[selectedWiFi].clientSSID, multiWiFi[selectedWiFi].clientPass); // no harm if called multiple times
    // once WiFi is configured and begin() called, ESP will keep connecting to the specified SSID in the background
    // until connection is established or new configuration is submitted or disconnect() is called
  }
}

// connected() is called when WiFi connection is established
void WLED::connected()
{
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

#ifndef WLED_DISABLE_ESPNOW
  // we are connected to WiFi and ESP-NOW will only be used on master for sending out packets
  // on the same channel as WiFi it is connected to (slaves will need to find it, see below)
  if (enableESPNow) {
    quickEspNow.onDataSent(espNowSentCB);     // see udp.cpp
    quickEspNow.onDataRcvd(espNowReceiveCB);  // see udp.cpp
    DEBUG_PRINTF_P(PSTR("ESP-NOW initing in STA mode, channel %d.\n"), WiFi.channel());
    bool espNowOK = quickEspNow.begin(); // Use no parameters to start ESP-NOW on same channel as WiFi, in STA mode
    statusESPNow = espNowOK ? ESP_NOW_STATE_ON : ESP_NOW_STATE_ERROR;
    scanESPNow = millis() + 5000;
  }
#endif

  interfacesInited = true;
  initInterfaces = false;
}

void WLED::handleConnection()
{
  const unsigned long now = millis();
  const bool wifiConfigured = isWiFiConfigured();

  // ignore connection handling if WiFi is configured and scan still running
  // or within first 2s if WiFi is not configured or AP is always active
  if ((wifiConfigured && multiWiFi.size() > 1 && WiFi.scanComplete() < 0) || (now < 2000 && (!wifiConfigured || apBehavior == AP_BEHAVIOR_ALWAYS)))
    return;

  if (wifiConfigured && (lastReconnectAttempt == 0UL || forceReconnect)) {
    // this is first attempt at connecting to SSID or we were forced to reconnect
    DEBUG_PRINTF_P(PSTR("WiFi: Initial connect or forced reconnect. @ %lus\n"), now/1000);
    selectedWiFi = findWiFi(); // find strongest WiFi
    initConnection(); // start connecting to preferred/configured WiFi
    interfacesInited = false;
    forceReconnect = false;
    return;
  }

  if (!Network.isConnected()) {
    if (!wifiConfigured && !apActive) {
      DEBUG_PRINTF_P(PSTR("WiFi: Not configured, opening AP! @ %lus\r\n"), now/1000);
      initAP(); // instantly go to AP mode (but will not disengage STA mode if engaged!)
      return;
    }
    if (!apActive && apBehavior == AP_BEHAVIOR_ALWAYS) {
      DEBUG_PRINTF_P(PSTR("WiFi: AP ALWAYS enabled. @ %lus\n"), now/1000);
      initAP(); // if STA is engaged (it should be) this will keep AP's channel in sync with STA channel
    }
    //send improv failed 6 seconds after second init attempt (24 sec. after provisioning)
    if (improvActive > 2 && now > lastReconnectAttempt + 6000) {
      sendImprovStateResponse(0x03, true);
      improvActive = 2;
    }
/*
    #ifdef ESP8266
    int staClients = wifi_softap_get_station_num();
    #else
    wifi_sta_list_t stationList;
    esp_wifi_ap_get_sta_list(&stationList);
    int staClients = stationList.num;
    #endif
*/
    // WiFi is configured with multiple networks; try to reconnect if not connected after 15s or 300s if clients connected to AP
    // this will cycle through all configured SSIDs (findWiFi() sorted SSIDs by signal strength)
    // ESP usually connects to WiFi within 10s but we should give it a bit of time before attempting another network
    // when a disconnect happens (see onEvent()) the WiFi scan is reinitiated and forced reconnect scheduled
    if (wifiConfigured && multiWiFi.size() > 1 && now > lastReconnectAttempt + ((apActive) ? WLED_AP_TIMEOUT : 15000)) {
      // this code is executed if ESP was unsuccessful in connecting to prefered WiFi. it is repeated every 15s
      // to select different WiFi from the list (if only one WiFi is configured there will be 2 min between calls) if connects
      // are not successful. if AP is still active when AP timeout is reached, stop AP
      if ((!apActive || apClients == 0)
        #ifndef WLED_DISABLE_ESPNOW
        // wait for 3 skipped heartbeats if ESP-NOW sync is enabled
        && now > 12000 + heartbeatESPNow
        #endif
        )
      {
        if (improvActive == 2) improvActive = 3;
        DEBUG_PRINTF_P(PSTR("WiFi: Last reconnect (%lus) too old (@ %lus).\n"), lastReconnectAttempt/1000, now/1000);
        if (++selectedWiFi >= multiWiFi.size()) selectedWiFi = 0; // we couldn't connect, try with another network from the list
        stopAP(true); // stop ESP-NOW and disengage AP mode
        initConnection();
        // postpone searching for 2 min after last SSID from list and rely on auto reconnect
        if (selectedWiFi + 1U == multiWiFi.size()) lastReconnectAttempt += 120000;
        return;
      }
    }
    // open AP if this is 12s after boot connect attempt or 12s after any disconnect (_NO_CONN)
    // i.e. initial connect was unsuccessful (12s is enough to connect to SSID if it exists)
    // !wasConnected means this is after boot and we haven't yet successfully connected to SSID
    if (!apActive && now > lastReconnectAttempt + 12000 && (!wasConnected || apBehavior == AP_BEHAVIOR_NO_CONN)) {
      if (!(apBehavior == AP_BEHAVIOR_TEMPORARY && now > WLED_AP_TIMEOUT)) { // start temporary AP only within first 5min
        DEBUG_PRINTF_P(PSTR("WiFi: Opening not connected AP. @ %lus\r\n"), now/1000);
        WiFi.disconnect(true); // disengage STA mode/prevent connecting to WiFi while AP is open (may be reset above)
        WiFi.mode(WIFI_MODE_AP);
        initAP();
        return;
      }
    }
    // disconnect AP after 5min if no clients connected and mode TEMPORARY
    if (apActive && apBehavior == AP_BEHAVIOR_TEMPORARY && now > WLED_AP_TIMEOUT) {
      // if AP was enabled more than 10min after boot or if client was connected more than 10min after boot do not disconnect AP mode
      if (apClients == 0 && now < 2*WLED_AP_TIMEOUT) {
        DEBUG_PRINTF_P(PSTR("WiFi: Temporary AP disabled. @ %lus\n"), now/1000);
        stopAP(true);
        WiFi.mode(WIFI_MODE_STA);
        return;
      }
    }
#ifndef WLED_DISABLE_ESPNOW
    // we are still not connected to WiFi and we may have AP active or not. if AP is active we will listen on its channel for ESP-NOW packets
    // otherwise we'll try to find our master by hopping channels (master is detected in ESP-NOW receive callback)
    bool isMasterDefined = masterESPNow[0] | masterESPNow[1] | masterESPNow[2] | masterESPNow[3] | masterESPNow[4] | masterESPNow[5];
    if (!apActive && apBehavior == AP_BEHAVIOR_TEMPORARY && enableESPNow && !sendNotificationsRT && isMasterDefined && now > WLED_AP_TIMEOUT) {
      // wait for 15s after starting to scan for WiFi before starting ESP-NOW (give ESP a chance to connect)
      if (statusESPNow == ESP_NOW_STATE_UNINIT && wifiConfigured && now > lastReconnectAttempt + 15000) {
        quickEspNow.onDataSent(espNowSentCB);     // see udp.cpp
        quickEspNow.onDataRcvd(espNowReceiveCB);  // see udp.cpp
        DEBUG_PRINTF_P(PSTR("ESP-NOW initing in unconnected (no)AP mode (channel %d).\n"), (int)channelESPNow);
        WiFi.disconnect();        // stop looking for WiFi
        // this will open AP named ESP_xxxxxxxx but nothing else. when switching channels you'll be unable to connect to it.
        // when you manage to connect to it, no services will be available
        WiFi.mode(WIFI_MODE_AP);  // force AP mode to fix channel
        // if we try to force AP disconnect, ESP-NOW will cease to function (unable to change channel)
        //WiFi.softAPdisconnect();
        WiFi.softAP(apSSID, apPass, channelESPNow, true); // we will not set apActive = true
        bool espNowOK = quickEspNow.begin(channelESPNow, WIFI_IF_AP); // use fixed channel AP mode
        statusESPNow = espNowOK ? ESP_NOW_STATE_ON : ESP_NOW_STATE_ERROR;
        scanESPNow = now;         // prevent immediate change of channel
        heartbeatESPNow = 0UL;
      }
      if (statusESPNow == ESP_NOW_STATE_ON && wifiConfigured) {
        if (now > 4000 + scanESPNow) {
          // change channel every 4s (after 30s of last heartbeat) and wait 120s after roll-over
          scanESPNow = now;
          if (++channelESPNow > 13) {
            channelESPNow = 1;
            scanESPNow += 120000;
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
        } else if (WiFi.channel() != channelESPNow && WiFi.getMode() == WIFI_MODE_AP) quickEspNow.setChannel(channelESPNow); // sometimes channel will chnage, force it back
      }
    }
#endif
  } else if (initInterfaces) { //newly connected
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

#ifndef WLED_DISABLE_ESPNOW
  // send ESP-NOW beacon every 2s if we are in sync mode (AKA master device) regardless of STA or AP mode
  // beacon will contain current/intended channel and local time (for loose synchronisation purposes)
  if (useESPNowSync && statusESPNow == ESP_NOW_STATE_ON && sendNotificationsRT && now > 2000 + scanESPNow) {
    EspNowBeacon buffer = {{'W','L','E','D'}, 0, (uint8_t)WiFi.channel(), toki.second(), {0}};
    quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));
    scanESPNow = now;
    DEBUG_PRINTF_P(PSTR("ESP-NOW beacon on channel %d.\n"), WiFi.channel());
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

  if (WLED_CONNECTED) {
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
