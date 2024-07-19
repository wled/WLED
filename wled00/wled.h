#ifndef WLED_H
#define WLED_H
/*
   Main sketch, global variable declarations
   @title WLED project sketch
   @version 0.14.3
   @author Christian Schwinne
 */

// version code in format yymmddb (b = daily build)
#define VERSION 2405180

//uncomment this if you have a "my_config.h" file you'd like to use
//#define WLED_USE_MY_CONFIG

// ESP8266-01 (blue) got too little storage space to work with WLED. 0.10.2 is the last release supporting this unit.

// ESP8266-01 (black) has 1MB flash and can thus fit the whole program, although OTA update is not possible. Use 1M(128K SPIFFS).
// 2-step OTA may still be possible: https://github.com/Aircoookie/WLED/issues/2040#issuecomment-981111096
// Uncomment some of the following lines to disable features:
// Alternatively, with platformio pass your chosen flags to your custom build target in platformio_override.ini

// You are required to disable over-the-air updates:
//#define WLED_DISABLE_OTA         // saves 14kb

// You can choose some of these features to disable:
//#define WLED_DISABLE_ALEXA       // saves 11kb
//#define WLED_DISABLE_HUESYNC     // saves 4kb
//#define WLED_DISABLE_INFRARED    // saves 12kb, there is no pin left for this on ESP8266-01
#ifndef WLED_DISABLE_MQTT
  #define WLED_ENABLE_MQTT         // saves 12kb
#endif
#ifndef WLED_DISABLE_ADALIGHT      // can be used to disable reading commands from serial RX pin (see issue #3128). 
  #define WLED_ENABLE_ADALIGHT     // disable saves 5Kb (uses GPIO3 (RX) for serial). Related serial protocols: Adalight/TPM2, Improv, Serial JSON, Continuous Serial Streaming 
#else
  #undef WLED_ENABLE_ADALIGHT      // disable has priority over enable
#endif
//#define WLED_ENABLE_DMX          // uses 3.5kb (use LEDPIN other than 2)
#define WLED_ENABLE_JSONLIVE     // peek LED output via /json/live (WS binary peek is always enabled)
#ifndef WLED_DISABLE_LOXONE
  #define WLED_ENABLE_LOXONE       // uses 1.2kb
#endif
#ifndef WLED_DISABLE_WEBSOCKETS
  #define WLED_ENABLE_WEBSOCKETS
#endif

//#define WLED_DISABLE_ESPNOW      // Removes dependence on esp now 

#define WLED_ENABLE_FS_EDITOR      // enable /edit page for editing FS content. Will also be disabled with OTA lock

// to toggle usb serial debug (un)comment the following line
//#define WLED_DEBUG

// filesystem specific debugging
//#define WLED_DEBUG_FS

#ifndef WLED_WATCHDOG_TIMEOUT
  // 3 seconds should be enough to detect a lockup
  // define WLED_WATCHDOG_TIMEOUT=0 to disable watchdog, default
  #define WLED_WATCHDOG_TIMEOUT 0
#endif

//optionally disable brownout detector on ESP32.
//This is generally a terrible idea, but improves boot success on boards with a 3.3v regulator + cap setup that can't provide 400mA peaks
//#define WLED_DISABLE_BROWNOUT_DET

// Library inclusions.
#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <ESPAsyncTCP.h>
  #include <LittleFS.h>
  extern "C"
  {
  #include <user_interface.h>
  }
  #ifndef WLED_DISABLE_ESPNOW
    #include <espnow.h>
  #endif
#else // ESP32
  #include <HardwareSerial.h>  // ensure we have the correct "Serial" on new MCUs (depends on ARDUINO_USB_MODE and ARDUINO_USB_CDC_ON_BOOT)
  #include <WiFi.h>
  #include <ETH.h>
  #include "esp_wifi.h"
  #include <ESPmDNS.h>
  #include <AsyncTCP.h>
  #if LOROL_LITTLEFS
    #ifndef CONFIG_LITTLEFS_FOR_IDF_3_2
      #define CONFIG_LITTLEFS_FOR_IDF_3_2
    #endif
    #include <LITTLEFS.h>
  #else
    #include <LittleFS.h>
  #endif
  #include "esp_task_wdt.h"

  #ifndef WLED_DISABLE_ESPNOW
    #include <esp_now.h>
  #endif
#endif
#include <Wire.h>
#include <SPI.h>

#include "src/dependencies/network/Network.h"

#ifdef WLED_USE_MY_CONFIG
  #include "my_config.h"
#endif

#include <ESPAsyncWebServer.h>
#ifdef WLED_ADD_EEPROM_SUPPORT
  #include <EEPROM.h>
#endif
#include <WiFiUdp.h>
#include <DNSServer.h>
#ifndef WLED_DISABLE_OTA
  #define NO_OTA_PORT
  #include <ArduinoOTA.h>
#endif
#include <SPIFFSEditor.h>
#include "src/dependencies/time/TimeLib.h"
#include "src/dependencies/timezone/Timezone.h"
#include "src/dependencies/toki/Toki.h"

#ifndef WLED_DISABLE_ALEXA
  #define ESPALEXA_ASYNC
  #define ESPALEXA_NO_SUBPAGE
  #define ESPALEXA_MAXDEVICES 10
  // #define ESPALEXA_DEBUG
  #include "src/dependencies/espalexa/Espalexa.h"
  #include "src/dependencies/espalexa/EspalexaDevice.h"
#endif

#ifdef WLED_ENABLE_DMX
 #ifdef ESP8266
  #include "src/dependencies/dmx/ESPDMX.h"
 #else //ESP32
  #include "src/dependencies/dmx/SparkFunDMX.h"
 #endif
#endif

