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


//colors.cpp
#if !defined(ARDUINO_ARCH_ESP32) || !defined(WLEDMM_FASTPATH) || defined(WLEDMM_SAVE_FLASH)  // WLEDMM: color utils moved into colorTools.hpp, so the compiler may inline these functions (faster)
uint32_t __attribute__((const)) color_blend(uint32_t,uint32_t,uint_fast16_t,bool b16=false);  // WLEDMM: added attribute const
uint32_t __attribute__((const)) color_add(uint32_t,uint32_t, bool fast=false);                // WLEDMM: added attribute const
uint32_t __attribute__((const)) color_fade(uint32_t c1, uint8_t amount, bool video=false);
#else
#include "colorTools.hpp"
#endif

inline uint32_t colorFromRgbw(byte* rgbw) { return uint32_t((byte(rgbw[3]) << 24) | (byte(rgbw[0]) << 16) | (byte(rgbw[1]) << 8) | (byte(rgbw[2]))); }
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb); //hue, sat to rgb
void colorKtoRGB(uint16_t kelvin, byte* rgb);
void colorCTtoRGB(uint16_t mired, byte* rgb); //white spectrum to rgb
void colorXYtoRGB(float x, float y, byte* rgb); // only defined if huesync disabled TODO
void colorRGBtoXY(byte* rgb, float* xy); // only defined if huesync disabled TODO
void colorFromDecOrHexString(byte* rgb, char* in);
bool colorFromHexString(byte* rgb, const char* in);
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t __attribute__((const)) approximateKelvinFromRGB(uint32_t rgb);                       // WLEDMM: added attribute const
void setRandomColor(byte* rgb);
uint8_t gamma8_cal(uint8_t b, float gamma);
void calcGammaTable(float gamma);
uint8_t __attribute__((pure)) gamma8(uint8_t b);                                              // WLEDMM: added attribute pure
uint32_t __attribute__((pure)) gamma32(uint32_t);                                             // WLEDMM: added attribute pure
uint8_t unGamma8(uint8_t value);                                                              // WLEDMM revert gamma correction
uint32_t unGamma24(uint32_t c);                                                               // WLEDMM for 24bit color (white left as-is)

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
bool writeObjectToFileUsingId(const char* file, uint16_t id, JsonDocument* content);
bool writeObjectToFile(const char* file, const char* key, JsonDocument* content);
bool readObjectFromFileUsingId(const char* file, uint16_t id, JsonDocument* dest);
bool readObjectFromFile(const char* file, const char* key, JsonDocument* dest);
void updateFSInfo();
void closeFile();
void invalidateFileNameCache();   // WLEDMM call when new files were uploaded

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
void applyRepeatActions();
byte relativeChange(byte property, int8_t amount, byte lowerBoundary = 0, byte higherBoundary = 0xFF);
void decodeIR(uint32_t code);
void decodeIR24(uint32_t code);
void decodeIR24OLD(uint32_t code);
void decodeIR24CT(uint32_t code);
void decodeIR40(uint32_t code);
void decodeIR44(uint32_t code);
void decodeIR21(uint32_t code);
void decodeIR6(uint32_t code);
void decodeIR9(uint32_t code);
void decodeIRJson(uint32_t code);
void decodeIR24MC(uint32_t code); //WLEDMM

void initIR();
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
void serializeModeNames(JsonArray arr, const char *qstring);
void serializeModeData(JsonObject root);
void serveJson(AsyncWebServerRequest* request);
#ifdef WLED_ENABLE_JSONLIVE
bool serveLiveLeds(AsyncWebServerRequest* request, uint32_t wsClient = 0);
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <esp_system.h>
int getCoreResetReason(int core);
String resetCode2Info(int reason);
esp_reset_reason_t getRestartReason();
String restartCode2Info(esp_reset_reason_t reason);
String restartCode2InfoLong(esp_reset_reason_t reason);
#endif

//led.cpp
void setValuesFromSegment(uint8_t s);
void setValuesFromMainSeg();
void setValuesFromFirstSelectedSeg();
void resetTimebase();
void toggleOnOff();
void applyBri();
void applyFinalBri();
void applyValuesToSelectedSegs();
void colorUpdated(byte callMode);
void stateUpdated(byte callMode);
void updateInterfaces(uint8_t callMode);
void handleTransitions();
void handleNightlight();

#if !defined(ARDUINO_ARCH_ESP32) || !defined(WLEDMM_FASTPATH) || defined(WLEDMM_SAVE_FLASH)  // WLEDMM: color utils moved into colorTools.hpp, so comiler can inline calls (up to 12% faster)
byte __attribute__((pure)) scaledBri(byte in);                     // WLEDMM: added attribute pure
#endif

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
void suspendPlaylist(); // WLEDMM support function for auto playlist usermod
void shufflePlaylist();
void unloadPlaylist();
int16_t loadPlaylist(JsonObject playlistObject, byte presetId = 0);
void handlePlaylist();
void serializePlaylist(JsonObject obj);

