#pragma once
#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
  #if __has_include(<NetworkClientSecure.h>)
    #include <NetworkClientSecure.h>
    #define HUE_SECURE_CLIENT NetworkClientSecure
  #elif __has_include(<WiFiClientSecure.h>)
    #include <WiFiClientSecure.h>
    #define HUE_SECURE_CLIENT WiFiClientSecure
  #endif
#endif

class PhilipsHueV2Usermod : public Usermod {
  private:
    static constexpr size_t RESPONSE_BUF_SIZE = 2048;
    bool enabled = false;
    bool initDone = false;
    char bridgeIp[16] = "";
    char apiKey[64] = "";
    char lightId[64] = "";
    unsigned long pollInterval = 2500;
    bool applyOnOff = true;
    bool applyBri = true;
    bool applyColor = true;
    bool attemptAuth = false;
    unsigned long lastPoll = 0;
    byte lastBri = 0;
    float lastX = 0.0f;
    float lastY = 0.0f;
    uint16_t lastCt = 0;
    String statusStr = "Idle";
    static const char _name[];
    static const char _enabled[];

#ifdef HUE_SECURE_CLIENT
    static void xyToRGB(float x, float y, byte* rgb) {
      float z = 1.0f - x - y;
      float Y = 1.0f;
      float X = (Y / y) * x;
      float Z = (Y / y) * z;
      float r = X * 1.656492f - Y * 0.354851f - Z * 0.255038f;
      float g = -X * 0.707196f + Y * 1.655397f + Z * 0.036152f;
      float b = X * 0.051713f - Y * 0.121364f + Z * 1.011530f;
      if (r > b && r > g && r > 1.0f) { g /= r; b /= r; r = 1.0f; }
      else if (g > b && g > r && g > 1.0f) { r /= g; b /= g; g = 1.0f; }
      else if (b > r && b > g && b > 1.0f) { r /= b; g /= b; b = 1.0f; }
      rgb[0] = (byte)(max(0.0f, r) * 255.0f);
      rgb[1] = (byte)(max(0.0f, g) * 255.0f);
      rgb[2] = (byte)(max(0.0f, b) * 255.0f);
    }

    String httpsRequest(const char* method, const char* path, const char* body = nullptr) {
      HUE_SECURE_CLIENT client;
      client.setInsecure();
      client.setTimeout(4);
      if (!client.connect(bridgeIp, 443)) { statusStr = F("Connection failed"); return ""; }
      String request = String(method) + " " + String(path) + " HTTP/1.1\r\n";
      request += "Host: " + String(bridgeIp) + "\r\n";
      if (strlen(apiKey) > 0) request += "hue-application-key: " + String(apiKey) + "\r\n";
      if (body) {
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + String(strlen(body)) + "\r\n";
      }
      request += "Connection: close\r\n\r\n";
      if (body) request += String(body);
      client.print(request);
      String rawResponse = "";
      unsigned long timeout = millis() + 4000;
      while (client.connected() && millis() < timeout) {
        if (client.available()) {
          String line = client.readStringUntil('\n');
          rawResponse += line + "\n";
          if (rawResponse.length() > RESPONSE_BUF_SIZE) break;
        }
        yield();
      }
      client.stop();
      int bodyStart = rawResponse.indexOf("\r\n\r\n");
      if (bodyStart < 0) bodyStart = rawResponse.indexOf("\n\n");
      if (bodyStart < 0) return "";
      return rawResponse.substring(bodyStart + 4);
    }

