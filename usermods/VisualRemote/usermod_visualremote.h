#pragma once

#include "wled.h"

void handleRemote_visualremote(uint8_t *data, size_t len);

class UsermodVisualRemote : public Usermod {
  private:

  public:
    void setup() {
      Serial.println("VisualRemote mod active!");
    }

    void loop() {

    }

    bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) {
        
      DEBUG_PRINT(F("CUSTOM ESP-NOW: ")); DEBUG_PRINT(last_signal_src); DEBUG_PRINT(F(" -> ")); DEBUG_PRINTLN(len);
      for (int i=0; i<len; i++) DEBUG_PRINTF_P(PSTR("%02x "), data[i]);
      DEBUG_PRINTLN();

      // Handle Remote here 
      //handleRemote_visualremote(data, len);

      // Process the message here
      return true; // Override further processing
    }



};
