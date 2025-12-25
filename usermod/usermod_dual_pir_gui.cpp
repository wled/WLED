#include "wled.h"

#define PIR_TOP_PIN 12
#define PIR_BOTTOM_PIN 33
#define PIR_DEBOUNCE_MS 200

class DualPirUsermod : public Usermod {
private:
  bool effectActive = false;
  unsigned long effectStart = 0;
  unsigned long lastPirRead = 0;

public:
  // Konfigurowalne w GUI (Custom Settings)
  int presetTop = 1;          // preset dla PIR1 (góra)
  int presetBottom = 2;       // preset dla PIR2 (dół)
  int presetOff = 0;          // preset wyłączający taśmę
  unsigned long effectDuration = 30000; // czas trwania efektu / blokady w ms

  void setup() {
    pinMode(PIR_TOP_PIN, INPUT);
    pinMode(PIR_BOTTOM_PIN, INPUT);
  }

  void loop() {
    unsigned long now = millis();
    if (now - lastPirRead < PIR_DEBOUNCE_MS) return;
    lastPirRead = now;

    int topState = digitalRead(PIR_TOP_PIN);
    int bottomState = digitalRead(PIR_BOTTOM_PIN);

    if (!effectActive) {
      if (topState == HIGH) {
        presetLoad(presetTop, true);
        effectActive = true;
        effectStart = now;
      } else if (bottomState == HIGH) {
        presetLoad(presetBottom, true);
        effectActive = true;
        effectStart = now;
      }
    }

    if (effectActive && now - effectStart >= effectDuration) {
      presetLoad(presetOff, true);
      effectActive = false;
    }
  }
};

// rejestracja usermodu
DualPirUsermod dualPirUsermod;
