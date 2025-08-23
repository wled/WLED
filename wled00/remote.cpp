#include "wled.h"
#ifndef WLED_DISABLE_ESPNOW

#define ESPNOW_BUSWAIT_TIMEOUT 24 // one frame timeout to wait for bus to finish updating

#define NIGHT_MODE_DEACTIVATED     -1
#define NIGHT_MODE_BRIGHTNESS      5

#define WIZMOTE_BUTTON_ON          1
#define WIZMOTE_BUTTON_OFF         2
#define WIZMOTE_BUTTON_NIGHT       3
#define WIZMOTE_BUTTON_ONE         16
#define WIZMOTE_BUTTON_TWO         17
#define WIZMOTE_BUTTON_THREE       18
#define WIZMOTE_BUTTON_FOUR        19
#define WIZMOTE_BUTTON_BRIGHT_UP   9
#define WIZMOTE_BUTTON_BRIGHT_DOWN 8

#define WIZ_SMART_BUTTON_ON          100
#define WIZ_SMART_BUTTON_OFF         101
#define WIZ_SMART_BUTTON_BRIGHT_UP   102
#define WIZ_SMART_BUTTON_BRIGHT_DOWN 103

// This is kind of an esoteric strucure because it's pulled from the "Wizmote"
// product spec. That remote is used as the baseline for behavior and availability
// since it's broadly commercially available and works out of the box as a drop-in
typedef struct WizMoteMessageStructure {
  uint8_t program;  // 0x91 for ON button, 0x81 for all others
  uint8_t seq[4];   // Incremetal sequence number 32 bit unsigned integer LSB first
  uint8_t dt1;      // Button Data Type (0x32)
  uint8_t button;   // Identifies which button is being pressed
  uint8_t dt2;      // Battery Level Data Type (0x01)
  uint8_t batLevel; // Battery Level 0-100
  
  uint8_t byte10;   // Unknown, maybe checksum
  uint8_t byte11;   // Unknown, maybe checksum
  uint8_t byte12;   // Unknown, maybe checksum
  uint8_t byte13;   // Unknown, maybe checksum
} message_structure_t;

static uint32_t last_seq = UINT32_MAX;
static int brightnessBeforeNightMode = NIGHT_MODE_DEACTIVATED;
static int16_t ESPNowButton = -1; // set in callback if new button value is received

// Pulled from the IR Remote logic but reduced to 10 steps with a constant of 3
static const byte brightnessSteps[] = {
  6, 9, 14, 22, 33, 50, 75, 113, 170, 255
};
static const size_t numBrightnessSteps = sizeof(brightnessSteps) / sizeof(byte);

inline bool nightModeActive() {
  return brightnessBeforeNightMode != NIGHT_MODE_DEACTIVATED;
}

static void activateNightMode() {
  if (nightModeActive()) return;
  brightnessBeforeNightMode = bri;
  bri = NIGHT_MODE_BRIGHTNESS;
  stateUpdated(CALL_MODE_BUTTON);
}

static bool resetNightMode() {
  if (!nightModeActive()) return false;
  bri = brightnessBeforeNightMode;
  brightnessBeforeNightMode = NIGHT_MODE_DEACTIVATED;
  stateUpdated(CALL_MODE_BUTTON);
  return true;
}

