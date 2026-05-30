// SPDX-License-Identifier: GPL-3.0-or-later
//
// DALI Gear Usermod for WLED
// Makes WLED act as a DALI control gear (IEC 62386) — i.e. a light that
// responds to commands from an external DALI master (wall dimmer, BMS, etc.).
//
// Phase 1: RX-only bus listening. Handles DAPC (direct arc power control)
// and basic indirect commands (OFF, MAX, MIN, UP, DOWN, LAST ACTIVE LEVEL).
// Phase 2: DT8 colour temperature (IEC 62386-209). Handles SET DTR0/DTR1
// special commands, ENABLE DEVICE TYPE 8, SET TEMPORARY COLOUR TEMPERATURE,
// ACTIVATE to map DALI Tc (mireds) → WLED CCT (Kelvin), and backward frame
// responses to QUERY STATUS (0x90), QUERY CONTROL GEAR PRESENT (0x91),
// QUERY DEVICE TYPE (0x18), QUERY ACTUAL LEVEL (0xA0),
// and QUERY COLOUR TYPE (0xF7 per IEC 62386-209).
//
// Hardware: requires a DALI bus interface circuit (see readme.md).
// ESP32 only — uses hardware timer API not available on ESP8266.

#include "wled.h"

#ifndef ARDUINO_ARCH_ESP32
#error "dali_gear usermod requires ESP32 (hardware timer API not available on ESP8266)"
#endif

#include <qqqDALI.h>

// ---------------------------------------------------------------------------
// DALI frame parsing helpers
// ---------------------------------------------------------------------------

// Returns true if the address byte of a forward frame is addressed to us.
// daliAddr: our configured short address (0–63), or -1 to accept broadcast only.
static bool daliAddressedToMe(uint8_t addrByte, int8_t daliAddr) {
  // Broadcast: 1111 111x  (0xFE or 0xFF)
  if ((addrByte | 0x01) == 0xFF) return true;
  // Short address: 0AAA AAA x  — top bit 0
  if (!(addrByte & 0x80) && daliAddr >= 0) {
    uint8_t frameAddr = (addrByte >> 1) & 0x3F;
    return frameAddr == (uint8_t)daliAddr;
  }
  // Group address: 100A AAA x — not handled
  return false;
}

// Map a DALI arc level (1–254) to WLED bri (1–255).
// Linear mapping is correct here: WLED's gamma correction handles the LED
// output curve, serving the same perceptual-uniformity purpose as DALI's
// logarithmic arc power table.
static uint8_t daliLevelToWledBri(uint8_t level) {
  if (level == 0) return 0;
  // level 1–254 → bri 1–255
  return (uint8_t)(((uint16_t)level * 255u + 127u) / 254u);
}

// Map WLED bri (1–255) back to a DALI arc level (1–254), for QUERY ACTUAL LEVEL.
static uint8_t wledBriToDaliLevel(uint8_t b) {
  if (b == 0) return 0;
  return (uint8_t)(((uint16_t)b * 254u + 127u) / 255u);
}

// ---------------------------------------------------------------------------
// ISR and timer — file-scope so the ISR can reach the Dali instance
// ---------------------------------------------------------------------------

static Dali _dali;
static hw_timer_t *_daliTimer = nullptr;

static void ARDUINO_ISR_ATTR daliTimerISR() {
  _dali.timer();
}

// ---------------------------------------------------------------------------
// Usermod class
// ---------------------------------------------------------------------------

class DaliGearUsermod : public Usermod {
  private:
    bool     _enabled       = false;
    bool     _initDone      = false;
    int8_t   _rxPin         = 14;  // default: Waveshare Pico-DALI2 RX
    int8_t   _txPin         = 17;  // default: Waveshare Pico-DALI2 TX
    bool     _txInverted    = false; // true for circuits with a single-stage inverting TX driver
                                     // (e.g. qqqDALI DIY PNP circuit).
                                     // false (default) for the Waveshare Pico-DALI2 and other
                                     // boards with double-inversion (NPN + opto-isolator).
    int8_t   _daliAddr      = -1;   // -1 = respond to broadcast only
    uint8_t  _lastDaliLevel = 0;    // last DALI arc level received (for info panel)

    // DT8 (IEC 62386-209) colour temperature state
    uint8_t  _dtr0          = 0;   // Data Transfer Register 0 (low byte of Tc mireds)
    uint8_t  _dtr1          = 0;   // Data Transfer Register 1 (high byte of Tc mireds)
    bool     _dt8Active     = false; // true after ENABLE DEVICE TYPE 8
    uint16_t _tempCCT       = 0;   // temporary colour temperature register (mireds)
    uint16_t _lastCCTKelvin = 0;   // last applied CCT in Kelvin (for info panel)

