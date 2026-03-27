#pragma once

#include "wled.h"

/*
 * Matter (Project CHIP) WiFi-only usermod for WLED
 *
 * Exposes WLED as a Matter Extended Color Light (device type 0x010D)
 * using WiFi-only commissioning (no BLE/Bluetooth required).
 *
 * This header uses the PIMPL pattern to hide all CHIP SDK dependencies
 * so that wled.h can include it without pulling in esp_matter headers.
 */

class MatterUsermod : public Usermod {
 public:
  struct Impl;          // opaque – defined in the .cpp with CHIP SDK types; must be public for PIMPL
  Impl *pImpl = nullptr;

  MatterUsermod();
  ~MatterUsermod();

  void     setup()     override;
  void     loop()      override;
  void     connected() override;
  uint16_t getId()     override;

  void addToJsonInfo(JsonObject &obj)    override;
  void addToConfig(JsonObject &obj)      override;
  bool readFromConfig(JsonObject &obj)   override;
};