#include "src/dependencies/e131/ESPAsyncE131.h"
#ifdef WLED_ENABLE_MQTT
#include "src/dependencies/async-mqtt-client/AsyncMqttClient.h"
#endif

#define ARDUINOJSON_DECODE_UNICODE 0
#include "src/dependencies/json/AsyncJson-v6.h"
#include "src/dependencies/json/ArduinoJson-v6.h"

// ESP32-WROVER features SPI RAM (aka PSRAM) which can be allocated using ps_malloc()
// we can create custom PSRAMDynamicJsonDocument to use such feature (replacing DynamicJsonDocument)
// The following is a construct to enable code to compile without it.
// There is a code that will still not use PSRAM though:
//    AsyncJsonResponse is a derived class that implements DynamicJsonDocument (AsyncJson-v6.h)
#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM) && defined(WLED_USE_PSRAM)
struct PSRAM_Allocator {
  void* allocate(size_t size) {
    if (psramFound()) return ps_malloc(size); // use PSRAM if it exists
    else              return malloc(size);    // fallback
  }
  void* reallocate(void* ptr, size_t new_size) {
    if (psramFound()) return ps_realloc(ptr, new_size); // use PSRAM if it exists
    else              return realloc(ptr, new_size);    // fallback
  }
  void deallocate(void* pointer) {
    free(pointer);
  }
};
using PSRAMDynamicJsonDocument = BasicJsonDocument<PSRAM_Allocator>;
#else
#define PSRAMDynamicJsonDocument DynamicJsonDocument
#endif

#include "const.h"
#include "fcn_declare.h"
#include "NodeStruct.h"
#include "pin_manager.h"
#include "bus_manager.h"
#include "FX.h"

#ifndef CLIENT_SSID
  #define CLIENT_SSID DEFAULT_CLIENT_SSID
#endif

#ifndef CLIENT_PASS
  #define CLIENT_PASS ""
#endif

#ifndef MDNS_NAME
  #define MDNS_NAME DEFAULT_MDNS_NAME
#endif

#if defined(WLED_AP_PASS) && !defined(WLED_AP_SSID)
  #error WLED_AP_PASS is defined but WLED_AP_SSID is still the default. \
         Please change WLED_AP_SSID to something unique.
#endif

#ifndef WLED_AP_SSID
  #define WLED_AP_SSID DEFAULT_AP_SSID
#endif

#ifndef WLED_AP_PASS
  #define WLED_AP_PASS DEFAULT_AP_PASS
#endif

#ifndef SPIFFS_EDITOR_AIRCOOOKIE
  #error You are not using the Aircoookie fork of the ESPAsyncWebserver library.\
  Using upstream puts your WiFi password at risk of being served by the filesystem.\
  Comment out this error message to build regardless.
#endif

#ifndef WLED_DISABLE_INFRARED
  #include <IRremoteESP8266.h>
  #include <IRrecv.h>
  #include <IRutils.h>
#endif

//Filesystem to use for preset and config files. SPIFFS or LittleFS on ESP8266, SPIFFS only on ESP32 (now using LITTLEFS port by lorol)
#ifdef ESP8266
  #define WLED_FS LittleFS
#else
  #if LOROL_LITTLEFS
    #define WLED_FS LITTLEFS
  #else
    #define WLED_FS LittleFS
  #endif
#endif

// GLOBAL VARIABLES
// both declared and defined in header (solution from http://www.keil.com/support/docs/1868.htm)
//
//e.g. byte test = 2 becomes WLED_GLOBAL byte test _INIT(2);
//     int arr[]{0,1,2} becomes WLED_GLOBAL int arr[] _INIT_N(({0,1,2}));

#ifndef WLED_DEFINE_GLOBAL_VARS
# define WLED_GLOBAL extern
# define _INIT(x)
# define _INIT_N(x)
#else
# define WLED_GLOBAL
# define _INIT(x) = x

//needed to ignore commas in array definitions
#define UNPACK( ... ) __VA_ARGS__
# define _INIT_N(x) UNPACK x
#endif

#define STRINGIFY(X) #X
#define TOSTRING(X) STRINGIFY(X)

#ifndef WLED_VERSION
  #define WLED_VERSION "dev"
#endif

// Global Variable definitions
WLED_GLOBAL char versionString[] _INIT(TOSTRING(WLED_VERSION));
#define WLED_CODENAME "Hoshi"

// AP and OTA default passwords (for maximum security change them!)
WLED_GLOBAL char apPass[65]  _INIT(WLED_AP_PASS);
WLED_GLOBAL char otaPass[33] _INIT(DEFAULT_OTA_PASS);

// Hardware and pin config
#ifndef BTNPIN
WLED_GLOBAL int8_t btnPin[WLED_MAX_BUTTONS] _INIT({0});
#else
WLED_GLOBAL int8_t btnPin[WLED_MAX_BUTTONS] _INIT({BTNPIN});
#endif
#ifndef RLYPIN
WLED_GLOBAL int8_t rlyPin _INIT(-1);
#else
WLED_GLOBAL int8_t rlyPin _INIT(RLYPIN);
#endif
//Relay mode (1 = active high, 0 = active low, flipped in cfg.json)
#ifndef RLYMDE
WLED_GLOBAL bool rlyMde _INIT(true);
#else
WLED_GLOBAL bool rlyMde _INIT(RLYMDE);
#endif
#ifndef IRPIN
WLED_GLOBAL int8_t irPin _INIT(-1);
#else
WLED_GLOBAL int8_t irPin _INIT(IRPIN);
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || (defined(RX) && defined(TX))
  // use RX/TX as set by the framework - these boards do _not_ have RX=3 and TX=1
  constexpr uint8_t hardwareRX = RX;
  constexpr uint8_t hardwareTX = TX;
