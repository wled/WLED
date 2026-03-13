#include "wled.h"
#include <memory>

/*
 * TLS backend detection — pick the first available:
 *   1. WiFiClientSecure / NetworkClientSecure (official espressif32 platform)
 *   2. ArduinoBearSSL wrapping WiFiClient (any platform, add lib dependency)
 */
#ifdef ARDUINO_ARCH_ESP32
  #if __has_include(<NetworkClientSecure.h>)
    #include <NetworkClientSecure.h>
    #define HUE_TLS_BACKEND_NATIVE
  #elif __has_include(<WiFiClientSecure.h>)
    #include <WiFiClientSecure.h>
    #define HUE_TLS_BACKEND_NATIVE
  #elif __has_include(<BearSSLClient.h>)
    #include <WiFi.h>
    #include <BearSSLClient.h>
    #define HUE_TLS_BACKEND_BEARSSL
  #endif
#endif

#if defined(HUE_TLS_BACKEND_NATIVE) || defined(HUE_TLS_BACKEND_BEARSSL)
  #define HUE_HAS_TLS
#endif

/*
 * Philips Hue V2 API Usermod
 *
 * Polls a Philips Hue bridge using the newer CLIP V2 API (HTTPS)
 * and syncs brightness and color to WLED.
 *
 * ESP32 only — requires a TLS backend. Supported options:
 *   - WiFiClientSecure/NetworkClientSecure (official espressif32 platform)
 *   - ArduinoBearSSL (any platform; add lib_deps = arduino-libraries/ArduinoBearSSL)
 *
 * Usage:
 *   1. Add this usermod to your build (it self-registers via REGISTER_USERMOD)
 *   2. Configure bridge IP, light resource ID, and API key via Usermod Settings
 *   3. To obtain an API key: press the link button on the bridge, then
 *      enable "Attempt Auth" in Usermod Settings and save.
 */
class PhilipsHueV2Usermod : public Usermod {

  private:
    static constexpr size_t RESPONSE_BUF_SIZE = 2048;

    bool enabled = false;
    bool initDone = false;

    // configuration
    char bridgeIp[16] = "";
    char apiKey[64] = "";
    char lightId[64] = "";
    unsigned long pollInterval = 2500;
    bool applyOnOff = true;
    bool applyBri = true;
    bool applyColor = true;
    bool attemptAuth = false;

    // state
    unsigned long lastPoll = 0;
    byte lastBri = 0;
    float lastX = 0.0f;
    float lastY = 0.0f;
    uint16_t lastCt = 0;
    String statusStr = "Idle";

    // PROGMEM strings
    static const char _name[];
    static const char _enabled[];

#ifdef HUE_HAS_TLS
    /*
     * Convert CIE 1931 xy coordinates to RGB.
     * Self-contained so the usermod works even when WLED_DISABLE_HUESYNC is set.
     */
    static void xyToRGB(float x, float y, byte* rgb) {
      float z = 1.0f - x - y;
      float Y = 1.0f;
      float X = (Y / y) * x;
      float Z = (Y / y) * z;

      float r = X * 1.656492f - Y * 0.354851f - Z * 0.255038f;
      float g = -X * 0.707196f + Y * 1.655397f + Z * 0.036152f;
      float b = X * 0.051713f - Y * 0.121364f + Z * 1.011530f;

      // clamp and scale
      if (r > b && r > g && r > 1.0f) { g /= r; b /= r; r = 1.0f; }
      else if (g > b && g > r && g > 1.0f) { r /= g; b /= g; g = 1.0f; }
      else if (b > r && b > g && b > 1.0f) { r /= b; g /= b; b = 1.0f; }

      r = (r < 0.0f) ? 0.0f : r;
      g = (g < 0.0f) ? 0.0f : g;
      b = (b < 0.0f) ? 0.0f : b;

      rgb[0] = (byte)(r * 255.0f);
      rgb[1] = (byte)(g * 255.0f);
      rgb[2] = (byte)(b * 255.0f);
    }

