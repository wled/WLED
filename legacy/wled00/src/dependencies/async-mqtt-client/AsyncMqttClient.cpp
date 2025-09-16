#include "AsyncMqttClient.hpp"

AsyncMqttClient::AsyncMqttClient()
: _connected(false)
, _connectPacketNotEnoughSpace(false)
, _disconnectFlagged(false)
, _tlsBadFingerprint(false)
, _lastClientActivity(0)
, _lastServerActivity(0)
, _lastPingRequestTime(0)
, _host(nullptr)
, _useIp(false)
#if ASYNC_TCP_SSL_ENABLED
, _secure(false)
#endif
, _port(0)
, _keepAlive(15)
, _cleanSession(true)
, _clientId(nullptr)
, _username(nullptr)
, _password(nullptr)
, _willTopic(nullptr)
, _willPayload(nullptr)
, _willPayloadLength(0)
, _willQos(0)
, _willRetain(false)
, _parsingInformation { .bufferState = AsyncMqttClientInternals::BufferState::NONE }
, _currentParsedPacket(nullptr)
, _remainingLengthBufferPosition(0)
, _nextPacketId(1) {
  _client.onConnect([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onConnect(c); }, this);
  _client.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onDisconnect(c); }, this);
  _client.onError([](void* obj, AsyncClient* c, int8_t error) { (static_cast<AsyncMqttClient*>(obj))->_onError(c, error); }, this);
  _client.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<AsyncMqttClient*>(obj))->_onTimeout(c, time); }, this);
  _client.onAck([](void* obj, AsyncClient* c, size_t len, uint32_t time) { (static_cast<AsyncMqttClient*>(obj))->_onAck(c, len, time); }, this);
  _client.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<AsyncMqttClient*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
  _client.onPoll([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onPoll(c); }, this);

#ifdef ESP32
  sprintf(_generatedClientId, "esp32%06x", (uint32_t)ESP.getEfuseMac());
  _xSemaphore = xSemaphoreCreateMutex();
#elif defined(ESP8266)
  sprintf(_generatedClientId, "esp8266%06x", (uint32_t)ESP.getChipId());
#endif
  _clientId = _generatedClientId;

  setMaxTopicLength(128);
}

AsyncMqttClient::~AsyncMqttClient() {
  delete _currentParsedPacket;
  delete[] _parsingInformation.topicBuffer;
#ifdef ESP32
  vSemaphoreDelete(_xSemaphore);
#endif
}

