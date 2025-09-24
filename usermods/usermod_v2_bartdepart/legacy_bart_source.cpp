#include "legacy_bart_source.h"
#include "util.h"

LegacyBartSource::LegacyBartSource() {
  client_.setInsecure();
}

void LegacyBartSource::reload(std::time_t now) {
  nextFetch_ = now;
  backoffMult_ = 1;
}

static String composeUrl(const String& base, const String& key, const String& station) {
  String url = base;
  url += "&key="; url += key;
  url += "&orig="; url += station;
  return url;
}

std::unique_ptr<BartStationModel> LegacyBartSource::fetch(std::time_t now) {
  if (now == 0 || now < nextFetch_) return nullptr;

  String url = composeUrl(apiBase_, apiKey_, apiStation_);
  if (!https_.begin(client_, url)) {
    https_.end();
    DEBUG_PRINTLN(F("BartDepart: LegacyBartSource::fetch: trouble initiating request"));
    nextFetch_ = now + updateSecs_ * backoffMult_;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }
  DEBUG_PRINTF("BartDepart: LegacyBartSource::fetch: free heap before GET: %u\n",
               ESP.getFreeHeap());
  int httpCode = https_.GET();
  if (httpCode < 200 || httpCode >= 300) {
    https_.end();
    DEBUG_PRINTF("BartDepart: LegacyBartSource::fetch: HTTP status not OK: %d\n", httpCode);
    nextFetch_ = now + updateSecs_ * backoffMult_;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }
  String payload = https_.getString();
  https_.end();

  size_t jsonSz = payload.length() * 2;
  DynamicJsonDocument doc(jsonSz);
  auto err = deserializeJson(doc, payload);
  if (err) {
    nextFetch_ = now + updateSecs_ * backoffMult_;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }

  JsonObject root = doc["root"].as<JsonObject>();
  if (root.isNull()) {
    nextFetch_ = now + updateSecs_ * backoffMult_;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }

  std::unique_ptr<BartStationModel> model(new BartStationModel());
  for (const String& pid : platformIds()) {
    if (pid.isEmpty()) continue;
    BartStationModel::Platform tp(pid);
    tp.update(root);
    model->platforms.push_back(std::move(tp));
  }

  nextFetch_ = now + updateSecs_;
  backoffMult_ = 1;
  return model;
}

void LegacyBartSource::addToConfig(JsonObject& root) {
  root["UpdateSecs"] = updateSecs_;
  root["ApiBase"] = apiBase_;
  root["ApiKey"] = apiKey_;
  root["ApiStation"] = apiStation_;
}

bool LegacyBartSource::readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) {
  bool ok = true;
  uint16_t prevUpdate = updateSecs_;
  String   prevBase   = apiBase_;
  String   prevKey    = apiKey_;
  String   prevStation= apiStation_;

  ok &= getJsonValue(root["UpdateSecs"], updateSecs_, 60);
  ok &= getJsonValue(root["ApiBase"],    apiBase_,    apiBase_);
  ok &= getJsonValue(root["ApiKey"],     apiKey_,     apiKey_);
  ok &= getJsonValue(root["ApiStation"], apiStation_, apiStation_);

  // Only invalidate when source identity changes (base/station)
  invalidate_history |= (apiBase_ != prevBase) || (apiStation_ != prevStation);
  return ok;
}
