#include "wled.h"
#include "pov.h"

static const char _data_FX_MODE_POV_IMAGE[] PROGMEM = "POV Image@!;;;;";

static POV s_pov;

void mode_pov_image(void) {
  // This effect displays columns from a BMP image for horizontal POV
  // All logic is handled here to ensure it only runs when this effect is selected
  Segment& mainseg = strip.getMainSegment();
  const char* segName = mainseg.name;
  if (!segName) {
    return;
  }
  
  // Only proceed for files ending with .bmp (case-insensitive)
  size_t segLen = strlen(segName);
  if (segLen < 4) return;
  const char* ext = segName + (segLen - 4);
  
  if ((ext[0] == '.') &&
      (ext[1] == 'b' || ext[1] == 'B') &&
      (ext[2] == 'm' || ext[2] == 'M') &&
      (ext[3] == 'p' || ext[3] == 'P')) {
    
    const char* current = s_pov.getFilename();
    
    // If image is already loaded and matches, show next column
    if (current && strcmp(segName, current) == 0) {
      s_pov.showNextColumn();
      return;
    }
    
    // Image is loaded but doesn't match, or not loaded yet
    // If we have a different image loaded, keep displaying it while trying the new one
    if (current) {
      s_pov.showNextColumn();
    }
    
    // Try to load the new image (rate limited)
    static unsigned long s_lastLoadAttemptMs = 0;
    unsigned long nowMs = millis();
    // Try to load at most twice per second
    if (nowMs - s_lastLoadAttemptMs >= 500) {
      s_lastLoadAttemptMs = nowMs;
      if (s_pov.loadImage(segName)) {
        // Successfully loaded, show first column
        s_pov.showNextColumn();
      }
      // If load fails, we'll keep displaying old image and retry on next call
    }
  }
}

class PovDisplayUsermod : public Usermod {
protected:
  bool enabled = false; //WLEDMM
  const char *_name; //WLEDMM
public:

  PovDisplayUsermod(const char *name, bool enabled)
    : enabled(enabled) , _name(name) {}
  
  void setup() override {
    strip.addEffect(255, &mode_pov_image, _data_FX_MODE_POV_IMAGE);
  }

  void loop() override {
  }

  uint16_t getId() override {
    return USERMOD_ID_POV_DISPLAY;
  }
};

static PovDisplayUsermod pov_display("POV Display", false);
REGISTER_USERMOD(pov_display);
