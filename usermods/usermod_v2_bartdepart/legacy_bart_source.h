#pragma once

#include <memory>
#include <vector>
#include "interfaces.h"
#include "bart_station_model.h"
#include "wled.h"
#include "WiFiClientSecure.h"
#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

class LegacyBartSource : public IDataSourceT<BartStationModel> {
private:
  uint16_t updateSecs_ = 60;
  String apiBase_ = "https://api.bart.gov/api/etd.aspx?cmd=etd&json=y";
  String apiKey_ = "MW9S-E7SL-26DU-VV8V";
  String apiStation_ = "19th";
  time_t nextFetch_ = 0;
  uint8_t backoffMult_ = 1;
  WiFiClientSecure client_;
  HTTPClient https_;

public:
  LegacyBartSource();
  std::unique_ptr<BartStationModel> fetch(std::time_t now) override;
  std::unique_ptr<BartStationModel> checkhistory(std::time_t now, std::time_t oldestTstamp) override { return nullptr; }
  void reload(std::time_t now) override;
  std::string name() const override { return "LegacyBartSource"; }

  void addToConfig(JsonObject& root) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override;
  const char* configKey() const override { return "LegacyBartSource"; }

  uint16_t updateSecs() const { return updateSecs_; }
  std::vector<String> platformIds() const { return { "1", "2", "3", "4" }; }
};
