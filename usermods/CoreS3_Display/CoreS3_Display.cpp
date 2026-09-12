#include "wled.h"
#include <WiFi.h>
#include <M5GFX.h>
#include <esp_heap_caps.h>
#include <memory>

#include "M5StackDisplayHardwareBackend.h"
#include "M5StackDisplayUI.h"
#include "M5StackDisplayTouchState.h"
#include "M5StackDisplayTouchHelpers.h"
#include "M5StackDisplayTouchContext.h"

#include "CoreS3_WLED_Logo.h"

// ===========================================================
// M5Stack Display Controller Usermod
//
// Current verified runtime
//   - M5Stack CoreS3
//   - 320 x 240 display
//   - Touch input
//   - LCD brightness control
//
// Prepared hardware profiles
//   - M5Stack Core2
//   - M5Stack Core2 for AWS
//
// The Core2-family profiles remain diagnostic-only until their
// Display / Touch / Power paths are implemented and verified on
// real hardware.
//
// Responsibilities
//   - WLED power / brightness control
//   - Effect and palette navigation
//   - Color hue / saturation control
//   - Preset navigation and management
//   - Boot-preset selection
//   - Startup animation
//   - Display sleep / wake
//   - Runtime synchronization with WLED state
//   - Browser screenshot endpoint (/cores3/screenshot.bmp)
//
// Architecture
//   UI and WLED-state logic are kept separate from the thin
//   Display / Touch / brightness hardware-access boundary so the
//   same controller behavior can later be reused by Core2-family
//   hardware profiles.
//
// Compatibility
//   The existing "CoreS3_Display" configuration key and usermod
//   class identity are intentionally retained so existing CoreS3
//   settings continue to load without migration.
// ===========================================================

static const char CORES3_DISPLAY_CONFIG_NAME[] PROGMEM = "CoreS3_Display";

// CoreS3_Power publishes read-only runtime health state.
// Display consumes these signals only for user-facing warning UX; it does
// not own or modify the power-control implementation.
extern "C" bool coreS3PowerInitializationComplete();
extern "C" bool coreS3PowerExternal5VReady();
extern "C" bool coreS3PowerSafeShutdownMonitorReady();

#if defined(WLED_M5STACK_CORES3_AUDIO)
// CoreS3_Audio publishes terminal initialization and codec-ready state.
// Display consumes these signals only to annotate Audio Reactive effects when
// the built-in microphone is definitively unavailable.
extern "C" bool coreS3AudioInitializationFinished();
extern "C" bool coreS3AudioCodecReady();
#endif

class CoreS3DisplayUsermod : public Usermod {
  private:

  // =========================================================
  // Display
  // =========================================================

  M5GFX display;
  M5StackDisplayHardwareBackend hardwareBackend;

  bool displayReady = false;
  bool touchReady = false;
  bool initDone = false;

  int16_t screenWidth = 0;
  int16_t screenHeight = 0;

  unsigned long lastUpdate = 0;

  // =========================================================
  // Browser Screenshot
  // =========================================================
  //
  // A GET request to /cores3/screenshot.bmp returns a one-shot
  // 24-bit BMP capture of the current 320 x 240 LCD contents.
  //
  // The frame and BMP buffers are allocated only while a request
  // is active and are placed in PSRAM. The finished BMP buffer is
  // retained by the asynchronous HTTP response until transmission
  // completes, then released automatically.
  //
  // This is intentionally a still-image endpoint. It does not
  // continuously stream frames and therefore does not add normal
  // runtime Display/Wi-Fi load when unused.
  // =========================================================

  bool screenshotCaptureInProgress = false;

  static constexpr size_t SCREENSHOT_BMP_HEADER_SIZE = 54;

  // =========================================================
  // Network access state
  // =========================================================
  //
  // CoreS3 is a local controller first. STA connectivity must not be
  // treated as a prerequisite for the touch UI because WLED can also be
  // reached through its SoftAP, and local LED control must remain usable
  // even when no network interface is currently available.
  // =========================================================

  enum NetworkAccessMode : uint8_t {
    NETWORK_ACCESS_NONE = 0,
    NETWORK_ACCESS_STA,
    NETWORK_ACCESS_AP
  };

  NetworkAccessMode lastNetworkAccessMode = NETWORK_ACCESS_NONE;
  String lastNetworkDisplayText = "";

  // Session-only Recovery AP state. This never changes the persisted
  // WLED AP behavior or the user's configured AP credentials.
  bool recoveryApSessionActive = false;
  String recoveryApSSID = "";
  unsigned long recoveryApLastStartAttempt = 0;

  bool readyScreenShown = false;
  bool connectingScreenShown = false;

  // =========================================================
  // Battery status
  // =========================================================
  //
  // MAIN-only status display. Battery is intentionally informational:
  // it has no touch target and does not alter WLED behavior.
  // =========================================================

  M5StackBatteryStatus batteryStatus;
  bool batteryStatusInitialized = false;

  unsigned long lastBatteryStatusRead = 0;

  static constexpr unsigned long BATTERY_STATUS_UPDATE_MS = 10000;

  // =========================================================
  // Runtime Health / Error UX
  // =========================================================
  //
  // Normal operation stays visually unchanged. Only actionable failures are
  // surfaced, once per boot, and only while MAIN is idle. Warnings are
  // temporary so the controller remains usable even when a subsystem fails.
  // =========================================================

  enum RuntimeHealthWarning : uint8_t {
    RUNTIME_HEALTH_WARNING_NONE = 0,
    RUNTIME_HEALTH_WARNING_LED_POWER,
    RUNTIME_HEALTH_WARNING_POWER_SAFETY,
    RUNTIME_HEALTH_WARNING_TOUCH
  };

  RuntimeHealthWarning activeRuntimeHealthWarning =
    RUNTIME_HEALTH_WARNING_NONE;

  uint8_t runtimeHealthWarningsShownMask = 0;
  unsigned long runtimeHealthStartMs = 0;
  unsigned long runtimeHealthWarningStartMs = 0;

  static constexpr uint8_t RUNTIME_HEALTH_SHOWN_LED_POWER = 0x01;
  static constexpr uint8_t RUNTIME_HEALTH_SHOWN_POWER_SAFETY = 0x02;
  static constexpr uint8_t RUNTIME_HEALTH_SHOWN_TOUCH = 0x04;

  // Give the deferred M5GFX I2C1 power-key monitor time to ARM before
  // declaring Safe Shutdown unavailable.
  static constexpr unsigned long POWER_SAFETY_WARNING_GRACE_MS = 5000;
  static constexpr unsigned long RUNTIME_HEALTH_WARNING_HOLD_MS = 2200;

  // Audio health is contextual rather than a global warning: only an effect
  // that actually depends on audio is annotated when codec initialization has
  // definitively failed. During initialization the normal capability text is
  // kept unchanged.
  bool lastAudioUnavailable = false;

  // =========================================================
  // Cached WLED state
  // =========================================================

  int8_t lastLedState = -1;

  int lastBrightnessValue = -1;
  int lastEffectMode = -1;

  int lastSpeedValue = -1;
  int lastIntensityValue = -1;

  int lastPaletteValue = -1;

  int lastHueValue = -1;
  int lastSaturationValue = -1;

  uint32_t lastPrimaryColor = 0;
  bool lastPrimaryColorValid = false;

  // COLOR page multi-slot editor state.
  //
  // selectedColorSlot is runtime-only UI state:
  //   0 = C1, 1 = C2, 2 = C3
  //
  // WLED Effect metadata decides which slots are selectable.
  uint8_t selectedColorSlot = 0;

  uint32_t lastSelectedColor = 0;
  bool lastSelectedColorValid = false;

  uint32_t lastColorSlots[3] = { 0, 0, 0 };
  bool lastColorSlotsValid = false;

  // Runtime-only BLACK toggle history.
  //
  // A long-press on C1/C2/C3 toggles that WLED color slot between black
  // (#000000) and its last observed non-black value. This history is never
  // written to Flash/config and is refreshed by both CoreS3 and Web UI color
  // changes.
  uint32_t lastNonBlackColorSlots[3] = { 0, 0, 0 };
  bool lastNonBlackColorSlotValid[3] = { false, false, false };

  // =========================================================
  // Preset state
  // =========================================================

  int lastPresetValue = -1;
  int lastBootPresetValue = -1;

  // CoreS3 UI navigation position.
  //
  // This intentionally differs from WLED currentPreset:
  // - currentPreset = WLED's currently active Preset
  // - presetNavigationCursorId = last Preset selected/browsed by this UI
  //
  // When currentPreset becomes 0 (Custom State) after Color/Effect changes,
  // the cursor remains on the last useful Preset position.
  uint8_t presetNavigationCursorId = 0;

  // Last non/zero WLED currentPreset value observed by the cursor sync logic.
  // This is runtime-only UI state and is not persisted to Flash.
  uint8_t lastObservedCurrentPreset = 0;

  uint8_t pendingPresetId = 0;
  String pendingPresetName = "";

  unsigned long pendingPresetRequestMs = 0;

  unsigned long lastPresetsModifiedTime = 0;

  bool presetNoEntries = false;

  static constexpr unsigned long PRESET_APPLY_PENDING_MS = 1500;

  // =========================================================
  // Preset RAM cache
  //
  // WLED Preset names are limited to 32 characters.
  //
  // Cache is intentionally a fixed array:
  //   - no repeated heap allocation during navigation
  //   - predictable memory use
  //   - maximum WLED persistent Presets = 250
  //
  // Approximate RAM:
  //   250 x 34 bytes = about 8.5KB
  // =========================================================

  struct PresetCacheEntry {
    uint8_t id;
    char name[33];
  };

  PresetCacheEntry presetCache[250];

  uint16_t presetCacheCount = 0;

  bool presetCacheReady = false;
  bool presetCacheBuilding = false;

  uint16_t presetCacheScanId = 1;

  unsigned long presetCacheLastScanMs = 0;

  unsigned long presetCacheSourceModifiedTime = 0;
  unsigned long presetCacheBuildSourceModifiedTime = 0;

  static constexpr unsigned long PRESET_CACHE_SCAN_INTERVAL_MS = 5;

  // =========================================================
  // Persistent Display Settings
  // =========================================================

  uint16_t lcdBrightness = 128;

  // 0 = Never
  uint16_t sleepTimeoutSec = 30;

  bool fadeEnabled = true;

  uint16_t fadeDurationMs = 250;

  // =========================================================
  // Current physical LCD brightness
  // =========================================================

  uint8_t currentDisplayBrightness = 0;

  // =========================================================
  // Display suspend
  // =========================================================

  static constexpr uint8_t DISPLAY_FADE_STEP = 4;

  enum DisplayPowerState : uint8_t {
    DISPLAY_POWER_ACTIVE = 0, DISPLAY_POWER_SLEEP_FADE_OUT, DISPLAY_POWER_SLEEPING, DISPLAY_POWER_WAKE_FADE_IN, DISPLAY_POWER_WAKE_WAIT_RELEASE };

  DisplayPowerState displayPowerState = DISPLAY_POWER_ACTIVE;

  unsigned long lastUserActivityMs = 0;
  unsigned long displayFadeLastStep = 0;

  // =========================================================
  // Startup animation
  // =========================================================

  enum StartupState : uint8_t {
    STARTUP_FADE_IN = 0, STARTUP_WAIT_WIFI, STARTUP_READY_HOLD, STARTUP_FADE_OUT, STARTUP_MAIN_FADE_IN, STARTUP_DONE };

  StartupState startupState = STARTUP_FADE_IN;

  unsigned long startupStateStart = 0;
  unsigned long startupLastFadeStep = 0;
  unsigned long startupLastDotsUpdate = 0;

  uint8_t startupDotCount = 0;

  NetworkAccessMode startupNetworkAccessMode = NETWORK_ACCESS_NONE;
  String startupIPAddress = "";

  static constexpr unsigned long STARTUP_READY_HOLD_MS = 700;
  static constexpr unsigned long STARTUP_DOTS_INTERVAL_MS = 350;

  // Display/UI fail-safe only. WLED itself continues its normal Wi-Fi
  // connection/reconnection behavior after the local UI becomes available.
  static constexpr unsigned long STARTUP_NETWORK_WAIT_TIMEOUT_MS = 10000;

  // =========================================================
  // Persistent logical HSV
  // =========================================================

  CHSV32 logicalColorHsv;

  bool logicalColorHsvValid = false;

  uint8_t logicalHueValue = 0;
  uint8_t logicalSaturationValue = 0;
  uint8_t logicalWhiteValue = 0;

  // =========================================================
  // Pages
  // =========================================================

  enum ScreenPage : uint8_t {
    SCREEN_MAIN = 0, SCREEN_COLOR, SCREEN_EFFECT, SCREEN_PRESET };

  ScreenPage currentPage = SCREEN_MAIN;

  // =========================================================
  // Preset sub pages
  // =========================================================

  enum PresetSubPage : uint8_t {
    PRESET_SUBPAGE_NAV = 0, PRESET_SUBPAGE_MANAGE, PRESET_SUBPAGE_SAVE, PRESET_SUBPAGE_OVERWRITE, PRESET_SUBPAGE_DELETE, PRESET_SUBPAGE_BOOT };

  PresetSubPage presetSubPage = PRESET_SUBPAGE_NAV;

  // =========================================================
  // New Preset save operation
  // =========================================================

  enum PresetSaveOperationState : uint8_t {
    PRESET_SAVE_OP_IDLE = 0, PRESET_SAVE_OP_WAIT_WLED, PRESET_SAVE_OP_WAIT_CACHE, PRESET_SAVE_OP_SUCCESS, PRESET_SAVE_OP_FAILED };

  PresetSaveOperationState presetSaveOperationState = PRESET_SAVE_OP_IDLE;

  uint8_t presetSaveCandidateId = 0;
  String presetSaveCandidateName = "";

  unsigned long presetSaveHoldStartTime = 0;
  bool presetSaveHoldTriggered = false;

  unsigned long presetSaveResultStartMs = 0;

  static constexpr unsigned long PRESET_SAVE_HOLD_MS = 1000;
  static constexpr unsigned long PRESET_SAVE_RESULT_HOLD_MS = 900;

  // =========================================================
  // Existing Preset overwrite selection
  // =========================================================

  uint8_t presetOverwriteTargetId = 0;
  String presetOverwriteTargetName = "";

  bool presetSaveOperationIsOverwrite = false;

  // =========================================================
  // Existing Preset delete selection / operation
  // =========================================================

  enum PresetDeleteOperationState : uint8_t {
    PRESET_DELETE_OP_IDLE = 0, PRESET_DELETE_OP_WAIT_CACHE, PRESET_DELETE_OP_SUCCESS, PRESET_DELETE_OP_FAILED };

  PresetDeleteOperationState presetDeleteOperationState = PRESET_DELETE_OP_IDLE;

  uint8_t presetDeleteTargetId = 0;
  String presetDeleteTargetName = "";

  bool presetDeleteWasCurrentPreset = false;
  bool presetDeleteWasBootPreset = false;

  unsigned long presetDeleteResultStartMs = 0;

  // =========================================================
  // Boot Preset selection / operation
  // =========================================================

  enum PresetBootOperationState : uint8_t {
    PRESET_BOOT_OP_IDLE = 0, PRESET_BOOT_OP_WAIT_CONFIG, PRESET_BOOT_OP_SUCCESS, PRESET_BOOT_OP_FAILED };

  PresetBootOperationState presetBootOperationState = PRESET_BOOT_OP_IDLE;

  uint8_t presetBootTargetId = 0;
  String presetBootTargetName = "NONE";

  unsigned long presetBootResultStartMs = 0;

  // =========================================================
  // Touch state
  // =========================================================

  M5StackTouchRuntimeState touchState;

  // =========================================================
  // Hue gesture
  // =========================================================

  CHSV32 hueEditHsv;

  bool hueEditValid = false;

  uint8_t hueEditValue = 0;
  uint8_t hueEditWhite = 0;

  // =========================================================
  // Saturation gesture
  // =========================================================

  CHSV32 saturationEditHsv;

  bool saturationEditValid = false;

  uint8_t saturationEditValue = 0;
  uint8_t saturationEditWhite = 0;

  // =========================================================
  // Layout
  // =========================================================

  static constexpr int16_t POWER_BUTTON_X = 8;
  static constexpr int16_t POWER_BUTTON_Y = 8;
  static constexpr int16_t POWER_BUTTON_W = 44;
  static constexpr int16_t POWER_BUTTON_H = 44;

  static constexpr int16_t HEADER_CONTENT_LEFT = 60;
  static constexpr int16_t HEADER_CONTENT_RIGHT = 312;

  static constexpr int16_t HEADER_CENTER_X = ( HEADER_CONTENT_LEFT + HEADER_CONTENT_RIGHT ) / 2;

  static constexpr int16_t HEADER_TITLE_Y = 18;
  static constexpr int16_t HEADER_IP_Y = 41;

  // MAIN status row is split into:
  //   left  = network / Recovery AP status
  //   right = display-only battery icon + percentage
  static constexpr int16_t HEADER_NETWORK_LEFT = 60;
  static constexpr int16_t HEADER_NETWORK_RIGHT = 238;
  static constexpr int16_t HEADER_NETWORK_CENTER_X =
    ( HEADER_NETWORK_LEFT + HEADER_NETWORK_RIGHT ) / 2;

  static constexpr int16_t BATTERY_STATUS_LEFT = 244;
  static constexpr int16_t BATTERY_STATUS_RIGHT = 312;

  static constexpr int16_t BATTERY_ICON_X = 246;
  static constexpr int16_t BATTERY_ICON_Y = 36;
  static constexpr int16_t BATTERY_ICON_W = 18;
  static constexpr int16_t BATTERY_ICON_H = 10;

  static constexpr int16_t BATTERY_PERCENT_X = 289;
  static constexpr int16_t BATTERY_PERCENT_Y = 41;

  static constexpr int16_t CONTROL_LEFT_X = 16;
  static constexpr int16_t CONTROL_RIGHT_X = 240;

  static constexpr int16_t CONTROL_BUTTON_W = 64;
  static constexpr int16_t CONTROL_BUTTON_H = 34;

  // =========================================================
  // MAIN layout
  // =========================================================

  static constexpr int16_t BRI_BUTTON_Y = 82;
  static constexpr int16_t FX_BUTTON_Y = 138;

  static constexpr int16_t EFFECT_DETAIL_X = 88;
  static constexpr int16_t EFFECT_DETAIL_Y = 138;
  static constexpr int16_t EFFECT_DETAIL_W = 144;
  static constexpr int16_t EFFECT_DETAIL_H = 34;

  // =========================================================
  // MAIN bottom visible buttons
  // =========================================================

  static constexpr int16_t MAIN_BOTTOM_BUTTON_Y = 188;
  static constexpr int16_t MAIN_BOTTOM_BUTTON_H = 40;

  static constexpr int16_t COLOR_BUTTON_X = 16;
  static constexpr int16_t COLOR_BUTTON_W = 140;

  static constexpr int16_t PRESET_OPEN_BUTTON_X = 164;
  static constexpr int16_t PRESET_OPEN_BUTTON_W = 140;

  // =========================================================
  // Back button
  // =========================================================

  static constexpr int16_t BACK_BUTTON_X = 268;
  static constexpr int16_t BACK_BUTTON_Y = 8;
  static constexpr int16_t BACK_BUTTON_W = 44;
  static constexpr int16_t BACK_BUTTON_H = 44;

  // =========================================================
  // COLOR layout
  // =========================================================

  static constexpr int16_t COLOR_SLOT_1_X = 32;
  static constexpr int16_t COLOR_SLOT_2_X = 124;
  static constexpr int16_t COLOR_SLOT_3_X = 216;

  static constexpr int16_t COLOR_SLOT_Y = 76;
  static constexpr int16_t COLOR_SLOT_W = 72;
  static constexpr int16_t COLOR_SLOT_H = 28;

  static constexpr int16_t COLOR_SLOT_LABEL_Y = 68;
  static constexpr int16_t COLOR_SELECTED_INFO_Y = 121;

  static constexpr int16_t HUE_BUTTON_Y = 151;

  static constexpr int16_t SATURATION_LABEL_Y = 198;
  static constexpr int16_t SATURATION_BUTTON_Y = 204;

  // =========================================================
  // EFFECT detail layout
  // =========================================================

  static constexpr int16_t SPEED_BUTTON_Y = 82;

  static constexpr int16_t INTENSITY_BUTTON_Y = 140;

  // =========================================================
  // Palette visible layout
  // =========================================================

  static constexpr int16_t PALETTE_LABEL_Y = 188;
  static constexpr int16_t PALETTE_BUTTON_Y = 198;

  // =========================================================
  // PRESET screen layout
  // =========================================================

  static constexpr int16_t PRESET_NAME_Y = 92;
  static constexpr int16_t PRESET_ID_Y = 128;
  static constexpr int16_t PRESET_STATUS_Y = 154;

  static constexpr int16_t PRESET_NAV_LABEL_Y = 188;
  static constexpr int16_t PRESET_NAV_BUTTON_Y = 198;

  // =========================================================
  // PRESET MANAGE button on navigation screen
  // =========================================================

  static constexpr int16_t PRESET_MANAGE_BUTTON_X = 88;
  static constexpr int16_t PRESET_MANAGE_BUTTON_Y = 198;
  static constexpr int16_t PRESET_MANAGE_BUTTON_W = 144;
  static constexpr int16_t PRESET_MANAGE_BUTTON_H = 34;

  // =========================================================
  // PRESET MANAGE screen
  // =========================================================

  static constexpr int16_t PRESET_SAVE_NEW_BUTTON_X = 60;
  static constexpr int16_t PRESET_SAVE_NEW_BUTTON_Y = 64;
  static constexpr int16_t PRESET_SAVE_NEW_BUTTON_W = 200;
  static constexpr int16_t PRESET_SAVE_NEW_BUTTON_H = 34;

  // =========================================================
  // PRESET MANAGE OVERWRITE button
  // =========================================================

  static constexpr int16_t PRESET_OVERWRITE_BUTTON_X = 60;
  static constexpr int16_t PRESET_OVERWRITE_BUTTON_Y = 106;
  static constexpr int16_t PRESET_OVERWRITE_BUTTON_W = 200;
  static constexpr int16_t PRESET_OVERWRITE_BUTTON_H = 34;

  // =========================================================
  // PRESET MANAGE DELETE button
  // =========================================================

  static constexpr int16_t PRESET_DELETE_BUTTON_X = 60;
  static constexpr int16_t PRESET_DELETE_BUTTON_Y = 148;
  static constexpr int16_t PRESET_DELETE_BUTTON_W = 200;
  static constexpr int16_t PRESET_DELETE_BUTTON_H = 34;

  // =========================================================
  // PRESET MANAGE BOOT PRESET button
  // =========================================================

  static constexpr int16_t PRESET_BOOT_BUTTON_X = 60;
  static constexpr int16_t PRESET_BOOT_BUTTON_Y = 190;
  static constexpr int16_t PRESET_BOOT_BUTTON_W = 200;
  static constexpr int16_t PRESET_BOOT_BUTTON_H = 34;

  // =========================================================
  // PRESET SAVE confirmation screen
  // =========================================================

  static constexpr int16_t PRESET_SAVE_HOLD_BUTTON_X = 60;
  static constexpr int16_t PRESET_SAVE_HOLD_BUTTON_Y = 180;
  static constexpr int16_t PRESET_SAVE_HOLD_BUTTON_W = 200;
  static constexpr int16_t PRESET_SAVE_HOLD_BUTTON_H = 44;

  // =========================================================
  // PRESET OVERWRITE selection / confirmation screen
  // =========================================================

  static constexpr int16_t PRESET_OVERWRITE_NAV_BUTTON_Y = 124;

  static constexpr int16_t PRESET_OVERWRITE_HOLD_BUTTON_X = 60;
  static constexpr int16_t PRESET_OVERWRITE_HOLD_BUTTON_Y = 194;
  static constexpr int16_t PRESET_OVERWRITE_HOLD_BUTTON_W = 200;
  static constexpr int16_t PRESET_OVERWRITE_HOLD_BUTTON_H = 40;

  // =========================================================
  // PRESET DELETE selection / confirmation screen
  // =========================================================

  static constexpr int16_t PRESET_DELETE_NAV_BUTTON_Y = 124;

  static constexpr int16_t PRESET_DELETE_HOLD_BUTTON_X = 60;
  static constexpr int16_t PRESET_DELETE_HOLD_BUTTON_Y = 194;
  static constexpr int16_t PRESET_DELETE_HOLD_BUTTON_W = 200;
  static constexpr int16_t PRESET_DELETE_HOLD_BUTTON_H = 40;

  // =========================================================
  // PRESET BOOT selection / confirmation screen
  // =========================================================

  static constexpr int16_t PRESET_BOOT_NAV_BUTTON_Y = 124;

  static constexpr int16_t PRESET_BOOT_HOLD_BUTTON_X = 60;
  static constexpr int16_t PRESET_BOOT_HOLD_BUTTON_Y = 194;
  static constexpr int16_t PRESET_BOOT_HOLD_BUTTON_W = 200;
  static constexpr int16_t PRESET_BOOT_HOLD_BUTTON_H = 40;

  // =========================================================
  // Touch timing
  // =========================================================

  static constexpr unsigned long TOUCH_POLL_MS = 15;

  static constexpr unsigned long TOUCH_RELEASE_CONFIRM_MS = 70;

  static constexpr unsigned long TOUCH_ACTION_COOLDOWN_MS = 250;

  // Recovery AP is intentionally harder to trigger than ordinary UI
  // actions because it temporarily exposes a Wi-Fi access point.
  static constexpr unsigned long WIFI_RECOVERY_HOLD_MS = 1500;
  static constexpr unsigned long WIFI_RECOVERY_REOPEN_MS = 1000;

  // =========================================================
  // COLOR slot behavior
  // =========================================================

  // Deliberately longer than the repeat-control threshold. BLACK is
  // reversible, but should still require an intentional hold.
  static constexpr unsigned long COLOR_SLOT_LONG_PRESS_MS = 600;

  // =========================================================
  // Brightness behavior
  // =========================================================

  static constexpr unsigned long BRI_LONG_PRESS_MS = 400;
  static constexpr unsigned long BRI_REPEAT_MS = 80;

  static constexpr int BRI_SHORT_STEP = 1;
  static constexpr int BRI_LONG_STEP = 5;

  // =========================================================
  // Effect behavior
  // =========================================================

  static constexpr unsigned long EFFECT_LONG_PRESS_MS = 400;
  static constexpr unsigned long EFFECT_REPEAT_MS = 250;

  // =========================================================
  // Hue behavior
  // =========================================================

  static constexpr unsigned long HUE_LONG_PRESS_MS = 400;
  static constexpr unsigned long HUE_REPEAT_MS = 80;

  static constexpr int HUE_SHORT_STEP = 1;
  static constexpr int HUE_LONG_STEP = 5;

  // =========================================================
  // Saturation behavior
  // =========================================================

  static constexpr unsigned long SATURATION_LONG_PRESS_MS = 400;
  static constexpr unsigned long SATURATION_REPEAT_MS = 80;

  static constexpr int SATURATION_SHORT_STEP = 1;
  static constexpr int SATURATION_LONG_STEP = 5;

  // =========================================================
  // Speed behavior
  // =========================================================

  static constexpr unsigned long SPEED_LONG_PRESS_MS = 400;
  static constexpr unsigned long SPEED_REPEAT_MS = 80;

  static constexpr int SPEED_SHORT_STEP = 1;
  static constexpr int SPEED_LONG_STEP = 5;

  // =========================================================
  // Intensity behavior
  // =========================================================

  static constexpr unsigned long INTENSITY_LONG_PRESS_MS = 400;
  static constexpr unsigned long INTENSITY_REPEAT_MS = 80;

  static constexpr int INTENSITY_SHORT_STEP = 1;
  static constexpr int INTENSITY_LONG_STEP = 5;

  // =========================================================
  // Palette behavior
  // =========================================================

  static constexpr unsigned long PALETTE_LONG_PRESS_MS = 400;
  static constexpr unsigned long PALETTE_REPEAT_MS = 250;

  // =========================================================
  // Preset behavior
  // =========================================================

  static constexpr unsigned long PRESET_LONG_PRESS_MS = 400;
  static constexpr unsigned long PRESET_REPEAT_MS = 600;

  // =========================================================
  // Shared long press / repeat timing helpers
  // =========================================================




  // =========================================================
  // Normal LCD brightness
  // =========================================================

