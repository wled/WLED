#pragma once

#include "wled.h"

/*
 * Matter (Project CHIP) WiFi-only usermod for WLED-MM
 *
 * Exposes WLED as a Matter Extended Color Light (device type 0x010D)
 * using WiFi-only commissioning (no BLE/Bluetooth required).
 *
 * Commissioning: The Matter stack uses on-network (mDNS) discovery when
 * WLED is already connected to WiFi. If the device has no WiFi credentials,
 * the Matter stack creates a Soft-AP (CHIP-XXXX) for initial provisioning.
 *
 * Clusters exposed:
 *   - On/Off (0x0006)
 *   - Level Control (0x0008) → WLED brightness
 *   - Color Control (0x0300) → WLED primary color (HSV + Color Temperature)
 *
 * Build requirement: ESP-IDF v5.1+ with the esp_matter component.
 * Enable with -D USERMOD_MATTER in your build environment.
 */

#include <esp_matter.h>
#include <esp_matter_core.h>

// ── Matter cluster IDs (from the Matter Application Cluster Specification) ──
static constexpr uint32_t MATTER_CL_ON_OFF       = 0x0006;
static constexpr uint32_t MATTER_CL_LEVEL_CTRL   = 0x0008;
static constexpr uint32_t MATTER_CL_COLOR_CTRL   = 0x0300;

// ── Matter attribute IDs ────────────────────────────────────────────────────
// On/Off cluster
static constexpr uint32_t MATTER_AT_ON_OFF        = 0x0000;
// Level Control cluster
static constexpr uint32_t MATTER_AT_CURRENT_LEVEL = 0x0000;
// Color Control cluster
static constexpr uint32_t MATTER_AT_CURRENT_HUE   = 0x0000;
static constexpr uint32_t MATTER_AT_CURRENT_SAT   = 0x0001;
static constexpr uint32_t MATTER_AT_COLOR_TEMP     = 0x0007;
static constexpr uint32_t MATTER_AT_COLOR_MODE     = 0x0008;

// ── Color mode values (Matter spec §3.2.7.10) ──────────────────────────────
static constexpr uint8_t MATTER_CM_HS   = 0; // Hue/Saturation
static constexpr uint8_t MATTER_CM_XY   = 1; // CIE x/y
static constexpr uint8_t MATTER_CM_TEMP = 2; // Color Temperature

class MatterUsermod : public Usermod {
 private:
  // ── Matter handles ──────────────────────────────────────────────────────
  esp_matter::node_t     *mNode     = nullptr;
  esp_matter::endpoint_t *mEndpoint = nullptr;
  uint16_t mEndpointId  = 0;
  bool     mStarted     = false;

  // ── Pending state set from the Matter task (consumed in loop()) ─────────
  volatile bool    mPending         = false;
  volatile bool    mPendingOn       = false;
  volatile uint8_t mPendingBri      = 0;
  volatile uint8_t mPendingHue      = 0;
  volatile uint8_t mPendingSat      = 0;
  volatile uint16_t mPendingCT      = 0;
  volatile uint8_t mPendingColorMode = MATTER_CM_HS;

  // ── Cached WLED state for external-change detection ─────────────────────
  byte     mPrevCol[4] = {0, 0, 0, 0};
  byte     mPrevBri    = 0;
  bool     mPrevOn     = false;
  unsigned long mLastSyncMs = 0;
  static constexpr unsigned long SYNC_INTERVAL_MS = 250;

  // ── Commissioning configuration ─────────────────────────────────────────
  uint32_t mPasscode      = 20202021;  // Matter test passcode
  uint16_t mDiscriminator = 3840;      // Matter default discriminator

  // Singleton – needed because Matter SDK uses C-style static callbacks.
  static MatterUsermod *_instance;

  // ─────────────────────────────────────────────────────────────────────────
  // Matter SDK callbacks (invoked from the Matter task context)
  // ─────────────────────────────────────────────────────────────────────────