#else
  // use defaults for RX/TX
  constexpr uint8_t hardwareRX = 3;
  constexpr uint8_t hardwareTX = 1;
#endif

//WLED_GLOBAL byte presetToApply _INIT(0);

WLED_GLOBAL char ntpServerName[33] _INIT("0.wled.pool.ntp.org");   // NTP server to use

// WiFi CONFIG (all these can be changed via web UI, no need to set them here)
WLED_GLOBAL char clientSSID[33] _INIT(CLIENT_SSID);
WLED_GLOBAL char clientPass[65] _INIT(CLIENT_PASS);
WLED_GLOBAL char cmDNS[33] _INIT(MDNS_NAME);                       // mDNS address (*.local, replaced by wledXXXXXX if default is used)
WLED_GLOBAL char apSSID[33] _INIT("");                             // AP off by default (unless setup)
WLED_GLOBAL byte apChannel _INIT(1);                               // 2.4GHz WiFi AP channel (1-13)
WLED_GLOBAL byte apHide    _INIT(0);                               // hidden AP SSID
WLED_GLOBAL byte apBehavior _INIT(AP_BEHAVIOR_BOOT_NO_CONN);       // access point opens when no connection after boot by default
WLED_GLOBAL IPAddress staticIP      _INIT_N(((  0,   0,  0,  0))); // static IP of ESP
WLED_GLOBAL IPAddress staticGateway _INIT_N(((  0,   0,  0,  0))); // gateway (router) IP
WLED_GLOBAL IPAddress staticSubnet  _INIT_N(((255, 255, 255, 0))); // most common subnet in home networks
#ifdef ARDUINO_ARCH_ESP32
WLED_GLOBAL bool noWifiSleep _INIT(true);                          // disabling modem sleep modes will increase heat output and power usage, but may help with connection issues
#else
WLED_GLOBAL bool noWifiSleep _INIT(false);
#endif
WLED_GLOBAL bool force802_3g _INIT(false);

#ifdef WLED_USE_ETHERNET
  #ifdef WLED_ETH_DEFAULT                                          // default ethernet board type if specified
    WLED_GLOBAL int ethernetType _INIT(WLED_ETH_DEFAULT);          // ethernet board type
  #else
    WLED_GLOBAL int ethernetType _INIT(WLED_ETH_NONE);             // use none for ethernet board type if default not defined
  #endif
#endif

// LED CONFIG
WLED_GLOBAL bool turnOnAtBoot _INIT(true);                // turn on LEDs at power-up
WLED_GLOBAL byte bootPreset   _INIT(0);                   // save preset to load after power-up

//if true, a segment per bus will be created on boot and LED settings save
//if false, only one segment spanning the total LEDs is created,
//but not on LED settings save if there is more than one segment currently
WLED_GLOBAL bool autoSegments       _INIT(false);
#ifdef ESP8266
WLED_GLOBAL bool useGlobalLedBuffer _INIT(false); // double buffering disabled on ESP8266
#else
WLED_GLOBAL bool useGlobalLedBuffer _INIT(true);  // double buffering enabled on ESP32
#endif
WLED_GLOBAL bool correctWB          _INIT(false); // CCT color correction of RGB color
WLED_GLOBAL bool cctFromRgb         _INIT(false); // CCT is calculated from RGB instead of using seg.cct
WLED_GLOBAL bool gammaCorrectCol    _INIT(true);  // use gamma correction on colors
WLED_GLOBAL bool gammaCorrectBri    _INIT(false); // use gamma correction on brightness
WLED_GLOBAL float gammaCorrectVal   _INIT(2.8f);  // gamma correction value

WLED_GLOBAL byte col[]    _INIT_N(({ 255, 160, 0, 0 }));  // current RGB(W) primary color. col[] should be updated if you want to change the color.
WLED_GLOBAL byte colSec[] _INIT_N(({ 0, 0, 0, 0 }));      // current RGB(W) secondary color
WLED_GLOBAL byte briS     _INIT(128);                     // default brightness

WLED_GLOBAL byte nightlightTargetBri _INIT(0);      // brightness after nightlight is over
WLED_GLOBAL byte nightlightDelayMins _INIT(60);
WLED_GLOBAL byte nightlightMode      _INIT(NL_MODE_FADE); // See const.h for available modes. Was nightlightFade

WLED_GLOBAL byte briMultiplier _INIT(100);          // % of brightness to set (to limit power, if you set it to 50 and set bri to 255, actual brightness will be 127)

// User Interface CONFIG
#ifndef SERVERNAME
WLED_GLOBAL char serverDescription[33] _INIT("WLED");  // Name of module - use default
#else
WLED_GLOBAL char serverDescription[33] _INIT(SERVERNAME);  // use predefined name
#endif
WLED_GLOBAL bool syncToggleReceive     _INIT(false);   // UIs which only have a single button for sync should toggle send+receive if this is true, only send otherwise
WLED_GLOBAL bool simplifiedUI          _INIT(false);   // enable simplified UI
WLED_GLOBAL byte cacheInvalidate       _INIT(0);       // used to invalidate browser cache when switching from regular to simplified UI

// Sync CONFIG
WLED_GLOBAL NodesMap Nodes;
WLED_GLOBAL bool nodeListEnabled _INIT(true);
WLED_GLOBAL bool nodeBroadcastEnabled _INIT(true);

WLED_GLOBAL byte buttonType[WLED_MAX_BUTTONS]  _INIT({BTN_TYPE_PUSH});
#if defined(IRTYPE) && defined(IRPIN)
WLED_GLOBAL byte irEnabled      _INIT(IRTYPE); // Infrared receiver
#else
WLED_GLOBAL byte irEnabled      _INIT(0);     // Infrared receiver disabled
#endif
WLED_GLOBAL bool irApplyToAllSelected _INIT(true); //apply IR to all selected segments

