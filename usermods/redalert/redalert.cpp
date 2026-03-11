#pragma once

#include "wled.h"
#include <HTTPClient.h>

#ifndef USERMOD_ID_PIKUD_HAOREF
// Pick some unused ID or add to usermod_id.h
#define USERMOD_ID_PIKUD_HAOREF 0xdead
#endif

class UsermodPikudHaoref : public Usermod {
private:
  // Strings reused in config/UI to save flash
  static const char _name[];
  static const char _enabled[];
  static const char _apiUrl[];
  static const char _areaName[];

  // Configurable parameters
  // Official Pikud Haoref alerts endpoint (JSON array)
  String apiUrl = "https://www.oref.org.il/WarningMessages/alert/alerts.json";
  // Area / city name to watch for (must match an entry in the 'cities' array)
  String areaName = "תל אביב - מזרח";
  bool   enabled = true;
  uint32_t pollIntervalMs = 15000;    // 15 seconds
  uint8_t alertBrightness = 255;
  uint32_t alertColor = RGBW32(255, 0, 0, 0);  // red
  bool flashAlert = true;
  uint32_t flashPeriodMs = 500;       // on/off every 500ms

  // Internal state
  unsigned long lastPoll = 0;
  bool alertActive = false;
  unsigned long alertStartMs = 0;
  int lastHttpCode = 0;

  // Helper: poll the alert endpoint
  void pollAlert() {
    if (!enabled) return;
    if (!Network.isConnected()) return;
    if (apiUrl.length() == 0) return;
    if (areaName.length() == 0) return;

    HTTPClient http;
    http.setTimeout(3000); // 3s

    http.begin(apiUrl);

    // Pikud Haoref endpoint is somewhat picky about headers; set a UA.
    http.addHeader("User-Agent", "WLED-PikudHaoref-ESP32");

    int httpCode = http.GET();
    lastHttpCode = httpCode;
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      // Response is a JSON array of alert objects, or [] when no alerts.
      DynamicJsonDocument doc(4096);
      DeserializationError err = deserializeJson(doc, payload);
      if (!err) {
        bool foundAlert = false;

        if (doc.is<JsonArray>()) {
          JsonArray alerts = doc.as<JsonArray>();
          for (JsonVariant v : alerts) {
            if (!v.is<JsonObject>()) continue;
            JsonObject alert = v.as<JsonObject>();
            JsonArray cities = alert["cities"].as<JsonArray>();
            for (JsonVariant cv : cities) {
              const char* cityName = cv.as<const char*>();
              if (!cityName) continue;
              // Simple substring match so a broader area string still works.
              if (String(cityName).indexOf(areaName) >= 0) {
                foundAlert = true;
                break;
              }
            }
            if (foundAlert) break;
          }
        }

        if (foundAlert && !alertActive) {
          alertStartMs = millis();
        }
        alertActive = foundAlert;
      }
    }
    http.end();
  }

  // Helper: render alert pattern on strip
  void renderAlertEffect() {
    if (!alertActive) return;

    uint32_t now = millis();
    bool onPhase = true;

    if (flashAlert) {
      // Simple square-wave flash
      onPhase = ((now / flashPeriodMs) % 2) == 0;
    }

    if (!onPhase) {
      // Off phase: let WLED draw normal effects
      return;
    }

    // Overwrite current frame with alert color
    // (done late in the loop to "win" over regular effects)
    uint16_t totalLen = strip.getLengthTotal();
    strip.setBrightness(alertBrightness);
    for (uint16_t i = 0; i < totalLen; i++) {
      strip.setPixelColor(i, alertColor);
    }
  }

public:
  // Called once at boot
  void setup() override {
    // Nothing special
  }

  // Called frequently; do non-blocking work here
  void loop() override {
    if (!enabled) return;

    unsigned long now = millis();

    // Periodic polling
    if (now - lastPoll >= pollIntervalMs) {
      lastPoll = now;
      pollAlert();
    }

    // Draw alert if active
    if (alertActive) {
      renderAlertEffect();
    }
  }

  // Called after WLED’s main effects finished drawing
  void handleOverlayDraw() override {
    // Alternative: do the drawing here instead of in loop()
    if (alertActive) {
      renderAlertEffect();
    }
  }

  // Provide JSON config
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));

    top[FPSTR(_enabled)]      = enabled;
    top[FPSTR(_apiUrl)]       = apiUrl;
    top[FPSTR(_areaName)]     = areaName;
    top["pollIntervalMs"]     = pollIntervalMs;
    top["alertBrightness"]    = alertBrightness;
    top["alertColor"]         = alertColor;
    top["flashAlert"]         = flashAlert;
    top["flashPeriodMs"]      = flashPeriodMs;
  }

  // Load JSON config
  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINTLN(F("PikudHaoref: No config found, using defaults."));
      return false;
    }

    bool cfg = true;
    cfg &= getJsonValue(top[FPSTR(_enabled)],   enabled, true);
    cfg &= getJsonValue(top[FPSTR(_apiUrl)],    apiUrl, apiUrl);
    cfg &= getJsonValue(top[FPSTR(_areaName)],  areaName, areaName);
    cfg &= getJsonValue(top["pollIntervalMs"],  pollIntervalMs, pollIntervalMs);
    cfg &= getJsonValue(top["alertBrightness"], alertBrightness, alertBrightness);
    cfg &= getJsonValue(top["alertColor"],      alertColor, alertColor);
    cfg &= getJsonValue(top["flashAlert"],      flashAlert, flashAlert);
    cfg &= getJsonValue(top["flashPeriodMs"],   flashPeriodMs, flashPeriodMs);

    return cfg;
  }

  // Info in /json/info and UI
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray arr = user.createNestedArray(FPSTR(_name));
    arr.add(alertActive ? F("ALERT") : F("OK"));
    arr.add(F(" Pikud Haoref"));

    JsonObject sensor = root["sensor"];
    if (sensor.isNull()) sensor = root.createNestedObject("sensor");
    JsonArray status = sensor.createNestedArray(FPSTR(_name));
    status.add(lastHttpCode);
    status.add(F(" last HTTP status"));
  }

  uint16_t getId() override {
    return USERMOD_ID_PIKUD_HAOREF;
  }
};

// flash-saving constant strings
const char UsermodPikudHaoref::_name[]     PROGMEM = "RedAlert";
const char UsermodPikudHaoref::_enabled[]  PROGMEM = "enabled";
const char UsermodPikudHaoref::_apiUrl[]   PROGMEM = "apiUrl";
const char UsermodPikudHaoref::_areaName[] PROGMEM = "areaName";

// register usermod instance
static UsermodPikudHaoref usermod_pikud_haoref;
REGISTER_USERMOD(usermod_pikud_haoref);