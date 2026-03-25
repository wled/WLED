/*
 * Zigbee RGB Light Usermod for ESP32-C6
 *
 * Exposes WLED as a Zigbee HA Color Dimmable Light, allowing
 * Zigbee coordinators (e.g. Zigbee2MQTT, ZHA) to control power,
 * brightness, hue, and saturation.
 *
 * Requires:
 *   - ESP32-C6 target (native 802.15.4 radio)
 *   - esp-zigbee-lib and esp-zboss-lib (declared in library.json)
 *   - Add zigbee_rgb_light to custom_usermods in your platformio env
 */

#ifdef CONFIG_IDF_TARGET_ESP32C6

#include "wled.h"

#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Default Zigbee endpoint number for the light
#ifndef ZIGBEE_RGB_LIGHT_ENDPOINT
  #define ZIGBEE_RGB_LIGHT_ENDPOINT 10
#endif

// Stack size for the Zigbee FreeRTOS task (in bytes)
#ifndef ZIGBEE_TASK_STACK_SIZE
  #define ZIGBEE_TASK_STACK_SIZE 4096
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
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
      }
      break;
    default:
      break;
  }
}

/* ---------------------------------------------------------------------------
 *  Usermod class
 * -------------------------------------------------------------------------*/
class ZigbeeRGBLightUsermod : public Usermod {
private:
  bool enabled  = true;
  bool initDone = false;
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
   * ---------------------------------------------------------------------*/
  static void zigbeeTask(void *pvParameters)
  {
    // Platform configuration — native 802.15.4 radio on the C6
    esp_zb_platform_config_t platform_cfg = {};
    platform_cfg.radio_config.radio_mode             = ZB_RADIO_MODE_NATIVE;
    platform_cfg.host_config.host_connection_mode     = ZB_HOST_CONNECTION_MODE_NONE;
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

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

    // Set the primary channel mask (all standard Zigbee channels)
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    // Start the stack (false = not coordinator)
    ESP_ERROR_CHECK(esp_zb_start(false));

    // Run the Zigbee main loop (one iteration at a time)
    for (;;) {
      esp_zb_main_loop_iteration();
    }
    // Task never returns; handle is cleaned up if the usermod is disabled
  }

public:
  ZigbeeRGBLightUsermod() { instance = this; }

  /* ---- Lifecycle ------------------------------------------------------ */

  void setup() override
  {
    if (!enabled) return;

    zbStateMutex = xSemaphoreCreateMutex();
    if (!zbStateMutex) return;

    if (xTaskCreate(
          zigbeeTask,
          "zigbee_rgb",
          ZIGBEE_TASK_STACK_SIZE,
          nullptr,
          ZIGBEE_TASK_PRIORITY,
          &zbTaskHandle
        ) != pdPASS) return;

    initDone = true;
  }

  void loop() override
  {
    if (!enabled || !initDone || strip.isUpdating()) return;

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

/* -- Registration ------------------------------------------------------- */
static ZigbeeRGBLightUsermod zigbee_rgb_light_mod;
REGISTER_USERMOD(zigbee_rgb_light_mod);

#endif // CONFIG_IDF_TARGET_ESP32C6