//presets.cpp
bool presetsSavePending(void);    // WLEDMM true if presetToSave, playlistSave or saveLedmap
bool presetsActionPending(void);  // WLEDMM true if presetToApply, presetToSave, playlistSave or saveLedmap
void initPresetsFile();
void handlePresets();
bool applyPreset(byte index, byte callMode = CALL_MODE_DIRECT_CHANGE);
void applyPresetWithFallback(uint8_t presetID, uint8_t callMode, uint8_t effectID = 0, uint8_t paletteID = 0);
inline bool applyTemporaryPreset() {return applyPreset(255);};
void savePreset(byte index, const char* pname = nullptr, JsonObject saveobj = JsonObject());
inline void saveTemporaryPreset() {savePreset(255);};
void deletePreset(byte index);
bool getPresetName(byte index, String& name);

//remote.cpp
void handleRemote();

//set.cpp
bool isAsterisksOnly(const char* str, byte maxLen);
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage);
bool handleSet(AsyncWebServerRequest *request, const String& req, bool apply=true);

//udp.cpp
void notify(byte callMode, bool followUp=false);
uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, uint8_t *buffer, uint8_t bri=255, bool isRGBW=false, uint8_t artnet_outouts=1, uint16_t artnet_leds_per_output=1, uint8_t artnet_fps_limit=1);
void realtimeLock(uint32_t timeoutMs, byte md = REALTIME_MODE_GENERIC);
void exitRealtime();
void handleNotifications();
void setRealtimePixel(uint16_t i, byte r, byte g, byte b, byte w);
void refreshNodeList();
void sendSysInfoUDP();

//network.cpp
int getSignalQuality(int rssi) __attribute__((const));
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
    bool enabled = false; //WLEDMM
    const char *_name; //WLEDMM
    bool initDone = false; //WLEDMM
    unsigned long lastTime = 0; //WLEDMM
  public:
    Usermod(const char *_name = nullptr, bool enabled=false) { um_data = nullptr; this->_name = _name; this->enabled=enabled;}
    virtual ~Usermod() { if (um_data) delete um_data; }
    virtual void setup() = 0; // pure virtual, has to be overriden
    virtual void loop() = 0;  // pure virtual, has to be overriden
    virtual void loop2() {}                                                  // WLEDMM called just before effects will be processed
    virtual void handleOverlayDraw() {}                                      // called after all effects have been processed, just before strip.show()
    virtual bool handleButton(uint8_t b) { return false; }                   // button overrides are possible here
    virtual bool getUMData(um_data_t **data) { if (data) *data = nullptr; return false; }; // usermod data exchange [see examples for audio effects]
    virtual void connected() {}                                              // called when WiFi is (re)connected
    virtual void appendConfigData() {}                                       // helper function called from usermod settings page to add metadata for entry fields
    virtual void addToJsonState(JsonObject& obj) {}                          // add JSON objects for WLED state
    virtual void addToJsonInfo(JsonObject& obj) {}                           // add JSON objects for UI Info page
    virtual void readFromJsonState(JsonObject& obj) {}                       // process JSON messages received from web server
    virtual void addToConfig(JsonObject& obj) {                              // add JSON entries that go to cfg.json
      JsonObject top = obj.createNestedObject(FPSTR(_name));                 // WLEDMM: set enabled and _name
      top[FPSTR("enabled")] = enabled;
    }
    virtual bool readFromConfig(JsonObject& obj) {                           // Note as of 2021-06 readFromConfig() now needs to return a bool, see usermod_v2_example.h
      JsonObject top = obj[FPSTR(_name)];                                    // WLEDMM: get enabled and _name
      return !top.isNull() && getJsonValue(top[FPSTR("enabled")], enabled); 
    }
    virtual void onMqttConnect(bool sessionPresent) {}                       // fired when MQTT connection is established (so usermod can subscribe)
    virtual bool onMqttMessage(char* topic, char* payload) { return false; } // fired upon MQTT message received (wled topic)
    virtual void onUpdateBegin(bool) {}                                      // fired prior to and after unsuccessful firmware update
    virtual void onStateChange(uint8_t mode) {}                              // fired upon WLED state change
    virtual uint16_t getId() {return USERMOD_ID_UNSPECIFIED;}
};

class UsermodManager {
  private:
    Usermod* ums[WLED_MAX_USERMODS];
    unsigned numMods = 0;

