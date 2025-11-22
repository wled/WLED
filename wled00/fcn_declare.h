#pragma once
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
bool backupConfig();
bool restoreConfig();
bool verifyConfig();
bool configBackupExists();
void resetConfig();
bool deserializeConfig(JsonObject doc, bool fromFS = false);
bool deserializeConfigFromFS();
bool deserializeConfigSec();
void serializeConfig(JsonObject doc);
void serializeConfigToFS();
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
  uint8_t bssid[6];
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
    memset(bssid, 0, sizeof(bssid));
  }
} wifi_config;

//dmx_output.cpp
void initDMXOutput();
void handleDMXOutput();

//dmx_input.cpp
void initDMXInput();
void handleDMXInput();

//e131.cpp
void handleE131Packet(e131_packet_t* p, IPAddress clientIP, byte protocol);
void handleDMXData(uint16_t uni, uint16_t dmxChannels, uint8_t* e131_data, uint8_t mde, uint8_t previousUniverses);
void handleArtnetPollReply(IPAddress ipAddress);
void prepareArtnetPollReply(ArtPollReply* reply);
void sendArtnetPollReply(ArtPollReply* reply, IPAddress ipAddress, uint16_t portAddress);

//file.cpp
bool handleFileRead(AsyncWebServerRequest*, String path);
bool writeObjectToFileUsingId(const char* file, uint16_t id, const JsonDocument* content);
bool writeObjectToFile(const char* file, const char* key, const JsonDocument* content);
bool readObjectFromFileUsingId(const char* file, uint16_t id, JsonDocument* dest, const JsonDocument* filter = nullptr);
bool readObjectFromFile(const char* file, const char* key, JsonDocument* dest, const JsonDocument* filter = nullptr);
void updateFSInfo();
void closeFile();
inline bool writeObjectToFileUsingId(const String &file, uint16_t id, const JsonDocument* content) { return writeObjectToFileUsingId(file.c_str(), id, content); };
inline bool writeObjectToFile(const String &file, const char* key, const JsonDocument* content) { return writeObjectToFile(file.c_str(), key, content); };
inline bool readObjectFromFileUsingId(const String &file, uint16_t id, JsonDocument* dest, const JsonDocument* filter = nullptr) { return readObjectFromFileUsingId(file.c_str(), id, dest); };
inline bool readObjectFromFile(const String &file, const char* key, JsonDocument* dest, const JsonDocument* filter = nullptr) { return readObjectFromFile(file.c_str(), key, dest); };
bool copyFile(const char* src_path, const char* dst_path);
bool backupFile(const char* filename);
bool restoreFile(const char* filename);
bool checkBackupExists(const char* filename);
bool validateJsonFile(const char* filename);
void dumpFilesToSerial();

//hue.cpp
void handleHue();
void reconnectHue();
void onHueError(void* arg, AsyncClient* client, int8_t error);
void onHueConnect(void* arg, AsyncClient* client);
void sendHuePoll();
void onHueData(void* arg, AsyncClient* client, void *data, size_t len);

//image_loader.cpp
class Segment;
#ifdef WLED_ENABLE_GIF
bool fileSeekCallback(unsigned long position);
unsigned long filePositionCallback(void);
int fileReadCallback(void);
int fileReadBlockCallback(void * buffer, int numberOfBytes);
int fileSizeCallback(void);
byte renderImageToSegment(Segment &seg);
void endImagePlayback(Segment* seg);
#endif

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

bool deserializeState(JsonObject root, byte callMode = CALL_MODE_DIRECT_CHANGE, byte presetId = 0);
void serializeSegment(const JsonObject& root, const Segment& seg, byte id, bool forPreset = false, bool segmentBounds = true);
void serializeState(JsonObject root, bool forPreset = false, bool includeBri = true, bool segmentBounds = true, bool selectedSegmentsOnly = false);
void serializeInfo(JsonObject root);
void serializeModeNames(JsonArray arr);
void serializeModeData(JsonArray fxdata);
void serveJson(AsyncWebServerRequest* request);
#ifdef WLED_ENABLE_JSONLIVE
bool serveLiveLeds(AsyncWebServerRequest* request, uint32_t wsClient = 0);
#endif