// increment `bri` to the next `brightnessSteps` value
static void brightnessUp() {
  if (nightModeActive()) return;
  // dumb incremental search is efficient enough for so few items
  for (unsigned index = 0; index < numBrightnessSteps; ++index) {
    if (brightnessSteps[index] > bri) {
      bri = brightnessSteps[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

// decrement `bri` to the next `brightnessSteps` value
static void brightnessDown() {
  if (nightModeActive()) return;
  // dumb incremental search is efficient enough for so few items
  for (int index = numBrightnessSteps - 1; index >= 0; --index) {
    if (brightnessSteps[index] < bri) {
      bri = brightnessSteps[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

static void setOn() {
  resetNightMode();
  if (!bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

static void setOff() {
  resetNightMode();
  if (bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

/**
 * @brief Exit night mode (if active) and apply a stored preset, using fallbacks.
 *
 * Ensures night mode is deactivated, then delegates to applyPresetWithFallback()
 * to apply the preset identified by presetID. The provided effectID and
 * paletteID are forwarded and may be used to override the preset's defaults.
 *
 * @param presetID Preset index/ID to apply.
 * @param effectID Effect override forwarded to the preset application routine.
 * @param paletteID Palette override forwarded to the preset application routine.
 */
void presetWithFallback(uint8_t presetID, uint8_t effectID, uint8_t paletteID) {
  resetNightMode();
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

/**
 * @brief Process a remote button press by looking up and executing a command in /remote.json.
 *
 * Blocks briefly while waiting for any active strip update to finish (up to ESPNOW_BUSWAIT_TIMEOUT)
 * and requires the JSON buffer lock. Looks for an entry keyed by the decimal button id (e.g. `"5":`)
 * in /remote.json and interprets its `cmd` value:
 * - If `cmd` is a string starting with "!" it executes a small set of builtin operations
 *   (brightness up/down, preset-with-fallback).
 * - If `cmd` is a non-`!` string it is treated as an API-style command (prefixed with `win&` if missing)
 *   and passed to handleSet(); a segment selector `SS=` is added if appropriate.
 * - If `cmd` is a JSON object it is deserialized into state via deserializeState().
 *
 * The function may call brightnessUp(), brightnessDown(), presetWithFallback(), handleSet(),
 * deserializeState(), and stateUpdated(CALL_MODE_BUTTON). It reads /remote.json and holds/releases
 * the global JSON buffer lock for the duration of the operation.
 *
 * @param button Decimal identifier of the remote button to look up in /remote.json.
 * @return true if a command was found and executed (or deserialized); false if no entry existed,
 *         the JSON buffer lock could not be acquired, or no action was taken.
 */
static bool remoteJson(int button)
{
  char objKey[10];
  bool parsed = false;

  if (!requestJSONBufferLock(22)) return false;

  sprintf_P(objKey, PSTR("\"%d\":"), button);

  unsigned long start = millis();
  while (strip.isUpdating() && millis()-start < ESPNOW_BUSWAIT_TIMEOUT) yield(); // wait for strip to finish updating, accessing FS during sendout causes glitches

  // attempt to read command from remote.json
  readObjectFromFile(PSTR("/remote.json"), objKey, pDoc);
  JsonObject fdo = pDoc->as<JsonObject>();
  if (fdo.isNull()) {
    // the received button does not exist
    //if (!WLED_FS.exists(F("/remote.json"))) errorFlag = ERR_FS_RMLOAD; //warn if file itself doesn't exist
    releaseJSONBufferLock();
    return parsed;
  }

  String cmdStr = fdo["cmd"].as<String>();
  JsonObject jsonCmdObj = fdo["cmd"]; //object

  if (jsonCmdObj.isNull())  // we could also use: fdo["cmd"].is<String>()
  {
    if (cmdStr.startsWith("!")) {
      // call limited set of C functions
      if (cmdStr.startsWith(F("!incBri"))) {
        brightnessUp();
        parsed = true;
      } else if (cmdStr.startsWith(F("!decBri"))) {
        brightnessDown();
        parsed = true;
      } else if (cmdStr.startsWith(F("!presetF"))) { //!presetFallback
        uint8_t p1 = fdo["PL"] | 1;
        uint8_t p2 = fdo["FX"] | random8(strip.getModeCount() -1);
        uint8_t p3 = fdo["FP"] | 0;
        presetWithFallback(p1, p2, p3);
        parsed = true;
      }
    } else {
      // HTTP API command
      String apireq = "win"; apireq += '&';                        // reduce flash string usage
      //if (cmdStr.indexOf("~") || fdo["rpt"]) lastValidCode = code; // repeatable action
      if (!cmdStr.startsWith(apireq)) cmdStr = apireq + cmdStr;    // if no "win&" prefix
      if (!irApplyToAllSelected && cmdStr.indexOf(F("SS="))<0) {
        char tmp[10];
        sprintf_P(tmp, PSTR("&SS=%d"), strip.getMainSegmentId());
        cmdStr += tmp;
      }
      fdo.clear();                                                 // clear JSON buffer (it is no longer needed)
      handleSet(nullptr, cmdStr, false);                           // no stateUpdated() call here
      stateUpdated(CALL_MODE_BUTTON);
      parsed = true;
    }
  } else {
    // command is JSON object (TODO: currently will not handle irApplyToAllSelected correctly)
    deserializeState(jsonCmdObj, CALL_MODE_BUTTON);
    parsed = true;
  }
  releaseJSONBufferLock();
  return parsed;
}

/**
 * @brief ESP-NOW receive callback that validates and records incoming WizMote/Smart-button packets for deferred processing.
 *
 * Performs lightweight validation suitable for callback context: verifies the packet originates from the linked remote, 
 * checks the packet length equals sizeof(message_structure_t), and deduplicates using the 32-bit sequence number encoded
 * in seq[0..3]. On success the function stores the received button ID into the global ESPNowButton and updates last_seq.
 *
 * This function must remain short-running (no file I/O or blocking operations) since it is invoked from the ESP-NOW
 * receive callback context.
 *
 * @param incomingData Pointer to the raw incoming packet bytes; must follow the WizMote message layout (message_structure_t).
 * @param len Length of the incoming packet in bytes; packets with a length different from sizeof(message_structure_t) are ignored.
 *
 * Side effects:
 * - Sets ESPNowButton to the incoming button value when validation succeeds.
 * - Updates last_seq with the packet's sequence number to prevent reprocessing duplicates.
 */
void handleWiZdata(uint8_t *incomingData, size_t len) {
  message_structure_t *incoming = reinterpret_cast<message_structure_t *>(incomingData);

  if (strcmp(last_signal_src, linked_remote) != 0) {
    DEBUG_PRINT(F("ESP Now Message Received from Unlinked Sender: "));
    DEBUG_PRINTLN(last_signal_src);
    return;
  }

  if (len != sizeof(message_structure_t)) {
    DEBUG_PRINTF_P(PSTR("Unknown incoming ESP Now message received of length %u\n"), len);
    return;
  }

  uint32_t cur_seq = incoming->seq[0] | (incoming->seq[1] << 8) | (incoming->seq[2] << 16) | (incoming->seq[3] << 24);
  if (cur_seq == last_seq) {
    return;
  }

  DEBUG_PRINT(F("Incoming ESP Now Packet ["));
  DEBUG_PRINT(cur_seq);
  DEBUG_PRINT(F("] from sender ["));
  DEBUG_PRINT(last_signal_src);
  DEBUG_PRINT(F("] button: "));
  DEBUG_PRINTLN(incoming->button);

  ESPNowButton = incoming->button; // save state, do not process in callback (can cause glitches)
  last_seq = cur_seq;
}

/**
 * @brief Process a queued ESP-NOW remote button event.
 *
 * Checks the global ESPNowButton; if set (>= 0) attempts to handle it via
 * remoteJson() (which may read /remote.json). If remoteJson() does not handle
 * the button, the button value is mapped to built-in actions (on/off, presets,
 * night mode, brightness up/down). After processing the value is cleared (set
 * to -1).
 *
 * @details
 * - Intended to be called from the main loop (deferred processing) — not from
 *   the ESP-NOW receive callback — to avoid doing filesystem work or applying
 *   state changes inside the interrupt/receive path.
 * - May perform filesystem access and will trigger state changes (calls such
 *   as setOn, setOff, presetWithFallback, activateNightMode,
 *   brightnessUp/brightnessDown) which in turn call stateUpdated().
 *
 * @note remoteJson() performs its own JSON-buffer locking, but this function
 * may still block briefly while remoteJson accesses storage.
 */
void handleRemote() {
  if(ESPNowButton >= 0) {
  if (!remoteJson(ESPNowButton))
    switch (ESPNowButton) {
      case WIZMOTE_BUTTON_ON             : setOn();                                         break;
      case WIZMOTE_BUTTON_OFF            : setOff();                                        break;
      case WIZMOTE_BUTTON_ONE            : presetWithFallback(1, FX_MODE_STATIC,        0); break;
      case WIZMOTE_BUTTON_TWO            : presetWithFallback(2, FX_MODE_BREATH,        0); break;
      case WIZMOTE_BUTTON_THREE          : presetWithFallback(3, FX_MODE_FIRE_FLICKER,  0); break;
      case WIZMOTE_BUTTON_FOUR           : presetWithFallback(4, FX_MODE_RAINBOW,       0); break;
      case WIZMOTE_BUTTON_NIGHT          : activateNightMode();                             break;
      case WIZMOTE_BUTTON_BRIGHT_UP      : brightnessUp();                                  break;
      case WIZMOTE_BUTTON_BRIGHT_DOWN    : brightnessDown();                                break;
      case WIZ_SMART_BUTTON_ON           : setOn();                                         break;
      case WIZ_SMART_BUTTON_OFF          : setOff();                                        break;
      case WIZ_SMART_BUTTON_BRIGHT_UP    : brightnessUp();                                  break;
      case WIZ_SMART_BUTTON_BRIGHT_DOWN  : brightnessDown();                                break;
      default: break;
    }
  }
  ESPNowButton = -1;
}

#else
/**
 * @brief Process a pending ESPNOW remote button event.
 *
 * Checks for a queued ESPNow button value (set by the packet callback) and, if present,
 * attempts to handle it via the JSON-defined remote actions in /remote.json. If no JSON
 * command is found, falls back to built-in mappings (on/off, presets, night mode, brightness
 * up/down). After processing the button is cleared.
 *
 * This function performs state changes (brightness, power, presets, etc.) and triggers
 * stateUpdated(CALL_MODE_BUTTON) where appropriate. When ESPNOW support is compiled out,
 * this function is a no-op. Call regularly from the main loop to process incoming remote events.
 */
void handleRemote() {}
#endif
