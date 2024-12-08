#include "wled.h"

/*
 * MQTT communication protocol for home automation
 */

#ifndef WLED_DISABLE_MQTT
#define MQTT_KEEP_ALIVE_TIME 60    // contact the MQTT broker every 60 seconds

static void parseMQTTBriPayload(char* payload)
{
  if      (strstr(payload, "ON") || strstr(payload, "on") || strstr(payload, "true")) {bri = briLast; stateUpdated(CALL_MODE_DIRECT_CHANGE);}
  else if (strstr(payload, "T" ) || strstr(payload, "t" )) {toggleOnOff(); stateUpdated(CALL_MODE_DIRECT_CHANGE);}
  else {
    uint8_t in = strtoul(payload, NULL, 10);
    if (in == 0 && bri > 0) briLast = bri;
    bri = in;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }
}


static void onMqttConnect(bool sessionPresent)
{
  //(re)subscribe to required topics
  char subuf[38];

  if (mqttDeviceTopic[0] != 0) {
    strlcpy(subuf, mqttDeviceTopic, 33);
    mqtt->subscribe(subuf, 0);
    strcat_P(subuf, PSTR("/col"));
    mqtt->subscribe(subuf, 0);
    strlcpy(subuf, mqttDeviceTopic, 33);
    strcat_P(subuf, PSTR("/api"));
    mqtt->subscribe(subuf, 0);
  }

  if (mqttGroupTopic[0] != 0) {
    strlcpy(subuf, mqttGroupTopic, 33);
    mqtt->subscribe(subuf, 0);
    strcat_P(subuf, PSTR("/col"));
    mqtt->subscribe(subuf, 0);
    strlcpy(subuf, mqttGroupTopic, 33);
    strcat_P(subuf, PSTR("/api"));
    mqtt->subscribe(subuf, 0);
  }

  UsermodManager::onMqttConnect(sessionPresent);

  DEBUG_PRINTLN(F("MQTT ready"));
  publishMqtt();
}


static void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  static char *payloadStr;

  DEBUG_PRINTF_P(PSTR("MQTT msg: %s\n"), topic);

  // paranoia check to avoid npe if no payload
  if (payload==nullptr) {
    DEBUG_PRINTLN(F("no payload -> leave"));
    return;
  }

  if (index == 0) {                       // start (1st partial packet or the only packet)
    if (payloadStr) delete[] payloadStr;  // fail-safe: release buffer
    payloadStr = new char[total+1];       // allocate new buffer
  }
  if (payloadStr == nullptr) return;      // buffer not allocated

  // copy (partial) packet to buffer and 0-terminate it if it is last packet
  char* buff = payloadStr + index;
  memcpy(buff, payload, len);
  if (index + len >= total) { // at end
    payloadStr[total] = '\0'; // terminate c style string
  } else {
    DEBUG_PRINTLN(F("MQTT partial packet received."));
    return; // process next packet
  }
  DEBUG_PRINTLN(payloadStr);

  size_t topicPrefixLen = strlen(mqttDeviceTopic);
  if (strncmp(topic, mqttDeviceTopic, topicPrefixLen) == 0) {
    topic += topicPrefixLen;
  } else {
    topicPrefixLen = strlen(mqttGroupTopic);
    if (strncmp(topic, mqttGroupTopic, topicPrefixLen) == 0) {
      topic += topicPrefixLen;
    } else {
      // Non-Wled Topic used here. Probably a usermod subscribed to this topic.
      UsermodManager::onMqttMessage(topic, payloadStr);
      delete[] payloadStr;
      payloadStr = nullptr;
      return;
    }
  }

  //Prefix is stripped from the topic at this point

  if (strcmp_P(topic, PSTR("/col")) == 0) {
    colorFromDecOrHexString(col, payloadStr);
    colorUpdated(CALL_MODE_DIRECT_CHANGE);
  } else if (strcmp_P(topic, PSTR("/api")) == 0) {
    if (requestJSONBufferLock(15)) {
      if (payloadStr[0] == '{') { //JSON API
        deserializeJson(*pDoc, payloadStr);
        deserializeState(pDoc->as<JsonObject>());
      } else { //HTTP API
        String apireq = "win"; apireq += '&'; // reduce flash string usage
        apireq += payloadStr;
        handleSet(nullptr, apireq);
      }
      releaseJSONBufferLock();
    }
  } else if (strlen(topic) != 0) {
    // non standard topic, check with usermods
    UsermodManager::onMqttMessage(topic, payloadStr);
  } else {
    // topmost topic (just wled/MAC)
    parseMQTTBriPayload(payloadStr);
  }
  delete[] payloadStr;
  payloadStr = nullptr;
}