WLED_GLOBAL uint16_t udpPort    _INIT(21324); // WLED notifier default port
WLED_GLOBAL uint16_t udpPort2   _INIT(65506); // WLED notifier supplemental port
WLED_GLOBAL uint16_t udpRgbPort _INIT(19446); // Hyperion port

WLED_GLOBAL uint8_t syncGroups    _INIT(0x01);                    // sync groups this instance syncs (bit mapped)
WLED_GLOBAL uint8_t receiveGroups _INIT(0x01);                    // sync receive groups this instance belongs to (bit mapped)
WLED_GLOBAL bool receiveNotificationBrightness _INIT(true);       // apply brightness from incoming notifications
WLED_GLOBAL bool receiveNotificationColor      _INIT(true);       // apply color
WLED_GLOBAL bool receiveNotificationEffects    _INIT(true);       // apply effects setup
WLED_GLOBAL bool receiveSegmentOptions         _INIT(false);      // apply segment options
WLED_GLOBAL bool receiveSegmentBounds          _INIT(false);      // apply segment bounds (start, stop, offset)
WLED_GLOBAL bool notifyDirect _INIT(false);                       // send notification if change via UI or HTTP API
WLED_GLOBAL bool notifyButton _INIT(false);                       // send if updated by button or infrared remote
WLED_GLOBAL bool notifyAlexa  _INIT(false);                       // send notification if updated via Alexa
WLED_GLOBAL bool notifyMacro  _INIT(false);                       // send notification for macro
WLED_GLOBAL bool notifyHue    _INIT(true);                        // send notification if Hue light changes
WLED_GLOBAL uint8_t udpNumRetries _INIT(0);                       // Number of times a UDP sync message is retransmitted. Increase to increase reliability

WLED_GLOBAL bool alexaEnabled _INIT(false);                       // enable device discovery by Amazon Echo
WLED_GLOBAL char alexaInvocationName[33] _INIT("Light");          // speech control name of device. Choose something voice-to-text can understand
WLED_GLOBAL byte alexaNumPresets _INIT(0);                        // number of presets to expose to Alexa, starting from preset 1, up to 9

WLED_GLOBAL uint16_t realtimeTimeoutMs _INIT(2500);               // ms timeout of realtime mode before returning to normal mode
WLED_GLOBAL int arlsOffset _INIT(0);                              // realtime LED offset
WLED_GLOBAL bool receiveDirect _INIT(true);                       // receive UDP realtime
WLED_GLOBAL bool arlsDisableGammaCorrection _INIT(true);          // activate if gamma correction is handled by the source
WLED_GLOBAL bool arlsForceMaxBri _INIT(false);                    // enable to force max brightness if source has very dark colors that would be black

#ifdef WLED_ENABLE_DMX
 #ifdef ESP8266
  WLED_GLOBAL DMXESPSerial dmx;
 #else //ESP32
  WLED_GLOBAL SparkFunDMX dmx;
 #endif
WLED_GLOBAL uint16_t e131ProxyUniverse _INIT(0);                  // output this E1.31 (sACN) / ArtNet universe via MAX485 (0 = disabled)
#endif
WLED_GLOBAL uint16_t e131Universe _INIT(1);                       // settings for E1.31 (sACN) protocol (only DMX_MODE_MULTIPLE_* can span over consecutive universes)
WLED_GLOBAL uint16_t e131Port _INIT(5568);                        // DMX in port. E1.31 default is 5568, Art-Net is 6454
WLED_GLOBAL byte e131Priority _INIT(0);                           // E1.31 port priority (if != 0 priority handling is active)
WLED_GLOBAL E131Priority highPriority _INIT(3);                   // E1.31 highest priority tracking, init = timeout in seconds
WLED_GLOBAL byte DMXMode _INIT(DMX_MODE_MULTIPLE_RGB);            // DMX mode (s.a.)
WLED_GLOBAL uint16_t DMXAddress _INIT(1);                         // DMX start address of fixture, a.k.a. first Channel [for E1.31 (sACN) protocol]
WLED_GLOBAL uint16_t DMXSegmentSpacing _INIT(0);                  // Number of void/unused channels between each segments DMX channels
WLED_GLOBAL byte e131LastSequenceNumber[E131_MAX_UNIVERSE_COUNT]; // to detect packet loss
WLED_GLOBAL bool e131Multicast _INIT(false);                      // multicast or unicast
WLED_GLOBAL bool e131SkipOutOfSequence _INIT(false);              // freeze instead of flickering
WLED_GLOBAL uint16_t pollReplyCount _INIT(0);                     // count number of replies for ArtPoll node report

// mqtt
WLED_GLOBAL unsigned long lastMqttReconnectAttempt _INIT(0);  // used for other periodic tasks too
#ifndef WLED_DISABLE_MQTT
  #ifndef MQTT_MAX_TOPIC_LEN
    #define MQTT_MAX_TOPIC_LEN 32
  #endif
  #ifndef MQTT_MAX_SERVER_LEN
    #define MQTT_MAX_SERVER_LEN 32
  #endif
