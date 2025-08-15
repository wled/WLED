#include "wled.h"

/*
 * MQTT communication protocol for home automation
 */

#ifndef WLED_DISABLE_MQTT
#define MQTT_KEEP_ALIVE_TIME 60    // contact the MQTT broker every 60 seconds

#if MQTT_MAX_TOPIC_LEN > 32
#warning "MQTT topics length > 32 is not recommended for compatibility with usermods!"
#endif

static const char* sTopicFormat PROGMEM = "%.*s/%s";

// parse payload for brightness, ON/OFF or toggle
// briLast is used to remember last brightness value in case of ON/OFF or toggle
// bri is set to 0 if payload is "0" or "OFF" or "false"
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
  char subuf[MQTT_MAX_TOPIC_LEN + 9];

  if (mqttDeviceTopic[0] != 0) {
    mqtt->subscribe(mqttDeviceTopic, 0);
    snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "col");
    mqtt->subscribe(subuf, 0);
    snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "api");
    mqtt->subscribe(subuf, 0);
  }

  if (mqttGroupTopic[0] != 0) {
    mqtt->subscribe(mqttGroupTopic, 0);
    snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttGroupTopic, "col");
    mqtt->subscribe(subuf, 0);
    snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttGroupTopic, "api");
    mqtt->subscribe(subuf, 0);
  }

  UsermodManager::onMqttConnect(sessionPresent);

  DEBUG_PRINTLN(F("MQTT ready"));

#ifndef USERMOD_SMARTNEST
  snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "status");
  mqtt->publish(subuf, 0, true, "online"); // retain message for a LWT
#endif

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
    p_free(payloadStr);                   // release buffer if it exists
    payloadStr = static_cast<char*>(p_malloc(total+1)); // allocate new buffer
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
      p_free(payloadStr);
      payloadStr = nullptr;
      return;
    }
  }

  //Prefix is stripped from the topic at this point

  if (strcmp_P(topic, PSTR("/col")) == 0) {
    colorFromDecOrHexString(colPri, payloadStr);
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
  p_free(payloadStr);
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
  char subuf[MQTT_MAX_TOPIC_LEN + 16];

  sprintf_P(s, PSTR("%u"), bri);
  snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "g");
  mqtt->publish(subuf, 0, retainMqttMsg, s);         // optionally retain message (#2263)

  sprintf_P(s, PSTR("#%06X"), (colPri[3] << 24) | (colPri[0] << 16) | (colPri[1] << 8) | (colPri[2]));
  snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "c");
  mqtt->publish(subuf, 0, retainMqttMsg, s);         // optionally retain message (#2263)

  snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "status");
  mqtt->publish(subuf, 0, true, "online");  // retain message for a LWT

  // TODO: use a DynamicBufferList.  Requires a list-read-capable MQTT client API.
  DynamicBuffer buf(1024);
  bufferPrint pbuf(buf.data(), buf.size());
  XML_response(pbuf);
  snprintf_P(subuf, sizeof(subuf)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "v");
  mqtt->publish(subuf, 0, retainMqttMsg, buf.data(), pbuf.size());   // optionally retain message (#2263)
  #endif
}


//HA autodiscovery was removed in favor of the native integration in HA v0.102.0

bool initMqtt()
{
  if (!mqttEnabled || mqttServer[0] == 0 || !WLED_CONNECTED) return false;

  if (mqtt == nullptr) {
    void *ptr = p_malloc(sizeof(AsyncMqttClient));
    mqtt = new (ptr) AsyncMqttClient(); // use placement new (into PSRAM), client will never be deleted
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
    #ifdef ARDUINO_ARCH_ESP32
    String mqttMDNS = mqttServer;
    mqttMDNS.toLowerCase(); // make sure we have a lowercase hostname
    int pos = mqttMDNS.indexOf(F(".local"));
    if (pos > 0) mqttMDNS.remove(pos); // remove .local domain if present (and anything following it)
    if (strlen(cmDNS) > 0 && mqttMDNS.length() > 0 && mqttMDNS.indexOf('.') < 0) { // if mDNS is enabled and server does not have domain
      mqttIP = MDNS.queryHost(mqttMDNS.c_str());
      if (mqttIP != IPAddress()) // if MDNS resolved the hostname
        mqtt->setServer(mqttIP, mqttPort);
      else
        mqtt->setServer(mqttServer, mqttPort);
    } else
    #endif
      mqtt->setServer(mqttServer, mqttPort);
  }
  mqtt->setClientId(mqttClientID);
  if (mqttUser[0] && mqttPass[0]) mqtt->setCredentials(mqttUser, mqttPass);

  #ifndef USERMOD_SMARTNEST
  snprintf_P(mqttStatusTopic, sizeof(mqttStatusTopic)-1, sTopicFormat, MQTT_MAX_TOPIC_LEN, mqttDeviceTopic, "status");
  mqtt->setWill(mqttStatusTopic, 0, true, "offline"); // LWT message
  #endif
  mqtt->setKeepAlive(MQTT_KEEP_ALIVE_TIME);
  mqtt->connect();
  return true;
}
#endif