    // Backward frame scheduling — DALI requires response 7Te–22Te (≈2.9–9.2ms)
    // after the forward frame stop bits. We schedule via timestamp.
    uint8_t  _pendingBF        = 0;     // backward frame byte to send (0 = none pending)
    uint32_t _pendingBFTime    = 0;     // millis() threshold — send when now >= this

    static const char _name[];
    static const char _enabled_key[];

    // ---------------------------------------------------------------------------
    // Bus HAL callbacks (static so they can be passed as function pointers).
    // TX polarity depends on interface hardware:
    //   _txInverted = false (default): GPIO HIGH = bus idle, GPIO LOW = assert bus.
    //     Used by Waveshare Pico-DALI2 (NPN + opto-isolator = double inversion).
    //   _txInverted = true: GPIO LOW = bus idle, GPIO HIGH = assert bus.
    //     Used by the qqqDALI DIY PNP circuit (single inversion via PNP transistor).
    // ---------------------------------------------------------------------------
    static uint8_t busIsHigh() {
      return digitalRead(_rxPinStatic);
    }
    static void busSetLow() {
      // "set bus low" = assert the DALI bus
      digitalWrite(_txPinStatic, _txInvertedStatic ? HIGH : LOW);
    }
    static void busSetHigh() {
      // "set bus high" = release the DALI bus (idle)
      digitalWrite(_txPinStatic, _txInvertedStatic ? LOW : HIGH);
    }

    // Static copies of pins/config needed by the HAL callbacks
    static int8_t _rxPinStatic;
    static int8_t _txPinStatic;
    static bool   _txInvertedStatic;

    // ---------------------------------------------------------------------------
    // Schedule a DALI backward frame to be sent after the mandatory settling time.
    // DALI IEC 62386-102 requires 7Te (≈2.9ms) min, 22Te (≈9.2ms) max.
    // We target 4ms — safely inside the window even with loop jitter.
    // ---------------------------------------------------------------------------
    void scheduleBF(uint8_t byte) {
      _pendingBF     = byte;
      _pendingBFTime = millis() + 4; // 4ms after frame received in loop()
    }

