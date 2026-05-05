#include "wled.h"

class BootLorUsermod : public Usermod {
private:
  static constexpr const char* _name = "boot_lor";
  
  int8_t bootLor = 2;                 // -1 disabled, 0/1/2 valid lor values
  bool waitForUdpPacket = true;       // wait for first UDP packet before starting delay
  uint8_t additionalWaitSec = 0;      // seconds after connection or UDP packet
  uint16_t assertForSec = 10;         // reassert window after first apply
  
  unsigned long referenceMs = 0;
  unsigned long firstAppliedMs = 0;
  
  bool referenceSet = false;
  bool applied = false;
  bool finished = false;
  
  bool isValidLor(int8_t value) const {
    return value >= -1 && value <= 2;
  }
  
  bool isEnabled() const {
    return isValidLor(bootLor) && bootLor >= 0;
  }
  
  void setReferenceTime(unsigned long now) {
    referenceMs = now;
    referenceSet = true;
  }
  
  bool additionalWaitElapsed() const {
    const unsigned long waitMs = (unsigned long)additionalWaitSec * 1000UL;
    return millis() - referenceMs >= waitMs;
  }
  
  bool readyToApply() const {
    if (!isEnabled() || finished || !referenceSet) return false;
    if (!WLED_CONNECTED) return false;
    
    return additionalWaitElapsed();
  }
  
  void applyBootLor() {
    if (realtimeOverride != bootLor) {
      realtimeOverride = bootLor;
    }
    
    if (!applied) {
      applied = true;
      firstAppliedMs = millis();
    }
  }
  
  void runIfReady() {
    if (!readyToApply()) return;
    
    applyBootLor();
    
    if (millis() - firstAppliedMs > (unsigned long)assertForSec * 1000UL) {
      finished = true;
    }
  }
  
public:
  void setup() override {
  }
  
  void connected() override {
    if (!waitForUdpPacket && !referenceSet) {
      setReferenceTime(millis());
    }
  }
  
  void loop() override {
    runIfReady();
  }
  
  bool onUdpPacket(uint8_t* payload, size_t len) override {
    if (waitForUdpPacket && !referenceSet && WLED_CONNECTED) {
      setReferenceTime(millis());
    }
    
    runIfReady();
    return false;
  }
  
  void addToConfig(JsonObject& root) override {
    JsonObject top = root[_name];
    if (top.isNull()) top = root.createNestedObject(_name);
    
    top["bootLor"] = bootLor;
    top["waitForUdpPacket"] = waitForUdpPacket;
    top["additionalWaitSec"] = additionalWaitSec;
    top["assertForSec"] = assertForSec;
  }
  
  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[_name];
    if (top.isNull()) return false;
    
    int8_t newBootLor = top["bootLor"] | bootLor;
    if (isValidLor(newBootLor)) bootLor = newBootLor;
    
    waitForUdpPacket = top["waitForUdpPacket"] | waitForUdpPacket;
    additionalWaitSec = top["additionalWaitSec"] | additionalWaitSec;
    assertForSec = top["assertForSec"] | assertForSec;
    
    return true;
  }
  
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    
    JsonArray infoArr = user.createNestedArray("Boot LOR");
    infoArr.add(bootLor);
    infoArr.add(finished ? "finished" : applied ? "applied" : "waiting");
    infoArr.add(realtimeOverride);
  }
  
  uint16_t getId() override {
    return USERMOD_ID_BOOT_LOR;
  }
};

static BootLorUsermod boot_lor_usermod;
REGISTER_USERMOD(boot_lor_usermod);
