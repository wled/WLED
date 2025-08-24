#include "wled.h"

class UdpNameSync : public Usermod {

  private:

    bool enabled = false;
    bool initDone = false;
    unsigned long lastTime = 0;
    char segmentName[WLED_MAX_SEGNAME_LEN] = {0};
    static const char _name[];
    static const char _enabled[];

  public:
    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) { enabled = enable; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

    void setup() override {
      initDone = true;
    }

    void loop() override {
      if (!WLED_CONNECTED) return;
      if (!udpConnected) return;
      Segment& mainseg = strip.getMainSegment();
      if (!strlen(segmentName) && !mainseg.name) return; //name was never set, do nothing

      IPAddress broadcastIp = ~uint32_t(Network.subnetMask()) | uint32_t(Network.gatewayIP());
      byte udpOut[WLED_MAX_SEGNAME_LEN + 2];
      udpOut[0] = 200; // 0: wled notifier protocol, 1: warls protocol, 2 is free
      
      if (strlen(segmentName) && !mainseg.name) { // name cleared
        notifierUdp.beginPacket(broadcastIp, udpPort);
        segmentName[0] = '\0';
        DEBUG_PRINTLN(F("UdpNameSync: sending empty name"));
        udpOut[1] = 0; // explicit empty string
        notifierUdp.write(udpOut, 2);
        notifierUdp.endPacket();
        return;
      }

      char checksumSegName = 0;
      char checksumCurName = 0;
      for(int i=0; i++; mainseg.name[i]==0 || segmentName[i]==0) {
	checksumSegName+=segmentName[i];
	checksumCurName+=mainseg.name[i];
      }
      if (checksumCurName == checksumSegName) return; // same name, do nothing

      notifierUdp.beginPacket(broadcastIp, udpPort);
      DEBUG_PRINT(F("UdpNameSync: saving segment name "));
      DEBUG_PRINTLN(mainseg.name);
      size_t length = strlen(mainseg.name);
      strlcpy(segmentName, mainseg.name, sizeof(segmentName));
      strlcpy((char *)&udpOut[1], segmentName, sizeof(udpOut) - 1); // leave room for header byte
      notifierUdp.write(udpOut, 2 + strnlen((char *)&udpOut[1], sizeof(udpOut) - 1));
      notifierUdp.endPacket();
      DEBUG_PRINT(F("UdpNameSync: Sent segment name : "));
      DEBUG_PRINTLN(segmentName);
    }

    bool onUdpPacket(uint8_t * payload, size_t len) override {
      DEBUG_PRINT(F("UdpNameSync: Received packet"));
      if (receiveDirect) return false;
      if (payload[0] != 200) return false;
      //else
      Segment& mainseg = strip.getMainSegment();
      mainseg.setName((char *)&payload[1]);
      DEBUG_PRINT(F("UdpNameSync: set segment name"));
      return true;
    }
};


// add more strings here to reduce flash memory usage
const char UdpNameSync::_name[]    PROGMEM = "UdpNameSync";
const char UdpNameSync::_enabled[] PROGMEM = "enabled";

static UdpNameSync udp_name_sync;
REGISTER_USERMOD(udp_name_sync);