    // ---------------------------------------------------------------------------
    // Apply a DALI arc level to WLED
    // ---------------------------------------------------------------------------
    void applyLevel(uint8_t daliLevel) {
      _lastDaliLevel = daliLevel;
      if (daliLevel == 0) {
        briLast = bri ? bri : briLast; // preserve last brightness for toggle
        bri = 0;
      } else {
        bri = daliLevelToWledBri(daliLevel);
      }
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    // ---------------------------------------------------------------------------
    // Apply a colour temperature in mireds to WLED via strip.setCCT(Kelvin)
    // ---------------------------------------------------------------------------
    void applyCCT(uint16_t mireds) {
      if (mireds == 0) return; // 0 mireds is undefined / mask value — ignore
      // Convert mireds to Kelvin. Clamp to WLED's accepted range (1900–10091 K).
      uint32_t kelvin = 1000000UL / mireds;
      if (kelvin < 1900)  kelvin = 1900;
      if (kelvin > 10091) kelvin = 10091;
      _lastCCTKelvin = (uint16_t)kelvin;
      strip.setCCT(_lastCCTKelvin);
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
      DEBUG_PRINTF("[DALI] CCT applied: %u mireds → %u K\n", mireds, (unsigned)kelvin);
    }

    // ---------------------------------------------------------------------------
    // Handle an indirect DALI command (S=1 in address byte)
    // ---------------------------------------------------------------------------
    void handleCommand(uint8_t cmd) {
      switch (cmd) {
        case DALI_OFF:
          applyLevel(0);
          break;
        case DALI_UP:
          if (bri > 0) {
            bri = (bri > 245) ? 255 : bri + 10;
            stateUpdated(CALL_MODE_DIRECT_CHANGE);
          }
          break;
        case DALI_DOWN:
          if (bri > 10) bri -= 10;
          else if (bri > 0) bri = 1;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_RECALL_MAX_LEVEL:
          bri = 255;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_RECALL_MIN_LEVEL:
          bri = 1;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_GO_TO_LAST_ACTIVE_LEVEL:
          bri = briLast ? briLast : 128;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_ON_AND_STEP_UP:
          if (bri == 0) bri = 1;
          else bri = (bri > 245) ? 255 : bri + 10;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_STEP_UP:
          if (bri < 255) bri++;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_STEP_DOWN:
          if (bri > 1) bri--;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;
        case DALI_STEP_DOWN_AND_OFF:
          if (bri <= 1) bri = 0;
          else bri--;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
          break;

        // IEC 62386-102 §11.2 query commands — backward frame responses.
        // These allow a DALI master to detect gear presence and read basic status
        // before sending DT8 or other application commands.

        case 0x90: {
          // QUERY STATUS — respond with status byte.
          // Bit 2 = lamp arc power on (1 if bri > 0).
          // Bit 6 = missing short address (1 if no address configured).
          // All other status/fault bits = 0 (no failures to report).
          uint8_t status = ((bri > 0) ? 0x04u : 0x00u)
                         | ((_daliAddr < 0) ? 0x40u : 0x00u);
          DEBUG_PRINTF("[DALI] QUERY STATUS → 0x%02x\n", status);
          scheduleBF(status);
          break;
        }

        case 0x91:
          // QUERY CONTROL GEAR PRESENT — respond 0xFF ("Yes").
          // Many masters send this first to detect whether any gear is on the bus;
          // silence here causes the master to skip all subsequent commands.
          DEBUG_PRINTLN(F("[DALI] QUERY CONTROL GEAR PRESENT → 0xFF"));
          scheduleBF(0xFF);
          break;

        case 0x18:
          // QUERY DEVICE TYPE — respond 0x08 (device type 8 = colour control, IEC 62386-209).
          // Conformant DALI-2 masters send this before issuing ENABLE DEVICE TYPE 8 or any
          // DT8 application extended commands. Silence causes such masters to skip CCT control.
          DEBUG_PRINTLN(F("[DALI] QUERY DEVICE TYPE → 0x08"));
          scheduleBF(0x08);
          break;

        case 0xA0:
          // QUERY ACTUAL LEVEL — respond with current arc level (0–254).
          // Derived from the current WLED brightness so it stays accurate even if
          // bri was changed via the WLED UI rather than a DALI command.
          DEBUG_PRINTF("[DALI] QUERY ACTUAL LEVEL → %u\n", wledBriToDaliLevel(bri));
          scheduleBF(wledBriToDaliLevel(bri));
          break;

        // DT8 (IEC 62386-209) application extended commands.
        // These are only valid when preceded by ENABLE DEVICE TYPE 8 (addr=0xC1, cmd=8).
        // 0xE1 = SET TEMPORARY COLOUR TEMPERATURE — loads DTR0+DTR1 into temp register.
        // 0xE2 = ACTIVATE — applies the temporary colour temperature.
        // 0xF7 = QUERY COLOUR TYPE (IEC 62386-209 §11.3.4.2) — master asks which DT8
        //        colour modes are supported. Response bitmask:
        //          bit 0 = XY colour, bit 1 = Tc colour temperature,
        //          bit 2 = Primary N, bit 3 = RGBWAF.  We support Tc only → 0x02.
        //        Note: some non-standard masters send this as 0xE7 instead. Both are
        //        handled here to maximise interoperability.
        case 0xE1:
          if (_dt8Active) {
            _tempCCT = ((uint16_t)_dtr1 << 8) | _dtr0;
            DEBUG_PRINTF("[DALI] SET TEMPORARY COLOUR TEMPERATURE: %u mireds (DTR1=0x%02x DTR0=0x%02x)\n",
                         _tempCCT, _dtr1, _dtr0);
          } else {
            DEBUG_PRINTLN(F("[DALI] SET TEMPORARY COLOUR TEMPERATURE received but DT8 not active — ignored"));
          }
          break;
        case 0xE2:
          if (_dt8Active && _tempCCT > 0) {
            DEBUG_PRINTF("[DALI] ACTIVATE: applying %u mireds\n", _tempCCT);
            applyCCT(_tempCCT);
          } else {
            DEBUG_PRINTF("[DALI] ACTIVATE: skipped (dt8Active=%d tempCCT=%u)\n", _dt8Active, _tempCCT);
          }
          _dt8Active = false;
          break;

        case 0xE7:
          // 0xE7 is not QUERY COLOUR TYPE per IEC 62386-209 — do not respond.
          // (QUERY COLOUR TYPE is 0xF7; some non-standard masters mistakenly use
          // 0xE7, but sending a backward frame here would violate the spec.)
          DEBUG_PRINTLN(F("[DALI] cmd 0xE7 (not a query — no response)"));
          break;

        case 0xF7: // QUERY COLOUR TYPE — IEC 62386-209 §11.3.4.2
          // Respond regardless of _dt8Active state; master needs to know our
          // capabilities before it will send ENABLE DEVICE TYPE 8.
          // Response bitmask: bit 1 = Tc colour temperature supported → 0x02.
          DEBUG_PRINTLN(F("[DALI] QUERY COLOUR TYPE (0xF7) → scheduling backward frame 0x02 (Tc supported)"));
          scheduleBF(0x02);
          break;

        default:
          DEBUG_PRINTF("[DALI] unhandled command 0x%02x (%u) — ignored\n", cmd, cmd);
          break;
      }
    }

  public:

    void setup() override {
      if (!_enabled || _rxPin < 0 || _txPin < 0) {
        _initDone = true;
        return;
      }

      // Claim pins via WLED pin manager
      if (!PinManager::allocatePin(_rxPin, false, PinOwner::UM_DALI_GEAR)) {
        DEBUG_PRINTLN(F("[DALI] RX pin allocation failed"));
        _enabled = false;
        _initDone = true;
        return;
      }
      if (!PinManager::allocatePin(_txPin, true, PinOwner::UM_DALI_GEAR)) {
        DEBUG_PRINTLN(F("[DALI] TX pin allocation failed"));
        PinManager::deallocatePin(_rxPin, PinOwner::UM_DALI_GEAR);
        _enabled = false;
        _initDone = true;
        return;
      }

      // Configure GPIO
      pinMode(_rxPin, INPUT);
      pinMode(_txPin, OUTPUT);
      // Idle state: bus not asserted. Polarity depends on interface circuit.
      digitalWrite(_txPin, _txInverted ? LOW : HIGH);

      // Store static copies for HAL callbacks
      _rxPinStatic      = _rxPin;
      _txPinStatic      = _txPin;
      _txInvertedStatic = _txInverted;

      _dali.begin(busIsHigh, busSetLow, busSetHigh);

      // Hardware timer: IDF v4 API
      // Timer 1 (timer 0 is used by SparkFunDMX), prescaler 80 → 1 MHz tick.
      // Alarm at 104 ticks → ~9615 Hz ≈ 1200 baud × 8 oversample.
      _daliTimer = timerBegin(1, 80, true);
      timerAttachInterrupt(_daliTimer, &daliTimerISR, true);
      timerAlarmWrite(_daliTimer, 104, true);
      timerAlarmEnable(_daliTimer);

      DEBUG_PRINTF("[DALI] Gear usermod initialised (RX=%d TX=%d txInv=%d addr=%d)\n",
                   _rxPin, _txPin, (int)_txInverted, _daliAddr);
      _initDone = true;
    }


    void loop() override {
      if (!_enabled || !_initDone || _rxPin < 0) return;

      // Send any pending backward frame once the settling window opens (≥7Te ≈ 2.9ms).
      if (_pendingBF && (millis() >= _pendingBFTime)) {
        uint8_t bf = _pendingBF;
        _pendingBF = 0;
        uint8_t result = _dali.tx(&bf, 8);
        DEBUG_PRINTF("[DALI] backward frame 0x%02x sent (tx result=%u)\n", bf, result);
      }

      uint8_t data[4];
      uint8_t bits = _dali.rx(data);

      if (bits == 0) return; // nothing received

      // A DALI forward frame is exactly 16 bits (2 bytes).
      // 1-bit returns are normal bus-idle sampling noise from the library — discard silently.
      // Log only genuinely unexpected lengths (partial frames: 3–15 bits).
      if (bits != 16) {
        if (bits > 2) {
          DEBUG_PRINTF("[DALI] partial frame: %u bits (data: 0x%02x 0x%02x 0x%02x 0x%02x)\n",
                       bits, data[0], data[1], data[2], data[3]);
        }
        return;
      }

      uint8_t addrByte = data[0];
      uint8_t cmdByte  = data[1];

      DEBUG_PRINTF("[DALI] raw frame: addr=0x%02x cmd=0x%02x\n", addrByte, cmdByte);

      // Sniff special broadcast commands that are NOT gear-addressed.
      // These must be processed regardless of our _daliAddr setting.
      //   0xA3 xx — SET DTR0 (Data Transfer Register 0) = xx
      //   0xC3 xx — SET DTR1 (Data Transfer Register 1) = xx
      //   0xC1 08 — ENABLE DEVICE TYPE 8
      if (addrByte == 0xA3) {
        _dtr0 = cmdByte;
        DEBUG_PRINTF("[DALI] SET DTR0 = 0x%02x (%u)\n", cmdByte, cmdByte);
        return;
      }
      if (addrByte == 0xC3) {
        _dtr1 = cmdByte;
        DEBUG_PRINTF("[DALI] SET DTR1 = 0x%02x (%u)\n", cmdByte, cmdByte);
        return;
      }
      if (addrByte == 0xC1) {
        if (cmdByte == 8) {
          _dt8Active = true;
          DEBUG_PRINTLN(F("[DALI] ENABLE DEVICE TYPE 8"));
        } else {
          DEBUG_PRINTF("[DALI] ENABLE DEVICE TYPE %u (not handled)\n", cmdByte);
        }
        return;
      }

      if (!daliAddressedToMe(addrByte, _daliAddr)) {
        DEBUG_PRINTF("[DALI] frame not for us: addr=0x%02x (our addr=%d) — ignored\n",
                     addrByte, _daliAddr);
        return;
      }

      bool isDapc = !(addrByte & 0x01); // S bit = 0 → DAPC

      if (isDapc) {
        if (cmdByte == 255) {
          DEBUG_PRINTLN(F("[DALI] DAPC 255 (mask) — ignored"));
        } else {
          DEBUG_PRINTF("[DALI] DAPC level=%u → bri=%u\n", cmdByte, daliLevelToWledBri(cmdByte));
          applyLevel(cmdByte);
          // Some masters use a non-standard combined flow: DTR0/DTR1 set the colour
          // temperature, ENABLE DEVICE TYPE 8 arms it, and the subsequent DAPC applies
          // both brightness and CCT in one go (without 0xE1+0xE2).
          if (_dt8Active && (_dtr1 || _dtr0)) {
            uint16_t mireds = ((uint16_t)_dtr1 << 8) | _dtr0;
            applyCCT(mireds);
          }
          _dt8Active = false;
        }
      } else {
        DEBUG_PRINTF("[DALI] command 0x%02x (%u)\n", cmdByte, cmdByte);
        handleCommand(cmdByte);
      }
    }


    void addToJsonInfo(JsonObject& root) override {
      if (!_initDone) return;
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray arr = user.createNestedArray(FPSTR(_name));
      if (!_enabled) {
        arr.add(F("disabled"));
        return;
      }
      if (_rxPin < 0 || _txPin < 0) {
        arr.add(F("pins not configured"));
        return;
      }
      arr.add(_lastDaliLevel);
      arr.add(F(" DALI level"));
      if (_lastCCTKelvin > 0) {
        JsonArray cctArr = user.createNestedArray(F("DALIGear CCT"));
        cctArr.add(_lastCCTKelvin);
        cctArr.add(F(" K"));
      }
    }


    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled_key)] = _enabled;
      top["pin_rx"]     = _rxPin;
      top["pin_tx"]     = _txPin;
      top["tx_inverted"] = _txInverted;
      top["daliAddr"]   = _daliAddr;
    }


    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled_key)], _enabled,     false);
      configComplete &= getJsonValue(top["pin_rx"],             _rxPin,       (int8_t)14);
      configComplete &= getJsonValue(top["pin_tx"],             _txPin,       (int8_t)17);
      configComplete &= getJsonValue(top["tx_inverted"],        _txInverted,  false);
      configComplete &= getJsonValue(top["daliAddr"],           _daliAddr,    (int8_t)-1);

      return configComplete;
    }


    void appendConfigData() override {
      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":pin_rx',1,'DALI RX pin');"));
      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":pin_tx',1,'DALI TX pin');"));
      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":tx_inverted',1,'Invert TX — enable for single-stage inverting circuits (e.g. DIY PNP). Leave off for Waveshare Pico-DALI2 and NPN+opto boards.');"));
      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":daliAddr',1,'Short address (0\u201363) or -1 for broadcast only');"));
    }


    uint16_t getId() override { return USERMOD_ID_DALI_GEAR; }
};

// Static member definitions
int8_t DaliGearUsermod::_rxPinStatic      = -1;
int8_t DaliGearUsermod::_txPinStatic      = -1;
bool   DaliGearUsermod::_txInvertedStatic = false;

const char DaliGearUsermod::_name[]        PROGMEM = "DALIGear";
const char DaliGearUsermod::_enabled_key[] PROGMEM = "enabled";

static DaliGearUsermod dali_gear_usermod;
REGISTER_USERMOD(dali_gear_usermod);
