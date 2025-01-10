#define WLED_DEFINE_GLOBAL_VARS //only in one source file, wled.cpp!
#include "wled.h"
#include "wled_ethernet.h"
#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP32
#include "esp_ota_ops.h"
#endif
#warning WLED-MM is licensed under the EUPL-1.2. By installing WLED MM you implicitly accept the terms!

#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_DISABLE_BROWNOUT_DET)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

#if defined(WLED_DEBUG) && defined(ARDUINO_ARCH_ESP32)
#include "../tools/ESP32-Chip_info.hpp"
#endif


// WLEDMM some buildenv sanity checks

#ifdef ARDUINO_ARCH_ESP32 // ESP32
  #if !defined(ESP32)
    #error please fix your build environment. ESP32 is not defined.
  #endif
  #if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
    #error please fix your build environment. ESP32 and ESP8266 are both defined.
  #endif
  // only one of ARDUINO_ARCH_ESP32S2, ARDUINO_ARCH_ESP32S3, ARDUINO_ARCH_ESP32C3 allowed
  #if defined(ARDUINO_ARCH_ESP32S3) && ( defined(ARDUINO_ARCH_ESP32S2) || defined(ARDUINO_ARCH_ESP32C3) )
    #error please fix your build environment. only one of ARDUINO_ARCH_ESP32S3, ARDUINO_ARCH_ESP32S2, ARDUINO_ARCH_ESP32C3 may be defined
  #endif
  #if defined(ARDUINO_ARCH_ESP32S2) && ( defined(ARDUINO_ARCH_ESP32S3) || defined(ARDUINO_ARCH_ESP32C3) )
    #error please fix your build environment. only one of ARDUINO_ARCH_ESP32S3, ARDUINO_ARCH_ESP32S2, ARDUINO_ARCH_ESP32C3 may be defined
  #endif
  #if defined(CONFIG_IDF_TARGET_ESP32) && ( defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3))
    #error please fix your build environment. only one CONFIG_IDF_TARGET may be defined
  #endif
  // make sure we have a supported CONFIG_IDF_TARGET_
  #if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    #error please fix your build environment. No supported CONFIG_IDF_TARGET was defined
  #endif
  #if CONFIG_IDF_TARGET_ESP32_SOLO || CONFIG_IDF_TARGET_ESP32SOLO
    #warning ESP32 SOLO (single core) is not supported.
  #endif
  // only one of CONFIG_IDF_TARGET_ESP32, CONFIG_IDF_TARGET_ESP32S2, CONFIG_IDF_TARGET_ESP32S3, CONFIG_IDF_TARGET_ESP32C3 is allowed
  #if defined(CONFIG_IDF_TARGET_ESP32) && ( defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3))
    #error please fix your build environment. only one CONFIG_IDF_TARGET may be defined
  #endif
  #if defined(CONFIG_IDF_TARGET_ESP32S3) && ( defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3))
    #error please fix your build environment. only one CONFIG_IDF_TARGET may be defined
  #endif
  #if defined(CONFIG_IDF_TARGET_ESP32C3) && ( defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2))
    #error please fix your build environment. only one CONFIG_IDF_TARGET may be defined
  #endif

#else // 8266
  #if !defined(ARDUINO_ARCH_ESP8266) && !defined(ARDUINO_ARCH_ESP8265)
    #error please fix your build environment. Neither ARDUINO_ARCH_ESP8266 nor ARDUINO_ARCH_ESP32 are defined
  #else
    #if !defined(ESP8266) && !defined(ESP8265)
      #error please fix your build environment. ESP8266 is not defined.
    #endif
  #endif
#endif
// WLEDMM end


#if INCLUDE_xTaskGetHandle && defined(ARDUINO_ARCH_ESP32) && (defined(WLED_DEBUG) || defined(WLED_DEBUG_HEAP))
// WLEDMM stack debug tool - find async_tcp task, and queries it's free stack
static int wledmm_get_tcp_stacksize(void) {
  static TaskHandle_t tcp_taskHandle = NULL;                   // to store the task handle for later calls
  char * tcp_taskname = pcTaskGetTaskName(tcp_taskHandle);     // ask for name of the known task (to make sure we are still looking at the right one)

  if ((tcp_taskHandle == NULL) || (tcp_taskname == NULL) || (strncmp(tcp_taskname, "async_tcp", 9) != 0)) {
    tcp_taskHandle = xTaskGetHandle("async_tcp");              // need to look for the task by name. FreeRTOS docs say this is very slow, so we store the result for next time
    //DEBUG_PRINT(F("async_tcp task ")); DEBUG_PRINTLN( (tcp_taskHandle != NULL) ? F("found") : F("not found"));
  }

  if (tcp_taskHandle != NULL) return uxTaskGetStackHighWaterMark(tcp_taskHandle); // got it !!
  else return -1;
}
#endif

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
  long dly = millis();
  while (millis() - dly < 450) {
    yield();        // enough time to send response to client
  }
  applyBri();
  USER_PRINTLN(F("\nWLED RESTART\n"));
  USER_FLUSH();   // WLEDMM: wait until Serial has completed sending buffered data
  ESP.restart();
}

#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_FASTPATH)
#define yield() {}  // WLEDMM yield() is completely unnecessary on esp32. See https://github.com/espressif/arduino-esp32/issues/1385
#endif

