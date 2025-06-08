#include "wled.h"
#include "Arduino.h"

#include <deque>

#define USERMOD_ID_BUZZER 900
#ifndef USERMOD_BUZZER_PIN
#define USERMOD_BUZZER_PIN GPIO_NUM_32
#endif

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * 
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

class BuzzerUsermod : public Usermod {
  private:
    unsigned long lastTime_ = 0;
    unsigned long delay_ = 0;
    std::deque<std::pair<uint8_t, unsigned long>> sequence_ {};
  public:
    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() {
      // Setup the pin, and default to LOW
      pinMode(USERMOD_BUZZER_PIN, OUTPUT);
      digitalWrite(USERMOD_BUZZER_PIN, LOW);

      // Beep on startup
      sequence_.push_back({ HIGH, 50 });
      sequence_.push_back({ LOW, 0 });
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
      // Double beep on WiFi
      sequence_.push_back({ LOW, 100 });
      sequence_.push_back({ HIGH, 50 });
      sequence_.push_back({ LOW, 30 });
      sequence_.push_back({ HIGH, 50 });
      sequence_.push_back({ LOW, 0 });
    }

    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
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
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_BUZZER;
    }
};

static BuzzerUsermod buzzer;
REGISTER_USERMOD(buzzer);