#include "wled.h"
#include <HTTPClient.h>
#include "redalert_text_utils.h"

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
  static const char _okEnabled[];
  static const char _allAreasEnabled[];

  static const char _alertPreset[];
  static const char _preAlertPreset[];
  static const char _endPreset[];
  static const char _okPreset[];

  static const char _idleTimeoutSec[];
  static const char _idlePreset[];
  static const char _verboseLogs[];
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
  String normalizedAreaName = RedAlertText::normalizeAreaText(areaName);

  // Per-state toggles and presets
  bool    enableAlert     = true;
  bool    enablePreAlert  = false;
  bool    enableEnd       = false;
  bool    enableOk        = false;
  bool    enableAllAreas  = false;
  uint8_t alertPreset     = 0;
  uint8_t preAlertPreset  = 0;
  uint8_t endPreset       = 0;
  uint8_t okPreset        = 0;

  // Idle timeout handling (seconds + preset)
  uint32_t idleTimeoutSec = 0;        // 0 = disabled
  uint8_t  idlePreset     = 0;
  bool     idleEnabled    = false;
  bool     verboseLogs    = true;     // current behavior: verbose logging on

  // Internal state
  unsigned long lastPoll = 0;
  int lastHttpCode = 0;

  AlertState currentState = STATE_OK;
  unsigned long lastStateChangeMs = 0;
  bool idlePresetApplied = false;

  void refreshNormalizedAreaName() {
    normalizedAreaName = RedAlertText::normalizeAreaText(areaName);
  }

  const __FlashStringHelper* stateToLabel(AlertState s) {
    switch (s) {
      case STATE_PRE_ALERT: return F("PRE_ALERT");
      case STATE_ALERT:     return F("ALERT");
      case STATE_END:       return F("END");
      case STATE_OK:
      default:              return F("OK");
    }
  }

  int extractCategory(JsonObject& alert) {
    int category = 0;

    // Prefer "cat" (string or number)
    JsonVariant catVar = alert["cat"];
    if (!catVar.isNull()) {
      if (catVar.is<int>()) {
        category = catVar.as<int>();
      } else if (catVar.is<const char*>()) {
        const char* catStr = catVar.as<const char*>();
        if (catStr) category = atoi(catStr);
      }
    } else {
      // Fallback to "category"
      JsonVariant cat2 = alert["category"];
      if (!cat2.isNull()) {
        if (cat2.is<int>()) {
          category = cat2.as<int>();
        } else if (cat2.is<const char*>()) {
          const char* catStr2 = cat2.as<const char*>();
          if (catStr2) category = atoi(catStr2);
        }
      }
    }

    return category;
  }

  bool areaMatches(const char* cityNameRaw) {
    if (enableAllAreas) return true;
    if (!cityNameRaw) return false;

    String cityNorm = RedAlertText::normalizeAreaText(String(cityNameRaw));
    if (cityNorm.length() == 0 || normalizedAreaName.length() == 0) return false;

    // Strict direction: city text may contain configured area text.
    bool match = (cityNorm == normalizedAreaName || cityNorm.indexOf(normalizedAreaName) >= 0);

    if (verboseLogs) {
      DEBUG_PRINT(F("RedAlert: area compare city=\""));
      DEBUG_PRINT(cityNorm);
      DEBUG_PRINT(F("\" area=\""));
      DEBUG_PRINT(normalizedAreaName);
      DEBUG_PRINT(F("\" -> "));
      DEBUG_PRINTLN(match ? F("MATCH") : F("NO_MATCH"));
    }

    return match;
  }

  void updateState(AlertState newState) {
    if (newState == currentState) return;

    DEBUG_PRINT(F("RedAlert: state change "));
    DEBUG_PRINT(stateToLabel(currentState));
    DEBUG_PRINT(F(" -> "));
    DEBUG_PRINTLN(stateToLabel(newState));

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
        if (enableOk && okPreset > 0) {
          applyPreset(okPreset);
        }
        break;
    }
  }

  // Helper: poll the alert endpoint
  void pollAlert() {
    if (!enabled) return;
    if (!Network.isConnected()) return;
    if (apiUrl.length() == 0) return;
    // If we are NOT in "all areas" mode, require non-empty normalized area text.
    if (!enableAllAreas && normalizedAreaName.length() == 0) return;

    HTTPClient http;
    http.setTimeout(3000); // 3s

    http.begin(apiUrl);

    // Pikud Haoref endpoint is somewhat picky about headers; set a UA.
    http.addHeader("User-Agent", "WLED-PikudHaoref-ESP32");

    int httpCode = http.GET();
    lastHttpCode = httpCode;

    if (verboseLogs) {
      DEBUG_PRINT(F("RedAlert: HTTP GET "));
      DEBUG_PRINT(apiUrl);
      DEBUG_PRINT(F(" -> code "));
      DEBUG_PRINTLN(httpCode);
    }

    if (httpCode == HTTP_CODE_OK) {
      AlertState newState = STATE_OK;
      String payload = http.getString();

      if (verboseLogs) {
        DEBUG_PRINTLN(F("RedAlert: raw JSON payload:"));
        DEBUG_PRINTLN(payload);
      }

      // Response can be:
      //  - a single JSON object (current Pikud Haoref format)
      //  - or an array of such objects (for future/other wrappers)
      DynamicJsonDocument doc(4096);
      DeserializationError err = deserializeJson(doc, payload);
      if (err) {
        DEBUG_PRINT(F("RedAlert: JSON parse error: "));
        DEBUG_PRINTLN(err.c_str());
      } else {
        if (doc.is<JsonArray>()) {
          // Array of alert objects
          JsonArray alerts = doc.as<JsonArray>();
          for (JsonVariant v : alerts) {
            if (!v.is<JsonObject>()) continue;
            JsonObject alert = v.as<JsonObject>();

            // Newer format uses "data" array; older/wrapper formats may use "cities"
            JsonArray cities = alert["data"].as<JsonArray>();
            if (cities.isNull()) cities = alert["cities"].as<JsonArray>();
            if (cities.isNull()) continue;

            for (JsonVariant cv : cities) {
              const char* cityName = cv.as<const char*>();
              if (!cityName) continue;
              if (!areaMatches(cityName)) continue;

              int category = extractCategory(alert);

              if (verboseLogs) {
                DEBUG_PRINT(F("RedAlert: match (array), city=\""));
                DEBUG_PRINT(cityName);
                DEBUG_PRINT(F("\", category="));
                DEBUG_PRINTLN(category);
              }

              if (category == 14) {
                newState = STATE_PRE_ALERT;  // "pre_alert"
              } else if (category == 13) {
                newState = STATE_END;        // "end"
              } else {
                newState = STATE_ALERT;      // main "alert"
              }
              break;
            }
            if (newState != STATE_OK) break;
          }
        } else if (doc.is<JsonObject>()) {
          // Single alert object (current Pikud Haoref "alerts.json" shape)
          JsonObject alert = doc.as<JsonObject>();

          JsonArray cities = alert["data"].as<JsonArray>();
          if (cities.isNull()) cities = alert["cities"].as<JsonArray>();

          if (!cities.isNull()) {
            for (JsonVariant cv : cities) {
              const char* cityName = cv.as<const char*>();
              if (!cityName) continue;
              if (!areaMatches(cityName)) continue;

              int category = extractCategory(alert);

              if (verboseLogs) {
                DEBUG_PRINT(F("RedAlert: match (object), city=\""));
                DEBUG_PRINT(cityName);
                DEBUG_PRINT(F("\", category="));
                DEBUG_PRINTLN(category);
              }

              if (category == 14) {
                newState = STATE_PRE_ALERT;
              } else if (category == 13) {
                newState = STATE_END;
              } else {
                newState = STATE_ALERT;
              }
              break;
            }
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
    refreshNormalizedAreaName();
    initDone = true;

    if (verboseLogs) DEBUG_PRINTLN(F("RedAlert: setup complete"));
  }

  // Called frequently; do non-blocking work here
  void loop() override {
    if (!enabled) return;

    // First handle periodic polling
    unsigned long now = millis();
    if (now - lastPoll >= pollIntervalMs) {
      lastPoll = now;
      pollAlert();                 // may change state and update lastStateChangeMs
    }

    // Re-sample time *after* polling, so idle timing is relative to the
    // most recent state change, not the time before pollAlert() ran.
    now = millis();

    // Idle timeout handling:
    // If idle fallback is enabled and we have been in the *same* state
    // for configured time without any state changes, optionally apply
    // a fallback preset once.
    if (initDone && idleEnabled &&
        idleTimeoutSec > 0 && idlePreset > 0 &&
        lastStateChangeMs > 0 && !idlePresetApplied) {
      if (now - lastStateChangeMs >= idleTimeoutSec * 1000UL) {
        applyPreset(idlePreset);
        idlePresetApplied = true;

        if (verboseLogs) {
          DEBUG_PRINT(F("RedAlert: idle timeout reached, applying idlePreset="));
          DEBUG_PRINTLN(idlePreset);
        }
      }
    }

  }

  // Provide JSON config
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));

    // Core
    JsonObject core = top.createNestedObject("core");
    core["Enabled"]            = enabled;
    core["API URL"]            = apiUrl;
    core["Poll interval (ms)"] = pollIntervalMs;
    core["Verbose logs"]       = verboseLogs;

    // Area selection
    JsonObject area = top.createNestedObject("area");
    area["Match all areas"]    = enableAllAreas;
    area["Target area name"]   = areaName;

    // State actions (flat but grouped under "states" for a single divider)
    JsonObject states = top.createNestedObject("states");
    states["Alert enabled"]        = enableAlert;
    states["Alert preset"]         = alertPreset;
    states["Pre-alert enabled"]    = enablePreAlert;
    states["Pre-alert preset"]     = preAlertPreset;
    states["End enabled"]          = enableEnd;
    states["End preset"]           = endPreset;
    states["OK enabled"]           = enableOk;
    states["OK preset"]            = okPreset;

    // Idle fallback
    JsonObject idle = top.createNestedObject("idle");
    idle["Enable idle fallback"] = idleEnabled;
    idle["Idle timeout (sec)"]   = idleTimeoutSec;
    idle["Idle preset"]          = idlePreset;
  }

  // Load JSON config
  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINTLN(F("PikudHaoref: No config found, using defaults."));
      return false;
    }

    bool cfg = true;

    // Core (new grouped layout), with legacy flat-key fallback
    JsonObject core = top["core"];
    if (!core.isNull()) {
      // New pretty labels
      cfg &= getJsonValue(core["Enabled"],            enabled, enabled);
      cfg &= getJsonValue(core["API URL"],            apiUrl, apiUrl);
      cfg &= getJsonValue(core["Poll interval (ms)"], pollIntervalMs, pollIntervalMs);
      cfg &= getJsonValue(core["Verbose logs"],       verboseLogs, verboseLogs);
      // Backward compatibility for old keys in core object
      cfg &= getJsonValue(core[FPSTR(_enabled)],      enabled, enabled);
      cfg &= getJsonValue(core[FPSTR(_apiUrl)],       apiUrl, apiUrl);
      cfg &= getJsonValue(core["pollIntervalMs"],     pollIntervalMs, pollIntervalMs);
      cfg &= getJsonValue(core[FPSTR(_verboseLogs)],  verboseLogs, verboseLogs);
    } else {
      cfg &= getJsonValue(top[FPSTR(_enabled)],       enabled, enabled);
      cfg &= getJsonValue(top[FPSTR(_apiUrl)],        apiUrl, apiUrl);
      cfg &= getJsonValue(top["pollIntervalMs"],      pollIntervalMs, pollIntervalMs);
      cfg &= getJsonValue(top[FPSTR(_verboseLogs)],   verboseLogs, verboseLogs);
    }

    // Area selection (new grouped layout), with legacy flat-key fallback
    JsonObject area = top["area"];
    if (!area.isNull()) {
      cfg &= getJsonValue(area["Match all areas"],      enableAllAreas, enableAllAreas);
      cfg &= getJsonValue(area["Target area name"],     areaName, areaName);
      // Backward compatibility
      cfg &= getJsonValue(area[FPSTR(_allAreasEnabled)], enableAllAreas, enableAllAreas);
      cfg &= getJsonValue(area[FPSTR(_areaName)],        areaName, areaName);
    } else {
      cfg &= getJsonValue(top[FPSTR(_allAreasEnabled)],  enableAllAreas, enableAllAreas);
      cfg &= getJsonValue(top[FPSTR(_areaName)],         areaName, areaName);
    }

    // State actions (new grouped layout), with legacy flat-key fallback
    JsonObject states = top["states"];
    if (!states.isNull()) {
      // New flat-in-group layout with pretty labels
      if (!states["Alert enabled"].isNull() || !states["Alert preset"].isNull()) {
        cfg &= getJsonValue(states["Alert enabled"],        enableAlert, enableAlert);
        cfg &= getJsonValue(states["Alert preset"],         alertPreset, alertPreset);
        cfg &= getJsonValue(states["Pre-alert enabled"],    enablePreAlert, enablePreAlert);
        cfg &= getJsonValue(states["Pre-alert preset"],     preAlertPreset, preAlertPreset);
        cfg &= getJsonValue(states["End enabled"],          enableEnd, enableEnd);
        cfg &= getJsonValue(states["End preset"],           endPreset, endPreset);
        cfg &= getJsonValue(states["OK enabled"],           enableOk, enableOk);
        cfg &= getJsonValue(states["OK preset"],            okPreset, okPreset);
        // Backward compatibility for original flat keys in states
        cfg &= getJsonValue(states["alertEnabled"],         enableAlert, enableAlert);
        cfg &= getJsonValue(states["alertPreset"],          alertPreset, alertPreset);
        cfg &= getJsonValue(states["preAlertEnabled"],      enablePreAlert, enablePreAlert);
        cfg &= getJsonValue(states["preAlertPreset"],       preAlertPreset, preAlertPreset);
        cfg &= getJsonValue(states["endEnabled"],           enableEnd, enableEnd);
        cfg &= getJsonValue(states["endPreset"],            endPreset, endPreset);
        cfg &= getJsonValue(states["okEnabled"],            enableOk, enableOk);
        cfg &= getJsonValue(states["okPreset"],             okPreset, okPreset);
      } else {
        // Backward compatibility: older nested-in-group layout
        JsonObject sAlert = states["alert"];
        if (!sAlert.isNull()) {
          cfg &= getJsonValue(sAlert["enabled"], enableAlert, enableAlert);
          cfg &= getJsonValue(sAlert["preset"],  alertPreset, alertPreset);
        }

        JsonObject sPreAlert = states["preAlert"];
        if (!sPreAlert.isNull()) {
          cfg &= getJsonValue(sPreAlert["enabled"], enablePreAlert, enablePreAlert);
          cfg &= getJsonValue(sPreAlert["preset"],  preAlertPreset, preAlertPreset);
        }

        JsonObject sEnd = states["end"];
        if (!sEnd.isNull()) {
          cfg &= getJsonValue(sEnd["enabled"], enableEnd, enableEnd);
          cfg &= getJsonValue(sEnd["preset"],  endPreset, endPreset);
        }

        JsonObject sOk = states["ok"];
        if (!sOk.isNull()) {
          cfg &= getJsonValue(sOk["enabled"], enableOk, enableOk);
          cfg &= getJsonValue(sOk["preset"],  okPreset, okPreset);
        }
      }
    } else {
      cfg &= getJsonValue(top[FPSTR(_alertEnabled)],    enableAlert, enableAlert);
      cfg &= getJsonValue(top[FPSTR(_preAlertEnabled)], enablePreAlert, enablePreAlert);
      cfg &= getJsonValue(top[FPSTR(_endEnabled)],      enableEnd, enableEnd);
      cfg &= getJsonValue(top[FPSTR(_okEnabled)],       enableOk, enableOk);
      cfg &= getJsonValue(top[FPSTR(_alertPreset)],     alertPreset, alertPreset);
      cfg &= getJsonValue(top[FPSTR(_preAlertPreset)],  preAlertPreset, preAlertPreset);
      cfg &= getJsonValue(top[FPSTR(_endPreset)],       endPreset, endPreset);
      cfg &= getJsonValue(top[FPSTR(_okPreset)],        okPreset, okPreset);
    }

    // Idle fallback (new grouped layout), with legacy flat-key fallback
    JsonObject idle = top["idle"];
    if (!idle.isNull()) {
      cfg &= getJsonValue(idle["Enable idle fallback"], idleEnabled, idleEnabled);
      cfg &= getJsonValue(idle["Idle timeout (sec)"],   idleTimeoutSec, idleTimeoutSec);
      cfg &= getJsonValue(idle["Idle preset"],          idlePreset, idlePreset);
      // Backward compatibility
      cfg &= getJsonValue(idle["timeoutSec"],           idleTimeoutSec, idleTimeoutSec);
      cfg &= getJsonValue(idle["preset"],               idlePreset, idlePreset);
    } else {
      cfg &= getJsonValue(top[FPSTR(_idleTimeoutSec)],  idleTimeoutSec, idleTimeoutSec);
      cfg &= getJsonValue(top[FPSTR(_idlePreset)],      idlePreset, idlePreset);
    }

    refreshNormalizedAreaName();

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
const char UsermodPikudHaoref::_okEnabled[]        PROGMEM = "okEnabled";
const char UsermodPikudHaoref::_allAreasEnabled[]  PROGMEM = "allAreasEnabled";
const char UsermodPikudHaoref::_alertPreset[]      PROGMEM = "alertPreset";
const char UsermodPikudHaoref::_preAlertPreset[]   PROGMEM = "preAlertPreset";
const char UsermodPikudHaoref::_endPreset[]        PROGMEM = "endPreset";
const char UsermodPikudHaoref::_okPreset[]         PROGMEM = "okPreset";
const char UsermodPikudHaoref::_idleTimeoutSec[]   PROGMEM = "idleTimeoutSec";
const char UsermodPikudHaoref::_idlePreset[]       PROGMEM = "idlePreset";
const char UsermodPikudHaoref::_verboseLogs[]      PROGMEM = "verboseLogs";
const char UsermodPikudHaoref::_resetUrl[]         PROGMEM = "resetUrl";

// register usermod instance
static UsermodPikudHaoref usermod_pikud_haoref;
REGISTER_USERMOD(usermod_pikud_haoref);