  public:
    void loop();
    void loop2();   // WLEDMM loop just before drawing effects (presets and everything already handled)
    void handleOverlayDraw();
    bool handleButton(uint8_t b);
    bool getUMData(um_data_t **um_data, uint8_t mod_id = USERMOD_ID_RESERVED); // USERMOD_ID_RESERVED will poll all usermods
    void setup();
    void connected();
    // void appendConfigData(); //WLEDMM not used
    void addToJsonState(JsonObject& obj);
    void addToJsonInfo(JsonObject& obj);
    void readFromJsonState(JsonObject& obj);
    void addToConfig(JsonObject& obj);
    bool readFromConfig(JsonObject& obj);
    void onMqttConnect(bool sessionPresent);
    bool onMqttMessage(char* topic, char* payload);
    void onUpdateBegin(bool);
    void onStateChange(uint8_t);
    bool add(Usermod* um);
    Usermod* lookup(uint16_t mod_id);
    Usermod* lookupName(const char *mod_name); //WLEDMM
    byte getModCount() {return numMods;};
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
bool updateVal(const char* req, const char* key, byte* val, byte minv=0, byte maxv=255);
void oappendUseDeflate(bool OnOff); // enable / disable string squeezing
bool oappend(const char* txt); // append new c string to temp buffer efficiently
bool oappendi(int i);          // append new number to temp buffer efficiently
void sappend(char stype, const char* key, int val);
void sappends(char stype, const char* key, char* val);
void prepareHostname(char* hostname);
bool isAsterisksOnly(const char* str, byte maxLen)  __attribute__((pure));
bool requestJSONBufferLock(uint8_t module=255);
void releaseJSONBufferLock();
uint8_t extractModeName(uint8_t mode, const char *src, char *dest, uint8_t maxLen);
uint8_t extractModeSlider(uint8_t mode, uint8_t slider, char *dest, uint8_t maxLen, uint8_t *var = nullptr);
int16_t extractModeDefaults(uint8_t mode, const char *segVar);
void checkSettingsPIN(const char *pin);
uint16_t  __attribute__((pure)) crc16(const unsigned char* data_p, size_t length);   // WLEDMM: added attribute pure

uint16_t beatsin88_t(accum88 beats_per_minute_88, uint16_t lowest = 0, uint16_t highest = 65535, uint32_t timebase = 0, uint16_t phase_offset = 0);
uint16_t beatsin16_t(accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535, uint32_t timebase = 0, uint16_t phase_offset = 0);
uint8_t beatsin8_t(accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255, uint32_t timebase = 0, uint8_t phase_offset = 0);

um_data_t* simulateSound(uint8_t simulationId);
// WLEDMM enumerateLedmaps(); moved to FX.h
uint8_t get_random_wheel_index(uint8_t pos);
CRGB getCRGBForBand(int x, uint8_t *fftResult, int pal); //WLEDMM netmindz ar palette
char *cleanUpName(char *in); // to clean up a name that was read from file

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
//float atan2_t(float y, float x);
//float acos_t(float x);
//float asin_t(float x);
//template <typename T> T atan_t(T x);
//float floor_t(float x);
//float fmod_t(float num, float denom);
#define sin_t sin_approx
#define cos_t cos_approx
#define tan_t tan_approx

#include <math.h>  // standard math functions. use a lot of flash
#define atan2_t atan2f
#define asin_t asinf
#define acos_t acosf
#define atan_t atanf
#define fmod_t fmodf
#define floor_t floorf
/*
#define sin_t sinf
#define cos_t cosf
#define tan_t tanf
*/

//wled_serial.cpp
void handleSerial();
void updateBaudRate(uint32_t rate);
bool canUseSerial(void);   // WLEDMM returns true if Serial can be used for debug output (i.e. not configured for other purpose)

//wled_server.cpp
bool isIp(String str);
void createEditHandler(bool enable);
bool captivePortal(AsyncWebServerRequest *request);
void initServer();
void serveIndexOrWelcome(AsyncWebServerRequest *request);
void serveIndex(AsyncWebServerRequest* request);
String msgProcessor(const String& var);
void serveMessage(AsyncWebServerRequest* request, uint16_t code, const String& headl, const String& subl="", byte optionT=255);
String settingsProcessor(const String& var);
String dmxProcessor(const String& var);
void serveSettings(AsyncWebServerRequest* request, bool post = false);
void serveSettingsJS(AsyncWebServerRequest* request);

//ws.cpp
void handleWs();
void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
void sendDataWs(AsyncWebSocketClient * client = nullptr);

//xml.cpp
void XML_response(AsyncWebServerRequest *request, char* dest = nullptr);
void URL_response(AsyncWebServerRequest *request);
void getSettingsJS(AsyncWebServerRequest* request, byte subPage, char* dest); //WLEDMM add request

#endif
