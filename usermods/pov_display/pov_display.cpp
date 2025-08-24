#include "wled.h"
#include "pov.h"

static const char _data_FX_MODE_POV_IMAGE[] PROGMEM = "POV Image@!;;;;";

POV pov;

uint16_t mode_pov_image(void) {
  Segment& mainseg = strip.getMainSegment();
  const char* segName = mainseg.name;
  if (!segName) {
     return FRAMETIME;
   }
  // Only proceed for files ending with .bmp (case-insensitive)
  size_t segLen = strlen(segName);
  if (segLen < 4) return FRAMETIME;
  const char* ext = segName + (segLen - 4);
  // compare case-insensitive to ".bmp"
  if (!((ext[0]=='.') &&
        (ext[1]=='b' || ext[1]=='B') &&
        (ext[2]=='m' || ext[2]=='M') &&
        (ext[3]=='p' || ext[3]=='P'))) {
    return FRAMETIME;
  }

  const char* current = pov.getFilename();
  if (current && strcmp(segName, current) == 0) {
     pov.showNextLine();
     return FRAMETIME;
   }

  pov.loadImage(segName);
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
