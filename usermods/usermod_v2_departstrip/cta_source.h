#pragma once

#include <memory>
#include <vector>

#include "interfaces.h"
#include "depart_model.h"
#include "http_transport.h"
#include "util.h"
#include "wled.h"

#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

// Chicago Transit Authority (CTA) TrainTracker XML source.
// Fetches predicted arrivals and maps them onto DepartStrip boards.
class CtaTrainTrackerSource : public IDataSourceT<DepartModel> {
private:
  struct StopQuery {
    String canonical;    // normalized "mapId[.direction][=alias]" token
    String mapIdStr;     // mapId as string (no direction or alias)
    uint32_t mapId = 0;
    uint8_t direction = 0; // 0 = any direction as returned by API
    String labelSuffix;  // optional suffix appended to destNm
  };

  bool     enabled_ = false;
  uint32_t updateSecs_ = 60;
  String   baseUrl_;
  String   apiKey_;
  String   agency_ = F("CTA");
  String   stopKey_;
  std::vector<StopQuery> queries_;
  time_t   nextFetch_ = 0;
  uint8_t  backoffMult_ = 1;
  time_t   lastBackoffLog_ = 0;
  std::string configKey_ = "cta_source";
  departstrip::net::HttpTransport httpTransport_;
  String   lastStopName_;

public:
  explicit CtaTrainTrackerSource(const char* key = "cta_source");
  std::unique_ptr<DepartModel> fetch(std::time_t now) override;
  void reload(std::time_t now) override;
  std::string name() const override { return configKey_; }
  void appendConfigData(Print& s) override {}
  const char* configKey() const override { return configKey_.c_str(); }
  String sourceKey() const override;
  const char* sourceType() const override { return "cta"; }
  const String& agency() const { return agency_; }
  const String& stopName() const { return lastStopName_; }

  void addToConfig(JsonObject& root) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override;

private:
  struct ParsedEta {
    time_t arrival = 0;
    String label;
  };

  String composeUrl(const StopQuery& query) const;
  bool httpBegin(const String& url, int& outLen, int& outStatus, HTTPClient& http, bool& usedSecure);
  void closeHttp(HTTPClient& http, bool usedSecure);
  bool readHttpBody(HTTPClient& http, int lenHint, String& outBody);
  bool parseResponse(const String& xml,
                     const StopQuery& query,
                     std::time_t now,
                     DepartModel::Entry::Batch& batch);
  bool parseTimestamp(const String& value, time_t& outUtc) const;
  static void splitStopCodes(const String& input, std::vector<String>& out);
  static bool parseStopToken(const String& token, StopQuery& out, String& explicitAgency);
  static bool parseUnsigned(const String& s, uint32_t& out);
  static bool parseUnsigned8(const String& s, uint8_t& out);
  static String extractTagValue(const String& block, const __FlashStringHelper* tag);
  static String extractTagValue(const String& block, const char* tag);
};
