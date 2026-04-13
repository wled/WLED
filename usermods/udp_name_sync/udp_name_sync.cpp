#include "wled.h"

class UdpNameSync : public Usermod {

  private:

    bool enabled = false;
    char segmentName[WLED_MAX_SEGNAME_LEN] = {0};
    static constexpr uint8_t kPacketType = 200; // custom usermod packet type
    static const char _name[];
    static const char _enabled[];

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
      if (segmentName[0] == '\0' && !mainseg.name) return; //name was never set, do nothing

      const char* curName = mainseg.name ? mainseg.name : "";
      if (strncmp(curName, segmentName, sizeof(segmentName)) == 0) return; // same name, do nothing

      IPAddress broadcastIp = uint32_t(Network.localIP()) | ~uint32_t(Network.subnetMask());
      byte udpOut[WLED_MAX_SEGNAME_LEN + 2];
      udpOut[0] = kPacketType; // custom usermod packet type (avoid 0..5 used by core protocols)

      if (segmentName[0] != '\0' && !mainseg.name) { // name cleared
        notifierUdp.beginPacket(broadcastIp, udpPort);
        segmentName[0] = '\0';
        DEBUG_PRINTLN(F("UdpNameSync: sending empty name"));
        udpOut[1] = 0; // explicit empty string
        notifierUdp.write(udpOut, 2);
        notifierUdp.endPacket();
        return;
      }

      notifierUdp.beginPacket(broadcastIp, udpPort);
      DEBUG_PRINT(F("UdpNameSync: saving segment name "));
      DEBUG_PRINTLN(curName);
      strlcpy(segmentName, curName, sizeof(segmentName));
      strlcpy((char *)&udpOut[1], segmentName, sizeof(udpOut) - 1); // leave room for header byte
      size_t nameLen = strnlen((char *)&udpOut[1], sizeof(udpOut) - 1);
      notifierUdp.write(udpOut, 2 + nameLen);
      notifierUdp.endPacket();
      DEBUG_PRINT(F("UdpNameSync: Sent segment name : "));
      DEBUG_PRINTLN(segmentName);
      return;
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

static UdpNameSync udp_name_sync;
REGISTER_USERMOD(udp_name_sync);
