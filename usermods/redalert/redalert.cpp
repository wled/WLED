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
  static const char _alertEnabled[];
  static const char _preAlertEnabled[];
  static const char _endEnabled[];

  static const char _alertPreset[];
  static const char _preAlertPreset[];
  static const char _endPreset[];

  static const char _idleTimeoutSec[];
  static const char _idlePreset[];
  static const char _resetUrl[];

  enum AlertState : uint8_t {
    STATE_OK = 0,
    STATE_PRE_ALERT = 1,
    STATE_ALERT = 2,
    STATE_END = 3
  };

  bool initDone = false;

  // Configurable parameters
  // Official Pikud Haoref alerts endpoint (JSON array)
  String apiUrl = "https://www.oref.org.il/WarningMessages/alert/alerts.json";
  // Area / city name to watch for (must match an entry in the 'cities' array)
  String areaName = "תל אביב - מזרח";
  bool   enabled = true;
  uint32_t pollIntervalMs = 15000;    // 15 seconds

  // Per-state toggles and presets
  bool    enableAlert     = true;
  bool    enablePreAlert  = false;
  bool    enableEnd       = false;
  uint8_t alertPreset     = 0;
  uint8_t preAlertPreset  = 0;
  uint8_t endPreset       = 0;

  // Idle timeout handling (seconds + preset)
  uint32_t idleTimeoutSec = 0;        // 0 = disabled
  uint8_t  idlePreset     = 0;

  // Internal state
  unsigned long lastPoll = 0;
  int lastHttpCode = 0;

  AlertState currentState = STATE_OK;
  unsigned long lastStateChangeMs = 0;
  bool idlePresetApplied = false;

  void updateState(AlertState newState) {
    if (newState == currentState) return;

    currentState = newState;
    lastStateChangeMs = millis();
    idlePresetApplied = false;

    if (!initDone) return; // prevent crashes at boot

    switch (currentState) {
      case STATE_ALERT:
        if (enableAlert && alertPreset > 0) {
          applyPreset(alertPreset);
        }
        break;
      case STATE_PRE_ALERT:
        if (enablePreAlert && preAlertPreset > 0) {
          applyPreset(preAlertPreset);
        }
        break;
      case STATE_END:
        if (enableEnd && endPreset > 0) {
          applyPreset(endPreset);
        }
        break;
      case STATE_OK:
      default:
        // No automatic preset on OK; user can rely on idlePreset instead.
        break;
    }
  }

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
      AlertState newState = STATE_OK;
      String payload = http.getString();

      // Response is a JSON array of alert objects, or [] when no alerts.
      DynamicJsonDocument doc(4096);
      DeserializationError err = deserializeJson(doc, payload);
      if (!err) {
        if (doc.is<JsonArray>()) {
          JsonArray alerts = doc.as<JsonArray>();
          for (JsonVariant v : alerts) {
            if (!v.is<JsonObject>()) continue;
            JsonObject alert = v.as<JsonObject>();
            // Expecting structure similar to oref_alert integration:
            // "cities": [ "area1", ... ], "category": <int>, etc.
            JsonArray cities = alert["cities"].as<JsonArray>();
            for (JsonVariant cv : cities) {
              const char* cityName = cv.as<const char*>();
              if (!cityName) continue;
              // Simple substring match so a broader area string still works.
              if (String(cityName).indexOf(areaName) >= 0) {
                int category = alert["category"] | 0;
                if (category == 14) {
                  newState = STATE_PRE_ALERT;  // "pre_alert"
                } else if (category == 13) {
                  newState = STATE_END;        // "end"
                } else {
                  newState = STATE_ALERT;      // main "alert"
                }
                break;
              }
            }
            if (newState != STATE_OK) break;
          }
        }
        updateState(newState);
      }
    }
    http.end();
  }

