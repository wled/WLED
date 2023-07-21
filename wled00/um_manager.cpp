#include "wled.h"
/*
 * Registration and management utility for v2 usermods
 */

//Usermod Manager internals
void UsermodManager::setup()             { for (byte i = 0; i < numMods; i++) ums[i]->setup(); }
void UsermodManager::connected()         { for (byte i = 0; i < numMods; i++) ums[i]->connected(); }
void UsermodManager::loop()              { for (byte i = 0; i < numMods; i++) ums[i]->loop();  }
void UsermodManager::handleOverlayDraw() { for (byte i = 0; i < numMods; i++) ums[i]->handleOverlayDraw(); }
void UsermodManager::appendConfigData()  { for (byte i = 0; i < numMods; i++) ums[i]->appendConfigData(); }
bool UsermodManager::handleButton(uint8_t b) {
  bool overrideIO = false;
  for (byte i = 0; i < numMods; i++) {
    if (ums[i]->handleButton(b)) overrideIO = true;
  }
  return overrideIO;
}
bool UsermodManager::getUMData(um_data_t **data, uint8_t mod_id) {
  for (byte i = 0; i < numMods; i++) {
    if (mod_id > 0 && ums[i]->getId() != mod_id) continue;  // only get data form requested usermod if provided
    if (ums[i]->getUMData(data)) return true;               // if usermod does provide data return immediately (only one usermod can povide data at one time)
  }
  return false;
}
void UsermodManager::addToJsonState(JsonObject& obj)    { for (byte i = 0; i < numMods; i++) ums[i]->addToJsonState(obj); }
void UsermodManager::addToJsonInfo(JsonObject& obj)     { for (byte i = 0; i < numMods; i++) ums[i]->addToJsonInfo(obj); }
void UsermodManager::readFromJsonState(JsonObject& obj) { for (byte i = 0; i < numMods; i++) ums[i]->readFromJsonState(obj); }
void UsermodManager::addToConfig(JsonObject& obj)       { for (byte i = 0; i < numMods; i++) ums[i]->addToConfig(obj); }
bool UsermodManager::readFromConfig(JsonObject& obj)    {
  bool allComplete = true;
  for (byte i = 0; i < numMods; i++) {
    if (!ums[i]->readFromConfig(obj)) allComplete = false;
  }
  return allComplete;
}
void UsermodManager::onMqttConnect(bool sessionPresent) { for (byte i = 0; i < numMods; i++) ums[i]->onMqttConnect(sessionPresent); }
bool UsermodManager::onMqttMessage(char* topic, char* payload) {
  for (byte i = 0; i < numMods; i++) if (ums[i]->onMqttMessage(topic, payload)) return true;
  return false;
}
void UsermodManager::onUpdateBegin(bool init) { for (byte i = 0; i < numMods; i++) ums[i]->onUpdateBegin(init); } // notify usermods that update is to begin
void UsermodManager::onStateChange(uint8_t mode) { for (byte i = 0; i < numMods; i++) ums[i]->onStateChange(mode); } // notify usermods that WLED state changed

/*
 * Enables usermods to lookup another Usermod.
 */
Usermod* UsermodManager::lookup(uint16_t mod_id) {
  for (byte i = 0; i < numMods; i++) {
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