WLED_GLOBAL AsyncMqttClient *mqtt _INIT(NULL);
WLED_GLOBAL bool mqttEnabled _INIT(false);
WLED_GLOBAL char mqttStatusTopic[40] _INIT("");            // this must be global because of async handlers
WLED_GLOBAL char mqttDeviceTopic[MQTT_MAX_TOPIC_LEN+1] _INIT("");         // main MQTT topic (individual per device, default is wled/mac)
WLED_GLOBAL char mqttGroupTopic[MQTT_MAX_TOPIC_LEN+1]  _INIT("wled/all"); // second MQTT topic (for example to group devices)
WLED_GLOBAL char mqttServer[MQTT_MAX_SERVER_LEN+1]     _INIT("");         // both domains and IPs should work (no SSL)
WLED_GLOBAL char mqttUser[41] _INIT("");                   // optional: username for MQTT auth
WLED_GLOBAL char mqttPass[65] _INIT("");                   // optional: password for MQTT auth
WLED_GLOBAL char mqttClientID[41] _INIT("");               // override the client ID
WLED_GLOBAL uint16_t mqttPort _INIT(1883);
WLED_GLOBAL bool retainMqttMsg _INIT(false);               // retain brightness and color
#define WLED_MQTT_CONNECTED (mqtt != nullptr && mqtt->connected())
#else
#define WLED_MQTT_CONNECTED false
#endif

#ifndef WLED_DISABLE_HUESYNC
WLED_GLOBAL bool huePollingEnabled _INIT(false);           // poll hue bridge for light state
WLED_GLOBAL uint16_t huePollIntervalMs _INIT(2500);        // low values (< 1sec) may cause lag but offer quicker response
WLED_GLOBAL char hueApiKey[47] _INIT("api");               // key token will be obtained from bridge
WLED_GLOBAL byte huePollLightId _INIT(1);                  // ID of hue lamp to sync to. Find the ID in the hue app ("about" section)
WLED_GLOBAL IPAddress hueIP _INIT_N(((0, 0, 0, 0))); // IP address of the bridge
WLED_GLOBAL bool hueApplyOnOff _INIT(true);
WLED_GLOBAL bool hueApplyBri _INIT(true);
WLED_GLOBAL bool hueApplyColor _INIT(true);
#endif

WLED_GLOBAL uint16_t serialBaud _INIT(1152); // serial baud rate, multiply by 100

#ifndef WLED_DISABLE_ESPNOW
WLED_GLOBAL bool enable_espnow_remote _INIT(false);
WLED_GLOBAL char linked_remote[13]   _INIT("");
WLED_GLOBAL char last_signal_src[13]   _INIT("");
#endif

// Time CONFIG
WLED_GLOBAL bool ntpEnabled _INIT(false);    // get internet time. Only required if you use clock overlays or time-activated macros
WLED_GLOBAL bool useAMPM _INIT(false);       // 12h/24h clock format
WLED_GLOBAL byte currentTimezone _INIT(0);   // Timezone ID. Refer to timezones array in wled10_ntp.ino
WLED_GLOBAL int utcOffsetSecs _INIT(0);      // Seconds to offset from UTC before timzone calculation

WLED_GLOBAL byte overlayCurrent _INIT(0);    // 0: no overlay 1: analog clock 2: was single-digit clock 3: was cronixie
WLED_GLOBAL byte overlayMin _INIT(0), overlayMax _INIT(DEFAULT_LED_COUNT - 1);   // boundaries of overlay mode

WLED_GLOBAL byte analogClock12pixel _INIT(0);               // The pixel in your strip where "midnight" would be
WLED_GLOBAL bool analogClockSecondsTrail _INIT(false);      // Display seconds as trail of LEDs instead of a single pixel
WLED_GLOBAL bool analogClock5MinuteMarks _INIT(false);      // Light pixels at every 5-minute position

WLED_GLOBAL bool countdownMode _INIT(false);                         // Clock will count down towards date
WLED_GLOBAL byte countdownYear _INIT(20), countdownMonth _INIT(1);   // Countdown target date, year is last two digits
WLED_GLOBAL byte countdownDay  _INIT(1) , countdownHour  _INIT(0);
WLED_GLOBAL byte countdownMin  _INIT(0) , countdownSec   _INIT(0);

WLED_GLOBAL byte macroNl   _INIT(0);        // after nightlight delay over
WLED_GLOBAL byte macroCountdown _INIT(0);
WLED_GLOBAL byte macroAlexaOn _INIT(0), macroAlexaOff _INIT(0);
WLED_GLOBAL byte macroButton[WLED_MAX_BUTTONS]        _INIT({0});
WLED_GLOBAL byte macroLongPress[WLED_MAX_BUTTONS]     _INIT({0});
WLED_GLOBAL byte macroDoublePress[WLED_MAX_BUTTONS]   _INIT({0});

// Security CONFIG
WLED_GLOBAL bool otaLock     _INIT(false);  // prevents OTA firmware updates without password. ALWAYS enable if system exposed to any public networks
WLED_GLOBAL bool wifiLock    _INIT(false);  // prevents access to WiFi settings when OTA lock is enabled
WLED_GLOBAL bool aOtaEnabled _INIT(true);   // ArduinoOTA allows easy updates directly from the IDE. Careful, it does not auto-disable when OTA lock is on
WLED_GLOBAL char settingsPIN[5] _INIT("");  // PIN for settings pages
WLED_GLOBAL bool correctPIN     _INIT(true);
WLED_GLOBAL unsigned long lastEditTime _INIT(0);

WLED_GLOBAL uint16_t userVar0 _INIT(0), userVar1 _INIT(0); //available for use in usermod

#ifdef WLED_ENABLE_DMX
  // dmx CONFIG
  WLED_GLOBAL byte DMXChannels _INIT(7);        // number of channels per fixture
  WLED_GLOBAL byte DMXFixtureMap[15] _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
  // assigns the different channels to different functions. See wled21_dmx.ino for more information.
  WLED_GLOBAL uint16_t DMXGap _INIT(10);          // gap between the fixtures. makes addressing easier because you don't have to memorize odd numbers when climbing up onto a rig.
  WLED_GLOBAL uint16_t DMXStart _INIT(10);        // start address of the first fixture
  WLED_GLOBAL uint16_t DMXStartLED _INIT(0);      // LED from which DMX fixtures start
