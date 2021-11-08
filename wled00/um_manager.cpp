#include "wled.h"
/*
 * Registration and management utility for v2 usermods
 */

//Usermod Manager internals
void UsermodManager::loop()              { for (byte i = 0; i < numMods; i++) ums[i]->loop();  }
void UsermodManager::handleOverlayDraw() { for (byte i = 0; i < numMods; i++) ums[i]->handleOverlayDraw(); }
bool UsermodManager::handleButton(uint8_t b) { 
  bool overrideIO = false;
  for (byte i = 0; i < numMods; i++) {
    if (ums[i]->handleButton(b)) overrideIO = true;
  }
  return overrideIO;
}

void UsermodManager::setup()     { for (byte i = 0; i < numMods; i++) ums[i]->setup(); }
void UsermodManager::connected() { for (byte i = 0; i < numMods; i++) ums[i]->connected(); }

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
  ums[numMods] = um;
  numMods++;
  return true;
}

byte UsermodManager::getModCount() {return numMods;}