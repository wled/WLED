#pragma once

#include "interfaces.h"
#include "depart_model.h"
#include "util.h"
#include "http_transport.h"
#include "wled.h"
#include <vector>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266HTTPClient.h>
#else
#include <HTTPClient.h>
#endif

// Placeholder GTFS-RT source that currently only handles configuration.
class GtfsRtSource : public IDataSourceT<DepartModel> {
private:
  bool     enabled_ = false;
  uint32_t updateSecs_ = 60;
  String   baseUrl_;
  String   apiKey_;
  String   agency_;
  std::vector<String> stopCodes_;
  time_t   nextFetch_ = 0;
  uint8_t  backoffMult_ = 1;
  time_t   lastBackoffLog_ = 0;
  std::string configKey_ = "gtfsrt_source";
  departstrip::net::HttpTransport httpTransport_;

public:
  explicit GtfsRtSource(const char* key = "gtfsrt_source");
  std::unique_ptr<DepartModel> fetch(std::time_t now) override;
  void reload(std::time_t now) override;
  std::string name() const override { return configKey_; }
  void appendConfigData(Print& s) override {}
  String sourceKey() const override;
  const char* sourceType() const override { return "gtfsrt"; }

  void addToConfig(JsonObject& root) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override;
  const char* configKey() const override { return configKey_.c_str(); }

  const String& agency() const { return agency_; }
  const std::vector<String>& stopCodes() const { return stopCodes_; }

private:
  String composeUrl(const String& agency, const String& stopCode) const;
  bool httpBegin(const String& url, int& outLen, int& outStatus, HTTPClient& http, bool& usedSecure);
  void closeHttpClient(HTTPClient& http, bool usedSecure);
};
