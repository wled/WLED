#ifndef WLED_FCN_DECLARE_H
#define WLED_FCN_DECLARE_H

/*
 * All globally accessible functions are declared here
 */

//alexa.cpp
#ifndef WLED_DISABLE_ALEXA
void onAlexaChange(EspalexaDevice* dev);
void alexaInit();
void handleAlexa();
void onAlexaChange(EspalexaDevice* dev);
#endif

//button.cpp
void shortPressAction(uint8_t b=0);
void longPressAction(uint8_t b=0);
void doublePressAction(uint8_t b=0);
bool isButtonPressed(uint8_t b=0);
void handleButton();
void handleIO();
void IRAM_ATTR touchButtonISR();

//cfg.cpp
bool deserializeConfig(JsonObject doc, bool fromFS = false);
void deserializeConfigFromFS();
bool deserializeConfigSec();
void serializeConfig();
void serializeConfigSec();

template<typename DestType>
bool getJsonValue(const JsonVariant& element, DestType& destination) {
  if (element.isNull()) {
    return false;
  }

  destination = element.as<DestType>();
  return true;
}

template<typename DestType, typename DefaultType>
bool getJsonValue(const JsonVariant& element, DestType& destination, const DefaultType defaultValue) {
  if(!getJsonValue(element, destination)) {
    destination = defaultValue;
    return false;
  }

  return true;
}

typedef struct WiFiConfig {
  char clientSSID[33];
  char clientPass[65];
  IPAddress staticIP;
  IPAddress staticGW;
  IPAddress staticSN;
  WiFiConfig(const char *ssid="", const char *pass="", uint32_t ip=0, uint32_t gw=0, uint32_t subnet=0x00FFFFFF) // little endian
  : staticIP(ip)
  , staticGW(gw)
  , staticSN(subnet)
  {
    strncpy(clientSSID, ssid, 32); clientSSID[32] = 0;
    strncpy(clientPass, pass, 64); clientPass[64] = 0;
  }
} wifi_config;

//colors.cpp
// similar to NeoPixelBus NeoGammaTableMethod but allows dynamic changes (superseded by NPB::NeoGammaDynamicTableMethod)
class NeoGammaWLEDMethod {
  public:
    [[gnu::hot]] static uint8_t Correct(uint8_t value);         // apply Gamma to single channel
    [[gnu::hot]] static uint32_t Correct32(uint32_t color);     // apply Gamma to RGBW32 color (WLED specific, not used by NPB)
    static void calcGammaTable(float gamma);                              // re-calculates & fills gamma table
    static inline uint8_t rawGamma8(uint8_t val) { return gammaT[val]; }  // get value from Gamma table (WLED specific, not used by NPB)
  private:
    static uint8_t gammaT[];
};
#define gamma32(c) NeoGammaWLEDMethod::Correct32(c)
#define gamma8(c)  NeoGammaWLEDMethod::rawGamma8(c)
[[gnu::hot]] uint32_t color_blend(uint32_t,uint32_t,uint16_t,bool b16=false);
[[gnu::hot]] uint32_t color_add(uint32_t,uint32_t, bool fast=false);
[[gnu::hot]] uint32_t color_fade(uint32_t c1, uint8_t amount, bool video=false);
CRGBPalette16 generateHarmonicRandomPalette(CRGBPalette16 &basepalette);
CRGBPalette16 generateRandomPalette();
inline uint32_t colorFromRgbw(byte* rgbw) { return uint32_t((byte(rgbw[3]) << 24) | (byte(rgbw[0]) << 16) | (byte(rgbw[1]) << 8) | (byte(rgbw[2]))); }
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb); //hue, sat to rgb
void colorKtoRGB(uint16_t kelvin, byte* rgb);
void colorCTtoRGB(uint16_t mired, byte* rgb); //white spectrum to rgb
void colorXYtoRGB(float x, float y, byte* rgb); // only defined if huesync disabled TODO
void colorRGBtoXY(byte* rgb, float* xy); // only defined if huesync disabled TODO
void colorFromDecOrHexString(byte* rgb, char* in);
bool colorFromHexString(byte* rgb, const char* in);
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t approximateKelvinFromRGB(uint32_t rgb);
void setRandomColor(byte* rgb);

