#pragma once

#include "interfaces.h"
#include "depart_model.h"
#include "util.h"
#include <vector>

// A simple view that renders one or more agency:stopCode boards onto a segment.
class DepartureView : public IDataViewT<DepartModel> {
private:
  uint16_t boardingSecs_ = 0;  // show past etd for this long (default)
  String keysStr_;             // Raw string (e.g., "AC:50958,AC:50959" or "AC:50958 50959")
  std::vector<String> keys_;   // Parsed keys (each "AGENCY:StopCode")
  std::vector<String> agencies_; // Precomputed agencies for each key (same indexing as keys_)
  int16_t segmentId_ = -1;
  std::string configKey_;

  // Reusable buffers to avoid per-frame heap churn
  struct Cand { uint32_t color; uint16_t bsum; const String* key; uint8_t alpha; };
  struct Src  { const DepartModel::Entry::Batch* batch; const String* agency; };
  std::vector<Cand> cands_;
  std::vector<Cand> strong_;
  std::vector<Src>  sources_;
public:
  explicit DepartureView(const String& keys) : keysStr_(keys), segmentId_(-1), configKey_() {
    parseKeysFrom(keysStr_);
    updateConfigKey_();
  }
  const String& viewKey() const { return keysStr_; }

  void view(std::time_t now, const DepartModel& model) override;
  void addToConfig(JsonObject& root) override {
    root["SegmentId"] = segmentId_;
    root["AgencyStopCodes"] = keysStr_;
    root["BoardingSecs"] = boardingSecs_;
  }
  void appendConfigData(Print& s) override { appendConfigData(s, nullptr); }
  void appendConfigData(Print& s, const DepartModel* model) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override {
    bool ok = true;
    ok &= getJsonValue(root["SegmentId"], segmentId_, segmentId_);
    {
      String prev = keysStr_;
      String tmp;
      bool got = getJsonValue(root["AgencyStopCodes"], tmp, (const char*)nullptr);
      if (got) keysStr_ = tmp; else {
        // Backward compatibility with single code fields
        tmp = String();
        bool gotOld = getJsonValue(root["AgencyStopCode"], tmp, (const char*)nullptr);
        if (gotOld) keysStr_ = tmp; else ok &= getJsonValue(root["Key"], keysStr_, keysStr_);
      }
      if (keysStr_ != prev) {
        // Re-parse and update config key
        parseKeysFrom(keysStr_);
        updateConfigKey_();
      }
    }
    ok &= getJsonValue(root["BoardingSecs"], boardingSecs_, (uint16_t)0);
    return ok;
  }
  const char* configKey() const override { return configKey_.c_str(); }
  std::string name() const override { return configKey_; }

private:
  void parseKeysFrom(const String& in) {
    keys_.clear();
    agencies_.clear();
    String agencyHint, token;
    auto flush = [&]() {
      token.trim();
      if (!token.length()) return;
      int c = token.indexOf(':');
      if (c > 0) {
        keys_.push_back(token);
        String ag = token.substring(0, c);
        agencies_.push_back(ag);
        if (!agencyHint.length()) agencyHint = ag;
      } else {
        if (agencyHint.length() > 0) {
          String k = agencyHint; k += ':'; k += token; keys_.push_back(k);
          agencies_.push_back(agencyHint);
        } else {
          keys_.push_back(token);
          agencies_.push_back(String());
        }
      }
      token = String();
    };
    for (unsigned i = 0; i < in.length(); ++i) {
      char ch = in.charAt(i);
      if (ch == ',' || ch == ' ' || ch == '\t' || ch == '\n' || ch == ';') flush();
      else token += ch;
    }
    flush();
  }

  void updateConfigKey_() {
    String s = String("DepartureView_") + keysStr_;
    s.replace(':', '_'); s.replace(',', '_'); s.replace(' ', '_'); s.replace(';', '_');
    s.replace('\'', '_'); s.replace('\"', '_'); s.replace('\\', '_');
    configKey_ = std::string(s.c_str());
  }
};