public:
  // Called once at boot
  void setup() override {
    initDone = true;
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

    // Idle timeout handling: if state hasn't changed for configured time,
    // optionally apply a fallback preset once.
    if (initDone && idleTimeoutSec > 0 && idlePreset > 0 && lastStateChangeMs > 0 && !idlePresetApplied) {
      if (now - lastStateChangeMs >= idleTimeoutSec * 1000UL) {
        applyPreset(idlePreset);
        idlePresetApplied = true;
      }
    }

  }

  // Provide JSON config
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));

    top[FPSTR(_enabled)]      = enabled;
    top[FPSTR(_apiUrl)]       = apiUrl;
    top[FPSTR(_areaName)]     = areaName;
    top["pollIntervalMs"]     = pollIntervalMs;

    top[FPSTR(_alertEnabled)]     = enableAlert;
    top[FPSTR(_preAlertEnabled)]  = enablePreAlert;
    top[FPSTR(_endEnabled)]       = enableEnd;
    top[FPSTR(_alertPreset)]      = alertPreset;
    top[FPSTR(_preAlertPreset)]   = preAlertPreset;
    top[FPSTR(_endPreset)]        = endPreset;
    top[FPSTR(_idleTimeoutSec)]   = idleTimeoutSec;
    top[FPSTR(_idlePreset)]       = idlePreset;
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

    cfg &= getJsonValue(top[FPSTR(_alertEnabled)],    enableAlert, enableAlert);
    cfg &= getJsonValue(top[FPSTR(_preAlertEnabled)], enablePreAlert, enablePreAlert);
    cfg &= getJsonValue(top[FPSTR(_endEnabled)],      enableEnd, enableEnd);
    cfg &= getJsonValue(top[FPSTR(_alertPreset)],     alertPreset, alertPreset);
    cfg &= getJsonValue(top[FPSTR(_preAlertPreset)],  preAlertPreset, preAlertPreset);
    cfg &= getJsonValue(top[FPSTR(_endPreset)],       endPreset, endPreset);
    cfg &= getJsonValue(top[FPSTR(_idleTimeoutSec)],  idleTimeoutSec, idleTimeoutSec);
    cfg &= getJsonValue(top[FPSTR(_idlePreset)],      idlePreset, idlePreset);

    return cfg;
  }

  // Info in /json/info and UI
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray arr = user.createNestedArray(FPSTR(_name));
    const __FlashStringHelper* stateLabel = F("OK");
    switch (currentState) {
      case STATE_PRE_ALERT: stateLabel = F("PRE_ALERT"); break;
      case STATE_ALERT:     stateLabel = F("ALERT");     break;
      case STATE_END:       stateLabel = F("END");       break;
      case STATE_OK:
      default:              stateLabel = F("OK");        break;
    }
    arr.add(stateLabel);
    arr.add(F(" Pikud Haoref"));

    // Add a small button to reset the API URL back to default
    String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
    uiDomString += FPSTR(_name);
    uiDomString += F(":{");
    uiDomString += FPSTR(_resetUrl);
    uiDomString += F(":true}});\">Reset URL</button>");
    arr.add(uiDomString);

    JsonObject sensor = root["sensor"];
    if (sensor.isNull()) sensor = root.createNestedObject("sensor");
    JsonArray status = sensor.createNestedArray(FPSTR(_name));
    status.add(lastHttpCode);
    status.add(F(" last HTTP status"));
  }

  // Handle JSON state updates (e.g. from the Reset URL button)
  void readFromJsonState(JsonObject &root) override {
    if (!initDone) return;

    JsonObject um = root[FPSTR(_name)];
    if (um.isNull()) return;

    if (um[FPSTR(_resetUrl)]) {
      apiUrl = F("https://www.oref.org.il/WarningMessages/alert/alerts.json");
    }
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
const char UsermodPikudHaoref::_alertEnabled[]     PROGMEM = "alertEnabled";
const char UsermodPikudHaoref::_preAlertEnabled[]  PROGMEM = "preAlertEnabled";
const char UsermodPikudHaoref::_endEnabled[]       PROGMEM = "endEnabled";
const char UsermodPikudHaoref::_alertPreset[]      PROGMEM = "alertPreset";
const char UsermodPikudHaoref::_preAlertPreset[]   PROGMEM = "preAlertPreset";
const char UsermodPikudHaoref::_endPreset[]        PROGMEM = "endPreset";
const char UsermodPikudHaoref::_idleTimeoutSec[]   PROGMEM = "idleTimeoutSec";
const char UsermodPikudHaoref::_idlePreset[]       PROGMEM = "idlePreset";
const char UsermodPikudHaoref::_resetUrl[]         PROGMEM = "resetUrl";

// register usermod instance
static UsermodPikudHaoref usermod_pikud_haoref;
REGISTER_USERMOD(usermod_pikud_haoref);