//led.cpp
void setValuesFromSegment(uint8_t s);
#define setValuesFromMainSeg()          setValuesFromSegment(strip.getMainSegmentId())
#define setValuesFromFirstSelectedSeg() setValuesFromSegment(strip.getFirstSelectedSegId())
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
bool presetNeedsSaving();
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
void handleWiZdata(uint8_t *incomingData, size_t len);
void handleRemote();

//set.cpp
bool isAsterisksOnly(const char* str, byte maxLen);
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage);
bool handleSet(AsyncWebServerRequest *request, const String& req, bool apply=true);

//udp.cpp
void notify(byte callMode, bool followUp=false);
uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, const uint8_t* buffer, uint8_t bri=255, bool isRGBW=false);
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
bool initEthernet(); // result is informational
int  getSignalQuality(int rssi);
void fillMAC2Str(char *str, const uint8_t *mac);
void fillStr2MAC(uint8_t *mac, const char *str);
int  findWiFi(bool doScan = false);
bool isWiFiConfigured();
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
    Usermod() : um_data(nullptr) {};
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
    virtual bool onUdpPacket(uint8_t* payload, size_t len) { return false; } //fired upon UDP packet received
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

namespace UsermodManager {
  void loop();
  void handleOverlayDraw();
  bool handleButton(uint8_t b);
  bool getUMData(um_data_t **um_data, uint8_t mod_id = USERMOD_ID_RESERVED); // USERMOD_ID_RESERVED will poll all usermods
  void setup();
  void connected();
  void appendConfigData(Print&);
  void addToJsonState(JsonObject& obj);
  void addToJsonInfo(JsonObject& obj);
  void readFromJsonState(JsonObject& obj);
  void addToConfig(JsonObject& obj);
  bool readFromConfig(JsonObject& obj);
#ifndef WLED_DISABLE_MQTT
  void onMqttConnect(bool sessionPresent);
  bool onMqttMessage(char* topic, char* payload);
#endif
#ifndef WLED_DISABLE_ESPNOW
  bool onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len);
#endif
  bool onUdpPacket(uint8_t* payload, size_t len);
  void onUpdateBegin(bool);
  void onStateChange(uint8_t);
  Usermod* lookup(uint16_t mod_id);
  size_t getModCount();
};

// Register usermods by building a static list via a linker section
#define REGISTER_USERMOD(x) Usermod* const um_##x __attribute__((__section__(".dtors.tbl.usermods.1"), used)) = &x

//usermod.cpp
void userSetup();
void userConnected();
void userLoop();

