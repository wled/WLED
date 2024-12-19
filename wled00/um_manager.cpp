#include "wled.h"
/*
 * Registration and management utility for v2 usermods
 */

static std::vector<Usermod*> usermods;

//Usermod Manager internals
void UsermodManager::setup()                            { for (const auto &ums : usermods) ums->setup(); }
void UsermodManager::connected()                        { for (const auto &ums : usermods) ums->connected(); }
void UsermodManager::loop()                             { for (const auto &ums : usermods) ums->loop();  }
void UsermodManager::handleOverlayDraw()                { for (const auto &ums : usermods) ums->handleOverlayDraw(); }
void UsermodManager::appendConfigData(Print& dest)      { for (const auto &ums : usermods) ums->appendConfigData(dest); }
void UsermodManager::addToJsonState(JsonObject& obj)    { for (const auto &ums : usermods) ums->addToJsonState(obj); }
void UsermodManager::addToJsonInfo(JsonObject& obj)     { for (const auto &ums : usermods) ums->addToJsonInfo(obj); }
void UsermodManager::readFromJsonState(JsonObject& obj) { for (const auto &ums : usermods) ums->readFromJsonState(obj); }
void UsermodManager::addToConfig(JsonObject& obj)       { for (const auto &ums : usermods) ums->addToConfig(obj); }
void UsermodManager::onUpdateBegin(bool init)           { for (const auto &ums : usermods) ums->onUpdateBegin(init); } // notify usermods that update is to begin
void UsermodManager::onStateChange(uint8_t mode)        { for (const auto &ums : usermods) ums->onStateChange(mode); } // notify usermods that WLED state changed
bool UsermodManager::handleButton(uint8_t b) {
  bool overrideIO = false;
  for (const auto &ums : usermods) {
    if (ums->handleButton(b)) overrideIO = true;
  }
  return overrideIO;
}
bool UsermodManager::getUMData(um_data_t **data, uint8_t mod_id) {
  for (const auto &ums : usermods) {
    if (mod_id > 0 && ums->getId() != mod_id) continue;  // only get data form requested usermod if provided
    if (ums->getUMData(data)) return true;               // if usermod does provide data return immediately (only one usermod can provide data at one time)
  }
  return false;
}
bool UsermodManager::readFromConfig(JsonObject& obj)    {
  bool allComplete = true;
  for (const auto &ums : usermods) {
    if (!ums->readFromConfig(obj)) allComplete = false;
  }
  return allComplete;
}
#ifndef WLED_DISABLE_MQTT
void UsermodManager::onMqttConnect(bool sessionPresent) { for (const auto &ums : usermods) ums->onMqttConnect(sessionPresent); }
bool UsermodManager::onMqttMessage(char* topic, char* payload) {
  for (const auto &ums : usermods) if (ums->onMqttMessage(topic, payload)) return true;
  return false;
}
#endif
#ifndef WLED_DISABLE_ESPNOW
bool UsermodManager::onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) {
  for (const auto &ums : usermods) if (ums->onEspNowMessage(sender, payload, len)) return true;
  return false;
}
#endif

/*
 * Enables usermods to lookup another Usermod.
 */
Usermod* UsermodManager::lookup(uint16_t mod_id) {
  for (const auto &ums : usermods) if (ums->getId() == mod_id) return ums;
  return nullptr;
}

bool UsermodManager::add(Usermod* um) {
  if (um == nullptr) return false;
  usermods.push_back(um);
  return true;
}

byte UsermodManager::getModCount() {
  return usermods.size();
}


/* Usermod v2 interface shim for oappend */
Print* Usermod::oappend_shim = nullptr;

void Usermod::appendConfigData(Print& settingsScript) {
  assert(!oappend_shim);
  oappend_shim = &settingsScript;
  this->appendConfigData();
  oappend_shim = nullptr;
}
