#include "wled.h"
#include "Arduino.h"

#include <deque>

#define USERMOD_ID_BUZZER 900
#ifndef USERMOD_BUZZER_PIN
#ifdef GPIO_NUM_32
#define USERMOD_BUZZER_PIN GPIO_NUM_32
#else
#define USERMOD_BUZZER_PIN 21
#endif
#endif

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/WLED-dev/WLED/wiki/Add-own-functionality
 * 
 */

class BuzzerUsermod : public Usermod {
  private:
    unsigned long lastTime_ = 0;
    unsigned long delay_ = 0;
    std::deque<std::pair<uint8_t, unsigned long>> sequence_ {};
  public:
    /*
     * `configuración()` se llama una vez al arrancar. En este punto WiFi aún no está conectado.
     * Úsalo para inicializar variables, sensores o similares.
     */
    void setup() {
      // Configuración the pin, and default to LOW
      pinMode(USERMOD_BUZZER_PIN, OUTPUT);
      digitalWrite(USERMOD_BUZZER_PIN, LOW);

      // Beep on startup
      sequence_.push_back({ HIGH, 50 });
      sequence_.push_back({ LOW, 0 });
    }


    /*
     * `connected()` se llama cada vez que el WiFi se (re)conecta.
     * Úsalo para inicializar interfaces de red.
     */
    void connected() {
      // Doble beep on WiFi
      sequence_.push_back({ LOW, 100 });
      sequence_.push_back({ HIGH, 50 });
      sequence_.push_back({ LOW, 30 });
      sequence_.push_back({ HIGH, 50 });
      sequence_.push_back({ LOW, 0 });
    }

    /*
     * `bucle()` se llama de forma continua. Aquí puedes comprobar eventos, leer sensores, etc.
     */
    void loop() {
      if (sequence_.size() < 1) return; // Wait until there is a sequence
      if (millis() - lastTime_ <= delay_) return; // Wait until delay has elapsed

      auto event = sequence_.front();
      sequence_.pop_front();

      digitalWrite(USERMOD_BUZZER_PIN, event.first);
      delay_ = event.second;

      lastTime_ = millis();
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please definir it in constante.h!).
     * This could be used in the futuro for the sistema to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_BUZZER;
    }
};

static BuzzerUsermod buzzer;
REGISTER_USERMOD(buzzer);