#endif

// internal global variable declarations
// wifi
WLED_GLOBAL bool apActive _INIT(false);
WLED_GLOBAL bool forceReconnect _INIT(false);
WLED_GLOBAL unsigned long lastReconnectAttempt _INIT(0);
WLED_GLOBAL bool interfacesInited _INIT(false);
WLED_GLOBAL bool wasConnected _INIT(false);

// color
WLED_GLOBAL byte lastRandomIndex _INIT(0);        // used to save last random color so the new one is not the same

// transitions
WLED_GLOBAL bool          fadeTransition          _INIT(true);    // enable crossfading brightness/color
WLED_GLOBAL bool          modeBlending            _INIT(true);    // enable effect blending
WLED_GLOBAL bool          transitionActive        _INIT(false);
WLED_GLOBAL uint16_t      transitionDelay         _INIT(750);     // global transition duration
WLED_GLOBAL uint16_t      transitionDelayDefault  _INIT(750);     // default transition time (stored in cfg.json)
WLED_GLOBAL unsigned long transitionStartTime;
WLED_GLOBAL float         tperLast                _INIT(0.0f);    // crossfade transition progress, 0.0f - 1.0f
WLED_GLOBAL bool          jsonTransitionOnce      _INIT(false);   // flag to override transitionDelay (playlist, JSON API: "live" & "seg":{"i"} & "tt")
WLED_GLOBAL uint8_t       randomPaletteChangeTime _INIT(5);       // amount of time [s] between random palette changes (min: 1s, max: 255s)

// nightlight
WLED_GLOBAL bool nightlightActive _INIT(false);
WLED_GLOBAL bool nightlightActiveOld _INIT(false);
WLED_GLOBAL uint32_t nightlightDelayMs _INIT(10);
WLED_GLOBAL byte nightlightDelayMinsDefault _INIT(nightlightDelayMins);
WLED_GLOBAL unsigned long nightlightStartTime;
WLED_GLOBAL byte briNlT _INIT(0);                     // current nightlight brightness
WLED_GLOBAL byte colNlT[] _INIT_N(({ 0, 0, 0, 0 }));        // current nightlight color

// brightness
WLED_GLOBAL unsigned long lastOnTime _INIT(0);
WLED_GLOBAL bool offMode             _INIT(!turnOnAtBoot);
WLED_GLOBAL byte bri                 _INIT(briS);          // global brightness (set)
WLED_GLOBAL byte briOld              _INIT(0);             // global brightness while in transition loop (previous iteration)
WLED_GLOBAL byte briT                _INIT(0);             // global brightness during transition
WLED_GLOBAL byte briLast             _INIT(128);           // brightness before turned off. Used for toggle function
WLED_GLOBAL byte whiteLast           _INIT(128);           // white channel before turned off. Used for toggle function

// button
WLED_GLOBAL bool buttonPublishMqtt                            _INIT(false);
WLED_GLOBAL bool buttonPressedBefore[WLED_MAX_BUTTONS]        _INIT({false});
WLED_GLOBAL bool buttonLongPressed[WLED_MAX_BUTTONS]          _INIT({false});
WLED_GLOBAL unsigned long buttonPressedTime[WLED_MAX_BUTTONS] _INIT({0});
WLED_GLOBAL unsigned long buttonWaitTime[WLED_MAX_BUTTONS]    _INIT({0});
WLED_GLOBAL bool disablePullUp                                _INIT(false);
WLED_GLOBAL byte touchThreshold                               _INIT(TOUCH_THRESHOLD);

// notifications
WLED_GLOBAL bool notifyDirectDefault _INIT(notifyDirect);
WLED_GLOBAL bool receiveNotifications _INIT(true);
WLED_GLOBAL unsigned long notificationSentTime _INIT(0);
WLED_GLOBAL byte notificationSentCallMode _INIT(CALL_MODE_INIT);
WLED_GLOBAL uint8_t notificationCount _INIT(0);

// effects
WLED_GLOBAL byte effectCurrent _INIT(0);
WLED_GLOBAL byte effectSpeed _INIT(128);
WLED_GLOBAL byte effectIntensity _INIT(128);
WLED_GLOBAL byte effectPalette _INIT(0);
WLED_GLOBAL bool stateChanged _INIT(false);

// network
WLED_GLOBAL bool udpConnected _INIT(false), udp2Connected _INIT(false), udpRgbConnected _INIT(false);

// ui style
WLED_GLOBAL bool showWelcomePage _INIT(false);

// hue
WLED_GLOBAL byte hueError _INIT(HUE_ERROR_INACTIVE);
// WLED_GLOBAL uint16_t hueFailCount _INIT(0);
WLED_GLOBAL float hueXLast _INIT(0), hueYLast _INIT(0);
WLED_GLOBAL uint16_t hueHueLast _INIT(0), hueCtLast _INIT(0);
WLED_GLOBAL byte hueSatLast _INIT(0), hueBriLast _INIT(0);
WLED_GLOBAL unsigned long hueLastRequestSent _INIT(0);
WLED_GLOBAL bool hueAuthRequired _INIT(false);
WLED_GLOBAL bool hueReceived _INIT(false);
WLED_GLOBAL bool hueStoreAllowed _INIT(false), hueNewKey _INIT(false);

// countdown
WLED_GLOBAL unsigned long countdownTime _INIT(1514764800L);
WLED_GLOBAL bool countdownOverTriggered _INIT(true);