void WLED::loop()
{
  #ifdef WLED_DEBUG
  static unsigned long maxUsermodMillis = 0;
  static uint16_t avgUsermodMillis = 0;
  static unsigned long maxStripMillis = 0;
  static uint16_t avgStripMillis = 0;
  #endif

  handleTime();
  #ifndef WLED_DISABLE_INFRARED
  handleIR();        // 2nd call to function needed for ESP32 to return valid results -- should be good for ESP8266, too
  #endif
  handleConnection();
  #ifndef WLED_DISABLE_ESPNOW
  handleRemote();
  #endif
  handleSerial();
  handleImprovWifiScan();

  #if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)  // WLEDMM experimental: handleNotifications() calls strip.show(); handleTransitions modifies segments
  if (!suspendStripService) {
  #endif
    handleNotifications();
    handleTransitions();
  #if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)  // WLEDMM end 
  }
  #endif

#ifdef WLED_ENABLE_DMX
  handleDMXOutput();
#endif
#ifdef WLED_ENABLE_DMX_INPUT
  dmxInput.update();
#endif
  userLoop();

  #ifdef WLED_DEBUG
  unsigned long usermodMillis = millis();
  #endif
  usermods.loop();
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

  yield();

  // https://github.com/Makuna/NeoPixelBus/wiki/ESP32-and-RTOS-Tasks
  // On ESP32, when the CPU is loaded, asynchronous WiFi libraries (like ESPAsyncWebServer or async-mqtt-client) may interfere with interrupts used to control the LEDs (I2S mode is less affected by this), 
  // which causes flickering of LEDs.
  #if defined(ARDUINO_ARCH_ESP32) && (defined(WLEDMM_FASTPATH) || defined(WLEDMM_PROTECT_SERVICE))  // WLEDMM experimental: avoid strip flickering
  #define FILEWRITE_MAX_WAIT_MS 30  // max time for waiting - aligned with 33 fps
  //if (doReboot || doSerializeConfig || doCloseFile || loadLedmap || presetsActionPending()) {     // WLEDMM trx this to also wait before reading from files
  if (doReboot || doSerializeConfig || doCloseFile || presetsSavePending()) {                       // WLEDMM wait until strip gets idle before writing to files
    unsigned long waitStripStart = millis();
    while (strip.isUpdating() && (millis() - waitStripStart < FILEWRITE_MAX_WAIT_MS)) {delay(3);}
  }
  #endif

  if (doSerializeConfig)
  {   
    serializeConfig();
  } 
    

  if (doReboot && !doInitBusses) // if busses have to be inited & saved, wait until next iteration
    reset();

  if (doCloseFile) {
    closeFile();
    yield();
  }

  if (!realtimeMode || realtimeOverride || (realtimeMode && useMainSegmentOnly))  // block stuff if WARLS/Adalight is enabled
  {
    if (apActive) dnsServer.processNextRequest();
    #ifndef WLED_DISABLE_OTA
    if (WLED_CONNECTED && aOtaEnabled && !otaLock && correctPIN) ArduinoOTA.handle();
    #endif
    handleNightlight();
    handlePlaylist();
    yield();

    #ifndef WLED_DISABLE_HUESYNC
    handleHue();
    yield();
    #endif

    handlePresets();
    yield();

#if defined(_MoonModules_WLED_) && defined(WLEDMM_FASTPATH)
    #ifdef WLED_DEBUG
    unsigned long usermod2Millis = millis();
    #endif
    usermods.loop2();
    #ifdef WLED_DEBUG
    usermod2Millis = millis() - usermod2Millis;
    avgUsermodMillis += usermod2Millis;
    if (usermod2Millis > maxUsermodMillis) maxUsermodMillis = usermod2Millis;
    #endif
    yield();
#endif

    #ifdef WLED_DEBUG
    unsigned long stripMillis = millis();
    #endif
    if (!offMode || strip.isOffRefreshRequired()) {
#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)  // WLEDMM experimental 
      static unsigned long lastTimeService = 0; // WLEDMM needed to remove stale lock
      if (!suspendStripService && !doInitBusses && !loadLedmap) { // WLEDMM prevent effect drawing while strip or segments are being updated
#endif
        strip.service();
#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)
        lastTimeService = millis();
      } else {
        if (suspendStripService && (millis() - lastTimeService > 1500)) { // WLEDMM remove stale lock after 1.5 seconds
          USER_PRINTLN("--> looptask: stale suspendStripService lock removed after 1500 ms."); // should not happen - check for missing "suspendStripService = false"
          suspendStripService = false;
        }
      }
#endif
    }
    #ifdef ESP8266
    else if (!noWifiSleep)
      delay(1); //required to make sure ESP enters modem sleep (see #1184)
    #endif
    #ifdef WLED_DEBUG
    stripMillis = millis() - stripMillis;
    #ifndef WLED_DEBUG_HEAP  // WLEDMM heap debug messages take some time - this warning is popping in too often
    if (stripMillis > 50) DEBUG_PRINTLN("Slow strip.");
    #endif
    avgStripMillis += stripMillis;
    if (stripMillis > maxStripMillis) maxStripMillis = stripMillis;
    #endif
  }

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
  if (strlen(settingsPIN)>0 && millis() - lastEditTime > 900000) {
    correctPIN = false;
    createEditHandler(false);
  }

  //LED settings have been saved, re-init busses
  //This code block causes severe FPS drop on ESP32 with the original "if (busConfigs[0] != nullptr)" conditional. Investigate!
  if (doInitBusses) {
    unsigned long waitStart = millis();                                             // WLEDMM: to avoid crash,
    while (strip.isUpdating() && (millis() - waitStart < 250)) {yield(); delay(5);} // wait a bit until busses are idle (max 250ms)
    doInitBusses = false;
    DEBUG_PRINTLN(F("Re-init busses."));
    bool aligned = strip.checkSegmentAlignment(); //see if old segments match old bus(ses)
    busses.removeAll();
    uint32_t mem = 0;
    for (uint8_t i = 0; i < WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES; i++) {
      if (busConfigs[i] == nullptr) break;
      mem += BusManager::memUsage(*busConfigs[i]);
      if (mem <= MAX_LED_MEMORY) {
        busses.add(*busConfigs[i]);
      }
      delete busConfigs[i]; busConfigs[i] = nullptr;
    }
    strip.finalizeInit();
    busses.setBrightness(bri); // fix re-initialised bus' brightness #4005
    loadLedmap = true;
    if (aligned) strip.makeAutoSegments();
    else strip.fixInvalidSegments();
    yield();
    serializeConfig();
  }

  //WLEDMM refactored (to be done: setUpMatrix is called in finalizeInit and also in deserializeMap, deserializeMap is called in finalizeInit and also here)
  if (loadLedmap) {
    if (!strip.deserializeMap(loadedLedmap) && strip.isMatrix) strip.setUpMatrix(); //WLEDMM: always if nonexistent:  && loadedLedmap == 0
    strip.enumerateLedmaps(); //WLEDMM
    loadLedmap = false;
  }

  yield();
  #if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)  // WLEDMM experimental: pause handleWs while strip/segment data might be inconsistent
  if (!suspendStripService)
  #endif
  handleWs();
  handleStatusLED();

// DEBUG serial logging (every 30s)
#if defined(WLED_DEBUG) && !defined(WLED_DEBUG_HEAP)
  if (millis() - debugTime > 29999) {
    DEBUG_PRINTLN(F("---DEBUG INFO---"));
    DEBUG_PRINT(F("Name: "));       DEBUG_PRINTLN(serverDescription);
    DEBUG_PRINT(F("Runtime: "));       DEBUG_PRINTLN(millis());
    DEBUG_PRINT(F("Unix time: "));     toki.printTime(toki.getTime());
    DEBUG_PRINT(F("Free heap : "));     DEBUG_PRINTLN(ESP.getFreeHeap());
    DEBUG_PRINT(F("Free heap: "));     DEBUG_PRINTLN(ESP.getFreeHeap());
  //WLEDMM
	#ifdef ARDUINO_ARCH_ESP32
    DEBUG_PRINT(F("Avail heap: "));     DEBUG_PRINTLN(ESP.getMaxAllocHeap());
    DEBUG_PRINTF("%s min free stack %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL)); //WLEDMM
	#endif
    #if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
      //DEBUG_PRINT(F("Total PSRAM: "));    DEBUG_PRINT(ESP.getPsramSize()/1024); DEBUG_PRINTLN("kB");
      DEBUG_PRINT(F("Free PSRAM : "));     DEBUG_PRINT(ESP.getFreePsram()/1024); DEBUG_PRINTLN("kB");
      DEBUG_PRINT(F("Avail PSRAM: "));     DEBUG_PRINT(ESP.getMaxAllocPsram()/1024); DEBUG_PRINTLN("kB");
      DEBUG_PRINT(F("PSRAM in use:")); DEBUG_PRINT(int(ESP.getPsramSize() - ESP.getFreePsram())); DEBUG_PRINTLN(F(" Bytes"));

    } else {
      //DEBUG_PRINTLN(F("No PSRAM"));
	}
    #endif
    DEBUG_PRINT(F("Wifi state: "));      DEBUG_PRINTLN(WiFi.status());

    if (WiFi.status() != lastWifiState) {
      wifiStateChangedTime = millis();
    }
    lastWifiState = WiFi.status();
    DEBUG_PRINT(F("State time: "));      DEBUG_PRINTLN(wifiStateChangedTime);
    DEBUG_PRINT(F("NTP last sync: "));   DEBUG_PRINTLN(ntpLastSyncTime);
    DEBUG_PRINT(F("Client IP: "));       DEBUG_PRINTLN(Network.localIP());
    if (loops > 0) { // avoid division by zero
      DEBUG_PRINT(F("Loops/sec: "));       DEBUG_PRINTLN(loops / 30);
      DEBUG_PRINT(F("UM time[ms]: "));     DEBUG_PRINT(avgUsermodMillis/loops); DEBUG_PRINT("/");DEBUG_PRINTLN(maxUsermodMillis);
      DEBUG_PRINT(F("Strip time[ms]: "));  DEBUG_PRINT(avgStripMillis/loops); DEBUG_PRINT("/"); DEBUG_PRINTLN(maxStripMillis);
    }
    strip.printSize();
    loops = 0;
    maxUsermodMillis = 0;
    maxStripMillis = 0;
    avgUsermodMillis = 0;
    avgStripMillis = 0;
    debugTime = millis();
    DEBUG_PRINTLN(F("---END OF DEBUG INFO---"));
  }
  loops++;
#endif
#ifdef WLED_DEBUG_HEAP
  if (millis() - debugTime > 4999 ) { // WLEDMM: Special case for debugging heap faster
    DEBUG_PRINT(F("*** Free heap: "));     DEBUG_PRINT(heap_caps_get_free_size(0x1800));
    DEBUG_PRINT(F("\tLargest free block: "));     DEBUG_PRINT(heap_caps_get_largest_free_block(0x1800));
    DEBUG_PRINT(F(" *** \t\tArduino min free stack: ")); DEBUG_PRINT(uxTaskGetStackHighWaterMark(NULL));
#if INCLUDE_xTaskGetHandle
    DEBUG_PRINT(F("   TCP min free stack: ")); DEBUG_PRINT(wledmm_get_tcp_stacksize());
#endif
    DEBUG_PRINTLN(F(" ***"));    
    debugTime = millis();
  }
#endif        // WLED_DEBUG_HEAP
  toki.resetTick();

#if WLED_WATCHDOG_TIMEOUT > 0
  // we finished our mainloop, reset the watchdog timer
  if (!strip.isUpdating())
  #ifdef ARDUINO_ARCH_ESP32
    esp_task_wdt_reset();
  #else
    ESP.wdtFeed();
  #endif
#endif

}

