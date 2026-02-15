#pragma once

#include <memory>
#include <vector>
#include "interfaces.h"
#include "depart_model.h"
#include "http_transport.h"
#include "wled.h"
#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

// SIRI StopMonitoring JSON source for DepartStrip.
// Configurable per-agency/stop; can be instantiated multiple times.
class SiriSource : public IDataSourceT<DepartModel> {
private:
  struct StopQuery {
    String canonical;        // normalized "Stop->Dest=Alias" representation
    String monitoringRef;    // value for MonitoringRef
    String destinationRef;   // optional DestinationRef
    String notViaRef;        // optional NotViaRef (without leading '!')
    String labelSuffix;      // optional suffix appended to formatted line label
  };

  bool     enabled_ = false;
  uint32_t updateSecs_ = 60;
  String   baseUrl_ = "";
  String   apiKey_   = "";
  String   agency_   = "";
  String   stopCode_ = "";
  std::vector<StopQuery> queries_;
  time_t   nextFetch_ = 0;
  uint8_t  backoffMult_ = 1;
  time_t   lastBackoffLog_ = 0;
  departstrip::net::HttpTransport httpTransport_;
  HTTPClient http_;
  std::string configKey_ = "siri_source";
  String   lastStopName_;
  bool httpActive_ = false;
  bool httpUsedSecure_ = false;
  size_t lastJsonCapacity_ = 12288;

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
  String sourceKey() const override { String k(agency_); k += ':'; k += stopCode_; return k; }
  const char* sourceType() const override { return "siri"; }

  const String& agency() const { return agency_; }

  void addToConfig(JsonObject& root) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override;
  const char* configKey() const override { return configKey_.c_str(); }

private:
  String composeUrl(const String& agency, const StopQuery& query) const;
  static bool parseRFC3339ToUTC(const char* s, time_t& outUtc);
  // Helpers to keep fetch() concise
  bool httpBegin(const String& url, int& outLen);
  bool parseJsonFromHttp(JsonDocument& doc,
                         bool chunked = false,
                         char* sniffBuf = nullptr,
                         size_t sniffCap = 0,
                         size_t* sniffLenOut = nullptr,
                         bool* sniffTruncatedOut = nullptr);
  size_t computeJsonCapacity(int contentLen);
  JsonObject getSiriRoot(JsonDocument& doc, bool& usedTopLevelFallback);
  bool appendItemsFromSiri(JsonObject siri,
                           const StopQuery& query,
                           std::time_t now,
                           DepartModel::Entry::Batch& batch,
                           String& firstStopName);
  static JsonDocument* acquireJsonDoc(size_t capacity, bool& fromPool);
  static void releaseJsonDoc(JsonDocument* doc, bool fromPool);
  void endHttp();
};