    /*
     * Make an HTTPS request to the Hue bridge.
     * Supports WiFiClientSecure/NetworkClientSecure or BearSSL backends.
     * Returns the response body or empty string on failure.
     */
    String httpsRequest(const char* method, const char* path, const char* body = nullptr) {
      DEBUG_PRINTF("[%s] httpsRequest %s %s to %s\n", _name, method, path, bridgeIp);
      /*
       * TLS clients (WiFiClientSecure etc.) use very large stack buffers
       * (10–16 KB on ESP-IDF v4).  The Arduino loopTask has only 8 KB of
       * stack by default, so we MUST heap-allocate the client to avoid a
       * stack overflow.
       */
#ifdef HUE_TLS_BACKEND_NATIVE
  #if __has_include(<NetworkClientSecure.h>)
      std::unique_ptr<NetworkClientSecure> client(new NetworkClientSecure);
  #else
      std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
  #endif
      if (!client) { DEBUG_PRINTF("[%s] TLS client alloc failed\n", _name); statusStr = F("Alloc failed"); return ""; }
      client->setInsecure();  // Hue bridge uses self-signed cert
      client->setTimeout(4);  // 4 second timeout
#elif defined(HUE_TLS_BACKEND_BEARSSL)
      WiFiClient tcpClient;
      std::unique_ptr<BearSSLClient> client(new BearSSLClient(tcpClient, nullptr, 0));
      if (!client) { DEBUG_PRINTF("[%s] TLS client alloc failed\n", _name); statusStr = F("Alloc failed"); return ""; }
      client->setInsecure(BearSSLClient::SNI::Insecure);
#endif

      DEBUG_PRINTF("[%s] Connecting to %s:443 (free heap: %u)...\n", _name, bridgeIp, ESP.getFreeHeap());
      if (!client->connect(bridgeIp, 443)) {
#ifdef HUE_TLS_BACKEND_NATIVE
        char errBuf[128] = {0};
        int err = client->lastError(errBuf, sizeof(errBuf));
        DEBUG_PRINTF("[%s] Connection failed: %s (error %d, errno %d, free heap: %u)\n",
                     _name, errBuf, err, errno, ESP.getFreeHeap());
        statusStr = String(errBuf);
#else
        DEBUG_PRINTF("[%s] Connection failed (errno %d)\n", _name, errno);
        statusStr = F("Connection failed");
#endif
        return "";
      }
      DEBUG_PRINTF("[%s] Connected\n", _name);

      // build HTTP request
      String request = String(method) + " " + String(path) + " HTTP/1.1\r\n";
      request += "Host: " + String(bridgeIp) + "\r\n";
      if (strlen(apiKey) > 0) {
        request += "hue-application-key: " + String(apiKey) + "\r\n";
      }
      if (body != nullptr) {
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + String(strlen(body)) + "\r\n";
      }
      request += "Connection: close\r\n\r\n";
      if (body != nullptr) {
        request += String(body);
      }

      client->print(request);

      // read response
      String rawResponse = "";
      unsigned long timeout = millis() + 4000;
      while (client->connected() && millis() < timeout) {
        if (client->available()) {
          String line = client->readStringUntil('\n');
          rawResponse += line + "\n";
          if (rawResponse.length() > RESPONSE_BUF_SIZE) break;
        }
        yield();
      }
      client->stop();

      DEBUG_PRINTF("[%s] Response length: %u bytes\n", _name, rawResponse.length());

      // extract body (skip HTTP headers)
      int bodyStart = rawResponse.indexOf("\r\n\r\n");
      if (bodyStart < 0) bodyStart = rawResponse.indexOf("\n\n");
      if (bodyStart < 0) {
        DEBUG_PRINTF("[%s] No HTTP body delimiter found in response\n", _name);
        return "";
      }
      return rawResponse.substring(bodyStart + 4);
    }