#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_FASTPATH)
#undef yield  // WLEDMM restore yield()
#endif

void WLED::enableWatchdog() {
#if WLED_WATCHDOG_TIMEOUT > 0
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
#endif
}

void WLED::disableWatchdog() {
#if WLED_WATCHDOG_TIMEOUT > 0
DEBUG_PRINTLN(F("Watchdog: disabled"));
#ifdef ARDUINO_ARCH_ESP32
  esp_task_wdt_delete(NULL);
#else
  ESP.wdtDisable();
#endif
#endif
}

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

#if !defined(WLEDMM_NO_SERIAL_WAIT) || defined(WLED_DEBUG)
  if (!Serial) delay(1000); // WLEDMM make sure that Serial has initalized
#else
  if (!Serial) delay(300);  // just a tiny wait to avoid problems later when acessing serial
#endif

  #ifdef ARDUINO_ARCH_ESP32
  #if defined(WLED_DEBUG) && (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || ARDUINO_USB_CDC_ON_BOOT)
  if (!Serial) delay(2500);  // WLEDMM allow CDC USB serial to initialise (WLED_DEBUG only)
  #endif
  #if ARDUINO_USB_CDC_ON_BOOT || ARDUINO_USB_MODE
    #if ARDUINO_USB_CDC_ON_BOOT && (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6))
    //  WLEDMM avoid "hung devices" when USB_CDC is enabled; see https://github.com/espressif/arduino-esp32/issues/9043
    Serial.setTxTimeoutMs(0);    // potential side-effect: incomplete debug output, with missing characters whenever TX buffer is full.
    #endif
#if !defined(WLEDMM_NO_SERIAL_WAIT) || defined(WLED_DEBUG)
  if (!Serial) delay(2500);  // WLEDMM: always allow CDC USB serial to initialise
  if (Serial) Serial.println("wait 1");  // waiting a bit longer ensures that a  debug messages are shown in serial monitor
  if (!Serial) delay(2500);
  if (Serial) Serial.println("wait 2");
  if (!Serial) delay(2500);

  if (Serial) Serial.flush(); // WLEDMM
#endif

  //Serial.setTimeout(350); // WLEDMM: don't change timeout, as it causes crashes later
  // WLEDMM: redirect debug output to HWCDC
  #if ARDUINO_USB_CDC_ON_BOOT && (defined(WLED_DEBUG) || defined(SR_DEBUG))
  Serial0.setDebugOutput(false);
  Serial.setDebugOutput(true);
  #endif
  // WLEDMM don't touch serial timeout when we use CDC USB or tinyUSB
  #else // "standard" serial-to-USB chip
  if (Serial) Serial.setTimeout(50);  // WLEDMM - only when serial is initialized
  #endif
  #else  // 8266
  if (Serial) Serial.setTimeout(50);  // WLEDMM - only when serial is initialized
  #endif

  //Serial0.setDebugOutput(false);
  #if CORE_DEBUG_LEVEL || defined(WLED_DEBUG_HEAP) || defined(WLED_DEBUG)
  Serial.setDebugOutput(true);  // enables kernel debug messages on Serial
  #endif
  USER_FLUSH(); delay(100);
  USER_PRINTLN();
  USER_PRINT(F("---WLED "));
  #ifdef WLEDMM_FASTPATH
  USER_PRINT("=FASTPATH= ");
  #endif
  USER_PRINT(versionString);
  USER_PRINT(" ");
  USER_PRINT(VERSION);
  USER_PRINTLN(F(" INIT---"));
  #ifdef WLED_RELEASE_NAME
  USER_PRINTF(" WLEDMM_%s %s, build %s.\n", versionString, releaseString, TOSTRING(VERSION)); // WLEDMM specific
  #endif
  #ifdef ARDUINO_ARCH_ESP32
  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  USER_PRINTF("Running from: %s which is %u bytes and type %u subtype %u at address %x\n",running_partition->label,running_partition->size,running_partition->type,running_partition->subtype,running_partition->address);
  #endif
