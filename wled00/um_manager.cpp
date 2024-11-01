#include "wled.h"
/*
 * Registration and management utility for v2 usermods
 */

//Usermod Manager internals
void UsermodManager::setup()             { for (unsigned i = 0; i < numMods; i++) ums[i]->setup(); }
void UsermodManager::connected()         { for (unsigned i = 0; i < numMods; i++) ums[i]->connected(); }
void UsermodManager::loop()              { for (unsigned i = 0; i < numMods; i++) ums[i]->loop();  }
void UsermodManager::handleOverlayDraw() { for (unsigned i = 0; i < numMods; i++) ums[i]->handleOverlayDraw(); }
void UsermodManager::appendConfigData(Print& dest)  { for (unsigned i = 0; i < numMods; i++) ums[i]->appendConfigData(dest); }
bool UsermodManager::handleButton(uint8_t b) {
  bool overrideIO = false;
  for (unsigned i = 0; i < numMods; i++) {
    if (ums[i]->handleButton(b)) overrideIO = true;
  }
  return overrideIO;
}
bool UsermodManager::getUMData(um_data_t **data, uint8_t mod_id) {
  for (unsigned i = 0; i < numMods; i++) {
    if (mod_id > 0 && ums[i]->getId() != mod_id) continue;  // only get data form requested usermod if provided
    if (ums[i]->getUMData(data)) return true;               // if usermod does provide data return immediately (only one usermod can provide data at one time)
  }
  return false;
}
void UsermodManager::addToJsonState(JsonObject& obj)    { for (unsigned i = 0; i < numMods; i++) ums[i]->addToJsonState(obj); }
void UsermodManager::addToJsonInfo(JsonObject& obj)     { for (unsigned i = 0; i < numMods; i++) ums[i]->addToJsonInfo(obj); }
void UsermodManager::readFromJsonState(JsonObject& obj) { for (unsigned i = 0; i < numMods; i++) ums[i]->readFromJsonState(obj); }
void UsermodManager::addToConfig(JsonObject& obj)       { for (unsigned i = 0; i < numMods; i++) ums[i]->addToConfig(obj); }
bool UsermodManager::readFromConfig(JsonObject& obj)    {
  bool allComplete = true;
  for (unsigned i = 0; i < numMods; i++) {
    if (!ums[i]->readFromConfig(obj)) allComplete = false;
  }
  return allComplete;
}
#ifndef WLED_DISABLE_MQTT
void UsermodManager::onMqttConnect(bool sessionPresent) { for (unsigned i = 0; i < numMods; i++) ums[i]->onMqttConnect(sessionPresent); }
bool UsermodManager::onMqttMessage(char* topic, char* payload) {
  for (unsigned i = 0; i < numMods; i++) if (ums[i]->onMqttMessage(topic, payload)) return true;
  return false;
}
#endif
#ifndef WLED_DISABLE_ESPNOW
bool UsermodManager::onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) {
  for (unsigned i = 0; i < numMods; i++) if (ums[i]->onEspNowMessage(sender, payload, len)) return true;
  return false;
}
#endif
void UsermodManager::onUpdateBegin(bool init) { for (unsigned i = 0; i < numMods; i++) ums[i]->onUpdateBegin(init); } // notify usermods that update is to begin
void UsermodManager::onStateChange(uint8_t mode) { for (unsigned i = 0; i < numMods; i++) ums[i]->onStateChange(mode); } // notify usermods that WLED state changed

/*
 * Enables usermods to lookup another Usermod.
 */
Usermod* UsermodManager::lookup(uint16_t mod_id) {
  for (unsigned i = 0; i < numMods; i++) {
    if (ums[i]->getId() == mod_id) {
      return ums[i];
    }
  }
  return nullptr;
}

bool UsermodManager::add(Usermod* um)
{
  if (numMods >= WLED_MAX_USERMODS || um == nullptr) return false;
  ums[numMods++] = um;
  return true;
}

Usermod* UsermodManager::ums[WLED_MAX_USERMODS] = {nullptr};
byte UsermodManager::numMods = 0;

/* Usermod v2 interface shim for oappend */
Print* Usermod::oappend_shim = nullptr;

void Usermod::appendConfigData(Print& settingsScript) {
  assert(!oappend_shim);
  oappend_shim = &settingsScript;
  this->appendConfigData();
  oappend_shim = nullptr;
}
