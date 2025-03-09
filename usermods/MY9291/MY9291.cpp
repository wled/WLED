#include "wled.h"
#include "MY92xx.h"

#define MY92XX_MODEL        MY92XX_MODEL_MY9291
#define MY92XX_CHIPS        1
#define MY92XX_DI_PIN       13
#define MY92XX_DCKI_PIN     15

#define MY92XX_RED          0
#define MY92XX_GREEN        1
#define MY92XX_BLUE         2
#define MY92XX_WHITE        3

class MY9291Usermod : public Usermod {
  private:
    my92xx _my92xx = my92xx(MY92XX_MODEL, MY92XX_CHIPS, MY92XX_DI_PIN, MY92XX_DCKI_PIN, MY92XX_COMMAND_DEFAULT);

  public:

    void setup() {
      _my92xx.setState(true);
    }

    void connected() {
    }

    void loop() {
      uint32_t c = strip.getPixelColor(0);
      int w = ((c >> 24) & 0xff) * bri / 255.0;
      int r = ((c >> 16) & 0xff) * bri / 255.0;
      int g = ((c >> 8) & 0xff) * bri / 255.0;
      int b = (c & 0xff) * bri / 255.0;
      _my92xx.setChannel(MY92XX_RED, r);
      _my92xx.setChannel(MY92XX_GREEN, g);
      _my92xx.setChannel(MY92XX_BLUE, b);
      _my92xx.setChannel(MY92XX_WHITE, w);
      _my92xx.update();
    }

    uint16_t getId() {
      return USERMOD_ID_MY9291;
    }
};

static MY9291Usermod my9291;
REGISTER_USERMOD(my9291);