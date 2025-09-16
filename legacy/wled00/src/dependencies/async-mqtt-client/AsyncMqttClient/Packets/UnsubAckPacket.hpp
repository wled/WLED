#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class UnsubAckPacket : public Packet {
 public:
  explicit UnsubAckPacket(ParsingInformation* parsingInformation, OnUnsubAckInternalCallback callback);
  ~UnsubAckPacket();

  void parseVariableHeader(char* data, size_t len, size_t* currentBytePosition);
  void parsePayload(char* data, size_t len, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnUnsubAckInternalCallback _callback;

  uint8_t _bytePosition;
  char _packetIdMsb;
  uint16_t _packetId;
};
}  // namespace AsyncMqttClientInternals
