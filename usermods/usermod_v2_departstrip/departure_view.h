#pragma once

#include "interfaces.h"
#include "depart_model.h"
#include "util.h"
#include <vector>

// A simple view that renders one or more agency:stopCode boards onto a segment.
class DepartureView : public IDataViewT<DepartModel> {
private:
  uint16_t boardingSecs_ = 60; // show past etd for this long
  String keysStr_;             // Raw string (e.g., "AC:50958,AC:50959" or "AC:50958 50959")
  std::vector<String> keys_;   // Parsed keys (each "AGENCY:StopCode")
  int16_t segmentId_ = -1;
  std::string configKey_;
public:
  explicit DepartureView(const String& keys) : keysStr_(keys), segmentId_(-1), configKey_() {
    // Parse now so view() has a ready list
    auto parseKeys = [&](const String& in) {
      keys_.clear();
      // Split by comma or whitespace
      String agencyHint;
      String token;
      auto flush = [&]() {
        token.trim();
        if (!token.length()) return;
        int c = token.indexOf(':');
        if (c > 0) {
          keys_.push_back(token);
          if (agencyHint.length() == 0) agencyHint = token.substring(0, c);
        } else {
          if (agencyHint.length() > 0) {
            String k = agencyHint; k += ':'; k += token; keys_.push_back(k);
          } else {
            // As a fallback, keep token as-is (may be full key already)
            keys_.push_back(token);
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
    };
    parseKeys(keysStr_);
    String s = String("DepartureView_") + keysStr_;
    s.replace(':', '_'); s.replace(',', '_'); s.replace(' ', '_'); s.replace(';', '_');
    configKey_ = std::string(s.c_str());
  }
  const String& viewKey() const { return keysStr_; }

  void view(std::time_t now, const DepartModel& model) override;
  void addToConfig(JsonObject& root) override {
    root["SegmentId"] = segmentId_;
    root["AgencyStopCodes"] = keysStr_;
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
        // Parse keys
        keys_.clear();
        {
          String in = keysStr_;
          String agencyHint;
          String token;
          auto flush = [&]() {
            token.trim();
            if (!token.length()) return;
            int c = token.indexOf(':');
            if (c > 0) {
              keys_.push_back(token);
              if (agencyHint.length() == 0) agencyHint = token.substring(0, c);
            } else {
              if (agencyHint.length() > 0) { String k = agencyHint; k += ':'; k += token; keys_.push_back(k); }
              else keys_.push_back(token);
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
        String s = String("DepartureView_") + keysStr_;
        s.replace(':', '_'); s.replace(',', '_'); s.replace(' ', '_'); s.replace(';', '_');
        configKey_ = std::string(s.c_str());
      }
    }
    return ok;
  }
  const char* configKey() const override { return configKey_.c_str(); }
  std::string name() const override { return configKey_; }
};
