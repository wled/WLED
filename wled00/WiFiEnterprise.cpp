#ifdef ARDUINO_ARCH_ESP32

#include "WiFiEnterprise.h"
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_wpa2.h>         // WPA2-Enterprise
#include <ESPAsyncWebServer.h>
#include <vector>

extern AsyncWebServer server; // NOTE: non-pointer, matches your WLED

// ---- storage ----
static const char* kCfgPath = "/wled_eap.json";
static const char* kCaPath  = "/wled_eap_ca.pem";

static bool fsBegin() {
  static bool started = false;
  if (!started) started = LittleFS.begin(true);
  return started;
}

bool eapLoad(EapConfig& out) {
  out = EapConfig(); // defaults
  if (!fsBegin()) return false;

  if (LittleFS.exists(kCfgPath)) {
    File f = LittleFS.open(kCfgPath, "r");
    if (f) {
      DynamicJsonDocument d(1024);
      if (deserializeJson(d, f) == DeserializationError::Ok) {
        out.enabled     = d["enabled"]    | false;
        out.ssid        = d["ssid"]       | "";
        out.identity    = d["identity"]   | "";
        out.username    = d["username"]   | "";
        out.password    = d["password"]   | "";
        out.validateCa  = d["validateCa"] | false;
        out.method      = (d["method"] == String("TTLS")) ? EapMethod::TTLS : EapMethod::PEAP;
        out.phase2      = (d["phase2"] == String("PAP"))  ? Phase2::PAP     : Phase2::MSCHAPV2;
      }
      f.close();
    }
  }
  out.caPresent = LittleFS.exists(kCaPath);
  return true;
}

bool eapSave(const EapConfig& in) {
  if (!fsBegin()) return false;
  DynamicJsonDocument d(1024);
  d["enabled"]    = in.enabled;
  d["ssid"]       = in.ssid;
  d["identity"]   = in.identity;
  d["username"]   = in.username;
  d["password"]   = in.password;   // NOTE: you can omit this in /eap/get for privacy
  d["validateCa"] = in.validateCa;
  d["method"]     = (in.method == EapMethod::TTLS) ? "TTLS" : "PEAP";
  d["phase2"]     = (in.phase2 == Phase2::PAP)     ? "PAP"  : "MSCHAPV2";

  File f = LittleFS.open(kCfgPath, "w");
  if (!f) return false;
  bool ok = (serializeJson(d, f) > 0);
  f.close();
  return ok;
}

// ---- connection ----
static std::vector<uint8_t> g_caMem; // keep CA bytes alive during connect

void eapConnect(const EapConfig& c) {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);

  // Some cores expose TTLS phase2 setter; if not, supplicant will choose based on creds.
  #ifdef esp_wifi_sta_wpa2_ent_set_ttls_phase2_method
  if (c.method == EapMethod::TTLS) {
    int m = (c.phase2 == Phase2::PAP) ? 0 /*PAP*/ : 3 /*MSCHAPv2*/;
    esp_wifi_sta_wpa2_ent_set_ttls_phase2_method(m);
  }
  #endif

  // outer identity (anonymous or same as username), inner user/password
  esp_wifi_sta_wpa2_ent_set_identity((const uint8_t*)c.identity.c_str(), c.identity.length());
  esp_wifi_sta_wpa2_ent_set_username((const uint8_t*)c.username.c_str(), c.username.length());
  esp_wifi_sta_wpa2_ent_set_password((const uint8_t*)c.password.c_str(), c.password.length());

  // Optional: CA validation
  if (c.validateCa && LittleFS.exists(kCaPath)) {
    File ca = LittleFS.open(kCaPath, "r");
    if (ca) {
      g_caMem.resize(ca.size());
      ca.read(g_caMem.data(), g_caMem.size());
      ca.close();
      if (!g_caMem.empty())
        esp_wifi_sta_wpa2_ent_set_ca_cert(g_caMem.data(), g_caMem.size());
    }
  }

  esp_wpa2_config_t cfg = WPA2_CONFIG_INIT_DEFAULT();
  esp_wifi_sta_wpa2_ent_enable(&cfg);

  // Start association (no PSK for Enterprise)
  WiFi.begin(c.ssid.c_str());
}

// ---- HTTP UI/API ----
static void sendJson(AsyncWebServerRequest* req, const String& s) {
  req->send(200, "application/json", s);
}

void eapInitHttpRoutes() {
  // GET current settings
  server.on("/eap/get", HTTP_GET, [](AsyncWebServerRequest* req){
    EapConfig c; eapLoad(c);
    DynamicJsonDocument d(1024);
    d["enabled"]    = c.enabled;
    d["ssid"]       = c.ssid;
    d["identity"]   = c.identity;
    d["username"]   = c.username;
    d["password"]   = c.password;      // consider omitting for privacy
    d["validateCa"] = c.validateCa;
    d["method"]     = (c.method == EapMethod::TTLS) ? "TTLS" : "PEAP";
    d["phase2"]     = (c.phase2 == Phase2::PAP) ? "PAP" : "MSCHAPV2";
    d["caPresent"]  = c.caPresent;
    String out; serializeJson(d, out);
    sendJson(req, out);
  });

  // POST save JSON body
  server.on("/eap/save", HTTP_POST,
    [](AsyncWebServerRequest* req){ /* answered by body handler */ },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      DynamicJsonDocument d(1024);
      if (deserializeJson(d, data, len) != DeserializationError::Ok) {
        req->send(400, "text/plain", "bad json"); return;
      }
      EapConfig c;
      c.enabled    = d["enabled"]    | false;
      c.ssid       = d["ssid"]       | "";
      c.identity   = d["identity"]   | "";
      c.username   = d["username"]   | "";
      c.password   = d["password"]   | "";
      c.validateCa = d["validateCa"] | false;
      c.method     = (d["method"] == String("TTLS")) ? EapMethod::TTLS : EapMethod::PEAP;
      c.phase2     = (d["phase2"] == String("PAP"))  ? Phase2::PAP     : Phase2::MSCHAPV2;

      if (!eapSave(c)) { req->send(500, "text/plain", "save failed"); return; }
      req->send(200, "text/plain", "OK");
    }
  );

  // POST upload CA (multipart/form-data, field name="file")
  server.on("/eap/upload_ca", HTTP_POST,
    [](AsyncWebServerRequest* req){ req->send(200, "text/plain", "OK"); },
    [](AsyncWebServerRequest* req, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!fsBegin()) return;
      static File f;
      if (index == 0) { if (LittleFS.exists(kCaPath)) LittleFS.remove(kCaPath); f = LittleFS.open(kCaPath, "w"); }
      if (f) f.write(data, len);
      if (final && f) f.close();
    }
  );

  // POST delete CA
  server.on("/eap/delete_ca", HTTP_POST, [](AsyncWebServerRequest* req){
    if (fsBegin() && LittleFS.exists(kCaPath)) LittleFS.remove(kCaPath);
    req->send(200, "text/plain", "OK");
  });
}

#endif