// Print adapter for flat buffers
namespace { 
class bufferPrint : public Print {
  char* _buf;
  size_t _size, _offset;
  public:

  bufferPrint(char* buf, size_t size) : _buf(buf), _size(size), _offset(0) {};

  size_t write(const uint8_t *buffer, size_t size) {
    size = std::min(size, _size - _offset);
    memcpy(_buf + _offset, buffer, size);
    _offset += size;
    return size;
  }

  size_t write(uint8_t c) {
    return this->write(&c, 1);
  }

  char* data() const { return _buf; }
  size_t size() const { return _offset; }
  size_t capacity() const { return _size; }
};
}; // anonymous namespace


void publishMqtt()
{
  if (!WLED_MQTT_CONNECTED) return;
  DEBUG_PRINTLN(F("Publish MQTT"));

  #ifndef USERMOD_SMARTNEST
  char s[10];
  char subuf[48];

  sprintf_P(s, PSTR("%u"), bri);
  strlcpy(subuf, mqttDeviceTopic, 33);
  strcat_P(subuf, PSTR("/g"));
  mqtt->publish(subuf, 0, retainMqttMsg, s);         // optionally retain message (#2263)

  sprintf_P(s, PSTR("#%06X"), (col[3] << 24) | (col[0] << 16) | (col[1] << 8) | (col[2]));
  strlcpy(subuf, mqttDeviceTopic, 33);
  strcat_P(subuf, PSTR("/c"));
  mqtt->publish(subuf, 0, retainMqttMsg, s);         // optionally retain message (#2263)

  strlcpy(subuf, mqttDeviceTopic, 33);
  strcat_P(subuf, PSTR("/status"));
  mqtt->publish(subuf, 0, true, "online");          // retain message for a LWT

  // TODO: use a DynamicBufferList.  Requires a list-read-capable MQTT client API.
  DynamicBuffer buf(1024);
  bufferPrint pbuf(buf.data(), buf.size());
  XML_response(pbuf);
  strlcpy(subuf, mqttDeviceTopic, 33);
  strcat_P(subuf, PSTR("/v"));
  mqtt->publish(subuf, 0, retainMqttMsg, buf.data(), pbuf.size());   // optionally retain message (#2263)
  #endif
}


//HA autodiscovery was removed in favor of the native integration in HA v0.102.0

bool initMqtt()
{
  if (!mqttEnabled || mqttServer[0] == 0 || !WLED_CONNECTED) return false;

  if (mqtt == nullptr) {
    mqtt = new AsyncMqttClient();
    if (!mqtt) return false;
    mqtt->onMessage(onMqttMessage);
    mqtt->onConnect(onMqttConnect);
  }
  if (mqtt->connected()) return true;

  DEBUG_PRINTLN(F("Reconnecting MQTT"));
  IPAddress mqttIP;
  if (mqttIP.fromString(mqttServer)) //see if server is IP or domain
  {
    mqtt->setServer(mqttIP, mqttPort);
  } else {
    mqtt->setServer(mqttServer, mqttPort);
  }
  mqtt->setClientId(mqttClientID);
  if (mqttUser[0] && mqttPass[0]) mqtt->setCredentials(mqttUser, mqttPass);

  #ifndef USERMOD_SMARTNEST
  strlcpy(mqttStatusTopic, mqttDeviceTopic, 33);
  strcat_P(mqttStatusTopic, PSTR("/status"));
  mqtt->setWill(mqttStatusTopic, 0, true, "offline"); // LWT message
  #endif
  mqtt->setKeepAlive(MQTT_KEEP_ALIVE_TIME);
  mqtt->connect();
  return true;
}
#endif
