#pragma once

namespace AsyncMqttClientInternals {
struct PendingPubRel {
  uint16_t packetId;
};

struct PendingAck {
  uint8_t packetType;
  uint8_t headerFlag;
  uint16_t packetId;
};
}  // namespace AsyncMqttClientInternals