//util.cpp
#ifdef ESP8266
#define HW_RND_REGISTER RANDOM_REG32
#else // ESP32 family
#include "soc/wdev_reg.h"
#define HW_RND_REGISTER REG_READ(WDEV_RND_REG)
#endif
#define inoise8 perlin8   // fastled legacy alias
#define inoise16 perlin16 // fastled legacy alias
#define hex2int(a) (((a)>='0' && (a)<='9') ? (a)-'0' : ((a)>='A' && (a)<='F') ? (a)-'A'+10 : ((a)>='a' && (a)<='f') ? (a)-'a'+10 : 0)
[[gnu::pure]] int getNumVal(const String &req, uint16_t pos);
void parseNumber(const char* str, byte &val, byte minv=0, byte maxv=255);
bool getVal(JsonVariant elem, byte &val, byte vmin=0, byte vmax=255); // getVal supports inc/decrementing and random ("X~Y(r|[w]~[-][Z])" form)
[[gnu::pure]] bool getBoolVal(const JsonVariant &elem, bool dflt);
bool updateVal(const char* req, const char* key, byte &val, byte minv=0, byte maxv=255);
size_t printSetFormCheckbox(Print& settingsScript, const char* key, int val);
size_t printSetFormValue(Print& settingsScript, const char* key, int val);
size_t printSetFormValue(Print& settingsScript, const char* key, const char* val);
size_t printSetFormIndex(Print& settingsScript, const char* key, int index);
size_t printSetClassElementHTML(Print& settingsScript, const char* key, const int index, const char* val);
void prepareHostname(char* hostname);
[[gnu::pure]] bool isAsterisksOnly(const char* str, byte maxLen);
bool requestJSONBufferLock(uint8_t moduleID=255);
void releaseJSONBufferLock();
uint8_t extractModeName(uint8_t mode, const char *src, char *dest, uint8_t maxLen);
uint8_t extractModeSlider(uint8_t mode, uint8_t slider, char *dest, uint8_t maxLen, uint8_t *var = nullptr);
int16_t extractModeDefaults(uint8_t mode, const char *segVar);
void checkSettingsPIN(const char *pin);
uint16_t crc16(const unsigned char* data_p, size_t length);
String computeSHA1(const String& input);
String getDeviceId();
uint16_t beatsin88_t(accum88 beats_per_minute_88, uint16_t lowest = 0, uint16_t highest = 65535, uint32_t timebase = 0, uint16_t phase_offset = 0);
uint16_t beatsin16_t(accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535, uint32_t timebase = 0, uint16_t phase_offset = 0);
uint8_t beatsin8_t(accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255, uint32_t timebase = 0, uint8_t phase_offset = 0);
um_data_t* simulateSound(uint8_t simulationId);
void enumerateLedmaps();
[[gnu::hot]] uint8_t get_random_wheel_index(uint8_t pos);
[[gnu::hot, gnu::pure]] float mapf(float x, float in_min, float in_max, float out_min, float out_max);
uint32_t hashInt(uint32_t s);
int32_t perlin1D_raw(uint32_t x, bool is16bit = false);
int32_t perlin2D_raw(uint32_t x, uint32_t y, bool is16bit = false);
int32_t perlin3D_raw(uint32_t x, uint32_t y, uint32_t z, bool is16bit = false);
uint16_t perlin16(uint32_t x);
uint16_t perlin16(uint32_t x, uint32_t y);
uint16_t perlin16(uint32_t x, uint32_t y, uint32_t z);
uint8_t perlin8(uint16_t x);
uint8_t perlin8(uint16_t x, uint16_t y);
uint8_t perlin8(uint16_t x, uint16_t y, uint16_t z);

// fast (true) random numbers using hardware RNG, all functions return values in the range lowerlimit to upperlimit-1
// note: for true random numbers with high entropy, do not call faster than every 200ns (5MHz)
// tests show it is still highly random reading it quickly in a loop (better than fastled PRNG)
// for 8bit and 16bit random functions: no limit check is done for best speed
// 32bit inputs are used for speed and code size, limits don't work if inverted or out of range
// inlining does save code size except for random(a,b) and 32bit random with limits
#define random hw_random // replace arduino random()
inline uint32_t hw_random() { return HW_RND_REGISTER; };
uint32_t hw_random(uint32_t upperlimit); // not inlined for code size
int32_t hw_random(int32_t lowerlimit, int32_t upperlimit);
inline uint16_t hw_random16() { return HW_RND_REGISTER; };
inline uint16_t hw_random16(uint32_t upperlimit) { return (hw_random16() * upperlimit) >> 16; }; // input range 0-65535 (uint16_t)
inline int16_t hw_random16(int32_t lowerlimit, int32_t upperlimit) { int32_t range = upperlimit - lowerlimit; return lowerlimit + hw_random16(range); }; // signed limits, use int16_t ranges
inline uint8_t hw_random8() { return HW_RND_REGISTER; };
inline uint8_t hw_random8(uint32_t upperlimit) { return (hw_random8() * upperlimit) >> 8; }; // input range 0-255
inline uint8_t hw_random8(uint32_t lowerlimit, uint32_t upperlimit) { uint32_t range = upperlimit - lowerlimit; return lowerlimit + hw_random8(range); }; // input range 0-255