#ifdef ARDUINO_ARCH_ESP32
  DEBUG_PRINT(F("esp32 "));
  DEBUG_PRINTLN(ESP.getSdkVersion());
  #if defined(ESP_ARDUINO_VERSION)
    //DEBUG_PRINTF(F("arduino-esp32  0x%06x\n"), ESP_ARDUINO_VERSION);
    DEBUG_PRINTF("arduino-esp32 v%d.%d.%d\n", int(ESP_ARDUINO_VERSION_MAJOR), int(ESP_ARDUINO_VERSION_MINOR), int(ESP_ARDUINO_VERSION_PATCH));  // availeable since v2.0.0
  #else
    DEBUG_PRINTLN(F("arduino-esp32 v1.0.x\n"));  // we can't say in more detail.
  #endif

  USER_PRINT(F("CPU:   ")); USER_PRINT(ESP.getChipModel());
  USER_PRINT(F(" rev.")); USER_PRINT(ESP.getChipRevision());
  USER_PRINT(F(", ")); USER_PRINT(ESP.getChipCores()); USER_PRINT(F(" core(s)"));
  USER_PRINT(F(", ")); USER_PRINT(ESP.getCpuFreqMHz()); USER_PRINTLN(F("MHz."));

  // WLEDMM begin
  delay(20); USER_FLUSH(); // drain serial output buffers
  USER_PRINT(F("CPU    "));
  esp_reset_reason_t resetReason = getRestartReason();
  USER_PRINT(restartCode2InfoLong(resetReason));
  USER_PRINT(F(" (code "));
  USER_PRINT((int)resetReason);
  USER_PRINT(F("). "));
  int core0code = getCoreResetReason(0);
  int core1code = getCoreResetReason(1);
  USER_PRINTF("Core#0 %s (%d)", resetCode2Info(core0code).c_str(), core0code);
  if (core1code > 0) {USER_PRINTF("; Core#1 %s (%d)", resetCode2Info(core1code).c_str(), core1code);}
  USER_PRINTLN(F("."));
  if ((core0code > 1) && (core0code <= 20) && (core0code != 3) && (core0code != 12) && (core0code != 14)) errorFlag = ERR_SYS_REBOOT; // abnormal reboot
  if ((resetReason >= 4) && (resetReason < 10)) errorFlag = ERR_SYS_REBOOT; // abnormal reboot (crash, brownout, watchdog, etc)
  if ((resetReason == ESP_RST_BROWNOUT) || (core0code == 15)) errorFlag = ERR_SYS_BROWNOUT; // brownout detected
  // WLEDMM end

  USER_PRINT(F("FLASH: ")); USER_PRINT((ESP.getFlashChipSize()/1024)/1024);
  USER_PRINT(F("MB, Mode ")); USER_PRINT(ESP.getFlashChipMode());
  #ifdef WLED_DEBUG
  switch (ESP.getFlashChipMode()) {
    // missing: Octal modes
    case FM_QIO:  DEBUG_PRINT(F(" (QIO)")); break;
    case FM_QOUT: DEBUG_PRINT(F(" (QOUT)"));break;
    case FM_DIO:  DEBUG_PRINT(F(" (DIO)")); break;
    case FM_DOUT: DEBUG_PRINT(F(" (DOUT)"));break;
    #if defined(CONFIG_IDF_TARGET_ESP32S3) && CONFIG_ESPTOOLPY_FLASHMODE_OPI
      case FM_FAST_READ: DEBUG_PRINT(F(" (OPI)"));break;
    #else
      case FM_FAST_READ: DEBUG_PRINT(F(" (fast_read)"));break;
    #endif
    case FM_SLOW_READ: DEBUG_PRINT(F(" (slow_read)"));break;
    default: break;
  }
  #endif
  USER_PRINT(F(", speed ")); USER_PRINT(ESP.getFlashChipSpeed()/1000000);USER_PRINTLN(F("MHz."));
  
  #if defined(WLED_DEBUG) && defined(ARDUINO_ARCH_ESP32)
  showRealSpeed();
  #endif

#else
  // WLEDMM: more info for 8266
  USER_PRINTLN();
  USER_PRINTF("CPU:   ESP8266 (id 0x%08X)", ESP.getChipId());
  USER_PRINT(F(", ")); USER_PRINT(ESP.getCpuFreqMHz()); USER_PRINTLN(F("MHz."));
  USER_PRINT(F("CPU    Last Restart Reason = "));
  USER_PRINT((int)ESP.getResetInfoPtr()->reason); USER_PRINT(F(" -> "));
  USER_PRINTLN(ESP.getResetInfo());

  USER_PRINT(F("FLASH: ")); USER_PRINT((ESP.getFlashChipRealSize()/1024)/1024);
  USER_PRINT(F("MB, Mode ")); USER_PRINT((int)ESP.getFlashChipMode());
  #ifdef WLED_DEBUG
  switch (ESP.getFlashChipMode()) {
    // missing: Octal modes
    case FM_QIO:  DEBUG_PRINT(F(" (QIO)")); break;
    case FM_QOUT: DEBUG_PRINT(F(" (QOUT)"));break;
    case FM_DIO:  DEBUG_PRINT(F(" (DIO)")); break;
    case FM_DOUT: DEBUG_PRINT(F(" (DOUT)"));break;
    default: break;
  }
  #endif
  USER_PRINT(F(", speed ")); USER_PRINT(ESP.getFlashChipSpeed()/1000000);USER_PRINT(F("MHz; "));
  USER_PRINT(F(" chip ID = 0x"));
  USER_PRINTF("%08X\n", ESP.getFlashChipId());
  USER_PRINTLN();

  DEBUG_PRINT(F("esp8266 "));
  DEBUG_PRINTLN(ESP.getCoreVersion());
