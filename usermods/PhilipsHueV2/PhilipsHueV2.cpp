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
    #define HUE_NATIVE_IS_NETWORK_CLIENT  // has setBufferSizes, setHandshakeTimeout
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
    bool mdnsDiscovered = false;    // true if bridgeIp was auto-filled via mDNS
    unsigned long lastMdnsAttempt = 0; // rate-limit mDNS queries

    // persistent TCP client — reused to avoid socket exhaustion (errno 11)
    WiFiClient tcpClient;

    // --- SSE (Server-Sent Events) stream ---
    enum SseState : uint8_t { SSE_DISABLED, SSE_DISCONNECTED, SSE_READING_HEADERS, SSE_STREAMING };
    SseState sseState = SSE_DISCONNECTED;
    Client* sseClient = nullptr;                          // points into sseTlsOwner or ssePlainClient
    std::unique_ptr<Client> sseTlsOwner;                  // owns heap-allocated TLS wrapper
    std::unique_ptr<WiFiClient> sseTransportOwner;        // owns heap WiFiClient for BearSSL
    WiFiClient ssePlainClient;                            // plain HTTP persistent client
    String sseLineBuf;                                    // partial line accumulator
    unsigned long sseLastDataTime = 0;                    // last time data was received
    unsigned long sseReconnectTime = 0;                   // next reconnect attempt
    unsigned long sseReconnectInterval = 5000;            // backoff interval (grows on failure)
    bool sseInitialPollDone = false;                      // true after first full state poll
    bool sseEnabled = true;                               // config: use SSE instead of polling

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
      //coordinates to rgb (https://www.developers.meethue.com/documentation/color-conversions-rgb-xy)
      float z = 1.0f - x - y;
      float X = (1.0f / y) * x;
      float Z = (1.0f / y) * z;
      float r = (int)255 * (X * 1.656492f - 0.354851f - Z * 0.255038f);
      float g = (int)255 * (-X * 0.707196f + 1.655397f + Z * 0.036152f);
      float b = (int)255 * (X * 0.051713f - 0.121364f + Z * 1.011530f);
      if (r > b && r > g && r > 1.0f)
      {
        // red is too big
        g = g / r;
        b = b / r;
        r = 1.0f;
      }
      else if (g > b && g > r && g > 1.0f)
      {
        // green is too big
        r = r / g;
        b = b / g;
        g = 1.0f;
      }
      else if (b > r && b > g && b > 1.0f)
      {
        // blue is too big
        r = r / b;
        g = g / b;
        b = 1.0f;
      }
      // Apply gamma correction
      r = r <= 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * powf(r, (1.0f / 2.4f)) - 0.055f;
      g = g <= 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * powf(g, (1.0f / 2.4f)) - 0.055f;
      b = b <= 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * powf(b, (1.0f / 2.4f)) - 0.055f;

      if (r > b && r > g)
      {
        // red is biggest
        if (r > 1.0f)
        {
          g = g / r;
          b = b / r;
          r = 1.0f;
        }
      }
      else if (g > b && g > r)
      {
        // green is biggest
        if (g > 1.0f)
        {
          r = r / g;
          b = b / g;
          g = 1.0f;
        }
      }
      else if (b > r && b > g)
      {
        // blue is biggest
        if (b > 1.0f)
        {
          r = r / b;
          g = g / b;
          b = 1.0f;
        }
      }
      rgb[0] = byte(255.0f * r);
      rgb[1] = byte(255.0f * g);
      rgb[2] = byte(255.0f * b);
    }

    /*
     * Attempt to discover a Philips Hue bridge on the local network via mDNS.
     * The bridge advertises itself as _hue._tcp.
     * Returns true if a bridge was found and bridgeIp was populated.
     */
    bool discoverBridgeMdns() {
      DEBUG_PRINTF("[%s] mDNS: querying for _hue._tcp ...\n", _name);
      statusStr = F("mDNS searching...");

      int n = MDNS.queryService("hue", "tcp");
      if (n <= 0) {
        DEBUG_PRINTF("[%s] mDNS: no Hue bridge found\n", _name);
        statusStr = F("No bridge found (mDNS)");
        return false;
      }

      // use the first result
      IPAddress ip = MDNS.IP(0);
      snprintf(bridgeIp, sizeof(bridgeIp), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      mdnsDiscovered = true;
      DEBUG_PRINTF("[%s] mDNS: found Hue bridge at %s (port %d)\n", _name, bridgeIp, MDNS.port(0));
      statusStr = "Bridge found: " + String(bridgeIp);
      return true;
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
  #ifdef HUE_NATIVE_IS_NETWORK_CLIENT
        auto *tlsClient = new NetworkClientSecure;
  #else
        auto *tlsClient = new WiFiClientSecure;
  #endif
        if (!tlsClient) { DEBUG_PRINTF("[%s] TLS alloc failed\n", _name); statusStr = F("Alloc failed"); return ""; }
        tlsClient->setInsecure();
        tlsClient->setTimeout(4);
  #ifdef HUE_NATIVE_IS_NETWORK_CLIENT
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

      // read response
      // First wait for data with a longer timeout, then drain everything.
      String rawResponse = "";
      unsigned long startTime = millis();
      // wait up to 5s for the first byte
      while (!client->available() && client->connected() && millis() - startTime < 5000) {
        yield();
      }
      // read while connected or data still in buffer
      unsigned long idleStart = millis();
      while (client->connected() || client->available()) {
        if (client->available()) {
          String line = client->readStringUntil('\n');
          rawResponse += line + "\n";
          if (rawResponse.length() > RESPONSE_BUF_SIZE) break;
          idleStart = millis();
        } else {
          if (millis() - idleStart > 1000) break;  // no data for 1s — done
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
     * Apply a parsed light JSON object to WLED state.
     * Used by both pollLight() and SSE event handler.
     * Returns true if any state changed.
     */
    bool applyLightState(JsonObject light) {
      bool isOn = light["on"]["on"] | false;
      float brightness = light["dimming"]["brightness"] | -1.0f;

      // if the event doesn't contain on/dimming, keep current state
      bool hasOn = !light["on"].isNull();
      bool hasBri = !light["dimming"].isNull();

      byte newBri = lastBri;
      if (hasOn || hasBri) {
        if (hasOn && !isOn) {
          newBri = 0;
        } else if (hasBri && brightness >= 0) {
          newBri = (byte)((brightness * 255.0f) / 100.0f);
          if (newBri < 1 && brightness > 0) newBri = 1;
        }
        if (hasOn && isOn && !hasBri && lastBri == 0) {
          newBri = briLast > 0 ? briLast : 128;  // turned on but no brightness in event
        }
        DEBUG_PRINTF("[%s] State: on=%s brightness=%.1f%% -> bri=%d\n",
                     _name, hasOn ? (isOn ? "true" : "false") : "n/a",
                     hasBri ? brightness : -1.0f, newBri);
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
      return changed;
    }

    // ======================= SSE (Server-Sent Events) =======================

    /*
     * Open a persistent HTTP(S) connection to the Hue SSE endpoint.
     * The bridge streams light change events in real-time.
     */
    void sseConnect() {
      const char* host = nullptr;
      bool usePlainHttp = parseBridgeHost(host);
      uint16_t port = usePlainHttp ? 8088 : 443;

      DEBUG_PRINTF("[%s] SSE: connecting to %s:%u (free heap: %u)\n", _name, host, port, ESP.getFreeHeap());

      // clean up any previous connection (no reconnect schedule — we're connecting now)
      sseDisconnect(false);

      // --- create the appropriate client ---
      if (usePlainHttp) {
        ssePlainClient.setTimeout(2);
        sseClient = &ssePlainClient;
      }
#ifdef HUE_TLS_BACKEND_NATIVE
      else {
  #ifdef HUE_NATIVE_IS_NETWORK_CLIENT
        auto *tlsClient = new NetworkClientSecure;
  #else
        auto *tlsClient = new WiFiClientSecure;
  #endif
        if (!tlsClient) { DEBUG_PRINTF("[%s] SSE: TLS alloc failed\n", _name); statusStr = F("SSE alloc failed"); return; }
        tlsClient->setInsecure();
        tlsClient->setTimeout(4);
  #ifdef HUE_NATIVE_IS_NETWORK_CLIENT
        tlsClient->setHandshakeTimeout(5);
        tlsClient->setBufferSizes(4096, 1024);
  #endif
        sseTlsOwner.reset(tlsClient);
        sseClient = tlsClient;
      }
#endif
#ifdef HUE_TLS_BACKEND_BEARSSL
      if (!usePlainHttp && !sseClient) {
        auto *transport = new WiFiClient;
        if (!transport) { DEBUG_PRINTF("[%s] SSE: WiFiClient alloc failed\n", _name); statusStr = F("SSE alloc failed"); return; }
        auto *bearClient = new BearSSLClient(*transport, nullptr, 0);
        if (!bearClient) { delete transport; DEBUG_PRINTF("[%s] SSE: BearSSL alloc failed\n", _name); statusStr = F("SSE alloc failed"); return; }
        bearClient->setInsecure(BearSSLClient::SNI::Insecure);
        sseTlsOwner.reset(bearClient);
        sseTransportOwner.reset(transport);
        sseClient = bearClient;
      }
#endif

      if (!sseClient) {
        DEBUG_PRINTF("[%s] SSE: no TLS backend\n", _name);
        statusStr = F("No TLS backend");
        sseState = SSE_DISABLED;
        return;
      }

      // --- connect ---
      if (!sseClient->connect(host, port)) {
        int e = errno;
        DEBUG_PRINTF("[%s] SSE: connect failed (errno %d, heap: %u)\n", _name, e, ESP.getFreeHeap());
        statusStr = F("SSE connect failed");
        sseDisconnect();
        return;
      }

      DEBUG_PRINTF("[%s] SSE: connected, sending request\n", _name);

      // Send HTTP/1.1 request for SSE stream
      String req = F("GET /eventstream/clip/v2 HTTP/1.1\r\n");
      req += "Host: " + String(host) + "\r\n";
      req += "hue-application-key: " + String(apiKey) + "\r\n";
      req += F("Accept: text/event-stream\r\n");
      req += F("Connection: keep-alive\r\n\r\n");
      sseClient->print(req);

      sseState = SSE_READING_HEADERS;
      sseLastDataTime = millis();
      sseLineBuf = "";
      sseLineBuf.reserve(512);
      sseReconnectInterval = 5000;  // reset backoff on successful connect
      statusStr = F("SSE connecting...");
    }

    /*
     * Cleanly tear down the SSE connection and free TLS memory.
     * scheduleReconnect=true (default) arms the backoff timer so loop() will
     * reconnect after sseReconnectInterval ms.  Pass false when intentionally
     * shutting down (e.g. config change, WiFi lost) to suppress auto-reconnect.
     */
    void sseDisconnect(bool scheduleReconnect = true) {
      if (sseClient) {
        sseClient->stop();
        sseClient = nullptr;
      }
      sseTlsOwner.reset();
      sseTransportOwner.reset();
      ssePlainClient.stop();
      sseLineBuf = "";
      bool wasConnected = (sseState == SSE_STREAMING || sseState == SSE_READING_HEADERS);
      sseState = SSE_DISCONNECTED;
      sseInitialPollDone = false;

      if (scheduleReconnect) {
        sseReconnectTime = millis() + sseReconnectInterval;
        // increase backoff for next failure, capped at 60s
        sseReconnectInterval = min(sseReconnectInterval * 2, 60000UL);
        if (wasConnected) {
          DEBUG_PRINTF("[%s] SSE: disconnected, reconnecting in %lums\n", _name, sseReconnectInterval);
        } else {
          DEBUG_PRINTF("[%s] SSE: connect failed, retry in %lums\n", _name, sseReconnectInterval);
        }
      } else {
        sseReconnectTime = ULONG_MAX;  // never auto-reconnect
        DEBUG_PRINTF("[%s] SSE: shut down\n", _name);
      }
    }

    /*
     * Handle a complete SSE "data:" payload — a JSON array of events.
     * Each event with type "update" that matches our lightId gets applied.
     */
    void handleSseEvent(const String& data) {
      // SSE data is a JSON array: [{"creationtime":..., "data":[{light_state}], "id":..., "type":"update"}]
      StaticJsonDocument<256> filter;
      filter[0]["type"] = true;
      filter[0]["data"][0]["id"] = true;
      filter[0]["data"][0]["on"]["on"] = true;
      filter[0]["data"][0]["dimming"]["brightness"] = true;
      filter[0]["data"][0]["color"]["xy"] = true;
      filter[0]["data"][0]["color_temperature"]["mirek"] = true;
      filter[0]["data"][0]["color_temperature"]["mirek_valid"] = true;

      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, data, DeserializationOption::Filter(filter));
      if (err) {
        DEBUG_PRINTF("[%s] SSE: JSON parse error: %s\n", _name, err.c_str());
        return;
      }

      JsonArray events = doc.as<JsonArray>();
      for (JsonObject event : events) {
        const char* type = event["type"];
        if (!type || strcmp(type, "update") != 0) continue;

        JsonArray eventData = event["data"];
        for (JsonObject item : eventData) {
          const char* id = item["id"];
          if (!id || strcmp(id, lightId) != 0) continue;

          DEBUG_PRINTF("[%s] SSE: event for our light\n", _name);
          if (applyLightState(item)) {
            statusStr = F("SSE OK");
          }
        }
      }
    }

    /*
     * Non-blocking SSE stream reader. Called every loop() iteration.
     * Reads available bytes, assembles lines, and dispatches events.
     */
    void ssePoll() {
      if (!sseClient || sseState == SSE_DISCONNECTED || sseState == SSE_DISABLED) return;

      // check connection health
      if (!sseClient->connected()) {
        DEBUG_PRINTF("[%s] SSE: connection lost\n", _name);
        sseDisconnect();
        statusStr = F("SSE disconnected");
        return;
      }

      // inactivity timeout — Hue bridge should send at least a comment every ~30s
      if (millis() - sseLastDataTime > 60000) {
        DEBUG_PRINTF("[%s] SSE: inactivity timeout\n", _name);
        sseDisconnect();
        statusStr = F("SSE timeout");
        return;
      }

      // read up to 512 bytes per loop iteration to stay non-blocking
      int bytesRead = 0;
      while (sseClient->available() && bytesRead < 512) {
        char c = sseClient->read();
        bytesRead++;
        sseLastDataTime = millis();

        if (c == '\n') {
          // complete line received
          sseLineBuf.trim();  // remove \r

          if (sseState == SSE_READING_HEADERS) {
            if (sseLineBuf.length() == 0) {
              // empty line = end of headers
              DEBUG_PRINTF("[%s] SSE: headers complete, streaming\n", _name);
              sseState = SSE_STREAMING;
              statusStr = F("SSE streaming");
            } else if (sseLineBuf.startsWith(F("HTTP/"))) {
              // validate status code
              int code = sseLineBuf.substring(9, 12).toInt();
              if (code != 200) {
                DEBUG_PRINTF("[%s] SSE: bad status: %s\n", _name, sseLineBuf.c_str());
                sseDisconnect();
                statusStr = F("SSE bad status");
                return;
              }
              DEBUG_PRINTF("[%s] SSE: %s\n", _name, sseLineBuf.c_str());
            }
          } else if (sseState == SSE_STREAMING) {
            // skip chunked encoding size lines (hex-only lines)
            if (sseLineBuf.length() > 0 && sseLineBuf.length() <= 8) {
              bool isChunkSize = true;
              for (unsigned i = 0; i < sseLineBuf.length(); i++) {
                char ch = sseLineBuf.charAt(i);
                if (!isxdigit(ch)) { isChunkSize = false; break; }
              }
              if (isChunkSize) { sseLineBuf = ""; continue; }
            }

            if (sseLineBuf.startsWith(F("data: "))) {
              String payload = sseLineBuf.substring(6);
              handleSseEvent(payload);
            } else if (sseLineBuf.startsWith(F("id: "))) {
              // SSE event ID — ignore
            } else if (sseLineBuf.startsWith(F(": "))) {
              // SSE comment / keep-alive — ignore
              DEBUG_PRINTF("[%s] SSE: keep-alive\n", _name);
            }
            // empty line = end of SSE event block (already handled by dispatching on data: line)
          }

          sseLineBuf = "";
        } else {
          if (sseLineBuf.length() < 4096) {
            sseLineBuf += c;
          }
          // else: line too long, discard extra characters
        }
      }
    }

    // ======================== End SSE ========================

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

      StaticJsonDocument<256> filter;
      filter["errors"] = true;
      filter["data"][0]["on"]["on"] = true;
      filter["data"][0]["dimming"]["brightness"] = true;
      filter["data"][0]["color"]["xy"] = true;
      filter["data"][0]["color_temperature"]["mirek"] = true;
      filter["data"][0]["color_temperature"]["mirek_valid"] = true;

      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
      if (err) {
        DEBUG_PRINTF("[%s] Poll: JSON parse error: %s\n", _name, err.c_str());
        statusStr = "Poll JSON err";
        return;
      }

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

      applyLightState(data[0]);
      failCount = 0;
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
      if (!WLED_CONNECTED) {
        if (sseState != SSE_DISCONNECTED) sseDisconnect(false);
        return;
      }
      if (strlen(bridgeIp) == 0) {
        // no bridge configured — try mDNS discovery (rate-limited)
        unsigned long now = millis();
        if (now - lastMdnsAttempt >= 30000) {
          lastMdnsAttempt = now;
          discoverBridgeMdns();
        }
        return;
      }

      // --- SSE: non-blocking read every iteration ---
      if (sseEnabled && sseState != SSE_DISABLED) {
        ssePoll();

        // if SSE is connected and streaming, do an initial poll for full state sync
        if (sseState == SSE_STREAMING && !sseInitialPollDone) {
          unsigned long now = millis();
          if (now - lastPoll >= pollInterval) {
            lastPoll = now;
            pollLight();
            sseInitialPollDone = true;
          }
        }

        // if SSE is disconnected, try to (re)connect when the backoff timer fires
        if (sseState == SSE_DISCONNECTED && strlen(apiKey) > 0 && strlen(lightId) > 0) {
          unsigned long now = millis();
          if (now >= sseReconnectTime) {
            sseConnect();
            if (sseState == SSE_DISCONNECTED) {
              // sseConnect failed — sseDisconnect() already scheduled next retry
            } else {
              // connected — reset interval for any future disconnection
              sseReconnectInterval = 5000;
            }
          }
        }
      }

      // --- Timed actions: auth, fetch lights, or polling fallback ---
      unsigned long now = millis();
      if (now - lastPoll < pollInterval) return;
      lastPoll = now;

      if (attemptAuth) {
        authenticate();
      } else if (fetchLights) {
        discoverLights();
      } else if (!sseEnabled || sseState == SSE_DISABLED) {
        // polling fallback when SSE is disabled
        if (failCount > 0) {
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

      if (sseEnabled) {
        JsonArray sseRow = user.createNestedArray(F("Hue SSE"));
        const char* sseStr = (sseState == SSE_STREAMING) ? "Streaming" :
                             (sseState == SSE_READING_HEADERS) ? "Connecting..." :
                             (sseState == SSE_DISABLED) ? "No TLS" : "Disconnected";
        sseRow.add(sseStr);
      }

      if (strlen(lightName) > 0) {
        JsonArray row = user.createNestedArray(F("Hue Light"));
        row.add(lightName);
      }

      if (strlen(bridgeIp) > 0) {
        JsonArray row = user.createNestedArray(F("Hue Bridge"));
        String label = String(bridgeIp);
        if (mdnsDiscovered) label += F(" (mDNS)");
        row.add(label);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[F("bridgeIp")] = mdnsDiscovered ? "" : bridgeIp;  // don't persist mDNS-discovered IP
      top[F("apiKey")] = apiKey;
      top[F("lightId")] = lightId;
      top[F("pollInterval")] = pollInterval;
      top[F("applyOnOff")] = applyOnOff;
      top[F("applyBri")] = applyBri;
      top[F("applyColor")] = applyColor;
      top[F("attemptAuth")] = attemptAuth;
      top[F("fetchLights")] = fetchLights;
      top[F("sseEnabled")] = sseEnabled;
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
      configComplete &= getJsonValue(top[F("sseEnabled")], sseEnabled, true);

      // char arrays must be read with strlcpy, not getJsonValue
      const char* s = nullptr;
      if (!(s = top[F("bridgeIp")])) configComplete = false; else strlcpy(bridgeIp, s, sizeof(bridgeIp));
      if (!(s = top[F("apiKey")])) configComplete = false; else strlcpy(apiKey, s, sizeof(apiKey));
      if (!(s = top[F("lightId")])) configComplete = false; else strlcpy(lightId, s, sizeof(lightId));

      // if user manually set a bridge IP, clear the mDNS flag
      if (strlen(bridgeIp) > 0) mdnsDiscovered = false;

      // disconnect SSE on config change so it reconnects with new settings
      if (initDone) {
        sseDisconnect(false);
        sseReconnectTime = 0;   // reconnect immediately with new settings
        sseReconnectInterval = 5000;  // reset backoff
        loadLightsFromFS();
      }


      DEBUG_PRINTF("[%s] Config %s: enabled=%s bridgeIp=%s pollInterval=%lu\n",
                   _name, configComplete ? "loaded" : "incomplete",
                   enabled ? "true" : "false", bridgeIp, pollInterval);

      return configComplete;
    }

    void appendConfigData() override {
      DEBUG_PRINTF("[%s] appendConfigData: lightsDiscovered=%s\n", _name, lightsDiscovered ? "true" : "false");
      oappend(F("addInfo('Philips Hue V2:bridgeIp',1,'Leave empty for mDNS auto-discovery');"));
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
      oappend(F("addInfo('Philips Hue V2:sseEnabled',1,'Real-time events (vs polling)');"));
    }

    uint16_t getId() override {
      return USERMOD_ID_PHILIPS_HUE_V2;
    }
};

const char PhilipsHueV2Usermod::_name[]    PROGMEM = "Philips Hue V2";
const char PhilipsHueV2Usermod::_enabled[] PROGMEM = "enabled";

static PhilipsHueV2Usermod philips_hue_v2;
REGISTER_USERMOD(philips_hue_v2);
