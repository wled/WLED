/*
 * Zigbee RGB Light Usermod for ESP32-C6
 *
 * Exposes WLED as a Zigbee HA Color Dimmable Light, allowing
 * Zigbee coordinators (e.g. Philips Hue Bridge v2, Zigbee2MQTT, ZHA)
 * to control power, brightness, and color (CIE XY and Hue/Saturation).
 *
 * Bidirectional state sync:
 *   - Coordinator -> WLED: raw ZCL command parsing (on/off, level, color XY/HS)
 *   - WLED -> Coordinator: Zigbee attribute reporting (cache update via scheduler_alarm)
 *
 * Requires:
 *   - ESP32-C6 target (native 802.15.4 radio)
 *   - esp-zigbee-lib and esp-zboss-lib (bundled in arduino-esp32 framework)
 *   - Define USERMOD_ZIGBEE_RGB_LIGHT in build_flags; included via usermods_list.cpp
 */

#ifdef CONFIG_IDF_TARGET_ESP32C6

#include "wled.h"

#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "aps/esp_zigbee_aps.h"
#include "esp_coexist.h"
#include "esp_ieee802154.h"
#include "esp_coex_i154.h"

// ZBOSS internal headers — needed for raw command handler to intercept
// manufacturer-specific commands that crash the stack (issue #681) and
// to parse ZCL command payloads directly (move_to_color, move_to_level, etc.).
// These are pure C headers with no extern "C" guards, so we wrap them.
// zb_zcl_common.h references zb_af_endpoint_desc_t in a few function
// declarations near the end of the file; the actual struct definition
// lives in AF headers that are not shipped with the PlatformIO Arduino
// framework.  A forward declaration is enough to satisfy the compiler.
extern "C" {
  struct zb_af_endpoint_desc_s;
  typedef struct zb_af_endpoint_desc_s zb_af_endpoint_desc_t;
  #include "zboss_api_buf.h"
  #include "zcl/zb_zcl_common.h"
  #include "zcl/zb_zcl_color_control.h"
  #include "zcl/zb_zcl_level_control.h"

  // From zboss_api_zdo.h — can't include the full header due to missing
  // types in the PlatformIO Arduino framework.  We only need the poll
  // interval management (PIM) functions for End Device polling.
  void zb_zdo_pim_set_long_poll_interval(zb_time_t ms);
  void zb_zdo_pim_toggle_turbo_poll_retry_feature(zb_bool_t enable);

  // From zboss_api_zcl.h — can't include the full header due to
  // additional missing types in the PlatformIO Arduino framework.
  zb_bool_t zb_zcl_send_default_handler(zb_uint8_t param,
    const zb_zcl_parsed_hdr_t *cmd_info, zb_zcl_status_t status);
}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Default Zigbee endpoint number for the light
#ifndef ZIGBEE_RGB_LIGHT_ENDPOINT
  #define ZIGBEE_RGB_LIGHT_ENDPOINT 10
#endif

// Stack size for the Zigbee FreeRTOS task (in bytes).
// The WiFi+Zigbee gateway example from Espressif uses 8192; however, scheduler
// alarm callbacks (used for attribute reporting) run on this same stack and
// need additional headroom for ZCL frame construction and NWK encryption.
#ifndef ZIGBEE_TASK_STACK_SIZE
  #define ZIGBEE_TASK_STACK_SIZE 16384
#endif

// Priority for the Zigbee FreeRTOS task
#ifndef ZIGBEE_TASK_PRIORITY
  #define ZIGBEE_TASK_PRIORITY 5
#endif

/* ---------------------------------------------------------------------------
 *  Zigbee signal handler (extern "C" – called by the Zigbee stack)
 *  Forward-declare the usermod class so the signal handler can access instance.
 * -------------------------------------------------------------------------*/
class ZigbeeRGBLightUsermod;
static ZigbeeRGBLightUsermod *s_zb_instance = nullptr;  // set in constructor
static volatile bool *s_zb_paired_flag = nullptr;        // points to instance->zbPaired
static char *s_zb_eui64_str = nullptr;                   // points to instance->eui64Str
static uint16_t s_zb_parent_short = 0xFFFF;              // parent short address for report destination
static uint16_t s_zb_my_short     = 0xFFFF;              // our own short address (cached for safe cross-task reads)
static volatile int s_zb_binds_completed = 0;            // how many bind responses received
static const int    s_zb_binds_needed    = 3;            // on_off, level, color