//dmx.cpp
void initDMX();
void handleDMX();

//e131.cpp
void handleE131Packet(e131_packet_t* p, IPAddress clientIP, byte protocol);
void handleArtnetPollReply(IPAddress ipAddress);
void prepareArtnetPollReply(ArtPollReply* reply);
void sendArtnetPollReply(ArtPollReply* reply, IPAddress ipAddress, uint16_t portAddress);

//file.cpp
bool handleFileRead(AsyncWebServerRequest*, String path);
bool writeObjectToFileUsingId(const char* file, uint16_t id, JsonDocument* content);
bool writeObjectToFile(const char* file, const char* key, JsonDocument* content);
bool readObjectFromFileUsingId(const char* file, uint16_t id, JsonDocument* dest);
bool readObjectFromFile(const char* file, const char* key, JsonDocument* dest);
void updateFSInfo();
void closeFile();
inline bool writeObjectToFileUsingId(const String &file, uint16_t id, JsonDocument* content) { return writeObjectToFileUsingId(file.c_str(), id, content); };
inline bool writeObjectToFile(const String &file, const char* key, JsonDocument* content) { return writeObjectToFile(file.c_str(), key, content); };
inline bool readObjectFromFileUsingId(const String &file, uint16_t id, JsonDocument* dest) { return readObjectFromFileUsingId(file.c_str(), id, dest); };
inline bool readObjectFromFile(const String &file, const char* key, JsonDocument* dest) { return readObjectFromFile(file.c_str(), key, dest); };

//hue.cpp
void handleHue();
void reconnectHue();
void onHueError(void* arg, AsyncClient* client, int8_t error);
void onHueConnect(void* arg, AsyncClient* client);
void sendHuePoll();
void onHueData(void* arg, AsyncClient* client, void *data, size_t len);

//improv.cpp
enum ImprovRPCType {
  Command_Wifi = 0x01,
  Request_State = 0x02,
  Request_Info = 0x03,
  Request_Scan = 0x04
};

void handleImprovPacket();
void sendImprovRPCResult(ImprovRPCType type, uint8_t n_strings = 0, const char **strings = nullptr);
void sendImprovStateResponse(uint8_t state, bool error = false);
void sendImprovInfoResponse();
void startImprovWifiScan();
void handleImprovWifiScan();
void sendImprovIPRPCResult(ImprovRPCType type);

//ir.cpp
void initIR();
void deInitIR();
void handleIR();

//json.cpp
#include "ESPAsyncWebServer.h"
#include "src/dependencies/json/ArduinoJson-v6.h"
#include "src/dependencies/json/AsyncJson-v6.h"
#include "FX.h"

bool deserializeSegment(JsonObject elem, byte it, byte presetId = 0);
bool deserializeState(JsonObject root, byte callMode = CALL_MODE_DIRECT_CHANGE, byte presetId = 0);
void serializeSegment(JsonObject& root, Segment& seg, byte id, bool forPreset = false, bool segmentBounds = true);
void serializeState(JsonObject root, bool forPreset = false, bool includeBri = true, bool segmentBounds = true, bool selectedSegmentsOnly = false);
void serializeInfo(JsonObject root);
void serializeModeNames(JsonArray root);
void serializeModeData(JsonArray root);
void serveJson(AsyncWebServerRequest* request);
#ifdef WLED_ENABLE_JSONLIVE
bool serveLiveLeds(AsyncWebServerRequest* request, uint32_t wsClient = 0);
#endif

//led.cpp
void setValuesFromSegment(uint8_t s);
void setValuesFromMainSeg();
void setValuesFromFirstSelectedSeg();
void toggleOnOff();
void applyBri();
void applyFinalBri();
void applyValuesToSelectedSegs();
void colorUpdated(byte callMode);
void stateUpdated(byte callMode);
void updateInterfaces(uint8_t callMode);
void handleTransitions();
void handleNightlight();
byte scaledBri(byte in);

