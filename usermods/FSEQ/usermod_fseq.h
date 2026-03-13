#pragma once

#ifndef USED_STORAGE_FILESYSTEMS
#ifdef WLED_USE_SD_SPI
#define USED_STORAGE_FILESYSTEMS "SD SPI, LittleFS"
#else
#define USED_STORAGE_FILESYSTEMS "SD MMC, LittleFS"
#endif
#endif

#include "wled.h"
#ifdef WLED_USE_SD_SPI
#include <SD.h>
#include <SPI.h>
#endif

#ifndef SD_ADAPTER
#if defined(WLED_USE_SD) || defined(WLED_USE_SD_SPI)
#ifdef WLED_USE_SD_SPI
#ifndef WLED_USE_SD
#define WLED_USE_SD
#endif
#ifndef WLED_PIN_SCK
#define WLED_PIN_SCK SCK
#endif
#ifndef WLED_PIN_MISO
#define WLED_PIN_MISO MISO
#endif
#ifndef WLED_PIN_MOSI
#define WLED_PIN_MOSI MOSI
#endif
#ifndef WLED_PIN_SS
#define WLED_PIN_SS SS
#endif
#define SD_ADAPTER SD
#else
#define SD_ADAPTER SD_MMC
#endif
#endif
#endif

#ifdef WLED_USE_SD_SPI
#ifndef SPI_PORT_DEFINED
inline SPIClass spiPort = SPIClass(VSPI);
#define SPI_PORT_DEFINED
#include "../usermods/FSEQ/fseq_player.h"
#include "../usermods/FSEQ/sd_manager.h"
#include "../usermods/FSEQ/web_ui_manager.h"
#include "wled.h"
#endif
#endif

// Usermod for FSEQ playback with UDP and web UI support
class UsermodFseq : public Usermod {
private:
  WebUIManager webUI;        // Web UI Manager module (handles endpoints)
  static const char _name[]; // for storing usermod name in config

public:
  // Setup function called once at startup
  void setup() {
    DEBUG_PRINTF("[%s] Usermod loaded\n", FPSTR(_name));

    // Initialize SD card using SDManager
    SDManager sd;
    if (!sd.begin()) {
      DEBUG_PRINTF("[%s] SD initialization FAILED.\n", FPSTR(_name));
    } else {
      DEBUG_PRINTF("[%s] SD initialization successful.\n", FPSTR(_name));
    }

    // Register web endpoints defined in WebUIManager
    webUI.registerEndpoints();
  }

  // Loop function called continuously
  void loop() {
    // Process FSEQ playback (includes UDP sync commands)
    FSEQPlayer::handlePlayRecording();
  }

  // Unique ID for the usermod
  uint16_t getId() override { return USERMOD_ID_SD_CARD; }

  // Add a link in the Info tab to your SD UI
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) {
      user = root.createNestedObject("u");
    }
    // Create an array with two items: label and value
    JsonArray arr = user.createNestedArray("Usermod FSEQ UI");

    String ip = WiFi.localIP().toString();
    arr.add("http://" + ip + "/fsequi"); // value
  }

  // Save your SPI pins to WLED config JSON
  void addToConfig(JsonObject &root) override {
#ifdef WLED_USE_SD_SPI
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top["csPin"] = configPinSourceSelect;
    top["sckPin"] = configPinSourceClock;
    top["misoPin"] = configPinPoci;
    top["mosiPin"] = configPinPico;
#endif
  }

  // Read your SPI pins from WLED config JSON
  bool readFromConfig(JsonObject &root) override {
#ifdef WLED_USE_SD_SPI
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
      return false;

    if (top["csPin"].is<int>())
      configPinSourceSelect = top["csPin"].as<int>();
    if (top["sckPin"].is<int>())
      configPinSourceClock = top["sckPin"].as<int>();
    if (top["misoPin"].is<int>())
      configPinPoci = top["misoPin"].as<int>();
    if (top["mosiPin"].is<int>())
      configPinPico = top["mosiPin"].as<int>();

    reinit_SD_SPI(); // reinitialize SD with new pins
    return true;
#else
    return false;
#endif
  }

#ifdef WLED_USE_SD_SPI
  // Reinitialize SD SPI with updated pins
  void reinit_SD_SPI() {
    // Deinit SD if needed
    SD_ADAPTER.end();
    // Reallocate pins
    PinManager::deallocatePin(configPinSourceSelect, PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinSourceClock, PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinPoci, PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinPico, PinOwner::UM_SdCard);

    PinManagerPinType pins[4] = {{configPinSourceSelect, true},
                                 {configPinSourceClock, true},
                                 {configPinPoci, false},
                                 { configPinPico,
                                   true }};
    if (!PinManager::allocateMultiplePins(pins, 4, PinOwner::UM_SdCard)) {
      DEBUG_PRINTF("[%s] SPI pin allocation failed!\n", FPSTR(_name));
      return;
    }

    // Reinit SPI with new pins
    spiPort.begin(configPinSourceClock, configPinPoci, configPinPico,
                  configPinSourceSelect);

    // Try to begin SD again
    if (!SD_ADAPTER.begin(configPinSourceSelect, spiPort)) {
      DEBUG_PRINTF("[%s] SPI begin failed!\n", FPSTR(_name));
    } else {
      DEBUG_PRINTF("[%s] SD SPI reinitialized with new pins\n", FPSTR(_name));
    }
  }

  // Getter methods and static variables for SD pins
  static int8_t getCsPin() { return configPinSourceSelect; }
  static int8_t getSckPin() { return configPinSourceClock; }
  static int8_t getMisoPin() { return configPinPoci; }
  static int8_t getMosiPin() { return configPinPico; }

  static int8_t configPinSourceSelect;
  static int8_t configPinSourceClock;
  static int8_t configPinPoci;
  static int8_t configPinPico;
#endif
};