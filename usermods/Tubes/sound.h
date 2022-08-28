
#include "wled.h"
#include "fcn_declare.h"
#include "particle.h"


class Sounder {
  public:
    bool active = true;
    bool overlay = false;
    uint8_t volume;

    void setup() {
    }

    void update() {
        if (!this->active) {
            particleVolume = DEFAULT_PARTICLE_VOLUME; // Average volume
            return;
        }

        um_data_t *um_data;
        if (!usermods.getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
            this->active = false;
            this->overlay = false;
            return;
        }

        float volumeSmth = *(float*)um_data->u_data[0];
        this->volume = constrain(volumeSmth, 0, 255);     // Keep the sample from overflowing.
        particleVolume = this->volume;
    }

    void handleOverlayDraw() {
        if (!this->active)
            return; 
        if (!this->overlay)
            return;

        int len = scale8(this->volume, 32);

        Segment& segment = strip.getMainSegment();

        for (int i = 0; i < len; i++) {
            uint32_t color = segment.color_from_palette(i, true, true, 255, 192);
            strip.setPixelColor(i, color);
        }
    }

    static float mapf(float x, float in_min, float in_max, float out_min, float out_max){
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

};
