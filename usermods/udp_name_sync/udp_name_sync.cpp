#include "wled.h"

class UdpNameSync : public Usermod {

  private:

    bool enabled = false;
    char segmentName[WLED_MAX_SEGNAME_LEN] = {0};
    static constexpr uint8_t kPacketType = 200; // custom usermod packet type
    static const char _name[];
    static const char _enabled[];
    
    // Retry mechanism variables (similar to core UDP sync)
    unsigned long _lastNameSentTime = 0;
    uint8_t _nameSendCount = 0;
    bool _nameNeedsSync = false;

  public:
    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool value) { enabled = value; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() const { return enabled; }

    void setup() override {
      // Enabled when this usermod is compiled, set to false if you prefer runtime opt-in
      enable(true);
    }

    void loop() override {
      if (!enabled) return;
      if (!WLED_CONNECTED) return;
      if (!udpConnected) return;
      
      Segment& mainseg = strip.getMainSegment();
      // Early return only if name was never set and no retry is pending
      // This allows empty-name packets to reach the retry branch when _nameNeedsSync is set
      if (segmentName[0] == '\0' && !mainseg.name && !_nameNeedsSync) return; //name was never set, do nothing

      const char* curName = mainseg.name ? mainseg.name : "";
      
      // Check for name change first - this takes priority over retries
      if (strncmp(curName, segmentName, sizeof(segmentName)) != 0) {
        // Name changed, send new name (initial send, reset retry counter)
        sendNamePacket(false);
        return;
      }
      
      // Name hasn't changed - check if we need to retry
      if (_nameNeedsSync && udpConnected && (_nameSendCount < udpNumRetries) && ((millis() - _lastNameSentTime) > 250)) {
        sendNamePacket(true); // retry
        return;
      }
      
      // If we were waiting for retries but they're now complete, clear the flag
      if (_nameNeedsSync && (_nameSendCount >= udpNumRetries || !udpConnected)) {
        _nameNeedsSync = false;
      }
      
      // Name is in sync, no action needed
      return;
    }
    
    void sendNamePacket(bool isRetry) {
      IPAddress broadcastIp = uint32_t(WLEDNetwork.localIP()) | ~uint32_t(WLEDNetwork.subnetMask());
      byte udpOut[WLED_MAX_SEGNAME_LEN + 2];
      udpOut[0] = kPacketType; // custom usermod packet type (avoid 0..5 used by core protocols)
      
      Segment& mainseg = strip.getMainSegment();
      const char* curName = mainseg.name ? mainseg.name : "";
      
      if (segmentName[0] != '\0' && !mainseg.name) { // name cleared
        strlcpy(segmentName, "", sizeof(segmentName));
        if (!isRetry) DEBUG_PRINTLN(F("UdpNameSync: sending empty name"));
        udpOut[1] = 0; // explicit empty string
        notifierUdp.beginPacket(broadcastIp, udpPort);
        notifierUdp.write(udpOut, 2);
        notifierUdp.endPacket();
      } else {
        strlcpy(segmentName, curName, sizeof(segmentName));
        strlcpy((char *)&udpOut[1], segmentName, sizeof(udpOut) - 1); // leave room for header byte
        size_t nameLen = strnlen((char *)&udpOut[1], sizeof(udpOut) - 1);
        notifierUdp.beginPacket(broadcastIp, udpPort);
        notifierUdp.write(udpOut, 2 + nameLen);
        notifierUdp.endPacket();
        if (!isRetry) {
          DEBUG_PRINT(F("UdpNameSync: Sent segment name : "));
          DEBUG_PRINTLN(segmentName);
        }
      }
      
      // Update retry tracking (match core behavior)
      _lastNameSentTime = millis();
      _nameSendCount = isRetry ? _nameSendCount + 1 : 0;
      _nameNeedsSync = true;
    }

    bool onUdpPacket(uint8_t * payload, size_t len) override {
      DEBUG_PRINT(F("UdpNameSync: Received packet"));
      if (!enabled) return false;
      if (receiveDirect) return false;
      if (len < 2) return false;                 // need type + at least 1 byte for name (can be 0)
      if (payload[0] != kPacketType) return false;
      Segment& mainseg = strip.getMainSegment();
      char tmp[WLED_MAX_SEGNAME_LEN] = {0};
      size_t copyLen = len - 1;
      if (copyLen > sizeof(tmp) - 1) copyLen = sizeof(tmp) - 1;
      memcpy(tmp, &payload[1], copyLen);
      tmp[copyLen] = '\0';
      mainseg.setName(tmp);
      DEBUG_PRINT(F("UdpNameSync: set segment name"));
      return true;
     }
};

// Static member definitions
const char UdpNameSync::_name[] PROGMEM = "UDP Name Sync";
const char UdpNameSync::_enabled[] PROGMEM = "enabled";

static UdpNameSync udp_name_sync;
REGISTER_USERMOD(udp_name_sync);
