#pragma once
#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>

enum class EapMethod : uint8_t { PEAP = 0, TTLS = 1 };
enum class Phase2   : uint8_t { MSCHAPV2 = 0, PAP = 1 };

struct EapConfig {
  bool   enabled       = false;      // enterprise mode on/off
  String ssid;                       // target SSID (e.g. "iitk-sec")
  String identity;                   // outer/anonymous identity (often blank or same as username)
  String username;                   // inner identity
  String password;                   // SSO Wi-Fi password
  EapMethod method     = EapMethod::PEAP;
  Phase2   phase2      = Phase2::MSCHAPV2; // used when TTLS
  bool     validateCa  = false;      // validate RADIUS server?
  bool     caPresent   = false;      // CA file exists on FS
};

// load/save config from LittleFS (/wled_eap.json)
// returns true if FS was accessible; fields are filled with defaults if not present
bool eapLoad(EapConfig& out);
bool eapSave(const EapConfig& in);

// perform WPA2-Enterprise connect (PEAP or TTLS)
void eapConnect(const EapConfig& c);

// register HTTP routes (/eap/get, /eap/save, /eap/upload_ca, /eap/delete_ca)
void eapInitHttpRoutes();

#endif
