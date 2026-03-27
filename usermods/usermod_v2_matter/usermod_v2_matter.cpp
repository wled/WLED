#include "usermod_v2_matter.h"

/*
 * Matter (Project CHIP) WiFi-only usermod for WLED
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

// ── WLED global state / helpers needed by this compilation unit ──────────────
// Declared here (extern) to avoid pulling in wled.h (which would cascade into
// AsyncTCP.h and other Arduino libraries not on the library include path).
// Keep in sync with wled00/wled.h and wled00/const.h declarations.
typedef uint8_t byte;

extern byte bri;        // wled.h: global brightness (set)
extern byte briLast;    // wled.h: brightness before turned off
extern byte colPri[4];  // wled.h: current RGB(W) primary colour
extern bool externalWiFiManager; // wled.h: suppresses WiFi.mode(NULL) in initConnection()
extern bool isWiFiConfigured(); // network.cpp — true when SSID is set (non-default)

// Colour helper functions (wled00/colors.cpp)
extern void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);
extern void colorCTtoRGB(uint16_t mired, byte* rgb);

// Trigger a WLED state update after changing bri/colour
extern void colorUpdated(byte callMode);

// Constants from wled00/const.h
#define CALL_MODE_DIRECT_CHANGE  1
#define USERMOD_ID_MATTER        59

// Debug macros — mirror wled.h's WLED_DEBUG section
#ifdef WLED_DEBUG
  #define DEBUGOUT Serial
  #define DEBUG_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUG_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
#endif

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

// ─────────────────────────────────────────────────────────────────────────────
// PIMPL implementation — hides all CHIP SDK types from the header
// ─────────────────────────────────────────────────────────────────────────────

struct MatterUsermod::Impl {
  // ── Matter handles ──────────────────────────────────────────────────────
  esp_matter::node_t     *mNode     = nullptr;
  esp_matter::endpoint_t *mEndpoint = nullptr;
  uint16_t mEndpointId  = 0;
  bool     mStarted     = false;   // true after esp_matter::start() succeeds
  bool     mNodeReady   = false;   // true after node+endpoint created in setup()

  // ── Pending state set from the Matter task (consumed in loop()) ─────────
  // Separate dirty flags so applyPending only touches what actually changed.
  volatile bool    mPendingOn       = false;
  volatile bool    mPendingOnDirty  = false;
  volatile uint8_t mPendingBri      = 0;
  volatile bool    mPendingBriDirty = false;
  volatile uint8_t mPendingHue      = 0;
  volatile uint8_t mPendingSat      = 0;
  volatile bool    mPendingHSDirty  = false;
  volatile uint16_t mPendingCT      = 0;
  volatile bool    mPendingCTDirty  = false;
  volatile uint8_t mPendingColorMode = MATTER_CM_HS;

  // ── Cached WLED state for external-change detection ─────────────────────
  byte     mPrevCol[4] = {0, 0, 0, 0};
  byte     mPrevBri    = 0;
  bool     mPrevOn     = false;
  unsigned long mLastApplyMs   = 0;  // time of last Matter→WLED apply
  bool     mSyncing    = false;      // true while syncToMatter is calling attribute::update

  // ── Commissioning configuration ─────────────────────────────────────────
  uint32_t mPasscode      = 20202021;  // Matter test passcode
  uint16_t mDiscriminator = 3840;      // Matter default discriminator

  // ── QR code payload (Matter Core Spec §5.1.3) ───────────────────────────
  // Encodes onboarding info into the "MT:XXXXXX" base-38 string.
  // vendor_id=0xFFF1 (test), product_id=0x8000, flow=0, rendezvous=4 (OnNetwork)
  void buildQRPayload(char *out, size_t outLen) const {
    static const char kBase38[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.";
    // Build 88-bit little-endian payload
    // [0:2]   version (0)
    // [3:18]  vendor_id
    // [19:34] product_id
    // [35:36] commissioning_flow (0=Standard)
    // [37:48] rendezvous_info (4=OnNetwork)
    // [49:60] discriminator (12 bits)
    // [61:87] passcode (27 bits)
    const uint16_t vendor_id   = 0xFFF1;
    const uint16_t product_id  = 0x8000;
    const uint8_t  flow        = 0;
    const uint16_t rendezvous  = 4; // OnNetwork
    // Assemble as a 96-bit (12-byte) little-endian integer
    uint8_t buf[12] = {};
    // Use 64-bit chunks to avoid ULL arithmetic on 32-bit MCU
    uint64_t lo = 0, hi = 0;
    lo |= ((uint64_t)(vendor_id)  & 0xFFFF) << 3;
    lo |= ((uint64_t)(product_id) & 0xFFFF) << 19;
    lo |= ((uint64_t)(flow)       & 0x3)    << 35;
    lo |= ((uint64_t)(rendezvous) & 0xFFF)  << 37;
    lo |= ((uint64_t)(mDiscriminator & 0xFFF)) << 49;
    // passcode spans bits 61-87: 3 bits go into lo (bits 61-63), 24 into hi
    lo |= ((uint64_t)(mPasscode & 0x7)) << 61;
    hi  = (uint64_t)(mPasscode >> 3) & 0xFFFFFF;
    for (int i = 0; i < 8; i++) buf[i] = (lo >> (8*i)) & 0xFF;
    for (int i = 0; i < 3; i++) buf[8+i] = (hi >> (8*i)) & 0xFF;

    // Base-38 encode: 3 bytes → 5 chars, then remaining 2 bytes → 3 chars
    char encoded[20] = {};  // 3*5 + 3 = 18 chars + null
    int pos = 0;
    for (int i = 0; i < 9; i += 3) {
      uint32_t val = buf[i] | ((uint32_t)buf[i+1] << 8) | ((uint32_t)buf[i+2] << 16);
      for (int j = 0; j < 5; j++) { encoded[pos++] = kBase38[val % 38]; val /= 38; }
    }
    // Last 2 bytes → 3 chars
    uint32_t val = buf[9] | ((uint32_t)buf[10] << 8);
    for (int j = 0; j < 3; j++) { encoded[pos++] = kBase38[val % 38]; val /= 38; }
    encoded[pos] = '\0';

    snprintf(out, outLen, "MT:%s", encoded);
  }

  // ── 11-digit manual pairing code (Matter Core Spec §5.1.4) ─────────────
  // Format: chunk1(1) + chunk2(5) + chunk3(4) + verhoeff_check(1)
  void buildManualCode(char *out, size_t outLen) const {
    uint32_t chunk1 = (mDiscriminator >> 10) & 0x3;
    uint32_t chunk2 = ((mDiscriminator & 0x300) << 6) | (mPasscode & 0x3FFF);
    uint32_t chunk3 = (mPasscode >> 14) & 0x1FFF;

    char digits[11];
    snprintf(digits, sizeof(digits), "%01lu%05lu%04lu",
             (unsigned long)chunk1, (unsigned long)chunk2, (unsigned long)chunk3);

    // Verhoeff check digit
    static const uint8_t kD[10][10] = {
      {0,1,2,3,4,5,6,7,8,9},{1,2,3,4,0,6,7,8,9,5},{2,3,4,0,1,7,8,9,5,6},
      {3,4,0,1,2,8,9,5,6,7},{4,0,1,2,3,9,5,6,7,8},{5,9,8,7,6,0,4,3,2,1},
      {6,5,9,8,7,1,0,4,3,2},{7,6,5,9,8,2,1,0,4,3},{8,7,6,5,9,3,2,1,0,4},
      {9,8,7,6,5,4,3,2,1,0},
    };
    static const uint8_t kP[8][10] = {
      {0,1,2,3,4,5,6,7,8,9},{1,5,7,6,2,8,3,0,9,4},{5,8,0,3,7,9,6,1,4,2},
      {8,9,1,6,0,4,3,5,2,7},{9,4,5,3,1,2,6,8,7,0},{4,2,8,6,5,7,3,9,0,1},
      {2,7,9,3,8,0,6,4,1,5},{7,0,4,6,9,1,3,2,5,8},
    };
    static const uint8_t kInv[10] = {0,4,3,2,1,5,6,7,8,9};

    uint8_t c = 0;
    for (int i = 9; i >= 0; i--) {
      int pos = 10 - 1 - i;  // position from right, 0-indexed
      c = kD[c][kP[(pos + 1) % 8][digits[i] - '0']];
    }
    uint8_t check = kInv[c];

    snprintf(out, outLen, "%s%u", digits, check);
  }
};

// Singleton pointer – needed because Matter SDK uses C-style static callbacks.
static MatterUsermod::Impl *_implInstance = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Matter SDK callbacks (invoked from the Matter task context)
// ─────────────────────────────────────────────────────────────────────────────

static esp_err_t _attrCb(esp_matter::attribute::callback_type_t type,
                          uint16_t endpoint_id,
                          uint32_t cluster_id,
                          uint32_t attribute_id,
                          esp_matter_attr_val_t *val,
                          void *priv_data)
{
  if (!_implInstance || type != esp_matter::attribute::POST_UPDATE) return ESP_OK;
  if (endpoint_id != _implInstance->mEndpointId) return ESP_OK;
  // Ignore callbacks that we ourselves triggered via attribute::update in syncToMatter
  if (_implInstance->mSyncing) return ESP_OK;

  if (cluster_id == MATTER_CL_ON_OFF && attribute_id == MATTER_AT_ON_OFF) {
    _implInstance->mPendingOn      = val->val.b;
    _implInstance->mPendingOnDirty = true;
  } else if (cluster_id == MATTER_CL_LEVEL_CTRL && attribute_id == MATTER_AT_CURRENT_LEVEL) {
    _implInstance->mPendingBri      = val->val.u8;
    _implInstance->mPendingBriDirty = true;
  } else if (cluster_id == MATTER_CL_COLOR_CTRL) {
    switch (attribute_id) {
      case MATTER_AT_CURRENT_HUE:
        _implInstance->mPendingHue     = val->val.u8;
        _implInstance->mPendingHSDirty = true;
        break;
      case MATTER_AT_CURRENT_SAT:
        _implInstance->mPendingSat     = val->val.u8;
        _implInstance->mPendingHSDirty = true;
        break;
      case MATTER_AT_COLOR_TEMP:
        _implInstance->mPendingCT      = val->val.u16;
        _implInstance->mPendingCTDirty = true;
        break;
      case MATTER_AT_COLOR_MODE:
        _implInstance->mPendingColorMode = val->val.u8;
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

// ─────────────────────────────────────────────────────────────────────────────
// Apply pending Matter state to WLED (called from Arduino loop context)
// ─────────────────────────────────────────────────────────────────────────────

static void applyPending(MatterUsermod::Impl *d)
{
  bool anyChange = d->mPendingOnDirty || d->mPendingBriDirty ||
                   d->mPendingHSDirty || d->mPendingCTDirty;
  if (!anyChange) return;

  // Snapshot and clear flags atomically (single-core Arduino loop context)
  bool applyOn  = d->mPendingOnDirty;  d->mPendingOnDirty  = false;
  bool applyBri = d->mPendingBriDirty; d->mPendingBriDirty = false;
  bool applyHS  = d->mPendingHSDirty;  d->mPendingHSDirty  = false;
  bool applyCT  = d->mPendingCTDirty;  d->mPendingCTDirty  = false;

  if (applyOn || applyBri) {
    bool on  = d->mPendingOn;
    uint8_t level = d->mPendingBri;
    if (on) {
      bri = (level > 0) ? level : (briLast > 0 ? briLast : 128);
    } else {
      bri = 0;
    }
  }

  if (applyCT && d->mPendingColorMode == MATTER_CM_TEMP) {
    byte rgb[3];
    colorCTtoRGB(d->mPendingCT, rgb);
    colPri[0] = rgb[0];
    colPri[1] = rgb[1];
    colPri[2] = rgb[2];
  } else if (applyHS && d->mPendingColorMode != MATTER_CM_TEMP) {
    // Matter hue:  0-254  → WLED hue: 0-65535
    uint16_t wledHue = (uint16_t)(((uint32_t)d->mPendingHue * 65535U) / 254U);
    // Matter sat:  0-254  → WLED sat: 0-255
    byte wledSat = (d->mPendingSat >= 254) ? 255
                 : (byte)(((uint16_t)d->mPendingSat * 255U) / 254U);
    byte rgb[3];
    colorHStoRGB(wledHue, wledSat, rgb);
    colPri[0] = rgb[0];
    colPri[1] = rgb[1];
    colPri[2] = rgb[2];
  }

  d->mLastApplyMs = millis();
  colorUpdated(CALL_MODE_DIRECT_CHANGE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Push WLED state changes (from UI / API / presets) back to Matter
// ─────────────────────────────────────────────────────────────────────────────

static void syncToMatter(MatterUsermod::Impl *d)
{
  bool curOn = (bri > 0);
  if (curOn == d->mPrevOn && bri == d->mPrevBri && memcmp(colPri, d->mPrevCol, 4) == 0)
    return; // nothing changed

  d->mPrevOn  = curOn;
  d->mPrevBri = bri;
  memcpy(d->mPrevCol, colPri, 4);

  d->mSyncing = true;  // suppress _attrCb re-entry while we push updates
  esp_matter_attr_val_t val;

  // On/Off
  val = esp_matter_bool(curOn);
  esp_matter::attribute::update(d->mEndpointId, MATTER_CL_ON_OFF,
                                MATTER_AT_ON_OFF, &val);

  // Level
  val = esp_matter_nullable_uint8(curOn ? bri : (uint8_t)0);
  esp_matter::attribute::update(d->mEndpointId, MATTER_CL_LEVEL_CTRL,
                                MATTER_AT_CURRENT_LEVEL, &val);

  // Color – convert current RGB to Matter hue/saturation (0-254 range).
  byte r = colPri[0], g = colPri[1], b = colPri[2];
  byte maxC = max(r, max(g, b));
  byte minC = min(r, min(g, b));
  uint8_t matterSat = (maxC == 0) ? 0
                    : (uint8_t)(((uint16_t)(maxC - minC) * 254U) / maxC);

  uint8_t matterHue = 0;
  if (maxC != minC) {
    float delta = (float)(maxC - minC);
    float hf;
    if      (r == maxC) hf = fmodf((float)(g - b) / delta, 6.0f);
    else if (g == maxC) hf = ((float)(b - r) / delta) + 2.0f;
    else                hf = ((float)(r - g) / delta) + 4.0f;
    if (hf < 0.0f) hf += 6.0f;
    uint16_t raw = (uint16_t)(hf / 6.0f * 254.0f + 0.5f);
    matterHue = (raw > 254) ? 254 : (uint8_t)raw;
  }

  val = esp_matter_nullable_uint8(matterHue);
  esp_matter::attribute::update(d->mEndpointId, MATTER_CL_COLOR_CTRL,
                                MATTER_AT_CURRENT_HUE, &val);

  val = esp_matter_nullable_uint8(matterSat);
  esp_matter::attribute::update(d->mEndpointId, MATTER_CL_COLOR_CTRL,
                                MATTER_AT_CURRENT_SAT, &val);

  d->mSyncing = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// MatterUsermod public interface
// ─────────────────────────────────────────────────────────────────────────────

MatterUsermod::MatterUsermod()
  : Usermod(), pImpl(new Impl())
{
  _implInstance = pImpl;
}

MatterUsermod::~MatterUsermod()
{
  if (pImpl == _implInstance) _implInstance = nullptr;
  delete pImpl;
}

void MatterUsermod::setup()
{
  // Build the Matter data model (node + endpoint) but do NOT start the stack
  // yet.  Starting Matter here would cause its internal WiFi driver to take
  // over and destroy WLED's softAP — preventing the user from ever entering
  // WiFi credentials.  The stack is started in connected() once WLED has
  // successfully joined an AP as a STA client.

  // Create Matter node
  esp_matter::node::config_t nodeCfg;
  pImpl->mNode = esp_matter::node::create(&nodeCfg, _attrCb, _identifyCb);
  if (!pImpl->mNode) {
    DEBUG_PRINTLN(F("Matter: node creation failed"));
    return;
  }

  // Create Extended Color Light endpoint
  esp_matter::endpoint::extended_color_light::config_t lightCfg;
  lightCfg.on_off.on_off                     = (bri > 0);
  lightCfg.level_control.current_level       = bri;
  lightCfg.color_control.color_mode          = MATTER_CM_HS;
  lightCfg.color_control.enhanced_color_mode = MATTER_CM_HS;
  lightCfg.color_control.hue_saturation.current_hue        = 0;
  lightCfg.color_control.hue_saturation.current_saturation = 0;

  pImpl->mEndpoint = esp_matter::endpoint::extended_color_light::create(
      pImpl->mNode, &lightCfg, esp_matter::endpoint_flags::ENDPOINT_FLAG_NONE, nullptr);
  if (!pImpl->mEndpoint) {
    DEBUG_PRINTLN(F("Matter: endpoint creation failed"));
    return;
  }
  pImpl->mEndpointId = esp_matter::endpoint::get_id(pImpl->mEndpoint);

  // extended_color_light::create() only adds color_temperature + xy features.
  // Add hue_saturation explicitly so that color_capabilities gets bit 0x01 set
  // and Google Home (and other controllers) expose RGB colour controls.
  esp_matter::cluster_t *cc = esp_matter::cluster::get(
      pImpl->mEndpoint, MATTER_CL_COLOR_CTRL);
  if (cc) {
    esp_matter::cluster::color_control::feature::hue_saturation::config_t hsCfg;
    hsCfg.current_hue        = 0;
    hsCfg.current_saturation = 0;
    esp_matter::cluster::color_control::feature::hue_saturation::add(cc, &hsCfg);
    DEBUG_PRINTLN(F("Matter: hue_saturation feature added to color_control cluster"));
  } else {
    DEBUG_PRINTLN(F("Matter: WARNING - could not find color_control cluster"));
  }

  pImpl->mNodeReady = true;
  DEBUG_PRINTLN(F("Matter: node ready"));

  // If WiFi credentials are already configured, start Matter immediately so
  // its WiFi driver owns the netif from the first connection attempt.
  // If no credentials exist (AP mode needed), defer start to connected() so
  // we don't destroy WLED's softAP before the user can enter credentials.
  if (isWiFiConfigured()) {
    DEBUG_PRINTLN(F("Matter: WiFi configured, starting stack now"));
    externalWiFiManager = true;
    esp_err_t err = esp_matter::start(_eventCb);
    if (err != ESP_OK) {
      DEBUG_PRINTF("Matter: start failed (0x%x)\n", err);
      return;
    }
    pImpl->mStarted = true;
    DEBUG_PRINTLN(F("Matter: started"));
    DEBUG_PRINTF("Matter: passcode %lu  discriminator %u\n",
                (unsigned long)pImpl->mPasscode, pImpl->mDiscriminator);
  } else {
    DEBUG_PRINTLN(F("Matter: no WiFi credentials, deferring start to connected()"));
  }
}

void MatterUsermod::loop()
{
  if (!pImpl->mStarted) return;
  applyPending(pImpl);
  syncToMatter(pImpl);
}

void MatterUsermod::connected()
{
  // Called by WLED after it successfully joins a WiFi AP as a STA client.
  // Only relevant when there were no WiFi credentials at setup() time (first
  // boot / AP mode). In that case, start Matter now that WiFi is up.
  if (pImpl->mStarted || !pImpl->mNodeReady) return;

  DEBUG_PRINTLN(F("Matter: connected() - starting stack (deferred)"));
  externalWiFiManager = true;
  esp_err_t err = esp_matter::start(_eventCb);
  if (err != ESP_OK) {
    DEBUG_PRINTF("Matter: start failed (0x%x)\n", err);
    return;
  }
  pImpl->mStarted = true;
  DEBUG_PRINTLN(F("Matter: started (deferred)"));
  DEBUG_PRINTF("Matter: passcode %lu  discriminator %u\n",
              (unsigned long)pImpl->mPasscode, pImpl->mDiscriminator);
}

uint16_t MatterUsermod::getId()
{
  return USERMOD_ID_MATTER;
}

void MatterUsermod::addToJsonInfo(JsonObject &obj)
{
  // The WLED info page JS iterates obj["u"] only — each entry must be a
  // named array: [value] or [value, " unit"]
  JsonObject user = obj[F("u")];
  if (user.isNull()) user = obj.createNestedObject(F("u"));

  // Status
  JsonArray statusArr = user.createNestedArray(F("Matter"));
  statusArr.add(pImpl->mStarted ? F("Running") : F("Not started"));

  // 11-digit manual pairing / "setup" code
  char manual[13];
  pImpl->buildManualCode(manual, sizeof(manual));
  JsonArray codeArr = user.createNestedArray(F("Matter pairing code"));
  codeArr.add(manual);

  // QR code as a clickable link that opens the rendered image in a new tab
  char qr[28];
  pImpl->buildQRPayload(qr, sizeof(qr));
  char url[128];
  snprintf(url, sizeof(url),
           "https://api.qrserver.com/v1/create-qr-code/?size=300x300&data=%s", qr);
  char link[160];
  snprintf(link, sizeof(link),
           "<a href=\"%s\" target=\"_blank\">%s</a>", url, qr);
  JsonArray qrArr = user.createNestedArray(F("Matter QR code"));
  qrArr.add(link);
}

void MatterUsermod::addToConfig(JsonObject &obj)
{
  JsonObject top = obj.createNestedObject(F("Matter"));
  top[F("passcode")]      = pImpl->mPasscode;
  top[F("discriminator")] = pImpl->mDiscriminator;
}

bool MatterUsermod::readFromConfig(JsonObject &obj)
{
  JsonObject top = obj[F("Matter")];
  if (top.isNull()) return false;
  pImpl->mPasscode      = top[F("passcode")]      | 20202021;
  pImpl->mDiscriminator = top[F("discriminator")] | 3840;
  return !top.isNull();
}

// Instance and registration are handled in wled00/usermods_list.cpp
// so they live in the always-linked wled00 translation unit.

