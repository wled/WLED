#ifndef WLED_FCN_DECLARE_H
#define WLED_FCN_DECLARE_H
#include <Arduino.h>
#include "src/dependencies/espalexa/EspalexaDevice.h"
#include "src/dependencies/e131/ESPAsyncE131.h"

/*
 * All globally accessible functions are declared here
 */

//alexa.cpp
void onAlexaChange(EspalexaDevice* dev);
void alexaInit();
void handleAlexa();
void onAlexaChange(EspalexaDevice* dev);

//blynk.cpp
#ifndef WLED_DISABLE_BLYNK
void initBlynk(const char* auth, const char* host, uint16_t port);
void handleBlynk();
void updateBlynk();
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

//file.cpp
bool handleFileRead(AsyncWebServerRequest*, String path);
bool writeObjectToFileUsingId(const char* file, uint16_t id, JsonDocument* content);
bool writeObjectToFile(const char* file, const char* key, JsonDocument* content);
bool readObjectFromFileUsingId(const char* file, uint16_t id, JsonDocument* dest);
bool readObjectFromFile(const char* file, const char* key, JsonDocument* dest);
void updateFSInfo();
void closeFile();

//hue.cpp
void handleHue();
void reconnectHue();
void onHueError(void* arg, AsyncClient* client, int8_t error);
void onHueConnect(void* arg, AsyncClient* client);
void sendHuePoll();
void onHueData(void* arg, AsyncClient* client, void *data, size_t len);

//improv.cpp
void handleImprovPacket();
void sendImprovStateResponse(uint8_t state, bool error = false);
void sendImprovInfoResponse();
void sendImprovRPCResponse(uint8_t commandId);

//ir.cpp
//bool decodeIRCustom(uint32_t code);
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

void initIR();
void handleIR();

//json.cpp
#include "ESPAsyncWebServer.h"
#include "src/dependencies/json/ArduinoJson-v6.h"
#include "src/dependencies/json/AsyncJson-v6.h"
#include "FX.h"

void deserializeSegment(JsonObject elem, byte it, byte presetId = 0);
bool deserializeState(JsonObject root, byte callMode = CALL_MODE_DIRECT_CHANGE, byte presetId = 0);
void serializeSegment(JsonObject& root, WS2812FX::Segment& seg, byte id, bool forPreset = false, bool segmentBounds = true);
void serializeState(JsonObject root, bool forPreset = false, bool includeBri = true, bool segmentBounds = true);
void serializeInfo(JsonObject root);
void serveJson(AsyncWebServerRequest* request);
#ifdef WLED_ENABLE_JSONLIVE
bool serveLiveLeds(AsyncWebServerRequest* request, uint32_t wsClient = 0);
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
byte scaledBri(byte in);

//lx_parser.cpp
bool parseLx(int lxValue, byte* rgbw);
void parseLxJson(int lxValue, byte segId, bool secondary);

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

//presets.cpp
bool applyPreset(byte index, byte callMode = CALL_MODE_DIRECT_CHANGE);
inline bool applyTemporaryPreset() {return applyPreset(255);};
void savePreset(byte index, const char* pname = nullptr, JsonObject saveobj = JsonObject());
inline void saveTemporaryPreset() {savePreset(255);};
void deletePreset(byte index);

//set.cpp
bool isAsterisksOnly(const char* str, byte maxLen);
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage);
bool handleSet(AsyncWebServerRequest *request, const String& req, bool apply=true);
int getNumVal(const String* req, uint16_t pos);
void parseNumber(const char* str, byte* val, byte minv=0, byte maxv=255);
bool updateVal(const String* req, const char* key, byte* val, byte minv=0, byte maxv=255);

//udp.cpp
void notify(byte callMode, bool followUp=false);
uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, byte *buffer, uint8_t bri=255, bool isRGBW=false);
void realtimeLock(uint32_t timeoutMs, byte md = REALTIME_MODE_GENERIC);
void exitRealtime();
void handleNotifications();
void setRealtimePixel(uint16_t i, byte r, byte g, byte b, byte w);
void refreshNodeList();
void sendSysInfoUDP();

//util.cpp
//bool oappend(const char* txt); // append new c string to temp buffer efficiently
//bool oappendi(int i);          // append new number to temp buffer efficiently
//void sappend(char stype, const char* key, int val);
//void sappends(char stype, const char* key, char* val);
//void prepareHostname(char* hostname);
//bool isAsterisksOnly(const char* str, byte maxLen);
bool requestJSONBufferLock(uint8_t module=255);
void releaseJSONBufferLock();
uint8_t extractModeName(uint8_t mode, const char *src, char *dest, uint8_t maxLen);

//um_manager.cpp
class Usermod {
  public:
    virtual void loop() {}
    virtual void handleOverlayDraw() {}
    virtual bool handleButton(uint8_t b) { return false; }
    virtual void setup() {}
    virtual void connected() {}
    virtual void addToJsonState(JsonObject& obj) {}
    virtual void addToJsonInfo(JsonObject& obj) {}
    virtual void readFromJsonState(JsonObject& obj) {}
    virtual void addToConfig(JsonObject& obj) {}
    virtual bool readFromConfig(JsonObject& obj) { return true; } // Note as of 2021-06 readFromConfig() now needs to return a bool, see usermod_v2_example.h
    virtual void onMqttConnect(bool sessionPresent) {}
    virtual bool onMqttMessage(char* topic, char* payload) { return false; }
    virtual uint16_t getId() {return USERMOD_ID_UNSPECIFIED;}
};

class UsermodManager {
  private:
    Usermod* ums[WLED_MAX_USERMODS];
    byte numMods = 0;

  public:
    void loop();
    void handleOverlayDraw();
    bool handleButton(uint8_t b);
    void setup();
    void connected();
    void addToJsonState(JsonObject& obj);
    void addToJsonInfo(JsonObject& obj);
    void readFromJsonState(JsonObject& obj);
    void addToConfig(JsonObject& obj);
    bool readFromConfig(JsonObject& obj);
    void onMqttConnect(bool sessionPresent);
    bool onMqttMessage(char* topic, char* payload);
    bool add(Usermod* um);
    Usermod* lookup(uint16_t mod_id);
    byte getModCount();
};

//usermods_list.cpp
void registerUsermods();

//usermod.cpp
void userSetup();
void userConnected();
void userLoop();

//wled_eeprom.cpp
void applyMacro(byte index);
void deEEP();
void deEEPSettings();
void clearEEPROM();

//wled_serial.cpp
void handleSerial();
void updateBaudRate(uint32_t rate);

//wled_server.cpp
bool isIp(String str);
bool captivePortal(AsyncWebServerRequest *request);
void initServer();
void serveIndexOrWelcome(AsyncWebServerRequest *request);
void serveIndex(AsyncWebServerRequest* request);
String msgProcessor(const String& var);
void serveMessage(AsyncWebServerRequest* request, uint16_t code, const String& headl, const String& subl="", byte optionT=255);
String settingsProcessor(const String& var);
String dmxProcessor(const String& var);
void serveSettings(AsyncWebServerRequest* request, bool post = false);

//ws.cpp
void handleWs();
void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
void sendDataWs(AsyncWebSocketClient * client = nullptr);

//xml.cpp
void XML_response(AsyncWebServerRequest *request, char* dest = nullptr);
void URL_response(AsyncWebServerRequest *request);
void sappend(char stype, const char* key, int val);
void sappends(char stype, const char* key, char* val);
void getSettingsJS(byte subPage, char* dest);

#endif