  uint8_t getNormalDisplayBrightness() {
    return (uint8_t)constrain( (int)lcdBrightness, 1, 255 );
  }

  // =========================================================
  // Sleep timeout
  // =========================================================

  unsigned long getSleepTimeoutMs() {
    return (unsigned long)sleepTimeoutSec * 1000UL;
  }

  // =========================================================
  // Fade interval calculation
  // =========================================================

  unsigned long getFadeIntervalMs() {
    uint16_t normalBrightness = getNormalDisplayBrightness();

    uint16_t steps = ( normalBrightness + DISPLAY_FADE_STEP - 1 ) / DISPLAY_FADE_STEP;

    if ( steps == 0 ) {
      steps = 1;
    }

    unsigned long interval = (unsigned long)fadeDurationMs / steps;

    if ( interval < 1 ) {
      interval = 1;
    }

    return interval;
  }

  // =========================================================
  // Hardware backend facade
  //
  // Keep board-specific implementation behind one backend while
  // preserving the existing usermod call sites. This minimizes the
  // regression surface for the hardware-verified CoreS3 UI.
  // =========================================================

  const char* getHardwareProbeStateName() {
    return hardwareBackend.probeStateName();
  }

  const char* getDetectedPmuName() {
    return hardwareBackend.detectedPmuName();
  }

  const char* getDetectedImuName() {
    return hardwareBackend.detectedImuName();
  }

  const char* getDetectedVariantName() {
    return hardwareBackend.detectedVariantName();
  }

  bool isCore2FamilyProfile() {
    return hardwareBackend.isCore2FamilyProfile();
  }

  bool isCore2DiagnosticOnlyMode() {
    return hardwareBackend.isCore2DiagnosticOnlyMode();
  }

  const char* getHardwareRuntimeModeName() {
    return hardwareBackend.runtimeModeName();
  }

  const char* getHardwarePortStatusName() {
    return hardwareBackend.portStatusName();
  }

  const char* getDetectedRevisionName() {
    return hardwareBackend.detectedRevisionName();
  }

  void runHardwareDiagnostics() {
    hardwareBackend.runDiagnostics();
  }

  const char* getHardwareProfileName() {
    return hardwareBackend.profileName();
  }

  const char* getHardwareRevisionName() {
    return hardwareBackend.revisionName();
  }

  bool isHardwareDisplayRuntimeEnabled() {
    return hardwareBackend.isDisplayRuntimeEnabled();
  }

  bool initializeDisplayHardware() {
    return hardwareBackend.initializeDisplay( screenWidth, screenHeight, touchReady );
  }

  bool readDisplayTouch( int16_t& touchX, int16_t& touchY ) {
    return hardwareBackend.readTouch( touchX, touchY );
  }

  void writeDisplayBrightness( uint8_t value ) {
    hardwareBackend.writeBrightness( value );
  }

  // =========================================================
  // Display brightness
  // =========================================================

  void setDisplayBrightness( uint8_t value ) {
    currentDisplayBrightness = value;

    writeDisplayBrightness( value );
  }

  // =========================================================
  // Generic Fade
  // =========================================================

  bool updateFade( uint8_t targetBrightness, unsigned long now, unsigned long& lastFadeStep ) {
    if ( currentDisplayBrightness == targetBrightness ) {
      return true;
    }

    if (!fadeEnabled) {
      setDisplayBrightness( targetBrightness );

      return true;
    }

    unsigned long fadeInterval = getFadeIntervalMs();

    if ( now - lastFadeStep < fadeInterval ) {
      return false;
    }

    lastFadeStep = now;

    if ( currentDisplayBrightness < targetBrightness ) {
      int nextValue = currentDisplayBrightness + DISPLAY_FADE_STEP;

      if ( nextValue > targetBrightness ) {
        nextValue = targetBrightness;
      }

      setDisplayBrightness( (uint8_t)nextValue );
    }
    else {
      int nextValue = currentDisplayBrightness - DISPLAY_FADE_STEP;

      if ( nextValue < targetBrightness ) {
        nextValue = targetBrightness;
      }

      setDisplayBrightness( (uint8_t)nextValue );
    }

    return ( currentDisplayBrightness == targetBrightness );
  }

  // =========================================================
  // Preset cache rebuild start
  // =========================================================

  void startPresetCacheRebuild() {
    presetCacheCount = 0;

    presetCacheScanId = 1;

    presetCacheLastScanMs = 0;

    presetCacheReady = false;

    presetCacheBuilding = true;

    presetNoEntries = false;

    presetCacheBuildSourceModifiedTime = presetsModifiedTime;

    Serial.printf( "[CoreS3_Display] " "Preset cache rebuild start " "(modified=%lu)\n", presetCacheBuildSourceModifiedTime );
  }

  // =========================================================
  // Preset cache rebuild complete
  // =========================================================