#ifdef WLED_ENABLE_LOXONE
//lx_parser.cpp
bool parseLx(int lxValue, byte* rgbw);
void parseLxJson(int lxValue, byte segId, bool secondary);
#endif

//mqtt.cpp
bool initMqtt();
void publishMqtt();

//ntp.cpp
void handleTime();
void handleNetworkTime();
void sendNTPPacket();
bool checkNTPResponse();
void updateLocalTime();
void getTimeString(char* out);
bool checkCountdown();
void setCountdown();
byte weekdayMondayFirst();
void checkTimers();
void calculateSunriseAndSunset();
void setTimeFromAPI(uint32_t timein);

//overlay.cpp
void handleOverlayDraw();
void _overlayAnalogCountdown();
void _overlayAnalogClock();

//playlist.cpp
void shufflePlaylist();
void unloadPlaylist();
int16_t loadPlaylist(JsonObject playlistObject, byte presetId = 0);
void handlePlaylist();
void serializePlaylist(JsonObject obj);

//presets.cpp
const char *getPresetsFileName(bool persistent = true);
void initPresetsFile();
void handlePresets();
bool applyPreset(byte index, byte callMode = CALL_MODE_DIRECT_CHANGE);
bool applyPresetFromPlaylist(byte index);
void applyPresetWithFallback(uint8_t presetID, uint8_t callMode, uint8_t effectID = 0, uint8_t paletteID = 0);
inline bool applyTemporaryPreset() {return applyPreset(255);};
void savePreset(byte index, const char* pname = nullptr, JsonObject saveobj = JsonObject());
inline void saveTemporaryPreset() {savePreset(255);};
void deletePreset(byte index);
bool getPresetName(byte index, String& name);

//remote.cpp
void handleRemote(uint8_t *data, size_t len);

//set.cpp
bool isAsterisksOnly(const char* str, byte maxLen);
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage);
bool handleSet(AsyncWebServerRequest *request, const String& req, bool apply=true);

//udp.cpp
void notify(byte callMode, bool followUp=false);
uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, uint8_t *buffer, uint8_t bri=255, bool isRGBW=false);
void realtimeLock(uint32_t timeoutMs, byte md = REALTIME_MODE_GENERIC);
void exitRealtime();
void handleNotifications();
void setRealtimePixel(uint16_t i, byte r, byte g, byte b, byte w);
void refreshNodeList();
void sendSysInfoUDP();
#ifndef WLED_DISABLE_ESPNOW
void espNowSentCB(uint8_t* address, uint8_t status);
void espNowReceiveCB(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast);
#endif

//network.cpp
int getSignalQuality(int rssi);
void WiFiEvent(WiFiEvent_t event);

//um_manager.cpp
typedef enum UM_Data_Types {
  UMT_BYTE = 0,
  UMT_UINT16,
  UMT_INT16,
  UMT_UINT32,
  UMT_INT32,
  UMT_FLOAT,
  UMT_DOUBLE,
  UMT_BYTE_ARR,
  UMT_UINT16_ARR,
  UMT_INT16_ARR,
  UMT_UINT32_ARR,
  UMT_INT32_ARR,
  UMT_FLOAT_ARR,
  UMT_DOUBLE_ARR
} um_types_t;
typedef struct UM_Exchange_Data {
  // should just use: size_t arr_size, void **arr_ptr, byte *ptr_type
  size_t       u_size;                 // size of u_data array
  um_types_t  *u_type;                 // array of data types
  void       **u_data;                 // array of pointers to data
  UM_Exchange_Data() {
    u_size = 0;
    u_type = nullptr;
    u_data = nullptr;
  }
  ~UM_Exchange_Data() {
    if (u_type) delete[] u_type;
    if (u_data) delete[] u_data;
  }
} um_data_t;
const unsigned int um_data_size = sizeof(um_data_t);  // 12 bytes

