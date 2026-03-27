/*
 * Zigbee RGB Light Usermod for ESP32-C6
 *
 * Exposes WLED as a Zigbee HA Color Dimmable Light, allowing
 * Zigbee coordinators (e.g. Zigbee2MQTT, ZHA) to control power,
 * brightness, hue, and saturation.
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
#include "esp_coexist.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Default Zigbee endpoint number for the light
#ifndef ZIGBEE_RGB_LIGHT_ENDPOINT
  #define ZIGBEE_RGB_LIGHT_ENDPOINT 10
#endif

// Stack size for the Zigbee FreeRTOS task (in bytes).
// The WiFi+Zigbee gateway example from Espressif uses 8192; 4096 is enough for
// a pure light but the coexistence init path (esp_coex_wifi_i154_enable) adds
// several hundred bytes of additional stack usage.
#ifndef ZIGBEE_TASK_STACK_SIZE
  #define ZIGBEE_TASK_STACK_SIZE 8192
#endif

// Priority for the Zigbee FreeRTOS task
#ifndef ZIGBEE_TASK_PRIORITY
  #define ZIGBEE_TASK_PRIORITY 5
#endif

/* ---------------------------------------------------------------------------
 *  Zigbee signal handler (extern "C" – called by the Zigbee stack)
 * -------------------------------------------------------------------------*/
extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
  uint32_t *p_sg_p       = signal_struct->p_app_signal;
  esp_err_t err_status   = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;

  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
      // Fires during early startup; ESP_OK = production config loaded from NVS,
      // any other status = no production config found (normal on new devices).
      ESP_LOGI("ZigbeeRGB", "Production config: %s",
               (err_status == ESP_OK) ? "loaded" : "not found (normal)");
      break;
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGI("ZigbeeRGB", "Zigbee stack started (SKIP_STARTUP)");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        ESP_LOGI("ZigbeeRGB", "Zigbee BDB init OK, starting network steering");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
      } else {
        ESP_LOGE("ZigbeeRGB", "Zigbee BDB init failed: 0x%x", err_status);
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      // Fires when network steering scan completes.  ESP_OK = joined a network;
      // any other status = no coordinator found (normal when not yet paired).
      ESP_LOGI("ZigbeeRGB", "Network steering complete, status: 0x%x%s",
               err_status, (err_status == ESP_OK) ? " (joined)" : " (no network, will retry)");
      if (err_status != ESP_OK) {
        // Retry steering after a short pause so we do not busy-loop the RF.
        esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning,
                               ESP_ZB_BDB_MODE_NETWORK_STEERING, 5000);
      }
      break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
      ESP_LOGI("ZigbeeRGB", "Network steering: permit join status");
      break;
    default:
      ESP_LOGI("ZigbeeRGB", "Zigbee signal: 0x%x, status: 0x%x", sig_type, err_status);
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
  TaskHandle_t zbTaskHandle = nullptr;

  // Mutex protecting shared state between the Zigbee task and loop()
  SemaphoreHandle_t zbStateMutex = nullptr;

  // Zigbee state — written from the Zigbee task, read in loop()
  volatile bool    stateChanged  = false;
  volatile bool    powerOn       = true;
  volatile uint8_t zbBrightness  = 254;
  volatile uint8_t zbHue         = 0;
  volatile uint8_t zbSaturation  = 254;

  // Flash-string keys
  static const char _name[];
  static const char _enabled[];

  // Singleton pointer (needed for static Zigbee callbacks)
  static ZigbeeRGBLightUsermod *instance;

  /* -----------------------------------------------------------------------
   *  Static Zigbee action callback (dispatches to instance method)
   * ---------------------------------------------------------------------*/
  static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                     const void *message)
  {
    if (!instance) return ESP_ERR_INVALID_STATE;
    if (callback_id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
      return instance->handleAttributeSet(
        static_cast<const esp_zb_zcl_set_attr_value_message_t *>(message));
    }
    return ESP_OK;
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

    if (zbStateMutex && xSemaphoreTake(zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      switch (cluster) {

        case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
          if (attrId == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            powerOn = *(const bool *)message->attribute.data.value;
            stateChanged = true;
          }
          break;

        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
          if (attrId == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
            zbBrightness = *(const uint8_t *)message->attribute.data.value;
            stateChanged = true;
          }
          break;

        case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
          if (attrId == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID) {
            zbHue = *(const uint8_t *)message->attribute.data.value;
            stateChanged = true;
          } else if (attrId == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID) {
            zbSaturation = *(const uint8_t *)message->attribute.data.value;
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
   *  Apply the cached Zigbee state to the WLED engine
   * ---------------------------------------------------------------------*/
  void applyState()
  {
    // Snapshot shared state under the mutex
    bool    localPower;
    uint8_t localBri, localHue, localSat;
    if (zbStateMutex && xSemaphoreTake(zbStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localPower = powerOn;
      localBri   = zbBrightness;
      localHue   = zbHue;
      localSat   = zbSaturation;
      xSemaphoreGive(zbStateMutex);
    } else {
      return; // could not acquire mutex, try again next loop
    }

    if (!localPower) {
      bri = 0;
    } else {
      bri = localBri;

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
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  /* -----------------------------------------------------------------------
   *  FreeRTOS task — runs the Zigbee stack
   *  Note: esp_zb_platform_config() is called in setup() before this task
   *  is created, matching the pattern in Espressif's IDF examples.
   * ---------------------------------------------------------------------*/
  static void zigbeeTask(void *pvParameters)
  {
    // Initialise the Zigbee stack as an End Device
    esp_zb_cfg_t zb_nwk_cfg = {};
    zb_nwk_cfg.esp_zb_role        = ESP_ZB_DEVICE_TYPE_ED;
    zb_nwk_cfg.install_code_policy = false;
    zb_nwk_cfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    zb_nwk_cfg.nwk_cfg.zed_cfg.keep_alive = 3000;
    esp_zb_init(&zb_nwk_cfg);

    // Create HA Color Dimmable Light endpoint (On/Off + Level + Color)
    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    esp_zb_ep_list_t *ep_list = esp_zb_color_dimmable_light_ep_create(
      ZIGBEE_RGB_LIGHT_ENDPOINT, &light_cfg);
    esp_zb_device_register(ep_list);

    // Register the attribute-write callback
    esp_zb_core_action_handler_register(zb_action_handler);

    // Set the primary channel mask.
    // Channel 26 (2480 MHz) has ZERO spectral overlap with any WiFi channel —
    // the 802.11 spectrum ends at ~2472 MHz (ch13).  This is the safest choice
    // for WiFi AP coexistence.  Channel 25 (2475 MHz) still has partial overlap
    // with WiFi ch13/14; channel 26 does not.
    // Channel 26 bit in the mask = (1 << (26 - 11)) = bit 15 = 0x00008000.
    esp_zb_set_primary_network_channel_set(1 << (26 - 11));

    // Start the stack (false = not coordinator)
    ESP_ERROR_CHECK(esp_zb_start(false));

    // Run the Zigbee main loop — never returns
    esp_zb_stack_main_loop();
    vTaskDelete(nullptr);
  }

public:
  ZigbeeRGBLightUsermod() {
    instance = this;
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
      ESP_LOGI("ZigbeeRGB", "WiFi/802.15.4 coexistence enabled (before WiFi.mode)");
    } else {
      ESP_LOGE("ZigbeeRGB", "esp_coex_wifi_i154_enable failed: 0x%x", coex_err);
    }
#endif

    zbStateMutex = xSemaphoreCreateMutex();
    if (!zbStateMutex) return;

    // Configure the Zigbee platform now (before any task is created), matching
    // the pattern in Espressif's IDF examples where esp_zb_platform_config()
    // is called in app_main before RTOS tasks start.
    esp_zb_platform_config_t platform_cfg = {};
    platform_cfg.radio_config.radio_mode          = ZB_RADIO_MODE_NATIVE;
    platform_cfg.host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;
    if (esp_zb_platform_config(&platform_cfg) != ESP_OK) {
      DEBUG_PRINTLN(F("ZigbeeRGBLight: esp_zb_platform_config failed"));
      return;
    }

    initDone = true;
    // The Zigbee task is NOT started here.  It will be started by
    // connected() once the device has a WiFi client (STA) IP address.
    // This avoids the 802.15.4 radio interfering with the WiFi AP before
    // a home network connection is established.
    DEBUG_PRINTLN(F("ZigbeeRGBLight: ready, waiting for WiFi STA connection"));
  }

  // Called by WLED when the STA interface gets an IP address (i.e. we are
  // connected to a home WiFi network).  This is the trigger to start Zigbee.
  void connected() override
  {
    if (!enabled || !initDone || zbStarted) return;

    ESP_LOGI("ZigbeeRGB", "WiFi STA connected — starting Zigbee task");

    if (xTaskCreate(
          zigbeeTask,
          "zigbee_rgb",
          ZIGBEE_TASK_STACK_SIZE,
          nullptr,
          ZIGBEE_TASK_PRIORITY,
          &zbTaskHandle
        ) != pdPASS) {
      ESP_LOGE("ZigbeeRGB", "Failed to create Zigbee task");
      return;
    }

    zbStarted = true;
    DEBUG_PRINTLN(F("ZigbeeRGBLight: Zigbee task created"));
  }

  void loop() override
  {
    if (!enabled || !zbStarted || strip.isUpdating()) return;

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
    JsonArray arr = user.createNestedArray(FPSTR(_name));
    arr.add(enabled ? F("active") : F("disabled"));
  }

  uint16_t getId() override
  {
    return USERMOD_ID_ZIGBEE_RGB_LIGHT;
  }
};

/* -- Static member definitions ------------------------------------------ */
ZigbeeRGBLightUsermod *ZigbeeRGBLightUsermod::instance   = nullptr;
const char ZigbeeRGBLightUsermod::_name[]    PROGMEM = "ZigbeeRGBLight";
const char ZigbeeRGBLightUsermod::_enabled[] PROGMEM = "enabled";

#endif // CONFIG_IDF_TARGET_ESP32C6
