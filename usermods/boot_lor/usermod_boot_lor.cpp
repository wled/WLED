#include "wled.h"

/**
 * @brief Applies a configured WLED realtime override mode during startup.
 *
 * The usermod waits for network connectivity, waits an optional additional
 * delay, then applies the configured realtime override value and reasserts it
 * for a short settling period.
 */
class BootLorUsermod : public Usermod {
private:
  static constexpr const char* _name = "boot_lor"; ///< JSON configuration key for this usermod.
  
  int8_t bootLor = 2;                ///< Realtime override mode to apply; -1 disables the usermod.
  uint8_t additionalWaitSec = 0;     ///< Additional delay, in seconds, after network connection.
  uint16_t assertForSec = 10;        ///< Duration, in seconds, to reassert the configured override.
  
  unsigned long connectedMs = 0;     ///< Timestamp when network connectivity became available.
  unsigned long firstAppliedMs = 0;  ///< Timestamp of the first successful realtime override application.
  
  bool connectedSeen = false;        ///< True once the network connection timestamp has been captured.
  bool applied = false;              ///< True once the configured realtime override has been applied.
  bool finished = false;             ///< True once the assertion window has completed.
  
  /**
   * @brief Checks whether a realtime override value is valid.
   *
   * @param value Realtime override value to validate.
   * @return true if the value is -1, 0, 1, or 2.
   */
  bool isValidLor(int8_t value) const {
    return value >= -1 && value <= 2;
  }
  
  /**
   * @brief Checks whether this usermod should run.
   *
   * @return true when the configured realtime override is valid and not disabled.
   */
  bool isEnabled() const {
    return isValidLor(bootLor) && bootLor >= 0;
  }
  
  /**
   * @brief Stores the timestamp used to start the post-connection wait period.
   *
   * @param now Current millis() timestamp.
   */
  void setConnectedTime(unsigned long now) {
    connectedMs = now;
    connectedSeen = true;
  }
  
  /**
   * @brief Checks whether the configured post-connection wait period has elapsed.
   *
   * @return true once enough time has passed since network connection.
   */
  bool additionalWaitElapsed() const {
    const unsigned long waitMs = (unsigned long)additionalWaitSec * 1000UL;
    return millis() - connectedMs >= waitMs;
  }
  
  /**
   * @brief Checks whether all conditions are met to apply the realtime override.
   *
   * @return true when the usermod is enabled, connected, and past its wait period.
   */
  bool readyToApply() const {
    if (!isEnabled() || finished || !connectedSeen) return false;
    if (!WLED_CONNECTED) return false;
    
    return additionalWaitElapsed();
  }
  
  /**
   * @brief Applies the configured realtime override and records first application time.
   */
  void applyBootLor() {
    if (realtimeOverride != bootLor) {
      realtimeOverride = bootLor;
    }
    
    if (!applied) {
      applied = true;
      firstAppliedMs = millis();
    }
  }
  
  /**
   * @brief Applies and reasserts the configured realtime override when ready.
   */
  void runIfReady() {
    if (!readyToApply()) return;
    
    applyBootLor();
    
    if (millis() - firstAppliedMs > (unsigned long)assertForSec * 1000UL) {
      finished = true;
    }
  }
  
public:
  /**
   * @brief Initializes the usermod.
   */
  void setup() override {
  }
  
  /**
   * @brief Starts the wait timer when networking becomes available.
   */
  void connected() override {
    if (!connectedSeen) {
      setConnectedTime(millis());
    }
  }
  
  /**
   * @brief Runs the non-blocking assertion state machine.
   */
  void loop() override {
    runIfReady();
  }
  
  /**
   * @brief Adds this usermod's settings to the WLED configuration JSON.
   *
   * @param root Root configuration JSON object.
   */
  void addToConfig(JsonObject& root) override {
    JsonObject top = root[_name];
    if (top.isNull()) top = root.createNestedObject(_name);
    
    top["bootLor"] = bootLor;
    top["additionalWaitSec"] = additionalWaitSec;
    top["assertForSec"] = assertForSec;
  }
  
  /**
   * @brief Reads this usermod's settings from the WLED configuration JSON.
   *
   * @param root Root configuration JSON object.
   * @return true if this usermod's configuration object exists.
   */
  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[_name];
    if (top.isNull()) return false;
    
    int8_t newBootLor = top["bootLor"] | bootLor;
    if (isValidLor(newBootLor)) bootLor = newBootLor;
    
    additionalWaitSec = top["additionalWaitSec"] | additionalWaitSec;
    assertForSec = top["assertForSec"] | assertForSec;
    
    return true;
  }
  
  /**
   * @brief Adds runtime status information to the WLED info JSON.
   *
   * @param root Root info JSON object.
   */
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    
    JsonArray infoArr = user.createNestedArray("Boot LOR");
    infoArr.add(bootLor);
    infoArr.add(finished ? "finished" : applied ? "applied" : "waiting");
    infoArr.add(realtimeOverride);
  }
  
  /**
   * @brief Returns the registered usermod identifier.
   *
   * @return USERMOD_ID_BOOT_LOR.
   */
  uint16_t getId() override {
    return USERMOD_ID_BOOT_LOR;
  }
};

static BootLorUsermod boot_lor_usermod;
REGISTER_USERMOD(boot_lor_usermod);