  static esp_err_t _attrCb(esp_matter::attribute::callback_type_t type,
                            uint16_t endpoint_id,
                            uint32_t cluster_id,
                            uint32_t attribute_id,
                            esp_matter_attr_val_t *val,
                            void *priv_data)
  {
    if (!_instance || type != esp_matter::attribute::POST_UPDATE) return ESP_OK;
    if (endpoint_id != _instance->mEndpointId) return ESP_OK;

    if (cluster_id == MATTER_CL_ON_OFF && attribute_id == MATTER_AT_ON_OFF) {
      _instance->mPendingOn = val->val.b;
      _instance->mPending   = true;
    } else if (cluster_id == MATTER_CL_LEVEL_CTRL && attribute_id == MATTER_AT_CURRENT_LEVEL) {
      _instance->mPendingBri = val->val.u8;
      _instance->mPending    = true;
    } else if (cluster_id == MATTER_CL_COLOR_CTRL) {
      switch (attribute_id) {
        case MATTER_AT_CURRENT_HUE:
          _instance->mPendingHue = val->val.u8;
          _instance->mPending    = true;
          break;
        case MATTER_AT_CURRENT_SAT:
          _instance->mPendingSat = val->val.u8;
          _instance->mPending    = true;
          break;
        case MATTER_AT_COLOR_TEMP:
          _instance->mPendingCT = val->val.u16;
          _instance->mPending   = true;
          break;
        case MATTER_AT_COLOR_MODE:
          _instance->mPendingColorMode = val->val.u8;
          _instance->mPending          = true;
          break;
      }
    }
    return ESP_OK;
  }

  static esp_err_t _identifyCb(esp_matter::identification::callback_type_t type,
                                uint16_t endpoint_id,
                                uint8_t  effect_id,
                                uint8_t  effect_variant,
                                void    *priv_data)
  {
    // Identification request – could flash LEDs; ignored for now.
    return ESP_OK;
  }

  static void _eventCb(const ChipDeviceEvent *event, intptr_t arg)
  {
    // Reserved for handling commissioning / fabric events in the future.
  }

  // ─────────────────────────────────────────────────────────────────────────
  // Apply pending Matter state to WLED (called from Arduino loop context)
  // ─────────────────────────────────────────────────────────────────────────