AsyncMqttClient& AsyncMqttClient::setKeepAlive(uint16_t keepAlive) {
  _keepAlive = keepAlive;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setClientId(const char* clientId) {
  _clientId = clientId;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setCleanSession(bool cleanSession) {
  _cleanSession = cleanSession;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setMaxTopicLength(uint16_t maxTopicLength) {
  _parsingInformation.maxTopicLength = maxTopicLength;
  delete[] _parsingInformation.topicBuffer;
  _parsingInformation.topicBuffer = new char[maxTopicLength + 1];
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setCredentials(const char* username, const char* password) {
  _username = username;
  _password = password;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setWill(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length) {
  _willTopic = topic;
  _willQos = qos;
  _willRetain = retain;
  _willPayload = payload;
  _willPayloadLength = length;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setServer(IPAddress ip, uint16_t port) {
  _useIp = true;
  _ip = ip;
  _port = port;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setServer(const char* host, uint16_t port) {
  _useIp = false;
  _host = host;
  _port = port;
  return *this;
}

#if ASYNC_TCP_SSL_ENABLED
AsyncMqttClient& AsyncMqttClient::setSecure(bool secure) {
  _secure = secure;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::addServerFingerprint(const uint8_t* fingerprint) {
  std::array<uint8_t, SHA1_SIZE> newFingerprint;
  memcpy(newFingerprint.data(), fingerprint, SHA1_SIZE);
  _secureServerFingerprints.push_back(newFingerprint);
  return *this;
}
#endif

AsyncMqttClient& AsyncMqttClient::onConnect(AsyncMqttClientInternals::OnConnectUserCallback callback) {
  _onConnectUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onDisconnect(AsyncMqttClientInternals::OnDisconnectUserCallback callback) {
  _onDisconnectUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onSubscribe(AsyncMqttClientInternals::OnSubscribeUserCallback callback) {
  _onSubscribeUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onUnsubscribe(AsyncMqttClientInternals::OnUnsubscribeUserCallback callback) {
  _onUnsubscribeUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onMessage(AsyncMqttClientInternals::OnMessageUserCallback callback) {
  _onMessageUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onPublish(AsyncMqttClientInternals::OnPublishUserCallback callback) {
  _onPublishUserCallbacks.push_back(callback);
  return *this;
}

void AsyncMqttClient::_freeCurrentParsedPacket() {
  delete _currentParsedPacket;
  _currentParsedPacket = nullptr;
}

void AsyncMqttClient::_clear() {
  _lastPingRequestTime = 0;
  _connected = false;
  _disconnectFlagged = false;
  _connectPacketNotEnoughSpace = false;
  _tlsBadFingerprint = false;
  _freeCurrentParsedPacket();

  _pendingPubRels.clear();
  _pendingPubRels.shrink_to_fit();

  _toSendAcks.clear();
  _toSendAcks.shrink_to_fit();

  _nextPacketId = 1;
  _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::NONE;
}

/* TCP */
void AsyncMqttClient::_onConnect(AsyncClient* client) {
  (void)client;

#if ASYNC_TCP_SSL_ENABLED
  if (_secure && _secureServerFingerprints.size() > 0) {
    SSL* clientSsl = _client.getSSL();

    bool sslFoundFingerprint = false;
    for (std::array<uint8_t, SHA1_SIZE> fingerprint : _secureServerFingerprints) {
      if (ssl_match_fingerprint(clientSsl, fingerprint.data()) == SSL_OK) {
        sslFoundFingerprint = true;
        break;
      }
    }

    if (!sslFoundFingerprint) {
      _tlsBadFingerprint = true;
      _client.close(true);
      return;
    }
  }
#endif

  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.CONNECT;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.CONNECT_RESERVED;

  uint16_t protocolNameLength = 4;
  char protocolNameLengthBytes[2];
  protocolNameLengthBytes[0] = protocolNameLength >> 8;
  protocolNameLengthBytes[1] = protocolNameLength & 0xFF;

  char protocolLevel[1];
  protocolLevel[0] = 0x04;

  char connectFlags[1];
  connectFlags[0] = 0;
  if (_cleanSession) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.CLEAN_SESSION;
  if (_username != nullptr) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.USERNAME;
  if (_password != nullptr) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.PASSWORD;
  if (_willTopic != nullptr) {
    connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL;
    if (_willRetain) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_RETAIN;
    switch (_willQos) {
      case 0:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS0;
        break;
      case 1:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS1;
        break;
      case 2:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS2;
        break;
    }
  }

  char keepAliveBytes[2];
  keepAliveBytes[0] = _keepAlive >> 8;
  keepAliveBytes[1] = _keepAlive & 0xFF;

  uint16_t clientIdLength = strlen(_clientId);
  char clientIdLengthBytes[2];
  clientIdLengthBytes[0] = clientIdLength >> 8;
  clientIdLengthBytes[1] = clientIdLength & 0xFF;

  // Optional fields
  uint16_t willTopicLength = 0;
  char willTopicLengthBytes[2];
  uint16_t willPayloadLength = _willPayloadLength;
  char willPayloadLengthBytes[2];
  if (_willTopic != nullptr) {
    willTopicLength = strlen(_willTopic);
    willTopicLengthBytes[0] = willTopicLength >> 8;
    willTopicLengthBytes[1] = willTopicLength & 0xFF;

    if (_willPayload != nullptr && willPayloadLength == 0) willPayloadLength = strlen(_willPayload);

    willPayloadLengthBytes[0] = willPayloadLength >> 8;
    willPayloadLengthBytes[1] = willPayloadLength & 0xFF;
  }

  uint16_t usernameLength = 0;
  char usernameLengthBytes[2];
  if (_username != nullptr) {
    usernameLength = strlen(_username);
    usernameLengthBytes[0] = usernameLength >> 8;
    usernameLengthBytes[1] = usernameLength & 0xFF;
  }

  uint16_t passwordLength = 0;
  char passwordLengthBytes[2];
  if (_password != nullptr) {
    passwordLength = strlen(_password);
    passwordLengthBytes[0] = passwordLength >> 8;
    passwordLengthBytes[1] = passwordLength & 0xFF;
  }

  uint32_t remainingLength = 2 + protocolNameLength + 1 + 1 + 2 + 2 + clientIdLength;  // always present
  if (_willTopic != nullptr) remainingLength += 2 + willTopicLength + 2 + willPayloadLength;
  if (_username != nullptr) remainingLength += 2 + usernameLength;
  if (_password != nullptr) remainingLength += 2 + passwordLength;
  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(remainingLength, fixedHeader + 1);

  uint32_t neededSpace = 1 + remainingLengthLength;
  neededSpace += 2;
  neededSpace += protocolNameLength;
  neededSpace += 1;
  neededSpace += 1;
  neededSpace += 2;
  neededSpace += 2;
  neededSpace += clientIdLength;
  if (_willTopic != nullptr) {
    neededSpace += 2;
    neededSpace += willTopicLength;

    neededSpace += 2;
    if (_willPayload != nullptr) neededSpace += willPayloadLength;
  }
  if (_username != nullptr) {
    neededSpace += 2;
    neededSpace += usernameLength;
  }
  if (_password != nullptr) {
    neededSpace += 2;
    neededSpace += passwordLength;
  }

  SEMAPHORE_TAKE();
  if (_client.space() < neededSpace) {
    _connectPacketNotEnoughSpace = true;
    _client.close(true);
    SEMAPHORE_GIVE();
    return;
  }

  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(protocolNameLengthBytes, 2);
  _client.add("MQTT", protocolNameLength);
  _client.add(protocolLevel, 1);
  _client.add(connectFlags, 1);
  _client.add(keepAliveBytes, 2);
  _client.add(clientIdLengthBytes, 2);
  _client.add(_clientId, clientIdLength);
  if (_willTopic != nullptr) {
    _client.add(willTopicLengthBytes, 2);
    _client.add(_willTopic, willTopicLength);

    _client.add(willPayloadLengthBytes, 2);
    if (_willPayload != nullptr) _client.add(_willPayload, willPayloadLength);
  }
  if (_username != nullptr) {
    _client.add(usernameLengthBytes, 2);
    _client.add(_username, usernameLength);
  }
  if (_password != nullptr) {
    _client.add(passwordLengthBytes, 2);
    _client.add(_password, passwordLength);
  }
  _client.send();
  _lastClientActivity = millis();
  SEMAPHORE_GIVE();
}

void AsyncMqttClient::_onDisconnect(AsyncClient* client) {
  (void)client;
  if (!_disconnectFlagged) {
    AsyncMqttClientDisconnectReason reason;

    if (_connectPacketNotEnoughSpace) {
      reason = AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE;
    } else if (_tlsBadFingerprint) {
      reason = AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT;
    } else {
      reason = AsyncMqttClientDisconnectReason::TCP_DISCONNECTED;
    }
    for (auto callback : _onDisconnectUserCallbacks) callback(reason);
  }
  _clear();
}

void AsyncMqttClient::_onError(AsyncClient* client, int8_t error) {
  (void)client;
  (void)error;
  // _onDisconnect called anyway
}

void AsyncMqttClient::_onTimeout(AsyncClient* client, uint32_t time) {
  (void)client;
  (void)time;
  // disconnection will be handled by ping/pong management
}

void AsyncMqttClient::_onAck(AsyncClient* client, size_t len, uint32_t time) {
  (void)client;
  (void)len;
  (void)time;
}

void AsyncMqttClient::_onData(AsyncClient* client, char* data, size_t len) {
  (void)client;
  size_t currentBytePosition = 0;
  char currentByte;
  do {
    switch (_parsingInformation.bufferState) {
      case AsyncMqttClientInternals::BufferState::NONE:
        currentByte = data[currentBytePosition++];
        _parsingInformation.packetType = currentByte >> 4;
        _parsingInformation.packetFlags = (currentByte << 4) >> 4;
        _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::REMAINING_LENGTH;
        _lastServerActivity = millis();
        switch (_parsingInformation.packetType) {
          case AsyncMqttClientInternals::PacketType.CONNACK:
            _currentParsedPacket = new AsyncMqttClientInternals::ConnAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onConnAck, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.PINGRESP:
            _currentParsedPacket = new AsyncMqttClientInternals::PingRespPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPingResp, this));
            break;
          case AsyncMqttClientInternals::PacketType.SUBACK:
            _currentParsedPacket = new AsyncMqttClientInternals::SubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onSubAck, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.UNSUBACK:
            _currentParsedPacket = new AsyncMqttClientInternals::UnsubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onUnsubAck, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBLISH:
            _currentParsedPacket = new AsyncMqttClientInternals::PublishPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9), std::bind(&AsyncMqttClient::_onPublish, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.PUBREL:
            _currentParsedPacket = new AsyncMqttClientInternals::PubRelPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubRel, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBACK:
            _currentParsedPacket = new AsyncMqttClientInternals::PubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubAck, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBREC:
            _currentParsedPacket = new AsyncMqttClientInternals::PubRecPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubRec, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBCOMP:
            _currentParsedPacket = new AsyncMqttClientInternals::PubCompPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubComp, this, std::placeholders::_1));
            break;
          default:
            break;
        }
        break;
      case AsyncMqttClientInternals::BufferState::REMAINING_LENGTH:
        currentByte = data[currentBytePosition++];
        _remainingLengthBuffer[_remainingLengthBufferPosition++] = currentByte;
        if (currentByte >> 7 == 0) {
          _parsingInformation.remainingLength = AsyncMqttClientInternals::Helpers::decodeRemainingLength(_remainingLengthBuffer);
          _remainingLengthBufferPosition = 0;
          if (_parsingInformation.remainingLength > 0) {
            _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::VARIABLE_HEADER;
          } else {
            // PINGRESP is a special case where it has no variable header, so the packet ends right here
            _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::NONE;
            _onPingResp();
          }
        }
        break;
      case AsyncMqttClientInternals::BufferState::VARIABLE_HEADER:
        _currentParsedPacket->parseVariableHeader(data, len, &currentBytePosition);
        break;
      case AsyncMqttClientInternals::BufferState::PAYLOAD:
        _currentParsedPacket->parsePayload(data, len, &currentBytePosition);
        break;
      default:
        currentBytePosition = len;
    }
  } while (currentBytePosition != len);
}

void AsyncMqttClient::_onPoll(AsyncClient* client) {
  if (!_connected) return;

  // if there is too much time the client has sent a ping request without a response, disconnect client to avoid half open connections
  if (_lastPingRequestTime != 0 && (millis() - _lastPingRequestTime) >= (_keepAlive * 1000 * 2)) {
    disconnect();
    return;
  // send ping to ensure the server will receive at least one message inside keepalive window
  } else if (_lastPingRequestTime == 0 && (millis() - _lastClientActivity) >= (_keepAlive * 1000 * 0.7)) {
    _sendPing();

  // send ping to verify if the server is still there (ensure this is not a half connection)
  } else if (_connected && _lastPingRequestTime == 0 && (millis() - _lastServerActivity) >= (_keepAlive * 1000 * 0.7)) {
    _sendPing();
  }

  // handle to send ack packets

  _sendAcks();

  // handle disconnect

  if (_disconnectFlagged) {
    _sendDisconnect();
  }
}

/* MQTT */
void AsyncMqttClient::_onPingResp() {
  _freeCurrentParsedPacket();
  _lastPingRequestTime = 0;
}

void AsyncMqttClient::_onConnAck(bool sessionPresent, uint8_t connectReturnCode) {
  (void)sessionPresent;
  _freeCurrentParsedPacket();

  if (connectReturnCode == 0) {
    _connected = true;
    for (auto callback : _onConnectUserCallbacks) callback(sessionPresent);
  } else {
    for (auto callback : _onDisconnectUserCallbacks) callback(static_cast<AsyncMqttClientDisconnectReason>(connectReturnCode));
    _disconnectFlagged = true;
  }
}

void AsyncMqttClient::_onSubAck(uint16_t packetId, char status) {
  _freeCurrentParsedPacket();

  for (auto callback : _onSubscribeUserCallbacks) callback(packetId, status);
}

void AsyncMqttClient::_onUnsubAck(uint16_t packetId) {
  _freeCurrentParsedPacket();

  for (auto callback : _onUnsubscribeUserCallbacks) callback(packetId);
}

void AsyncMqttClient::_onMessage(char* topic, char* payload, uint8_t qos, bool dup, bool retain, size_t len, size_t index, size_t total, uint16_t packetId) {
  bool notifyPublish = true;

  if (qos == 2) {
    for (AsyncMqttClientInternals::PendingPubRel pendingPubRel : _pendingPubRels) {
      if (pendingPubRel.packetId == packetId) {
        notifyPublish = false;
        break;
      }
    }
  }

  if (notifyPublish) {
    AsyncMqttClientMessageProperties properties;
    properties.qos = qos;
    properties.dup = dup;
    properties.retain = retain;

    for (auto callback : _onMessageUserCallbacks) callback(topic, payload, properties, len, index, total);
  }
}

void AsyncMqttClient::_onPublish(uint16_t packetId, uint8_t qos) {
  AsyncMqttClientInternals::PendingAck pendingAck;

  if (qos == 1) {
    pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBACK;
    pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBACK_RESERVED;
    pendingAck.packetId = packetId;
    _toSendAcks.push_back(pendingAck);
  } else if (qos == 2) {
    pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBREC;
    pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBREC_RESERVED;
    pendingAck.packetId = packetId;
    _toSendAcks.push_back(pendingAck);

    bool pubRelAwaiting = false;
    for (AsyncMqttClientInternals::PendingPubRel pendingPubRel : _pendingPubRels) {
      if (pendingPubRel.packetId == packetId) {
        pubRelAwaiting = true;
        break;
      }
    }

    if (!pubRelAwaiting) {
      AsyncMqttClientInternals::PendingPubRel pendingPubRel;
      pendingPubRel.packetId = packetId;
      _pendingPubRels.push_back(pendingPubRel);
    }

    _sendAcks();
  }

  _freeCurrentParsedPacket();
}

void AsyncMqttClient::_onPubRel(uint16_t packetId) {
  _freeCurrentParsedPacket();

  AsyncMqttClientInternals::PendingAck pendingAck;
  pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBCOMP;
  pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBCOMP_RESERVED;
  pendingAck.packetId = packetId;
  _toSendAcks.push_back(pendingAck);

  for (size_t i = 0; i < _pendingPubRels.size(); i++) {
    if (_pendingPubRels[i].packetId == packetId) {
      _pendingPubRels.erase(_pendingPubRels.begin() + i);
      _pendingPubRels.shrink_to_fit();
    }
  }

  _sendAcks();
}

void AsyncMqttClient::_onPubAck(uint16_t packetId) {
  _freeCurrentParsedPacket();

  for (auto callback : _onPublishUserCallbacks) callback(packetId);
}

void AsyncMqttClient::_onPubRec(uint16_t packetId) {
  _freeCurrentParsedPacket();

  AsyncMqttClientInternals::PendingAck pendingAck;
  pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBREL;
  pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBREL_RESERVED;
  pendingAck.packetId = packetId;
  _toSendAcks.push_back(pendingAck);

  _sendAcks();
}

void AsyncMqttClient::_onPubComp(uint16_t packetId) {
  _freeCurrentParsedPacket();

  for (auto callback : _onPublishUserCallbacks) callback(packetId);
}

bool AsyncMqttClient::_sendPing() {
  char fixedHeader[2];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.PINGREQ;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.PINGREQ_RESERVED;
  fixedHeader[1] = 0;

  size_t neededSpace = 2;

  SEMAPHORE_TAKE(false);
  if (_client.space() < neededSpace) { SEMAPHORE_GIVE(); return false; }

  _client.add(fixedHeader, 2);
  _client.send();
  _lastClientActivity = millis();
  _lastPingRequestTime = millis();

  SEMAPHORE_GIVE();
  return true;
}

void AsyncMqttClient::_sendAcks() {
  uint8_t neededAckSpace = 2 + 2;

  SEMAPHORE_TAKE();
  for (size_t i = 0; i < _toSendAcks.size(); i++) {
    if (_client.space() < neededAckSpace) break;

    AsyncMqttClientInternals::PendingAck pendingAck = _toSendAcks[i];

    char fixedHeader[2];
    fixedHeader[0] = pendingAck.packetType;
    fixedHeader[0] = fixedHeader[0] << 4;
    fixedHeader[0] = fixedHeader[0] | pendingAck.headerFlag;
    fixedHeader[1] = 2;

    char packetIdBytes[2];
    packetIdBytes[0] = pendingAck.packetId >> 8;
    packetIdBytes[1] = pendingAck.packetId & 0xFF;

    _client.add(fixedHeader, 2);
    _client.add(packetIdBytes, 2);
    _client.send();

    _toSendAcks.erase(_toSendAcks.begin() + i);
    _toSendAcks.shrink_to_fit();

    _lastClientActivity = millis();
  }
  SEMAPHORE_GIVE();
}

bool AsyncMqttClient::_sendDisconnect() {
  if (!_connected) return true;

  const uint8_t neededSpace = 2;

  SEMAPHORE_TAKE(false);

  if (_client.space() < neededSpace) { SEMAPHORE_GIVE(); return false; }

  char fixedHeader[2];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.DISCONNECT;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.DISCONNECT_RESERVED;
  fixedHeader[1] = 0;

  _client.add(fixedHeader, 2);
  _client.send();
  _client.close(true);

  _disconnectFlagged = false;

  SEMAPHORE_GIVE();
  return true;
}

uint16_t AsyncMqttClient::_getNextPacketId() {
  uint16_t nextPacketId = _nextPacketId;

  if (_nextPacketId == 65535) _nextPacketId = 0;  // 0 is forbidden
  _nextPacketId++;

  return nextPacketId;
}

bool AsyncMqttClient::connected() const {
  return _connected;
}

void AsyncMqttClient::connect() {
  if (_connected) return;

#if ASYNC_TCP_SSL_ENABLED
  if (_useIp) {
    _client.connect(_ip, _port, _secure);
  } else {
    _client.connect(_host, _port, _secure);
  }
#else
  if (_useIp) {
    _client.connect(_ip, _port);
  } else {
    _client.connect(_host, _port);
  }
#endif
}

void AsyncMqttClient::disconnect(bool force) {
  if (!_connected) return;

  if (force) {
    _client.close(true);
  } else {
    _disconnectFlagged = true;
    _sendDisconnect();
  }
}

uint16_t AsyncMqttClient::subscribe(const char* topic, uint8_t qos) {
  if (!_connected) return 0;

  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.SUBSCRIBE;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.SUBSCRIBE_RESERVED;

  uint16_t topicLength = strlen(topic);
  char topicLengthBytes[2];
  topicLengthBytes[0] = topicLength >> 8;
  topicLengthBytes[1] = topicLength & 0xFF;

  char qosByte[1];
  qosByte[0] = qos;

  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(2 + 2 + topicLength + 1, fixedHeader + 1);

  size_t neededSpace = 0;
  neededSpace += 1 + remainingLengthLength;
  neededSpace += 2;
  neededSpace += 2;
  neededSpace += topicLength;
  neededSpace += 1;

  SEMAPHORE_TAKE(0);
  if (_client.space() < neededSpace) { SEMAPHORE_GIVE(); return 0; }

  uint16_t packetId = _getNextPacketId();
  char packetIdBytes[2];
  packetIdBytes[0] = packetId >> 8;
  packetIdBytes[1] = packetId & 0xFF;

  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(packetIdBytes, 2);
  _client.add(topicLengthBytes, 2);
  _client.add(topic, topicLength);
  _client.add(qosByte, 1);
  _client.send();
  _lastClientActivity = millis();

  SEMAPHORE_GIVE();
  return packetId;
}

uint16_t AsyncMqttClient::unsubscribe(const char* topic) {
  if (!_connected) return 0;

  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.UNSUBSCRIBE;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.UNSUBSCRIBE_RESERVED;

  uint16_t topicLength = strlen(topic);
  char topicLengthBytes[2];
  topicLengthBytes[0] = topicLength >> 8;
  topicLengthBytes[1] = topicLength & 0xFF;

  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(2 + 2 + topicLength, fixedHeader + 1);

  size_t neededSpace = 0;
  neededSpace += 1 + remainingLengthLength;
  neededSpace += 2;
  neededSpace += 2;
  neededSpace += topicLength;

  SEMAPHORE_TAKE(0);
  if (_client.space() < neededSpace) { SEMAPHORE_GIVE(); return 0; }

  uint16_t packetId = _getNextPacketId();
  char packetIdBytes[2];
  packetIdBytes[0] = packetId >> 8;
  packetIdBytes[1] = packetId & 0xFF;

  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(packetIdBytes, 2);
  _client.add(topicLengthBytes, 2);
  _client.add(topic, topicLength);
  _client.send();
  _lastClientActivity = millis();

  SEMAPHORE_GIVE();
  return packetId;
}

uint16_t AsyncMqttClient::publish(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length, bool dup, uint16_t message_id) {
  if (!_connected) return 0;

  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.PUBLISH;
  fixedHeader[0] = fixedHeader[0] << 4;
  if (dup) fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_DUP;
  if (retain) fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_RETAIN;
  switch (qos) {
    case 0:
      fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_QOS0;
      break;
    case 1:
      fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_QOS1;
      break;
    case 2:
      fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_QOS2;
      break;
  }

  uint16_t topicLength = strlen(topic);
  char topicLengthBytes[2];
  topicLengthBytes[0] = topicLength >> 8;
  topicLengthBytes[1] = topicLength & 0xFF;

  uint32_t payloadLength = length;
  if (payload != nullptr && payloadLength == 0) payloadLength = strlen(payload);

  uint32_t remainingLength = 2 + topicLength + payloadLength;
  if (qos != 0) remainingLength += 2;
  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(remainingLength, fixedHeader + 1);

  size_t neededSpace = 0;
  neededSpace += 1 + remainingLengthLength;
  neededSpace += 2;
  neededSpace += topicLength;
  if (qos != 0) neededSpace += 2;
  if (payload != nullptr) neededSpace += payloadLength;

  SEMAPHORE_TAKE(0);
  if (_client.space() < neededSpace) { SEMAPHORE_GIVE(); return 0; }

  uint16_t packetId = 0;
  char packetIdBytes[2];
  if (qos != 0) {
    if (dup && message_id > 0) {
      packetId = message_id;
    } else {
      packetId = _getNextPacketId();
    }

    packetIdBytes[0] = packetId >> 8;
    packetIdBytes[1] = packetId & 0xFF;
  }

  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(topicLengthBytes, 2);
  _client.add(topic, topicLength);
  if (qos != 0) _client.add(packetIdBytes, 2);
  if (payload != nullptr) _client.add(payload, payloadLength);
  _client.send();
  _lastClientActivity = millis();

  SEMAPHORE_GIVE();
  if (qos != 0) {
    return packetId;
  } else {
    return 1;
  }
}
