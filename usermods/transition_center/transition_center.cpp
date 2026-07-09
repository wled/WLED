#include "wled.h"

/*
 * Exposes `blendingCenterPct` (center of the Inside-out/Outside-in transition
 * styles, 0-100% of segment width) as a usermod setting.
 */
class TransitionCenterUsermod : public Usermod {
  private:
    static const char _name[];

  public:
    void setup() override {}
    void loop() override {}

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("center_pct")] = blendingCenterPct;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;
      int v = blendingCenterPct;
      bool ok = getJsonValue(top[F("center_pct")], v, 50);
      blendingCenterPct = constrain(v, 0, 100);
      return ok;
    }

    void appendConfigData() override {
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":center_pct"));
      oappend(F("',1,'% of strip length; 50 = middle, 0 = start, 100 = end');"));
    }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char TransitionCenterUsermod::_name[] PROGMEM = "TransitionCenter";

static TransitionCenterUsermod transition_center_um;
REGISTER_USERMOD(transition_center_um);
