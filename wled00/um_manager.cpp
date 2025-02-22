#include "wled.h"
/*
 * Registration and management utility for v2 usermods
 */

static std::vector<Usermod*> ums;
static std::vector<Usermod*> umsEnabled;

//Usermod Manager internals
void UsermodManager::setup()             { for (Usermod* um : umsEnabled) um->setup(); }
void UsermodManager::connected()         { for (Usermod* um : umsEnabled) um->connected(); }
void UsermodManager::loop()              { for (Usermod* um : umsEnabled) um->loop();  }
void UsermodManager::handleOverlayDraw() { for (Usermod* um : umsEnabled) um->handleOverlayDraw(); }
void UsermodManager::appendConfigData(Print& dest)  { for (Usermod* um : ums) um->appendConfigData(dest); }
bool UsermodManager::handleButton(uint8_t b) {
  bool overrideIO = false;
  for (Usermod* um : umsEnabled) {
    if (um->handleButton(b)) overrideIO = true;
  }
  return overrideIO;
}
bool UsermodManager::getUMData(um_data_t **data, uint8_t mod_id) {
  for (Usermod* um : umsEnabled) {
    if (mod_id > 0 && um->getId() != mod_id) continue;  // only get data form requested usermod if provided
    if (um->getUMData(data)) return true;               // if usermod does provide data return immediately (only one usermod can provide data at one time)
  }
  return false;
}
void UsermodManager::addToJsonState(JsonObject& obj)    { for (Usermod* um : umsEnabled) um->addToJsonState(obj); }
void UsermodManager::addToJsonInfo(JsonObject& obj)     { for (Usermod* um : ums) um->addToJsonInfo(obj); }
void UsermodManager::readFromJsonState(JsonObject& obj) { for (Usermod* um : umsEnabled) um->readFromJsonState(obj); }
void UsermodManager::addToConfig(JsonObject& obj)       { for (Usermod* um : ums) um->addToConfig(obj); }
bool UsermodManager::readFromConfig(JsonObject& obj)    {
  bool allComplete = true;
  umsEnabled.clear();
  for (Usermod* um : ums) {
    if (!um->readFromConfig(obj)) allComplete = false;
    if(!um->isEnabled()) {
      umsEnabled.push_back(um);
    }
  }
  return allComplete;
}
#ifndef WLED_DISABLE_MQTT
void UsermodManager::onMqttConnect(bool sessionPresent) { for (Usermod* um : umsEnabled) um->onMqttConnect(sessionPresent); }
bool UsermodManager::onMqttMessage(char* topic, char* payload) {
  for (Usermod* um : umsEnabled) if (um->onMqttMessage(topic, payload)) return true;
  return false;
}
#endif
#ifndef WLED_DISABLE_ESPNOW
bool UsermodManager::onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) {
  for (Usermod* um : umsEnabled) if (um->onEspNowMessage(sender, payload, len)) return true;
  return false;
}
#endif
void UsermodManager::onUpdateBegin(bool init) { for (Usermod* um : umsEnabled) um->onUpdateBegin(init); } // notify usermods that update is to begin
void UsermodManager::onStateChange(uint8_t mode) { for (Usermod* um : umsEnabled) um->onStateChange(mode); } // notify usermods that WLED state changed

/*
 * Enables usermods to lookup another Usermod.
 */
Usermod* UsermodManager::lookup(uint16_t mod_id) {
  for (Usermod* um : ums) {
    if (um->getId() == mod_id) {
      return um;
    }
  }
  return nullptr;
}

bool UsermodManager::add(Usermod* um)
{
  if (ums.size() >= WLED_MAX_USERMODS || um == nullptr) return false;
  ums.push_back(um);
  return true;
}

byte UsermodManager::getModCount() { return ums.size(); }

/* Usermod v2 interface shim for oappend */
Print* Usermod::oappend_shim = nullptr;

void Usermod::appendConfigData(Print& settingsScript) {
  assert(!oappend_shim);
  oappend_shim = &settingsScript;
  this->appendConfigData();
  oappend_shim = nullptr;
}
