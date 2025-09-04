#pragma once

#include <memory>
#include <vector>
#include "interfaces.h"
#include "depart_model.h"
#include "wled.h"
#include <WiFiClient.h>
#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

// SIRI StopMonitoring JSON source for DepartStrip.
// Configurable per-agency/stop; can be instantiated multiple times.
class SiriSource : public IDataSourceT<DepartModel> {
private:
  bool     enabled_ = false;
  uint16_t updateSecs_ = 60;
  String   baseUrl_ = "";
  String   apiKey_   = "";
  String   agency_   = "";
  String   stopCode_ = "";
  time_t   nextFetch_ = 0;
  uint8_t  backoffMult_ = 1;
  time_t   lastBackoffLog_ = 0;
  WiFiClient client_;
  HTTPClient http_;
  std::string configKey_ = "siri_source";
  String   lastStopName_;

public:
  explicit SiriSource(const char* key = "siri_source",
                       const char* defAgency = nullptr,
                       const char* defStopCode = nullptr,
                       const char* defBaseUrl = nullptr);
  std::unique_ptr<DepartModel> fetch(std::time_t now) override;
  void reload(std::time_t now) override;
  std::string name() const override { return configKey_; }
  void appendConfigData(Print& s) override {}
  const String& stopName() const { return lastStopName_; }
  String sourceKey() const { String k(agency_); k += ':'; k += stopCode_; return k; }

  const String& agency() const { return agency_; }

  void addToConfig(JsonObject& root) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override;
  const char* configKey() const override { return configKey_.c_str(); }

private:
  String composeUrl(const String& agency, const String& stopCode) const;
  static bool parseRFC3339ToUTC(const char* s, time_t& outUtc);
  // Helpers to keep fetch() concise
  bool httpBegin(const String& url, int& outLen);
  bool parseJsonFromHttp(DynamicJsonDocument& doc, bool withFilter = true, bool wideFilter = false);
  static size_t computeJsonCapacity(int contentLen);
  JsonObject getSiriRoot(DynamicJsonDocument& doc, bool& usedTopLevelFallback);
  bool buildModelFromSiri(JsonObject siri, std::time_t now, std::unique_ptr<DepartModel>& outModel);
};
