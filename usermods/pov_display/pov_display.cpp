#include "wled.h"
#include "pov.h"

static const char _data_FX_MODE_POV_IMAGE[] PROGMEM = "POV Image@!;;;;";

POV pov;

uint16_t mode_pov_image(void) {
  Segment& mainseg = strip.getMainSegment();
  if (!mainseg.name || !strstr(mainseg.name, ".bmp")) {
    return FRAMETIME;
  }
  if ( 0 == strcmp(mainseg.name, pov.getFilename()) ) {
    pov.showNextLine();
    return FRAMETIME;
  }
  pov.loadImage(mainseg.name);
  return FRAMETIME;
}

class PovDisplayUsermod : public Usermod {
protected:
  bool enabled = false; //WLEDMM
  const char *_name; //WLEDMM
  bool initDone = false; //WLEDMM
  unsigned long lastTime = 0; //WLEDMM
public:

  PovDisplayUsermod(const char *name, bool enabled) {
    this->_name=name;
    this->enabled = enabled;
  }
  
  void setup() override {
    strip.addEffect(255, &mode_pov_image, _data_FX_MODE_POV_IMAGE);
    initDone=true;
  }


  void loop() override {
    // if usermod is disabled or called during strip updating just exit
    // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
    if (!enabled || strip.isUpdating()) return;

    // do your magic here
    if (millis() - lastTime > 1000) {
      //Serial.println("I'm alive!");
      lastTime = millis();
    }
  }

  bool onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) override {
    return false;
  }

  uint16_t getId() override {
    return USERMOD_ID_POV_DISPLAY;
  }
};

static PovDisplayUsermod pov_display("POV Display", false);
REGISTER_USERMOD(pov_display);
