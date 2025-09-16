#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class PingRespPacket : public Packet {
 public:
  explicit PingRespPacket(ParsingInformation* parsingInformation, OnPingRespInternalCallback callback);
  ~PingRespPacket();

  void parseVariableHeader(char* data, size_t len, size_t* currentBytePosition);
  void parsePayload(char* data, size_t len, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnPingRespInternalCallback _callback;
};
}  // namespace AsyncMqttClientInternals
