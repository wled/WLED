#pragma once

#include "wled.h"

#include "sd_adapter_compat.h"


#include "fseq_player.h"
#include "web_ui_manager.h"

// Usermod for FSEQ playback with UDP and web UI support
class UsermodFseq : public Usermod {
private:
  WebUIManager webUI;        // Web UI Manager module (handles endpoints)
  static const char _name[]; // for storing usermod name in config

public:
  // Setup function called once at startup
  void setup() {
    DEBUG_PRINTF("[%s] Usermod loaded\n", FPSTR(_name));

    // Register web endpoints defined in WebUIManager
    webUI.registerEndpoints();
  }

  // Loop function called continuously
  void loop() {
    // Process FSEQ playback (includes UDP sync commands)
    FSEQPlayer::handlePlayRecording();
  }

  // Unique ID for the usermod
  uint16_t getId() override { return USERMOD_ID_FSEQ; }

  // Add a link in the Info tab to your SD
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) {
      user = root.createNestedObject("u");
    }
    JsonArray arr = user.createNestedArray("FSEQ UI");
      
    String button = R"rawliteral(
                   <button class="btn ibtn" style="width:100px;" onclick="window.open(getURL('/fsequi'),'_self');" id="updBt">Open UI</button>
                    )rawliteral";
      
    arr.add(button);
  }
  void addToConfig(JsonObject &root) override {}
  bool readFromConfig(JsonObject &root) override { return true; }
};