  void finishPresetCacheRebuild() {
    presetCacheBuilding = false;

    presetCacheReady = true;

    presetCacheSourceModifiedTime = presetCacheBuildSourceModifiedTime;

    presetNoEntries = ( presetCacheCount == 0 );

    normalizePresetNavigationCursor();

    syncPresetNavigationCursorFromCurrentPreset();

    Serial.printf( "[CoreS3_Display] " "Preset cache ready: %u preset(s)\n", (unsigned)presetCacheCount );

    if ( presetsModifiedTime != presetCacheSourceModifiedTime ) {
      Serial.println( F( "[CoreS3_Display] " "Preset changed during cache build. Rebuilding." ) );

      startPresetCacheRebuild();

      return;
    }

    if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_NAV && displayPowerState == DISPLAY_POWER_ACTIVE && touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      drawPresetDetails( getDisplayedPresetId(), pendingPresetId > 0 );

      drawPresetNavigation( M5STACK_TOUCH_TARGET_NONE );

      lastPresetValue = getDisplayedPresetId();
    }
    else if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_MANAGE && displayPowerState == DISPLAY_POWER_ACTIVE && touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      drawPresetManageScreen();
    }
    else if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_SAVE && presetSaveOperationState == PRESET_SAVE_OP_IDLE && displayPowerState == DISPLAY_POWER_ACTIVE && touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      preparePresetSaveCandidate();
      drawPresetSaveScreen();
    }
    else if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_OVERWRITE && presetSaveOperationState == PRESET_SAVE_OP_IDLE && displayPowerState == DISPLAY_POWER_ACTIVE && touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      String refreshedName;

      if ( presetOverwriteTargetId == 0 || !getCachedPresetName( presetOverwriteTargetId, refreshedName ) ) {
        preparePresetOverwriteTarget();
      }
      else {
        presetOverwriteTargetName = refreshedName;
      }

      drawPresetOverwriteScreen();
    }
    else if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_DELETE && presetDeleteOperationState == PRESET_DELETE_OP_IDLE && displayPowerState == DISPLAY_POWER_ACTIVE && touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      String refreshedName;

      if ( presetDeleteTargetId == 0 || !getCachedPresetName( presetDeleteTargetId, refreshedName ) ) {
        preparePresetDeleteTarget();
      }
      else {
        presetDeleteTargetName = refreshedName;
      }

      drawPresetDeleteScreen();
    }
    else if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_BOOT && presetBootOperationState == PRESET_BOOT_OP_IDLE && displayPowerState == DISPLAY_POWER_ACTIVE && touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      if ( presetBootTargetId > 0 ) {
        String refreshedName;

        if ( getCachedPresetName( presetBootTargetId, refreshedName ) ) {
          presetBootTargetName = refreshedName;
        }
        else {
          presetBootTargetId = 0;
          presetBootTargetName = "NONE";
        }
      }

      drawPresetBootScreen();
    }
  }

  // =========================================================
  // Background Preset cache service
  // =========================================================

  void servicePresetCache() {
    if ( presetCacheReady && !presetCacheBuilding && presetsModifiedTime != presetCacheSourceModifiedTime ) {
      startPresetCacheRebuild();
    }

    if (!presetCacheBuilding) {
      return;
    }

    if ( pendingPresetId > 0 ) {
      return;
    }

    if ( presetNeedsSaving() ) {
      return;
    }

    unsigned long now = millis();

    if ( now - presetCacheLastScanMs < PRESET_CACHE_SCAN_INTERVAL_MS ) {
      return;
    }

    presetCacheLastScanMs = now;

    if ( presetCacheScanId > 250 ) {
      finishPresetCacheRebuild();

      return;
    }

    String presetName;

    uint8_t scanId = (uint8_t)presetCacheScanId;

    if ( getPresetName( scanId, presetName ) ) {
      if ( presetCacheCount < 250 ) {
        presetCache[ presetCacheCount ].id = scanId;

        strlcpy( presetCache[ presetCacheCount ].name, presetName.c_str(), sizeof( presetCache[ presetCacheCount ].name ) );

        presetCacheCount++;
      }
    }

    presetCacheScanId++;

    if ( presetCacheScanId > 250 ) {
      finishPresetCacheRebuild();
    }
  }

  // =========================================================
  // Find Preset in RAM cache
  // =========================================================

  int findPresetCacheIndex( uint8_t presetId ) {
    for ( uint16_t i = 0; i < presetCacheCount; i++ ) {
      if ( presetCache[i].id == presetId ) {
        return (int)i;
      }
    }

    return -1;
  }

  bool getCachedPresetName( uint8_t presetId, String& name ) {
    int index = findPresetCacheIndex( presetId );

    if ( index < 0 ) {
      return false;
    }

    name = presetCache[ index ].name;

    return true;
  }

  uint8_t findFirstFreePresetId() {
    if ( !presetCacheReady || presetCacheBuilding ) {
      return 0;
    }

    uint16_t expectedId = 1;

    for ( uint16_t i = 0; i < presetCacheCount; i++ ) {
      uint8_t cachedId = presetCache[i].id;

      if ( cachedId < expectedId ) {
        continue;
      }

      if ( cachedId == expectedId ) {
        expectedId++;

        if ( expectedId > 250 ) {
          return 0;
        }

        continue;
      }

      break;
    }

    if ( expectedId >= 1 && expectedId <= 250 ) {
      return (uint8_t)expectedId;
    }

    return 0;
  }

  bool preparePresetSaveCandidate() {
    presetSaveCandidateId = findFirstFreePresetId();

    presetSaveCandidateName = "";

    if ( presetSaveCandidateId == 0 ) {
      return false;
    }

    char presetName[33];

    snprintf( presetName, sizeof(presetName), "CoreS3 Preset %u", presetSaveCandidateId );

    presetSaveCandidateName = presetName;

    return true;
  }
  bool preparePresetOverwriteTarget() {
    presetOverwriteTargetId = 0;

    presetOverwriteTargetName = "";

    if ( !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 ) {
      return false;
    }

    uint8_t basePresetId = getPresetManagementBaseId();

    int targetIndex = findPresetCacheIndex( basePresetId );

    if ( targetIndex < 0 ) {
      targetIndex = 0;
    }

    presetOverwriteTargetId = presetCache[ targetIndex ].id;

    presetOverwriteTargetName = presetCache[ targetIndex ].name;

    return true;
  }

  bool stepPresetOverwriteTarget( int direction ) {
    if ( direction == 0 || !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 ) {
      return false;
    }

    String targetName;

    uint8_t targetId = findAdjacentPreset( presetOverwriteTargetId, direction, &targetName );

    if ( targetId == 0 ) {
      return false;
    }

    presetOverwriteTargetId = targetId;

    presetOverwriteTargetName = targetName;

    return true;
  }
  bool preparePresetDeleteTarget() {
    presetDeleteTargetId = 0;

    presetDeleteTargetName = "";

    if ( !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 ) {
      return false;
    }

    uint8_t basePresetId = getPresetManagementBaseId();

    int targetIndex = findPresetCacheIndex( basePresetId );

    if ( targetIndex < 0 ) {
      targetIndex = 0;
    }

    presetDeleteTargetId = presetCache[ targetIndex ].id;

    presetDeleteTargetName = presetCache[ targetIndex ].name;

    return true;
  }

  bool stepPresetDeleteTarget( int direction ) {
    if ( direction == 0 || !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 ) {
      return false;
    }

    String targetName;

    uint8_t targetId = findAdjacentPreset( presetDeleteTargetId, direction, &targetName );

    if ( targetId == 0 ) {
      return false;
    }

    presetDeleteTargetId = targetId;

    presetDeleteTargetName = targetName;

    return true;
  }
  bool preparePresetBootTarget() {
    presetBootTargetId = 0;
    presetBootTargetName = "NONE";

    if ( !presetCacheReady || presetCacheBuilding ) {
      return false;
    }

    if ( isPresetNavigationCursorValid() ) {
      String cursorName;

      if ( getCachedPresetName( presetNavigationCursorId, cursorName ) ) {
        presetBootTargetId = presetNavigationCursorId;
        presetBootTargetName = cursorName;

        return true;
      }
    }

    if ( currentPreset > 0 ) {
      String currentName;

      if ( getCachedPresetName( currentPreset, currentName ) ) {
        presetBootTargetId = currentPreset;
        presetBootTargetName = currentName;

        return true;
      }
    }

    if ( bootPreset > 0 ) {
      String bootName;

      if ( getCachedPresetName( bootPreset, bootName ) ) {
        presetBootTargetId = bootPreset;
        presetBootTargetName = bootName;
      }
    }

    return true;
  }

  bool stepPresetBootTarget( int direction ) {
    if ( direction == 0 || !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 ) {
      return false;
    }

    if ( presetBootTargetId == 0 ) {
      uint16_t targetIndex = ( direction > 0 ) ? 0 : ( presetCacheCount - 1 );

      presetBootTargetId = presetCache[targetIndex].id;
      presetBootTargetName = presetCache[targetIndex].name;

      return true;
    }

    int currentIndex = findPresetCacheIndex( presetBootTargetId );

    if ( currentIndex < 0 ) {
      presetBootTargetId = 0;
      presetBootTargetName = "NONE";

      return true;
    }

    if ( direction > 0 ) {
      if ( currentIndex >= (int)presetCacheCount - 1 ) {
        presetBootTargetId = 0;
        presetBootTargetName = "NONE";
      }
      else {
        presetBootTargetId = presetCache[currentIndex + 1].id;
        presetBootTargetName = presetCache[currentIndex + 1].name;
      }
    }
    else {
      if ( currentIndex <= 0 ) {
        presetBootTargetId = 0;
        presetBootTargetName = "NONE";
      }
      else {
        presetBootTargetId = presetCache[currentIndex - 1].id;
        presetBootTargetName = presetCache[currentIndex - 1].name;
      }
    }

    return true;
  }

  bool isPresetSaveBusy() {
    return ( presetSaveOperationState != PRESET_SAVE_OP_IDLE );
  }

  bool isPresetDeleteBusy() {
    return ( presetDeleteOperationState != PRESET_DELETE_OP_IDLE );
  }

  bool isPresetBootBusy() {
    return ( presetBootOperationState != PRESET_BOOT_OP_IDLE );
  }

  bool requestNewPresetSave() {
    presetSaveOperationIsOverwrite = false;

    if ( presetSaveOperationState != PRESET_SAVE_OP_IDLE || !presetCacheReady || presetCacheBuilding || pendingPresetId > 0 || presetSaveCandidateId == 0 || findPresetCacheIndex( presetSaveCandidateId ) >= 0 || presetNeedsSaving() ) {
      presetSaveOperationState = PRESET_SAVE_OP_FAILED;

      presetSaveResultStartMs = millis();

      drawPresetSaveOperationStatus();

      Serial.println( F( "[CoreS3_Display] " "Preset save request rejected" ) );

      return false;
    }

    savePreset( presetSaveCandidateId, presetSaveCandidateName.c_str() );

    if ( !presetNeedsSaving() ) {
      presetSaveOperationState = PRESET_SAVE_OP_FAILED;

      presetSaveResultStartMs = millis();

      drawPresetSaveOperationStatus();

      Serial.println( F( "[CoreS3_Display] " "Preset save could not be queued" ) );

      return false;
    }

    presetSaveOperationState = PRESET_SAVE_OP_WAIT_WLED;

    presetSaveResultStartMs = 0;

    lastUserActivityMs = millis();

    drawPresetSaveOperationStatus();

    Serial.printf( "[CoreS3_Display] " "Preset save request: %u (%s)\n", presetSaveCandidateId, presetSaveCandidateName.c_str() );

    return true;
  }

  bool requestPresetOverwrite() {
    presetSaveOperationIsOverwrite = true;

    presetSaveCandidateId = presetOverwriteTargetId;

    presetSaveCandidateName = presetOverwriteTargetName;

    if ( presetSaveOperationState != PRESET_SAVE_OP_IDLE || !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 || pendingPresetId > 0 || presetSaveCandidateId == 0 || findPresetCacheIndex( presetSaveCandidateId ) < 0 || presetSaveCandidateName.length() == 0 || presetNeedsSaving() ) {
      presetSaveOperationState = PRESET_SAVE_OP_FAILED;

      presetSaveResultStartMs = millis();

      drawPresetSaveOperationStatus();

      Serial.println( F( "[CoreS3_Display] " "Preset overwrite request rejected" ) );

      return false;
    }

    savePreset( presetSaveCandidateId, presetSaveCandidateName.c_str() );

    if ( !presetNeedsSaving() ) {
      presetSaveOperationState = PRESET_SAVE_OP_FAILED;

      presetSaveResultStartMs = millis();

      drawPresetSaveOperationStatus();

      Serial.println( F( "[CoreS3_Display] " "Preset overwrite could not be queued" ) );

      return false;
    }

    presetSaveOperationState = PRESET_SAVE_OP_WAIT_WLED;

    presetSaveResultStartMs = 0;

    lastUserActivityMs = millis();

    drawPresetSaveOperationStatus();

    Serial.printf( "[CoreS3_Display] " "Preset overwrite request: %u (%s)\n", presetSaveCandidateId, presetSaveCandidateName.c_str() );

    return true;
  }

  bool requestPresetDelete() {
    if ( presetDeleteOperationState != PRESET_DELETE_OP_IDLE || !presetCacheReady || presetCacheBuilding || presetCacheCount == 0 || pendingPresetId > 0 || presetDeleteTargetId == 0 || findPresetCacheIndex( presetDeleteTargetId ) < 0 || presetDeleteTargetName.length() == 0 || presetNeedsSaving() ) {
      presetDeleteOperationState = PRESET_DELETE_OP_FAILED;

      presetDeleteResultStartMs = millis();

      drawPresetDeleteOperationStatus();

      Serial.println( F( "[CoreS3_Display] " "Preset delete request rejected" ) );

      return false;
    }

    presetDeleteWasCurrentPreset = ( currentPreset == presetDeleteTargetId );
    presetDeleteWasBootPreset = ( bootPreset == presetDeleteTargetId );

    deletePreset( presetDeleteTargetId );

    if ( !presetCacheBuilding ) {
      startPresetCacheRebuild();
    }

    presetDeleteOperationState = PRESET_DELETE_OP_WAIT_CACHE;

    presetDeleteResultStartMs = 0;

    lastUserActivityMs = millis();

    drawPresetDeleteOperationStatus();

    Serial.printf( "[CoreS3_Display] " "Preset delete request: %u (%s)\n", presetDeleteTargetId, presetDeleteTargetName.c_str() );

    return true;
  }

  bool requestPresetBootSetting() {
    bool validTarget =
      presetBootTargetId == 0 ||
      findPresetCacheIndex( presetBootTargetId ) >= 0;

    if ( presetBootOperationState != PRESET_BOOT_OP_IDLE ||
         !presetCacheReady ||
         presetCacheBuilding ||
         pendingPresetId > 0 ||
         presetNeedsSaving() ||
         !validTarget ||
         presetBootTargetId == bootPreset ) {
      presetBootOperationState = PRESET_BOOT_OP_FAILED;
      presetBootResultStartMs = millis();

      drawPresetBootOperationStatus();

      Serial.println( F( "[CoreS3_Display] " "Boot Preset request rejected" ) );

      return false;
    }

    bootPreset = presetBootTargetId;
    configNeedsWrite = true;

    presetBootOperationState = PRESET_BOOT_OP_WAIT_CONFIG;
    presetBootResultStartMs = 0;
    lastUserActivityMs = millis();

    drawPresetBootOperationStatus();

    if ( presetBootTargetId == 0 ) {
      Serial.println( F( "[CoreS3_Display] " "Boot Preset clear request" ) );
    }
    else {
      Serial.printf( "[CoreS3_Display] " "Boot Preset request: %u (%s)\n", presetBootTargetId, presetBootTargetName.c_str() );
    }

    return true;
  }

  void servicePresetSaveOperation() {
    if ( presetSaveOperationState == PRESET_SAVE_OP_IDLE ) {
      return;
    }

    unsigned long now = millis();

    if ( presetSaveOperationState == PRESET_SAVE_OP_WAIT_WLED ) {
      if ( presetNeedsSaving() ) {
        return;
      }

      if ( !presetCacheBuilding ) {
        startPresetCacheRebuild();
      }

      presetSaveOperationState = PRESET_SAVE_OP_WAIT_CACHE;

      if ( currentPage == SCREEN_PRESET && ( presetSubPage == PRESET_SUBPAGE_SAVE || presetSubPage == PRESET_SUBPAGE_OVERWRITE ) && displayPowerState == DISPLAY_POWER_ACTIVE ) {
        drawPresetSaveOperationStatus();
      }

      return;
    }

    if ( presetSaveOperationState == PRESET_SAVE_OP_WAIT_CACHE ) {
      if ( !presetCacheReady || presetCacheBuilding ) {
        return;
      }

      bool presetVerified = ( findPresetCacheIndex( presetSaveCandidateId ) >= 0 );

      if ( presetVerified && presetSaveOperationIsOverwrite ) {
        String verifiedName;

        presetVerified = getCachedPresetName( presetSaveCandidateId, verifiedName ) && verifiedName == presetSaveCandidateName;
      }

      if (presetVerified) {
        presetSaveOperationState = PRESET_SAVE_OP_SUCCESS;

        presetNavigationCursorId = presetSaveCandidateId;

        // The save operation intentionally changes the CoreS3 navigation
        // position without necessarily changing WLED's active Preset.
        lastObservedCurrentPreset = currentPreset;

        Serial.printf( "[CoreS3_Display] " "%s verified: %u (%s)\n", presetSaveOperationIsOverwrite ? "Preset overwrite" : "Preset save", presetSaveCandidateId, presetSaveCandidateName.c_str() );
      }
      else {
        presetSaveOperationState = PRESET_SAVE_OP_FAILED;

        Serial.printf( "[CoreS3_Display] " "%s verification failed: %u\n", presetSaveOperationIsOverwrite ? "Preset overwrite" : "Preset save", presetSaveCandidateId );
      }

      presetSaveResultStartMs = now;

      if ( currentPage == SCREEN_PRESET && ( presetSubPage == PRESET_SUBPAGE_SAVE || presetSubPage == PRESET_SUBPAGE_OVERWRITE ) && displayPowerState == DISPLAY_POWER_ACTIVE ) {
        drawPresetSaveOperationStatus();
      }

      return;
    }

    if ( presetSaveOperationState == PRESET_SAVE_OP_SUCCESS || presetSaveOperationState == PRESET_SAVE_OP_FAILED ) {
      if ( now - presetSaveResultStartMs < PRESET_SAVE_RESULT_HOLD_MS ) {
        return;
      }

      int16_t touchX = -1;
      int16_t touchY = -1;

      if ( readDisplayTouch( touchX, touchY ) ) {
        return;
      }

      bool saveSucceeded = ( presetSaveOperationState == PRESET_SAVE_OP_SUCCESS );

      bool completedOverwrite = presetSaveOperationIsOverwrite;

      presetSaveOperationState = PRESET_SAVE_OP_IDLE;

      presetSaveOperationIsOverwrite = false;

      presetSaveResultStartMs = 0;

      presetSaveHoldStartTime = 0;

      presetSaveHoldTriggered = false;

      resetTouchGesture();

      if (completedOverwrite) {
        presetOverwriteTargetId = 0;

        presetOverwriteTargetName = "";
      }

      if (saveSucceeded) {
        presetSaveCandidateId = 0;

        presetSaveCandidateName = "";

        drawPresetScreen();
      }
      else {
        presetSaveCandidateId = 0;

        presetSaveCandidateName = "";

        drawPresetManageScreen();
      }
    }
  }

  void servicePresetDeleteOperation() {
    if ( presetDeleteOperationState == PRESET_DELETE_OP_IDLE ) {
      return;
    }

    unsigned long now = millis();

    if ( presetDeleteOperationState == PRESET_DELETE_OP_WAIT_CACHE ) {
      if ( !presetCacheReady || presetCacheBuilding ) {
        return;
      }

      bool deleteVerified = ( findPresetCacheIndex( presetDeleteTargetId ) < 0 );

      if (deleteVerified) {
        if ( presetDeleteWasCurrentPreset && currentPreset == presetDeleteTargetId ) {
          currentPreset = 0;

          lastPresetValue = -1;
        }

        if ( presetDeleteWasBootPreset && bootPreset == presetDeleteTargetId ) {
          bootPreset = 0;
          configNeedsWrite = true;
          lastBootPresetValue = -1;

          Serial.println( F( "[CoreS3_Display] " "Deleted Boot Preset cleared" ) );
        }

        presetDeleteOperationState = PRESET_DELETE_OP_SUCCESS;

        Serial.printf( "[CoreS3_Display] " "Preset delete verified: %u (%s)\n", presetDeleteTargetId, presetDeleteTargetName.c_str() );
      }
      else {
        presetDeleteOperationState = PRESET_DELETE_OP_FAILED;

        Serial.printf( "[CoreS3_Display] " "Preset delete verification failed: %u\n", presetDeleteTargetId );
      }

      presetDeleteResultStartMs = now;

      if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_DELETE && displayPowerState == DISPLAY_POWER_ACTIVE ) {
        drawPresetDeleteOperationStatus();
      }

      return;
    }

    if ( presetDeleteOperationState == PRESET_DELETE_OP_SUCCESS || presetDeleteOperationState == PRESET_DELETE_OP_FAILED ) {
      if ( now - presetDeleteResultStartMs < PRESET_SAVE_RESULT_HOLD_MS ) {
        return;
      }

      int16_t touchX = -1;
      int16_t touchY = -1;

      if ( readDisplayTouch( touchX, touchY ) ) {
        return;
      }

      bool deleteSucceeded = ( presetDeleteOperationState == PRESET_DELETE_OP_SUCCESS );

      presetDeleteOperationState = PRESET_DELETE_OP_IDLE;

      presetDeleteResultStartMs = 0;

      presetDeleteWasCurrentPreset = false;
      presetDeleteWasBootPreset = false;

      presetSaveHoldStartTime = 0;

      presetSaveHoldTriggered = false;

      resetTouchGesture();

      presetDeleteTargetId = 0;

      presetDeleteTargetName = "";

      if (deleteSucceeded) {
        drawPresetScreen();
      }
      else {
        drawPresetManageScreen();
      }
    }
  }

  void servicePresetBootOperation() {
    if ( presetBootOperationState == PRESET_BOOT_OP_IDLE ) {
      return;
    }

    unsigned long now = millis();

    if ( presetBootOperationState == PRESET_BOOT_OP_WAIT_CONFIG ) {
      if ( configNeedsWrite ) {
        return;
      }

      if ( bootPreset == presetBootTargetId ) {
        presetBootOperationState = PRESET_BOOT_OP_SUCCESS;

        if ( presetBootTargetId == 0 ) {
          Serial.println( F( "[CoreS3_Display] " "Boot Preset clear verified" ) );
        }
        else {
          Serial.printf( "[CoreS3_Display] " "Boot Preset verified: %u (%s)\n", presetBootTargetId, presetBootTargetName.c_str() );
        }
      }
      else {
        presetBootOperationState = PRESET_BOOT_OP_FAILED;

        Serial.printf( "[CoreS3_Display] " "Boot Preset verification failed: target=%u actual=%u\n", presetBootTargetId, bootPreset );
      }

      presetBootResultStartMs = now;
      lastBootPresetValue = bootPreset;

      if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_BOOT && displayPowerState == DISPLAY_POWER_ACTIVE ) {
        drawPresetBootOperationStatus();
      }

      return;
    }

    if ( presetBootOperationState == PRESET_BOOT_OP_SUCCESS || presetBootOperationState == PRESET_BOOT_OP_FAILED ) {
      if ( now - presetBootResultStartMs < PRESET_SAVE_RESULT_HOLD_MS ) {
        return;
      }

      int16_t touchX = -1;
      int16_t touchY = -1;

      if ( readDisplayTouch( touchX, touchY ) ) {
        return;
      }

      presetBootOperationState = PRESET_BOOT_OP_IDLE;
      presetBootResultStartMs = 0;
      presetSaveHoldStartTime = 0;
      presetSaveHoldTriggered = false;

      resetTouchGesture();

      presetBootTargetId = 0;
      presetBootTargetName = "NONE";

      drawPresetManageScreen();
    }
  }

  // =========================================================
  // Network access helpers
  // =========================================================

  NetworkAccessMode getNetworkAccessMode() {
    // Prefer the normal station connection when both STA and SoftAP are active.
    if ( WiFi.status() == WL_CONNECTED ) {
      return NETWORK_ACCESS_STA;
    }

    // WLED owns SoftAP lifecycle and exposes apActive through wled.h.
    // A running SoftAP is a valid network-ready state even though
    // WiFi.status() is not WL_CONNECTED.
    if ( apActive ) {
      return NETWORK_ACCESS_AP;
    }

    return NETWORK_ACCESS_NONE;
  }

  String getNetworkIPAddress( NetworkAccessMode mode ) {
    if ( mode == NETWORK_ACCESS_STA ) {
      return WiFi.localIP().toString();
    }

    if ( mode == NETWORK_ACCESS_AP ) {
      return WiFi.softAPIP().toString();
    }

    return "";
  }

  String getNetworkDisplayText( NetworkAccessMode mode ) {
    if ( mode == NETWORK_ACCESS_STA ) {
      return WiFi.localIP().toString();
    }

    if ( mode == NETWORK_ACCESS_AP ) {
      if ( recoveryApSessionActive && recoveryApSSID.length() > 0 ) {
        String displaySSID = recoveryApSSID;

        if ( displaySSID.length() > 18 ) {
          displaySSID = displaySSID.substring( 0, 18 );
        }

        return displaySSID + "  " + WiFi.softAPIP().toString();
      }

      return String( "AP: " ) + WiFi.softAPIP().toString();
    }

    return "Offline - Hold Recovery AP";
  }

  String getCurrentNetworkDisplayText() {
    return getNetworkDisplayText( getNetworkAccessMode() );
  }

  bool startWiFiRecoveryAP() {
    if ( getNetworkAccessMode() != NETWORK_ACCESS_NONE ) {
      return false;
    }

    recoveryApLastStartAttempt = millis();

    // WLED::initAP(true) deliberately uses WLED's compiled recovery/default
    // AP credentials and bypasses AP_BEHAVIOR_BUTTON_ONLY. Back up the live
    // config variables first, then restore them immediately so this emergency
    // session does not modify the user's stored AP configuration.
    char savedApSSID[ sizeof(apSSID) ];
    char savedApPass[ sizeof(apPass) ];

    strlcpy( savedApSSID, apSSID, sizeof(savedApSSID) );
    strlcpy( savedApPass, apPass, sizeof(savedApPass) );

    WLED::instance().initAP( true );

    String startedSSID = apSSID;

    strlcpy( apSSID, savedApSSID, sizeof(apSSID) );
    strlcpy( apPass, savedApPass, sizeof(apPass) );

    if ( !apActive ) {
      recoveryApSessionActive = false;
      recoveryApSSID = "";

      return false;
    }

    recoveryApSessionActive = true;
    recoveryApSSID = startedSSID;

    return true;
  }

  void serviceWiFiRecoveryAP() {
    if ( !recoveryApSessionActive ) {
      return;
    }

    if ( WiFi.status() == WL_CONNECTED ) {
      recoveryApSessionActive = false;
      recoveryApSSID = "";

      return;
    }

    if ( apActive ) {
      return;
    }

    const unsigned long now = millis();

    if ( now - recoveryApLastStartAttempt < WIFI_RECOVERY_REOPEN_MS ) {
      return;
    }

    // WLED's normal reconnect path may temporarily tear down SoftAP while it
    // retries STA. During an explicit recovery session, reopen the emergency
    // AP so the user retains a path back into the Web UI.
    startWiFiRecoveryAP();
  }

  // =========================================================
  // Runtime Health / Error UX
  // =========================================================

  uint8_t getRuntimeHealthWarningMask( RuntimeHealthWarning warning ) const {
    switch ( warning ) {
      case RUNTIME_HEALTH_WARNING_LED_POWER:
        return RUNTIME_HEALTH_SHOWN_LED_POWER;

      case RUNTIME_HEALTH_WARNING_POWER_SAFETY:
        return RUNTIME_HEALTH_SHOWN_POWER_SAFETY;

      case RUNTIME_HEALTH_WARNING_TOUCH:
        return RUNTIME_HEALTH_SHOWN_TOUCH;

      case RUNTIME_HEALTH_WARNING_NONE:
      default:
        return 0;
    }
  }

  RuntimeHealthWarning getNextRuntimeHealthWarning( unsigned long now ) {
    const bool graceExpired =
      now - runtimeHealthStartMs >= POWER_SAFETY_WARNING_GRACE_MS;

    if (
      !( runtimeHealthWarningsShownMask & RUNTIME_HEALTH_SHOWN_LED_POWER ) &&
      (
        ( coreS3PowerInitializationComplete() && !coreS3PowerExternal5VReady() ) ||
        ( graceExpired && !coreS3PowerInitializationComplete() )
      )
    ) {
      return RUNTIME_HEALTH_WARNING_LED_POWER;
    }

    if (
      !( runtimeHealthWarningsShownMask & RUNTIME_HEALTH_SHOWN_POWER_SAFETY ) &&
      graceExpired &&
      !coreS3PowerSafeShutdownMonitorReady()
    ) {
      return RUNTIME_HEALTH_WARNING_POWER_SAFETY;
    }

    if (
      !( runtimeHealthWarningsShownMask & RUNTIME_HEALTH_SHOWN_TOUCH ) &&
      !touchReady
    ) {
      return RUNTIME_HEALTH_WARNING_TOUCH;
    }

    return RUNTIME_HEALTH_WARNING_NONE;
  }

  void drawRuntimeHealthWarning( RuntimeHealthWarning warning ) {
    display.fillScreen( TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    if ( warning == RUNTIME_HEALTH_WARNING_LED_POWER ) {
      display.setTextColor( TFT_RED, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( "LED POWER ERROR", screenWidth / 2, 82 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( "External 5V unavailable", screenWidth / 2, 122 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
      display.drawString( "Check power path and reboot", screenWidth / 2, 151 );

      return;
    }

    if ( warning == RUNTIME_HEALTH_WARNING_POWER_SAFETY ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( "POWER SAFETY WARNING", screenWidth / 2, 82 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( "Safe shutdown unavailable", screenWidth / 2, 122 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
      display.drawString( "Use WLED power control", screenWidth / 2, 151 );

      return;
    }

    if ( warning == RUNTIME_HEALTH_WARNING_TOUCH ) {
      display.setTextColor( TFT_RED, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( "TOUCH ERROR", screenWidth / 2, 82 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( "Touch input unavailable", screenWidth / 2, 122 );

      display.setTextColor( TFT_CYAN, TFT_BLACK );
      display.drawString( "Use WLED Web UI", screenWidth / 2, 151 );
    }
  }

  bool serviceRuntimeHealthWarnings() {
    if (
      startupState != STARTUP_DONE ||
      displayPowerState != DISPLAY_POWER_ACTIVE ||
      currentPage != SCREEN_MAIN
    ) {
      return false;
    }

    const unsigned long now = millis();

    if ( activeRuntimeHealthWarning != RUNTIME_HEALTH_WARNING_NONE ) {
      if ( now - runtimeHealthWarningStartMs < RUNTIME_HEALTH_WARNING_HOLD_MS ) {
        return true;
      }

      runtimeHealthWarningsShownMask |=
        getRuntimeHealthWarningMask( activeRuntimeHealthWarning );

      activeRuntimeHealthWarning = RUNTIME_HEALTH_WARNING_NONE;
      runtimeHealthWarningStartMs = 0;

      drawMainScreen( getCurrentNetworkDisplayText() );

      lastNetworkAccessMode = getNetworkAccessMode();
      lastNetworkDisplayText = getCurrentNetworkDisplayText();
      lastUserActivityMs = now;

      return true;
    }

    if ( touchState.touchTarget != M5STACK_TOUCH_TARGET_NONE ) {
      return false;
    }

    const RuntimeHealthWarning nextWarning =
      getNextRuntimeHealthWarning( now );

    if ( nextWarning == RUNTIME_HEALTH_WARNING_NONE ) {
      return false;
    }

    activeRuntimeHealthWarning = nextWarning;
    runtimeHealthWarningStartMs = now;

    resetTouchGesture();
    drawRuntimeHealthWarning( nextWarning );

    Serial.printf(
      "[CoreS3_Display] Runtime health warning: %u\n",
      (unsigned)nextWarning
    );

    return true;
  }

  // =========================================================
  // Startup logo
  // =========================================================

  void drawStartupBase() {
    display.fillScreen( TFT_BLACK );

    bool logoResult = display.drawPng( CORES3_WLED_LOGO_PNG, CORES3_WLED_LOGO_PNG_LEN, 8, 20 );

    if (!logoResult) {
      display.setTextDatum( textdatum_t::middle_center );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "WLED M5Stack CoreS3", screenWidth / 2, 68 );

      Serial.println( F( "[CoreS3_Display] " "WARNING: startup PNG draw failed" ) );
    }
  }

  void drawStartupConnectingStatus() {
    display.fillRect( 0, 125, screenWidth, 100, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );
    display.setTextColor( TFT_WHITE, TFT_BLACK );
    display.setTextSize( 2 );

    // Reserve the width of all three dots from the beginning so the
    // "Wi-Fi Connecting" label never shifts while the dots animate.
    const char* label = "Wi-Fi Connecting";
    const int16_t labelWidth = display.textWidth( label );
    const int16_t dotWidth = display.textWidth( "." );
    const int16_t reservedDotsWidth = dotWidth * 3;

    const int16_t labelCenterX =
      ( screenWidth / 2 ) - ( reservedDotsWidth / 2 );

    display.drawString( label, labelCenterX, 153 );

    const int16_t labelRight =
      labelCenterX + ( labelWidth / 2 );

    for ( uint8_t i = 0; i < startupDotCount && i < 3; i++ ) {
      const int16_t dotCenterX =
        labelRight + ( dotWidth / 2 ) + ( i * dotWidth );

      display.drawString( ".", dotCenterX, 153 );
    }

    display.setTextSize( 1 );
    display.setTextColor( TFT_DARKGREY, TFT_BLACK );

    display.drawString( "Starting WLED...", screenWidth / 2, 185 );
  }


  void drawStartupConnectedStatus( const String& ipAddress ) {
    display.fillRect( 0, 125, screenWidth, 100, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_GREEN, TFT_BLACK );

    display.setTextSize( 2 );

    display.drawString( "Wi-Fi Connected", screenWidth / 2, 150 );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( ipAddress, screenWidth / 2, 181 );
  }

  void drawStartupAccessPointStatus( const String& ipAddress ) {
    display.fillRect( 0, 125, screenWidth, 100, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );
    display.setTextColor( TFT_GREEN, TFT_BLACK );
    display.setTextSize( 2 );

    display.drawString( "Wi-Fi AP Ready", screenWidth / 2, 150 );

    display.setTextColor( TFT_WHITE, TFT_BLACK );
    display.setTextSize( 1 );

    display.drawString( ipAddress, screenWidth / 2, 181 );
  }

  void drawStartupLocalControlStatus() {
    display.fillRect( 0, 125, screenWidth, 100, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );
    display.setTextColor( TFT_YELLOW, TFT_BLACK );
    display.setTextSize( 2 );

    display.drawString( "Local Control Ready", screenWidth / 2, 150 );

    display.setTextColor( TFT_DARKGREY, TFT_BLACK );
    display.setTextSize( 1 );

    display.drawString( "Wi-Fi unavailable", screenWidth / 2, 181 );
  }

  void handleStartupSequence() {
    if ( startupState == STARTUP_DONE ) {
      return;
    }

    unsigned long now = millis();

    if ( startupState == STARTUP_FADE_IN ) {
      if ( updateFade( getNormalDisplayBrightness(), now, startupLastFadeStep ) ) {
        startupState = STARTUP_WAIT_WIFI;

        startupStateStart = now;

        startupLastDotsUpdate = now;
      }

      return;
    }

    if ( startupState == STARTUP_WAIT_WIFI ) {
      if ( now - startupLastDotsUpdate >= STARTUP_DOTS_INTERVAL_MS ) {
        startupLastDotsUpdate = now;

        startupDotCount++;

        if ( startupDotCount > 3 ) {
          startupDotCount = 0;
        }

        drawStartupConnectingStatus();
      }

      NetworkAccessMode currentNetworkMode = getNetworkAccessMode();

      if ( currentNetworkMode != NETWORK_ACCESS_NONE ) {
        startupNetworkAccessMode = currentNetworkMode;
        startupIPAddress = getNetworkIPAddress( currentNetworkMode );

        if ( currentNetworkMode == NETWORK_ACCESS_STA ) {
          drawStartupConnectedStatus( startupIPAddress );
        }
        else {
          drawStartupAccessPointStatus( startupIPAddress );
        }

        startupState = STARTUP_READY_HOLD;
        startupStateStart = now;

        return;
      }

      if ( now - startupStateStart >= STARTUP_NETWORK_WAIT_TIMEOUT_MS ) {
        startupNetworkAccessMode = NETWORK_ACCESS_NONE;
        startupIPAddress = "";

        drawStartupLocalControlStatus();

        startupState = STARTUP_READY_HOLD;
        startupStateStart = now;
      }

      return;
    }

    if ( startupState == STARTUP_READY_HOLD ) {
      if ( now - startupStateStart >= STARTUP_READY_HOLD_MS ) {
        startupState = STARTUP_FADE_OUT;

        startupLastFadeStep = now;
      }

      return;
    }

    if ( startupState == STARTUP_FADE_OUT ) {
      if ( updateFade( 0, now, startupLastFadeStep ) ) {
        drawMainScreen( getNetworkDisplayText( startupNetworkAccessMode ) );

        setDisplayBrightness( 0 );

        startupState = STARTUP_MAIN_FADE_IN;

        startupLastFadeStep = now;
      }

      return;
    }

    if ( startupState == STARTUP_MAIN_FADE_IN ) {
      if ( updateFade( getNormalDisplayBrightness(), now, startupLastFadeStep ) ) {
        startupState = STARTUP_DONE;

        lastNetworkAccessMode = getNetworkAccessMode();
        lastNetworkDisplayText = getNetworkDisplayText( lastNetworkAccessMode );

        readyScreenShown = true;

        connectingScreenShown = false;

        lastUserActivityMs = now;

        displayPowerState = DISPLAY_POWER_ACTIVE;

        Serial.println( F( "[CoreS3_Display] " "Startup animation complete" ) );
      }

      return;
    }
  }

  bool pollWakeTouch( unsigned long now ) {
    if ( now - touchState.wakeTouchLastPoll < TOUCH_POLL_MS ) {
      return touchState.wakeTouchState;
    }

    touchState.wakeTouchLastPoll = now;

    int16_t x = -1;
    int16_t y = -1;

    touchState.wakeTouchState = ( readDisplayTouch( x, y ) );

    return touchState.wakeTouchState;
  }

  bool settlePendingPreset() {
    if ( pendingPresetId == 0 ) {
      return false;
    }

    bool completed = ( currentPreset == pendingPresetId );

    bool timedOut = ( millis() - pendingPresetRequestMs >= PRESET_APPLY_PENDING_MS );

    if ( !completed && !timedOut ) {
      return false;
    }

    pendingPresetId = 0;

    pendingPresetName = "";

    pendingPresetRequestMs = 0;

    return true;
  }
  uint8_t getDisplayedPresetId() {
    if ( pendingPresetId > 0 ) {
      if ( millis() - pendingPresetRequestMs < PRESET_APPLY_PENDING_MS ) {
        return pendingPresetId;
      }
    }

    if ( isPresetNavigationCursorValid() ) {
      return presetNavigationCursorId;
    }

    if (
      currentPreset > 0 &&
      presetCacheReady &&
      !presetCacheBuilding &&
      findPresetCacheIndex( currentPreset ) >= 0
    ) {
      return currentPreset;
    }

    return 0;
  }

  void redrawCurrentPageForWake() {
    settlePendingPreset();

    if ( currentPage == SCREEN_COLOR ) {
      drawColorScreen();

      return;
    }

    if ( currentPage == SCREEN_EFFECT ) {
      drawEffectDetailScreen();

      return;
    }

    if ( currentPage == SCREEN_PRESET ) {
      if ( presetSubPage == PRESET_SUBPAGE_MANAGE ) {
        drawPresetManageScreen();
      }
      else if ( presetSubPage == PRESET_SUBPAGE_SAVE ) {
        drawPresetSaveScreen();
      }
      else if ( presetSubPage == PRESET_SUBPAGE_OVERWRITE ) {
        drawPresetOverwriteScreen();
      }
      else if ( presetSubPage == PRESET_SUBPAGE_DELETE ) {
        drawPresetDeleteScreen();
      }
      else if ( presetSubPage == PRESET_SUBPAGE_BOOT ) {
        drawPresetBootScreen();
      }
      else {
        drawPresetScreen();
      }

      return;
    }

    drawMainScreen( getCurrentNetworkDisplayText() );
  }

  void beginDisplaySleep( unsigned long now ) {
    resetTouchGesture();

    displayPowerState = DISPLAY_POWER_SLEEP_FADE_OUT;

    displayFadeLastStep = now;

    touchState.wakeTouchLastPoll = 0;

    touchState.wakeTouchState = false;

    touchState.wakeReleaseCandidate = 0;
  }

  void beginDisplayWake( unsigned long now ) {
    resetTouchGesture();

    setDisplayBrightness( 0 );

    redrawCurrentPageForWake();

    setDisplayBrightness( 0 );

    displayPowerState = DISPLAY_POWER_WAKE_FADE_IN;

    displayFadeLastStep = now;

    touchState.wakeReleaseCandidate = 0;

    lastUserActivityMs = now;
  }

  bool handleDisplayPowerManagement() {
    unsigned long now = millis();

    if ( displayPowerState == DISPLAY_POWER_ACTIVE ) {
      if ( sleepTimeoutSec > 0 && !touchState.touchActive && now - lastUserActivityMs >= getSleepTimeoutMs() ) {
        beginDisplaySleep( now );

        return true;
      }

      return false;
    }

    if ( displayPowerState == DISPLAY_POWER_SLEEP_FADE_OUT ) {
      if ( pollWakeTouch( now ) ) {
        beginDisplayWake( now );

        return true;
      }

      if ( updateFade( 0, now, displayFadeLastStep ) ) {
        displayPowerState = DISPLAY_POWER_SLEEPING;

        setDisplayBrightness( 0 );

        Serial.println( F( "[CoreS3_Display] " "Display sleeping" ) );
      }

      return true;
    }

    if ( displayPowerState == DISPLAY_POWER_SLEEPING ) {
      if ( pollWakeTouch( now ) ) {
        beginDisplayWake( now );
      }

      return true;
    }

    if ( displayPowerState == DISPLAY_POWER_WAKE_FADE_IN ) {
      pollWakeTouch( now );

      if ( updateFade( getNormalDisplayBrightness(), now, displayFadeLastStep ) ) {
        displayPowerState = DISPLAY_POWER_WAKE_WAIT_RELEASE;

        touchState.wakeReleaseCandidate = 0;

      }

      return true;
    }

    if ( displayPowerState == DISPLAY_POWER_WAKE_WAIT_RELEASE ) {
      bool touching = pollWakeTouch( now );

      if (touching) {
        touchState.wakeReleaseCandidate = 0;

        return true;
      }

      if ( touchState.wakeReleaseCandidate == 0 ) {
        touchState.wakeReleaseCandidate = now;

        return true;
      }

      if ( now - touchState.wakeReleaseCandidate >= TOUCH_RELEASE_CONFIRM_MS ) {
        displayPowerState = DISPLAY_POWER_ACTIVE;

        lastUserActivityMs = now;

        touchState.wakeReleaseCandidate = 0;

        touchState.wakeTouchState = false;

        resetTouchGesture();

        Serial.println( F( "[CoreS3_Display] " "Display active" ) );
      }

      return true;
    }

    return false;
  }

  uint16_t rgbTo565( uint8_t r, uint8_t g, uint8_t b ) {
    return ( ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | ((uint16_t)b >> 3) );
  }

  struct M5StackEffectColorCapabilities {
    bool color1 = true;
    bool color2 = true;
    bool color3 = true;
  };

  uint32_t getPrimaryColor() {
    if ( strip.getSegmentsNum() > 0 ) {
      return strip.getMainSegment().colors[0];
    }

    return 0;
  }

  uint32_t getColorSlotValue( uint8_t colorSlot ) {
    if ( strip.getSegmentsNum() == 0 || colorSlot > 2 ) {
      return 0;
    }

    return strip.getMainSegment().colors[ colorSlot ];
  }

  uint32_t getSelectedColor() {
    return getColorSlotValue( selectedColorSlot );
  }

  bool isEffectColorSlotEnabled(
    const M5StackEffectColorCapabilities& capability,
    uint8_t colorSlot
  ) {
    if ( colorSlot == 0 ) {
      return capability.color1;
    }

    if ( colorSlot == 1 ) {
      return capability.color2;
    }

    if ( colorSlot == 2 ) {
      return capability.color3;
    }

    return false;
  }

  uint8_t getFirstEnabledColorSlot(
    const M5StackEffectColorCapabilities& capability
  ) {
    if ( capability.color1 ) {
      return 0;
    }

    if ( capability.color2 ) {
      return 1;
    }

    if ( capability.color3 ) {
      return 2;
    }

    return 0;
  }

  bool normalizeSelectedColorSlot( uint8_t effectMode ) {
    const M5StackEffectColorCapabilities capability =
      getEffectColorCapabilities( effectMode );

    if ( !effectUsesAnyColor( capability ) ) {
      selectedColorSlot = 0;
      return false;
    }

    if ( !isEffectColorSlotEnabled( capability, selectedColorSlot ) ) {
      selectedColorSlot = getFirstEnabledColorSlot( capability );
    }

    return true;
  }

  void cacheCurrentColorSlots() {
    if ( strip.getSegmentsNum() == 0 ) {
      lastColorSlotsValid = false;
      return;
    }

    Segment& mainSegment = strip.getMainSegment();

    for ( uint8_t colorSlot = 0; colorSlot < 3; colorSlot++ ) {
      const uint32_t slotColor = mainSegment.colors[ colorSlot ];

      lastColorSlots[ colorSlot ] = slotColor;

      if ( slotColor != 0 ) {
        lastNonBlackColorSlots[ colorSlot ] = slotColor;
        lastNonBlackColorSlotValid[ colorSlot ] = true;
      }
    }

    lastColorSlotsValid = true;
  }

  bool currentColorSlotsChanged() {
    if ( strip.getSegmentsNum() == 0 ) {
      return lastColorSlotsValid;
    }

    if ( !lastColorSlotsValid ) {
      return true;
    }

    Segment& mainSegment = strip.getMainSegment();

    for ( uint8_t colorSlot = 0; colorSlot < 3; colorSlot++ ) {
      if ( mainSegment.colors[ colorSlot ] != lastColorSlots[ colorSlot ] ) {
        return true;
      }
    }

    return false;
  }

  void selectColorSlot( uint8_t colorSlot ) {
    const uint8_t effectMode = getCurrentEffectMode();

    const M5StackEffectColorCapabilities capability =
      getEffectColorCapabilities( effectMode );

    if ( !isEffectColorSlotEnabled( capability, colorSlot ) ) {
      return;
    }

    selectedColorSlot = colorSlot;

    hueEditValid = false;
    saturationEditValid = false;

    const uint32_t selectedColor = getSelectedColor();

    syncLogicalColorFromRgb( selectedColor );

    lastSelectedColor = selectedColor;
    lastSelectedColorValid = true;

    cacheCurrentColorSlots();

    if ( currentPage == SCREEN_COLOR ) {
      drawColorDetails( selectedColor );

      drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

      drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );
    }
  }

  bool toggleColorSlotBlack( uint8_t colorSlot ) {
    if ( strip.getSegmentsNum() == 0 || colorSlot > 2 ) {
      return false;
    }

    const M5StackEffectColorCapabilities capability =
      getEffectColorCapabilities( getCurrentEffectMode() );

    if ( !isEffectColorSlotEnabled( capability, colorSlot ) ) {
      return false;
    }

    Segment& mainSegment = strip.getMainSegment();

    const uint32_t currentColor = mainSegment.colors[ colorSlot ];

    uint32_t newColor = 0;

    if ( currentColor != 0 ) {
      lastNonBlackColorSlots[ colorSlot ] = currentColor;
      lastNonBlackColorSlotValid[ colorSlot ] = true;

      newColor = 0;
    }
    else {
      // If this slot has never had a non-black color during this runtime,
      // restore to RGB white. The user can then immediately tune Hue/Sat.
      newColor =
        lastNonBlackColorSlotValid[ colorSlot ]
          ? lastNonBlackColorSlots[ colorSlot ]
          : 0x00FFFFFF;
    }

    selectedColorSlot = colorSlot;

    hueEditValid = false;
    saturationEditValid = false;

    if ( newColor != currentColor ) {
      mainSegment.setColor( colorSlot, newColor );

      stateUpdated( CALL_MODE_BUTTON );
    }

    syncLogicalColorFromRgb( newColor );

    lastSelectedColor = newColor;
    lastSelectedColorValid = true;

    if ( colorSlot == 0 ) {
      lastPrimaryColor = newColor;
      lastPrimaryColorValid = true;
    }

    cacheCurrentColorSlots();

    lastHueValue = logicalHueValue;
    lastSaturationValue = logicalSaturationValue;

    if ( currentPage == SCREEN_COLOR ) {
      drawColorDetails( newColor );

      drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

      drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );
    }

    return true;
  }

  uint8_t getCurrentEffectMode() {
    if ( strip.getSegmentsNum() > 0 ) {
      return strip.getMainSegment().mode;
    }

    return 0;
  }

  uint8_t getCurrentSpeed() {
    if ( strip.getSegmentsNum() > 0 ) {
      return strip.getMainSegment().speed;
    }

    return 0;
  }

  uint8_t getCurrentIntensity() {
    if ( strip.getSegmentsNum() > 0 ) {
      return strip.getMainSegment().intensity;
    }

    return 0;
  }

  uint8_t getCurrentPalette() {
    if ( strip.getSegmentsNum() > 0 ) {
      return strip.getMainSegment().palette;
    }

    return 0;
  }

  size_t getSelectablePaletteCount() {
    return FIXED_PALETTE_COUNT + customPalettes.size() + usermodPalettes.size();
  }

  uint8_t paletteIdFromSequenceIndex( size_t sequenceIndex ) {
    if ( sequenceIndex < FIXED_PALETTE_COUNT ) {
      return (uint8_t)sequenceIndex;
    }

    sequenceIndex -= FIXED_PALETTE_COUNT;

    if ( sequenceIndex < customPalettes.size() ) {
      return (uint8_t)( WLED_CUSTOM_PALETTE_ID_BASE - sequenceIndex );
    }

    sequenceIndex -= customPalettes.size();

    if ( sequenceIndex < usermodPalettes.size() ) {
      return (uint8_t)( WLED_USERMOD_PALETTE_ID_BASE - sequenceIndex );
    }

    return 0;
  }

  int findPaletteSequenceIndex( uint8_t paletteId ) {
    if ( paletteId < FIXED_PALETTE_COUNT ) {
      return paletteId;
    }

    if ( paletteId > WLED_CUSTOM_PALETTE_ID_BASE ) {
      size_t usermodIndex = WLED_USERMOD_PALETTE_ID_BASE - paletteId;

      if ( usermodIndex < usermodPalettes.size() ) {
        return (int)( FIXED_PALETTE_COUNT + customPalettes.size() + usermodIndex );
      }

      return -1;
    }

    if ( paletteId >= FIXED_PALETTE_COUNT && paletteId <= WLED_CUSTOM_PALETTE_ID_BASE ) {
      size_t customIndex = WLED_CUSTOM_PALETTE_ID_BASE - paletteId;

      if ( customIndex < customPalettes.size() ) {
        return (int)( FIXED_PALETTE_COUNT + customIndex );
      }

      return -1;
    }

    return -1;
  }

  void getPaletteName( uint8_t paletteId, char* paletteName, size_t paletteNameSize ) {
    if ( paletteName == nullptr || paletteNameSize == 0 ) {
      return;
    }

    paletteName[0] = '\0';

    extractModeName( paletteId, JSON_palette_names, paletteName, paletteNameSize - 1 );

    if ( strlen(paletteName) == 0 ) {
      snprintf( paletteName, paletteNameSize, "Palette %u", paletteId );
    }
  }

  uint8_t findAdjacentPreset( uint8_t startPreset, int direction, String* foundName = nullptr ) {
    if ( direction == 0 ) {
      return 0;
    }

    if ( !presetCacheReady || presetCacheCount == 0 ) {
      return 0;
    }

    int currentIndex = findPresetCacheIndex( startPreset );

    int newIndex;

    if ( currentIndex < 0 ) {
      if ( direction > 0 ) {
        newIndex = 0;
      }
      else {
        newIndex = presetCacheCount - 1;
      }
    }
    else {
      newIndex = currentIndex + ( direction > 0 ? 1 : -1 );

      if ( newIndex >= presetCacheCount ) {
        newIndex = 0;
      }

      if ( newIndex < 0 ) {
        newIndex = presetCacheCount - 1;
      }
    }

    if ( foundName != nullptr ) {
      *foundName = presetCache[ newIndex ].name;
    }

    return presetCache[ newIndex ].id;
  }

  bool isPresetNavigationCursorValid() {
    return (
      presetNavigationCursorId > 0 &&
      presetCacheReady &&
      !presetCacheBuilding &&
      findPresetCacheIndex( presetNavigationCursorId ) >= 0
    );
  }

  void normalizePresetNavigationCursor() {
    if ( !presetCacheReady || presetCacheBuilding ) {
      return;
    }

    if ( presetCacheCount == 0 ) {
      presetNavigationCursorId = 0;
      return;
    }

    if ( isPresetNavigationCursorValid() ) {
      return;
    }

    if ( presetNavigationCursorId == 0 ) {
      if ( currentPreset > 0 && findPresetCacheIndex( currentPreset ) >= 0 ) {
        presetNavigationCursorId = currentPreset;
      }

      return;
    }

    const uint8_t previousCursor = presetNavigationCursorId;

    // If the cursor Preset was deleted, prefer the next higher saved ID.
    // If there is no higher ID, fall back to the final saved Preset.
    for ( uint16_t index = 0; index < presetCacheCount; index++ ) {
      if ( presetCache[ index ].id > previousCursor ) {
        presetNavigationCursorId = presetCache[ index ].id;
        return;
      }
    }

    presetNavigationCursorId = presetCache[ presetCacheCount - 1 ].id;
  }

  void syncPresetNavigationCursorFromCurrentPreset() {
    if ( currentPreset == lastObservedCurrentPreset ) {
      return;
    }

    if ( currentPreset == 0 ) {
      // WLED entered Custom State. Keep the CoreS3 navigation cursor where
      // the user last selected a Preset.
      lastObservedCurrentPreset = 0;
      return;
    }

    if ( !presetCacheReady || presetCacheBuilding ) {
      // Retry once the cache is available. Do not mark this value observed yet.
      return;
    }

    if ( findPresetCacheIndex( currentPreset ) < 0 ) {
      // Retry if the Preset cache is in transition.
      return;
    }

    presetNavigationCursorId = currentPreset;
    lastObservedCurrentPreset = currentPreset;
  }

  uint8_t getPresetManagementBaseId() {
    if ( isPresetNavigationCursorValid() ) {
      return presetNavigationCursorId;
    }

    if (
      currentPreset > 0 &&
      presetCacheReady &&
      !presetCacheBuilding &&
      findPresetCacheIndex( currentPreset ) >= 0
    ) {
      return currentPreset;
    }

    if ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 ) {
      return presetCache[ 0 ].id;
    }

    return 0;
  }
  uint8_t getPresetNavigationBaseId() {
    if ( pendingPresetId > 0 && millis() - pendingPresetRequestMs < PRESET_APPLY_PENDING_MS ) {
      return pendingPresetId;
    }

    if ( isPresetNavigationCursorValid() ) {
      return presetNavigationCursorId;
    }

    if (
      currentPreset > 0 &&
      presetCacheReady &&
      !presetCacheBuilding &&
      findPresetCacheIndex( currentPreset ) >= 0
    ) {
      return currentPreset;
    }

    return 0;
  }

  uint8_t getHueFromColor( uint32_t color ) {
    CRGBW rgb( color );

    CHSV32 hsv;

    rgb2hsv( rgb, hsv );

    return (uint8_t)( hsv.h >> 8 );
  }

  uint8_t getSaturationFromColor( uint32_t color ) {
    CRGBW rgb( color );

    CHSV32 hsv;

    rgb2hsv( rgb, hsv );

    return hsv.s;
  }

  void syncLogicalColorFromRgb( uint32_t color ) {
    CRGBW rgb( color );

    rgb2hsv( rgb, logicalColorHsv );

    logicalHueValue = (uint8_t)( logicalColorHsv.h >> 8 );

    logicalSaturationValue = logicalColorHsv.s;

    logicalWhiteValue = rgb.w;

    logicalColorHsvValid = true;

    lastHueValue = logicalHueValue;

    lastSaturationValue = logicalSaturationValue;
  }

  uint8_t getDisplayedHue() {
    uint32_t currentColor = getSelectedColor();

    if ( logicalColorHsvValid && lastSelectedColorValid && currentColor == lastSelectedColor ) {
      return logicalHueValue;
    }

    return getHueFromColor( currentColor );
  }

  uint8_t getDisplayedSaturation() {
    uint32_t currentColor = getSelectedColor();

    if ( logicalColorHsvValid && lastSelectedColorValid && currentColor == lastSelectedColor ) {
      return logicalSaturationValue;
    }

    return getSaturationFromColor( currentColor );
  }

  // =========================================================
  // Common centered text / standard page header helpers
  // =========================================================

  void setCenteredTextStyle( uint16_t color, uint8_t size, uint16_t background = TFT_BLACK ) {
    display.setTextDatum( textdatum_t::middle_center );
    display.setTextColor( color, background );
    display.setTextSize( size );
  }

  void drawStandardPageHeader( const char* title, const char* subtitle, uint16_t titleColor = TFT_WHITE ) {
    setCenteredTextStyle( titleColor, 2 );
    display.drawString( title, screenWidth / 2, 18 );

    setCenteredTextStyle( TFT_WHITE, 1 );
    display.drawString( subtitle, screenWidth / 2, 41 );

    display.drawFastHLine( 8, 58, screenWidth - 16, TFT_DARKGREY );
  }

  void drawPowerIcon( int16_t centerX, int16_t centerY, uint16_t iconColor, uint16_t backgroundColor ) {
    display.drawCircle( centerX, centerY + 2, 11, iconColor );

    display.drawCircle( centerX, centerY + 2, 10, iconColor );

    display.fillRect( centerX - 4, centerY - 11, 9, 8, backgroundColor );

    display.drawFastVLine( centerX - 1, centerY - 14, 12, iconColor );

    display.drawFastVLine( centerX, centerY - 14, 12, iconColor );

    display.drawFastVLine( centerX + 1, centerY - 14, 12, iconColor );
  }

  void drawPowerButton( bool ledOn, bool pressed ) {
    uint16_t stateColor = ledOn ? TFT_GREEN : TFT_RED;

    uint16_t backgroundColor = pressed ? stateColor : TFT_BLACK;

    uint16_t iconColor = pressed ? TFT_BLACK : stateColor;

    display.fillRect( POWER_BUTTON_X - 2, POWER_BUTTON_Y - 2, POWER_BUTTON_W + 4, POWER_BUTTON_H + 4, TFT_BLACK );

    display.fillRect( POWER_BUTTON_X, POWER_BUTTON_Y, POWER_BUTTON_W, POWER_BUTTON_H, backgroundColor );

    display.drawRect( POWER_BUTTON_X, POWER_BUTTON_Y, POWER_BUTTON_W, POWER_BUTTON_H, stateColor );

    display.drawRect( POWER_BUTTON_X + 1, POWER_BUTTON_Y + 1, POWER_BUTTON_W - 2, POWER_BUTTON_H - 2, stateColor );

    int16_t centerX = POWER_BUTTON_X + (POWER_BUTTON_W / 2);

    int16_t centerY = POWER_BUTTON_Y + (POWER_BUTTON_H / 2);

    drawPowerIcon( centerX, centerY, iconColor, backgroundColor );

    touchState.powerButtonVisualPressed = pressed;
  }

  void drawTriangleButton( int16_t x, int16_t y, bool pointRight, bool pressed ) {
    const uint16_t buttonColor = TFT_CYAN;

    const int16_t w = CONTROL_BUTTON_W;

    const int16_t h = CONTROL_BUTTON_H;

    display.fillRect( x - 2, y - 2, w + 4, h + 4, TFT_BLACK );

    if (pressed) {
      display.fillRect( x, y, w, h, buttonColor );
    }
    else {
      display.fillRect( x, y, w, h, TFT_BLACK );

      display.drawRect( x, y, w, h, buttonColor );

      display.drawRect( x + 1, y + 1, w - 2, h - 2, buttonColor );
    }

    int16_t centerX = x + (w / 2);

    int16_t centerY = y + (h / 2);

    uint16_t triangleColor = pressed ? TFT_BLACK : buttonColor;

    if (pointRight) {
      display.fillTriangle( centerX + 10, centerY, centerX - 7, centerY - 9, centerX - 7, centerY + 9, triangleColor );
    }
    else {
      display.fillTriangle( centerX - 10, centerY, centerX + 7, centerY - 9, centerX + 7, centerY + 9, triangleColor );
    }
  }

  // =========================================================
  // Shared numeric control drawing
  // =========================================================

  void drawNumericControl( int16_t clearY, int16_t clearH, const char* label, int16_t labelY, int16_t buttonY, int16_t valueY, const char* valueText, M5StackTouchTarget pressedTarget, M5StackTouchTarget downTarget, M5StackTouchTarget upTarget ) {
    display.fillRect( 0, clearY, screenWidth, clearH, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( label, screenWidth / 2, labelY );

    drawTriangleButton( CONTROL_LEFT_X, buttonY, false, pressedTarget == downTarget );

    drawTriangleButton( CONTROL_RIGHT_X, buttonY, true, pressedTarget == upTarget );

    display.setTextSize( 2 );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.drawString( valueText, screenWidth / 2, valueY );
  }

  void drawBrightness( int brightnessValue, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    brightnessValue = constrain( brightnessValue, 0, 255 );

    int brightnessPercent = 0;

    if ( brightnessValue >= 255 ) {
      brightnessPercent = 100;
    }
    else if ( brightnessValue > 0 ) {
      brightnessPercent = max( 1, ( brightnessValue * 100 ) / 255 );
    }

    char valueText[16];

    snprintf(
      valueText,
      sizeof(valueText),
      "%d  %d%%",
      brightnessValue,
      brightnessPercent
    );

    drawNumericControl(
      62,
      58,
      "LED Brightness",
      70,
      BRI_BUTTON_Y,
      99,
      valueText,
      pressedTarget,
      M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN,
      M5STACK_TOUCH_TARGET_BRIGHTNESS_UP
    );
  }

  struct M5StackEffectCapabilities {
    bool supports1D = true;
    bool supports2D = false;
    bool supports3D = false;
    bool audioVolume = false;
    bool audioFrequency = false;
  };

  M5StackEffectCapabilities getEffectCapabilities( uint8_t effectMode ) {
    M5StackEffectCapabilities capability;

    const char* modeData = strip.getModeData( effectMode );

    if ( modeData == nullptr ) {
      return capability;
    }

    const char* metadataStart = strchr( modeData, '@' );

    if ( metadataStart == nullptr ) {
      // WLED metadata specification defaults missing flags to 1D.
      return capability;
    }

    const char* flagsStart = metadataStart + 1;

    // Metadata sections:
    // parameters ; colors ; palette ; flags ; defaults
    for ( uint8_t section = 0; section < 3; section++ ) {
      flagsStart = strchr( flagsStart, ';' );

      if ( flagsStart == nullptr ) {
        return capability;
      }

      flagsStart++;
    }

    const char* flagsEnd = strchr( flagsStart, ';' );

    if ( flagsEnd == nullptr ) {
      flagsEnd = flagsStart + strlen( flagsStart );
    }

    if ( flagsStart == flagsEnd ) {
      return capability;
    }

    bool dimensionFlagFound = false;

    capability.supports1D = false;

    for ( const char* flag = flagsStart; flag < flagsEnd; flag++ ) {
      switch ( *flag ) {
        case '0':
          // Flag 0 means the effect also works well on a single LED.
          // Treat it as 1D-capable for the compact CoreS3 display.
          capability.supports1D = true;
          dimensionFlagFound = true;
          break;

        case '1':
          capability.supports1D = true;
          dimensionFlagFound = true;
          break;

        case '2':
          capability.supports2D = true;
          dimensionFlagFound = true;
          break;

        case '3':
          capability.supports3D = true;
          dimensionFlagFound = true;
          break;

        case 'v':
          capability.audioVolume = true;
          break;

        case 'f':
          capability.audioFrequency = true;
          break;

        default:
          break;
      }
    }

    if ( !dimensionFlagFound ) {
      // WLED metadata specification: missing dimension flags fall back to 1D.
      capability.supports1D = true;
    }

    return capability;
  }

  bool effectUsesAudioReactive(
    const M5StackEffectCapabilities& capability
  ) const {
    return capability.audioVolume || capability.audioFrequency;
  }

  bool isCoreS3AudioUnavailable() const {
#if defined(WLED_M5STACK_CORES3_AUDIO)
    return
      coreS3AudioInitializationFinished() &&
      !coreS3AudioCodecReady();
#else
    return false;
#endif
  }

  bool isAudioUnavailableForEffect( uint8_t effectMode ) {
    const M5StackEffectCapabilities capability =
      getEffectCapabilities( effectMode );

    return
      effectUsesAudioReactive( capability ) &&
      isCoreS3AudioUnavailable();
  }

  bool effectRequires2D( const M5StackEffectCapabilities& capability ) {
    return capability.supports2D && !capability.supports1D;
  }

  bool currentMainSegmentIs2D() {
    if ( strip.getSegmentsNum() == 0 ) {
      return false;
    }

    return strip.getMainSegment().is2D();
  }

  void getEffectDimensionText(
    const M5StackEffectCapabilities& capability,
    char* text,
    size_t textSize
  ) {
    if ( text == nullptr || textSize == 0 ) {
      return;
    }

    text[0] = '\0';

    if ( capability.supports1D && capability.supports2D && capability.supports3D ) {
      strncpy( text, "1D/2D/3D", textSize - 1 );
    }
    else if ( capability.supports1D && capability.supports2D ) {
      strncpy( text, "1D/2D", textSize - 1 );
    }
    else if ( capability.supports1D && capability.supports3D ) {
      strncpy( text, "1D/3D", textSize - 1 );
    }
    else if ( capability.supports2D && capability.supports3D ) {
      strncpy( text, "2D/3D", textSize - 1 );
    }
    else if ( capability.supports2D ) {
      strncpy( text, "2D", textSize - 1 );
    }
    else if ( capability.supports3D ) {
      strncpy( text, "3D", textSize - 1 );
    }
    else {
      strncpy( text, "1D", textSize - 1 );
    }

    text[ textSize - 1 ] = '\0';
  }

  void getEffectAudioText(
    const M5StackEffectCapabilities& capability,
    char* text,
    size_t textSize
  ) {
    if ( text == nullptr || textSize == 0 ) {
      return;
    }

    text[0] = '\0';

    if ( capability.audioVolume && capability.audioFrequency ) {
      strncpy( text, "AUDIO", textSize - 1 );
    }
    else if ( capability.audioFrequency ) {
      strncpy( text, "FFT", textSize - 1 );
    }
    else if ( capability.audioVolume ) {
      strncpy( text, "VOL", textSize - 1 );
    }

    text[ textSize - 1 ] = '\0';
  }

  void getEffectCapabilityText(
    uint8_t effectMode,
    char* text,
    size_t textSize,
    bool& incompatibleWithCurrentSegment
  ) {
    if ( text == nullptr || textSize == 0 ) {
      incompatibleWithCurrentSegment = false;
      return;
    }

    const M5StackEffectCapabilities capability = getEffectCapabilities( effectMode );

    incompatibleWithCurrentSegment =
      effectRequires2D( capability ) && !currentMainSegmentIs2D();

    const bool audioUnavailable =
      effectUsesAudioReactive( capability ) &&
      isCoreS3AudioUnavailable();

    char dimensionText[16];
    char audioText[8];

    getEffectDimensionText( capability, dimensionText, sizeof(dimensionText) );
    getEffectAudioText( capability, audioText, sizeof(audioText) );

    if ( incompatibleWithCurrentSegment ) {
      if ( audioUnavailable ) {
        snprintf( text, textSize, "2D REQ | AUDIO UNAVAILABLE" );
      }
      else if ( audioText[0] != '\0' ) {
        snprintf( text, textSize, "2D REQUIRED | %s", audioText );
      }
      else {
        snprintf( text, textSize, "2D REQUIRED" );
      }

      return;
    }

    if ( audioUnavailable ) {
      snprintf( text, textSize, "AUDIO UNAVAILABLE" );
      return;
    }

    if ( audioText[0] != '\0' ) {
      snprintf( text, textSize, "%s | %s", dimensionText, audioText );
    }
    else {
      snprintf( text, textSize, "%s", dimensionText );
    }
  }

  bool getEffectMetadataSection(
    uint8_t effectMode,
    uint8_t sectionIndex,
    const char*& sectionStart,
    const char*& sectionEnd
  ) {
    sectionStart = nullptr;
    sectionEnd = nullptr;

    const char* modeData = strip.getModeData( effectMode );

    if ( modeData == nullptr ) {
      return false;
    }

    const char* metadataStart = strchr( modeData, '@' );

    if ( metadataStart == nullptr ) {
      return false;
    }

    sectionStart = metadataStart + 1;

    for ( uint8_t section = 0; section < sectionIndex; section++ ) {
      const char* separator = strchr( sectionStart, ';' );

      if ( separator == nullptr ) {
        sectionStart = nullptr;
        return false;
      }

      sectionStart = separator + 1;
    }

    sectionEnd = strchr( sectionStart, ';' );

    if ( sectionEnd == nullptr ) {
      sectionEnd = sectionStart + strlen( sectionStart );
    }

    return true;
  }

  void copyEffectMetadataLabel(
    const char* fieldStart,
    const char* fieldEnd,
    const char* defaultLabel,
    char* label,
    size_t labelSize
  ) {
    if ( label == nullptr || labelSize == 0 ) {
      return;
    }

    label[0] = '\0';

    if ( fieldStart == nullptr || fieldEnd == nullptr || fieldStart >= fieldEnd ) {
      return;
    }

    if ( fieldEnd - fieldStart == 1 && *fieldStart == '!' ) {
      strncpy( label, defaultLabel, labelSize - 1 );
      label[ labelSize - 1 ] = '\0';
      return;
    }

    size_t copyLength = (size_t)( fieldEnd - fieldStart );

    if ( copyLength >= labelSize ) {
      copyLength = labelSize - 1;
    }

    memcpy( label, fieldStart, copyLength );
    label[ copyLength ] = '\0';
  }

  bool getEffectSliderMetadata(
    uint8_t effectMode,
    uint8_t sliderIndex,
    const char* defaultLabel,
    char* label,
    size_t labelSize
  ) {
    if ( label == nullptr || labelSize == 0 ) {
      return false;
    }

    label[0] = '\0';

    const char* sectionStart = nullptr;
    const char* sectionEnd = nullptr;

    if ( !getEffectMetadataSection( effectMode, 0, sectionStart, sectionEnd ) ) {
      // WLED metadata fallback: missing parameter section means the
      // standard Speed + Intensity sliders are available.
      if ( sliderIndex <= 1 ) {
        strncpy( label, defaultLabel, labelSize - 1 );
        label[ labelSize - 1 ] = '\0';
        return true;
      }

      return false;
    }

    const char* fieldStart = sectionStart;

    for ( uint8_t fieldIndex = 0; fieldIndex < sliderIndex; fieldIndex++ ) {
      const char* comma = nullptr;

      for ( const char* cursor = fieldStart; cursor < sectionEnd; cursor++ ) {
        if ( *cursor == ',' ) {
          comma = cursor;
          break;
        }
      }

      if ( comma == nullptr ) {
        // Explicit metadata section exists but this field is missing.
        // WLED treats a missing/empty label as a disabled control.
        return false;
      }

      fieldStart = comma + 1;
    }

    const char* fieldEnd = sectionEnd;

    for ( const char* cursor = fieldStart; cursor < sectionEnd; cursor++ ) {
      if ( *cursor == ',' ) {
        fieldEnd = cursor;
        break;
      }
    }

    if ( fieldStart >= fieldEnd ) {
      return false;
    }

    copyEffectMetadataLabel(
      fieldStart,
      fieldEnd,
      defaultLabel,
      label,
      labelSize
    );

    // Keep labels compact for the 320 px CoreS3 display.
    if ( strlen(label) > 24 ) {
      label[24] = '\0';
    }

    return label[0] != '\0';
  }

  bool getEffectSpeedControlMetadata(
    uint8_t effectMode,
    char* label,
    size_t labelSize
  ) {
    return getEffectSliderMetadata(
      effectMode,
      0,
      "Speed",
      label,
      labelSize
    );
  }

  bool getEffectIntensityControlMetadata(
    uint8_t effectMode,
    char* label,
    size_t labelSize
  ) {
    return getEffectSliderMetadata(
      effectMode,
      1,
      "Intensity",
      label,
      labelSize
    );
  }

  bool isEffectSpeedControlVisible( uint8_t effectMode ) {
    char label[32];

    return getEffectSpeedControlMetadata(
      effectMode,
      label,
      sizeof(label)
    );
  }

  bool isEffectIntensityControlVisible( uint8_t effectMode ) {
    char label[32];

    return getEffectIntensityControlMetadata(
      effectMode,
      label,
      sizeof(label)
    );
  }

  bool getEffectColorSlotCustomLabel(
    uint8_t effectMode,
    uint8_t colorSlot,
    char* label,
    size_t labelSize
  ) {
    if ( label == nullptr || labelSize == 0 || colorSlot > 2 ) {
      return false;
    }

    label[0] = '\0';

    const char* sectionStart = nullptr;
    const char* sectionEnd = nullptr;

    if ( !getEffectMetadataSection( effectMode, 1, sectionStart, sectionEnd ) ) {
      // Missing Colors metadata uses WLED's default Fx/Bg/Cs labels.
      // Keep CoreS3's stable C1/C2/C3 naming without an extra comment.
      return false;
    }

    const char* fieldStart = sectionStart;

    for ( uint8_t fieldIndex = 0; fieldIndex < colorSlot; fieldIndex++ ) {
      const char* comma = nullptr;

      for ( const char* cursor = fieldStart; cursor < sectionEnd; cursor++ ) {
        if ( *cursor == ',' ) {
          comma = cursor;
          break;
        }
      }

      if ( comma == nullptr ) {
        return false;
      }

      fieldStart = comma + 1;
    }

    const char* fieldEnd = sectionEnd;

    for ( const char* cursor = fieldStart; cursor < sectionEnd; cursor++ ) {
      if ( *cursor == ',' ) {
        fieldEnd = cursor;
        break;
      }
    }

    if ( fieldStart >= fieldEnd ) {
      return false;
    }

    // "!" means WLED's default Fx/Bg/Cs label. It is intentionally not
    // repeated because CoreS3 keeps C1/C2/C3 as the primary slot names.
    if ( fieldEnd - fieldStart == 1 && *fieldStart == '!' ) {
      return false;
    }

    size_t copyLength = (size_t)( fieldEnd - fieldStart );

    // Keep the supplemental line compact on the 320 px display.
    if ( copyLength > 24 ) {
      copyLength = 24;
    }

    if ( copyLength >= labelSize ) {
      copyLength = labelSize - 1;
    }

    memcpy( label, fieldStart, copyLength );
    label[ copyLength ] = '\0';

    return label[0] != '\0';
  }

  M5StackEffectColorCapabilities getEffectColorCapabilities( uint8_t effectMode ) {
    M5StackEffectColorCapabilities capability;

    const char* sectionStart = nullptr;
    const char* sectionEnd = nullptr;

    if ( !getEffectMetadataSection( effectMode, 1, sectionStart, sectionEnd ) ) {
      // WLED metadata fallback: missing Colors section means all three
      // color slots (Fx/Bg/Cs) are available.
      return capability;
    }

    capability.color1 = false;
    capability.color2 = false;
    capability.color3 = false;

    if ( sectionStart >= sectionEnd ) {
      // Explicit empty Colors section means the Effect uses no color slots.
      return capability;
    }

    const char* fieldStart = sectionStart;

    for ( uint8_t colorIndex = 0; colorIndex < 3; colorIndex++ ) {
      const char* fieldEnd = sectionEnd;

      for ( const char* cursor = fieldStart; cursor < sectionEnd; cursor++ ) {
        if ( *cursor == ',' ) {
          fieldEnd = cursor;
          break;
        }
      }

      const bool enabled = fieldStart < fieldEnd;

      if ( colorIndex == 0 ) {
        capability.color1 = enabled;
      }
      else if ( colorIndex == 1 ) {
        capability.color2 = enabled;
      }
      else {
        capability.color3 = enabled;
      }

      if ( fieldEnd >= sectionEnd ) {
        break;
      }

      fieldStart = fieldEnd + 1;
    }

    return capability;
  }

  bool effectUsesPrimaryColor( uint8_t effectMode ) {
    return getEffectColorCapabilities( effectMode ).color1;
  }

  bool effectUsesAnyColor( const M5StackEffectColorCapabilities& capability ) {
    return capability.color1 || capability.color2 || capability.color3;
  }

  void getEffectColorCapabilityText(
    uint8_t effectMode,
    char* text,
    size_t textSize
  ) {
    if ( text == nullptr || textSize == 0 ) {
      return;
    }

    const M5StackEffectColorCapabilities capability =
      getEffectColorCapabilities( effectMode );

    const char* capabilityText = "NONE";

    if ( capability.color1 && capability.color2 && capability.color3 ) {
      capabilityText = "C1/C2/C3";
    }
    else if ( capability.color1 && capability.color2 ) {
      capabilityText = "C1/C2";
    }
    else if ( capability.color1 && capability.color3 ) {
      capabilityText = "C1/C3";
    }
    else if ( capability.color2 && capability.color3 ) {
      capabilityText = "C2/C3";
    }
    else if ( capability.color1 ) {
      capabilityText = "C1";
    }
    else if ( capability.color2 ) {
      capabilityText = "C2";
    }
    else if ( capability.color3 ) {
      capabilityText = "C3";
    }

    strncpy( text, capabilityText, textSize - 1 );
    text[ textSize - 1 ] = '\0';
  }

  bool isEffectPaletteControlVisible( uint8_t effectMode ) {
    const char* sectionStart = nullptr;
    const char* sectionEnd = nullptr;

    if ( !getEffectMetadataSection( effectMode, 2, sectionStart, sectionEnd ) ) {
      // WLED metadata fallback: missing Palette section means enabled.
      return true;
    }

    // Explicit empty Palette section means this Effect does not use palettes.
    return sectionStart < sectionEnd;
  }

  void getEffectName( uint8_t effectMode, char* effectName, size_t effectNameSize ) {
    if ( effectName == nullptr || effectNameSize == 0 ) {
      return;
    }

    effectName[0] = '\0';

    extractModeName( effectMode, nullptr, effectName, effectNameSize - 1 );

    if ( strlen(effectName) == 0 ) {
      strncpy( effectName, "Unknown", effectNameSize - 1 );

      effectName[ effectNameSize - 1 ] = '\0';
    }

    if ( strlen(effectName) > 22 ) {
      effectName[22] = '\0';
    }
  }

  void drawEffectDetailButton( uint8_t effectMode, bool pressed ) {
    const uint16_t buttonColor = TFT_CYAN;

    uint16_t backgroundColor = pressed ? buttonColor : TFT_BLACK;

    uint16_t textColor = pressed ? TFT_BLACK : TFT_WHITE;

    display.fillRect( EFFECT_DETAIL_X - 2, EFFECT_DETAIL_Y - 2, EFFECT_DETAIL_W + 4, EFFECT_DETAIL_H + 4, TFT_BLACK );

    display.fillRect( EFFECT_DETAIL_X, EFFECT_DETAIL_Y, EFFECT_DETAIL_W, EFFECT_DETAIL_H, backgroundColor );

    display.drawRect( EFFECT_DETAIL_X, EFFECT_DETAIL_Y, EFFECT_DETAIL_W, EFFECT_DETAIL_H, buttonColor );

    display.drawRect( EFFECT_DETAIL_X + 1, EFFECT_DETAIL_Y + 1, EFFECT_DETAIL_W - 2, EFFECT_DETAIL_H - 2, buttonColor );

    char effectName[64];

    getEffectName( effectMode, effectName, sizeof(effectName) );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( textColor, backgroundColor );

    if ( strlen(effectName) <= 10 ) {
      display.setTextSize( 2 );
    }
    else {
      display.setTextSize( 1 );
    }

    display.drawString( effectName, EFFECT_DETAIL_X + (EFFECT_DETAIL_W / 2), EFFECT_DETAIL_Y + (EFFECT_DETAIL_H / 2) );

    touchState.effectDetailVisualPressed = pressed;
  }

  void drawEffect( uint8_t effectMode, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    display.fillRect( 0, 120, screenWidth, 58, TFT_BLACK );

    char capabilityText[40];
    char effectLabel[56];
    bool incompatibleWithCurrentSegment = false;

    getEffectCapabilityText(
      effectMode,
      capabilityText,
      sizeof(capabilityText),
      incompatibleWithCurrentSegment
    );

    snprintf( effectLabel, sizeof(effectLabel), "Effect  %s", capabilityText );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor(
      ( incompatibleWithCurrentSegment || isAudioUnavailableForEffect( effectMode ) )
        ? TFT_YELLOW
        : TFT_WHITE,
      TFT_BLACK
    );

    display.setTextSize( 1 );

    display.drawString( effectLabel, screenWidth / 2, 128 );

    drawTriangleButton( CONTROL_LEFT_X, FX_BUTTON_Y, false, pressedTarget == M5STACK_TOUCH_TARGET_EFFECT_PREV );

    drawEffectDetailButton( effectMode, pressedTarget == M5STACK_TOUCH_TARGET_EFFECT_DETAIL );

    drawTriangleButton( CONTROL_RIGHT_X, FX_BUTTON_Y, true, pressedTarget == M5STACK_TOUCH_TARGET_EFFECT_NEXT );
  }

  void drawColorButton( uint32_t color, bool pressed ) {
    const uint16_t buttonColor = TFT_CYAN;

    const uint8_t effectMode = getCurrentEffectMode();

    const M5StackEffectColorCapabilities colorCapability =
      getEffectColorCapabilities( effectMode );

    const bool primaryColorUsed = colorCapability.color1;

    const bool anyColorUsed = effectUsesAnyColor( colorCapability );

    uint16_t backgroundColor = pressed ? buttonColor : TFT_BLACK;

    uint16_t titleColor = pressed ? TFT_BLACK : TFT_WHITE;

    uint16_t capabilityColor =
      pressed ? TFT_BLACK :
      ( primaryColorUsed ? TFT_CYAN : ( anyColorUsed ? TFT_YELLOW : TFT_DARKGREY ) );

    uint16_t previewColor = rgbTo565( R(color), G(color), B(color) );

    display.fillRect( COLOR_BUTTON_X - 2, MAIN_BOTTOM_BUTTON_Y - 2, COLOR_BUTTON_W + 4, MAIN_BOTTOM_BUTTON_H + 4, TFT_BLACK );

    display.fillRect( COLOR_BUTTON_X, MAIN_BOTTOM_BUTTON_Y, COLOR_BUTTON_W, MAIN_BOTTOM_BUTTON_H, backgroundColor );

    display.drawRect( COLOR_BUTTON_X, MAIN_BOTTOM_BUTTON_Y, COLOR_BUTTON_W, MAIN_BOTTOM_BUTTON_H, buttonColor );

    display.drawRect( COLOR_BUTTON_X + 1, MAIN_BOTTOM_BUTTON_Y + 1, COLOR_BUTTON_W - 2, MAIN_BOTTOM_BUTTON_H - 2, buttonColor );

    static constexpr int16_t SWATCH_X = 28;
    static constexpr int16_t SWATCH_W = 24;
    static constexpr int16_t SWATCH_H = 24;

    int16_t swatchY = MAIN_BOTTOM_BUTTON_Y + ( ( MAIN_BOTTOM_BUTTON_H - SWATCH_H ) / 2 );

    if ( primaryColorUsed ) {
      display.fillRect( SWATCH_X, swatchY, SWATCH_W, SWATCH_H, previewColor );

      display.drawRect( SWATCH_X, swatchY, SWATCH_W, SWATCH_H, TFT_WHITE );
    }
    else {
      display.fillRect( SWATCH_X, swatchY, SWATCH_W, SWATCH_H, TFT_BLACK );

      display.drawRect( SWATCH_X, swatchY, SWATCH_W, SWATCH_H, TFT_DARKGREY );

      display.drawLine( SWATCH_X + 4, swatchY + 4, SWATCH_X + SWATCH_W - 5, swatchY + SWATCH_H - 5, TFT_DARKGREY );

      display.drawLine( SWATCH_X + SWATCH_W - 5, swatchY + 4, SWATCH_X + 4, swatchY + SWATCH_H - 5, TFT_DARKGREY );
    }

    char capabilityText[16];

    getEffectColorCapabilityText(
      effectMode,
      capabilityText,
      sizeof(capabilityText)
    );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( titleColor, backgroundColor );

    display.setTextSize( 2 );

    display.drawString( "COLOR", 105, MAIN_BOTTOM_BUTTON_Y + 12 );

    display.setTextColor( capabilityColor, backgroundColor );

    display.setTextSize( 1 );

    display.drawString( capabilityText, 105, MAIN_BOTTOM_BUTTON_Y + 29 );

    touchState.colorButtonVisualPressed = pressed;
  }

  void drawPresetOpenButton( bool pressed ) {
    const uint16_t buttonColor = TFT_CYAN;

    uint16_t backgroundColor = pressed ? buttonColor : TFT_BLACK;

    uint16_t textColor = pressed ? TFT_BLACK : TFT_WHITE;

    display.fillRect( PRESET_OPEN_BUTTON_X - 2, MAIN_BOTTOM_BUTTON_Y - 2, PRESET_OPEN_BUTTON_W + 4, MAIN_BOTTOM_BUTTON_H + 4, TFT_BLACK );

    display.fillRect( PRESET_OPEN_BUTTON_X, MAIN_BOTTOM_BUTTON_Y, PRESET_OPEN_BUTTON_W, MAIN_BOTTOM_BUTTON_H, backgroundColor );

    display.drawRect( PRESET_OPEN_BUTTON_X, MAIN_BOTTOM_BUTTON_Y, PRESET_OPEN_BUTTON_W, MAIN_BOTTOM_BUTTON_H, buttonColor );

    display.drawRect( PRESET_OPEN_BUTTON_X + 1, MAIN_BOTTOM_BUTTON_Y + 1, PRESET_OPEN_BUTTON_W - 2, MAIN_BOTTOM_BUTTON_H - 2, buttonColor );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( textColor, backgroundColor );

    display.setTextSize( 2 );

    display.drawString( "PRESET", PRESET_OPEN_BUTTON_X + (PRESET_OPEN_BUTTON_W / 2), MAIN_BOTTOM_BUTTON_Y + (MAIN_BOTTOM_BUTTON_H / 2) );

    touchState.presetOpenButtonVisualPressed = pressed;
  }

  void drawBackButton( bool pressed ) {
    const uint16_t buttonColor = TFT_CYAN;

    uint16_t backgroundColor = pressed ? buttonColor : TFT_BLACK;

    uint16_t iconColor = pressed ? TFT_BLACK : buttonColor;

    display.fillRect( BACK_BUTTON_X - 2, BACK_BUTTON_Y - 2, BACK_BUTTON_W + 4, BACK_BUTTON_H + 4, TFT_BLACK );

    display.fillRect( BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, backgroundColor );

    display.drawRect( BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, buttonColor );

    display.drawRect( BACK_BUTTON_X + 1, BACK_BUTTON_Y + 1, BACK_BUTTON_W - 2, BACK_BUTTON_H - 2, buttonColor );

    int16_t centerX = BACK_BUTTON_X + (BACK_BUTTON_W / 2);

    int16_t centerY = BACK_BUTTON_Y + (BACK_BUTTON_H / 2);

    display.fillTriangle( centerX - 11, centerY, centerX - 1, centerY - 9, centerX - 1, centerY + 9, iconColor );

    display.fillRect( centerX - 1, centerY - 2, 13, 5, iconColor );

    touchState.backButtonVisualPressed = pressed;
  }

  void drawColorDetails( uint32_t color ) {
    display.fillRect( 0, 60, screenWidth, 78, TFT_BLACK );

    const M5StackEffectColorCapabilities capability =
      getEffectColorCapabilities( getCurrentEffectMode() );

    const int16_t slotX[3] = {
      COLOR_SLOT_1_X,
      COLOR_SLOT_2_X,
      COLOR_SLOT_3_X
    };

    Segment& mainSegment = strip.getMainSegment();

    for ( uint8_t colorSlot = 0; colorSlot < 3; colorSlot++ ) {
      const bool enabled =
        isEffectColorSlotEnabled( capability, colorSlot );

      const bool selected =
        enabled && colorSlot == selectedColorSlot;

      char slotLabel[4];

      snprintf( slotLabel, sizeof(slotLabel), "C%u", colorSlot + 1 );

      display.setTextDatum( textdatum_t::middle_center );

      display.setTextColor(
        selected ? TFT_CYAN : ( enabled ? TFT_WHITE : TFT_DARKGREY ),
        TFT_BLACK
      );

      display.setTextSize( 1 );

      display.drawString(
        slotLabel,
        slotX[ colorSlot ] + ( COLOR_SLOT_W / 2 ),
        COLOR_SLOT_LABEL_Y
      );

      if ( enabled ) {
        const uint32_t slotColor = mainSegment.colors[ colorSlot ];

        const uint16_t previewColor =
          rgbTo565( R(slotColor), G(slotColor), B(slotColor) );

        display.fillRect(
          slotX[ colorSlot ],
          COLOR_SLOT_Y,
          COLOR_SLOT_W,
          COLOR_SLOT_H,
          previewColor
        );

        display.drawRect(
          slotX[ colorSlot ],
          COLOR_SLOT_Y,
          COLOR_SLOT_W,
          COLOR_SLOT_H,
          selected ? TFT_CYAN : TFT_DARKGREY
        );

        if ( selected ) {
          display.drawRect(
            slotX[ colorSlot ] + 1,
            COLOR_SLOT_Y + 1,
            COLOR_SLOT_W - 2,
            COLOR_SLOT_H - 2,
            TFT_WHITE
          );
        }
      }
      else {
        display.fillRect(
          slotX[ colorSlot ],
          COLOR_SLOT_Y,
          COLOR_SLOT_W,
          COLOR_SLOT_H,
          TFT_BLACK
        );

        display.drawRect(
          slotX[ colorSlot ],
          COLOR_SLOT_Y,
          COLOR_SLOT_W,
          COLOR_SLOT_H,
          TFT_DARKGREY
        );

        display.drawLine(
          slotX[ colorSlot ] + 8,
          COLOR_SLOT_Y + 5,
          slotX[ colorSlot ] + COLOR_SLOT_W - 9,
          COLOR_SLOT_Y + COLOR_SLOT_H - 6,
          TFT_DARKGREY
        );

        display.drawLine(
          slotX[ colorSlot ] + COLOR_SLOT_W - 9,
          COLOR_SLOT_Y + 5,
          slotX[ colorSlot ] + 8,
          COLOR_SLOT_Y + COLOR_SLOT_H - 6,
          TFT_DARKGREY
        );
      }
    }

    char selectedText[24];
    char customColorLabel[32];

    const bool hasCustomColorLabel =
      getEffectColorSlotCustomLabel(
        getCurrentEffectMode(),
        selectedColorSlot,
        customColorLabel,
        sizeof(customColorLabel)
      );

    snprintf(
      selectedText,
      sizeof(selectedText),
      "C%u #%02X%02X%02X",
      selectedColorSlot + 1,
      R(color),
      G(color),
      B(color)
    );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 2 );

    display.drawString(
      selectedText,
      screenWidth / 2,
      116
    );

    char slotHintText[56];

    if ( hasCustomColorLabel ) {
      snprintf(
        slotHintText,
        sizeof(slotHintText),
        "Role: %s  Hold: %s",
        customColorLabel,
        color == 0 ? "RESTORE" : "BLACK"
      );

      display.setTextColor( TFT_CYAN, TFT_BLACK );
    }
    else {
      snprintf(
        slotHintText,
        sizeof(slotHintText),
        "Hold: %s",
        color == 0 ? "RESTORE" : "BLACK"
      );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
    }

    display.setTextSize( 1 );

    display.drawString(
      slotHintText,
      screenWidth / 2,
      130
    );

    display.drawFastHLine( 32, 136, screenWidth - 64, TFT_DARKGREY );
  }

  void drawHue( uint8_t hueValue, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    char valueText[8];

    snprintf( valueText, sizeof(valueText), "%u", hueValue );

    drawNumericControl( 138, 56, "Hue", 144, HUE_BUTTON_Y, HUE_BUTTON_Y + (CONTROL_BUTTON_H / 2), valueText, pressedTarget, M5STACK_TOUCH_TARGET_HUE_DOWN, M5STACK_TOUCH_TARGET_HUE_UP );
  }

  void drawSaturation( uint8_t saturationValue, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    char valueText[8];

    snprintf( valueText, sizeof(valueText), "%u", saturationValue );

    drawNumericControl( 194, 46, "Saturation", SATURATION_LABEL_Y, SATURATION_BUTTON_Y, SATURATION_BUTTON_Y + (CONTROL_BUTTON_H / 2), valueText, pressedTarget, M5STACK_TOUCH_TARGET_SATURATION_DOWN, M5STACK_TOUCH_TARGET_SATURATION_UP );
  }

  void drawSpeed( uint8_t speedValue, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    char label[32];

    if ( !getEffectSpeedControlMetadata( getCurrentEffectMode(), label, sizeof(label) ) ) {
      display.fillRect( 0, 62, screenWidth, 58, TFT_BLACK );
      return;
    }

    char valueText[8];

    snprintf( valueText, sizeof(valueText), "%u", speedValue );

    drawNumericControl( 62, 58, label, 70, SPEED_BUTTON_Y, 99, valueText, pressedTarget, M5STACK_TOUCH_TARGET_SPEED_DOWN, M5STACK_TOUCH_TARGET_SPEED_UP );
  }

  void drawIntensity( uint8_t intensityValue, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    char label[32];

    if ( !getEffectIntensityControlMetadata( getCurrentEffectMode(), label, sizeof(label) ) ) {
      display.fillRect( 0, 120, screenWidth, 60, TFT_BLACK );
      return;
    }

    char valueText[8];

    snprintf( valueText, sizeof(valueText), "%u", intensityValue );

    drawNumericControl( 120, 60, label, 128, INTENSITY_BUTTON_Y, 157, valueText, pressedTarget, M5STACK_TOUCH_TARGET_INTENSITY_DOWN, M5STACK_TOUCH_TARGET_INTENSITY_UP );
  }

  void drawPalette( uint8_t paletteId, M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    display.fillRect( 0, 180, screenWidth, 60, TFT_BLACK );

    if ( !isEffectPaletteControlVisible( getCurrentEffectMode() ) ) {
      return;
    }

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "Palette", screenWidth / 2, PALETTE_LABEL_Y );

    drawTriangleButton( CONTROL_LEFT_X, PALETTE_BUTTON_Y, false, pressedTarget == M5STACK_TOUCH_TARGET_PALETTE_PREV );

    drawTriangleButton( CONTROL_RIGHT_X, PALETTE_BUTTON_Y, true, pressedTarget == M5STACK_TOUCH_TARGET_PALETTE_NEXT );

    char paletteName[64];

    getPaletteName( paletteId, paletteName, sizeof(paletteName) );

    if ( strlen(paletteName) > 22 ) {
      paletteName[22] = '\0';
    }

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    if ( strlen(paletteName) <= 10 ) {
      display.setTextSize( 2 );
    }
    else {
      display.setTextSize( 1 );
    }

    display.drawString( paletteName, screenWidth / 2, PALETTE_BUTTON_Y + (CONTROL_BUTTON_H / 2) );
  }

  void drawEffectPageName( uint8_t effectMode ) {
    display.fillRect( 40, 30, 240, 26, TFT_BLACK );

    char effectName[64];
    char capabilityText[40];
    bool incompatibleWithCurrentSegment = false;

    getEffectName( effectMode, effectName, sizeof(effectName) );

    getEffectCapabilityText(
      effectMode,
      capabilityText,
      sizeof(capabilityText),
      incompatibleWithCurrentSegment
    );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( effectName, screenWidth / 2, 36 );

    display.setTextColor(
      ( incompatibleWithCurrentSegment || isAudioUnavailableForEffect( effectMode ) )
        ? TFT_YELLOW
        : TFT_CYAN,
      TFT_BLACK
    );

    display.drawString( capabilityText, screenWidth / 2, 49 );
  }

  String getPresetDisplayName( uint8_t presetId ) {
    if ( presetId == 0 ) {
      return "Custom State";
    }

    if ( pendingPresetId == presetId && pendingPresetName.length() > 0 ) {
      return pendingPresetName;
    }

    String name;

    if ( getCachedPresetName( presetId, name ) ) {
      return name;
    }

    char fallback[24];

    snprintf( fallback, sizeof(fallback), "Preset %u", presetId );

    return String(fallback);
  }

  void drawPresetDetails( uint8_t presetId, bool applying = false ) {
    display.fillRect( 0, 60, screenWidth, 118, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    // -------------------------------------------------------
    // Internal Preset ID scan number is intentionally hidden.
    // -------------------------------------------------------

    if ( !presetCacheReady ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "Loading Presets", screenWidth / 2, PRESET_NAME_Y );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "Updating Preset List", screenWidth / 2, PRESET_ID_Y );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "WLED remains active", screenWidth / 2, PRESET_STATUS_Y );

      return;
    }

    if ( presetNoEntries ) {
      display.setTextColor( TFT_RED, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "No Presets", screenWidth / 2, PRESET_NAME_Y );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "No saved preset found", screenWidth / 2, PRESET_ID_Y );

      display.drawString( "Use MANAGE to create", screenWidth / 2, PRESET_STATUS_Y );

      return;
    }

    if ( presetId == 0 ) {
      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "Custom State", screenWidth / 2, PRESET_NAME_Y );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "No active preset", screenWidth / 2, PRESET_ID_Y );

      display.drawString( "Use arrows to apply", screenWidth / 2, PRESET_STATUS_Y );

      return;
    }

    String presetName = getPresetDisplayName( presetId );

    if ( presetName.length() > 28 ) {
      presetName = presetName.substring( 0, 25 ) + "...";
    }

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    if ( presetName.length() <= 12 ) {
      display.setTextSize( 2 );
    }
    else {
      display.setTextSize( 1 );
    }

    display.drawString( presetName, screenWidth / 2, PRESET_NAME_Y );

    char idText[24];

    snprintf( idText, sizeof(idText), "Preset ID: %u", presetId );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( idText, screenWidth / 2, PRESET_ID_Y );

    if (applying) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );

      display.drawString( "Applying...", screenWidth / 2, PRESET_STATUS_Y );
    }
    else if ( currentPreset == presetId ) {
      display.setTextColor( TFT_GREEN, TFT_BLACK );

      display.drawString( "Active Preset", screenWidth / 2, PRESET_STATUS_Y );
    }
    else {
      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Saved WLED Preset", screenWidth / 2, PRESET_STATUS_Y );
    }
  }

  void drawPresetTextButton( int16_t x, int16_t y, int16_t w, int16_t h, const char* label, bool enabled, bool pressed, uint8_t textSize ) {
    uint16_t buttonColor = enabled ? TFT_CYAN : TFT_DARKGREY;

    uint16_t backgroundColor = ( enabled && pressed ) ? buttonColor : TFT_BLACK;

    uint16_t textColor = ( enabled && pressed ) ? TFT_BLACK : ( enabled ? TFT_WHITE : TFT_DARKGREY );

    display.fillRect( x - 2, y - 2, w + 4, h + 4, TFT_BLACK );

    display.fillRect( x, y, w, h, backgroundColor );

    display.drawRect( x, y, w, h, buttonColor );

    display.drawRect( x + 1, y + 1, w - 2, h - 2, buttonColor );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( textColor, backgroundColor );

    display.setTextSize( textSize );

    display.drawString( label, x + (w / 2), y + (h / 2) );
  }

  void drawPresetDangerTextButton( int16_t x, int16_t y, int16_t w, int16_t h, const char* label, bool enabled, bool pressed, uint8_t textSize ) {
    uint16_t buttonColor = enabled ? TFT_RED : TFT_DARKGREY;

    uint16_t backgroundColor = ( enabled && pressed ) ? buttonColor : TFT_BLACK;

    uint16_t textColor = ( enabled && pressed ) ? TFT_BLACK : ( enabled ? TFT_RED : TFT_DARKGREY );

    display.fillRect( x - 2, y - 2, w + 4, h + 4, TFT_BLACK );

    display.fillRect( x, y, w, h, backgroundColor );

    display.drawRect( x, y, w, h, buttonColor );

    display.drawRect( x + 1, y + 1, w - 2, h - 2, buttonColor );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( textColor, backgroundColor );

    display.setTextSize( textSize );

    display.drawString( label, x + (w / 2), y + (h / 2) );
  }

  void drawPresetManageButton( bool pressed ) {
    bool enabled = ( presetCacheReady && !presetCacheBuilding && pendingPresetId == 0 );

    const char* label = presetCacheReady ? "MANAGE" : "LOADING";

    drawPresetTextButton( PRESET_MANAGE_BUTTON_X, PRESET_MANAGE_BUTTON_Y, PRESET_MANAGE_BUTTON_W, PRESET_MANAGE_BUTTON_H, label, enabled, pressed && enabled, 1 );

    touchState.presetManageButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetNavigation( M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    display.fillRect( 0, 178, screenWidth, 62, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "Preset", screenWidth / 2, PRESET_NAV_LABEL_Y );

    drawTriangleButton( CONTROL_LEFT_X, PRESET_NAV_BUTTON_Y, false, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_PREV );

    drawTriangleButton( CONTROL_RIGHT_X, PRESET_NAV_BUTTON_Y, true, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_NEXT );

    drawPresetManageButton( pressedTarget == M5STACK_TOUCH_TARGET_PRESET_MANAGE );
  }

  uint16_t getBatteryStatusColor() const {
    if ( !batteryStatus.available || !batteryStatus.present ) {
      return TFT_DARKGREY;
    }

    if ( batteryStatus.level <= 15 ) {
      return TFT_RED;
    }

    if ( batteryStatus.level <= 35 ) {
      return TFT_YELLOW;
    }

    return TFT_GREEN;
  }

  bool sampleBatteryStatus( bool forceRead = false ) {
    const unsigned long now = millis();

    if (
      !forceRead &&
      batteryStatusInitialized &&
      now - lastBatteryStatusRead < BATTERY_STATUS_UPDATE_MS
    ) {
      return false;
    }

    lastBatteryStatusRead = now;

    M5StackBatteryStatus newStatus;
    hardwareBackend.readBatteryStatus( newStatus );

    const bool changed =
      !batteryStatusInitialized ||
      newStatus.available != batteryStatus.available ||
      newStatus.present != batteryStatus.present ||
      newStatus.charging != batteryStatus.charging ||
      newStatus.level != batteryStatus.level;

    batteryStatus = newStatus;
    batteryStatusInitialized = true;

    return changed;
  }

  void drawMainBatteryStatus() {
    display.fillRect(
      BATTERY_STATUS_LEFT,
      28,
      BATTERY_STATUS_RIGHT - BATTERY_STATUS_LEFT,
      28,
      TFT_BLACK
    );

    const uint16_t batteryColor = getBatteryStatusColor();

    // Battery body + positive terminal.
    display.drawRect(
      BATTERY_ICON_X,
      BATTERY_ICON_Y,
      BATTERY_ICON_W,
      BATTERY_ICON_H,
      batteryColor
    );

    display.fillRect(
      BATTERY_ICON_X + BATTERY_ICON_W,
      BATTERY_ICON_Y + 3,
      2,
      4,
      batteryColor
    );

    if ( batteryStatus.available && batteryStatus.present ) {
      const int16_t interiorW = BATTERY_ICON_W - 4;
      int16_t fillW =
        ( interiorW * batteryStatus.level + 99 ) / 100;

      fillW = constrain( fillW, 0, interiorW );

      if ( fillW > 0 ) {
        display.fillRect(
          BATTERY_ICON_X + 2,
          BATTERY_ICON_Y + 2,
          fillW,
          BATTERY_ICON_H - 4,
          batteryColor
        );
      }
    }

    char batteryText[8];

    if ( batteryStatus.available && batteryStatus.present ) {
      snprintf(
        batteryText,
        sizeof(batteryText),
        "%u%%",
        batteryStatus.level
      );
    }
    else {
      strlcpy(
        batteryText,
        "--%",
        sizeof(batteryText)
      );
    }

    display.setTextDatum( textdatum_t::middle_center );
    display.setTextColor( batteryColor, TFT_BLACK );
    display.setTextSize( 1 );

    display.drawString(
      batteryText,
      BATTERY_PERCENT_X,
      BATTERY_PERCENT_Y
    );
  }

  void drawMainNetworkStatusLine(
    const String& networkStatusText,
    uint16_t textColor = TFT_WHITE
  ) {
    display.fillRect(
      HEADER_NETWORK_LEFT,
      28,
      HEADER_NETWORK_RIGHT - HEADER_NETWORK_LEFT,
      28,
      TFT_BLACK
    );

    display.setTextDatum( textdatum_t::middle_center );
    display.setTextColor( textColor, TFT_BLACK );
    display.setTextSize( 1 );

    display.drawString(
      networkStatusText,
      HEADER_NETWORK_CENTER_X,
      HEADER_IP_Y
    );
  }

  void drawMainScreen( const String& networkStatusText ) {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_MAIN;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 2 );

    display.drawString( "WLED M5Stack CoreS3", HEADER_CENTER_X, HEADER_TITLE_Y );

    drawMainNetworkStatusLine( networkStatusText );

    sampleBatteryStatus( true );
    drawMainBatteryStatus();

    display.drawFastHLine( 8, 58, screenWidth - 16, TFT_DARKGREY );

    drawPowerButton( bri > 0, false );

    drawBrightness( bri, M5STACK_TOUCH_TARGET_NONE );

    uint8_t effectMode = getCurrentEffectMode();

    drawEffect( effectMode, M5STACK_TOUCH_TARGET_NONE );

    uint32_t primaryColor = getPrimaryColor();

    drawColorButton( primaryColor, false );

    drawPresetOpenButton( false );

    syncLogicalColorFromRgb( primaryColor );

    lastLedState = bri > 0 ? 1 : 0;

    lastBrightnessValue = bri;

    lastEffectMode = effectMode;

    lastSpeedValue = getCurrentSpeed();

    lastIntensityValue = getCurrentIntensity();

    lastPaletteValue = getCurrentPalette();

    lastPresetValue = currentPreset;

    lastPrimaryColor = primaryColor;

    lastPrimaryColorValid = true;
  }

  void drawColorScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_COLOR;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    const uint8_t effectMode = getCurrentEffectMode();

    const M5StackEffectColorCapabilities colorCapability =
      getEffectColorCapabilities( effectMode );

    char capabilityText[16];
    char subtitleText[32];

    getEffectColorCapabilityText(
      effectMode,
      capabilityText,
      sizeof(capabilityText)
    );

    snprintf(
      subtitleText,
      sizeof(subtitleText),
      "Effect uses %s",
      capabilityText
    );

    const bool anyColorUsed =
      effectUsesAnyColor( colorCapability );

    drawStandardPageHeader(
      "COLOR",
      subtitleText,
      anyColorUsed ? TFT_WHITE : TFT_YELLOW
    );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    uint32_t primaryColor = getPrimaryColor();

    if ( !anyColorUsed ) {
      selectedColorSlot = 0;

      lastSelectedColorValid = false;

      display.fillRect( 0, 60, screenWidth, 180, TFT_BLACK );

      display.setTextDatum( textdatum_t::middle_center );

      display.setTextColor( TFT_YELLOW, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "COLOR NOT USED", screenWidth / 2, 104 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString(
        "This effect does not use color slots.",
        screenWidth / 2,
        139
      );
    }
    else {
      normalizeSelectedColorSlot( effectMode );

      const uint32_t selectedColor = getSelectedColor();

      syncLogicalColorFromRgb( selectedColor );

      lastSelectedColor = selectedColor;
      lastSelectedColorValid = true;

      drawColorDetails( selectedColor );

      drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

      drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );
    }

    cacheCurrentColorSlots();

    lastLedState = bri > 0 ? 1 : 0;

    lastEffectMode = effectMode;

    lastPrimaryColor = primaryColor;

    lastPrimaryColorValid = true;

    lastHueValue = logicalHueValue;

    lastSaturationValue = logicalSaturationValue;
  }

  void drawEffectDetailScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_EFFECT;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 2 );

    display.drawString( "EFFECT", screenWidth / 2, 18 );

    uint8_t effectMode = getCurrentEffectMode();

    drawEffectPageName( effectMode );

    display.drawFastHLine( 8, 58, screenWidth - 16, TFT_DARKGREY );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    uint8_t speedValue = getCurrentSpeed();

    uint8_t intensityValue = getCurrentIntensity();

    uint8_t paletteValue = getCurrentPalette();

    drawSpeed( speedValue, M5STACK_TOUCH_TARGET_NONE );

    drawIntensity( intensityValue, M5STACK_TOUCH_TARGET_NONE );

    drawPalette( paletteValue, M5STACK_TOUCH_TARGET_NONE );

    lastLedState = bri > 0 ? 1 : 0;

    lastEffectMode = effectMode;

    lastSpeedValue = speedValue;

    lastIntensityValue = intensityValue;

    lastPaletteValue = paletteValue;
  }

  void drawPresetScreen() {
    syncPresetNavigationCursorFromCurrentPreset();

    normalizePresetNavigationCursor();

    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_PRESET;

    presetSubPage = PRESET_SUBPAGE_NAV;

    presetSaveOperationState = PRESET_SAVE_OP_IDLE;

    presetSaveOperationIsOverwrite = false;

    presetSaveCandidateId = 0;

    presetSaveCandidateName = "";

    presetOverwriteTargetId = 0;

    presetOverwriteTargetName = "";

    presetDeleteOperationState = PRESET_DELETE_OP_IDLE;

    presetDeleteTargetId = 0;

    presetDeleteTargetName = "";

    presetDeleteWasCurrentPreset = false;
    presetDeleteWasBootPreset = false;

    presetDeleteResultStartMs = 0;

    presetBootOperationState = PRESET_BOOT_OP_IDLE;
    presetBootTargetId = 0;
    presetBootTargetName = "NONE";
    presetBootResultStartMs = 0;

    presetSaveHoldStartTime = 0;

    presetSaveHoldTriggered = false;

    presetSaveResultStartMs = 0;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    drawStandardPageHeader( "PRESET", "Saved WLED Preset" );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    uint8_t presetId = getDisplayedPresetId();

    bool applying = ( pendingPresetId > 0 );

    drawPresetDetails( presetId, applying );

    drawPresetNavigation( M5STACK_TOUCH_TARGET_NONE );

    lastLedState = bri > 0 ? 1 : 0;

    lastPresetValue = presetId;
    lastBootPresetValue = bootPreset;

    lastPresetsModifiedTime = presetsModifiedTime;
  }

  void drawPresetSaveNewButton( bool pressed ) {
    uint8_t freePresetId = findFirstFreePresetId();

    bool enabled = ( presetCacheReady && !presetCacheBuilding && freePresetId > 0 && pendingPresetId == 0 && !presetNeedsSaving() );

    char buttonLabel[24];

    if ( !presetCacheReady || presetCacheBuilding ) {
      strlcpy( buttonLabel, "SAVE NEW", sizeof(buttonLabel) );
    }
    else if ( freePresetId == 0 ) {
      strlcpy( buttonLabel, "SAVE NEW  FULL", sizeof(buttonLabel) );
    }
    else {
      snprintf(
        buttonLabel,
        sizeof(buttonLabel),
        "SAVE NEW  #%u",
        freePresetId
      );
    }

    drawPresetTextButton(
      PRESET_SAVE_NEW_BUTTON_X,
      PRESET_SAVE_NEW_BUTTON_Y,
      PRESET_SAVE_NEW_BUTTON_W,
      PRESET_SAVE_NEW_BUTTON_H,
      buttonLabel,
      enabled,
      pressed && enabled,
      2
    );

    touchState.presetSaveNewButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetOverwriteOpenButton( bool pressed ) {
    bool enabled = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && pendingPresetId == 0 && !presetNeedsSaving() );

    drawPresetTextButton( PRESET_OVERWRITE_BUTTON_X, PRESET_OVERWRITE_BUTTON_Y, PRESET_OVERWRITE_BUTTON_W, PRESET_OVERWRITE_BUTTON_H, "OVERWRITE", enabled, pressed && enabled, 2 );

    touchState.presetOverwriteOpenButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetDeleteOpenButton( bool pressed ) {
    bool enabled = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && pendingPresetId == 0 && !presetNeedsSaving() );

    drawPresetDangerTextButton( PRESET_DELETE_BUTTON_X, PRESET_DELETE_BUTTON_Y, PRESET_DELETE_BUTTON_W, PRESET_DELETE_BUTTON_H, "DELETE", enabled, pressed && enabled, 2 );

    touchState.presetDeleteOpenButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetBootOpenButton( bool pressed ) {
    bool enabled = ( presetCacheReady && !presetCacheBuilding && pendingPresetId == 0 && !presetNeedsSaving() );

    drawPresetTextButton( PRESET_BOOT_BUTTON_X, PRESET_BOOT_BUTTON_Y, PRESET_BOOT_BUTTON_W, PRESET_BOOT_BUTTON_H, "BOOT PRESET", enabled, pressed && enabled, 2 );

    touchState.presetBootOpenButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetSaveHoldButton( bool pressed ) {
    bool enabled = ( presetSaveOperationState == PRESET_SAVE_OP_IDLE && presetCacheReady && !presetCacheBuilding && presetSaveCandidateId > 0 && findPresetCacheIndex( presetSaveCandidateId ) < 0 && pendingPresetId == 0 && !presetNeedsSaving() );

    drawPresetTextButton( PRESET_SAVE_HOLD_BUTTON_X, PRESET_SAVE_HOLD_BUTTON_Y, PRESET_SAVE_HOLD_BUTTON_W, PRESET_SAVE_HOLD_BUTTON_H, "HOLD TO SAVE", enabled, pressed && enabled, 2 );

    touchState.presetSaveHoldButtonVisualPressed = ( pressed && enabled );
  }

  void getPresetManageCurrentStateText(
    char* text,
    size_t textSize
  ) {
    if ( text == nullptr || textSize == 0 ) {
      return;
    }

    text[0] = '\0';

    if ( pendingPresetId > 0 ) {
      snprintf(
        text,
        textSize,
        "Current: Applying Preset #%u",
        pendingPresetId
      );

      return;
    }

    if ( currentPreset == 0 ) {
      strlcpy(
        text,
        "Current: Custom State",
        textSize
      );

      return;
    }

    snprintf(
      text,
      textSize,
      "Current: Active Preset #%u",
      currentPreset
    );
  }

  void drawPresetManageScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_PRESET;

    presetSubPage = PRESET_SUBPAGE_MANAGE;

    presetSaveOperationState = PRESET_SAVE_OP_IDLE;

    presetSaveOperationIsOverwrite = false;

    presetSaveCandidateId = 0;

    presetSaveCandidateName = "";

    presetOverwriteTargetId = 0;

    presetOverwriteTargetName = "";

    presetDeleteOperationState = PRESET_DELETE_OP_IDLE;

    presetDeleteTargetId = 0;

    presetDeleteTargetName = "";

    presetDeleteWasCurrentPreset = false;
    presetDeleteWasBootPreset = false;

    presetDeleteResultStartMs = 0;

    presetBootOperationState = PRESET_BOOT_OP_IDLE;
    presetBootTargetId = 0;
    presetBootTargetName = "NONE";
    presetBootResultStartMs = 0;

    presetSaveHoldStartTime = 0;

    presetSaveHoldTriggered = false;

    presetSaveResultStartMs = 0;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    char manageSubtitle[40];

    getPresetManageCurrentStateText(
      manageSubtitle,
      sizeof(manageSubtitle)
    );

    drawStandardPageHeader( "PRESET MANAGE", manageSubtitle );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    drawPresetSaveNewButton( false );

    drawPresetOverwriteOpenButton( false );

    drawPresetDeleteOpenButton( false );

    drawPresetBootOpenButton( false );

    lastLedState = bri > 0 ? 1 : 0;

    lastPresetValue = currentPreset;

    lastPresetsModifiedTime = presetsModifiedTime;
  }

  // =========================================================
  // SAVE NEW / OVERWRITE operation status
  //
  // Internal 1..250 cache scan position is intentionally
  // hidden from the user.
  // =========================================================

  void drawPresetSaveOperationStatus() {
    if ( currentPage != SCREEN_PRESET || ( presetSubPage != PRESET_SUBPAGE_SAVE && presetSubPage != PRESET_SUBPAGE_OVERWRITE ) ) {
      return;
    }

    display.fillRect( 0, 60, screenWidth, 180, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    if ( presetSaveOperationState == PRESET_SAVE_OP_WAIT_WLED ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( presetSaveOperationIsOverwrite ? "Overwriting..." : "Saving...", screenWidth / 2, 110 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( presetSaveCandidateName, screenWidth / 2, 142 );

      char idText[24];

      snprintf( idText, sizeof(idText), "Preset ID: %u", presetSaveCandidateId );

      display.drawString( idText, screenWidth / 2, 163 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Writing WLED Preset", screenWidth / 2, 188 );

      return;
    }

    if ( presetSaveOperationState == PRESET_SAVE_OP_WAIT_CACHE ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "Updating Preset List", screenWidth / 2, 110 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( presetSaveOperationIsOverwrite ? "Verifying Overwrite..." : "Verifying New Preset...", screenWidth / 2, 158 );

      return;
    }

    if ( presetSaveOperationState == PRESET_SAVE_OP_SUCCESS ) {
      display.setTextColor( TFT_GREEN, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( presetSaveOperationIsOverwrite ? "OVERWRITTEN" : "SAVED", screenWidth / 2, 104 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( presetSaveCandidateName, screenWidth / 2, 140 );

      char idText[24];

      snprintf( idText, sizeof(idText), "Preset ID: %u", presetSaveCandidateId );

      display.drawString( idText, screenWidth / 2, 165 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Preset cache verified", screenWidth / 2, 192 );

      return;
    }

    if ( presetSaveOperationState == PRESET_SAVE_OP_FAILED ) {
      display.setTextColor( TFT_RED, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( presetSaveOperationIsOverwrite ? "OVERWRITE FAILED" : "SAVE FAILED", screenWidth / 2, 108 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "Preset was not verified", screenWidth / 2, 150 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Returning to PRESET MANAGE", screenWidth / 2, 180 );
    }
  }

  void drawPresetSaveScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_PRESET;

    presetSubPage = PRESET_SUBPAGE_SAVE;

    if ( presetSaveOperationState == PRESET_SAVE_OP_IDLE ) {
      presetSaveOperationIsOverwrite = false;
    }

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    drawStandardPageHeader( "SAVE PRESET", "Current WLED State" );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    if ( presetSaveOperationState != PRESET_SAVE_OP_IDLE ) {
      drawPresetSaveOperationStatus();
      return;
    }

    if ( presetSaveCandidateId == 0 ) {
      display.setTextColor( TFT_RED, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "No Free ID", screenWidth / 2, 110 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "Preset IDs 1-250 are unavailable", screenWidth / 2, 150 );

      return;
    }

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    if ( presetSaveCandidateName.length() <= 18 ) {
      display.setTextSize( 2 );
    }
    else {
      display.setTextSize( 1 );
    }

    display.drawString( presetSaveCandidateName, screenWidth / 2, 88 );

    char idText[24];

    snprintf( idText, sizeof(idText), "Preset ID: %u", presetSaveCandidateId );

    display.setTextSize( 1 );

    display.drawString( idText, screenWidth / 2, 120 );

    display.setTextColor( TFT_DARKGREY, TFT_BLACK );

    display.drawString( "Hold 1 second to save", screenWidth / 2, 151 );

    drawPresetSaveHoldButton( false );

    lastLedState = bri > 0 ? 1 : 0;
  }

  void drawPresetOverwriteNavigation( M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    display.fillRect( 0, 112, screenWidth, 58, TFT_BLACK );

    drawTriangleButton( CONTROL_LEFT_X, PRESET_OVERWRITE_NAV_BUTTON_Y, false, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV );

    drawTriangleButton( CONTROL_RIGHT_X, PRESET_OVERWRITE_NAV_BUTTON_Y, true, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_NEXT );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_DARKGREY, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "TARGET", screenWidth / 2, PRESET_OVERWRITE_NAV_BUTTON_Y + (CONTROL_BUTTON_H / 2) );
  }

  void drawPresetOverwriteHoldButton( bool pressed ) {
    bool enabled = ( presetSaveOperationState == PRESET_SAVE_OP_IDLE && presetCacheReady && !presetCacheBuilding && presetOverwriteTargetId > 0 && findPresetCacheIndex( presetOverwriteTargetId ) >= 0 && pendingPresetId == 0 && !presetNeedsSaving() );

    drawPresetTextButton( PRESET_OVERWRITE_HOLD_BUTTON_X, PRESET_OVERWRITE_HOLD_BUTTON_Y, PRESET_OVERWRITE_HOLD_BUTTON_W, PRESET_OVERWRITE_HOLD_BUTTON_H, "HOLD TO OVERWRITE", enabled, pressed && enabled, 1 );

    touchState.presetOverwriteHoldButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetOverwriteScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_PRESET;

    presetSubPage = PRESET_SUBPAGE_OVERWRITE;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    drawStandardPageHeader( "OVERWRITE PRESET", "Select destination only" );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    if ( presetSaveOperationState != PRESET_SAVE_OP_IDLE ) {
      drawPresetSaveOperationStatus();
      return;
    }

    if ( presetOverwriteTargetId == 0 || findPresetCacheIndex( presetOverwriteTargetId ) < 0 ) {
      display.setTextColor( TFT_RED, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "No Preset", screenWidth / 2, 104 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "Nothing can be overwritten", screenWidth / 2, 145 );

      return;
    }

    String displayName = presetOverwriteTargetName;

    if ( displayName.length() > 28 ) {
      displayName = displayName.substring( 0, 25 ) + "...";
    }

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    if ( displayName.length() <= 12 ) {
      display.setTextSize( 2 );
    }
    else {
      display.setTextSize( 1 );
    }

    display.drawString( displayName, screenWidth / 2, 82 );

    char idText[24];

    snprintf( idText, sizeof(idText), "Preset ID: %u", presetOverwriteTargetId );

    display.setTextSize( 1 );

    display.drawString( idText, screenWidth / 2, 105 );

    drawPresetOverwriteNavigation( M5STACK_TOUCH_TARGET_NONE );

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "Current WLED State", screenWidth / 2, 174 );

    display.setTextColor( TFT_DARKGREY, TFT_BLACK );

    display.drawString( "will replace this Preset", screenWidth / 2, 187 );

    drawPresetOverwriteHoldButton( false );

    lastLedState = bri > 0 ? 1 : 0;
  }

  void drawPresetDeleteNavigation( M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    display.fillRect( 0, 112, screenWidth, 58, TFT_BLACK );

    drawTriangleButton( CONTROL_LEFT_X, PRESET_DELETE_NAV_BUTTON_Y, false, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV );

    drawTriangleButton( CONTROL_RIGHT_X, PRESET_DELETE_NAV_BUTTON_Y, true, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_NEXT );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_RED, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "TARGET", screenWidth / 2, PRESET_DELETE_NAV_BUTTON_Y + (CONTROL_BUTTON_H / 2) );
  }

  void drawPresetDeleteHoldButton( bool pressed ) {
    bool enabled = ( presetDeleteOperationState == PRESET_DELETE_OP_IDLE && presetCacheReady && !presetCacheBuilding && presetDeleteTargetId > 0 && findPresetCacheIndex( presetDeleteTargetId ) >= 0 && pendingPresetId == 0 && !presetNeedsSaving() );

    drawPresetDangerTextButton( PRESET_DELETE_HOLD_BUTTON_X, PRESET_DELETE_HOLD_BUTTON_Y, PRESET_DELETE_HOLD_BUTTON_W, PRESET_DELETE_HOLD_BUTTON_H, "HOLD TO DELETE", enabled, pressed && enabled, 1 );

    touchState.presetDeleteHoldButtonVisualPressed = ( pressed && enabled );
  }

  // =========================================================
  // DELETE operation status
  //
  // Internal cache scan position is not displayed.
  // =========================================================

  void drawPresetDeleteOperationStatus() {
    if ( currentPage != SCREEN_PRESET || presetSubPage != PRESET_SUBPAGE_DELETE ) {
      return;
    }

    display.fillRect( 0, 60, screenWidth, 180, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    if ( presetDeleteOperationState == PRESET_DELETE_OP_WAIT_CACHE ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "Updating Preset List", screenWidth / 2, 105 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( presetDeleteTargetName, screenWidth / 2, 145 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Verifying Deletion...", screenWidth / 2, 175 );

      return;
    }

    if ( presetDeleteOperationState == PRESET_DELETE_OP_SUCCESS ) {
      display.setTextColor( TFT_GREEN, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "DELETED", screenWidth / 2, 104 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( presetDeleteTargetName, screenWidth / 2, 140 );

      char idText[24];

      snprintf( idText, sizeof(idText), "Preset ID: %u", presetDeleteTargetId );

      display.drawString( idText, screenWidth / 2, 165 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Preset cache verified", screenWidth / 2, 192 );

      return;
    }

    if ( presetDeleteOperationState == PRESET_DELETE_OP_FAILED ) {
      display.setTextColor( TFT_RED, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "DELETE FAILED", screenWidth / 2, 108 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "Preset still exists", screenWidth / 2, 150 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.drawString( "Returning to PRESET MANAGE", screenWidth / 2, 180 );
    }
  }

  void drawPresetDeleteScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_PRESET;

    presetSubPage = PRESET_SUBPAGE_DELETE;

    readyScreenShown = true;

    connectingScreenShown = false;

    resetTouchGesture();

    drawStandardPageHeader( "DELETE PRESET", "Select target only", TFT_RED );

    drawPowerButton( bri > 0, false );

    drawBackButton( false );

    if ( presetDeleteOperationState != PRESET_DELETE_OP_IDLE ) {
      drawPresetDeleteOperationStatus();
      return;
    }

    if ( presetDeleteTargetId == 0 || findPresetCacheIndex( presetDeleteTargetId ) < 0 ) {
      display.setTextColor( TFT_RED, TFT_BLACK );

      display.setTextSize( 2 );

      display.drawString( "No Preset", screenWidth / 2, 104 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );

      display.setTextSize( 1 );

      display.drawString( "Nothing can be deleted", screenWidth / 2, 145 );

      return;
    }

    String displayName = presetDeleteTargetName;

    if ( displayName.length() > 28 ) {
      displayName = displayName.substring( 0, 25 ) + "...";
    }

    display.setTextColor( TFT_WHITE, TFT_BLACK );

    if ( displayName.length() <= 12 ) {
      display.setTextSize( 2 );
    }
    else {
      display.setTextSize( 1 );
    }

    display.drawString( displayName, screenWidth / 2, 82 );

    char idText[24];

    snprintf( idText, sizeof(idText), "Preset ID: %u", presetDeleteTargetId );

    display.setTextSize( 1 );

    display.drawString( idText, screenWidth / 2, 105 );

    drawPresetDeleteNavigation( M5STACK_TOUCH_TARGET_NONE );

    display.setTextColor( TFT_RED, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "Delete this Preset", screenWidth / 2, 174 );

    display.setTextColor( TFT_DARKGREY, TFT_BLACK );

    display.drawString( "Current LED state is kept", screenWidth / 2, 187 );

    drawPresetDeleteHoldButton( false );

    lastLedState = bri > 0 ? 1 : 0;
  }

  void drawPresetBootNavigation( M5StackTouchTarget pressedTarget = M5STACK_TOUCH_TARGET_NONE ) {
    display.fillRect( 0, 112, screenWidth, 58, TFT_BLACK );

    drawTriangleButton( CONTROL_LEFT_X, PRESET_BOOT_NAV_BUTTON_Y, false, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV );

    drawTriangleButton( CONTROL_RIGHT_X, PRESET_BOOT_NAV_BUTTON_Y, true, pressedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_NEXT );

    display.setTextDatum( textdatum_t::middle_center );

    display.setTextColor( TFT_CYAN, TFT_BLACK );

    display.setTextSize( 1 );

    display.drawString( "BOOT TARGET", screenWidth / 2, PRESET_BOOT_NAV_BUTTON_Y + (CONTROL_BUTTON_H / 2) );
  }

  bool isPresetBootTargetValid() {
    return presetBootTargetId == 0 || findPresetCacheIndex( presetBootTargetId ) >= 0;
  }

  bool isPresetBootTargetCurrent() {
    return isPresetBootTargetValid() && presetBootTargetId == bootPreset;
  }

  void drawPresetBootHoldButton( bool pressed ) {
    bool enabled = ( presetBootOperationState == PRESET_BOOT_OP_IDLE && presetCacheReady && !presetCacheBuilding && pendingPresetId == 0 && !presetNeedsSaving() && isPresetBootTargetValid() && !isPresetBootTargetCurrent() );

    const char* label;

    if ( isPresetBootTargetCurrent() ) {
      label = ( presetBootTargetId == 0 ) ? "BOOT ALREADY NONE" : "CURRENT BOOT PRESET";
    }
    else {
      label = ( presetBootTargetId == 0 ) ? "HOLD TO CLEAR BOOT" : "HOLD TO SET BOOT";
    }

    drawPresetTextButton( PRESET_BOOT_HOLD_BUTTON_X, PRESET_BOOT_HOLD_BUTTON_Y, PRESET_BOOT_HOLD_BUTTON_W, PRESET_BOOT_HOLD_BUTTON_H, label, enabled, pressed && enabled, 1 );

    touchState.presetBootHoldButtonVisualPressed = ( pressed && enabled );
  }

  void drawPresetBootOperationStatus() {
    if ( currentPage != SCREEN_PRESET || presetSubPage != PRESET_SUBPAGE_BOOT ) {
      return;
    }

    display.fillRect( 0, 60, screenWidth, 180, TFT_BLACK );

    display.setTextDatum( textdatum_t::middle_center );

    if ( presetBootOperationState == PRESET_BOOT_OP_WAIT_CONFIG ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( "Saving Boot Setting", screenWidth / 2, 105 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( presetBootTargetId == 0 ? "NONE" : presetBootTargetName, screenWidth / 2, 145 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
      display.drawString( "Writing WLED Config...", screenWidth / 2, 175 );

      return;
    }

    if ( presetBootOperationState == PRESET_BOOT_OP_SUCCESS ) {
      display.setTextColor( TFT_GREEN, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( presetBootTargetId == 0 ? "BOOT CLEARED" : "BOOT PRESET SET", screenWidth / 2, 104 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( presetBootTargetId == 0 ? "NONE" : presetBootTargetName, screenWidth / 2, 140 );

      if ( presetBootTargetId > 0 ) {
        char idText[24];
        snprintf( idText, sizeof(idText), "Preset ID: %u", presetBootTargetId );
        display.drawString( idText, screenWidth / 2, 165 );
      }
      else {
        display.drawString( "No startup Preset", screenWidth / 2, 165 );
      }

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
      display.drawString( "WLED config saved", screenWidth / 2, 192 );

      return;
    }

    if ( presetBootOperationState == PRESET_BOOT_OP_FAILED ) {
      display.setTextColor( TFT_RED, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( "BOOT SET FAILED", screenWidth / 2, 108 );

      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( "Setting was not verified", screenWidth / 2, 150 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
      display.drawString( "Returning to PRESET MANAGE", screenWidth / 2, 180 );
    }
  }

  void drawPresetBootScreen() {
    display.fillScreen( TFT_BLACK );

    currentPage = SCREEN_PRESET;
    presetSubPage = PRESET_SUBPAGE_BOOT;
    readyScreenShown = true;
    connectingScreenShown = false;

    resetTouchGesture();

    drawStandardPageHeader( "BOOT PRESET", "Startup Preset" );

    drawPowerButton( bri > 0, false );
    drawBackButton( false );

    if ( presetBootOperationState != PRESET_BOOT_OP_IDLE ) {
      drawPresetBootOperationStatus();
      return;
    }

    if ( !presetCacheReady || presetCacheBuilding ) {
      display.setTextColor( TFT_YELLOW, TFT_BLACK );
      display.setTextSize( 2 );
      display.drawString( "Loading Presets", screenWidth / 2, 104 );

      display.setTextColor( TFT_DARKGREY, TFT_BLACK );
      display.setTextSize( 1 );
      display.drawString( "Updating Preset List", screenWidth / 2, 145 );
      return;
    }

    if ( presetBootTargetId > 0 && findPresetCacheIndex( presetBootTargetId ) < 0 ) {
      presetBootTargetId = 0;
      presetBootTargetName = "NONE";
    }

    String displayName = ( presetBootTargetId == 0 ) ? String("NONE") : presetBootTargetName;

    if ( displayName.length() > 28 ) {
      displayName = displayName.substring( 0, 25 ) + "...";
    }

    display.setTextColor( TFT_WHITE, TFT_BLACK );
    display.setTextSize( displayName.length() <= 12 ? 2 : 1 );
    display.drawString( displayName, screenWidth / 2, 82 );

    display.setTextSize( 1 );

    if ( presetBootTargetId == 0 ) {
      display.drawString( "No startup Preset", screenWidth / 2, 105 );
    }
    else {
      char idText[24];
      snprintf( idText, sizeof(idText), "Preset ID: %u", presetBootTargetId );
      display.drawString( idText, screenWidth / 2, 105 );
    }

    drawPresetBootNavigation( M5STACK_TOUCH_TARGET_NONE );

    if ( presetBootTargetId == bootPreset && ( bootPreset == 0 || findPresetCacheIndex( bootPreset ) >= 0 ) ) {
      display.setTextColor( TFT_GREEN, TFT_BLACK );
      display.drawString( bootPreset == 0 ? "Boot Preset Disabled" : "Current Boot Preset", screenWidth / 2, 174 );
    }
    else if ( bootPreset > 0 && findPresetCacheIndex( bootPreset ) < 0 && presetBootTargetId == 0 ) {
      char missingText[40];
      snprintf( missingText, sizeof(missingText), "Configured Boot ID %u missing", bootPreset );
      display.setTextColor( TFT_RED, TFT_BLACK );
      display.drawString( missingText, screenWidth / 2, 174 );
    }
    else {
      display.setTextColor( TFT_WHITE, TFT_BLACK );
      display.drawString( "Set this at startup", screenWidth / 2, 174 );
    }

    display.setTextColor( TFT_DARKGREY, TFT_BLACK );
    display.drawString( "Current LED state is kept", screenWidth / 2, 187 );

    drawPresetBootHoldButton( false );

    lastBootPresetValue = bootPreset;
    lastLedState = bri > 0 ? 1 : 0;
  }


  void beginHueEdit() {
    uint32_t currentColor = getSelectedColor();

    if ( !logicalColorHsvValid || !lastSelectedColorValid || currentColor != lastSelectedColor ) {
      syncLogicalColorFromRgb( currentColor );

      lastSelectedColor = currentColor;

      lastSelectedColorValid = true;
    }

    hueEditHsv = logicalColorHsv;

    // A true black WLED color converts to HSV with V=0. CoreS3 intentionally
    // has no per-color Value control, so changing Hue alone could never leave
    // black. Once the user starts editing a black slot, bootstrap the edit
    // state to a visible fully-saturated color. The stored WLED color is not
    // changed until the first actual Hue step is applied.
    if ( currentColor == 0 ) {
      hueEditHsv.s = 255;
      hueEditHsv.v = 255;
    }

    hueEditValue = logicalHueValue;

    hueEditWhite = logicalWhiteValue;

    hueEditValid = true;
  }

  void beginSaturationEdit() {
    uint32_t currentColor = getSelectedColor();

    if ( !logicalColorHsvValid || !lastSelectedColorValid || currentColor != lastSelectedColor ) {
      syncLogicalColorFromRgb( currentColor );

      lastSelectedColor = currentColor;

      lastSelectedColorValid = true;
    }

    saturationEditHsv = logicalColorHsv;

    // Saturation also cannot make a true black color visible while HSV V is
    // zero. Bootstrap only the edit Value; the requested Saturation remains
    // under direct user control.
    if ( currentColor == 0 ) {
      saturationEditHsv.v = 255;
    }

    saturationEditValue = logicalSaturationValue;

    saturationEditWhite = logicalWhiteValue;

    saturationEditValid = true;
  }

  void resetTouchGesture() {
    touchState.touchActive = false;

    touchState.touchTarget = M5STACK_TOUCH_TARGET_NONE;

    touchState.lastTouchInsidePower = false;

    touchState.lastTouchInsideWiFiRecovery = false;

    touchState.lastTouchInsideBrightness = false;

    touchState.lastTouchInsideEffect = false;

    touchState.lastTouchInsideEffectDetail = false;

    touchState.lastTouchInsideColor = false;

    touchState.lastTouchInsidePresetOpen = false;

    touchState.lastTouchInsideBack = false;

    touchState.lastTouchInsideColorSlot = false;

    touchState.lastTouchInsideHue = false;

    touchState.lastTouchInsideSaturation = false;

    touchState.lastTouchInsideSpeed = false;

    touchState.lastTouchInsideIntensity = false;

    touchState.lastTouchInsidePalette = false;

    touchState.lastTouchInsidePresetNav = false;

    touchState.lastTouchInsidePresetManage = false;

    touchState.lastTouchInsidePresetSaveNew = false;

    touchState.lastTouchInsidePresetSaveHold = false;

    touchState.lastTouchInsidePresetOverwriteOpen = false;

    touchState.lastTouchInsidePresetOverwriteNav = false;

    touchState.lastTouchInsidePresetOverwriteHold = false;

    touchState.lastTouchInsidePresetDeleteOpen = false;

    touchState.lastTouchInsidePresetDeleteNav = false;

    touchState.lastTouchInsidePresetDeleteHold = false;

    touchState.lastTouchInsidePresetBootOpen = false;
    touchState.lastTouchInsidePresetBootNav = false;
    touchState.lastTouchInsidePresetBootHold = false;

    touchState.powerButtonVisualPressed = false;

    touchState.wifiRecoveryVisualPressed = false;

    touchState.brightnessButtonVisualPressed = false;

    touchState.effectButtonVisualPressed = false;

    touchState.effectDetailVisualPressed = false;

    touchState.colorButtonVisualPressed = false;

    touchState.presetOpenButtonVisualPressed = false;

    touchState.backButtonVisualPressed = false;

    touchState.hueButtonVisualPressed = false;

    touchState.saturationButtonVisualPressed = false;

    touchState.speedButtonVisualPressed = false;

    touchState.intensityButtonVisualPressed = false;

    touchState.paletteButtonVisualPressed = false;

    touchState.presetNavButtonVisualPressed = false;

    touchState.presetManageButtonVisualPressed = false;

    touchState.presetSaveNewButtonVisualPressed = false;

    touchState.presetSaveHoldButtonVisualPressed = false;

    touchState.presetOverwriteOpenButtonVisualPressed = false;

    touchState.presetOverwriteNavButtonVisualPressed = false;

    touchState.presetOverwriteHoldButtonVisualPressed = false;

    touchState.presetDeleteOpenButtonVisualPressed = false;

    touchState.presetDeleteNavButtonVisualPressed = false;

    touchState.presetDeleteHoldButtonVisualPressed = false;

    touchState.presetBootOpenButtonVisualPressed = false;
    touchState.presetBootNavButtonVisualPressed = false;
    touchState.presetBootHoldButtonVisualPressed = false;

    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.wifiRecoveryHoldState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.brightnessRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.effectRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.colorSlotHoldState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.hueRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.saturationRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.speedRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.intensityRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.paletteRepeatState );
    M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.presetRepeatState );

    hueEditValid = false;

    saturationEditValid = false;

    touchState.touchReleaseCandidate = 0;

    presetSaveHoldStartTime = 0;

    presetSaveHoldTriggered = false;

    touchState.lastTouchX = -1;

    touchState.lastTouchY = -1;
  }

  void toggleLedPowerFromTouch() {
    toggleOnOff();

    stateUpdated( CALL_MODE_BUTTON );

    lastLedState = -1;

    lastBrightnessValue = -1;
  }

  bool applyBrightnessValue( int newValue ) {
    newValue = constrain( newValue, 0, 255 );

    if ( newValue == bri ) {
      return false;
    }

    if ( newValue == 0 ) {
      if ( bri > 0 ) {
        briLast = bri;

        bri = 0;
      }
    }
    else {
      if ( bri == 0 ) {
        strip.restartRuntime();
      }

      bri = (uint8_t)newValue;
    }

    stateUpdated( CALL_MODE_BUTTON );

    lastLedState = -1;

    return true;
  }

  void applyBrightnessStep( int step ) {
    int newValue = constrain( (int)bri + step, 0, 255 );

    if ( applyBrightnessValue( newValue ) ) {
      if ( currentPage == SCREEN_MAIN ) {
        drawBrightness( bri, touchState.touchTarget );
      }

      lastBrightnessValue = bri;
    }
  }

  void brightnessShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN ) {
      applyBrightnessStep( -BRI_SHORT_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_BRIGHTNESS_UP ) {
      applyBrightnessStep( BRI_SHORT_STEP );
    }
  }

  void brightnessLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN ) {
      applyBrightnessStep( -BRI_LONG_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_BRIGHTNESS_UP ) {
      applyBrightnessStep( BRI_LONG_STEP );
    }
  }

  void applyEffectStep( int step ) {
    uint8_t modeCount = strip.getModeCount();

    if ( modeCount == 0 || step == 0 ) {
      return;
    }

    Segment& mainSegment = strip.getMainSegment();

    const int direction = ( step < 0 ) ? -1 : 1;

    int newMode = mainSegment.mode;

    bool validModeFound = false;

    for ( uint16_t attempt = 0; attempt < modeCount; attempt++ ) {
      newMode += direction;

      if ( newMode < 0 ) {
        newMode = modeCount - 1;
      }

      if ( newMode >= modeCount ) {
        newMode = 0;
      }

      const char* modeData = strip.getModeData( (uint8_t)newMode );

      if ( modeData != nullptr && strncmp_P( "RSVD", modeData, 4 ) != 0 ) {
        validModeFound = true;
        break;
      }
    }

    if ( !validModeFound || newMode == mainSegment.mode ) {
      return;
    }

    mainSegment.setMode( (uint8_t)newMode );

    stateUpdated( CALL_MODE_BUTTON );

    if ( currentPage == SCREEN_MAIN ) {
      drawEffect( mainSegment.mode, touchState.touchTarget );

      drawColorButton( getPrimaryColor(), false );
    }

    lastEffectMode = mainSegment.mode;

    lastSpeedValue = mainSegment.speed;

    lastIntensityValue = mainSegment.intensity;

    lastPaletteValue = mainSegment.palette;
  }

  void effectLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_EFFECT_PREV ) {
      applyEffectStep( -1 );
    }
    else if ( target == M5STACK_TOUCH_TARGET_EFFECT_NEXT ) {
      applyEffectStep( 1 );
    }
  }

  bool applySpeedValue( int newValue ) {
    if ( strip.getSegmentsNum() == 0 ) {
      return false;
    }

    newValue = constrain( newValue, 0, 255 );

    Segment& mainSegment = strip.getMainSegment();

    if ( newValue == mainSegment.speed ) {
      return false;
    }

    mainSegment.speed = (uint8_t)newValue;

    stateUpdated( CALL_MODE_BUTTON );

    if ( currentPage == SCREEN_EFFECT ) {
      drawSpeed( mainSegment.speed, touchState.touchTarget );
    }

    lastSpeedValue = mainSegment.speed;

    return true;
  }

  void applySpeedStep( int step ) {
    int newValue = constrain( (int)getCurrentSpeed() + step, 0, 255 );

    applySpeedValue( newValue );
  }

  void speedShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_SPEED_DOWN ) {
      applySpeedStep( -SPEED_SHORT_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_SPEED_UP ) {
      applySpeedStep( SPEED_SHORT_STEP );
    }
  }

  void speedLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_SPEED_DOWN ) {
      applySpeedStep( -SPEED_LONG_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_SPEED_UP ) {
      applySpeedStep( SPEED_LONG_STEP );
    }
  }

  bool applyIntensityValue( int newValue ) {
    if ( strip.getSegmentsNum() == 0 ) {
      return false;
    }

    newValue = constrain( newValue, 0, 255 );

    Segment& mainSegment = strip.getMainSegment();

    if ( newValue == mainSegment.intensity ) {
      return false;
    }

    mainSegment.intensity = (uint8_t)newValue;

    stateUpdated( CALL_MODE_BUTTON );

    if ( currentPage == SCREEN_EFFECT ) {
      drawIntensity( mainSegment.intensity, touchState.touchTarget );
    }

    lastIntensityValue = mainSegment.intensity;

    return true;
  }

  void applyIntensityStep( int step ) {
    int newValue = constrain( (int)getCurrentIntensity() + step, 0, 255 );

    applyIntensityValue( newValue );
  }

  void intensityShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_INTENSITY_DOWN ) {
      applyIntensityStep( -INTENSITY_SHORT_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_INTENSITY_UP ) {
      applyIntensityStep( INTENSITY_SHORT_STEP );
    }
  }

  void intensityLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_INTENSITY_DOWN ) {
      applyIntensityStep( -INTENSITY_LONG_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_INTENSITY_UP ) {
      applyIntensityStep( INTENSITY_LONG_STEP );
    }
  }

  bool applyPaletteValue( uint8_t newPalette ) {
    if ( strip.getSegmentsNum() == 0 ) {
      return false;
    }

    Segment& mainSegment = strip.getMainSegment();

    if ( newPalette == mainSegment.palette ) {
      return false;
    }

    mainSegment.setPalette( newPalette );

    stateUpdated( CALL_MODE_BUTTON );

    if ( currentPage == SCREEN_EFFECT ) {
      drawPalette( mainSegment.palette, touchState.touchTarget );
    }

    lastPaletteValue = mainSegment.palette;

    return true;
  }

  void applyPaletteStep( int step ) {
    size_t paletteCount = getSelectablePaletteCount();

    if ( paletteCount == 0 ) {
      return;
    }

    uint8_t currentPalette = getCurrentPalette();

    int currentIndex = findPaletteSequenceIndex( currentPalette );

    if ( currentIndex < 0 ) {
      currentIndex = 0;
    }

    int newIndex = currentIndex + step;

    while ( newIndex < 0 ) {
      newIndex += (int)paletteCount;
    }

    while ( newIndex >= (int)paletteCount ) {
      newIndex -= (int)paletteCount;
    }

    uint8_t newPalette = paletteIdFromSequenceIndex( (size_t)newIndex );

    applyPaletteValue( newPalette );
  }

  void paletteShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_PALETTE_PREV ) {
      applyPaletteStep( -1 );
    }
    else if ( target == M5STACK_TOUCH_TARGET_PALETTE_NEXT ) {
      applyPaletteStep( 1 );
    }
  }

  void paletteLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_PALETTE_PREV ) {
      applyPaletteStep( -1 );
    }
    else if ( target == M5STACK_TOUCH_TARGET_PALETTE_NEXT ) {
      applyPaletteStep( 1 );
    }
  }

  bool applyPresetStep( int direction ) {
    if ( direction == 0 ) {
      return false;
    }

    if ( !presetCacheReady ) {
      if ( currentPage == SCREEN_PRESET ) {
        drawPresetDetails( getDisplayedPresetId(), false );

        drawPresetNavigation( M5STACK_TOUCH_TARGET_NONE );
      }

      Serial.println( F( "[CoreS3_Display] " "Preset cache not ready" ) );

      return false;
    }

    uint8_t basePreset = getPresetNavigationBaseId();

    String presetName;

    uint8_t newPreset = findAdjacentPreset( basePreset, direction, &presetName );

    if ( newPreset == 0 ) {
      presetNoEntries = true;

      pendingPresetId = 0;

      pendingPresetName = "";

      pendingPresetRequestMs = 0;

      if ( currentPage == SCREEN_PRESET ) {
        drawPresetDetails( 0, false );

        drawPresetNavigation( touchState.touchTarget );
      }

      lastPresetValue = 0;

      return false;
    }

    presetNoEntries = false;

    // Remember the CoreS3 navigation position immediately. This keeps the
    // selected Preset as the UI starting point even if WLED later enters
    // Custom State after a Color/Effect adjustment.
    presetNavigationCursorId = newPreset;

    pendingPresetId = newPreset;

    pendingPresetName = presetName;

    pendingPresetRequestMs = millis();

    applyPreset( newPreset, CALL_MODE_BUTTON_PRESET );

    if ( currentPage == SCREEN_PRESET ) {
      drawPresetDetails( newPreset, true );

      drawPresetNavigation( touchState.touchTarget );
    }

    lastPresetValue = newPreset;

    Serial.printf( "[CoreS3_Display] " "Preset cache request: %u (%s)\n", newPreset, presetName.c_str() );

    return true;
  }

  void presetShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_PRESET_PREV ) {
      applyPresetStep( -1 );
    }
    else if ( target == M5STACK_TOUCH_TARGET_PRESET_NEXT ) {
      applyPresetStep( 1 );
    }
  }

  void presetLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_PRESET_PREV ) {
      applyPresetStep( -1 );
    }
    else if ( target == M5STACK_TOUCH_TARGET_PRESET_NEXT ) {
      applyPresetStep( 1 );
    }
  }

  bool applyHueValue( uint8_t newHue ) {
    if ( strip.getSegmentsNum() == 0 ) {
      return false;
    }

    if (!hueEditValid) {
      beginHueEdit();
    }

    if (!hueEditValid) {
      return false;
    }

    hueEditValue = newHue;

    hueEditHsv.h = ((uint16_t)newHue) << 8;

    logicalHueValue = newHue;

    logicalColorHsv = hueEditHsv;

    logicalSaturationValue = logicalColorHsv.s;

    logicalWhiteValue = hueEditWhite;

    logicalColorHsvValid = true;

    CRGBW newRgb;

    hsv2rgb_spectrum( logicalColorHsv, newRgb );

    newRgb.w = logicalWhiteValue;

    uint32_t newColor = newRgb.color32;

    Segment& mainSegment = strip.getMainSegment();

    if ( newColor != mainSegment.colors[ selectedColorSlot ] ) {
      mainSegment.setColor( selectedColorSlot, newColor );

      stateUpdated( CALL_MODE_BUTTON );
    }

    if ( currentPage == SCREEN_COLOR ) {
      drawColorDetails( newColor );

      drawHue( logicalHueValue, touchState.touchTarget );

      drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );
    }

    lastSelectedColor = newColor;
    lastSelectedColorValid = true;

    if ( selectedColorSlot == 0 ) {
      lastPrimaryColor = newColor;
      lastPrimaryColorValid = true;
    }

    cacheCurrentColorSlots();

    lastHueValue = logicalHueValue;

    lastSaturationValue = logicalSaturationValue;

    return true;
  }

  void applyHueStep( int step ) {
    if (!hueEditValid) {
      beginHueEdit();
    }

    if (!hueEditValid) {
      return;
    }

    int newValue = (int)logicalHueValue + step;

    while ( newValue < 0 ) {
      newValue += 256;
    }

    while ( newValue > 255 ) {
      newValue -= 256;
    }

    applyHueValue( (uint8_t)newValue );
  }

  void hueShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_HUE_DOWN ) {
      applyHueStep( -HUE_SHORT_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_HUE_UP ) {
      applyHueStep( HUE_SHORT_STEP );
    }
  }

  void hueLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_HUE_DOWN ) {
      applyHueStep( -HUE_LONG_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_HUE_UP ) {
      applyHueStep( HUE_LONG_STEP );
    }
  }

  bool applySaturationValue( uint8_t newSaturation ) {
    if ( strip.getSegmentsNum() == 0 ) {
      return false;
    }

    if (!saturationEditValid) {
      beginSaturationEdit();
    }

    if (!saturationEditValid) {
      return false;
    }

    saturationEditValue = newSaturation;

    saturationEditHsv.s = newSaturation;

    logicalSaturationValue = newSaturation;

    logicalColorHsv = saturationEditHsv;

    logicalHueValue = (uint8_t)( logicalColorHsv.h >> 8 );

    logicalWhiteValue = saturationEditWhite;

    logicalColorHsvValid = true;

    CRGBW newRgb;

    hsv2rgb_spectrum( logicalColorHsv, newRgb );

    newRgb.w = logicalWhiteValue;

    uint32_t newColor = newRgb.color32;

    Segment& mainSegment = strip.getMainSegment();

    if ( newColor != mainSegment.colors[ selectedColorSlot ] ) {
      mainSegment.setColor( selectedColorSlot, newColor );

      stateUpdated( CALL_MODE_BUTTON );
    }

    if ( currentPage == SCREEN_COLOR ) {
      drawColorDetails( newColor );

      drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

      drawSaturation( logicalSaturationValue, touchState.touchTarget );
    }

    lastSelectedColor = newColor;
    lastSelectedColorValid = true;

    if ( selectedColorSlot == 0 ) {
      lastPrimaryColor = newColor;
      lastPrimaryColorValid = true;
    }

    cacheCurrentColorSlots();

    lastHueValue = logicalHueValue;

    lastSaturationValue = logicalSaturationValue;

    return true;
  }

  void applySaturationStep( int step ) {
    if (!saturationEditValid) {
      beginSaturationEdit();
    }

    if (!saturationEditValid) {
      return;
    }

    int newValue = constrain( (int)logicalSaturationValue + step, 0, 255 );

    if ( newValue == logicalSaturationValue ) {
      return;
    }

    applySaturationValue( (uint8_t)newValue );
  }

  void saturationShortPress( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_SATURATION_DOWN ) {
      applySaturationStep( -SATURATION_SHORT_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_SATURATION_UP ) {
      applySaturationStep( SATURATION_SHORT_STEP );
    }
  }

  void saturationLongPressStep( M5StackTouchTarget target ) {
    if ( target == M5STACK_TOUCH_TARGET_SATURATION_DOWN ) {
      applySaturationStep( -SATURATION_LONG_STEP );
    }
    else if ( target == M5STACK_TOUCH_TARGET_SATURATION_UP ) {
      applySaturationStep( SATURATION_LONG_STEP );
    }
  }

  #include "M5StackDisplayTouchStateMachine.inc"

  void serviceAudioHealthPresentation( uint8_t effectMode ) {
    const bool audioUnavailable = isCoreS3AudioUnavailable();

    if ( audioUnavailable == lastAudioUnavailable ) {
      return;
    }

    const bool relevantVisiblePage =
      currentPage == SCREEN_MAIN || currentPage == SCREEN_EFFECT;

    const bool currentEffectUsesAudio =
      effectUsesAudioReactive( getEffectCapabilities( effectMode ) );

    // Avoid changing effect visuals in the middle of a touch gesture. Keep the
    // previous cached state so the change is retried after release.
    if (
      relevantVisiblePage &&
      currentEffectUsesAudio &&
      touchState.touchTarget != M5STACK_TOUCH_TARGET_NONE
    ) {
      return;
    }

    lastAudioUnavailable = audioUnavailable;

    if ( !currentEffectUsesAudio ) {
      return;
    }

    if ( currentPage == SCREEN_MAIN ) {
      drawEffect( effectMode, M5STACK_TOUCH_TARGET_NONE );
      return;
    }

    if ( currentPage == SCREEN_EFFECT ) {
      drawEffectPageName( effectMode );
    }
  }

  // =========================================================
  // Page-specific runtime display synchronization
  //
  // These helpers contain only UI/WLED state synchronization.
  // Hardware-specific display/touch access remains outside this layer
  // so the same page logic can be reused for Core2 variants later.
  // =========================================================

  bool updateMainPageState( uint8_t effectMode, uint8_t currentSpeed, uint8_t currentIntensity, uint8_t currentPalette, uint32_t primaryColor, bool primaryColorChanged ) {
    bool brightnessTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN || touchState.touchTarget == M5STACK_TOUCH_TARGET_BRIGHTNESS_UP );

    if ( !brightnessTouchActive && (int)bri != lastBrightnessValue ) {
      drawBrightness( bri, M5STACK_TOUCH_TARGET_NONE );

      lastBrightnessValue = bri;
    }

    bool effectTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_EFFECT_PREV || touchState.touchTarget == M5STACK_TOUCH_TARGET_EFFECT_DETAIL || touchState.touchTarget == M5STACK_TOUCH_TARGET_EFFECT_NEXT );

    if ( !effectTouchActive && (int)effectMode != lastEffectMode ) {
      drawEffect( effectMode, M5STACK_TOUCH_TARGET_NONE );

      drawColorButton( primaryColor, false );

      lastEffectMode = effectMode;

      lastSpeedValue = currentSpeed;

      lastIntensityValue = currentIntensity;

      lastPaletteValue = currentPalette;
    }

    if ( (int)currentPalette != lastPaletteValue ) {
      lastPaletteValue = currentPalette;
    }

    if ( (int)currentPreset != lastPresetValue ) {
      lastPresetValue = currentPreset;
    }

    if ( primaryColorChanged && touchState.touchTarget != M5STACK_TOUCH_TARGET_COLOR_OPEN ) {
      syncLogicalColorFromRgb( primaryColor );

      drawColorButton( primaryColor, false );

      return true;
    }

    return false;
  }

  bool updateColorPageState(
    uint8_t effectMode,
    uint32_t primaryColor,
    bool primaryColorChanged,
    bool colorControlTouchActive
  ) {
    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE && (int)effectMode != lastEffectMode ) {
      drawColorScreen();

      return true;
    }

    const M5StackEffectColorCapabilities capability =
      getEffectColorCapabilities( effectMode );

    if ( !effectUsesAnyColor( capability ) ) {
      if ( currentColorSlotsChanged() ) {
        cacheCurrentColorSlots();
      }

      return primaryColorChanged;
    }

    normalizeSelectedColorSlot( effectMode );

    if ( currentColorSlotsChanged() && !colorControlTouchActive ) {
      const uint32_t selectedColor = getSelectedColor();

      syncLogicalColorFromRgb( selectedColor );

      lastSelectedColor = selectedColor;
      lastSelectedColorValid = true;

      drawColorDetails( selectedColor );

      drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

      drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );

      cacheCurrentColorSlots();

      lastHueValue = logicalHueValue;

      lastSaturationValue = logicalSaturationValue;

      return primaryColorChanged;
    }

    return false;
  }

  bool updateEffectPageState( uint8_t effectMode, uint8_t currentSpeed, uint8_t currentIntensity, uint8_t currentPalette ) {
    bool speedTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_SPEED_DOWN || touchState.touchTarget == M5STACK_TOUCH_TARGET_SPEED_UP );

    bool intensityTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_INTENSITY_DOWN || touchState.touchTarget == M5STACK_TOUCH_TARGET_INTENSITY_UP );

    bool paletteTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PALETTE_PREV || touchState.touchTarget == M5STACK_TOUCH_TARGET_PALETTE_NEXT );

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE && (int)effectMode != lastEffectMode ) {
      drawEffectDetailScreen();

      return true;
    }

    if ( !speedTouchActive && (int)currentSpeed != lastSpeedValue ) {
      drawSpeed( currentSpeed, M5STACK_TOUCH_TARGET_NONE );

      lastSpeedValue = currentSpeed;
    }

    if ( !intensityTouchActive && (int)currentIntensity != lastIntensityValue ) {
      drawIntensity( currentIntensity, M5STACK_TOUCH_TARGET_NONE );

      lastIntensityValue = currentIntensity;
    }

    if ( !paletteTouchActive && (int)currentPalette != lastPaletteValue ) {
      drawPalette( currentPalette, M5STACK_TOUCH_TARGET_NONE );

      lastPaletteValue = currentPalette;
    }

    return false;
  }

  void updatePresetPageState( uint8_t displayedPreset, bool presetPendingSettled ) {
    if ( presetSubPage == PRESET_SUBPAGE_MANAGE ) {
      const bool presetFileChanged =
        ( presetsModifiedTime != lastPresetsModifiedTime );

      const bool activePresetChanged =
        ( (int)currentPreset != lastPresetValue );

      if (
        touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE &&
        ( activePresetChanged || presetFileChanged || presetPendingSettled )
      ) {
        drawPresetManageScreen();
      }

      return;
    }

    if ( presetSubPage == PRESET_SUBPAGE_NAV ) {
      bool presetTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_PREV || touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_NEXT );

      bool presetFileChanged = ( presetsModifiedTime != lastPresetsModifiedTime );

      if (presetFileChanged) {
        lastPresetsModifiedTime = presetsModifiedTime;

        presetNoEntries = false;
      }

      if ( !presetTouchActive && ( (int)displayedPreset != lastPresetValue || presetPendingSettled || presetFileChanged ) ) {
        drawPresetDetails( displayedPreset, pendingPresetId > 0 );

        drawPresetNavigation( M5STACK_TOUCH_TARGET_NONE );

        lastPresetValue = displayedPreset;
      }

      return;
    }

    if ( presetSubPage == PRESET_SUBPAGE_BOOT ) {
      if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE && (int)bootPreset != lastBootPresetValue ) {
        drawPresetBootScreen();
        lastBootPresetValue = bootPreset;
      }
    }
  }

  static void writeBmp16( uint8_t* destination, uint16_t value ) {
    destination[0] = (uint8_t)( value & 0xFF );
    destination[1] = (uint8_t)( ( value >> 8 ) & 0xFF );
  }

  static void writeBmp32( uint8_t* destination, uint32_t value ) {
    destination[0] = (uint8_t)( value & 0xFF );
    destination[1] = (uint8_t)( ( value >> 8 ) & 0xFF );
    destination[2] = (uint8_t)( ( value >> 16 ) & 0xFF );
    destination[3] = (uint8_t)( ( value >> 24 ) & 0xFF );
  }

  bool buildScreenshotBmp(
    uint8_t*& bmpData,
    size_t& bmpSize
  ) {
    bmpData = nullptr;
    bmpSize = 0;

    if ( !displayReady || screenWidth <= 0 || screenHeight <= 0 ) {
      return false;
    }

    const size_t width = (size_t)screenWidth;
    const size_t height = (size_t)screenHeight;
    const size_t pixelCount = width * height;
    const size_t frameBytes = pixelCount * sizeof(uint16_t);

    // 24-bit BMP rows are aligned to 4-byte boundaries.
    const size_t rowBytes = width * 3;
    const size_t rowStride = ( rowBytes + 3 ) & ~((size_t)3);
    const size_t imageBytes = rowStride * height;
    const size_t totalBytes = SCREENSHOT_BMP_HEADER_SIZE + imageBytes;

    uint16_t* frameBuffer = static_cast<uint16_t*>(
      heap_caps_malloc(
        frameBytes,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
      )
    );

    if ( frameBuffer == nullptr ) {
      return false;
    }

    uint8_t* outputBuffer = static_cast<uint8_t*>(
      heap_caps_malloc(
        totalBytes,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
      )
    );

    if ( outputBuffer == nullptr ) {
      heap_caps_free( frameBuffer );
      return false;
    }

    if (
      !hardwareBackend.readDisplayRgb565(
        frameBuffer,
        screenWidth,
        screenHeight
      )
    ) {
      heap_caps_free( outputBuffer );
      heap_caps_free( frameBuffer );

      return false;
    }

    memset( outputBuffer, 0, totalBytes );

    // -------------------------------------------------------
    // BITMAPFILEHEADER (14 bytes)
    // -------------------------------------------------------

    outputBuffer[0] = 'B';
    outputBuffer[1] = 'M';

    writeBmp32( outputBuffer + 2, (uint32_t)totalBytes );
    writeBmp32( outputBuffer + 10, (uint32_t)SCREENSHOT_BMP_HEADER_SIZE );

    // -------------------------------------------------------
    // BITMAPINFOHEADER (40 bytes)
    //
    // Negative height selects top-down row order. This lets us
    // preserve the LCD's natural 0..height-1 scan order without
    // reversing the captured frame.
    // -------------------------------------------------------

    writeBmp32( outputBuffer + 14, 40 );
    writeBmp32( outputBuffer + 18, (uint32_t)screenWidth );
    writeBmp32(
      outputBuffer + 22,
      (uint32_t)(int32_t)( -screenHeight )
    );

    writeBmp16( outputBuffer + 26, 1 );
    writeBmp16( outputBuffer + 28, 24 );

    writeBmp32( outputBuffer + 30, 0 );
    writeBmp32( outputBuffer + 34, (uint32_t)imageBytes );

    // 72 DPI, expressed as pixels per metre.
    writeBmp32( outputBuffer + 38, 2835 );
    writeBmp32( outputBuffer + 42, 2835 );

    for ( size_t y = 0; y < height; y++ ) {
      const uint16_t* sourceRow = frameBuffer + ( y * width );
      uint8_t* destinationRow =
        outputBuffer + SCREENSHOT_BMP_HEADER_SIZE + ( y * rowStride );

      for ( size_t x = 0; x < width; x++ ) {
        const uint16_t rawPixel = sourceRow[x];

        // M5GFX readRect() returns the CoreS3 panel RGB565 word with
        // the two bytes swapped relative to the logical TFT color value.
        //
        // Example:
        //   TFT_CYAN = 0x07FF
        //   readRect  = 0xFF07
        //
        // Restore the logical RGB565 word before expanding to RGB888.
        const uint16_t pixel = (uint16_t)(
          ( rawPixel >> 8 ) |
          ( rawPixel << 8 )
        );

        const uint8_t red5 = (uint8_t)( ( pixel >> 11 ) & 0x1F );
        const uint8_t green6 = (uint8_t)( ( pixel >> 5 ) & 0x3F );
        const uint8_t blue5 = (uint8_t)( pixel & 0x1F );

        const uint8_t red8 = (uint8_t)( ( red5 << 3 ) | ( red5 >> 2 ) );
        const uint8_t green8 = (uint8_t)( ( green6 << 2 ) | ( green6 >> 4 ) );
        const uint8_t blue8 = (uint8_t)( ( blue5 << 3 ) | ( blue5 >> 2 ) );

        uint8_t* destinationPixel = destinationRow + ( x * 3 );

        // BMP 24-bit pixel byte order is B, G, R.
        destinationPixel[0] = blue8;
        destinationPixel[1] = green8;
        destinationPixel[2] = red8;
      }
    }

    heap_caps_free( frameBuffer );

    bmpData = outputBuffer;
    bmpSize = totalBytes;

    return true;
  }

  void handleScreenshotRequest( AsyncWebServerRequest* request ) {
    if ( request == nullptr ) {
      return;
    }

    if (
      isCore2DiagnosticOnlyMode() ||
      !displayReady ||
      screenWidth <= 0 ||
      screenHeight <= 0
    ) {
      request->send(
        503,
        F("text/plain"),
        F("CoreS3 display screenshot is unavailable.\n")
      );

      return;
    }

    // Keep screenshot memory use bounded and avoid overlapping LCD reads.
    if ( screenshotCaptureInProgress ) {
      request->send(
        429,
        F("text/plain"),
        F("A CoreS3 screenshot request is already in progress.\n")
      );

      return;
    }

    screenshotCaptureInProgress = true;

    uint8_t* bmpData = nullptr;
    size_t bmpSize = 0;

    if ( !buildScreenshotBmp( bmpData, bmpSize ) ) {
      screenshotCaptureInProgress = false;

      Serial.println(
        F( "[CoreS3_Display] Screenshot: capture/buffer allocation failed" )
      );

      request->send(
        503,
        F("text/plain"),
        F("CoreS3 screenshot capture failed. PSRAM may be unavailable.\n")
      );

      return;
    }

    // The async response outlives this request handler. Keep the BMP buffer
    // alive with shared ownership until the response itself is destroyed.
    std::shared_ptr<uint8_t> bmpOwner(
      bmpData,
      [this]( uint8_t* pointer ) {
        if ( pointer != nullptr ) {
          heap_caps_free( pointer );
        }

        screenshotCaptureInProgress = false;
      }
    );

    const size_t responseSize = bmpSize;

    AsyncWebServerResponse* response = request->beginResponse(
      F("image/bmp"),
      responseSize,
      [bmpOwner, responseSize](
        uint8_t* buffer,
        size_t maxLength,
        size_t index
      ) -> size_t {
        if ( buffer == nullptr || index >= responseSize ) {
          return 0;
        }

        const size_t remaining = responseSize - index;
        const size_t copyLength =
          ( maxLength < remaining ) ? maxLength : remaining;

        memcpy(
          buffer,
          bmpOwner.get() + index,
          copyLength
        );

        return copyLength;
      }
    );

    if ( response == nullptr ) {
      screenshotCaptureInProgress = false;

      request->send(
        503,
        F("text/plain"),
        F("CoreS3 screenshot response could not be created.\n")
      );

      return;
    }

    response->addHeader(
      F("Cache-Control"),
      F("no-store, no-cache, must-revalidate, max-age=0")
    );

    response->addHeader( F("Pragma"), F("no-cache") );
    response->addHeader( F("Expires"), F("0") );

    response->addHeader(
      F("Content-Disposition"),
      F("inline; filename=\"cores3-screen.bmp\"")
    );

    request->send( response );

    Serial.printf(
      "[CoreS3_Display] Screenshot: %d x %d BMP (%u bytes)\n",
      screenWidth,
      screenHeight,
      (unsigned)responseSize
    );
  }

  void registerScreenshotEndpoint() {
    server.on(
      F("/cores3/screenshot.bmp"),
      HTTP_GET,
      [this]( AsyncWebServerRequest* request ) {
        handleScreenshotRequest( request );
      }
    );

    Serial.println(
      F( "[CoreS3_Display] Screenshot endpoint: /cores3/screenshot.bmp" )
    );
  }

  public:

  CoreS3DisplayUsermod()
    : hardwareBackend( display ) {
  }

  void addToConfig( JsonObject& root ) override {
    JsonObject top = root.createNestedObject( FPSTR( CORES3_DISPLAY_CONFIG_NAME ) );

    top[ F("sleep-timeout") ] = sleepTimeoutSec;

    top[ F("lcd-brightness") ] = lcdBrightness;

    top[ F("fade") ] = fadeEnabled;

    top[ F("fade-duration") ] = fadeDurationMs;
  }

  bool readFromConfig( JsonObject& root ) override {
    JsonObject top = root[ FPSTR( CORES3_DISPLAY_CONFIG_NAME ) ];

    if ( top.isNull() ) {
      Serial.println( F( "[CoreS3_Display] " "No display config found. Using defaults." ) );

      return false;
    }

    bool configComplete = true;

    int newSleepTimeout = sleepTimeoutSec;

    int newLcdBrightness = lcdBrightness;

    bool newFadeEnabled = fadeEnabled;

    int newFadeDuration = fadeDurationMs;

    configComplete &= getJsonValue( top[ F("sleep-timeout") ], newSleepTimeout, 30 );

    configComplete &= getJsonValue( top[ F("lcd-brightness") ], newLcdBrightness, 128 );

    configComplete &= getJsonValue( top[ F("fade") ], newFadeEnabled, true );

    configComplete &= getJsonValue( top[ F("fade-duration") ], newFadeDuration, 250 );

    newSleepTimeout = constrain( newSleepTimeout, 0, 3600 );

    newLcdBrightness = constrain( newLcdBrightness, 1, 255 );

    newFadeDuration = constrain( newFadeDuration, 50, 2000 );

    sleepTimeoutSec = (uint16_t)newSleepTimeout;

    lcdBrightness = (uint16_t)newLcdBrightness;

    fadeEnabled = newFadeEnabled;

    fadeDurationMs = (uint16_t)newFadeDuration;

    if ( initDone ) {
      Serial.printf(
        "[CoreS3_Display] "
        "Config updated: "
        "Sleep=%u sec, "
        "LCD=%u, "
        "Fade=%s, "
        "FadeDuration=%u ms\n",
        sleepTimeoutSec,
        lcdBrightness,
        fadeEnabled ? "ON" : "OFF",
        fadeDurationMs
      );
    }

    if ( initDone && displayReady && displayPowerState == DISPLAY_POWER_ACTIVE ) {
      setDisplayBrightness( getNormalDisplayBrightness() );
    }

    return configComplete;
  }

  void appendConfigData( Print& settingsScript ) override {
    settingsScript.print( F( "cs3st=addDropdown(" "'CoreS3_Display'," "'sleep-timeout'" ");" ) );

    settingsScript.print( F( "addOption(" "cs3st," "'Never'," "0" ");" ) );

    settingsScript.print( F( "addOption(" "cs3st," "'15 sec'," "15" ");" ) );

    settingsScript.print( F( "addOption(" "cs3st," "'30 sec'," "30" ");" ) );

    settingsScript.print( F( "addOption(" "cs3st," "'60 sec'," "60" ");" ) );

    settingsScript.print( F( "addOption(" "cs3st," "'120 sec'," "120" ");" ) );

    settingsScript.print( F( "addInfo(" "'CoreS3_Display:lcd-brightness'," "1," "'<small>1-255</small>'" ");" ) );

    settingsScript.print( F( "cs3fd=addDropdown(" "'CoreS3_Display'," "'fade-duration'" ");" ) );

    settingsScript.print( F( "addOption(" "cs3fd," "'Fast - 150 ms'," "150" ");" ) );

    settingsScript.print( F( "addOption(" "cs3fd," "'Normal - 250 ms'," "250" ");" ) );

    settingsScript.print( F( "addOption(" "cs3fd," "'Slow - 400 ms'," "400" ");" ) );

    settingsScript.print( F( "addOption(" "cs3fd," "'Very Slow - 600 ms'," "600" ");" ) );
  }

  void setup() override {
    Serial.println();

    Serial.println( F( "[CoreS3_Display][BUILD] CoreS3 Display v0.1.0" ) );
    Serial.println( F( "[CoreS3_Display] Initialization start" ) );

    registerScreenshotEndpoint();

    Serial.printf(
      "[CoreS3_Display] " "Hardware: %s, Revision=%s, Runtime=%s\n",
      getHardwareProfileName(),
      getHardwareRevisionName(),
      getHardwareRuntimeModeName()
    );

    Serial.printf(
      "[CoreS3_Display] " "Port status: %s\n",
      getHardwarePortStatusName()
    );

    Serial.printf( "[CoreS3_Display] " "Settings: " "Sleep=%u sec, " "LCD=%u, " "Fade=%s, " "FadeDuration=%u ms\n", sleepTimeoutSec, lcdBrightness, fadeEnabled ? "ON" : "OFF", fadeDurationMs );

    runHardwareDiagnostics();

    if ( isCore2DiagnosticOnlyMode() ) {
      initDone = true;

      Serial.println( F( "[CoreS3_Display] Core2 diagnostic-only runtime complete" ) );
      Serial.println( F( "[CoreS3_Display] Display/Touch/LCD brightness initialization intentionally skipped" ) );
      Serial.println( F( "[CoreS3_Display] Capture the hardware probe result before enabling Core2 UI/Power support" ) );
      Serial.println();

      return;
    }

    if ( !initializeDisplayHardware() ) {
      return;
    }

    setDisplayBrightness( 0 );

    drawStartupBase();

    startupDotCount = 3;

    drawStartupConnectingStatus();

    startupState = STARTUP_FADE_IN;

    startupStateStart = millis();

    startupLastFadeStep = startupStateStart;

    startupLastDotsUpdate = startupStateStart;

    runtimeHealthStartMs = startupStateStart;

    displayPowerState = DISPLAY_POWER_ACTIVE;

    lastUserActivityMs = startupStateStart;

    lastPresetsModifiedTime = presetsModifiedTime;

    presetCacheCount = 0;

    presetCacheReady = false;

    presetCacheBuilding = false;

    presetCacheScanId = 1;

    presetCacheSourceModifiedTime = presetsModifiedTime;

    displayReady = true;

    initDone = true;

    Serial.println( F( "[CoreS3_Display] Initialization complete" ) );

    Serial.println();
  }

  void loop() override {
    if (!displayReady) {
      return;
    }

    if ( startupState != STARTUP_DONE ) {
      handleStartupSequence();

      return;
    }

    serviceWiFiRecoveryAP();

    if ( !presetCacheReady && !presetCacheBuilding ) {
      startPresetCacheRebuild();
    }

    servicePresetCache();

    servicePresetSaveOperation();

    servicePresetDeleteOperation();

    servicePresetBootOperation();

    if ( serviceRuntimeHealthWarnings() ) {
      return;
    }

    // Follow a Preset explicitly selected from the WLED Web UI.
    // A transition to currentPreset == 0 intentionally does not erase the
    // CoreS3 navigation cursor.
    syncPresetNavigationCursorFromCurrentPreset();

    if ( displayPowerState == DISPLAY_POWER_ACTIVE ) {
      handleTouch();
    }

    if ( handleDisplayPowerManagement() ) {
      return;
    }

    unsigned long now = millis();

    if ( now - lastUpdate < 250 ) {
      return;
    }

    lastUpdate = now;

    bool presetPendingSettled = settlePendingPreset();

    NetworkAccessMode currentNetworkMode = getNetworkAccessMode();
    String currentNetworkDisplayText =
      getNetworkDisplayText( currentNetworkMode );

    if ( !readyScreenShown ) {
      drawMainScreen( currentNetworkDisplayText );

      lastNetworkAccessMode = currentNetworkMode;
      lastNetworkDisplayText = currentNetworkDisplayText;

      return;
    }

    const bool networkPresentationChanged =
      ( currentNetworkMode != lastNetworkAccessMode ) ||
      ( currentNetworkDisplayText != lastNetworkDisplayText );

    if ( networkPresentationChanged ) {
      // Do not redraw/reset MAIN in the middle of a touch gesture. This is
      // especially important when a Recovery AP starts during a long press.
      if (
        currentPage == SCREEN_MAIN &&
        touchState.touchTarget != M5STACK_TOUCH_TARGET_NONE
      ) {
        // Keep the old cache so the transition is still observed after the
        // finger is released, unless the Recovery handler updates it itself.
      }
      else {
        lastNetworkAccessMode = currentNetworkMode;
        lastNetworkDisplayText = currentNetworkDisplayText;

        // Network changes are informational only. Do not interrupt COLOR,
        // EFFECT or PRESET operation. MAIN is the page that owns the
        // network-status line, so redraw it only when it is currently visible.
        if ( currentPage == SCREEN_MAIN ) {
          drawMainScreen( currentNetworkDisplayText );

          return;
        }
      }
    }

    if (
      currentPage == SCREEN_MAIN &&
      sampleBatteryStatus()
    ) {
      drawMainBatteryStatus();
    }

    bool ledOn = (bri > 0);

    if ( (int8_t)ledOn != lastLedState ) {
      if ( touchState.touchTarget != M5STACK_TOUCH_TARGET_POWER ) {
        drawPowerButton( ledOn, false );
      }

      lastLedState = ledOn ? 1 : 0;
    }

    uint8_t effectMode = getCurrentEffectMode();

    serviceAudioHealthPresentation( effectMode );

    uint8_t currentSpeed = getCurrentSpeed();

    uint8_t currentIntensity = getCurrentIntensity();

    uint8_t currentPalette = getCurrentPalette();

    uint8_t displayedPreset = getDisplayedPresetId();

    uint32_t primaryColor = getPrimaryColor();

    bool primaryColorChanged = ( !lastPrimaryColorValid || primaryColor != lastPrimaryColor );

    bool hueTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_HUE_DOWN || touchState.touchTarget == M5STACK_TOUCH_TARGET_HUE_UP );

    bool saturationTouchActive = ( touchState.touchTarget == M5STACK_TOUCH_TARGET_SATURATION_DOWN || touchState.touchTarget == M5STACK_TOUCH_TARGET_SATURATION_UP );

    bool colorSlotTouchActive = (
      touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_1 ||
      touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_2 ||
      touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_3
    );

    bool colorControlTouchActive = ( hueTouchActive || saturationTouchActive || colorSlotTouchActive );

    bool primaryColorChangeHandled = false;

    if ( currentPage == SCREEN_MAIN ) {
      primaryColorChangeHandled = updateMainPageState( effectMode, currentSpeed, currentIntensity, currentPalette, primaryColor, primaryColorChanged );
    }
    else if ( currentPage == SCREEN_COLOR ) {
      primaryColorChangeHandled = updateColorPageState( effectMode, primaryColor, primaryColorChanged, colorControlTouchActive );
    }
    else if ( currentPage == SCREEN_EFFECT ) {
      if ( updateEffectPageState( effectMode, currentSpeed, currentIntensity, currentPalette ) ) {
        return;
      }
    }
    else if ( currentPage == SCREEN_PRESET ) {
      updatePresetPageState( displayedPreset, presetPendingSettled );
    }

    if ( primaryColorChanged && primaryColorChangeHandled ) {
      lastPrimaryColor = primaryColor;

      lastPrimaryColorValid = true;
    }
  }

  void addToJsonInfo( JsonObject& root ) override {
    JsonObject user = root["u"];

    if ( user.isNull() ) {
      user = root.createNestedObject( "u" );
    }

    JsonArray hardwareProfileInfo = user.createNestedArray( "M5Stack Hardware Profile" );

    hardwareProfileInfo.add( getHardwareProfileName() );

    JsonArray hardwareRevisionInfo = user.createNestedArray( "M5Stack Hardware Revision" );

    hardwareRevisionInfo.add( getHardwareRevisionName() );

    JsonArray hardwareRuntimeInfo = user.createNestedArray( "M5Stack Runtime Mode" );

    hardwareRuntimeInfo.add( getHardwareRuntimeModeName() );

    JsonArray hardwarePortStatusInfo = user.createNestedArray( "M5Stack Porting Status" );

    hardwarePortStatusInfo.add( getHardwarePortStatusName() );

    JsonArray hardwareProbeInfo = user.createNestedArray( "M5Stack Hardware Probe" );

    hardwareProbeInfo.add( getHardwareProbeStateName() );

    JsonArray hardwareVariantInfo = user.createNestedArray( "M5Stack Detected Variant" );

    hardwareVariantInfo.add( getDetectedVariantName() );

    JsonArray hardwareDetectedRevisionInfo = user.createNestedArray( "M5Stack Detected Revision" );

    hardwareDetectedRevisionInfo.add( getDetectedRevisionName() );

    JsonArray hardwarePmuInfo = user.createNestedArray( "M5Stack Detected PMU" );

    hardwarePmuInfo.add( getDetectedPmuName() );

    JsonArray hardwareImuInfo = user.createNestedArray( "M5Stack Detected IMU" );

    hardwareImuInfo.add( getDetectedImuName() );

    JsonArray hardwareCore2I2cInfo = user.createNestedArray( "M5Stack Core2 I2C Signature" );

    if ( hardwareBackend.isProbeComplete() ) {
      char signature[64];

      snprintf(
        signature,
        sizeof(signature),
        "34:%c 35:%c 38:%c 40:%c 51:%c 68:%c",
        hardwareBackend.hasI2CAddress( 0x34 ) ? 'Y' : 'N',
        hardwareBackend.hasI2CAddress( 0x35 ) ? 'Y' : 'N',
        hardwareBackend.hasI2CAddress( 0x38 ) ? 'Y' : 'N',
        hardwareBackend.hasI2CAddress( 0x40 ) ? 'Y' : 'N',
        hardwareBackend.hasI2CAddress( 0x51 ) ? 'Y' : 'N',
        hardwareBackend.hasI2CAddress( 0x68 ) ? 'Y' : 'N'
      );

      hardwareCore2I2cInfo.add( signature );
    }
    else {
      hardwareCore2I2cInfo.add( "Not probed" );
    }

    JsonArray displayInfo = user.createNestedArray( "CoreS3 Display" );

    if ( isCore2DiagnosticOnlyMode() ) {
      displayInfo.add( "DIAGNOSTIC ONLY - NOT INITIALIZED" );
    }
    else if (displayReady) {
      char text[32];

      snprintf( text, sizeof(text), "READY (%d x %d)", screenWidth, screenHeight );

      displayInfo.add( text );
    }
    else {
      displayInfo.add( "FAILED" );
    }

    JsonArray screenshotInfo = user.createNestedArray( "CoreS3 Display Screenshot" );

    if ( isCore2DiagnosticOnlyMode() ) {
      screenshotInfo.add( "UNAVAILABLE - DIAGNOSTIC ONLY" );
    }
    else if ( displayReady ) {
      screenshotInfo.add( "/cores3/screenshot.bmp" );
    }
    else {
      screenshotInfo.add( "UNAVAILABLE" );
    }

    JsonArray touchInfo = user.createNestedArray( "CoreS3 Display Touch" );

    if ( isCore2DiagnosticOnlyMode() ) {
      if ( hardwareBackend.isProbeComplete() ) {
        touchInfo.add(
          hardwareBackend.hasI2CAddress( 0x38 )
            ? "I2C 0x38 DETECTED - NOT INITIALIZED"
            : "I2C 0x38 NOT DETECTED"
        );
      }
      else {
        touchInfo.add( "DIAGNOSTIC PROBE UNAVAILABLE" );
      }
    }
    else {
      touchInfo.add( touchReady ? "READY" : "NOT FOUND" );
    }

    JsonArray wifiInfo = user.createNestedArray( "CoreS3 Display WiFi" );

    NetworkAccessMode networkMode = getNetworkAccessMode();

    if ( networkMode == NETWORK_ACCESS_STA ) {
      wifiInfo.add( String( "STA: " ) + WiFi.localIP().toString() );
    }
    else if ( networkMode == NETWORK_ACCESS_AP ) {
      if ( recoveryApSessionActive && recoveryApSSID.length() > 0 ) {
        wifiInfo.add(
          String( "Recovery AP: " ) +
          recoveryApSSID +
          " @ " +
          WiFi.softAPIP().toString()
        );
      }
      else {
        wifiInfo.add( String( "AP: " ) + WiFi.softAPIP().toString() );
      }
    }
    else {
      wifiInfo.add( "Offline - local control available" );
    }

    JsonArray ledInfo = user.createNestedArray( "CoreS3 Display LED" );

    ledInfo.add( bri > 0 ? "ON" : "OFF" );

    JsonArray brightnessInfo = user.createNestedArray( "CoreS3 Display Brightness" );

    brightnessInfo.add( bri );

    JsonArray effectInfo = user.createNestedArray( "CoreS3 Display Effect" );

    if ( strip.getSegmentsNum() > 0 ) {
      char effectName[64];

      getEffectName( strip.getMainSegment().mode, effectName, sizeof(effectName) );

      effectInfo.add( effectName );
    }
    else {
      effectInfo.add( "No segment" );
    }

    JsonArray speedInfo = user.createNestedArray( "CoreS3 Effect Speed" );

    if ( strip.getSegmentsNum() > 0 ) {
      speedInfo.add( getCurrentSpeed() );
    }
    else {
      speedInfo.add( "No segment" );
    }

    JsonArray intensityInfo = user.createNestedArray( "CoreS3 Effect Intensity" );

    if ( strip.getSegmentsNum() > 0 ) {
      intensityInfo.add( getCurrentIntensity() );
    }
    else {
      intensityInfo.add( "No segment" );
    }

    JsonArray paletteInfo = user.createNestedArray( "CoreS3 Effect Palette" );

    if ( strip.getSegmentsNum() > 0 ) {
      char paletteName[64];

      getPaletteName( getCurrentPalette(), paletteName, sizeof(paletteName) );

      paletteInfo.add( paletteName );
    }
    else {
      paletteInfo.add( "No segment" );
    }

    JsonArray paletteIdInfo = user.createNestedArray( "CoreS3 Effect Palette ID" );

    if ( strip.getSegmentsNum() > 0 ) {
      paletteIdInfo.add( getCurrentPalette() );
    }
    else {
      paletteIdInfo.add( "No segment" );
    }

    JsonArray paletteTouchInfo = user.createNestedArray( "CoreS3 Palette Touch Area" );

    paletteTouchInfo.add( "Expanded" );

    JsonArray presetInfo = user.createNestedArray( "CoreS3 Display Preset" );

    presetInfo.add( currentPreset > 0 ? "Active Preset" : "Custom State" );

    JsonArray presetIdInfo = user.createNestedArray( "CoreS3 Display Preset ID" );

    presetIdInfo.add( currentPreset );

    JsonArray bootPresetInfo = user.createNestedArray( "CoreS3 Boot Preset" );

    if ( bootPreset == 0 ) {
      bootPresetInfo.add( "NONE" );
    }
    else {
      bootPresetInfo.add( bootPreset );
    }

    JsonArray presetTouchInfo = user.createNestedArray( "CoreS3 Preset Touch Area" );

    presetTouchInfo.add( "Expanded" );

    JsonArray presetCacheInfo = user.createNestedArray( "CoreS3 Preset Cache" );

    if ( presetCacheReady ) {
      char cacheText[32];

      snprintf( cacheText, sizeof(cacheText), "READY (%u)", (unsigned)presetCacheCount );

      presetCacheInfo.add( cacheText );
    }
    else if ( presetCacheBuilding ) {
      // -----------------------------------------------------
      // Do not expose the internal 1..250 scan position.
      // -----------------------------------------------------

      presetCacheInfo.add( "LOADING" );
    }
    else {
      presetCacheInfo.add( "NOT READY" );
    }

    JsonArray colorInfo = user.createNestedArray( "CoreS3 Display Color" );

    if ( strip.getSegmentsNum() > 0 ) {
      uint32_t color = getPrimaryColor();

      char colorText[16];

      snprintf( colorText, sizeof(colorText), "#%02X%02X%02X", R(color), G(color), B(color) );

      colorInfo.add( colorText );
    }
    else {
      colorInfo.add( "No segment" );
    }

    JsonArray hueInfo = user.createNestedArray( "CoreS3 Display Hue" );

    if ( strip.getSegmentsNum() > 0 ) {
      hueInfo.add( getDisplayedHue() );
    }
    else {
      hueInfo.add( "No segment" );
    }

    JsonArray saturationInfo = user.createNestedArray( "CoreS3 Display Saturation" );

    if ( strip.getSegmentsNum() > 0 ) {
      saturationInfo.add( getDisplayedSaturation() );
    }
    else {
      saturationInfo.add( "No segment" );
    }

    JsonArray pageInfo = user.createNestedArray( "CoreS3 Display Page" );

    if ( currentPage == SCREEN_MAIN ) {
      pageInfo.add( "MAIN" );
    }
    else if ( currentPage == SCREEN_COLOR ) {
      pageInfo.add( "COLOR" );
    }
    else if ( currentPage == SCREEN_EFFECT ) {
      pageInfo.add( "EFFECT" );
    }
    else {
      pageInfo.add( "PRESET" );
    }

    JsonArray powerStateInfo = user.createNestedArray( "CoreS3 Display Power State" );

    switch ( displayPowerState ) {
    case DISPLAY_POWER_ACTIVE:
      powerStateInfo.add( "ACTIVE" );
      break;

    case DISPLAY_POWER_SLEEP_FADE_OUT:
      powerStateInfo.add( "SLEEP FADE OUT" );
      break;

    case DISPLAY_POWER_SLEEPING:
      powerStateInfo.add( "SLEEPING" );
      break;

    case DISPLAY_POWER_WAKE_FADE_IN:
      powerStateInfo.add( "WAKE FADE IN" );
      break;

    case DISPLAY_POWER_WAKE_WAIT_RELEASE:
      powerStateInfo.add( "WAKE WAIT RELEASE" );
      break;
    }

    JsonArray lcdBrightnessInfo = user.createNestedArray( "CoreS3 LCD Brightness" );

    lcdBrightnessInfo.add( lcdBrightness );

    JsonArray sleepInfo = user.createNestedArray( "CoreS3 Display Sleep" );

    if ( sleepTimeoutSec == 0 ) {
      sleepInfo.add( "Never" );
    }
    else {
      char sleepText[24];

      snprintf( sleepText, sizeof(sleepText), "%u sec", sleepTimeoutSec );

      sleepInfo.add( sleepText );
    }

    JsonArray fadeInfo = user.createNestedArray( "CoreS3 Display Fade" );

    if (!fadeEnabled) {
      fadeInfo.add( "OFF" );
    }
    else {
      char fadeText[24];

      snprintf( fadeText, sizeof(fadeText), "ON (%u ms)", fadeDurationMs );

      fadeInfo.add( fadeText );
    }

  }
};

static CoreS3DisplayUsermod coreS3DisplayUsermod;

REGISTER_USERMOD(coreS3DisplayUsermod);
