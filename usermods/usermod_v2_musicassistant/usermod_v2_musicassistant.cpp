#include "wled.h"

/*
 * Polls a Music Assistant server for the currently-playing track on a configured
 * player queue. When the track's cover art changes, fetches the JPEG from MA's
 * /imageproxy endpoint (unauthenticated) and hands it to the core "Image" effect's
 * live-render path (setLiveCoverArtFromStream(), wled00/image_loader.cpp), which is
 * only compiled in when WLED_ENABLE_JPEG is defined (see platformio.ini).
 *
 * By default this usermod only keeps that buffer fed - the user selects the "Image"
 * effect and ticks its "Live" checkbox on whichever segment/matrix should show cover
 * art. If "Auto switch effect" is enabled, it will also force the configured segment
 * into Image+Live whenever playback starts and a cover art image is available.
 */
class MusicAssistantUsermod : public Usermod {
  private:
    bool     enabled        = false;
    String   host           = "mass";
    uint16_t port           = 8095;
    String   token          = "";
    String   queueId        = "";
    uint32_t pollIntervalMs = 4000;
    uint16_t imageSize      = 80; // must be one of MA's supported sizes: 0,80,160,256,512,1024
    bool     autoSwitchEffect = false;
    uint8_t  segmentId        = 0; // which segment to control when autoSwitchEffect is on

    WiFiClient client;
    unsigned long lastPoll = 0;
    String lastProxyId = "";
    bool wasPlaying = false;
    char errorMessage[96] = "";

    static const char _name[];

    bool beginRequest(const __FlashStringHelper *method, const String &path, bool withAuth) {
      if (!client.connect(host.c_str(), port)) {
        strlcpy(errorMessage, "connection failed", sizeof(errorMessage));
        return false;
      }
      client.print(method); client.print(' '); client.print(path); client.println(F(" HTTP/1.1"));
      client.print(F("Host: ")); client.println(host);
      if (withAuth && token.length()) {
        client.print(F("Authorization: Bearer ")); client.println(token);
      }
      client.println(F("Connection: close"));
      return true;
    }

