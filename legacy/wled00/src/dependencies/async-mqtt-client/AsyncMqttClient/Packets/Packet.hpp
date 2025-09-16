#pragma once

namespace AsyncMqttClientInternals {
class Packet {
 public:
  virtual ~Packet() {}

  virtual void parseVariableHeader(char* data, size_t len, size_t* currentBytePosition) = 0;
  virtual void parsePayload(char* data, size_t len, size_t* currentBytePosition) = 0;
};
}  // namespace AsyncMqttClientInternals
