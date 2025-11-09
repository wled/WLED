#include "wled.h"

// Domyślne piny dla ESP32 DevKit (możesz je zmienić w GUI)
int PIR_TOP_PIN = 12;
int PIR_BOTTOM_PIN = 33;
#define PIR_DEBOUNCE_MS 200UL

class DualPirUsermod : public Usermod {
private:
  bool effectActive = false;          // czy preset jest aktywny (blokada)
  uint8_t blockedBy = 0;              // 0 = nikogo, 1 = top, 2 = bottom
  unsigned long effectStart = 0;
  unsigned long lastPirRead = 0;

public:
  // Konfigurowalne wartości (domyślnie)
  int presetTop = 1;                  // preset dla PIR góra
  int presetBottom = 2;               // preset dla PIR dół
  int presetOff = 0;                  // preset przy wyłączeniu / po zakończeniu efektu
  unsigned long effectDuration = 30000UL; // czas trwania efektu / blokady (ms)

  void setup() {
    // Ustaw piny jako wejścia. Jeśli Twój PIR wymaga pullup/pulldown, zmień INPUT na INPUT_PULLUP / INPUT_PULLDOWN
    pinMode(PIR_TOP_PIN, INPUT);
    pinMode(PIR_BOTTOM_PIN, INPUT);
  }

  void loop() {
    unsigned long now = millis();
    // debounce odczytów PIR
    if (now - lastPirRead < PIR_DEBOUNCE_MS) return;
    lastPirRead = now;

    int topState = digitalRead(PIR_TOP_PIN);
    int bottomState = digitalRead(PIR_BOTTOM_PIN);

    // jeśli brak aktywnego efektu -> reaguj tylko gdy nikt nie blokuje
    if (!effectActive) {
      // jeżeli oba PIR jednocześnie wykrywają ruch, preferujemy "górę"
      if (topState == HIGH && blockedBy == 0) {
        presetLoad(presetTop, true);
        effectActive = true;
        effectStart = now;
        blockedBy = 1; // blokujemy drugi czujnik aż do zakończenia efektu
      } else if (bottomState == HIGH && blockedBy == 0) {
        presetLoad(presetBottom, true);
        effectActive = true;
        effectStart = now;
        blockedBy = 2;
      }
    }

    // zakończenie efektu / odblokowanie po czasie
    if (effectActive && (now - effectStart >= effectDuration)) {
      presetLoad(presetOff, true);
      effectActive = false;
      blockedBy = 0;
    }
  }

  // Zapis ustawień do pliku config.json (widoczne w GUI > Usermods)
  void addToConfig(JsonObject& root) {
    JsonObject top = root.createNestedObject("dualPir");
    top["presetTop"] = presetTop;
    top["presetBottom"] = presetBottom;
    top["presetOff"] = presetOff;
    top["effectDuration"] = effectDuration;
    top["pinTop"] = PIR_TOP_PIN;
    top["pinBottom"] = PIR_BOTTOM_PIN;
  }

  // Odczyt ustawień z config.json przy starcie
  void readFromConfig(JsonObject& root) {
    JsonObject top = root["dualPir"];
    if (top.isNull()) return;
    presetTop = top["presetTop"] | presetTop;
    presetBottom = top["presetBottom"] | presetBottom;
    presetOff = top["presetOff"] | presetOff;
    effectDuration = top["effectDuration"] | effectDuration;

    // Pozwalamy także na zmianę pinów z GUI (jeżeli chcesz ich używać)
    PIR_TOP_PIN = top["pinTop"] | PIR_TOP_PIN;
    PIR_BOTTOM_PIN = top["pinBottom"] | PIR_BOTTOM_PIN;

    // Zaktualizuj konfigurację pinMode (może być wywołane przed setup() w niektórych wersjach WLED,
    // dlatego zabezpieczamy ponownym ustawieniem)
    pinMode(PIR_TOP_PIN, INPUT);
    pinMode(PIR_BOTTOM_PIN, INPUT);
  }

  // Dodaj informacje o stanie do JSON info (Settings -> Info)
  void addToJsonInfo(JsonObject& root) {
    JsonObject user = root.createNestedObject("dualPir");
    user["effectActive"] = effectActive;
    user["blockedBy"] = blockedBy; // 0/1/2
    user["presetTop"] = presetTop;
    user["presetBottom"] = presetBottom;
    user["presetOff"] = presetOff;
    user["effectDuration_ms"] = effectDuration;
    user["pinTop"] = PIR_TOP_PIN;
    user["pinBottom"] = PIR_BOTTOM_PIN;
  }

  // Opcjonalnie zwracamy ID usermodu (unikatowy)
  uint16_t getId() {
    return 1234;
  }
};

// rejestracja usermodu
DualPirUsermod dualPirUsermod;