    // reads the status line + headers of a response already sent by the caller; leaves 'client' positioned at the body
    bool readStatusAndHeaders(int &statusCode, long &contentLength) {
      statusCode = 0;
      contentLength = -1;
      client.setTimeout(2000); // this blocks loop()/rendering while waiting - keep it short
      String statusLine = client.readStringUntil('\n');
      statusLine.trim();
      if (statusLine.length() == 0) {
        strlcpy(errorMessage, "no response", sizeof(errorMessage));
        return false;
      }
      int sp = statusLine.indexOf(' ');
      if (sp > 0) statusCode = statusLine.substring(sp + 1).toInt();
      while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break; // blank line: end of headers
        if (line.startsWith(F("Content-Length:"))) contentLength = line.substring(line.indexOf(':') + 1).toInt();
      }
      return true;
    }

    void pollQueue() {
      if (queueId.isEmpty()) return;
      if (!beginRequest(F("POST"), F("/api"), true)) return;

      StaticJsonDocument<160> body;
      body["command"] = "player_queues/get";
      body["args"]["queue_id"] = queueId;
      String bodyStr;
      serializeJson(body, bodyStr);

      client.println(F("Content-Type: application/json"));
      client.print(F("Content-Length: ")); client.println(bodyStr.length());
      client.println();
      client.print(bodyStr);

      int statusCode; long contentLength;
      if (!readStatusAndHeaders(statusCode, contentLength) || statusCode != 200) {
        if (statusCode) { snprintf(errorMessage, sizeof(errorMessage), "HTTP %d from /api", statusCode); }
        client.stop();
        return;
      }

      // Filtered parse: PlayerQueue responses also include the full media_item/streamdetails
      // tree (many KB) which we don't need and can't afford to buffer on an ESP32.
      StaticJsonDocument<192> filter;
      filter["state"] = true;
      filter["current_item"]["image"]["proxy_id"] = true;
      DynamicJsonDocument doc(768);
      DeserializationError err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
      client.stop();
      if (err) {
        strlcpy(errorMessage, err.c_str(), sizeof(errorMessage));
        return;
      }

      const char* state   = doc["state"] | "";
      const char* proxyId = doc["current_item"]["image"]["proxy_id"] | "";
      bool nowPlaying = strcmp(state, "playing") == 0 && proxyId[0];

      if (autoSwitchEffect && nowPlaying && !wasPlaying) switchToImageEffect();
      wasPlaying = nowPlaying;

      if (!nowPlaying) return; // nothing playing: leave last art showing

      if (lastProxyId != proxyId) {
        lastProxyId = proxyId;
        fetchCoverArt(proxyId);
      } else {
        errorMessage[0] = '\0';
      }
    }

    void switchToImageEffect() {
      // getSegment() silently falls back to the main segment for an out-of-range id,
      // rather than crashing - so a stale/typo'd segmentId degrades gracefully.
      Segment &seg = strip.getSegment(segmentId);
      if (!seg.isActive()) return;
      if (seg.mode != FX_MODE_IMAGE) seg.setMode(FX_MODE_IMAGE);
      seg.check1 = true;
    }

    void fetchCoverArt(const String &proxyId) {
      #if defined(WLED_ENABLE_GIF) && defined(WLED_ENABLE_JPEG)
      String path = F("/imageproxy/");
      path += proxyId;
      path += F("?size=");
      path += imageSize;
      if (!beginRequest(F("GET"), path, false)) return;
      client.println();

      int statusCode; long contentLength;
      if (!readStatusAndHeaders(statusCode, contentLength)
          || statusCode != 200 || contentLength <= 0 || contentLength > LIVE_COVERART_MAX_BYTES) {
        strlcpy(errorMessage, "imageproxy fetch failed", sizeof(errorMessage));
        client.stop();
        return;
      }

      bool ok = setLiveCoverArtFromStream(client, (size_t)contentLength);
      client.stop();
      strlcpy(errorMessage, ok ? "" : "short read from imageproxy", sizeof(errorMessage));
      #else
      strlcpy(errorMessage, "core built without WLED_ENABLE_JPEG", sizeof(errorMessage));
      #endif
    }

  public:
    void setup() {}
    void connected() {}

    void loop() {
      if (!enabled || !WLED_CONNECTED) return;
      if (millis() - lastPoll < pollIntervalMs) return;
      lastPoll = millis();
      pollQueue();
    }

    void addToConfig(JsonObject &root) {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("enabled")]        = enabled;
      top[F("host")]           = host;
      top[F("port")]           = port;
      top[F("token")]          = token;
      top[F("queueId")]        = queueId;
      top[F("pollIntervalMs")]   = pollIntervalMs;
      top[F("imageSize")]        = imageSize;
      top[F("autoSwitchEffect")] = autoSwitchEffect;
      top[F("segmentId")]        = segmentId;
    }

    bool readFromConfig(JsonObject &root) {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[F("enabled")],        enabled);
      configComplete &= getJsonValue(top[F("host")],           host);
      configComplete &= getJsonValue(top[F("port")],           port);
      configComplete &= getJsonValue(top[F("token")],          token);
      configComplete &= getJsonValue(top[F("queueId")],        queueId);
      configComplete &= getJsonValue(top[F("pollIntervalMs")],   pollIntervalMs);
      configComplete &= getJsonValue(top[F("imageSize")],        imageSize);
      configComplete &= getJsonValue(top[F("autoSwitchEffect")], autoSwitchEffect);
      configComplete &= getJsonValue(top[F("segmentId")],        segmentId);
      return configComplete;
    }

    void addToJsonInfo(JsonObject &root) {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray infoArr = user.createNestedArray(FPSTR(_name));
      if (!enabled) { infoArr.add(F("disabled")); return; }
      if (errorMessage[0]) infoArr.add(errorMessage);
      else infoArr.add(lastProxyId.length() ? F("ok") : F("waiting for playback"));
    }

    uint16_t getId() { return USERMOD_ID_MUSICASSISTANT; }
};

const char MusicAssistantUsermod::_name[] PROGMEM = "MusicAssistant";

static MusicAssistantUsermod musicassistant_usermod;
REGISTER_USERMOD(musicassistant_usermod);