    /*
     * Authenticate with the Hue bridge using the link button method.
     * The user must press the link button on the bridge before calling this.
     */
    void authenticate() {
      DEBUG_PRINTF("[%s] Starting authentication...\n", _name);
      statusStr = F("Authenticating...");
      const char* body = "{\"devicetype\":\"wled#esp32\",\"generateclientkey\":true}";
      String response = httpsRequest("POST", "/api", body);

      if (response.length() == 0) {
        DEBUG_PRINTF("[%s] Auth: empty response from bridge\n", _name);
        return;
      }

      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, response);
      if (err) {
        DEBUG_PRINTF("[%s] Auth: JSON parse error: %s\n", _name, err.c_str());
        statusStr = F("Auth JSON parse error");
        return;
      }

      // check for error
      int errCode = doc[0]["error"]["type"] | 0;
      if (errCode == 101) {
        DEBUG_PRINTF("[%s] Auth: link button not pressed (error 101)\n", _name);
        statusStr = F("Press link button first");
        return;
      } else if (errCode != 0) {
        DEBUG_PRINTF("[%s] Auth: Hue error code %d\n", _name, errCode);
        statusStr = "Hue error: " + String(errCode);
        return;
      }

      // extract username (api key)
      const char* username = doc[0]["success"]["username"];
      if (username != nullptr && strlen(username) < sizeof(apiKey)) {
        strlcpy(apiKey, username, sizeof(apiKey));
        attemptAuth = false;
        statusStr = F("Auth OK - key saved");
        DEBUG_PRINTF("[%s] Auth successful, API key obtained\n", _name);
        serializeConfigToFS();  // persist the new API key
      } else {
        DEBUG_PRINTF("[%s] Auth failed: no username in response\n", _name);
        statusStr = F("Auth failed - no key");
      }
    }

    /*
     * Poll a specific light from the Hue bridge using the V2 API.
     */
    void pollLight() {
      if (strlen(apiKey) == 0) {
        DEBUG_PRINTF("[%s] Poll skipped: no API key configured\n", _name);
        statusStr = F("No API key");
        return;
      }
      if (strlen(lightId) == 0) {
        DEBUG_PRINTF("[%s] Poll skipped: no light ID configured\n", _name);
        statusStr = F("No light ID");
        return;
      }

      String path = "/clip/v2/resource/light/" + String(lightId);
      String response = httpsRequest("GET", path.c_str());

      if (response.length() == 0) {
        DEBUG_PRINTF("[%s] Poll: empty response\n", _name);
        return;
      }

      // V2 responses can be large; use a dynamic document
      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, response);
      if (err) {
        DEBUG_PRINTF("[%s] Poll: JSON parse error: %s\n", _name, err.c_str());
        statusStr = F("JSON parse error");
        return;
      }

      // check for errors array
      JsonArray errors = doc["errors"];
      if (errors && errors.size() > 0) {
        const char* desc = errors[0]["description"];
        DEBUG_PRINTF("[%s] Poll: API error: %s\n", _name, desc ? desc : "unknown");
        statusStr = desc ? String(desc) : F("API error");
        return;
      }

      JsonArray data = doc["data"];
      if (!data || data.size() == 0) {
        DEBUG_PRINTF("[%s] Poll: no 'data' array in response\n", _name);
        statusStr = F("No data in response");
        return;
      }

      JsonObject light = data[0];
      bool isOn = light["on"]["on"] | false;
      float brightness = light["dimming"]["brightness"] | 0.0f;
      DEBUG_PRINTF("[%s] Poll: on=%s brightness=%.1f%%\n", _name, isOn ? "true" : "false", brightness);

      // apply on/off and brightness
      byte newBri = 0;
      if (isOn) {
        newBri = (byte)((brightness * 255.0f) / 100.0f);
        if (newBri < 1 && brightness > 0) newBri = 1;
      }

      bool changed = false;
      if (newBri != lastBri) {
        DEBUG_PRINTF("[%s] Brightness changed: %d -> %d\n", _name, lastBri, newBri);
        if (applyOnOff) {
          if (newBri == 0) { bri = 0; }
          else if (bri == 0 && newBri > 0) { bri = briLast; }
        }
        if (applyBri && newBri > 0) {
          bri = newBri;
        }
        lastBri = newBri;
        changed = true;
      }

      // apply color (xy)
      if (applyColor) {
        JsonObject color = light["color"];
        if (!color.isNull()) {
          float x = color["xy"]["x"] | 0.0f;
          float y = color["xy"]["y"] | 0.0f;

          if (y > 0.0f && (x != lastX || y != lastY)) {
            byte rgb[3];
            xyToRGB(x, y, rgb);
            DEBUG_PRINTF("[%s] Color xy changed: (%.4f,%.4f) -> RGB(%d,%d,%d)\n", _name, x, y, rgb[0], rgb[1], rgb[2]);
            colPri[0] = rgb[0];
            colPri[1] = rgb[1];
            colPri[2] = rgb[2];
            lastX = x;
            lastY = y;
            changed = true;
          }
        }

        // color temperature fallback (when in ct mode)
        JsonObject ct = light["color_temperature"];
        if (!ct.isNull()) {
          bool mirekValid = ct["mirek_valid"] | false;
          if (mirekValid) {
            uint16_t mirek = ct["mirek"] | 0;
            if (mirek > 0 && mirek != lastCt) {
              DEBUG_PRINTF("[%s] Color temp changed: %d -> %d mirek\n", _name, lastCt, mirek);
              colorCTtoRGB(mirek, colPri);
              lastCt = mirek;
              changed = true;
            }
          }
        }
      }

      if (changed) {
        colorUpdated(CALL_MODE_DIRECT_CHANGE);
      }

      statusStr = F("OK");
    }