// Helper callback for esp_zb_scheduler_alarm — wraps commissioning start
// (needed because esp_zb_bdb_start_top_level_commissioning has a different
// signature than what esp_zb_scheduler_alarm expects).
static void zbStartCommissioning(uint8_t mode_mask)
{
  esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

// Forward declarations for functions used with esp_zb_scheduler_alarm
// and esp_zb_raw_command_handler_register — these must have C linkage-compatible
// signatures.  Implemented after the class definition.
// Note: non-static because they are friends of ZigbeeRGBLightUsermod.
void zbPollAttributesCallback(uint8_t param);
bool zb_raw_command_handler(uint8_t bufid);
void zbUpdateState(bool power, uint8_t bri, uint16_t colorX, uint16_t colorY, bool useXY);
void zbConfigureAllReporting(uint8_t unused);
void zbReadCurrentOnOffLevel(bool &power, uint8_t &level);
void zbReadCurrentColorXY(uint16_t &colorX, uint16_t &colorY);
void zbReportStateViaScheduler(uint8_t unused);

extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
  uint32_t *p_sg_p       = signal_struct->p_app_signal;
  esp_err_t err_status   = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;

  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
      ESP_LOGD("ZigbeeRGB", "Production config: %s",
               (err_status == ESP_OK) ? "loaded" : "not found (normal)");
      break;
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGI("ZigbeeRGB", "Initialize Zigbee stack");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      // Read EUI64 here — the stack has loaded the IEEE address by this point.
      if (s_zb_eui64_str) {
        esp_zb_ieee_addr_t eui64;
        esp_zb_get_long_address(eui64);
        snprintf(s_zb_eui64_str, 24,
                 "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                 eui64[7], eui64[6], eui64[5], eui64[4],
                 eui64[3], eui64[2], eui64[1], eui64[0]);
        ESP_LOGI("ZigbeeRGB", "EUI64: %s", s_zb_eui64_str);
      }
      if (err_status == ESP_OK) {
        ESP_LOGI("ZigbeeRGB", "Device started up in%s factory-reset mode",
                 esp_zb_bdb_is_factory_new() ? "" : " non");
        if (esp_zb_bdb_is_factory_new()) {
          ESP_LOGI("ZigbeeRGB", "Starting network steering");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
          ESP_LOGI("ZigbeeRGB", "Device rebooted (already commissioned)");
          if (s_zb_paired_flag) *s_zb_paired_flag = true;
          // Configure attribute reporting after a brief delay
          esp_zb_scheduler_alarm(zbConfigureAllReporting, 0, 2000);
        }
      } else {
        ESP_LOGW("ZigbeeRGB", "BDB init failed (status: 0x%x), retrying...", err_status);
        esp_zb_scheduler_alarm(zbStartCommissioning,
                               ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      if (err_status == ESP_OK) {
        esp_zb_ieee_addr_t extended_pan_id;
        esp_zb_get_extended_pan_id(extended_pan_id);
        ESP_LOGI("ZigbeeRGB",
                 "Joined network! Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, "
                 "PAN ID: 0x%04hx, Channel: %d, Short Address: 0x%04hx",
                 extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                 extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                 esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        if (s_zb_paired_flag) *s_zb_paired_flag = true;
        // Configure attribute reporting after a brief delay
        esp_zb_scheduler_alarm(zbConfigureAllReporting, 0, 2000);
      } else {
        ESP_LOGI("ZigbeeRGB", "Network steering failed (status: 0x%x), will retry in 5s",
                 err_status);
        esp_zb_scheduler_alarm(zbStartCommissioning,
                               ESP_ZB_BDB_MODE_NETWORK_STEERING, 5000);
      }
      break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
      if (err_status == ESP_OK) {
        uint8_t *permit_duration = (uint8_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (permit_duration && *permit_duration) {
          ESP_LOGD("ZigbeeRGB", "Network(0x%04hx) open for joining (%d seconds)",
                   esp_zb_get_pan_id(), *permit_duration);
        } else {
          ESP_LOGD("ZigbeeRGB", "Network(0x%04hx) closed for joining",
                   esp_zb_get_pan_id());
        }
      }
      break;
    default:
      ESP_LOGD("ZigbeeRGB", "Zigbee signal: 0x%x, status: 0x%x", sig_type, err_status);
      break;
  }
}

/* ---------------------------------------------------------------------------
 *  Usermod class
 * -------------------------------------------------------------------------*/
class ZigbeeRGBLightUsermod : public Usermod {
private:
  bool enabled      = true;
  bool initDone     = false;
  bool zbStarted    = false;  // true once the Zigbee task has been launched
  bool zbPaired     = false;  // true once successfully joined a network
  TaskHandle_t zbTaskHandle = nullptr;

  // EUI64 — populated after esp_zb_start() in the Zigbee task
  char eui64Str[24]    = {};  // EUI64 as XX:XX:XX:XX:XX:XX:XX:XX

  // Diagnostic counters for WLED→Zigbee reporting (visible in /json/info)
  volatile uint32_t reportTriggerCount  = 0;  // onStateChange calls (non-Zigbee)
  volatile uint32_t reportSchedulerCount = 0; // scheduler callback invocations

  // Mutex protecting shared state between the Zigbee task and loop()
  SemaphoreHandle_t zbStateMutex = nullptr;

  // Zigbee→WLED state — written from the Zigbee task, read in loop()
  volatile bool     stateChanged  = false;
  volatile bool     powerOn       = true;
  volatile uint8_t  zbBrightness  = 254;
  // Support both HS and XY color modes (Hue uses XY, others may use HS)
  volatile uint8_t  zbHue         = 0;
  volatile uint8_t  zbSaturation  = 254;
  volatile uint16_t zbColorX      = 0x616B;  // default ~white in CIE 1931
  volatile uint16_t zbColorY      = 0x607D;
  volatile bool     zbUseXY       = true;    // true = last color write was XY

  // WLED→Zigbee state — written from onStateChange(), read by Zigbee task callback
  volatile bool     reportPending  = false;
  volatile bool     reportPowerOn  = true;
  volatile uint8_t  reportBri      = 254;
  volatile uint16_t reportColorX   = 0x616B;
  volatile uint16_t reportColorY   = 0x607D;

  // Flash-string keys
  static const char _name[];
  static const char _enabled[];

  // Friend declarations for file-scope Zigbee callbacks that need private access
  friend void zbUpdateState(bool, uint8_t, uint16_t, uint16_t, bool);
  friend void zbReadCurrentOnOffLevel(bool &, uint8_t &);
  friend void zbReadCurrentColorXY(uint16_t &, uint16_t &);
  friend void zbPollAttributesCallback(uint8_t);
  friend void zbConfigureAllReporting(uint8_t);
  friend void zbReportStateViaScheduler(uint8_t);
  friend bool zb_raw_command_handler(uint8_t);

  /* -----------------------------------------------------------------------
   *  Static Zigbee action callback (dispatches to instance method)
   * ---------------------------------------------------------------------*/
  static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                     const void *message)
  {
    if (!s_zb_instance) return ESP_ERR_INVALID_STATE;
    if (callback_id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
      return s_zb_instance->handleAttributeSet(
        static_cast<const esp_zb_zcl_set_attr_value_message_t *>(message));
    }
    return ESP_OK;
  }

  /* -----------------------------------------------------------------------
   *  Zigbee attribute reporting (WLED→coordinator).
   *
   *  When WLED state changes from a non-Zigbee source (web UI, MQTT, etc.),
   *  onStateChange() stores the new state and schedules a callback via
   *  esp_zb_scheduler_alarm().  The callback (zbReportStateViaScheduler)
   *  runs inside the Zigbee task and calls esp_zb_zcl_set_attribute_val()
   *  to update the attribute cache.  The ZBOSS reporting engine then
   *  automatically sends attribute reports to bound coordinators.
   *
   *  This works with Home Assistant (ZHA), Zigbee2MQTT, and other
   *  standards-compliant coordinators.
   *
   *  NOTE: The Philips Hue Bridge does NOT update its internal state from
   *  unsolicited attribute reports (tested with both ZBOSS auto-reporting
   *  and explicit report_attr_cmd_req).  The Hue bridge only tracks state
   *  from commands it sends.  This is a known Hue limitation for
   *  non-certified devices — WLED→Hue state sync is not possible via
   *  Zigbee alone.
   * ---------------------------------------------------------------------*/

  // Called from onStateChange — stores pending state for the scheduler callback
  void prepareReport(bool power, uint8_t bri, uint16_t x, uint16_t y)
  {
    if (zbStateMutex && xSemaphoreTake(zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      reportPowerOn = power;
      reportBri     = bri;
      reportColorX  = x;
      reportColorY  = y;
      reportPending = true;
      xSemaphoreGive(zbStateMutex);
    }
  }

  /* -----------------------------------------------------------------------
   *  Handle incoming attribute writes from the Zigbee coordinator
   * ---------------------------------------------------------------------*/
  esp_err_t handleAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message)
  {
    if (!message || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
      return ESP_ERR_INVALID_ARG;
    }

    uint16_t cluster = message->info.cluster;
    uint16_t attrId  = message->attribute.id;

    ESP_LOGD("ZigbeeRGB", "Attr set: endpoint(%d) cluster(0x%04x) attr(0x%04x) size(%d)",
             message->info.dst_endpoint, cluster, attrId, message->attribute.data.size);

    if (zbStateMutex && xSemaphoreTake(zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      switch (cluster) {

        case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
          if (attrId == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            powerOn = *(const bool *)message->attribute.data.value;
            ESP_LOGD("ZigbeeRGB", "Power: %s", powerOn ? "ON" : "OFF");
            stateChanged = true;
          }
          break;

        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
          if (attrId == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
            zbBrightness = *(const uint8_t *)message->attribute.data.value;
            ESP_LOGD("ZigbeeRGB", "Level: %d", zbBrightness);
            stateChanged = true;
          }
          break;

        case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
          if (attrId == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID) {
            zbHue = *(const uint8_t *)message->attribute.data.value;
            zbUseXY = false;
            ESP_LOGD("ZigbeeRGB", "Hue: %d", zbHue);
            stateChanged = true;
          } else if (attrId == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID) {
            zbSaturation = *(const uint8_t *)message->attribute.data.value;
            zbUseXY = false;
            ESP_LOGD("ZigbeeRGB", "Saturation: %d", zbSaturation);
            stateChanged = true;
          } else if (attrId == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID) {
            zbColorX = *(const uint16_t *)message->attribute.data.value;
            zbUseXY = true;
            ESP_LOGD("ZigbeeRGB", "Color X: 0x%04x", (unsigned)zbColorX);
            stateChanged = true;
          } else if (attrId == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID) {
            zbColorY = *(const uint16_t *)message->attribute.data.value;
            zbUseXY = true;
            ESP_LOGD("ZigbeeRGB", "Color Y: 0x%04x", (unsigned)zbColorY);
            stateChanged = true;
          }
          break;

        default:
          break;
      }
      xSemaphoreGive(zbStateMutex);
    }
    return ESP_OK;
  }

  /* -----------------------------------------------------------------------
   *  CIE 1931 XY to RGB conversion (chromaticity only).
   *  ZCL color X/Y are 16-bit unsigned, scaled so that 1.0 = 0xFEFF (65279).
   *
   *  This extracts the pure color (hue + saturation) from the CIE xy
   *  coordinates.  Brightness is NOT baked in — WLED's global `bri` handles
   *  dimming separately.  Using a fixed Y=1.0 for the XYZ conversion
   *  avoids subtle hue shifts that occur when clamping at low luminance
   *  (where negative linear-RGB values get clipped asymmetrically).
   * ---------------------------------------------------------------------*/
  static void xyToRGB(uint16_t zclX, uint16_t zclY,
                      uint8_t &r, uint8_t &g, uint8_t &b)
  {
    // Convert ZCL 16-bit to float CIE xy (0..1)
    float x = static_cast<float>(zclX) / 65279.0f;
    float y = static_cast<float>(zclY) / 65279.0f;
    if (y < 0.001f) y = 0.001f; // avoid division by zero

    // Convert CIE xy to XYZ with Y=1.0 (full luminance, chromaticity only)
    float Y = 1.0f;
    float X = (Y / y) * x;
    float Z = (Y / y) * (1.0f - x - y);

    // XYZ to sRGB (D65 wide-gamut matrix, same as Hue uses)
    float rf =  X * 3.2404542f + Y * -1.5371385f + Z * -0.4985314f;
    float gf =  X * -0.9692660f + Y * 1.8760108f + Z * 0.0415560f;
    float bf =  X * 0.0556434f + Y * -0.2040259f + Z * 1.0572252f;

    // Clamp negative values (out-of-gamut)
    auto clamp01 = [](float v) -> float { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    rf = clamp01(rf);
    gf = clamp01(gf);
    bf = clamp01(bf);

    // Linear → sRGB gamma
    auto reverseGamma = [](float v) -> float {
      return v <= 0.0031308f ? 12.92f * v : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    };
    rf = reverseGamma(rf);
    gf = reverseGamma(gf);
    bf = reverseGamma(bf);

    // Normalize so the brightest channel reaches 255 (preserving color ratios).
    // Brightness is handled by WLED's global bri, not here.
    float maxC = rf;
    if (gf > maxC) maxC = gf;
    if (bf > maxC) maxC = bf;
    if (maxC > 0.001f) {
      float scale = 255.0f / maxC;
      r = static_cast<uint8_t>(rf * scale + 0.5f);
      g = static_cast<uint8_t>(gf * scale + 0.5f);
      b = static_cast<uint8_t>(bf * scale + 0.5f);
    } else {
      r = g = b = 0;
    }
  }

  /* -----------------------------------------------------------------------
   *  RGB to CIE 1931 XY conversion (for reporting WLED state to Zigbee)
   * ---------------------------------------------------------------------*/
  static void rgbToXY(uint8_t r, uint8_t g, uint8_t b,
                      uint16_t &zclX, uint16_t &zclY)
  {
    // sRGB gamma → linear
    auto applyGamma = [](float v) -> float {
      v /= 255.0f;
      return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
    };
    float rf = applyGamma(static_cast<float>(r));
    float gf = applyGamma(static_cast<float>(g));
    float bf = applyGamma(static_cast<float>(b));

    // sRGB to XYZ (D65)
    float X = rf * 0.4124564f + gf * 0.3575761f + bf * 0.1804375f;
    float Y = rf * 0.2126729f + gf * 0.7151522f + bf * 0.0721750f;
    float Z = rf * 0.0193339f + gf * 0.1191920f + bf * 0.9503041f;

    float sum = X + Y + Z;
    if (sum < 0.001f) {
      // Black — use D65 white point
      zclX = static_cast<uint16_t>(0.3127f * 65279.0f + 0.5f);
      zclY = static_cast<uint16_t>(0.3290f * 65279.0f + 0.5f);
      return;
    }
    float cx = X / sum;
    float cy = Y / sum;

    zclX = static_cast<uint16_t>(cx * 65279.0f + 0.5f);
    zclY = static_cast<uint16_t>(cy * 65279.0f + 0.5f);
  }

  /* -----------------------------------------------------------------------
   *  Apply the cached Zigbee state to the WLED engine
   * ---------------------------------------------------------------------*/
  void applyState()
  {
    // Snapshot shared state under the mutex
    bool     localPower;
    uint8_t  localBri, localHue, localSat;
    uint16_t localX, localY;
    bool     localUseXY;
    if (zbStateMutex && xSemaphoreTake(zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localPower = powerOn;
      localBri   = zbBrightness;
      localHue   = zbHue;
      localSat   = zbSaturation;
      localX     = zbColorX;
      localY     = zbColorY;
      localUseXY = zbUseXY;
      xSemaphoreGive(zbStateMutex);
    } else {
      return; // could not acquire mutex, try again next loop
    }

    if (!localPower) {
      bri = 0;
    } else {
      bri = localBri;

      if (localUseXY) {
        // XY color mode (used by Hue bridge) — extract chromaticity only,
        // WLED global bri handles dimming.
        uint8_t r, g, b;
        xyToRGB(localX, localY, r, g, b);
        colPri[0] = r;
        colPri[1] = g;
        colPri[2] = b;
        colPri[3] = 0;
      } else {
        // HS color mode
        // Zigbee hue is 0-254 → WLED hue is 0-65535
        uint16_t wledHue = static_cast<uint16_t>((static_cast<uint32_t>(localHue) * 65535U) / 254U);
        // Zigbee saturation is 0-254 → WLED saturation is 0-255
        uint8_t  wledSat = static_cast<uint8_t>((static_cast<uint16_t>(localSat) * 255U) / 254U);

        byte rgb[3];
        colorHStoRGB(wledHue, wledSat, rgb);

        colPri[0] = rgb[0];
        colPri[1] = rgb[1];
        colPri[2] = rgb[2];
        colPri[3] = 0; // no white channel
      }
    }
    colorUpdated(CALL_MODE_ZIGBEE);
  }

  /* -----------------------------------------------------------------------
   *  FreeRTOS task — runs the Zigbee stack
   *  Note: esp_zb_platform_config() is called in setup() before this task
   *  is created, matching the pattern in Espressif's IDF examples.
   *
   *  This task is started from connected() (after WiFi STA gets an IP)
   *  so that the WLED-AP remains fully functional for initial WiFi setup
   *  on fresh/unconfigured devices.
   *
   *  CRITICAL fixes for Philips Hue Bridge v2 pairing (discovered via
   *  standalone IDF test at ~/esp/zigbee_test/):
   *    - End Device mode (Hue rejects Router joins with ZDO Leave)
   *    - Distributed security (Hue uses distributed, NOT centralized TC)
   *    - ZLL distributed link key
   *    - app_device_version = 1 (required by Hue)
   *    - Extra on_off attributes (global_scene_control, on_time, off_wait_time)
   *    - Minimum join LQI = 0 (PCB antenna gives low LQI values)
   *    - rx_on_when_idle = true (mains-powered, non-sleepy)
   * ---------------------------------------------------------------------*/

  // ZLL (ZigBee Light Link) preconfigured distributed link key
  // Well-known key used by Hue bridge for distributed security
  static constexpr uint8_t zll_distributed_key[16] = {
    0x81, 0x42, 0x86, 0x86, 0x5D, 0xC1, 0xC8, 0xB2,
    0xC8, 0xCB, 0xC5, 0x2E, 0x5D, 0x65, 0xD1, 0xB8
  };

  static void zigbeeTask(void *pvParameters)
  {
    // Increase IO buffer pool and scheduler queue for better throughput
    // under WiFi coexistence pressure.  Must be called BEFORE esp_zb_init().
    esp_zb_io_buffer_size_set(128);        // default 80
    esp_zb_scheduler_queue_size_set(128);  // default 80
    ESP_LOGD("ZigbeeRGB", "IO buffer pool and scheduler queue set to 128");

    // Initialise the Zigbee stack as End Device
    // (Hue bridge rejects Router joins with ZDO Leave)
    esp_zb_cfg_t zb_nwk_cfg = {};
    zb_nwk_cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ED;
    zb_nwk_cfg.install_code_policy = false;
    zb_nwk_cfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    zb_nwk_cfg.nwk_cfg.zed_cfg.keep_alive = 3000;
    esp_zb_init(&zb_nwk_cfg);

    // CRITICAL: Enable joining to distributed security networks.
    // The Hue bridge uses distributed security (NOT centralized TC).
    // Without this, the device tries centralized TC key exchange which
    // will never work, causing "Have not got nwk key - authentication failed".
    esp_zb_enable_joining_to_distributed(true);
    ESP_LOGD("ZigbeeRGB", "Enabled joining to distributed security networks");

    // Set the ZLL distributed link key (well-known key used by Hue bridge)
    esp_zb_secur_TC_standard_distributed_key_set(const_cast<uint8_t *>(zll_distributed_key));
    ESP_LOGD("ZigbeeRGB", "Set ZLL distributed link key");

    // Set rx_on_when_idle = true (non-sleepy, mains-powered light)
    esp_zb_set_rx_on_when_idle(true);
    ESP_LOGD("ZigbeeRGB", "Set rx_on_when_idle = true (non-sleepy ED)");

    // Set minimum LQI to 0 -- the PCB antenna produces very low LQI values
    // (0-30) even at 1m distance, causing all beacons to be rejected.
    esp_zb_secur_network_min_join_lqi_set(0);
    ESP_LOGD("ZigbeeRGB", "Set minimum join LQI to 0");

    // --- Create the Color Dimmable Light endpoint ---
    // Use the color_dimmable_light helper for cluster creation, then manually
    // add the endpoint with app_device_version = 1 (required by Hue bridge).
    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    light_cfg.basic_cfg.power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    // Color capabilities: HS (bit 0) + XY (bit 3) = 0x0009
    light_cfg.color_cfg.color_capabilities = 0x0009;

    // Create cluster list from the helper (but NOT the endpoint yet)
    esp_zb_cluster_list_t *cluster_list = esp_zb_color_dimmable_light_clusters_create(&light_cfg);

    // --- Add extra on_off attributes required by Hue bridge ---
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_cluster_list_get_cluster(
      cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    if (on_off_cluster) {
      bool global_scene_control = ESP_ZB_ZCL_ON_OFF_GLOBAL_SCENE_CONTROL_DEFAULT_VALUE;
      uint16_t on_time = ESP_ZB_ZCL_ON_OFF_ON_TIME_DEFAULT_VALUE;
      uint16_t off_wait_time = ESP_ZB_ZCL_ON_OFF_OFF_WAIT_TIME_DEFAULT_VALUE;

      esp_zb_on_off_cluster_add_attr(on_off_cluster,
        ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL, &global_scene_control);
      esp_zb_on_off_cluster_add_attr(on_off_cluster,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME, &on_time);
      esp_zb_on_off_cluster_add_attr(on_off_cluster,
        ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME, &off_wait_time);
      ESP_LOGD("ZigbeeRGB", "Added extra on_off attributes");
    }

    // --- Add manufacturer info to Basic cluster ---
    esp_zb_attribute_list_t *basic_cluster = esp_zb_cluster_list_get_cluster(
      cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    if (basic_cluster) {
      // ZCL string format: first byte is the length, followed by the characters
      char manufacturer_name[] = "\x04" "WLED";
      char model_identifier[]  = "\x11" "WLED-Zigbee-Light";
      char sw_build_id[]       = "\x05" "0.1.0";
      uint8_t app_version = 1;
      uint8_t stack_version = 1;
      uint8_t hw_version = 1;

      esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer_name);
      esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model_identifier);
      esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, sw_build_id);
      esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &app_version);
      esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_version);
      esp_zb_basic_cluster_add_attr(basic_cluster,
        ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hw_version);
    }

    // --- Create endpoint with app_device_version = 1 ---
    // The Hue bridge requires app_device_version = 1. The default helper
    // function sets it to 0, so we manually create the endpoint.
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
      .endpoint = ZIGBEE_RGB_LIGHT_ENDPOINT,
      .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
      .app_device_version = 1,  // Hue bridge requires version 1
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    ESP_LOGD("ZigbeeRGB", "Created endpoint %d with app_device_version=1, device_id=0x%04x",
             ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID);

    esp_zb_device_register(ep_list);

    // --- Ensure reportable attributes have ESP_ZB_ZCL_ATTR_ACCESS_REPORTING ---
    // The cluster helper may not set this flag. Without it,
    // esp_zb_zcl_report_attr_cmd_req() may assert.
    // We patch the access flags AFTER registration so we can use
    // esp_zb_zcl_get_attribute(). This does NOT modify the data model
    // structure (no add/remove), just flips an access bit.
    {
      struct { uint16_t cluster; uint16_t attr; } reportable_attrs[] = {
        { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID },
        { ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,  ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID },
        { ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,  ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID },
        { ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,  ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID },
      };
      for (auto &ra : reportable_attrs) {
        esp_zb_zcl_attr_t *a = esp_zb_zcl_get_attribute(
          ZIGBEE_RGB_LIGHT_ENDPOINT, ra.cluster,
          ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ra.attr);
        if (a) {
          uint8_t old_access = a->access;
          a->access |= ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
          ESP_LOGD("ZigbeeRGB", "Attr cl=0x%04x id=0x%04x access 0x%02x -> 0x%02x",
                   ra.cluster, ra.attr, old_access, a->access);
        } else {
          ESP_LOGW("ZigbeeRGB", "Attr cl=0x%04x id=0x%04x NOT FOUND for reporting flag!",
                   ra.cluster, ra.attr);
        }
      }
    }

    // Register the attribute-write callback
    esp_zb_core_action_handler_register(zb_action_handler);

    // Register the raw command handler to intercept commands that would crash
    // the ZBOSS stack (manufacturer-specific scenes from Hue) or that ZBOSS
    // silently ignores (Off with effect).
    esp_zb_raw_command_handler_register(zb_raw_command_handler);

    // --- Channel mask ---
    // Scan ALL standard Zigbee channels (11-26) to find the Hue bridge
    // regardless of which channel it's operating on.
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    // Start the stack (false = not coordinator)
    ESP_ERROR_CHECK(esp_zb_start(false));

    // --- Configure End Device polling ---
    // The Hue bridge likely uses MAC indirect transmission for ALL End Devices,
    // queueing frames in a pending buffer.  Even though rx_on_when_idle=true,
    // the device must actively poll (send MAC Data Requests) to retrieve
    // queued frames.  Without polling, frames expire after ~7.68s and are
    // silently lost — this was the other cause of intermittent delivery failures.
    //
    // These ZBOSS PIM (Poll Interval Management) APIs must be called after
    // esp_zb_start() because the polling infrastructure is not initialized
    // until the stack is running.
    //
    // Set long poll interval to 1 second (1000ms).  This is how often the ED
    // will send a MAC Data Request to its parent when not in turbo poll mode.
    // For a mains-powered device the power cost is irrelevant.
    zb_zdo_pim_set_long_poll_interval(1000);
    ESP_LOGD("ZigbeeRGB", "Set long poll interval to 1000ms");

    // Enable turbo poll retry — if we get an ACK with pending bit set but
    // no data frame follows, retry the poll a few more times.
    zb_zdo_pim_toggle_turbo_poll_retry_feature(ZB_TRUE);
    ESP_LOGD("ZigbeeRGB", "Enabled turbo poll retry feature");

    // Run the Zigbee main loop — never returns
    esp_zb_stack_main_loop();
    vTaskDelete(nullptr);
  }

public:
  ZigbeeRGBLightUsermod() {
    s_zb_instance    = this;
    s_zb_paired_flag = &zbPaired;
    s_zb_eui64_str   = eui64Str;
  }

  /* ---- Lifecycle ------------------------------------------------------ */

  void setup() override
  {
    if (!enabled) return;

    // Enable 802.15.4 / WiFi coexistence arbitration before esp_wifi_init().
    // WLED calls WiFi.mode() (which triggers esp_wifi_init()) only AFTER
    // UsermodManager::setup() returns, so this is the correct place.
    // NOTE: this must NOT be in the constructor — with REGISTER_USERMOD the
    // static instance is constructed during do_global_ctors, before the IDF
    // coexistence subsystem is initialised, causing a Load access fault.
#if defined(CONFIG_ESP_COEX_SW_COEXIST_ENABLE) || defined(CONFIG_ESP_COEX_ENABLED)
    esp_err_t coex_err = esp_coex_wifi_i154_enable();
    if (coex_err == ESP_OK) {
      ESP_LOGD("ZigbeeRGB", "WiFi/802.15.4 coexistence enabled (before WiFi.mode)");
    } else {
      ESP_LOGE("ZigbeeRGB", "esp_coex_wifi_i154_enable failed: 0x%x", coex_err);
    }

    // Raise 802.15.4 coexistence priority from the default (LOW) to MIDDLE.
    // The default LOW priority means WiFi traffic always wins arbitration,
    // causing incoming Zigbee frames and ACKs to be aborted by the coex
    // arbiter — this was one cause of intermittent command delivery failures.
    // Using MIDDLE (not HIGH) to avoid starving WiFi — HIGH causes UDP send
    // failures and makes the WLED web interface unreachable.
    esp_ieee802154_coex_config_t coex_cfg = {
      .idle    = IEEE802154_IDLE,
      .txrx    = IEEE802154_MIDDLE,   // was IEEE802154_LOW
      .txrx_at = IEEE802154_MIDDLE,   // was IEEE802154_MIDDLE (unchanged)
    };
    esp_ieee802154_set_coex_config(coex_cfg);
    ESP_LOGD("ZigbeeRGB", "802.15.4 coexistence priority set to MIDDLE");
#endif

    zbStateMutex = xSemaphoreCreateMutex();
    if (!zbStateMutex) return;

    // Configure the Zigbee platform (lightweight — no radio activity).
    // The actual Zigbee task is deferred to connected() so that the AP
    // remains fully functional for initial WiFi setup on fresh devices.
    esp_zb_platform_config_t platform_cfg = {};
    platform_cfg.radio_config.radio_mode          = ZB_RADIO_MODE_NATIVE;
    platform_cfg.host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;
    if (esp_zb_platform_config(&platform_cfg) != ESP_OK) {
      DEBUG_PRINTLN(F("ZigbeeRGBLight: esp_zb_platform_config failed"));
      return;
    }

    initDone = true;
    ESP_LOGD("ZigbeeRGB", "Platform configured, Zigbee will start after WiFi STA connects");
  }

  // Called by WLED when the STA interface gets an IP address.
  // This is where we start the Zigbee task — deferring until here ensures
  // the AP works normally for initial WiFi configuration on fresh devices.
  void connected() override
  {
    if (!enabled || !initDone) return;

    // Re-enable coexistence and re-apply 802.15.4 priority on every WiFi
    // (re)connect.  WLED's initConnection() calls WiFi.mode(WIFI_MODE_NULL)
    // which tears down the WiFi stack and may reset both the coex enable
    // flag and the arbiter priority configuration.  Without re-applying,
    // the 802.15.4 priority reverts to LOW (the default), causing WiFi to
    // always win arbitration and starve Zigbee RX/TX — or causing
    // intermittent WiFi drops when the arbiter state is inconsistent.
#if defined(CONFIG_ESP_COEX_SW_COEXIST_ENABLE) || defined(CONFIG_ESP_COEX_ENABLED)
    esp_err_t coex_err = esp_coex_wifi_i154_enable();
    if (coex_err != ESP_OK) {
      ESP_LOGW("ZigbeeRGB", "esp_coex_wifi_i154_enable on reconnect: 0x%x (may already be enabled)", coex_err);
    }
    esp_ieee802154_coex_config_t coex_cfg = {
      .idle    = IEEE802154_IDLE,
      .txrx    = IEEE802154_MIDDLE,
      .txrx_at = IEEE802154_MIDDLE,
    };
    esp_ieee802154_set_coex_config(coex_cfg);
    ESP_LOGD("ZigbeeRGB", "802.15.4 coex re-applied (enable + MIDDLE priority) after WiFi reconnect");
#endif

    if (zbStarted) {
      ESP_LOGD("ZigbeeRGB", "WiFi STA connected (Zigbee already running)");
      return;
    }

    ESP_LOGI("ZigbeeRGB", "WiFi STA connected — starting Zigbee task");
    if (xTaskCreate(
          zigbeeTask,
          "zigbee_rgb",
          ZIGBEE_TASK_STACK_SIZE,
          nullptr,
          ZIGBEE_TASK_PRIORITY,
          &zbTaskHandle
        ) == pdPASS) {
      zbStarted = true;
    } else {
      ESP_LOGE("ZigbeeRGB", "Failed to create Zigbee task");
    }
  }

  void loop() override
  {
    if (!enabled || !zbStarted || strip.isUpdating()) return;

    // Apply Zigbee→WLED state changes
    bool pending = false;
    if (zbStateMutex && xSemaphoreTake(zbStateMutex, 0) == pdTRUE) {
      pending = stateChanged;
      stateChanged = false;
      xSemaphoreGive(zbStateMutex);
    }
    if (pending) {
      applyState();
    }
  }

  /* ---- State change notification -------------------------------------- */

  // Called by WLED when state changes from any source (web UI, MQTT, buttons, etc.)
  void onStateChange(uint8_t mode) override
  {
    if (!enabled || !zbStarted || !zbPaired) return;

    // Avoid echo loops — don't report back changes that came from Zigbee
    if (mode == CALL_MODE_ZIGBEE) return;

    // Convert current WLED state to Zigbee attribute values
    bool    newPower = (bri > 0);
    uint8_t newBri   = bri;
    // Convert RGB to CIE XY for Zigbee (Hue bridge uses XY color space)
    uint16_t newX, newY;
    rgbToXY(colPri[0], colPri[1], colPri[2], newX, newY);

    ESP_LOGD("ZigbeeRGB", "onStateChange mode=%d power=%d bri=%d rgb=(%d,%d,%d) xy=(0x%04x,0x%04x)",
             mode, newPower, newBri, colPri[0], colPri[1], colPri[2], newX, newY);

    reportTriggerCount++;

    // Store pending Zigbee report
    prepareReport(newPower, newBri, newX, newY);

    // Schedule attribute cache update in the Zigbee task context.
    // This is critical: esp_zb_zcl_set_attribute_val() called from within
    // the Zigbee task triggers the ZBOSS internal reporting engine, which
    // is what makes coordinators like ZHA/Home Assistant see the changes.
    // The explicit report_attr_cmd_req approach in sendPendingReports()
    // runs from loop() via esp_zb_lock but may not trigger the same
    // internal reporting path.
    esp_zb_scheduler_alarm(zbReportStateViaScheduler, 0, 10);
  }

  /* ---- Config persistence --------------------------------------------- */

  void addToConfig(JsonObject &root) override
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
  }

  bool readFromConfig(JsonObject &root) override
  {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
    return configComplete;
  }

  /* ---- Info / State --------------------------------------------------- */

  void addToJsonInfo(JsonObject &root) override
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    if (!enabled) {
      JsonArray arr = user.createNestedArray(FPSTR(_name));
      arr.add(F("disabled"));
      return;
    }

    if (zbPaired) {
      JsonArray arr = user.createNestedArray(FPSTR(_name));
      arr.add(F("paired"));

      // Diagnostic info for Zigbee state
      char parentBuf[12];
      snprintf(parentBuf, sizeof(parentBuf), "0x%04X", s_zb_parent_short);
      JsonArray parent_arr = user.createNestedArray(F("ZB Parent"));
      parent_arr.add(parentBuf);

      char bindBuf[16];
      snprintf(bindBuf, sizeof(bindBuf), "%d/%d", s_zb_binds_completed, s_zb_binds_needed);
      JsonArray bind_arr = user.createNestedArray(F("ZB Binds"));
      bind_arr.add(bindBuf);

      char countBuf[24];
      snprintf(countBuf, sizeof(countBuf), "%lu/%lu",
               (unsigned long)reportTriggerCount,
               (unsigned long)reportSchedulerCount);
      JsonArray cnt_arr = user.createNestedArray(F("ZB Reports T/S"));
      cnt_arr.add(countBuf);
    } else {
      // Show EUI64 so the user can identify this device.
      // EUI64 is populated after esp_zb_start() fires in the Zigbee task.
      if (zbStarted) {
        JsonArray eui_arr = user.createNestedArray(F("Zigbee EUI64"));
        eui_arr.add(eui64Str[0] ? eui64Str : "starting...");
      }

      JsonArray status_arr = user.createNestedArray(FPSTR(_name));
      status_arr.add(zbStarted ? F("searching...") : F("waiting for WiFi"));
    }
  }

  uint16_t getId() override
  {
    return USERMOD_ID_ZIGBEE_RGB_LIGHT;
  }
};

