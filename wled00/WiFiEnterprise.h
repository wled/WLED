#pragma once
#include <Arduino.h>
#include <WString.h>

enum class EapMethod {
  PEAP,
  TTLS
};

enum class Phase2 {
  MSCHAPV2,
  PAP
};

struct EapConfig {
  bool enabled    = false;
  String ssid;        // Enterprise SSID
  String identity;    // Outer identity (often anonymous)
  String username;    // Inner identity/username
  String password;    // Password
  bool validateCa = false;
  bool caPresent  = false;
  EapMethod method = EapMethod::PEAP;
  Phase2   phase2 = Phase2::MSCHAPV2;
};

// ---- Global config instance ----
// (declared here, defined in WiFiEnterprise.cpp)
extern EapConfig globalEapConfig;

// ---- API ----
bool eapLoad(EapConfig& out);
bool eapSave(const EapConfig& in);
void eapConnect(const EapConfig& cfg);
void eapInitHttpRoutes();