//timer
WLED_GLOBAL byte lastTimerMinute  _INIT(0);
WLED_GLOBAL byte timerHours[]     _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
WLED_GLOBAL int8_t timerMinutes[] _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
WLED_GLOBAL byte timerMacro[]     _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
//weekdays to activate on, bit pattern of arr elem: 0b11111111: sun,sat,fri,thu,wed,tue,mon,validity
WLED_GLOBAL byte timerWeekday[]   _INIT_N(({ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }));
//upper 4 bits start, lower 4 bits end month (default 28: start month 1 and end month 12)
WLED_GLOBAL byte timerMonth[]     _INIT_N(({28,28,28,28,28,28,28,28}));
WLED_GLOBAL byte timerDay[]       _INIT_N(({1,1,1,1,1,1,1,1}));
WLED_GLOBAL byte timerDayEnd[]		_INIT_N(({31,31,31,31,31,31,31,31}));

//improv
WLED_GLOBAL byte improvActive _INIT(0); //0: no improv packet received, 1: improv active, 2: provisioning
WLED_GLOBAL byte improvError _INIT(0);

//playlists
WLED_GLOBAL int16_t currentPlaylist _INIT(-1);
//still used for "PL=~" HTTP API command
WLED_GLOBAL byte presetCycCurr _INIT(0);
WLED_GLOBAL byte presetCycMin _INIT(1);
WLED_GLOBAL byte presetCycMax _INIT(5);

// realtime
WLED_GLOBAL byte realtimeMode _INIT(REALTIME_MODE_INACTIVE);
WLED_GLOBAL byte realtimeOverride _INIT(REALTIME_OVERRIDE_NONE);
WLED_GLOBAL IPAddress realtimeIP _INIT_N(((0, 0, 0, 0)));
WLED_GLOBAL unsigned long realtimeTimeout _INIT(0);
WLED_GLOBAL uint8_t tpmPacketCount _INIT(0);
WLED_GLOBAL uint16_t tpmPayloadFrameSize _INIT(0);
WLED_GLOBAL bool useMainSegmentOnly _INIT(false);

WLED_GLOBAL unsigned long lastInterfaceUpdate _INIT(0);
WLED_GLOBAL byte interfaceUpdateCallMode _INIT(CALL_MODE_INIT);

// alexa udp
WLED_GLOBAL String escapedMac;
#ifndef WLED_DISABLE_ALEXA
  WLED_GLOBAL Espalexa espalexa;
  WLED_GLOBAL EspalexaDevice* espalexaDevice;
#endif

// dns server
WLED_GLOBAL DNSServer dnsServer;

// network time
#define NTP_NEVER 999000000L
WLED_GLOBAL bool ntpConnected _INIT(false);
WLED_GLOBAL time_t localTime _INIT(0);
WLED_GLOBAL unsigned long ntpLastSyncTime _INIT(NTP_NEVER);
WLED_GLOBAL unsigned long ntpPacketSentTime _INIT(NTP_NEVER);
WLED_GLOBAL IPAddress ntpServerIP;
WLED_GLOBAL uint16_t ntpLocalPort _INIT(2390);
WLED_GLOBAL uint16_t rolloverMillis _INIT(0);
WLED_GLOBAL float longitude _INIT(0.0);
WLED_GLOBAL float latitude _INIT(0.0);
WLED_GLOBAL time_t sunrise _INIT(0);
WLED_GLOBAL time_t sunset _INIT(0);
WLED_GLOBAL Toki toki _INIT(Toki());

// Temp buffer
WLED_GLOBAL char* obuf;
WLED_GLOBAL uint16_t olen _INIT(0);

// General filesystem
WLED_GLOBAL size_t fsBytesUsed _INIT(0);
WLED_GLOBAL size_t fsBytesTotal _INIT(0);
WLED_GLOBAL unsigned long presetsModifiedTime _INIT(0L);
WLED_GLOBAL JsonDocument* fileDoc;
WLED_GLOBAL bool doCloseFile _INIT(false);

// presets
WLED_GLOBAL byte currentPreset _INIT(0);

WLED_GLOBAL byte errorFlag _INIT(0);

WLED_GLOBAL String messageHead, messageSub;
WLED_GLOBAL byte optionType;

WLED_GLOBAL bool doSerializeConfig _INIT(false);        // flag to initiate saving of config
WLED_GLOBAL bool doReboot          _INIT(false);        // flag to initiate reboot from async handlers
WLED_GLOBAL bool doPublishMqtt     _INIT(false);

// status led
#if defined(STATUSLED)
WLED_GLOBAL unsigned long ledStatusLastMillis _INIT(0);
WLED_GLOBAL uint8_t ledStatusType _INIT(0); // current status type - corresponds to number of blinks per second
WLED_GLOBAL bool ledStatusState _INIT(false); // the current LED state
#endif

// server library objects
WLED_GLOBAL AsyncWebServer server _INIT_N(((80)));
#ifdef WLED_ENABLE_WEBSOCKETS
WLED_GLOBAL AsyncWebSocket ws _INIT_N((("/ws")));
#endif
WLED_GLOBAL AsyncClient     *hueClient _INIT(NULL);
WLED_GLOBAL AsyncWebHandler *editHandler _INIT(nullptr);

// udp interface objects
WLED_GLOBAL WiFiUDP notifierUdp, rgbUdp, notifier2Udp;
WLED_GLOBAL WiFiUDP ntpUdp;
WLED_GLOBAL ESPAsyncE131 e131 _INIT_N(((handleE131Packet)));
WLED_GLOBAL ESPAsyncE131 ddp  _INIT_N(((handleE131Packet)));
WLED_GLOBAL bool e131NewData _INIT(false);

