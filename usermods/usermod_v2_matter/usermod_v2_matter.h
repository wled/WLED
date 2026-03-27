#pragma once

/*
 * Matter (Project CHIP) WiFi-only usermod for WLED
 *
 * Exposes WLED as a Matter Extended Color Light (device type 0x010D)
 * using WiFi-only commissioning (no BLE/Bluetooth required).
 *
 * This header uses the PIMPL pattern to hide all CHIP SDK dependencies
 * so that wled.h can include it without pulling in esp_matter headers.
 *
 * Include strategy:
 *   - When compiled as part of wled00/ (e.g. usermods_list.cpp), wled.h is
 *     already included before this header, so Usermod and JsonObject are
 *     already defined.
 *   - When compiled as a standalone PlatformIO library (usermod_v2_matter.cpp),
 *     wled.h is not yet available (it would pull in AsyncTCP.h which is not on
 *     the library include path).  In that case we include the minimal headers
 *     needed for the class definition directly.
 */

#ifndef WLED_H
// ── Library-build context ─────────────────────────────────────────────────
// wled.h has not been included yet, so we provide a minimal standalone shim.
// This shim mirrors fcn_declare.h's Usermod definition exactly (same vtable
// order, same data layout) so that the ODR is satisfied at link time.
#include <stdint.h>
#include <stddef.h>
#include <Print.h>
#include "../../wled00/src/dependencies/json/ArduinoJson-v6.h"

#ifndef WLED_USERMOD_DEFINED
#define WLED_USERMOD_DEFINED

// Matches the um_types_t enum in fcn_declare.h (um_manager.cpp section)
typedef enum UM_Data_Types {
  UMT_BYTE = 0, UMT_UINT16, UMT_INT16, UMT_UINT32, UMT_INT32,
  UMT_FLOAT, UMT_DOUBLE, UMT_BYTE_ARR, UMT_UINT16_ARR, UMT_INT16_ARR,
  UMT_UINT32_ARR, UMT_INT32_ARR, UMT_FLOAT_ARR, UMT_DOUBLE_ARR
} um_types_t;

// Matches UM_Exchange_Data in fcn_declare.h exactly
typedef struct UM_Exchange_Data {
  size_t       u_size  = 0;
  um_types_t  *u_type  = nullptr;
  void       **u_data  = nullptr;
  UM_Exchange_Data() {}
  ~UM_Exchange_Data() {
    if (u_type) delete[] u_type;
    if (u_data) delete[] u_data;
  }
} um_data_t;

// Minimal Usermod base — vtable order must exactly match fcn_declare.h.
// Keep this in sync with the class definition there.
class Usermod {
  protected:
    um_data_t *um_data;
  public:
    Usermod() : um_data(nullptr) {}
    virtual ~Usermod() { if (um_data) delete um_data; }
    virtual void setup()   = 0;
    virtual void loop()    = 0;
    virtual void handleOverlayDraw()                            {}
    virtual bool handleButton(uint8_t b)                        { return false; }
    virtual bool getUMData(um_data_t **data)                    { if (data) *data = nullptr; return false; }
    virtual void connected()                                    {}
    virtual void appendConfigData(Print& settingsScript);
    virtual void addToJsonState(JsonObject& obj)                {}
    virtual void addToJsonInfo(JsonObject& obj)                 {}
    virtual void readFromJsonState(JsonObject& obj)             {}
    virtual void addToConfig(JsonObject& obj)                   {}
    virtual bool readFromConfig(JsonObject& obj)                { return true; }
    virtual void onMqttConnect(bool sessionPresent)             {}
    virtual bool onMqttMessage(char* topic, char* payload)      { return false; }
    virtual bool onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) { return false; }
    virtual bool onUdpPacket(uint8_t* payload, size_t len)      { return false; }
    virtual void onUpdateBegin(bool)                            {}
    virtual void onStateChange(uint8_t mode)                    {}
    virtual uint16_t getId()                                    { return 255; } // USERMOD_ID_UNSPECIFIED
  private:
    static Print* oappend_shim;
    virtual void appendConfigData()                             {}
  protected:
    template<typename T> static inline void oappend(const T& t) { oappend_shim->print(t); }
};

#endif // WLED_USERMOD_DEFINED
#endif // !WLED_H

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