// memory allocation wrappers (util.cpp)
extern "C" {
  // prefer DRAM in d_xalloc functions, PSRAM as fallback
  void *d_malloc(size_t);
  void *d_calloc(size_t, size_t);
  void *d_realloc_malloc(void *ptr, size_t size);
  #ifndef ESP8266
  inline void d_free(void *ptr) { heap_caps_free(ptr); }
  #else
  inline void d_free(void *ptr) { free(ptr); }
  #endif
  #if defined(BOARD_HAS_PSRAM)
  // prefer PSRAM in p_xalloc functions, DRAM as fallback
  void *p_malloc(size_t);
  void *p_calloc(size_t, size_t);
  void *p_realloc_malloc(void *ptr, size_t size);
  inline void p_free(void *ptr) { heap_caps_free(ptr); }
  #else
  #define p_malloc d_malloc
  #define p_calloc d_calloc
  #define p_realloc_malloc d_realloc_malloc
  #define p_free d_free
  #endif
}
#ifndef ESP8266
inline size_t getFreeHeapSize() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); } // returns free heap (ESP.getFreeHeap() can include other memory types)
inline size_t getContiguousFreeHeap() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); } // returns largest contiguous free block
#else
inline size_t getFreeHeapSize() { return ESP.getFreeHeap(); } // returns free heap
inline size_t getContiguousFreeHeap() { return ESP.getMaxFreeBlockSize(); } // returns largest contiguous free block
#endif
#define BFRALLOC_NOBYTEACCESS    (1 << 0) // ESP32 has 32bit accessible DRAM (usually ~50kB free) that must not be byte-accessed
#define BFRALLOC_PREFER_DRAM     (1 << 1) // prefer DRAM over PSRAM
#define BFRALLOC_ENFORCE_DRAM    (1 << 2) // use DRAM only, no PSRAM
#define BFRALLOC_PREFER_PSRAM    (1 << 3) // prefer PSRAM over DRAM
#define BFRALLOC_ENFORCE_PSRAM   (1 << 4) // use PSRAM if available, otherwise uses DRAM
#define BFRALLOC_CLEAR           (1 << 5) // clear allocated buffer after allocation
void *allocate_buffer(size_t size, uint32_t type);

void handleBootLoop();   // detect and handle bootloops
#ifndef ESP8266
void bootloopCheckOTA(); // swap boot image if bootloop is detected instead of restoring config
#endif
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
//float cos_t(float phi); // use float math
//float sin_t(float phi);
//float tan_t(float x);
int16_t sin16_t(uint16_t theta);
int16_t cos16_t(uint16_t theta);
uint8_t sin8_t(uint8_t theta);
uint8_t cos8_t(uint8_t theta);
float sin_approx(float theta); // uses integer math (converted to float), accuracy +/-0.0015 (compared to sinf())
float cos_approx(float theta);
float tan_approx(float x);
float atan2_t(float y, float x);
float acos_t(float x);
float asin_t(float x);
template <typename T> T atan_t(T x);
float floor_t(float x);
float fmod_t(float num, float denom);
uint32_t sqrt32_bw(uint32_t x);
#define sin_t sin_approx
#define cos_t cos_approx
#define tan_t tan_approx

/*
#include <math.h>  // standard math functions. use a lot of flash
#define sin_t sinf
#define cos_t cosf
#define tan_t tanf
#define asin_t asinf
#define acos_t acosf
#define atan_t atanf
#define fmod_t fmodf
#define floor_t floorf
*/
//wled_serial.cpp
void handleSerial();
void updateBaudRate(uint32_t rate);

//wled_server.cpp
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