/* -- Static member definitions ------------------------------------------ */
const char ZigbeeRGBLightUsermod::_name[]    PROGMEM = "ZigbeeRGBLight";
const char ZigbeeRGBLightUsermod::_enabled[] PROGMEM = "enabled";

/* ---------------------------------------------------------------------------
 *  Scheduler callback: update the Zigbee attribute cache from within the
 *  Zigbee task context.  This is the approach that worked with Home Assistant
 *  (ZHA) — calling esp_zb_zcl_set_attribute_val() from the Zigbee task
 *  triggers the ZBOSS internal reporting engine, which sends attribute
 *  reports to bound coordinators.
 * -------------------------------------------------------------------------*/
void zbReportStateViaScheduler(uint8_t /*unused*/)
{
  if (!s_zb_instance) return;
  auto *self = s_zb_instance;
  self->reportSchedulerCount++;

  bool     rPower;
  uint8_t  rBri;
  uint16_t rX, rY;

  if (self->zbStateMutex && xSemaphoreTake(self->zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (!self->reportPending) {
      xSemaphoreGive(self->zbStateMutex);
      return;
    }
    rPower = self->reportPowerOn;
    rBri   = self->reportBri;
    rX     = self->reportColorX;
    rY     = self->reportColorY;
    self->reportPending = false;
    xSemaphoreGive(self->zbStateMutex);
  } else {
    // Could not acquire mutex — retry shortly
    esp_zb_scheduler_alarm(zbReportStateViaScheduler, 0, 50);
    return;
  }

  // Update the Zigbee attribute cache from within the Zigbee task.
  // The ZBOSS reporting engine detects value changes and automatically
  // sends attribute reports to bound coordinators.
  bool onOff = rPower;
  esp_zb_zcl_set_attribute_val(
    ZIGBEE_RGB_LIGHT_ENDPOINT,
    ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
    &onOff, false);

  esp_zb_zcl_set_attribute_val(
    ZIGBEE_RGB_LIGHT_ENDPOINT,
    ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
    &rBri, false);

  esp_zb_zcl_set_attribute_val(
    ZIGBEE_RGB_LIGHT_ENDPOINT,
    ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID,
    &rX, false);

  esp_zb_zcl_set_attribute_val(
    ZIGBEE_RGB_LIGHT_ENDPOINT,
    ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID,
    &rY, false);

  ESP_LOGI("ZigbeeRGB", "Attribute cache updated (scheduler): power=%d bri=%d x=0x%04x y=0x%04x",
           rPower, rBri, rX, rY);
}

/* ---------------------------------------------------------------------------
 *  Helper: update usermod shared state under the mutex, setting stateChanged.
 *  Call from the Zigbee task context only.
 * -------------------------------------------------------------------------*/
void zbUpdateState(bool power, uint8_t bri, uint16_t colorX, uint16_t colorY, bool useXY)
{
  if (!s_zb_instance) return;
  auto *self = s_zb_instance;

  if (self->zbStateMutex && xSemaphoreTake(self->zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    self->powerOn      = power;
    self->zbBrightness = bri;
    if (useXY) {
      self->zbColorX = colorX;
      self->zbColorY = colorY;
    }
    self->zbUseXY      = useXY;
    self->stateChanged = true;
    xSemaphoreGive(self->zbStateMutex);
  }
}

/* ---------------------------------------------------------------------------
 *  Attribute reporting infrastructure.
 *
 *  Zigbee ZCL attribute reporting requires TWO things:
 *  1. A binding in the local APS binding table (cluster → destination)
 *  2. Reporting configuration (min/max interval, delta threshold)
 *
 *  Without the binding, the ZBOSS reporting engine has no destination for
 *  the Report Attributes frames, so they are silently dropped.
 *
 *  Flow:
 *   zbConfigureAllReporting()  ← called 2s after join via scheduler_alarm
 *     → looks up parent IEEE address
 *     → calls zbCreateOneBinding() for each cluster
 *       → bind callback fires → zbBindCallback()
 *         → after all 3 bindings complete → zbSetupReportingConfig()
 *           → configures and starts reporting for all attributes
 * -------------------------------------------------------------------------*/

// Forward declare
static void zbSetupReportingConfig(uint8_t unused);

// Bind response callback — called by ZBOSS when ZDO Bind_rsp arrives
static void zbBindCallback(esp_zb_zdp_status_t status, void *user_ctx)
{
  uint16_t cluster = (uint16_t)(uintptr_t)user_ctx;
  ESP_LOGD("ZigbeeRGB", "Bind response for cluster 0x%04x: status=0x%02x", cluster, status);

  s_zb_binds_completed++;
  if (s_zb_binds_completed >= s_zb_binds_needed) {
    ESP_LOGD("ZigbeeRGB", "All %d bindings completed, configuring reporting...",
             s_zb_binds_needed);
    // Schedule reporting config in Zigbee task context
    esp_zb_scheduler_alarm(zbSetupReportingConfig, 0, 100);
  }
}

// Create one binding entry in the local APS binding table
static void zbCreateOneBinding(uint16_t clusterID,
                                const esp_zb_ieee_addr_t srcIeee,
                                const esp_zb_ieee_addr_t dstIeee,
                                uint16_t myShortAddr)
{
  esp_zb_zdo_bind_req_param_t bind_req;
  memset(&bind_req, 0, sizeof(bind_req));
  memcpy(bind_req.src_address, srcIeee, sizeof(esp_zb_ieee_addr_t));
  bind_req.src_endp     = ZIGBEE_RGB_LIGHT_ENDPOINT;
  bind_req.cluster_id   = clusterID;
  bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
  memcpy(bind_req.dst_address_u.addr_long, dstIeee, sizeof(esp_zb_ieee_addr_t));
  bind_req.dst_endp     = 1;   // Hue bridge endpoint
  bind_req.req_dst_addr = myShortAddr;  // send bind req to ourselves (local binding)

  ESP_LOGD("ZigbeeRGB", "Creating local binding for cluster 0x%04x → dst endpoint 1",
           clusterID);
  esp_zb_zdo_device_bind_req(&bind_req, zbBindCallback,
                              (void *)(uintptr_t)clusterID);
}

// Configure and start reporting for one attribute (called after bindings are created)
static void zbConfigureOneReport(uint16_t clusterID, uint16_t attrID,
                                  uint16_t minInterval, uint16_t maxInterval,
                                  uint16_t dstShortAddr)
{
  esp_zb_zcl_reporting_info_t info;
  memset(&info, 0, sizeof(info));
  info.direction    = ESP_ZB_ZCL_REPORT_DIRECTION_SEND;
  info.ep           = ZIGBEE_RGB_LIGHT_ENDPOINT;
  info.cluster_id   = clusterID;
  info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  info.attr_id      = attrID;
  info.u.send_info.min_interval     = minInterval;
  info.u.send_info.max_interval     = maxInterval;
  info.u.send_info.def_min_interval = minInterval;
  info.u.send_info.def_max_interval = maxInterval;
  // delta = 1 (report on any change)
  info.u.send_info.delta.u8 = 1;
  info.dst.short_addr = dstShortAddr;
  info.dst.endpoint   = 1;        // Hue bridge endpoint
  info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  info.manuf_code     = 0;

  esp_err_t err = esp_zb_zcl_update_reporting_info(&info);
  ESP_LOGD("ZigbeeRGB", "Configure report cl=0x%04x attr=0x%04x dst=0x%04x rc=0x%x",
           clusterID, attrID, dstShortAddr, err);

  if (err == ESP_OK) {
    esp_zb_zcl_attr_location_info_t loc;
    memset(&loc, 0, sizeof(loc));
    loc.endpoint_id  = ZIGBEE_RGB_LIGHT_ENDPOINT;
    loc.cluster_id   = clusterID;
    loc.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
    loc.attr_id      = attrID;
    loc.manuf_code   = 0;
    esp_err_t start_err = esp_zb_zcl_start_attr_reporting(loc);
    ESP_LOGD("ZigbeeRGB", "Start report cl=0x%04x attr=0x%04x rc=0x%x",
             clusterID, attrID, start_err);
  }
}

// Called after all bindings are created — configures and starts reporting
static void zbSetupReportingConfig(uint8_t /*unused*/)
{
  ESP_LOGD("ZigbeeRGB", "Setting up reporting configuration (dst=0x%04x)...",
           s_zb_parent_short);

  uint16_t dstShortAddr = s_zb_parent_short;

  zbConfigureOneReport(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                       ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, 1, 60, dstShortAddr);
  zbConfigureOneReport(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                       ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, 1, 60, dstShortAddr);
  zbConfigureOneReport(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                       ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, 1, 60, dstShortAddr);
  zbConfigureOneReport(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                       ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, 1, 60, dstShortAddr);
  ESP_LOGD("ZigbeeRGB", "Attribute reporting configured");
}

void zbConfigureAllReporting(uint8_t /*unused*/)
{
  ESP_LOGD("ZigbeeRGB", "Setting up bindings and attribute reporting...");

  // Reset bind counter
  s_zb_binds_completed = 0;

  // Get our own IEEE address
  esp_zb_ieee_addr_t myIeee;
  esp_zb_get_long_address(myIeee);
  uint16_t myShortAddr = esp_zb_get_short_address();
  s_zb_my_short = myShortAddr;

  // Get parent (Hue bridge) IEEE address from the neighbor table.
  // First try esp_zb_ieee_address_by_short(0x0000) — works if bridge is
  // the trust center / coordinator at address 0x0000.
  // If that fails, iterate the neighbor table looking for the parent entry.
  esp_zb_ieee_addr_t parentIeee;
  memset(parentIeee, 0, sizeof(parentIeee));
  uint16_t parentShort = 0x0000;
  esp_err_t ieee_err = esp_zb_ieee_address_by_short(0x0000, parentIeee);
  if (ieee_err != ESP_OK) {
    ESP_LOGW("ZigbeeRGB", "Could not resolve IEEE for short 0x0000 (err=0x%x), "
             "trying neighbor iteration...", ieee_err);
    // Iterate the neighbor table to find parent
    esp_zb_nwk_info_iterator_t iter = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    esp_zb_nwk_neighbor_info_t nbr;
    bool found = false;
    while (esp_zb_nwk_get_next_neighbor(&iter, &nbr) == ESP_OK) {
      ESP_LOGD("ZigbeeRGB", "Neighbor: short=0x%04x, relationship=%d, ieee=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
               nbr.short_addr, nbr.relationship,
               nbr.ieee_addr[7], nbr.ieee_addr[6], nbr.ieee_addr[5], nbr.ieee_addr[4],
               nbr.ieee_addr[3], nbr.ieee_addr[2], nbr.ieee_addr[1], nbr.ieee_addr[0]);
      if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_PARENT) {
        memcpy(parentIeee, nbr.ieee_addr, sizeof(esp_zb_ieee_addr_t));
        parentShort = nbr.short_addr;
        ESP_LOGD("ZigbeeRGB", "Found parent at short=0x%04x", nbr.short_addr);
        found = true;
        break;
      }
    }
    if (!found) {
      ESP_LOGE("ZigbeeRGB", "Could not find parent IEEE address, reporting will not work");
      return;
    }
  }

  ESP_LOGD("ZigbeeRGB", "Parent IEEE: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, short=0x%04x",
           parentIeee[7], parentIeee[6], parentIeee[5], parentIeee[4],
           parentIeee[3], parentIeee[2], parentIeee[1], parentIeee[0], parentShort);
  ESP_LOGD("ZigbeeRGB", "My short addr: 0x%04x", myShortAddr);

  // Store parent short address for reporting destination
  s_zb_parent_short = parentShort;

  // Create bindings for each cluster
  zbCreateOneBinding(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, myIeee, parentIeee, myShortAddr);
  zbCreateOneBinding(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, myIeee, parentIeee, myShortAddr);
  zbCreateOneBinding(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, myIeee, parentIeee, myShortAddr);

  // Reporting configuration will be set up in zbBindCallback after all bindings complete
}

/* ---------------------------------------------------------------------------
 *  Helper: read current on/off and level from the Zigbee attribute cache.
 *  Used by command handlers that only change color (not on/off or level)
 *  to carry forward the current on/off + brightness state.
 * -------------------------------------------------------------------------*/
void zbReadCurrentOnOffLevel(bool &power, uint8_t &level)
{
  power = true;
  level = 254;
  esp_zb_zcl_attr_t *attr;
  attr = esp_zb_zcl_get_attribute(
    ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID);
  if (attr && attr->data_p) power = *(bool *)attr->data_p;

  attr = esp_zb_zcl_get_attribute(
    ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID);
  if (attr && attr->data_p) level = *(uint8_t *)attr->data_p;
}

/* ---------------------------------------------------------------------------
 *  Helper: read current color XY from the Zigbee attribute cache.
 * -------------------------------------------------------------------------*/
void zbReadCurrentColorXY(uint16_t &colorX, uint16_t &colorY)
{
  colorX = 0x616B;
  colorY = 0x607D;
  esp_zb_zcl_attr_t *attr;
  attr = esp_zb_zcl_get_attribute(
    ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID);
  if (attr && attr->data_p) colorX = *(uint16_t *)attr->data_p;

  attr = esp_zb_zcl_get_attribute(
    ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID);
  if (attr && attr->data_p) colorY = *(uint16_t *)attr->data_p;
}

/* ---------------------------------------------------------------------------
 *  Unused — kept as forward-declared but the new raw handler handles
 *  everything directly instead of deferring to a poll.
 * -------------------------------------------------------------------------*/
void zbPollAttributesCallback(uint8_t /*param*/)
{
  // No longer used — raw handler processes commands directly.
  // Kept to satisfy the forward declaration.
}

/* ---------------------------------------------------------------------------
 *  Raw ZCL command handler — parses ZCL command payloads directly and
 *  updates the usermod shared state.
 *
 *  This is necessary because:
 *  1. SET_ATTR_VALUE_CB only fires for direct attribute writes, NOT when the
 *     stack processes ZCL commands like move_to_color, move_to_level, etc.
 *  2. ZBOSS may not update attributes without transition timer infrastructure.
 *  3. Some commands crash the stack (manufacturer-specific scenes, issue #681).
 *  4. Some commands are silently ignored (Off with effect, issue #519).
 *
 *  Strategy:
 *  - For commands we understand: parse the payload, update usermod state, then
 *    return FALSE to let ZBOSS also process it (send ZCL response, etc.).
 *  - For crash-causing commands: send FAIL response, return TRUE.
 *  - For "Off with effect": update state + attributes, send response, return TRUE
 *    (because ZBOSS silently ignores this command).
 * -------------------------------------------------------------------------*/
bool zb_raw_command_handler(uint8_t bufid)
{
  zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(bufid, zb_zcl_parsed_hdr_t);

  // --- Issue #681: Hue manufacturer-specific scene commands ---
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_SCENES &&
      cmd_info->is_manuf_specific &&
      cmd_info->manuf_specific == 0x100b) {
    ESP_LOGD("ZigbeeRGB", "Intercepted Hue manuf-specific scene cmd 0x%02x, responding FAIL",
             cmd_info->cmd_id);
    zb_zcl_send_default_handler(bufid, cmd_info, ZB_ZCL_STATUS_FAIL);
    return true;
  }

  // =====================================================================
  // ON/OFF cluster (0x0006) — commands: off=0x00, on=0x01, toggle=0x02,
  //                                      off_with_effect=0x40
  // =====================================================================
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF &&
      !cmd_info->is_common_command) {

    bool newPower;
    bool handled = false;  // true = we must fully handle (ZBOSS won't)

    if (cmd_info->cmd_id == 0x40) {
      // Off-with-effect — ZBOSS silently ignores this, must handle ourselves
      newPower = false;
      handled = true;
    } else if (cmd_info->cmd_id == 0x00) {
      newPower = false;
    } else if (cmd_info->cmd_id == 0x01) {
      newPower = true;
    } else if (cmd_info->cmd_id == 0x02) {
      // Toggle — read current state and invert
      bool curPower; uint8_t dummy;
      zbReadCurrentOnOffLevel(curPower, dummy);
      newPower = !curPower;
    } else {
      return false;
    }

    ESP_LOGD("ZigbeeRGB", "On/Off cmd 0x%02x → power=%d (handled=%d)",
             cmd_info->cmd_id, newPower, handled);

    // Read current level and color to carry forward
    uint8_t curLevel; bool dummy2;
    zbReadCurrentOnOffLevel(dummy2, curLevel);
    uint16_t curX, curY;
    zbReadCurrentColorXY(curX, curY);

    // Update usermod state
    zbUpdateState(newPower, curLevel, curX, curY, true);

    if (handled) {
      // For off_with_effect: update attribute cache and send response ourselves
      bool attrVal = newPower;
      esp_zb_zcl_set_attribute_val(
        ZIGBEE_RGB_LIGHT_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
        &attrVal, false);
      zb_zcl_send_default_handler(bufid, cmd_info, ZB_ZCL_STATUS_SUCCESS);
      return true;
    }
    // Let ZBOSS handle the standard on/off commands (it will send the response
    // and update attributes). We've already updated our usermod state.
    return false;
  }

  // =====================================================================
  // LEVEL CONTROL cluster (0x0008) — move_to_level=0x00,
  //                                   move_to_level_with_on_off=0x04
  // =====================================================================
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL &&
      !cmd_info->is_common_command &&
      (cmd_info->cmd_id == 0x00 || cmd_info->cmd_id == 0x04)) {

    uint8_t *payload = (uint8_t *)zb_buf_begin(bufid);
    zb_uint_t payload_len = zb_buf_len(bufid);
    if (payload_len < 3) {
      ESP_LOGW("ZigbeeRGB", "Level cmd payload too short (%d)", payload_len);
      return false;  // let ZBOSS handle the error
    }

    uint8_t level = payload[0];

    ESP_LOGD("ZigbeeRGB", "Level cmd 0x%02x → level=%d", cmd_info->cmd_id, level);

    // For move_to_level_with_on_off (0x04): level > 0 means ON, level == 0 means OFF
    bool newPower;
    if (cmd_info->cmd_id == 0x04) {
      newPower = (level > 0);
    } else {
      bool dummy; uint8_t dummy2;
      zbReadCurrentOnOffLevel(newPower, dummy2);
    }

    uint16_t curX, curY;
    zbReadCurrentColorXY(curX, curY);

    zbUpdateState(newPower, level, curX, curY, true);

    return false;  // let ZBOSS also process it
  }

  // =====================================================================
  // COLOR CONTROL cluster (0x0300) — move_to_color=0x07
  // =====================================================================
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL &&
      !cmd_info->is_common_command &&
      cmd_info->cmd_id == 0x07) {

    uint8_t *payload = (uint8_t *)zb_buf_begin(bufid);
    zb_uint_t payload_len = zb_buf_len(bufid);
    if (payload_len < 6) {
      ESP_LOGW("ZigbeeRGB", "move_to_color payload too short (%d)", payload_len);
      return false;
    }

    uint16_t colorX = payload[0] | (payload[1] << 8);
    uint16_t colorY = payload[2] | (payload[3] << 8);

    ESP_LOGD("ZigbeeRGB", "move_to_color → x=0x%04x y=0x%04x", colorX, colorY);

    bool curPower; uint8_t curLevel;
    zbReadCurrentOnOffLevel(curPower, curLevel);

    zbUpdateState(curPower, curLevel, colorX, colorY, true);

    return false;  // let ZBOSS also process it
  }

  // =====================================================================
  // COLOR CONTROL — move_to_hue_saturation=0x06
  // =====================================================================
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL &&
      !cmd_info->is_common_command &&
      cmd_info->cmd_id == 0x06) {

    uint8_t *payload = (uint8_t *)zb_buf_begin(bufid);
    zb_uint_t payload_len = zb_buf_len(bufid);
    if (payload_len < 4) {
      return false;
    }

    uint8_t hue = payload[0];
    uint8_t sat = payload[1];

    ESP_LOGD("ZigbeeRGB", "move_to_hue_saturation → hue=%d sat=%d", hue, sat);

    bool curPower; uint8_t curLevel;
    zbReadCurrentOnOffLevel(curPower, curLevel);

    if (s_zb_instance) {
      auto *self = s_zb_instance;
      if (self->zbStateMutex && xSemaphoreTake(self->zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        self->powerOn      = curPower;
        self->zbBrightness = curLevel;
        self->zbHue        = hue;
        self->zbSaturation = sat;
        self->zbUseXY      = false;
        self->stateChanged = true;
        xSemaphoreGive(self->zbStateMutex);
      }
    }

    return false;
  }

  // =====================================================================
  // COLOR CONTROL — move_to_hue=0x00
  // =====================================================================
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL &&
      !cmd_info->is_common_command &&
      cmd_info->cmd_id == 0x00) {

    uint8_t *payload = (uint8_t *)zb_buf_begin(bufid);
    zb_uint_t payload_len = zb_buf_len(bufid);
    if (payload_len < 4) {
      return false;
    }

    uint8_t hue = payload[0];

    ESP_LOGD("ZigbeeRGB", "move_to_hue → hue=%d", hue);

    bool curPower; uint8_t curLevel;
    zbReadCurrentOnOffLevel(curPower, curLevel);

    uint8_t curSat = 254;
    esp_zb_zcl_attr_t *attr = esp_zb_zcl_get_attribute(
      ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID);
    if (attr && attr->data_p) curSat = *(uint8_t *)attr->data_p;

    if (s_zb_instance) {
      auto *self = s_zb_instance;
      if (self->zbStateMutex && xSemaphoreTake(self->zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        self->powerOn      = curPower;
        self->zbBrightness = curLevel;
        self->zbHue        = hue;
        self->zbSaturation = curSat;
        self->zbUseXY      = false;
        self->stateChanged = true;
        xSemaphoreGive(self->zbStateMutex);
      }
    }

    return false;
  }

  // =====================================================================
  // COLOR CONTROL — move_to_saturation=0x03
  // =====================================================================
  if (cmd_info->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL &&
      !cmd_info->is_common_command &&
      cmd_info->cmd_id == 0x03) {

    uint8_t *payload = (uint8_t *)zb_buf_begin(bufid);
    zb_uint_t payload_len = zb_buf_len(bufid);
    if (payload_len < 3) {
      return false;
    }

    uint8_t sat = payload[0];

    ESP_LOGD("ZigbeeRGB", "move_to_saturation → sat=%d", sat);

    bool curPower; uint8_t curLevel;
    zbReadCurrentOnOffLevel(curPower, curLevel);

    uint8_t curHue = 0;
    esp_zb_zcl_attr_t *attr = esp_zb_zcl_get_attribute(
      ZIGBEE_RGB_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID);
    if (attr && attr->data_p) curHue = *(uint8_t *)attr->data_p;

    if (s_zb_instance) {
      auto *self = s_zb_instance;
      if (self->zbStateMutex && xSemaphoreTake(self->zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        self->powerOn      = curPower;
        self->zbBrightness = curLevel;
        self->zbHue        = curHue;
        self->zbSaturation = sat;
        self->zbUseXY      = false;
        self->stateChanged = true;
        xSemaphoreGive(self->zbStateMutex);
      }
    }

    return false;
  }

  // =====================================================================
  // Unrecognized commands — let ZBOSS handle them
  // =====================================================================
  if (!cmd_info->is_common_command) {
    ESP_LOGD("ZigbeeRGB", "Unhandled cluster 0x%04x cmd 0x%02x, passing to ZBOSS",
             cmd_info->cluster_id, cmd_info->cmd_id);
  }

  return false;
}

#endif // CONFIG_IDF_TARGET_ESP32C6
