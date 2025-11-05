#include "wled.h"

#define PIR_DEBOUNCE_MS 200UL

class DualPirUsermod : public Usermod {
private:
  int pinTop = 13;            // domyślny pin dla czujnika góra (zmieniono z 12)
  int pinBottom = 33;         // domyślny pin dla czujnika dół
  bool effectActive = false;  // czy efekt jest aktywny (blokada)
  uint8_t blockedBy = 0;      // 0 = brak, 1 = top, 2 = bottom
  unsigned long effectStart = 0;

  unsigned long lastPirReadTop = 0;
  unsigned long lastPirReadBottom = 0;

public:
  // Konfigurowalne wartości (domyślne)
  int presetTop = 1;
  int presetBottom = 2;
  int presetOff = 0;
  unsigned long effectDuration = 30000UL; // ms

  // zachowanie
  bool extendOnRetrigger = true; // jeśli ten sam sensor wykryje ruch, przedłuż efekt
  bool allowOverride = false;     // czy drugi sensor może nadpisać aktywny efekt
  bool useInputPullup = false;    // użyj INPUT_PULLUP zamiast INPUT jeśli trzeba

  void setup() {
    pinMode(pinTop, useInputPullup ? INPUT_PULLUP : INPUT);
    pinMode(pinBottom, useInputPullup ? INPUT_PULLUP : INPUT);
  }

  void loop() {
    unsigned long now = millis();

    int topState = digitalRead(pinTop);
    int bottomState = digitalRead(pinBottom);

    // Debounce dla top
    if (now - lastPirReadTop >= PIR_DEBOUNCE_MS) {
      if (topState == HIGH) {
        lastPirReadTop = now;
        handleTrigger(1, now);
      }
    }

    // Debounce dla bottom
    if (now - lastPirReadBottom >= PIR_DEBOUNCE_MS) {
      if (bottomState == HIGH) {
        lastPirReadBottom = now;
        handleTrigger(2, now);
      }
    }

    // sprawdź czy czas efektu minął
    if (effectActive && (now - effectStart >= effectDuration)) {
      presetLoad(presetOff, true);
      effectActive = false;
      blockedBy = 0;
    }
  }

  void handleTrigger(uint8_t sensorId, unsigned long now) {
    if (!effectActive) {
      // uruchom preset odpowiedni dla sensora
      if (sensorId == 1) presetLoad(presetTop, true);
      else presetLoad(presetBottom, true);
      effectActive = true;
      effectStart = now;
      blockedBy = sensorId;
      return;
    }

    // jeśli efekt już aktywny
    if (extendOnRetrigger && blockedBy == sensorId) {
      // przedłuż efekt (zresetuj licznik)
      effectStart = now;
      return;
    }

    if (allowOverride && blockedBy != sensorId) {
      // inny sensor nadpisuje aktualny efekt
      if (sensorId == 1) presetLoad(presetTop, true);
      else presetLoad(presetBottom, true);
      effectStart = now;
      blockedBy = sensorId;
      return;
    }

    // w przeciwnym razie ignorujemy trigger
  }

  // Zapis ustawień do config.json (Settings -> Usermods)
  void addToConfig(JsonObject& root) {
    JsonObject top = root.createNestedObject("dualPir");
    top["presetTop"] = presetTop;
    top["presetBottom"] = presetBottom;
    top["presetOff"] = presetOff;
    top["effectDuration"] = effectDuration;
    top["pinTop"] = pinTop;
    top["pinBottom"] = pinBottom;
    top["extendOnRetrigger"] = extendOnRetrigger;
    top["allowOverride"] = allowOverride;
    top["useInputPullup"] = useInputPullup;
  }

  // Odczyt ustawień z config.json przy starcie
  void readFromConfig(JsonObject& root) {
    if (!root.containsKey("dualPir")) return;
    JsonObject top = root["dualPir"];
    if (top.isNull()) return;

    presetTop = top["presetTop"] | presetTop;
    presetBottom = top["presetBottom"] | presetBottom;
    presetOff = top["presetOff"] | presetOff;
    effectDuration = top["effectDuration"] | effectDuration;

    pinTop = top["pinTop"] | pinTop;
    pinBottom = top["pinBottom"] | pinBottom;

    extendOnRetrigger = top["extendOnRetrigger"] | extendOnRetrigger;
    allowOverride = top["allowOverride"] | allowOverride;
    useInputPullup = top["useInputPullup"] | useInputPullup;

    // Ustaw piny ponownie (readFromConfig może być wywołane przed setup())
    pinMode(pinTop, useInputPullup ? INPUT_PULLUP : INPUT);
    pinMode(pinBottom, useInputPullup ? INPUT_PULLUP : INPUT);
  }

  // Dodaj informacje do Settings -> Info
  void addToJsonInfo(JsonObject& root) {
    JsonObject user = root.createNestedObject("dualPir");
    user["effectActive"] = effectActive;
    user["blockedBy"] = blockedBy;
    user["presetTop"] = presetTop;
    user["presetBottom"] = presetBottom;
    user["presetOff"] = presetOff;
    user["effectDuration_ms"] = effectDuration;
    user["pinTop"] = pinTop;
    user["pinBottom"] = pinBottom;
    user["extendOnRetrigger"] = extendOnRetrigger;
    user["allowOverride"] = allowOverride;
    user["useInputPullup"] = useInputPullup;
  }

  // Unikalne ID usermodu
  uint16_t getId() {
    return 4321;
  }
};

// rejestracja usermodu
DualPirUsermod dualPirUsermod;