#endif
  DEBUG_PRINT(F("heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());
#ifdef ARDUINO_ARCH_ESP32
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)  // unfortunately not available in older framework versions
  DEBUG_PRINT(F("\nArduino  max stack  ")); DEBUG_PRINTLN(getArduinoLoopTaskStackSize());
  #endif
  DEBUG_PRINTF("%s min free stack %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL)); //WLEDMM
#endif

#if defined(ARDUINO_ARCH_ESP32) && (defined(BOARD_HAS_PSRAM) || defined(CONFIG_ESPTOOLPY_FLASHMODE_OPI))
  //psramInit(); //WLEDMM?? softhack007: not sure if explicit init is really needed ... lets disable it here and see if that works
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    #if CONFIG_ESPTOOLPY_FLASHMODE_OPI || (CONFIG_SPIRAM_MODE_OCT && defined(BOARD_HAS_PSRAM))
      // S3: reserve GPIO 33-37 for "octal" PSRAM
      managed_pin_type pins[] = { {33, true}, {34, true}, {35, true}, {36, true}, {37, true} };
      pinManager.allocateMultiplePins(pins, sizeof(pins)/sizeof(managed_pin_type), PinOwner::SPI_RAM);
    #endif
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
  // S2: reserve GPIO 26-32 for PSRAM (may fail due to isPinOk() but that will also prevent other allocation)
  //managed_pin_type pins[] = { {26, true}, {27, true}, {28, true}, {29, true}, {30, true}, {31, true}, {32, true} };
  //pinManager.allocateMultiplePins(pins, sizeof(pins)/sizeof(managed_pin_type), PinOwner::SPI_RAM);
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
  // C3: reserve GPIO 12-17 for PSRAM (may fail due to isPinOk() but that will also prevent other allocation)
  //managed_pin_type pins[] = { {12, true}, {13, true}, {14, true}, {15, true}, {16, true}, {17, true} };
  //pinManager.allocateMultiplePins(pins, sizeof(pins)/sizeof(managed_pin_type), PinOwner::SPI_RAM);
  #else
  // GPIO16/GPIO17 reserved for SPI RAM
  managed_pin_type pins[] = { {16, true}, {17, true} };
  pinManager.allocateMultiplePins(pins, sizeof(pins)/sizeof(managed_pin_type), PinOwner::SPI_RAM);
  #endif
  #if defined(BOARD_HAS_PSRAM) && (defined(WLED_USE_PSRAM) || defined(WLED_USE_PSRAM_JSON))       // WLEDMM
  if (psramFound()) {
    DEBUG_PRINT(F("Total PSRAM: ")); DEBUG_PRINT(ESP.getPsramSize()/1024); DEBUG_PRINTLN("kB");
    DEBUG_PRINT(F("Free PSRAM : ")); DEBUG_PRINT(ESP.getFreePsram()/1024); DEBUG_PRINTLN("kB");
  }
  #else
    DEBUG_PRINTLN(F("PSRAM not used."));
  #endif
#endif
#if defined(ARDUINO_ARCH_ESP32)
  if ((strncmp("ESP32-PICO", ESP.getChipModel(), 10) == 0) || (strncmp("ESP32-U4WDH", ESP.getChipModel(), 11) == 0))
  { // WLEDMM detect pico board and esp32-mini1 board at runtime
    // special handling for PICO-D4: gpio16+17 are in use for onboard SPI FLASH (not PSRAM)
    managed_pin_type pins[] = { {16, true}, {17, true} };
    pinManager.allocateMultiplePins(pins, sizeof(pins)/sizeof(managed_pin_type), PinOwner::SPI_RAM);
  }
#endif

  //DEBUG_PRINT(F("LEDs inited. heap usage ~"));
  //DEBUG_PRINTLN(heapPreAlloc - ESP.getFreeHeap());
  USER_FLUSH();  // WLEDMM flush buffer now, before anything time-critical is started.

  pinManager.manageDebugTXPin();

#ifdef WLED_ENABLE_DMX //reserve GPIO2 as hardcoded DMX pin
  pinManager.allocatePin(2, true, PinOwner::DMX);
#endif

#if defined(ALL_JSON_TO_PSRAM) && defined(BOARD_HAS_PSRAM) && (defined(WLED_USE_PSRAM_JSON) || defined(WLED_USE_PSRAM))
  if (psramFound()) {
    DEBUG_PRINT(F("\nfree heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());
    USER_PRINTLN(F("JSON gabage collection (initial)."));
    doc.garbageCollect();   // WLEDMM experimental - this seems to move the complete doc[] into PSRAM
	  USER_PRINT(F("PSRAM in use:")); USER_PRINT(int(ESP.getPsramSize() - ESP.getFreePsram())); USER_PRINTLN(F(" Bytes."));
    DEBUG_PRINT(F("free heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());
  }
#endif

// WLEDMM experimental: support for single neoPixel on Adafruit boards
#if 0
  //#ifdef PIN_NEOPIXEL
  //pinManager.allocatePin(PIN_NEOPIXEL, true, PinOwner::BusDigital);
  //#endif
  #ifdef NEOPIXEL_POWER
    pinManager.allocatePin(NEOPIXEL_POWER, true, PinOwner::Relay);  // just to ensure this GPIO will not get used for other purposes
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
  #endif
  #ifdef NEOPIXEL_I2C_POWER
    pinManager.allocatePin(NEOPIXEL_I2C_POWER, true, PinOwner::Relay);  // just to ensure this GPIO will not get used for other purposes
    pinMode(NEOPIXEL_I2C_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_I2C_POWER, HIGH);
  #endif
#endif

  USER_PRINTLN();
  DEBUG_PRINTLN(F("Registering usermods ..."));
  registerUsermods();

  DEBUG_PRINT(F("heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());
  #ifdef ARDUINO_ARCH_ESP32
    DEBUG_PRINTF("%s min free stack %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL)); //WLEDMM
  #endif

  for (uint8_t i=1; i<WLED_MAX_BUTTONS; i++) btnPin[i] = -1;

  bool fsinit = false;
  USER_PRINTLN(F("Mounting FS ..."));
#ifdef ARDUINO_ARCH_ESP32
  fsinit = WLED_FS.begin(true);
#else
  fsinit = WLED_FS.begin();
#endif
  if (!fsinit) {
    USER_PRINTLN(F("Mount FS failed!"));  // WLEDMM
    errorFlag = ERR_FS_BEGIN;
  } else {
      USER_PRINTLN(F("Mount FS succeeded.")); // WLEDMM
  }
#ifdef WLED_ADD_EEPROM_SUPPORT
  else deEEP();
#else
  initPresetsFile();
#endif
  updateFSInfo();

  USER_PRINT(F("done Mounting FS; "));
  USER_PRINT(((fsBytesTotal-fsBytesUsed)/1024)); USER_PRINTLN(F(" kB free.\n"));

  // generate module IDs must be done before AP setup
  escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();

  WLED_SET_AP_SSID(); // otherwise it is empty on first boot until config is saved

  DEBUG_PRINTLN(F("Reading config"));
  deserializeConfigFromFS();

#if defined(STATUSLED) && STATUSLED>=0
  if (!pinManager.isPinAllocated(STATUSLED)) {
    // NOTE: Special case: The status LED should *NOT* be allocated.
    //       See comments in handleStatusLed().
    pinMode(STATUSLED, OUTPUT);
  }
#endif

  DEBUG_PRINTLN(F("Initializing strip"));
  beginStrip();
  DEBUG_PRINT(F("heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());

  USER_PRINTLN(F("\nUsermods setup ..."));
  userSetup();
  usermods.setup();
  DEBUG_PRINT(F("heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());

  if (strcmp(clientSSID, DEFAULT_CLIENT_SSID) == 0)
    showWelcomePage = true;
  WiFi.persistent(false);
  #ifdef WLED_USE_ETHERNET
  WiFi.onEvent(WiFiEvent);
  #endif

  #ifdef WLED_ENABLE_ADALIGHT
  //Serial RX (Adalight, Improv, Serial JSON) only possible if GPIO3 unused
  //Serial TX (Debug, Improv, Serial JSON) only possible if GPIO1 unused
  if (!pinManager.isPinAllocated(hardwareRX) && !pinManager.isPinAllocated(hardwareTX)) {
    if (Serial) Serial.println(F("\nAda"));
  }
  #endif

  // fill in unique mdns default
  if (strcmp(cmDNS, "x") == 0) sprintf_P(cmDNS, PSTR("wled-%*s"), 6, escapedMac.c_str() + 6);
#ifndef WLED_DISABLE_MQTT
  if (mqttDeviceTopic[0] == 0) sprintf_P(mqttDeviceTopic, PSTR("wled/%*s"), 6, escapedMac.c_str() + 6);
  if (mqttClientID[0] == 0)    sprintf_P(mqttClientID, PSTR("WLED-%*s"), 6, escapedMac.c_str() + 6);
#endif

#ifdef WLED_ENABLE_ADALIGHT
  if (Serial && (Serial.available() > 0) && (Serial.peek() == 'I')) handleImprovPacket();
#endif

#ifndef WLED_DISABLE_OTA
  if (aOtaEnabled) {
    ArduinoOTA.onStart([]() {
#ifdef ESP8266
      wifi_set_sleep_type(NONE_SLEEP_T);
#endif
      WLED::instance().disableWatchdog();
      DEBUG_PRINTLN(F("Start ArduinoOTA"));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      // reenable watchdog on failed update
      WLED::instance().enableWatchdog();
    });
    if (strlen(cmDNS) > 0)
      ArduinoOTA.setHostname(cmDNS);
  }
#endif
#ifdef WLED_ENABLE_DMX
  initDMXOutput();
#endif
#ifdef WLED_ENABLE_DMX_INPUT
  dmxInput.init(dmxInputReceivePin, dmxInputTransmitPin, dmxInputEnablePin, dmxInputPort);
#endif

#ifdef WLED_ENABLE_ADALIGHT
  if (Serial && (Serial.available() > 0) && (Serial.peek() == 'I')) handleImprovPacket();
#endif

  // HTTP server page init
  DEBUG_PRINTLN(F("initServer"));
  initServer();
  DEBUG_PRINT(F("heap ")); DEBUG_PRINTLN(ESP.getFreeHeap());
  #ifdef ARDUINO_ARCH_ESP32
  DEBUG_PRINT(pcTaskGetTaskName(NULL)); DEBUG_PRINT(F(" free stack ")); DEBUG_PRINTLN(uxTaskGetStackHighWaterMark(NULL));
  #endif

  // Seed FastLED random functions with an esp random value, which already works properly at this point.
#if defined(ARDUINO_ARCH_ESP32)
  uint32_t seed32 = esp_random();
  seed32 ^= random(0, INT32_MAX);  // WLEDMM some extra entropy (for older frameworks where esp_ramdom alone might be too predictable after startup)
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
  
  #ifdef ARDUINO_ARCH_ESP32
  #ifdef ARDUINO_RUNNING_CORE
    DEBUG_PRINTF("Arduino core=%d (loop is now on core #%d)\n", int(ARDUINO_RUNNING_CORE), int(xPortGetCoreID()));
  #endif
  #ifdef ARDUINO_EVENT_RUNNING_CORE
      DEBUG_PRINTF("Arduino Event core=%d\n", int(ARDUINO_EVENT_RUNNING_CORE));
  #endif
  #endif

  // WLEDMM : dump GPIO infos (experimental, UI integration pending)
  //#ifdef WLED_DEBUG
  USER_PRINTLN(F("\nGPIO\t| Assigned to\t\t| Info"));
  USER_PRINTLN(F("--------|-----------------------|------------"));
  for(int pinNr = 0; pinNr < WLED_NUM_PINS; pinNr++) { // 49 = highest PIN on ESP32-S3
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if((pinManager.isPinOk(pinNr, false)) || (pinNr > 18 && pinNr < 21)) {  // softhack007: list USB pins
#else
    if(pinManager.isPinOk(pinNr, false)) {
#endif
      //if ((!pinManager.isPinAllocated(pinNr)) && (pinManager.getPinSpecialText(pinNr).length() == 0)) continue;      // un-comment to hide no-name,unused GPIO pins
      bool is_inOut = pinManager.isPinOk(pinNr, true);
#if 0 // for testing
      USER_PRINT(pinManager.isPinAnalog(pinNr) ? "A": " ");
      USER_PRINT(pinManager.isPinADC1(pinNr) ? "1": " ");
      USER_PRINT(pinManager.isPinADC2(pinNr) ? "2": " ");
      USER_PRINT(pinManager.isPinTouch(pinNr) ? "T": " ");
      USER_PRINT(pinManager.isPinPWM(pinNr) ? " P": "  ");
      USER_PRINT(pinManager.isPinINT(pinNr) ? "I ": "  ");
#endif
      USER_PRINTF("%s  %2d\t  %-17s %s\t  %s\n", 
          (is_inOut?"i/o":"in "), 
          pinNr, 
          pinManager.getPinOwnerText(pinNr).c_str(),
          pinManager.getPinConflicts(pinNr).c_str(),
          pinManager.getPinSpecialText(pinNr).c_str()
      );
      USER_FLUSH();  // avoid lost lines (Serial buffer overflow)
    }
  }

#if 0 // for testing
  USER_PRINTLN(F("\n"));
  USER_PRINTF("ADC1-0 = %d, ADC1-3 = %d, ADC1-7 = %d, ADC2-0 = %d, ADC2-1 = %d, ADC2-8 = %d, ADC2-10 = %d\n",
    pinManager.getADCPin(PM_ADC1, 0), pinManager.getADCPin(PM_ADC1, 3), pinManager.getADCPin(PM_ADC1, 7), 
    pinManager.getADCPin(PM_ADC2, 0), pinManager.getADCPin(PM_ADC2, 1), pinManager.getADCPin(PM_ADC2, 8),
    pinManager.getADCPin(PM_ADC2, 10)
  );
  USER_PRINTLN();
  for(int p=0; p<11; p++) {
    if(pinManager.getADCPin(PinManagerClass::ADC1, p) < 255)
      USER_PRINTF("ADC1-%d = %d, ", p, pinManager.getADCPin(PinManagerClass::ADC1, p));
  }
  USER_PRINTLN();
  for(int p=0; p<11; p++) {
    if(pinManager.getADCPin(PinManagerClass::ADC2, p) < 255)
      USER_PRINTF("ADC2-%d = %d, ", p, pinManager.getADCPin(PinManagerClass::ADC2, p));
  }
  USER_PRINTLN(F("\n"));
#endif

  USER_PRINT(F("Free heap ")); USER_PRINTLN(ESP.getFreeHeap());USER_PRINTLN();
  USER_PRINTLN(F("WLED initialization done.\n"));
  delay(50);
  // repeat Ada prompt
  #ifdef WLED_ENABLE_ADALIGHT
  if (!pinManager.isPinAllocated(hardwareRX) && !pinManager.isPinAllocated(hardwareTX)) {
    if (Serial) Serial.println(F("\nAda"));
  }
  #endif

  //#endif
  // WLEDMM end
}

void WLED::beginStrip()
{
  // Initialize NeoPixel Strip and button
  strip.fill(BLACK);    // WLEDMM avoids random colors at power-on
  strip.finalizeInit(); // busses created during deserializeConfig()
  strip.makeAutoSegments();
  strip.setBrightness(0, true); // WLEDMM directly apply BLACK (no transition time)
  strip.setShowCallback(handleOverlayDraw);

  if (turnOnAtBoot) {
    if (briS > 0) bri = briS;
    else if (bri == 0) bri = 128;
  } else {
    // fix for #3196
    briLast = briS; bri = 0;
    strip.fill(BLACK);
    strip.show();
  }
  if (bootPreset > 0) {
    applyPreset(bootPreset, CALL_MODE_INIT);
  }
  colorUpdated(CALL_MODE_INIT);

  // init relay pin
  if (rlyPin>=0) {
    if (strip.isUpdating()) delay(FRAMETIME_FIXED); // WLEDMM ensure that no background led communication is happening while powering on the strip
    digitalWrite(rlyPin, (rlyMde ? bri : !bri));
    delay(75); // wait for relay to switch and power to stabilize
    strip.show(); // update LEDs
    delay(5);
  }
}

void WLED::initAP(bool resetAP)
{
  if (apBehavior == AP_BEHAVIOR_BUTTON_ONLY && !resetAP)
    return;

  if (resetAP) {
    WLED_SET_AP_SSID();
    strcpy_P(apPass, PSTR(WLED_AP_PASS));
  }
  USER_PRINT(F("Opening access point "));  // WLEDMM
  USER_PRINTLN(apSSID);                    // WLEDMM
  WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID, apPass, apChannel, apHide, 8); // WLED-MM allow up to 8 clients for ad-hoc "in the field" syncing.
  #if defined(LOLIN_WIFI_FIX) && (defined(ARDUINO_ARCH_ESP32C3) || defined(ARDUINO_ARCH_ESP32S2) || defined(ARDUINO_ARCH_ESP32S3))
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  #endif

  if (!apActive) // start captive portal if AP active
  {
    DEBUG_PRINTLN(F("Init AP interfaces"));
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
  }
  apActive = true;
}

bool WLED::initEthernet()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)

  static bool successfullyConfiguredEthernet = false;

  if (successfullyConfiguredEthernet) {
    // DEBUG_PRINTLN(F("initE: ETH already successfully configured, ignoring"));
    return false;
  }
  if (ethernetType == WLED_ETH_NONE) {
    return false;
  }
  if (ethernetType >= WLED_NUM_ETH_TYPES) {
    DEBUG_PRINT(F("initE: Ignoring attempt for invalid ethernetType ")); DEBUG_PRINTLN(ethernetType);
    return false;
  }

  DEBUG_PRINT(F("initE: Attempting ETH config: ")); DEBUG_PRINTLN(ethernetType);

  // Ethernet initialization should only succeed once -- else reboot required
  ethernet_settings es = ethernetBoards[ethernetType];
  managed_pin_type pinsToAllocate[10] = {
    // first six pins are non-configurable
    esp32_nonconfigurable_ethernet_pins[0],
    esp32_nonconfigurable_ethernet_pins[1],
    esp32_nonconfigurable_ethernet_pins[2],
    esp32_nonconfigurable_ethernet_pins[3],
    esp32_nonconfigurable_ethernet_pins[4],
    esp32_nonconfigurable_ethernet_pins[5],
    { (int8_t)es.eth_mdc,   true },  // [6] = MDC  is output and mandatory
    { (int8_t)es.eth_mdio,  true },  // [7] = MDIO is bidirectional and mandatory
    { (int8_t)es.eth_power, true },  // [8] = optional pin, not all boards use
    { ((int8_t)0xFE),       false }, // [9] = replaced with eth_clk_mode, mandatory
  };
  // update the clock pin....
  if (es.eth_clk_mode == ETH_CLOCK_GPIO0_IN) {
    pinsToAllocate[9].pin = 0;
    pinsToAllocate[9].isOutput = false;
  } else if (es.eth_clk_mode == ETH_CLOCK_GPIO0_OUT) {
    pinsToAllocate[9].pin = 0;
    pinsToAllocate[9].isOutput = true;
  } else if (es.eth_clk_mode == ETH_CLOCK_GPIO16_OUT) {
    pinsToAllocate[9].pin = 16;
    pinsToAllocate[9].isOutput = true;
  } else if (es.eth_clk_mode == ETH_CLOCK_GPIO17_OUT) {
    pinsToAllocate[9].pin = 17;
    pinsToAllocate[9].isOutput = true;
  } else {
    DEBUG_PRINT(F("initE: Failing due to invalid eth_clk_mode ("));
    DEBUG_PRINT(es.eth_clk_mode);
    DEBUG_PRINTLN(")");
    return false;
  }

  if (!pinManager.allocateMultiplePins(pinsToAllocate, 10, PinOwner::Ethernet)) {
    DEBUG_PRINTLN(F("initE: Failed to allocate ethernet pins"));
    return false;
  }

  /*
  For LAN8720 the most correct way is to perform clean reset each time before init
  applying LOW to power or nRST pin for at least 100 us (please refer to datasheet, page 59)
  ESP_IDF > V4 implements it (150 us, lan87xx_reset_hw(esp_eth_phy_t *phy) function in 
  /components/esp_eth/src/esp_eth_phy_lan87xx.c, line 280)
  but ESP_IDF < V4 does not. Lets do it:
  [not always needed, might be relevant in some EMI situations at startup and for hot resets]
  */
  #if ESP_IDF_VERSION_MAJOR==3
  if(es.eth_power>0 && es.eth_type==ETH_PHY_LAN8720) {
    pinMode(es.eth_power, OUTPUT);
    digitalWrite(es.eth_power, 0);
    delayMicroseconds(150);
    digitalWrite(es.eth_power, 1);
    delayMicroseconds(10);
  }
  #endif

  if (!ETH.begin(
                (uint8_t) es.eth_address,
                (int)     es.eth_power,
                (int)     es.eth_mdc,
                (int)     es.eth_mdio,
                (eth_phy_type_t)   es.eth_type,
                (eth_clock_mode_t) es.eth_clk_mode
                )) {
    DEBUG_PRINTLN(F("initC: ETH.begin() failed"));
    // de-allocate the allocated pins
    for (managed_pin_type mpt : pinsToAllocate) {
      pinManager.deallocatePin(mpt.pin, PinOwner::Ethernet);
    }
    return false;
  }

  successfullyConfiguredEthernet = true;
  USER_PRINTLN(F("initC: *** Ethernet successfully configured! ***"));  // WLEDMM
  return true;
#else
  return false; // Ethernet not enabled for build
#endif

}

void WLED::initConnection()
{
#ifdef ARDUINO_ARCH_ESP32
  unsigned long t_wait = millis();
  while(strip.isUpdating() && (millis() - t_wait < 86)) delay(1); // WLEDMM try to catch a moment when strip is idle
  //if (strip.isUpdating()) USER_PRINTLN("WLED::initConnection: strip still updating.");
#endif

  #ifdef WLED_ENABLE_WEBSOCKETS
  ws.onEvent(wsEvent);
  #endif

  WiFi.disconnect(true);        // close old connections
#ifdef ESP8266
  WiFi.setPhyMode(force802_3g ? WIFI_PHY_MODE_11G : WIFI_PHY_MODE_11N);
#endif

  if (staticIP[0] != 0 && staticGateway[0] != 0) {
    WiFi.config(staticIP, staticGateway, staticSubnet, IPAddress(1, 1, 1, 1));
  } else {
    WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0));
  }

  lastReconnectAttempt = millis();

  if (!WLED_WIFI_CONFIGURED) {
    USER_PRINTLN(F("No WiFi connection configured."));  // WLEDMM
    if (!apActive) initAP();        // instantly go to ap mode
    return;
  } else if (!apActive) {
    if (apBehavior == AP_BEHAVIOR_ALWAYS) {
      DEBUG_PRINTLN(F("Access point ALWAYS enabled."));
      initAP();
    } else {
      DEBUG_PRINTLN(F("Access point disabled (init)."));
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
    }
  }
  showWelcomePage = false;

  USER_PRINT(F("Connecting to "));
  USER_PRINT(clientSSID);
  USER_PRINT(" / ");
  for(unsigned i = 0; i<strlen(clientPass); i++) {
      USER_PRINT("*");
  }
  USER_PRINTLN(" ...");

  // convert the "serverDescription" into a valid DNS hostname (alphanumeric)
  char hostname[25];
  prepareHostname(hostname);

#ifdef ESP8266
  WiFi.hostname(hostname);
#endif

  WiFi.begin(clientSSID, clientPass);
#ifdef ARDUINO_ARCH_ESP32
  #if defined(LOLIN_WIFI_FIX) && (defined(ARDUINO_ARCH_ESP32C3) || defined(ARDUINO_ARCH_ESP32S2) || defined(ARDUINO_ARCH_ESP32S3))
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  #endif
  WiFi.setSleep(!noWifiSleep);
  WiFi.setHostname(hostname);
#else
  wifi_set_sleep_type((noWifiSleep) ? NONE_SLEEP_T : MODEM_SLEEP_T);
#endif
}

void WLED::initInterfaces()
{
  DEBUG_PRINTLN(F("Init STA interfaces"));

#ifndef WLED_DISABLE_HUESYNC
  IPAddress ipAddress = Network.localIP();
  if (hueIP[0] == 0) {
    hueIP[0] = ipAddress[0];
    hueIP[1] = ipAddress[1];
    hueIP[2] = ipAddress[2];
  }
#endif

// aOtaEnabled=false; strcpy(cmDNS, ""); // WLEDMM use this to disable OTA and mDNS

//WLEDMM: add netdebug variables
#ifdef WLED_DEBUG_HOST
  if (netDebugPrintIP[0] == 0) {
    //WLEDMM: this code moved from net_debug.cpp as we store IP as IPAddress type
    if (!netDebugPrintIP && !netDebugPrintIP.fromString(WLED_DEBUG_HOST)) {
      #ifdef ESP8266
        WiFi.hostByName(WLED_DEBUG_HOST, netDebugPrintIP, 750);
      #else
        #ifdef WLED_USE_ETHERNET
          // ETH.hostByName(WLED_DEBUG_HOST, netDebugPrintIP); WLEDMM: ETH.hostByName does not exist, WiFi.hostByName seems to do the same, but must be tested.
          WiFi.hostByName(WLED_DEBUG_HOST, netDebugPrintIP);
        #else
          WiFi.hostByName(WLED_DEBUG_HOST, netDebugPrintIP);
        #endif
      #endif
    } else {
      IPAddress ndIpAddress = Network.localIP();
      netDebugPrintIP[0] = ndIpAddress[0];
      netDebugPrintIP[1] = ndIpAddress[1];
      netDebugPrintIP[2] = ndIpAddress[2];
    }
  }
  if (netDebugPrintPort == 0) 
    #ifdef WLED_DEBUG_PORT
      netDebugPrintPort = WLED_DEBUG_PORT;
    #else
      netDebugPrintPort = 7868; //Default value
    #endif
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

  #ifndef WLED_DISABLE_OTA   // WLEDMM
  if (aOtaEnabled) {
    USER_PRINT(F("           ArduinoOTA: "));
    USER_PRINTLN(ArduinoOTA.getHostname());
  }
  #endif                     // WLEDMM end

  // Set up mDNS responder:
  if (strlen(cmDNS) > 0) {
    // "end" must be called before "begin" is called a 2nd time
    // see https://github.com/esp8266/Arduino/issues/7213
    MDNS.end();
    MDNS.begin(cmDNS);

    USER_PRINTF("mDNS started: %s.local\n", cmDNS); // WLEDMM
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
  reconnectHue();
#ifndef WLED_DISABLE_MQTT
  initMqtt();
#endif
  interfacesInited = true;
  wasConnected = true;
}

void WLED::handleConnection()
{
  static byte stacO = 0;
  static uint32_t lastHeap = UINT32_MAX;
  static unsigned long heapTime = 0;
  unsigned long now = millis();

  if (now < 2000 && (!WLED_WIFI_CONFIGURED || apBehavior == AP_BEHAVIOR_ALWAYS))
    return;

  if (lastReconnectAttempt == 0) {
    DEBUG_PRINTLN(F("lastReconnectAttempt == 0"));
    initConnection();
    return;
  }

  static unsigned retryCount = 0;  // WLEDMM
  #ifdef ARDUINO_ARCH_ESP32 
  // reconnect WiFi to clear stale allocations if heap gets too low
  if ((!strip.isUpdating()) && (now - heapTime > 5000)) { // WLEDMM: updated with better logic for small heap available by block, not total. // WLEDMM trying to use a moment when the strip is idle
#if defined(ARDUINO_ARCH_ESP32S2) || defined(WLED_ENABLE_HUB75MATRIX)
    uint32_t heap = ESP.getFreeHeap(); // WLEDMM works better on -S2
#else
    uint32_t heap = heap_caps_get_largest_free_block(0x1800); // WLEDMM: This is a better metric for free heap.
#endif
    if (heap < MIN_HEAP_SIZE && lastHeap < MIN_HEAP_SIZE) {
      if (retryCount < 5) {  // WLEDMM avoid repeated disconnects
        USER_PRINT(F("Heap too low! (step 2, force reconnect): "));
        USER_PRINTLN(heap);
        forceReconnect = true;
        strip.purgeSegments(true); // remove all but one segments from memory
        // WLEDMM
        errorFlag = ERR_LOW_MEM;
        retryCount ++;
      }
      errorFlag = ERR_LOW_MEM;
    } else if (heap < MIN_HEAP_SIZE) {
      USER_PRINT(F("Heap too low! (step 1, flush unread UDP): "));
      USER_PRINTLN(heap);      
      strip.purgeSegments();
      notifierUdp.flush();
      rgbUdp.flush();
      notifier2Udp.flush();
      ntpUdp.flush();
      // WLEDMM
      errorFlag = ERR_LOW_MEM;
      retryCount = 1;
    } else retryCount = 0;  // WLEDMM memory OK - reset counter
    lastHeap = heap;
    heapTime = now;
  }
  #else
  // reconnect WiFi to clear stale allocations if heap gets too low
  if (now - heapTime > 5000) {
    uint32_t heap = ESP.getFreeHeap();
    if (heap < MIN_HEAP_SIZE && lastHeap < MIN_HEAP_SIZE) {
      if (retryCount < 5) {  // WLEDMM avoid repeated disconnects
        USER_PRINT(F("Heap too low! (step 2, force reconnect): "));
        USER_PRINTLN(heap);
        forceReconnect = true;
        strip.purgeSegments(true); // remove all but one segments from memory
        // WLEDMM
        errorFlag = ERR_LOW_MEM;
        retryCount ++;
      }
    } else if (heap < MIN_HEAP_SIZE) {
      USER_PRINT(F("Heap too low! (step 1, purge segments): "));
      USER_PRINTLN(heap);      
      strip.purgeSegments();
      // WLEDMM
      errorFlag = ERR_LOW_MEM;
      retryCount = 1;
    } else retryCount = 0;  // WLEDMM memory OK - reset counter
    lastHeap = heap;
    heapTime = now;
  }
  #endif
  
  byte stac = 0;
  if (apActive) {
#ifdef ESP8266
    stac = wifi_softap_get_station_num();
#else
    wifi_sta_list_t stationList;
    esp_wifi_ap_get_sta_list(&stationList);
    stac = stationList.num;
#endif
    if (stac != stacO) {
      stacO = stac;
      DEBUG_PRINT(F("Connected AP clients: "));
      DEBUG_PRINTLN(stac);
      if (!WLED_CONNECTED && WLED_WIFI_CONFIGURED) {        // trying to connect, but not connected
        if (stac)
          WiFi.disconnect();        // disable search so that AP can work
        else
          initConnection();         // restart search
      }
    }
  }
  if (forceReconnect) {
    USER_PRINTLN(F("Forcing reconnect."));
    initConnection();
    interfacesInited = false;
    forceReconnect = false;
    wasConnected = false;
    return;
  }
  if (!Network.isConnected()) {
    if (interfacesInited) {
      USER_PRINTLN(F("Disconnected!"));
      interfacesInited = false;
      initConnection();
    }
    //send improv failed 6 seconds after second init attempt (24 sec. after provisioning)
    if (improvActive > 2 && now - lastReconnectAttempt > 6000) {
      sendImprovStateResponse(0x03, true);
      improvActive = 2;
    }
    if (now - lastReconnectAttempt > ((stac) ? 300000 : 18000) && WLED_WIFI_CONFIGURED) {
      if (improvActive == 2) improvActive = 3;
      DEBUG_PRINTLN(F("Last reconnect too old."));
      initConnection();
    }
    if (!apActive && now - lastReconnectAttempt > 12000 && (!wasConnected || apBehavior == AP_BEHAVIOR_NO_CONN)) {
      DEBUG_PRINTLN(F("Not connected AP."));
      initAP();
    }
  } else if (!interfacesInited) { //newly connected
    USER_PRINTLN("");
    USER_PRINT(F("Connected! IP address: http://"));
    USER_PRINTLN(Network.localIP());
    //if (Network.isEthernet()) {
    //  #if ESP32
    //  USER_PRINT(ETH.localIP());
    //  USER_PRINTLN(" via Ethernet");
    //  #endif
    //} else {
    //  USER_PRINT(Network.localIP());
    //  USER_PRINTLN(" via WiFi");
    //}

    if (improvActive) {
      if (improvError == 3) sendImprovStateResponse(0x00, true);
      sendImprovStateResponse(0x04);
      if (improvActive > 1) sendImprovIPRPCResult(ImprovRPCType::Command_Wifi);
    }
    initInterfaces();
    userConnected();
    usermods.connected();
    lastMqttReconnectAttempt = 0; // force immediate update

    // shut down AP
    if (apBehavior != AP_BEHAVIOR_ALWAYS && apActive) {
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      apActive = false;
      USER_PRINTLN(F("Access point disabled (handle)."));
    }
  }
}

// If status LED pin is allocated for other uses, does nothing
// else blink at 1Hz when WLED_CONNECTED is false (no WiFi, ?? no Ethernet ??)
// else blink at 2Hz when MQTT is enabled but not connected
// else turn the status LED off
void WLED::handleStatusLED()
{
  #if defined(STATUSLED)
  uint32_t c = 0;

  #if STATUSLED>=0
  if (pinManager.isPinAllocated(STATUSLED)) {
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
#if 0
      // WLEDMM un-comment this to stop the blinking
      if ((ledStatusType != 2) && (ledStatusType != 4))
        ledStatusState = !ledStatusState;
      else
        ledStatusState = HIGH;
#else
        ledStatusState = !ledStatusState;
#endif
      #if STATUSLED>=0
      digitalWrite(STATUSLED, ledStatusState);
      #else
      busses.setStatusPixel(ledStatusState ? c : 0);
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
      busses.setStatusPixel(0);
    #endif
  }
  #endif
}