  void applyPending()
  {
    if (!mPending) return;
    mPending = false;

    // On / Off — when turning on with no explicit level, restore last brightness
    if (mPendingOn) {
      bri = (mPendingBri > 0) ? mPendingBri : briLast;
    } else {
      bri = 0;
    }

    // Color
    if (mPendingColorMode == MATTER_CM_TEMP && mPendingCT > 0) {
      byte rgb[3];
      colorCTtoRGB(mPendingCT, rgb);
      col[0] = rgb[0];
      col[1] = rgb[1];
      col[2] = rgb[2];
    } else {
      // HSV mode (default)
      // Matter hue is 0-254 (mapped to 0°-360°), WLED expects 0-65535.
      uint16_t wledHue = (mPendingHue > 0)
                           ? (uint16_t)(((uint32_t)mPendingHue * 65535U) / 254U)
                           : 0;
      // Matter saturation is 0-254, WLED expects 0-255.
      byte wledSat = (mPendingSat < 254) ? (byte)(((uint16_t)mPendingSat * 255U) / 254U) : 255;
      byte rgb[3];
      colorHStoRGB(wledHue, wledSat, rgb);
      col[0] = rgb[0];
      col[1] = rgb[1];
      col[2] = rgb[2];
    }

    colorUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  // ─────────────────────────────────────────────────────────────────────────
  // Push WLED state changes (from UI / API / presets) back to Matter
  // ─────────────────────────────────────────────────────────────────────────

  void syncToMatter()
  {
    bool curOn = (bri > 0);
    if (curOn == mPrevOn && bri == mPrevBri && memcmp(col, mPrevCol, 4) == 0)
      return; // nothing changed

    mPrevOn  = curOn;
    mPrevBri = bri;
    memcpy(mPrevCol, col, 4);

    esp_matter_attr_val_t val;

    // On/Off
    val = esp_matter_bool(curOn);
    esp_matter::attribute::update(mEndpointId, MATTER_CL_ON_OFF,
                                  MATTER_AT_ON_OFF, &val);

    // Level
    val = esp_matter_nullable_uint8(curOn ? bri : (uint8_t)0);
    esp_matter::attribute::update(mEndpointId, MATTER_CL_LEVEL_CTRL,
                                  MATTER_AT_CURRENT_LEVEL, &val);

    // Color – convert current RGB to Matter hue/saturation (0-254 range).
    // Use a simple max/min approach matching the Matter spec range.
    byte r = col[0], g = col[1], b = col[2];
    byte maxC = max(r, max(g, b));
    byte minC = min(r, min(g, b));
    uint8_t matterSat = (maxC == 0) ? 0 : (uint8_t)(((uint16_t)(maxC - minC) * 254U) / maxC);

    uint8_t matterHue = 0;
    if (maxC != minC) {
      float hf;
      float delta = (float)(maxC - minC);
      if (r == maxC)      hf = fmodf((float)(g - b) / delta, 6.0f);
      else if (g == maxC) hf = ((float)(b - r) / delta) + 2.0f;
      else                hf = ((float)(r - g) / delta) + 4.0f;
      if (hf < 0.0f) hf += 6.0f;
      uint16_t raw = (uint16_t)((hf / 6.0f) * 254.0f + 0.5f);
      matterHue = (raw > 254) ? 254 : (uint8_t)raw;
    }

    val = esp_matter_nullable_uint8(matterHue);
    esp_matter::attribute::update(mEndpointId, MATTER_CL_COLOR_CTRL,
                                  MATTER_AT_CURRENT_HUE, &val);

    val = esp_matter_nullable_uint8(matterSat);
    esp_matter::attribute::update(mEndpointId, MATTER_CL_COLOR_CTRL,
                                  MATTER_AT_CURRENT_SAT, &val);
  }

 public:
  MatterUsermod(const char *name, bool enabled) : Usermod(name, enabled) {
    _instance = this;
  }

  // ── Usermod lifecycle ───────────────────────────────────────────────────

  void setup() override
  {
    if (!enabled) return;

    // Create Matter node
    esp_matter::node::config_t nodeCfg;
    mNode = esp_matter::node::create(&nodeCfg, _attrCb, _identifyCb);
    if (!mNode) {
      USER_PRINTLN(F("Matter: node creation failed"));
      return;
    }

    // Create Extended Color Light endpoint
    esp_matter::endpoint::extended_color_light::config_t lightCfg;
    lightCfg.on_off.on_off                   = (bri > 0);
    lightCfg.level_control.current_level     = bri;
    lightCfg.color_control.color_mode        = MATTER_CM_HS;
    lightCfg.color_control.enhanced_color_mode = MATTER_CM_HS;

    mEndpoint = esp_matter::endpoint::extended_color_light::create(
        mNode, &lightCfg, esp_matter::endpoint_flags::ENDPOINT_FLAG_NONE, nullptr);
    if (!mEndpoint) {
      USER_PRINTLN(F("Matter: endpoint creation failed"));
      return;
    }
    mEndpointId = esp_matter::endpoint::get_id(mEndpoint);

    // Start the Matter stack (WiFi-only; BLE is compiled out)
    esp_err_t err = esp_matter::start(_eventCb);
    if (err != ESP_OK) {
      USER_PRINTF("Matter: start failed (0x%x)\n", err);
      return;
    }

    mStarted = true;
    USER_PRINTLN(F("Matter: started (WiFi-only commissioning)"));
    USER_PRINTF("Matter: passcode %lu  discriminator %u\n",
                (unsigned long)mPasscode, mDiscriminator);
  }

  void loop() override
  {
    if (!enabled || !mStarted) return;
    applyPending();

    unsigned long now = millis();
    if (now - mLastSyncMs >= SYNC_INTERVAL_MS) {
      mLastSyncMs = now;
      syncToMatter();
    }
  }

  void connected() override
  {
    // WiFi (re)connected – no extra action needed; the Matter stack
    // handles network connectivity internally.
  }

  uint16_t getId() override { return USERMOD_ID_MATTER; }

  // ── JSON info page ──────────────────────────────────────────────────────

  void addToJsonInfo(JsonObject &obj) override
  {
    JsonObject matter = obj.createNestedObject(F("Matter"));
    matter[F("Status")]        = mStarted ? F("Running") : F("Not started");
    matter[F("Endpoint")]      = mEndpointId;
    matter[F("Passcode")]      = mPasscode;
    matter[F("Discriminator")] = mDiscriminator;
  }

  // ── Persistent configuration ────────────────────────────────────────────

  void addToConfig(JsonObject &obj) override
  {
    JsonObject top = obj.createNestedObject(FPSTR(_name));
    top[F("passcode")]      = mPasscode;
    top[F("discriminator")] = mDiscriminator;
  }

  bool readFromConfig(JsonObject &obj) override
  {
    JsonObject top = obj[FPSTR(_name)];
    if (top.isNull()) return false;
    mPasscode      = top[F("passcode")]      | 20202021;
    mDiscriminator = top[F("discriminator")] | 3840;
    return !top.isNull();
  }
};

MatterUsermod *MatterUsermod::_instance = nullptr;
static MatterUsermod usermod_matter("Project CHIP (Matter)", true);
REGISTER_USERMOD(usermod_matter);