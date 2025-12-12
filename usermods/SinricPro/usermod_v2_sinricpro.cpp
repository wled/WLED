#include "wled.h"

#ifdef WLED_ENABLE_SINRICPRO

#include <SinricPro.h>
#include <SinricProDimSwitch.h>

class SinricProUsermod : public Usermod {
  private:
    SinricProDimSwitch* dimSwitch = nullptr;
    bool initDone = false;

    // callback for power on/off from SinricPro
    bool onPowerState(const String &deviceId, bool &state) {
      if (state) {
        if (bri == 0) {
          bri = briLast ? briLast : 128;
          stateChanged = true;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }
      } else {
        if (bri > 0) {
          briLast = bri;
          bri = 0;
          stateChanged = true;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }
      }
      return true; // acknowledge success to SinricPro
    }

    // callback for brightness level (0-100) from SinricPro
    bool onPowerLevel(const String &deviceId, int &level) {
      if (level < 0) level = 0;
      if (level > 100) level = 100;
      byte newBri = (byte)map(level, 0, 100, 0, 255);
      bri = newBri;
      stateChanged = true;
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
      return true;
    }

  public:
    void setup() override {
      // nothing to do at early boot
    }

    void connected() override {
      if (initDone) return;

      Serial.begin(BAUD_RATE);

      // initialize SinricPro
      SinricPro.begin(APP_KEY, APP_SECRET);

      // create and register DimSwitch device
      static SinricProDimSwitch myDim(DIMSWITCH_ID);
      dimSwitch = &myDim;
      dimSwitch->onPowerState([this](const String &deviceId, bool &state){ return this->onPowerState(deviceId, state); });
      dimSwitch->onPowerLevel([this](const String &deviceId, int &level){ return this->onPowerLevel(deviceId, level); });

      SinricPro.add(dimSwitch);

      initDone = true;
    }

    void loop() override {
      if (!initDone) return;
      SinricPro.handle();
    }

    // notify SinricPro of WLED state changes (simple two-way sync)
    void onStateChange(uint8_t mode) override {
      if (!initDone || dimSwitch == nullptr) return;
      // send power state and level back to SinricPro
      bool on = (bri > 0);
      int level = map(bri, 0, 255, 0, 100);
      // The SinricPro library exposes send methods on the device object
      // Send current state back to SinricPro (best-effort)
      dimSwitch->sendPowerState(DIMSWITCH_ID, on);
      dimSwitch->sendPowerLevel(DIMSWITCH_ID, level);
    }

    uint16_t getId() override { return USERMOD_ID_EXAMPLE + 1; }
};

// register the usermod (the build system will pick up the folder when added to custom_usermods)
static SinricProUsermod sinricProUsermod;

#endif // WLED_ENABLE_SINRICPRO