// led fx library object
WLED_GLOBAL BusManager busses _INIT(BusManager());
WLED_GLOBAL WS2812FX strip _INIT(WS2812FX());
WLED_GLOBAL BusConfig* busConfigs[WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES] _INIT({nullptr}); //temporary, to remember values from network callback until after
WLED_GLOBAL bool doInitBusses _INIT(false);
WLED_GLOBAL int8_t loadLedmap _INIT(-1);
#ifndef ESP8266
WLED_GLOBAL char  *ledmapNames[WLED_MAX_LEDMAPS-1] _INIT_N(({nullptr}));
#endif
#if WLED_MAX_LEDMAPS>16
WLED_GLOBAL uint32_t ledMaps _INIT(0); // bitfield representation of available ledmaps
#else
WLED_GLOBAL uint16_t ledMaps _INIT(0); // bitfield representation of available ledmaps
#endif

// Usermod manager
WLED_GLOBAL UsermodManager usermods _INIT(UsermodManager());

// global I2C SDA pin (used for usermods)
#ifndef I2CSDAPIN
WLED_GLOBAL int8_t i2c_sda  _INIT(-1);
#else
WLED_GLOBAL int8_t i2c_sda  _INIT(I2CSDAPIN);
#endif
// global I2C SCL pin (used for usermods)
#ifndef I2CSCLPIN
WLED_GLOBAL int8_t i2c_scl  _INIT(-1);
#else
WLED_GLOBAL int8_t i2c_scl  _INIT(I2CSCLPIN);
#endif

// global SPI DATA/MOSI pin (used for usermods)
#ifndef SPIMOSIPIN
WLED_GLOBAL int8_t spi_mosi  _INIT(-1);
#else
WLED_GLOBAL int8_t spi_mosi  _INIT(SPIMOSIPIN);
#endif
// global SPI DATA/MISO pin (used for usermods)
#ifndef SPIMISOPIN
WLED_GLOBAL int8_t spi_miso  _INIT(-1);
#else
WLED_GLOBAL int8_t spi_miso  _INIT(SPIMISOPIN);
#endif
// global SPI CLOCK/SCLK pin (used for usermods)
#ifndef SPISCLKPIN
WLED_GLOBAL int8_t spi_sclk  _INIT(-1);
#else
WLED_GLOBAL int8_t spi_sclk  _INIT(SPISCLKPIN);
#endif

// global ArduinoJson buffer
WLED_GLOBAL StaticJsonDocument<JSON_BUFFER_SIZE> doc;
WLED_GLOBAL volatile uint8_t jsonBufferLock _INIT(0);

// enable additional debug output
#if defined(WLED_DEBUG_HOST)
  #include "net_debug.h"
  // On the host side, use netcat to receive the log statements: nc -l 7868 -u
  // use -D WLED_DEBUG_HOST='"192.168.xxx.xxx"' or FQDN within quotes
  #define DEBUGOUT NetDebug
  WLED_GLOBAL bool netDebugEnabled _INIT(true);
  WLED_GLOBAL char netDebugPrintHost[33] _INIT(WLED_DEBUG_HOST);
  #ifndef WLED_DEBUG_PORT
    #define WLED_DEBUG_PORT 7868
  #endif
  WLED_GLOBAL int netDebugPrintPort _INIT(WLED_DEBUG_PORT);
#else
  #define DEBUGOUT Serial
#endif

#ifdef WLED_DEBUG
  #ifndef ESP8266
  #include <rom/rtc.h>
  #endif
  #define DEBUG_PRINT(x) DEBUGOUT.print(x)
  #define DEBUG_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUG_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
#endif

#ifdef WLED_DEBUG_FS
  #define DEBUGFS_PRINT(x) DEBUGOUT.print(x)
  #define DEBUGFS_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUGFS_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUGFS_PRINT(x)
  #define DEBUGFS_PRINTLN(x)
  #define DEBUGFS_PRINTF(x...)
#endif

// debug macro variable definitions
#ifdef WLED_DEBUG
  WLED_GLOBAL unsigned long debugTime _INIT(0);
  WLED_GLOBAL int lastWifiState _INIT(3);
  WLED_GLOBAL unsigned long wifiStateChangedTime _INIT(0);
  WLED_GLOBAL unsigned long loops _INIT(0);
#endif

#ifdef ARDUINO_ARCH_ESP32
  #define WLED_CONNECTED (WiFi.status() == WL_CONNECTED || ETH.localIP()[0] != 0)
#else
  #define WLED_CONNECTED (WiFi.status() == WL_CONNECTED)
#endif
#define WLED_WIFI_CONFIGURED (strlen(clientSSID) >= 1 && strcmp(clientSSID, DEFAULT_CLIENT_SSID) != 0)

#ifndef WLED_AP_SSID_UNIQUE
  #define WLED_SET_AP_SSID() do { \
    strcpy_P(apSSID, PSTR(WLED_AP_SSID)); \
  } while(0)
#else
  #define WLED_SET_AP_SSID() do { \
    strcpy_P(apSSID, PSTR(WLED_AP_SSID)); \
    snprintf_P(\
      apSSID+strlen(WLED_AP_SSID), \
      sizeof(apSSID)-strlen(WLED_AP_SSID), \
      PSTR("-%*s"), \
      6, \
      escapedMac.c_str() + 6\
    ); \
  } while(0)
#endif

//macro to convert F to const
#define SET_F(x)  (const char*)F(x)

//color mangling macros
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))

class WLED {
public:
  WLED();
  static WLED& instance()
  {
    static WLED instance;
    return instance;
  }

  // boot starts here
  void setup();

  void loop();
  void reset();

  void beginStrip();
  void handleConnection();
  bool initEthernet(); // result is informational
  void initAP(bool resetAP = false);
  void initConnection();
  void initInterfaces();
  void handleStatusLED();
  void enableWatchdog();
  void disableWatchdog();
};
#endif        // WLED_H