    void authenticate() {
      statusStr = F("Authenticating...");
      const char* body = "{\"devicetype\":\"wled#esp32\",\"generateclientkey\":true}";
      String response = httpsRequest("POST", "/api", body);
      if (response.length() == 0) return;
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, response)) { statusStr = F("Auth JSON error"); return; }
      int errCode = doc[0]["error"]["type"] | 0;
      if (errCode == 101) { statusStr = F("Press link button first"); return; }
      if (errCode != 0) { statusStr = "Hue error: " + String(errCode); return; }
      const char* username = doc[0]["success"]["username"];
      if (username && strlen(username) < sizeof(apiKey)) {
        strlcpy(apiKey, username, sizeof(apiKey));
        attemptAuth = false;
        statusStr = F("Auth OK - key saved");
        serializeConfig();
      } else { statusStr = F("Auth failed - no key"); }
    }

    void pollLight() {
      if (!strlen(apiKey)) { statusStr = F("No API key"); return; }
      if (!strlen(lightId)) { statusStr = F("No light ID"); return; }
      String path = "/clip/v2/resource/light/" + String(lightId);
      String response = httpsRequest("GET", path.c_str());
      if (response.length() == 0) return;
      DynamicJsonDocument doc(2048);
      if (deserializeJson(doc, response)) { statusStr = F("JSON parse error"); return; }
      JsonArray errors = doc["errors"];
      if (errors && errors.size() > 0) {
        const char* desc = errors[0]["description"];
        statusStr = desc ? String(desc) : F("API error"); return;
      }
      JsonArray data = doc["data"];
      if (!data || data.size() == 0) { statusStr = F("No data"); return; }
      JsonObject light = data[0];
      bool isOn = light["on"]["on"] | false;
      float brightness = light["dimming"]["brightness"] | 0.0f;
      byte newBri = isOn ? (byte)((brightness * 255.0f) / 100.0f) : 0;
      if (newBri < 1 && brightness > 0 && isOn) newBri = 1;
      bool changed = false;
      if (newBri != lastBri) {
        if (applyOnOff) { if (newBri == 0) bri = 0; else if (bri == 0) bri = briLast; }
        if (applyBri && newBri > 0) bri = newBri;
        lastBri = newBri; changed = true;
      }
      if (applyColor) {
        JsonObject color = light["color"];
        if (!color.isNull()) {
          float x = color["xy"]["x"] | 0.0f;
          float y = color["xy"]["y"] | 0.0f;
          if (y > 0.0f && (x != lastX || y != lastY)) {
            byte rgb[3]; xyToRGB(x, y, rgb);
            colPri[0] = rgb[0]; colPri[1] = rgb[1]; colPri[2] = rgb[2];
            lastX = x; lastY = y; changed = true;
          }
        }
        JsonObject ct = light["color_temperature"];
        if (!ct.isNull() && (ct["mirek_valid"] | false)) {
          uint16_t mirek = ct["mirek"] | 0;
          if (mirek > 0 && mirek != lastCt) {
            colorCTtoRGB(mirek, colPri); lastCt = mirek; changed = true;
          }
        }
      }
      if (changed) colorUpdated(CALL_MODE_DIRECT_CHANGE);
      statusStr = F("OK");
    }
#endif

  public:
    void setup() override { initDone = true; }

    void loop() override {
#ifdef HUE_SECURE_CLIENT
      if (!enabled || !initDone || strip.isUpdating()) return;
      if (!WLED_CONNECTED || !strlen(bridgeIp)) return;
      unsigned long now = millis();
      if (now - lastPoll < pollInterval) return;
      lastPoll = now;
      if (attemptAuth) authenticate(); else pollLight();
#endif
    }

    void addToJsonInfo(JsonObject& root) override {
      if (!enabled) return;
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray status = user.createNestedArray(F("Hue V2"));
#ifdef HUE_SECURE_CLIENT
      status.add(statusStr);
#else
      status.add(F("SSL not available"));
#endif
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[F("bridgeIp")] = bridgeIp;
      top[F("apiKey")] = apiKey;
      top[F("lightId")] = lightId;
      top[F("pollInterval")] = pollInterval;
      top[F("applyOnOff")] = applyOnOff;
      top[F("applyBri")] = applyBri;
      top[F("applyColor")] = applyColor;
      top[F("attemptAuth")] = attemptAuth;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);
      configComplete &= getJsonValue(top[F("pollInterval")], pollInterval, 2500UL);
      configComplete &= getJsonValue(top[F("applyOnOff")], applyOnOff, true);
      configComplete &= getJsonValue(top[F("applyBri")], applyBri, true);
      configComplete &= getJsonValue(top[F("applyColor")], applyColor, true);
      configComplete &= getJsonValue(top[F("attemptAuth")], attemptAuth, false);
      const char* s = nullptr;
      if (!(s = top[F("bridgeIp")])) configComplete = false; else strlcpy(bridgeIp, s, sizeof(bridgeIp));
      if (!(s = top[F("apiKey")])) configComplete = false; else strlcpy(apiKey, s, sizeof(apiKey));
      if (!(s = top[F("lightId")])) configComplete = false; else strlcpy(lightId, s, sizeof(lightId));
      return configComplete;
    }

    void appendConfigData() override {
      oappend(F("addInfo('Philips Hue V2:bridgeIp',1,'Hue bridge IP');"));
      oappend(F("addInfo('Philips Hue V2:apiKey',1,'auto-filled after auth');"));
      oappend(F("addInfo('Philips Hue V2:lightId',1,'Light UUID from V2 API');"));
      oappend(F("addInfo('Philips Hue V2:pollInterval',1,'ms (min 1000)');"));
      oappend(F("addInfo('Philips Hue V2:attemptAuth',1,'Press link button first');"));
    }

    uint16_t getId() override { return USERMOD_ID_PHILIPS_HUE_V2; }
};

const char PhilipsHueV2Usermod::_name[] PROGMEM = "Philips Hue V2";
const char PhilipsHueV2Usermod::_enabled[] PROGMEM = "enabled";
