#include "wled.h"
/*
 * Registration and management utility for v2 usermods
 */

// Global usermod instance list
// Table begin and end references
// Zero-length arrays -- so they'll get assigned addresses, but consume no flash
// The numeric suffix ensures they're put in the right place; the linker script will sort them
// We stick them in the '.dtors' segment because it's always included by the linker scripts
// even though it never gets called.  Who calls exit() in an embedded program anyways?
// If someone ever does, though, it'll explode as these aren't function pointers.
DECLARE_DYNARRAY(Usermod*, usermods);

static size_t getCount() {  
  return DYNARRAY_LENGTH(usermods);
}


//Usermod Manager internals
void UsermodManager::setup()             { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->setup(); }
void UsermodManager::connected()         { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->connected(); }
void UsermodManager::loop()              { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->loop();  }
void UsermodManager::handleOverlayDraw() { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->handleOverlayDraw(); }
void UsermodManager::appendConfigData(Print& dest)  { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->appendConfigData(dest); }
bool UsermodManager::handleButton(uint8_t b) {
  bool overrideIO = false;
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) {
    if ((*mod)->handleButton(b)) overrideIO = true;
  }
  return overrideIO;
}
bool UsermodManager::getUMData(um_data_t **data, uint8_t mod_id) {
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) {
    if (mod_id > 0 && (*mod)->getId() != mod_id) continue;  // only get data form requested usermod if provided
    if ((*mod)->getUMData(data)) return true;               // if usermod does provide data return immediately (only one usermod can provide data at one time)
  }
  return false;
}
void UsermodManager::addToJsonState(JsonObject& obj)    { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->addToJsonState(obj); }
void UsermodManager::addToJsonInfo(JsonObject& obj)     {
  auto um_id_list = obj.createNestedArray("um");  
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) {
    um_id_list.add((*mod)->getId());
    (*mod)->addToJsonInfo(obj);
  }
}
void UsermodManager::readFromJsonState(JsonObject& obj) { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->readFromJsonState(obj); }
void UsermodManager::addToConfig(JsonObject& obj)       { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->addToConfig(obj); }
bool UsermodManager::readFromConfig(JsonObject& obj)    {
  Serial.printf_P(PSTR("Mods: %d  Begin: %08X  End: %08X\n"), getCount(), (intptr_t) &DYNARRAY_BEGIN(usermods)[0], (intptr_t) &DYNARRAY_END(usermods)[0]);
  Usermod** volatile x =  (Usermod**) DYNARRAY_BEGIN(usermods);
  Serial.printf_P(PSTR("X: %08X\n"), (intptr_t) x);
  Serial.printf_P(PSTR("*X: %08X\n"), (intptr_t) *x);

  bool allComplete = true;  
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) {
    if (!(*mod)->readFromConfig(obj)) allComplete = false;
  }
  return allComplete;
}
#ifndef WLED_DISABLE_MQTT
void UsermodManager::onMqttConnect(bool sessionPresent) { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->onMqttConnect(sessionPresent); }
bool UsermodManager::onMqttMessage(char* topic, char* payload) {
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) if ((*mod)->onMqttMessage(topic, payload)) return true;
  return false;
}
#endif
#ifndef WLED_DISABLE_ESPNOW
bool UsermodManager::onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) {
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) if ((*mod)->onEspNowMessage(sender, payload, len)) return true;
  return false;
}
#endif
bool UsermodManager::onUdpPacket(uint8_t* payload, size_t len) {
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) if ((*mod)->onUdpPacket(payload, len)) return true;
  return false;
}
void UsermodManager::onUpdateBegin(bool init) { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->onUpdateBegin(init); } // notify usermods that update is to begin
void UsermodManager::onStateChange(uint8_t mode) { for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) (*mod)->onStateChange(mode); } // notify usermods that WLED state changed

/*
 * Enables usermods to lookup another Usermod.
 */
Usermod* UsermodManager::lookup(uint16_t mod_id) {
  for (auto mod = DYNARRAY_BEGIN(usermods); mod < DYNARRAY_END(usermods); ++mod) {
    if ((*mod)->getId() == mod_id) {
      return *mod;
    }
  }
  return nullptr;
}

size_t UsermodManager::getModCount() { return getCount(); };

/* Usermod v2 interface shim for oappend */
Print* Usermod::oappend_shim = nullptr;

void Usermod::appendConfigData(Print& settingsScript) {
  assert(!oappend_shim);
  oappend_shim = &settingsScript;
  this->appendConfigData();
  oappend_shim = nullptr;
}