class Usermod {
  protected:
    um_data_t *um_data; // um_data should be allocated using new in (derived) Usermod's setup() or constructor
  public:
    Usermod() { um_data = nullptr; }
    virtual ~Usermod() { if (um_data) delete um_data; }
    virtual void setup() = 0; // pure virtual, has to be overriden
    virtual void loop() = 0;  // pure virtual, has to be overriden
    virtual void handleOverlayDraw() {}                                      // called after all effects have been processed, just before strip.show()
    virtual bool handleButton(uint8_t b) { return false; }                   // button overrides are possible here
    virtual bool getUMData(um_data_t **data) { if (data) *data = nullptr; return false; }; // usermod data exchange [see examples for audio effects]
    virtual void connected() {}                                              // called when WiFi is (re)connected
    virtual void appendConfigData(Print& settingsScript);                    // helper function called from usermod settings page to add metadata for entry fields
    virtual void addToJsonState(JsonObject& obj) {}                          // add JSON objects for WLED state
    virtual void addToJsonInfo(JsonObject& obj) {}                           // add JSON objects for UI Info page
    virtual void readFromJsonState(JsonObject& obj) {}                       // process JSON messages received from web server
    virtual void addToConfig(JsonObject& obj) {}                             // add JSON entries that go to cfg.json
    virtual bool readFromConfig(JsonObject& obj) { return true; } // Note as of 2021-06 readFromConfig() now needs to return a bool, see usermod_v2_example.h
    virtual void onMqttConnect(bool sessionPresent) {}                       // fired when MQTT connection is established (so usermod can subscribe)
    virtual bool onMqttMessage(char* topic, char* payload) { return false; } // fired upon MQTT message received (wled topic)
    virtual bool onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) { return false; } // fired upon ESP-NOW message received
    virtual void onUpdateBegin(bool) {}                                      // fired prior to and after unsuccessful firmware update
    virtual void onStateChange(uint8_t mode) {}                              // fired upon WLED state change
    virtual uint16_t getId() {return USERMOD_ID_UNSPECIFIED;}

  // API shims
  private:
    static Print* oappend_shim;
    // old form of appendConfigData; called by default appendConfigData(Print&) with oappend_shim set up
    // private so it is not accidentally invoked except via Usermod::appendConfigData(Print&)
    virtual void appendConfigData() {}    
  protected:
    // Shim for oappend(), which used to exist in utils.cpp
    template<typename T> static inline void oappend(const T& t) { oappend_shim->print(t); };
#ifdef ESP8266
    // Handle print(PSTR()) without crashing by detecting PROGMEM strings
    static void oappend(const char* c) { if ((intptr_t) c >= 0x40000000) oappend_shim->print(FPSTR(c)); else oappend_shim->print(c); };
#endif
};

class UsermodManager {
  private:
    static Usermod* ums[WLED_MAX_USERMODS];
    static byte numMods;

  public:
    static void loop();
    static void handleOverlayDraw();
    static bool handleButton(uint8_t b);
    static bool getUMData(um_data_t **um_data, uint8_t mod_id = USERMOD_ID_RESERVED); // USERMOD_ID_RESERVED will poll all usermods
    static void setup();
    static void connected();
    static void appendConfigData(Print&);
    static void addToJsonState(JsonObject& obj);
    static void addToJsonInfo(JsonObject& obj);
    static void readFromJsonState(JsonObject& obj);
    static void addToConfig(JsonObject& obj);
    static bool readFromConfig(JsonObject& obj);
#ifndef WLED_DISABLE_MQTT
    static void onMqttConnect(bool sessionPresent);
    static bool onMqttMessage(char* topic, char* payload);
#endif
#ifndef WLED_DISABLE_ESPNOW
    static bool onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len);
#endif
    static void onUpdateBegin(bool);
    static void onStateChange(uint8_t);
    static bool add(Usermod* um);
    static Usermod* lookup(uint16_t mod_id);
    static inline byte getModCount() {return numMods;};
};

//usermods_list.cpp
void registerUsermods();

//usermod.cpp
void userSetup();
void userConnected();
void userLoop();

