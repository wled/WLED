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
    static constexpr size_t RESPONSE_BUF_SIZE = 24576;

    bool enabled = false;
    bool initDone = false;

    // configuration (bridgeIp may optionally start with "http://" to force plain HTTP)
    char bridgeIp[24] = "";
    char apiKey[64] = "";
    char lightId[64] = "";
    unsigned long pollInterval = 2500;
    bool applyOnOff = true;
    bool applyBri = true;
    bool applyColor = true;
    bool attemptAuth = false;
    bool fetchLights = false;

    // state
    unsigned long lastPoll = 0;
    byte lastBri = 0;
    float lastX = 0.0f;
    float lastY = 0.0f;
    uint16_t lastCt = 0;
    String statusStr = "Idle";
    uint8_t failCount = 0;          // consecutive connection failures for backoff

    // persistent TCP client — reused to avoid socket exhaustion (errno 11)
    WiFiClient tcpClient;

    // discovered lights for dropdown — persisted to /hue_lights.json, not kept in RAM
    bool lightsDiscovered = false;  // true once a successful fetch has been saved
    char lightName[40] = "";        // display name of the currently monitored light

    // PROGMEM strings
    static const char _name[];
    static const char _enabled[];

    /*
     * Save discovered lights to /hue_lights.json for use in the settings dropdown.
     * Takes JsonArray already populated from the discovery response.
     */
    void saveLightsToFS(JsonArray data) {
      StaticJsonDocument<2048> doc;
      JsonArray arr = doc.to<JsonArray>();
      uint8_t count = 0;
      for (JsonObject light : data) {
        if (count >= 16) break;
        const char* id   = light["id"];
        const char* name = light["metadata"]["name"];
        if (!id) continue;
        JsonObject l = arr.createNestedObject();
        l[F("id")]   = id;
        l[F("name")] = name ? name : "?";
        count++;
      }
      File f = WLED_FS.open(F("/hue_lights.json"), "w");
      if (f) { serializeJson(doc, f); f.close(); }
      lightsDiscovered = (count > 0);
      DEBUG_PRINTF("[%s] Saved %u light(s) to /hue_lights.json\n", _name, count);
    }

    /*
     * Check whether /hue_lights.json exists and set the lightsDiscovered flag.
     */
    void loadLightsFromFS() {
      File f = WLED_FS.open(F("/hue_lights.json"), "r");
      lightsDiscovered = (bool)f;
      if (!f) { DEBUG_PRINTF("[%s] lightsDiscovered=false\n", _name); return; }
      // also cache the name of the currently configured light
      lightName[0] = '\0';
      if (strlen(lightId) > 0) {
        StaticJsonDocument<2048> doc;
        if (!deserializeJson(doc, f)) {
          for (JsonObject l : doc.as<JsonArray>()) {
            const char* id = l[F("id")];
            if (id && strcmp(id, lightId) == 0) {
              const char* name = l[F("name")];
              if (name) strlcpy(lightName, name, sizeof(lightName));
              break;
            }
          }
        }
      }
      f.close();
      DEBUG_PRINTF("[%s] lightsDiscovered=true lightName=%s\n", _name, lightName);
    }

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
     * Helper: extract the bare IP/hostname from bridgeIp, stripping any
     * "http://" prefix.  Returns true when plain HTTP should be used.
     */
    bool parseBridgeHost(const char* &host) const {
      if (strncmp(bridgeIp, "http://", 7) == 0) {
        host = bridgeIp + 7;
        return true;   // plain HTTP
      }
      host = bridgeIp;
      return false;     // use HTTPS
    }

    /*
     * Make an HTTP(S) request to the Hue bridge.
     * If bridgeIp starts with "http://" a plain-text WiFiClient on port 80
     * is used (useful for debugging TLS issues).  Otherwise an HTTPS
     * connection on port 443 is made using the available TLS backend.
     * Returns the response body or empty string on failure.
     */
    String httpsRequest(const char* method, const char* path, const char* body = nullptr) {
      const char* host = nullptr;
      bool usePlainHttp = parseBridgeHost(host);
      uint16_t port = usePlainHttp ? 8088 : 443;

      DEBUG_PRINTF("[%s] %s %s %s to %s:%u (free heap: %u)\n",
                   _name, usePlainHttp ? "HTTP" : "HTTPS", method, path, host, port, ESP.getFreeHeap());

      // ensure previous connection is closed
      tcpClient.stop();

      // --- create the appropriate client wrapper ---
      // TLS wrappers are heap-allocated (too large for the stack) and
      // cleaned up via unique_ptr at function exit.
      // For plain HTTP we use the persistent tcpClient directly.
      Client* client = nullptr;
      std::unique_ptr<Client> tlsOwner;         // releases heap TLS wrapper on return
      std::unique_ptr<WiFiClient> transportOwner; // releases heap WiFiClient used by BearSSL

#ifdef HUE_TLS_BACKEND_NATIVE
      if (!usePlainHttp) {
  #if __has_include(<NetworkClientSecure.h>)
        auto *tlsClient = new NetworkClientSecure;
  #else
        auto *tlsClient = new WiFiClientSecure;
  #endif
        if (!tlsClient) { DEBUG_PRINTF("[%s] TLS alloc failed\n", _name); statusStr = F("Alloc failed"); return ""; }
        tlsClient->setInsecure();
        tlsClient->setTimeout(4);
  #if __has_include(<NetworkClientSecure.h>)
        tlsClient->setHandshakeTimeout(5);
        tlsClient->setBufferSizes(4096, 1024);  // reduce TLS buffer from 32KB to 5KB
  #endif
        tlsOwner.reset(tlsClient);
        client = tlsClient;
        DEBUG_PRINTF("[%s] TLS client created (free heap: %u)\n", _name, ESP.getFreeHeap());
      }
#endif

#ifdef HUE_TLS_BACKEND_BEARSSL
      if (!usePlainHttp && !client) {
        // BearSSL wraps a WiFiClient by reference — create a fresh one on
        // the heap so it doesn't share state with the persistent tcpClient.
        auto *transport = new WiFiClient;
        if (!transport) { DEBUG_PRINTF("[%s] WiFiClient alloc failed\n", _name); statusStr = F("Alloc failed"); return ""; }
        auto *bearClient = new BearSSLClient(*transport, nullptr, 0);
        if (!bearClient) { delete transport; DEBUG_PRINTF("[%s] BearSSL alloc failed\n", _name); statusStr = F("Alloc failed"); return ""; }
        bearClient->setInsecure(BearSSLClient::SNI::Insecure);
        tlsOwner.reset(bearClient);
        // Note: transport is leaked if BearSSLClient destructor doesn't stop() the underlying client.
        // We clean it up explicitly after client->stop() below.
        transportOwner.reset(transport);
        client = bearClient;
        DEBUG_PRINTF("[%s] BearSSL client created (free heap: %u)\n", _name, ESP.getFreeHeap());
      }
#endif

      if (usePlainHttp) {
        tcpClient.setTimeout(2);
        client = &tcpClient;
      }

      if (!client) {
        DEBUG_PRINTF("[%s] No TLS backend available — use http:// prefix for plain HTTP\n", _name);
        statusStr = F("No TLS backend");
        return "";
      }

      // --- connect ---
      uint32_t heapBefore = ESP.getFreeHeap();
      if (!client->connect(host, port)) {
        int e = errno;
        uint32_t heapAfter = ESP.getFreeHeap();
        const char* errStr = (e == 11) ? "EAGAIN (no sockets)" :
                             (e == 111) ? "ECONNREFUSED" :
                             (e == 113) ? "EHOSTUNREACH" :
                             (e == 110) ? "ETIMEDOUT" : "?";
        DEBUG_PRINTF("[%s] Connection to %s:%u failed: errno %d (%s), heap %u -> %u (used %d)\n",
                     _name, host, port, e, errStr, heapBefore, heapAfter, (int)(heapBefore - heapAfter));
        statusStr = F("Connection failed");
        failCount = min((int)failCount + 4, 16);
        return "";
      }
      DEBUG_PRINTF("[%s] Connected to %s:%u\n", _name, host, port);

      // build HTTP request (use HTTP/1.0 to avoid chunked transfer encoding)
      String request = String(method) + " " + String(path) + " HTTP/1.0\r\n";
      request += "Host: " + String(host) + "\r\n";
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

      // read response — stop after 500ms of no new data to avoid blocking the loop
      String rawResponse = "";
      unsigned long idleStart = millis();
      while (client->connected()) {
        if (client->available()) {
          String line = client->readStringUntil('\n');
          rawResponse += line + "\n";
          if (rawResponse.length() > RESPONSE_BUF_SIZE) break;
          idleStart = millis();  // reset idle timer on data received
        } else {
          if (millis() - idleStart > 500) break;  // no data for 500ms — done
          yield();
        }
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

      DEBUG_PRINTF("[%s] Auth response: %s\n", _name, response.c_str());

      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, response);
      if (err) {
        DEBUG_PRINTF("[%s] Auth: JSON parse error: %s\n", _name, err.c_str());
        statusStr = "Auth JSON err: " + response.substring(0, 64);
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
     * Fetch all lights from the Hue bridge using the V2 API.
     * Results are logged to serial and stored for display in the info panel.
     */
    void discoverLights() {
      if (strlen(apiKey) == 0) {
        DEBUG_PRINTF("[%s] Discover: no API key configured\n", _name);
        statusStr = F("No API key");
        return;
      }

      DEBUG_PRINTF("[%s] Fetching light list...\n", _name);
      statusStr = F("Fetching lights...");
      String response = httpsRequest("GET", "/clip/v2/resource/light");

      if (response.length() == 0) {
        DEBUG_PRINTF("[%s] Discover: empty response\n", _name);
        return;
      }

      DEBUG_PRINTF("[%s] Discover response (%u bytes)\n", _name, response.length());

      // Use a filter to only parse the fields we need — the full response
      // can be 20 KB+ but we only care about id and name.
      StaticJsonDocument<128> filter;
      filter["errors"] = true;
      filter["data"][0]["id"] = true;
      filter["data"][0]["metadata"]["name"] = true;

      DynamicJsonDocument doc(4096);
      DeserializationError err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
      if (err) {
        DEBUG_PRINTF("[%s] Discover: JSON parse error: %s\n", _name, err.c_str());
        statusStr = "Discover JSON err: " + response.substring(0, 64);
        return;
      }

      JsonArray errors = doc["errors"];
      if (errors && errors.size() > 0) {
        const char* desc = errors[0]["description"];
        DEBUG_PRINTF("[%s] Discover: API error: %s\n", _name, desc ? desc : "unknown");
        statusStr = desc ? String(desc) : F("API error");
        return;
      }

      JsonArray data = doc["data"];
      if (!data || data.size() == 0) {
        DEBUG_PRINTF("[%s] Discover: no lights found\n", _name);
        statusStr = F("No lights found");
        return;
      }

      DEBUG_PRINTF("[%s] === Discovered %d light(s) ===\n", _name, data.size());
      for (JsonObject light : data) {
        const char* id   = light["id"];
        const char* name = light["metadata"]["name"];
        if (id) DEBUG_PRINTF("[%s]   %s - \"%s\"\n", _name, id, name ? name : "?");
      }
      DEBUG_PRINTF("[%s] === End of light list ===\n", _name);

      fetchLights = false;
      statusStr = "Found " + String(data.size()) + " light(s)";
      saveLightsToFS(data);
      serializeConfigToFS();  // persist fetchLights=false
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

      DEBUG_PRINTF("[%s] Poll response (%u bytes)\n", _name, response.length());

      // Use a filter to only parse the fields we need — a single light
      // response can be 1.5 KB+ but we only care about on/off, brightness,
      // color xy, and color temperature.
      StaticJsonDocument<256> filter;
      filter["errors"] = true;
      filter["data"][0]["on"]["on"] = true;
      filter["data"][0]["dimming"]["brightness"] = true;
      filter["data"][0]["color"]["xy"] = true;
      filter["data"][0]["color_temperature"]["mirek"] = true;
      filter["data"][0]["color_temperature"]["mirek_valid"] = true;

      // V2 responses can be large; use a dynamic document
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
      if (err) {
        DEBUG_PRINTF("[%s] Poll: JSON parse error: %s\n", _name, err.c_str());
        statusStr = "Poll JSON err: " + response.substring(0, 64);
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

      failCount = 0;  // successful poll — clear backoff
      statusStr = F("OK");
    }

  public:

    void setup() override {
#ifdef HUE_TLS_BACKEND_NATIVE
      DEBUG_PRINTF("[%s] TLS backend: native (WiFiClientSecure)\n", _name);
#elif defined(HUE_TLS_BACKEND_BEARSSL)
      DEBUG_PRINTF("[%s] TLS backend: BearSSL\n", _name);
#else
      DEBUG_PRINTF("[%s] TLS backend: none\n", _name);
#endif
      DEBUG_PRINTF("[%s] Setup: enabled=%s bridgeIp=%s\n", _name, enabled ? "true" : "false", bridgeIp);
      loadLightsFromFS();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !initDone || strip.isUpdating()) return;
      if (!WLED_CONNECTED) return;
      if (strlen(bridgeIp) == 0) return;

      unsigned long now = millis();
      if (now - lastPoll < pollInterval) return;
      lastPoll = now;

      if (attemptAuth) {
        authenticate();
      } else if (fetchLights) {
        discoverLights();
      } else {
        if (failCount > 0) {
          // back off — skip this interval and decrement counter
          DEBUG_PRINTF("[%s] Backing off, %u interval(s) remaining\n", _name, failCount);
          failCount--;
          return;
        }
        pollLight();
      }
    }

    void addToJsonInfo(JsonObject& root) override {
      if (!enabled) return;

      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray status = user.createNestedArray(F("Hue V2"));
      status.add(statusStr);

      if (strlen(lightName) > 0) {
        JsonArray row = user.createNestedArray(F("Hue Light"));
        row.add(lightName);
      }
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
      top[F("fetchLights")] = fetchLights;
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
      configComplete &= getJsonValue(top[F("fetchLights")], fetchLights, false);

      // char arrays must be read with strlcpy, not getJsonValue
      const char* s = nullptr;
      if (!(s = top[F("bridgeIp")])) configComplete = false; else strlcpy(bridgeIp, s, sizeof(bridgeIp));
      if (!(s = top[F("apiKey")])) configComplete = false; else strlcpy(apiKey, s, sizeof(apiKey));
      if (!(s = top[F("lightId")])) configComplete = false; else strlcpy(lightId, s, sizeof(lightId));

      if (initDone) loadLightsFromFS();  // refresh lightName if config updated at runtime


      DEBUG_PRINTF("[%s] Config %s: enabled=%s bridgeIp=%s pollInterval=%lu\n",
                   _name, configComplete ? "loaded" : "incomplete",
                   enabled ? "true" : "false", bridgeIp, pollInterval);

      return configComplete;
    }

    void appendConfigData() override {
      DEBUG_PRINTF("[%s] appendConfigData: lightsDiscovered=%s\n", _name, lightsDiscovered ? "true" : "false");
      oappend(F("addInfo('Philips Hue V2:bridgeIp',1,'IP or http://IP for plain HTTP');"));
      oappend(F("addInfo('Philips Hue V2:apiKey',1,'API key (auto-filled after auth)');"));

      // build dropdown for lightId if lights have been discovered
      if (lightsDiscovered) {
        File f = WLED_FS.open(F("/hue_lights.json"), "r");
        if (f) {
          StaticJsonDocument<2048> doc;
          if (!deserializeJson(doc, f)) {
            oappend(F("dd=addDropdown('Philips Hue V2','lightId');"));
            oappend(F("addOption(dd,'(select a light)','');"));
            for (JsonObject l : doc.as<JsonArray>()) {
              const char* id   = l[F("id")];
              const char* name = l[F("name")];
              if (!id) continue;
              oappend(F("addOption(dd,'"));
              String safeName = String(name ? name : "?");
              safeName.replace("'", "\\'");
              oappend(safeName.c_str());
              oappend(F("','"));
              oappend(id);
              oappend(F("');"));
            }
          }
          f.close();
        }
      } else {
        oappend(F("addInfo('Philips Hue V2:lightId',1,'Use Fetch Lights to populate list');"));
      }

      oappend(F("addInfo('Philips Hue V2:pollInterval',1,'ms between polls (min 1000)');"));
      oappend(F("addInfo('Philips Hue V2:attemptAuth',1,'Press link button first');"));
      oappend(F("addInfo('Philips Hue V2:fetchLights',1,'List lights (see Info tab)');"));
    }

    uint16_t getId() override {
      return USERMOD_ID_PHILIPS_HUE_V2;
    }
};

const char PhilipsHueV2Usermod::_name[]    PROGMEM = "Philips Hue V2";
const char PhilipsHueV2Usermod::_enabled[] PROGMEM = "enabled";

static PhilipsHueV2Usermod philips_hue_v2;
REGISTER_USERMOD(philips_hue_v2);
