#pragma once

namespace AsyncMqttClientInternals {
enum class BufferState : uint8_t {
  NONE = 0,
  REMAINING_LENGTH = 2,
  VARIABLE_HEADER = 3,
  PAYLOAD = 4
};

struct ParsingInformation {
  BufferState bufferState;

  uint16_t maxTopicLength;
  char* topicBuffer;

  uint8_t packetType;
  uint16_t packetFlags;
  uint32_t remainingLength;
};
}  // namespace AsyncMqttClientInternals