//util.cpp
int getNumVal(const String* req, uint16_t pos);
void parseNumber(const char* str, byte* val, byte minv=0, byte maxv=255);
bool getVal(JsonVariant elem, byte* val, byte minv=0, byte maxv=255);
bool getBoolVal(JsonVariant elem, bool dflt);
bool updateVal(const char* req, const char* key, byte* val, byte minv=0, byte maxv=255);
size_t printSetFormCheckbox(Print& settingsScript, const char* key, int val);
size_t printSetFormValue(Print& settingsScript, const char* key, int val);
size_t printSetFormValue(Print& settingsScript, const char* key, const char* val);
size_t printSetFormIndex(Print& settingsScript, const char* key, int index);
size_t printSetClassElementHTML(Print& settingsScript, const char* key, const int index, const char* val);
void prepareHostname(char* hostname);
bool isAsterisksOnly(const char* str, byte maxLen);
bool requestJSONBufferLock(uint8_t module=255);
void releaseJSONBufferLock();
uint8_t extractModeName(uint8_t mode, const char *src, char *dest, uint8_t maxLen);
uint8_t extractModeSlider(uint8_t mode, uint8_t slider, char *dest, uint8_t maxLen, uint8_t *var = nullptr);
int16_t extractModeDefaults(uint8_t mode, const char *segVar);
void checkSettingsPIN(const char *pin);
uint16_t crc16(const unsigned char* data_p, size_t length);
um_data_t* simulateSound(uint8_t simulationId);
void enumerateLedmaps();
uint8_t get_random_wheel_index(uint8_t pos);
float mapf(float x, float in_min, float in_max, float out_min, float out_max);

// RAII guard class for the JSON Buffer lock
// Modeled after std::lock_guard
class JSONBufferGuard {
  bool holding_lock;
  public:
    inline JSONBufferGuard(uint8_t module=255) : holding_lock(requestJSONBufferLock(module)) {};
    inline ~JSONBufferGuard() { if (holding_lock) releaseJSONBufferLock(); };
    inline JSONBufferGuard(const JSONBufferGuard&) = delete; // Noncopyable
    inline JSONBufferGuard& operator=(const JSONBufferGuard&) = delete;
    inline JSONBufferGuard(JSONBufferGuard&& r) : holding_lock(r.holding_lock) { r.holding_lock = false; };  // but movable
    inline JSONBufferGuard& operator=(JSONBufferGuard&& r) { holding_lock |= r.holding_lock; r.holding_lock = false; return *this; };
    inline bool owns_lock() const { return holding_lock; }
    explicit inline operator bool() const { return owns_lock(); };
    inline void release() { if (holding_lock) releaseJSONBufferLock(); holding_lock = false; }
};

#ifdef WLED_ADD_EEPROM_SUPPORT
//wled_eeprom.cpp
void applyMacro(byte index);
void deEEP();
void deEEPSettings();
void clearEEPROM();
#endif

//wled_math.cpp
#if defined(ESP8266) && !defined(WLED_USE_REAL_MATH)
  template <typename T> T atan_t(T x);
  float cos_t(float phi);
  float sin_t(float x);
  float tan_t(float x);
  float acos_t(float x);
  float asin_t(float x);
  float floor_t(float x);
  float fmod_t(float num, float denom);
#else
  #include <math.h>
  #define sin_t sinf
  #define cos_t cosf
  #define tan_t tanf
  #define asin_t asinf
  #define acos_t acosf
  #define atan_t atanf
  #define fmod_t fmodf
  #define floor_t floorf
#endif

//wled_serial.cpp
void handleSerial();
void updateBaudRate(uint32_t rate);

//wled_server.cpp
void createEditHandler(bool enable);
void initServer();
void serveMessage(AsyncWebServerRequest* request, uint16_t code, const String& headl, const String& subl="", byte optionT=255);
void serveJsonError(AsyncWebServerRequest* request, uint16_t code, uint16_t error);
void serveSettings(AsyncWebServerRequest* request, bool post = false);
void serveSettingsJS(AsyncWebServerRequest* request);

//ws.cpp
void handleWs();
void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
void sendDataWs(AsyncWebSocketClient * client = nullptr);

//xml.cpp
void XML_response(Print& dest);
void getSettingsJS(byte subPage, Print& dest);

#endif
