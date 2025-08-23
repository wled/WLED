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


/**
 * @brief Handle actions performed immediately after establishing an MQTT connection.
 *
 * Subscribes to configured device and group topics (base topic plus "/col" and "/api"),
 * notifies usermods of the connection, logs readiness, and publishes the current state.
 *
 * The function is safe to call on reconnect and will re-subscribe to topics each time.
 *
 * @param sessionPresent true if the broker reports an existing session for this client; false otherwise.
 */
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


/**
 * @brief Handle incoming MQTT messages for the device.
 *
 * Reassembles potentially fragmented MQTT payloads into a null-terminated C string,
 * strips the configured device/group topic prefix, and dispatches the resulting
 * topic+payload to the appropriate handler:
 * - "/col"  — parses color (decimal or hex) into the priority color buffer and triggers a color update.
 * - "/api"  — if payload starts with '{' it is parsed as JSON and applied to state; otherwise treated as an HTTP-style API request and forwarded to handleSet. JSON processing acquires the JSON buffer lock.
 * - non-empty remaining topic — forwarded to UsermodManager::onMqttMessage.
 * - empty remaining topic — interpreted as a brightness/state command and passed to parseMQTTBriPayload.
 *
 * The function returns early if payload is null or if the temporary buffer cannot be allocated.
 *
 * Side effects: updates global state (colors, brightness, etc.), invokes UsermodManager callbacks,
 * may allocate/free a temporary buffer for reassembly, and uses JSON buffer locking when handling "/api".
 *
 * @param topic MQTT topic for this message (may be modified internally to remove prefix).
 * @param payload Pointer to the received payload fragment.
 * @param properties MQTT message properties (unused by this implementation).
 * @param len Length of the current payload fragment.
 * @param index Byte offset of this fragment within the reassembled payload.
 * @param total Total expected size of the reassembled payload.
 */
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
  /**
 * @brief Number of bytes written into the buffer.
 *
 * @return size_t Current write position (bytes stored).
 */
size_t size() const { return _offset; }
  /**
 * @brief Return the total capacity of the underlying buffer.
 *
 * @return size_t Total capacity in bytes (maximum writable size).
 */
size_t capacity() const { return _size; }
};
}; /**
 * @brief Publish current device state to MQTT topics.
 *
 * Publishes brightness, color, retained online status, and a full XML state payload
 * to the configured device MQTT topic when the MQTT client is connected.
 *
 * Behavior and side effects:
 * - No-op if MQTT is not connected.
 * - Publishes "<deviceTopic>/g" with the current brightness (bri).
 * - Publishes "<deviceTopic>/c" with a hex color string derived from the priority color buffer (colPri).
 * - Publishes "<deviceTopic>/status" with the retained payload "online" (used as LWT indicator).
 * - Builds an XML representation of the full state and publishes it to "<deviceTopic>/v" (payload length provided).
 * - Uses the global retainMqttMsg flag to decide whether most messages are retained.
 *
 * Notes:
 * - When USERMOD_SMARTNEST is defined, the detailed publish block is excluded (no state publications).
 */


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

  sprintf_P(s, PSTR("#%06X"), (colPri[3] << 24) | (colPri[0] << 16) | (colPri[1] << 8) | (colPri[2]));
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