#endif // HUE_HAS_TLS

  public:

    void setup() override {
      DEBUG_PRINTF("[%s] Setup: enabled=%s bridgeIp=%s\n", _name, enabled ? "true" : "false", bridgeIp);
      initDone = true;
    }

    void loop() override {
#ifdef HUE_HAS_TLS
      if (!enabled || !initDone || strip.isUpdating()) return;
      if (!WLED_CONNECTED) return;
      if (strlen(bridgeIp) == 0) return;

      unsigned long now = millis();
      if (now - lastPoll < pollInterval) return;
      lastPoll = now;

      if (attemptAuth) {
        authenticate();
      } else {
        pollLight();
      }
#endif
    }

    void addToJsonInfo(JsonObject& root) override {
      if (!enabled) return;

      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray status = user.createNestedArray(F("Hue V2"));
#ifdef HUE_HAS_TLS
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

      // char arrays must be read with strlcpy, not getJsonValue
      const char* s = nullptr;
      if (!(s = top[F("bridgeIp")])) configComplete = false; else strlcpy(bridgeIp, s, sizeof(bridgeIp));
      if (!(s = top[F("apiKey")])) configComplete = false; else strlcpy(apiKey, s, sizeof(apiKey));
      if (!(s = top[F("lightId")])) configComplete = false; else strlcpy(lightId, s, sizeof(lightId));

      DEBUG_PRINTF("[%s] Config %s: enabled=%s bridgeIp=%s pollInterval=%lu\n",
                   _name, configComplete ? "loaded" : "incomplete",
                   enabled ? "true" : "false", bridgeIp, pollInterval);

      return configComplete;
    }

    void appendConfigData() override {
#ifdef HUE_HAS_TLS
      oappend(F("addInfo('Philips Hue V2:bridgeIp',1,'Hue bridge IP address');"));
      oappend(F("addInfo('Philips Hue V2:apiKey',1,'API key (auto-filled after auth)');"));
      oappend(F("addInfo('Philips Hue V2:lightId',1,'Light resource UUID from V2 API');"));
      oappend(F("addInfo('Philips Hue V2:pollInterval',1,'ms between polls (min 1000)');"));
      oappend(F("addInfo('Philips Hue V2:attemptAuth',1,'Press link button first');"));
#endif
    }

    uint16_t getId() override {
      return USERMOD_ID_PHILIPS_HUE_V2;
    }
};

const char PhilipsHueV2Usermod::_name[]    PROGMEM = "Philips Hue V2";
const char PhilipsHueV2Usermod::_enabled[] PROGMEM = "enabled";

static PhilipsHueV2Usermod philips_hue_v2;
REGISTER_USERMOD(philips_hue_v2);
