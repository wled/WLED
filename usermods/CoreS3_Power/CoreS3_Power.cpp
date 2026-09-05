#include "wled.h"
#include <Wire.h>
#include <M5GFX.h>
#include <driver/i2c.h>
#include <esp_system.h>

/*
 * CoreS3 Power Production Baseline
 *
 * Production baseline for M5Stack CoreS3 power and LED-bus runtime handling.
 *
 * LED runtime re-init protection:
 *   - WLED core re-init gate guarantees the usermod can inspect each pending
 *     bus configuration before strip.finalizeInit() replaces the old buses.
 *   - Physical digital buses are compared per primary data pin.
 *   - If any existing output shrinks or disappears, the complete old LED range
 *     is turned OFF and explicitly rendered BLACK before the bus rebuild.
 *   - WLED ON/OFF state is restored after re-init; an already-OFF state remains OFF.
 *   - Pure growth / unchanged configurations pass through without OFF/ON cycling.
 *
 * Preserved validated power behavior:
 *   - AW9523B external-5V enable writes (BOOST then BUS).
 *   - Runtime External 5V re-assert after M5GFX initialization.
 *   - AXP2101 power-key IRQ handling for Safe Shutdown.
 *   - Safe Physical Shutdown LED BLACK / suspend / cancel-and-restore logic.
 *   - AXP2101 DCDC3 Always-PWM stability setting.
 *   - Concise boot reset / PWRON / PWROFF cause reporting.
 */

// ============================================================
// Read-only CoreS3 power health bridge
//
// CoreS3_Display consumes these states for user-facing warning UX only.
// Power ownership and all register writes remain inside this usermod.
// ============================================================
static volatile bool coreS3PowerInitializationCompleteState = false;
static volatile bool coreS3PowerExternal5VReadyState = false;
static volatile bool coreS3PowerSafeShutdownMonitorReadyState = false;

// Guaranteed WLED bus re-init gate handshake.
//
// wled.cpp calls coreS3PowerShouldDeferBusReinit() immediately before it would
// consume doInitBusses and call strip.finalizeInit().
//
// prepared=false -> leave doInitBusses asserted and defer finalizeInit().
// prepared=true  -> allow WLED to consume doInitBusses and rebuild now.
static volatile bool coreS3PowerBusReinitPreparedState = false;

static inline void coreS3PowerMarkBusReinitPrepared()
{
  coreS3PowerBusReinitPreparedState = true;
}

extern "C" bool coreS3PowerShouldDeferBusReinit()
{
  // Keep normal WLED startup behavior before this usermod completes setup.
  if (!coreS3PowerInitializationCompleteState) return false;

  if (!coreS3PowerBusReinitPreparedState) return true;

  coreS3PowerBusReinitPreparedState = false;
  return false;
}

extern "C" bool coreS3PowerInitializationComplete()
{
  return coreS3PowerInitializationCompleteState;
}

extern "C" bool coreS3PowerExternal5VReady()
{
  return coreS3PowerExternal5VReadyState;
}

extern "C" bool coreS3PowerSafeShutdownMonitorReady()
{
  return coreS3PowerSafeShutdownMonitorReadyState;
}

class CoreS3PowerUsermod : public Usermod
{
private:
  static constexpr uint8_t AW9523B_ADDR = 0x58;
  static constexpr uint8_t AXP2101_ADDR = 0x34;

  static constexpr i2c_port_t CORES3_INTERNAL_I2C_PORT = I2C_NUM_1;
  static constexpr uint32_t CORES3_INTERNAL_I2C_FREQUENCY = 400000;

  static constexpr uint8_t AXP2101_REG_CHIP_ID          = 0x03;
  static constexpr uint8_t AXP2101_REG_PWRON_STATUS     = 0x20;
  static constexpr uint8_t AXP2101_REG_PWROFF_STATUS    = 0x21;

  static constexpr uint8_t AXP2101_PWROFF_PWRON_PULLDOWN_MASK = 0x01;
  static constexpr uint8_t AXP2101_PWROFF_SOFTWARE_MASK       = 0x02;
  static constexpr uint8_t AXP2101_PWROFF_PWRON_LOW_MASK      = 0x04;
  static constexpr uint8_t AXP2101_PWROFF_VSYS_UV_MASK        = 0x08;
  static constexpr uint8_t AXP2101_PWROFF_VBUS_OV_MASK        = 0x10;
  static constexpr uint8_t AXP2101_PWROFF_DCDC_UV_MASK        = 0x20;
  static constexpr uint8_t AXP2101_PWROFF_DCDC_OV_MASK        = 0x40;
  static constexpr uint8_t AXP2101_PWROFF_OVER_TEMP_MASK      = 0x80;

  static constexpr uint8_t AXP2101_REG_IRQ_ENABLE_1 = 0x41;
  static constexpr uint8_t AXP2101_REG_IRQ_STATUS_1 = 0x49;

  static constexpr uint8_t AXP2101_REG_DCDC_FORCE_PWM    = 0x81;
  // AXP2101 REG0x81: bit2=DCDC1 mode, bit4=DCDC3 mode.
  // 0=Auto PWM/PFM, 1=Always PWM. Keep all unrelated bits untouched.
  static constexpr uint8_t AXP2101_DCDC1_ALWAYS_PWM_MASK = 0x04;
  static constexpr uint8_t AXP2101_DCDC3_ALWAYS_PWM_MASK = 0x10;

  static constexpr uint8_t AXP2101_PKEY_POSITIVE_MASK = 0x01;
  static constexpr uint8_t AXP2101_PKEY_NEGATIVE_MASK = 0x02;
  static constexpr uint8_t AXP2101_PKEY_LONG_MASK     = 0x04;
  static constexpr uint8_t AXP2101_PKEY_SHORT_MASK    = 0x08;
  static constexpr uint8_t AXP2101_PKEY_EVENT_MASK    = 0x0F;

  static constexpr uint8_t AXP2101_PKEY_IRQ_ENABLE_MASK =
    AXP2101_PKEY_POSITIVE_MASK |
    AXP2101_PKEY_NEGATIVE_MASK |
    AXP2101_PKEY_LONG_MASK;

  static constexpr unsigned long POWER_KEY_POLL_INTERVAL_MS = 20;
  static constexpr unsigned long SAFE_SHUTDOWN_FALLBACK_HOLD_MS = 1500;
  static constexpr unsigned long SAFE_SHUTDOWN_BLACK_REFRESH_MS = 100;
  static constexpr unsigned long SAFE_SHUTDOWN_SHOW_WAIT_MS = 150;
  static constexpr unsigned long RUNTIME_POWER_HEALTH_POLL_MS = 10000;

  // Runtime LED-bus shrink protection timing.
  static constexpr unsigned long LED_REINIT_OFF_CONFIRM_TIMEOUT_MS = 3000;
  static constexpr unsigned long LED_REINIT_OFF_SETTLE_MS = 500;
  static constexpr unsigned long LED_REINIT_OUTPUT_WAIT_MS = 500;

  // Require multiple actual overlay/show frames that explicitly overwrite
  // the complete OLD logical range with BLACK before allowing finalizeInit().
  static constexpr uint8_t LED_REINIT_REQUIRED_BLACK_FRAMES = 3;
  static constexpr unsigned long LED_REINIT_BLACK_FRAME_TRIGGER_MS = 50;
  static constexpr unsigned long LED_REINIT_POST_BLACK_GUARD_MS = 20;

  static constexpr uint8_t REG_OUTPUT_P0 = 0x02;
  static constexpr uint8_t REG_OUTPUT_P1 = 0x03;
  static constexpr uint8_t REG_CONFIG_P0 = 0x04;
  static constexpr uint8_t REG_CONFIG_P1 = 0x05;
  static constexpr uint8_t REG_GCR       = 0x11;
  static constexpr uint8_t REG_LEDMODE_P0 = 0x12;
  static constexpr uint8_t REG_LEDMODE_P1 = 0x13;

  static constexpr uint8_t BUS_EN_MASK   = 0x02;
  static constexpr uint8_t BOOST_EN_MASK = 0x80;

  static constexpr uint8_t CORE_S3_CONFIG_P0  = 0x18;
  static constexpr uint8_t CORE_S3_CONFIG_P1  = 0x0C;
  static constexpr uint8_t CORE_S3_GCR        = 0x10;
  static constexpr uint8_t CORE_S3_LEDMODE_P0 = 0xFF;
  static constexpr uint8_t CORE_S3_LEDMODE_P1 = 0xFF;

  bool aw9523Found = false;
  bool axp2101Found = false;
  // CoreS3 external 5V state.
  bool external5VEnableAttempted = false;
  bool external5VEnableSuccess = false;
  bool runtimeExternal5VEnableAttempted = false;
  bool runtimeExternal5VEnableSuccess = false;
  bool runtimeExternal5VViolationLogged = false;
  unsigned long lastRuntimePowerHealthPoll = 0;

  bool busEnabled = false;
  bool boostEnabled = false;

  uint8_t p0Before = 0;
  uint8_t p1Before = 0;
  uint8_t p0After = 0;
  uint8_t p1After = 0;

  bool powerKeyMonitorReady = false;
  bool runtimePowerKeyMonitorAttempted = false;
  bool runtimePowerKeyBusReadyLogged = false;
  bool powerKeyPressed = false;
  bool safeShutdownBlankActive = false;
  bool safeShutdownEverTriggered = false;
  bool safeShutdownLastCanceled = false;

  uint8_t axpIrqEnableBefore = 0;
  uint8_t axpIrqEnableAfter = 0;
  uint8_t lastPowerKeyStatus = 0;
  uint8_t lastSafeShutdownTriggerStatus = 0;
  uint8_t savedLogicalBrightness = 0;

  unsigned long powerKeyPressedAt = 0;
  unsigned long lastPowerKeyPoll = 0;
  unsigned long lastShutdownBlackRefresh = 0;
  unsigned long lastRuntimeI2CFailureLog = 0;

  esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
  bool bootPowerOnStatusValid = false;
  bool bootPowerOffStatusValid = false;
  uint8_t bootPowerOnStatus = 0;
  uint8_t bootPowerOffStatus = 0;

  // DCDC3 Always-PWM stability state.
  bool dcdc3StabilityAttempted = false;
  bool dcdc3StabilityApplied = false;
  uint8_t dcdcModeBefore = 0;
  uint8_t dcdcModeAfter = 0;


  // Runtime LED shrink state machine.
  //
  // The WLED core gate keeps doInitBusses pending until either loop() or
  // handleOverlayDraw() classifies the new bus configuration. A real shrink
  // then runs OFF -> confirmed BLACK -> rebuild -> state restore.
  enum class LedShrinkSaveState : uint8_t {
    IDLE = 0,
    OFF_REQUEST_PENDING,
    WAIT_OFF,
    WAIT_REINIT_COMPLETE
  };

  LedShrinkSaveState ledShrinkSaveState = LedShrinkSaveState::IDLE;

  bool ledShrinkWasOn = false;
  uint8_t ledShrinkOriginalBrightness = 0;
  uint8_t ledShrinkOriginalBriLast = 0;

  uint16_t ledShrinkOldPhysical = 0;
  uint16_t ledShrinkTargetPhysical = 0;

  unsigned long ledShrinkOffRequestedAt = 0;
  unsigned long ledShrinkOffConfirmedAt = 0;

  // Actual BLACK frame confirmation.
  uint8_t ledShrinkBlackOverlayFrames = 0;
  unsigned long ledShrinkLastBlackTriggerAt = 0;

  const char* resetReasonText(esp_reset_reason_t reason)
  {
    switch (reason) {
      case ESP_RST_UNKNOWN:   return "UNKNOWN";
      case ESP_RST_POWERON:   return "POWERON";
      case ESP_RST_EXT:       return "EXT";
      case ESP_RST_SW:        return "SOFTWARE";
      case ESP_RST_PANIC:     return "PANIC";
      case ESP_RST_INT_WDT:   return "INT_WDT";
      case ESP_RST_TASK_WDT:  return "TASK_WDT";
      case ESP_RST_WDT:       return "WDT";
      case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
      case ESP_RST_BROWNOUT:  return "BROWNOUT";
      case ESP_RST_SDIO:      return "SDIO";
      default:                return "OTHER";
    }
  }

  void printPowerOffSource(uint8_t status)
  {
    Serial.printf("[CoreS3_Power][BOOT] AXP2101 PWROFF_STATUS=0x%02X\n", status);

    if (status == 0) {
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: NONE LATCHED / UNKNOWN"));
      return;
    }

    if (status & AXP2101_PWROFF_OVER_TEMP_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: PMIC DIE OVER TEMPERATURE"));
    if (status & AXP2101_PWROFF_DCDC_OV_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: DCDC OVER VOLTAGE"));
    if (status & AXP2101_PWROFF_DCDC_UV_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: DCDC UNDER VOLTAGE"));
    if (status & AXP2101_PWROFF_VBUS_OV_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: VBUS OVER VOLTAGE"));
    if (status & AXP2101_PWROFF_VSYS_UV_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: VSYS UNDER VOLTAGE"));
    if (status & AXP2101_PWROFF_PWRON_LOW_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: PWRON HELD LOW / EN MODE"));
    if (status & AXP2101_PWROFF_SOFTWARE_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: SOFTWARE POWER OFF"));
    if (status & AXP2101_PWROFF_PWRON_PULLDOWN_MASK)
      Serial.println(F("[CoreS3_Power][BOOT] Power-off cause: PWRON / POWER KEY PULL-DOWN"));
  }

  bool probeI2C(uint8_t address)
  {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
  }

  bool readRegister(uint8_t address, uint8_t reg, uint8_t &value)
  {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;

    uint8_t count = Wire.requestFrom(address, (uint8_t)1);
    if (count != 1 || !Wire.available()) return false;

    value = Wire.read();
    return true;
  }

  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value)
  {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
  }

  bool readRuntimeRegister(uint8_t address, uint8_t reg, uint8_t& value)
  {
    auto result = lgfx::i2c::transactionWriteRead(
      CORES3_INTERNAL_I2C_PORT,
      address,
      &reg,
      1,
      &value,
      1,
      CORES3_INTERNAL_I2C_FREQUENCY
    );
    return result.has_value();
  }

  bool writeRuntimeRegister(uint8_t address, uint8_t reg, uint8_t value)
  {
    const uint8_t data[2] = { reg, value };
    auto result = lgfx::i2c::transactionWrite(
      CORES3_INTERNAL_I2C_PORT,
      address,
      data,
      sizeof(data),
      CORES3_INTERNAL_I2C_FREQUENCY
    );
    return result.has_value();
  }

  void captureBootAxpDiagnostics()
  {
    bootPowerOnStatusValid =
      readRegister(AXP2101_ADDR, AXP2101_REG_PWRON_STATUS, bootPowerOnStatus);
    bootPowerOffStatusValid =
      readRegister(AXP2101_ADDR, AXP2101_REG_PWROFF_STATUS, bootPowerOffStatus);

    if (bootPowerOnStatusValid) {
      Serial.printf("[CoreS3_Power][BOOT] AXP2101 PWRON_STATUS=0x%02X\n", bootPowerOnStatus);
    }
    else {
      Serial.println(F("[CoreS3_Power][BOOT] AXP2101 PWRON_STATUS read FAILED"));
    }

    if (bootPowerOffStatusValid) {
      printPowerOffSource(bootPowerOffStatus);
    }
    else {
      Serial.println(F("[CoreS3_Power][BOOT] AXP2101 PWROFF_STATUS read FAILED"));
    }
  }

  bool configureAW9523()
  {
    bool ok = true;
    ok &= writeRegister(AW9523B_ADDR, REG_CONFIG_P0, CORE_S3_CONFIG_P0);
    ok &= writeRegister(AW9523B_ADDR, REG_CONFIG_P1, CORE_S3_CONFIG_P1);
    ok &= writeRegister(AW9523B_ADDR, REG_GCR, CORE_S3_GCR);
    ok &= writeRegister(AW9523B_ADDR, REG_LEDMODE_P0, CORE_S3_LEDMODE_P0);
    ok &= writeRegister(AW9523B_ADDR, REG_LEDMODE_P1, CORE_S3_LEDMODE_P1);
    return ok;
  }

  // ------------------------------------------------------------
  // CoreS3 External 5V startup enable
  //
  // CoreS3 external 5V:
  //   AW9523B P0 bit1 = BUS_EN
  //   AW9523B P1 bit7 = BOOST_EN
  //
  // Both outputs are enabled for normal CoreS3 external 5V operation.
  // Enable BOOST first, then BUS, matching the previously validated CoreS3
  // external-5V startup ordering.
  //
  // ------------------------------------------------------------
  bool enableExternal5VAtStartup()
  {
    external5VEnableAttempted = true;

    if (!readRegister(AW9523B_ADDR, REG_OUTPUT_P0, p0Before)) return false;
    if (!readRegister(AW9523B_ADDR, REG_OUTPUT_P1, p1Before)) return false;
    if (!configureAW9523()) return false;

    // Bring up the boost source first.
    uint8_t newP1 = p1Before | BOOST_EN_MASK;
    if (!writeRegister(AW9523B_ADDR, REG_OUTPUT_P1, newP1)) return false;
    delay(10);

    // Then connect the external BUS.
    uint8_t newP0 = p0Before | BUS_EN_MASK;
    if (!writeRegister(AW9523B_ADDR, REG_OUTPUT_P0, newP0)) return false;
    delay(10);

    if (!readRegister(AW9523B_ADDR, REG_OUTPUT_P0, p0After)) return false;
    if (!readRegister(AW9523B_ADDR, REG_OUTPUT_P1, p1After)) return false;

    busEnabled   = (p0After & BUS_EN_MASK) != 0;
    boostEnabled = (p1After & BOOST_EN_MASK) != 0;

    return busEnabled && boostEnabled;
  }

  // Re-assert the intended ON state after CoreS3_Display/M5GFX has taken
  // ownership of the internal I2C bus.
  bool applyRuntimeExternal5VEnable()
  {
    uint8_t p0 = 0;
    uint8_t p1 = 0;

    if (!readRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P0, p0)) return false;
    if (!readRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P1, p1)) return false;

    uint8_t newP1 = p1 | BOOST_EN_MASK;
    if (!writeRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P1, newP1)) return false;
    delay(2);

    uint8_t newP0 = p0 | BUS_EN_MASK;
    if (!writeRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P0, newP0)) return false;
    delay(2);

    uint8_t verifyP0 = 0;
    uint8_t verifyP1 = 0;

    if (!readRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P0, verifyP0)) return false;
    if (!readRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P1, verifyP1)) return false;

    p0After = verifyP0;
    p1After = verifyP1;

    busEnabled   = (verifyP0 & BUS_EN_MASK) != 0;
    boostEnabled = (verifyP1 & BOOST_EN_MASK) != 0;

    return busEnabled && boostEnabled;
  }

  void serviceRuntimeExternal5VEnable()
  {
    if (!powerKeyMonitorReady || runtimeExternal5VEnableAttempted) return;

    runtimeExternal5VEnableAttempted = true;
    runtimeExternal5VEnableSuccess = applyRuntimeExternal5VEnable();
    coreS3PowerExternal5VReadyState = runtimeExternal5VEnableSuccess;

    Serial.printf(
      "[CoreS3_Power][EXT5V] Runtime External 5V enable: %s BUS_EN=%s BOOST_EN=%s P0=0x%02X P1=0x%02X\n",
      runtimeExternal5VEnableSuccess ? "APPLIED" : "FAILED",
      busEnabled ? "ON" : "OFF",
      boostEnabled ? "ON" : "OFF",
      p0After,
      p1After
    );
  }

  bool configureRuntimePowerKeyMonitor()
  {
    runtimePowerKeyMonitorAttempted = true;
    uint8_t chipId = 0;

    if (!readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_CHIP_ID, chipId)) {
      Serial.println(F("[CoreS3_Power] Runtime power key: M5GFX I2C1 AXP2101 read FAILED"));
      return false;
    }

    if (chipId != 0x4A) {
      Serial.printf("[CoreS3_Power] Runtime power key: unexpected AXP2101 chip ID 0x%02X\n", chipId);
      return false;
    }

    if (!runtimePowerKeyBusReadyLogged) {
      runtimePowerKeyBusReadyLogged = true;
      Serial.printf("[CoreS3_Power] Runtime I2C: M5GFX I2C_NUM_1 AXP2101 READY (ID=0x%02X)\n", chipId);
    }

    if (!readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_ENABLE_1, axpIrqEnableBefore)) return false;

    uint8_t newIrqEnable = axpIrqEnableBefore | AXP2101_PKEY_IRQ_ENABLE_MASK;
    if (!writeRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_ENABLE_1, newIrqEnable)) return false;
    if (!readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_ENABLE_1, axpIrqEnableAfter)) return false;

    if ((axpIrqEnableAfter & AXP2101_PKEY_IRQ_ENABLE_MASK) != AXP2101_PKEY_IRQ_ENABLE_MASK) return false;

    uint8_t staleStatus = 0;
    if (readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_STATUS_1, staleStatus)) {
      uint8_t stalePowerKeyFlags = staleStatus & AXP2101_PKEY_EVENT_MASK;

      if (stalePowerKeyFlags != 0) {
        Serial.printf(
          "[CoreS3_Power][BOOT] Stale PKEY IRQ before clear: 0x%02X%s%s%s%s\n",
          stalePowerKeyFlags,
          (stalePowerKeyFlags & AXP2101_PKEY_POSITIVE_MASK) ? " RELEASE" : "",
          (stalePowerKeyFlags & AXP2101_PKEY_NEGATIVE_MASK) ? " PRESS" : "",
          (stalePowerKeyFlags & AXP2101_PKEY_LONG_MASK) ? " LONG" : "",
          (stalePowerKeyFlags & AXP2101_PKEY_SHORT_MASK) ? " SHORT" : ""
        );

        writeRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_STATUS_1, stalePowerKeyFlags);
      }
    }

    powerKeyPressed = false;
    powerKeyPressedAt = 0;
    lastPowerKeyPoll = millis();

    Serial.printf("[CoreS3_Power] Runtime PKEY IRQEN1: 0x%02X -> 0x%02X\n", axpIrqEnableBefore, axpIrqEnableAfter);
    Serial.println(F("[CoreS3_Power] Runtime power key monitor: ARMED on M5GFX I2C1"));
    return true;
  }

  // ------------------------------------------------------------
  // CoreS3 DCDC3 Always-PWM stability measure
  //
  // Keep DCDC1 in its existing mode and force DCDC3 to Always-PWM.
  // REG0x81 is updated read-modify-write so unrelated bits are preserved.
  // DCDC voltages/enables, charger/input/ADC settings, and REG0x23
  // DCDC OVP/UVP protection are not changed.
  // ------------------------------------------------------------
  void serviceDcdc3StabilityMode()
  {
    if (dcdc3StabilityAttempted || !powerKeyMonitorReady) return;

    dcdc3StabilityAttempted = true;

    if (!readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_DCDC_FORCE_PWM, dcdcModeBefore)) {
      Serial.println(F("[CoreS3_Power][DCDC3] REG81 read FAILED - mode unchanged"));
      return;
    }

    // R5A minimal stability change:
    // set DCDC3 Always-PWM only; preserve DCDC1 and every unrelated REG81 bit.
    const uint8_t target = dcdcModeBefore | AXP2101_DCDC3_ALWAYS_PWM_MASK;

    if (!writeRuntimeRegister(AXP2101_ADDR, AXP2101_REG_DCDC_FORCE_PWM, target)) {
      Serial.println(F("[CoreS3_Power][DCDC3] REG81 write FAILED"));
      return;
    }

    if (!readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_DCDC_FORCE_PWM, dcdcModeAfter)) {
      Serial.println(F("[CoreS3_Power][DCDC3] REG81 verify read FAILED"));
      return;
    }

    dcdc3StabilityApplied =
      (dcdcModeAfter & AXP2101_DCDC3_ALWAYS_PWM_MASK) != 0;

    Serial.printf(
      "[CoreS3_Power][DCDC3] REG81 0x%02X -> 0x%02X result=%s DCDC1=%s DCDC3=%s OVP_PROTECTION=UNCHANGED\n",
      dcdcModeBefore,
      dcdcModeAfter,
      dcdc3StabilityApplied ? "APPLIED" : "VERIFY_FAILED",
      (dcdcModeAfter & AXP2101_DCDC1_ALWAYS_PWM_MASK) ? "ALWAYS_PWM" : "AUTO",
      (dcdcModeAfter & AXP2101_DCDC3_ALWAYS_PWM_MASK) ? "ALWAYS_PWM" : "AUTO"
    );
  }

  void serviceRuntimePowerHealth()
  {
    if (!powerKeyMonitorReady || safeShutdownBlankActive) return;

    const unsigned long now = millis();
    if (
      lastRuntimePowerHealthPoll != 0 &&
      now - lastRuntimePowerHealthPoll < RUNTIME_POWER_HEALTH_POLL_MS
    ) return;

    lastRuntimePowerHealthPoll = now;

    uint8_t p0 = 0;
    uint8_t p1 = 0;
    const bool p0Ok = readRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P0, p0);
    const bool p1Ok = readRuntimeRegister(AW9523B_ADDR, REG_OUTPUT_P1, p1);

    if (!p0Ok || !p1Ok) {
      coreS3PowerExternal5VReadyState = false;
      if (!runtimeExternal5VViolationLogged) {
        runtimeExternal5VViolationLogged = true;
        Serial.println(F("[CoreS3_Power][EXT5V] WARNING: runtime state read failed"));
      }
      return;
    }

    busEnabled = (p0 & BUS_EN_MASK) != 0;
    boostEnabled = (p1 & BOOST_EN_MASK) != 0;
    const bool healthy = busEnabled && boostEnabled;
    coreS3PowerExternal5VReadyState = healthy;

    if (!healthy && !runtimeExternal5VViolationLogged) {
      runtimeExternal5VViolationLogged = true;
      Serial.printf(
        "[CoreS3_Power][EXT5V] WARNING: runtime path changed BUS_EN=%s BOOST_EN=%s P0=0x%02X P1=0x%02X\n",
        busEnabled ? "ON" : "OFF",
        boostEnabled ? "ON" : "OFF",
        p0,
        p1
      );
    }
    else if (healthy) {
      runtimeExternal5VViolationLogged = false;
    }
  }

  void waitForLedOutputComplete()
  {
    unsigned long waitStart = millis();
    while (strip.isUpdating() && millis() - waitStart < SAFE_SHUTDOWN_SHOW_WAIT_MS) {
      delay(1);
    }
  }

  void beginSafeShutdownBlank(uint8_t triggerStatus)
  {
    if (safeShutdownBlankActive) return;

    safeShutdownLastCanceled = false;
    safeShutdownEverTriggered = true;
    lastSafeShutdownTriggerStatus = triggerStatus;
    savedLogicalBrightness = bri;

    Serial.printf(
      "[CoreS3_Power] Safe shutdown trigger: IRQ=0x%02X%s%s\n",
      triggerStatus,
      (triggerStatus & AXP2101_PKEY_LONG_MASK) ? " LONG" : "",
      (triggerStatus & AXP2101_PKEY_NEGATIVE_MASK) ? " PRESS" : ""
    );

    Serial.printf(
      "[CoreS3_Power] Safe shutdown: WLED bri=%u, strip=%u -> physical 0\n",
      savedLogicalBrightness,
      strip.getBrightness()
    );

    strip.waitForIt();
    strip.setBrightness(0, true);
    strip.show();
    waitForLedOutputComplete();
    strip.suspend();
    strip.waitForIt();

    safeShutdownBlankActive = true;
    lastShutdownBlackRefresh = millis();

    Serial.println(F("[CoreS3_Power] Safe shutdown: BLACK frame sent and strip suspended"));
  }

  void maintainSafeShutdownBlank(unsigned long now)
  {
    if (!safeShutdownBlankActive || !powerKeyPressed) return;
    if (now - lastShutdownBlackRefresh < SAFE_SHUTDOWN_BLACK_REFRESH_MS) return;

    lastShutdownBlackRefresh = now;
    strip.setBrightness(0, true);
    strip.show();
    waitForLedOutputComplete();
  }

  void cancelSafeShutdownBlank()
  {
    if (!safeShutdownBlankActive) return;

    Serial.println(F("[CoreS3_Power] Safe shutdown canceled: restoring LED output"));
    strip.resume();

    uint8_t restoreBrightness = bri;
    if (restoreBrightness == 0 && savedLogicalBrightness > 0) {
      restoreBrightness = savedLogicalBrightness;
    }

    strip.setBrightness(restoreBrightness, true);
    strip.show();
    waitForLedOutputComplete();

    safeShutdownBlankActive = false;
    safeShutdownLastCanceled = true;
    strip.trigger();

    Serial.printf("[CoreS3_Power] Safe shutdown canceled: restored bri=%u\n", restoreBrightness);
  }

  uint16_t getPendingPhysicalLedCount() const
  {
    uint32_t total = 0;

    for (const auto &bc : busConfigs) {
      if (Bus::isVirtual(bc.type)) continue;
      total += bc.count;
    }

    if (total > 65535UL) total = 65535UL;
    return (uint16_t)total;
  }

  // ------------------------------------------------------------
  // Per-physical-bus shrink detection.
  //
  // Match each existing physical digital Bus to the pending BusConfig by
  // primary data pin. Any output that becomes shorter or disappears requires
  // old-range BLACK preparation before finalizeInit().
  // ------------------------------------------------------------
  bool pendingConfigShrinksAnyPhysicalBus()
  {
    const size_t oldBusCount = BusManager::getNumBusses();

    for (size_t oldIndex = 0; oldIndex < oldBusCount; ++oldIndex) {
      Bus* oldBus = BusManager::getBus(oldIndex);
      if (oldBus == nullptr) continue;
      if (!oldBus->isDigital() || oldBus->isVirtual() || oldBus->isPlaceholder()) continue;

      uint8_t oldPins[OUTPUT_MAX_PINS] = {255, 255, 255, 255, 255};
      const size_t oldPinCount = oldBus->getPins(oldPins);
      if (oldPinCount == 0 || oldPins[0] == 255) continue;

      const uint8_t oldPrimaryPin = oldPins[0];
      const uint16_t oldLength = oldBus->getLength();

      bool matchingPendingBusFound = false;
      uint16_t pendingLength = 0;

      for (const auto &bc : busConfigs) {
        if (Bus::isVirtual(bc.type)) continue;
        if (!Bus::isDigital(bc.type)) continue;
        if (bc.pins[0] != oldPrimaryPin) continue;

        matchingPendingBusFound = true;
        pendingLength = bc.count;
        break;
      }

      if (!matchingPendingBusFound) {
        Serial.printf(
          "[CoreS3_Power][LED] Bus shrink detected: pin=%u %u->removed\n",
          oldPrimaryPin,
          oldLength
        );
        return true;
      }

      if (pendingLength < oldLength) {
        Serial.printf(
          "[CoreS3_Power][LED] Bus shrink detected: pin=%u %u->%u\n",
          oldPrimaryPin,
          oldLength,
          pendingLength
        );
        return true;
      }
    }

    return false;
  }

  void captureLedShrinkReinitRequest()
  {
    if (ledShrinkSaveState != LedShrinkSaveState::IDLE) return;
    if (!doInitBusses) return;

    // A previous hook in this same main-loop may already have approved this
    // exact re-init request. Avoid duplicate pass-through logging/preparation.
    if (coreS3PowerBusReinitPreparedState) return;

    // Safe-shutdown owns LED output. Do not start the shrink OFF/ON sequence;
    // simply let normal WLED re-init proceed if a settings request arrives.
    if (safeShutdownBlankActive) {
      coreS3PowerMarkBusReinitPrepared();
      return;
    }

    const uint16_t oldPhysical = strip.getLengthPhysical();
    const uint16_t targetPhysical = getPendingPhysicalLedCount();

    // Keep an explicit safe pass-through for initialization / an empty pending
    // configuration. The normal LED Hardware UI used in this CoreS3 build has
    // real physical buses and non-zero counts, so this is only a guardrail.
    if (
      BusManager::getNumBusses() == 0 ||
      oldPhysical == 0 ||
      targetPhysical == 0
    ) {
      coreS3PowerMarkBusReinitPrepared();
      return;
    }

    // The decision is now PER OLD PHYSICAL BUS, not aggregate total length.
    // This correctly catches e.g. 5/10/15 -> 15/10/5 (total 30 -> 30).
    const bool anyPhysicalBusShrinks = pendingConfigShrinksAnyPhysicalBus();

    if (!anyPhysicalBusShrinks) {
      coreS3PowerMarkBusReinitPrepared();
      return;
    }

    ledShrinkOldPhysical = oldPhysical;
    ledShrinkTargetPhysical = targetPhysical;
    ledShrinkWasOn = (bri > 0);
    ledShrinkOriginalBrightness = bri;
    ledShrinkOriginalBriLast = briLast;
    ledShrinkBlackOverlayFrames = 0;
    ledShrinkLastBlackTriggerAt = 0;

    // Critical: prevent WLED's doInitBusses block later in this SAME main
    // loop. The pending busConfigs vector remains intact for the delayed
    // finalizeInit() call.
    doInitBusses = false;
    ledShrinkSaveState = LedShrinkSaveState::OFF_REQUEST_PENDING;

    Serial.printf(
      "[CoreS3_Power][LED] Safe bus re-init start: %u->%u state=%s\n",
      ledShrinkOldPhysical,
      ledShrinkTargetPhysical,
      ledShrinkWasOn ? "ON" : "OFF"
    );
  }

  bool waitForLedOutputIdle(unsigned long timeoutMs)
  {
    const unsigned long startedAt = millis();

    while (!BusManager::canAllShow()) {
      if (millis() - startedAt >= timeoutMs) return false;
      delay(1);
    }

    return true;
  }

  void serviceLedShrinkSaveOffCycle()
  {
    const unsigned long now = millis();

    switch (ledShrinkSaveState) {
      case LedShrinkSaveState::IDLE:
        return;

      case LedShrinkSaveState::OFF_REQUEST_PENDING:
      {
        // Keep the pending bus rebuild blocked while the OLD physical count
        // is still alive.
        doInitBusses = false;

        ledShrinkOffRequestedAt = now;
        ledShrinkOffConfirmedAt = 0;

        if (ledShrinkWasOn) {
          // Use the exact WLED power path proven manually 5/5.
          toggleOnOff();
          stateUpdated(CALL_MODE_BUTTON);
          strip.trigger();
        } else {
          ledShrinkOffConfirmedAt = now;
        }

        // Do NOT suspend yet. Normal strip.service() must render the OFF state
        // across the complete OLD LED range.
        ledShrinkSaveState = LedShrinkSaveState::WAIT_OFF;
        return;
      }

      case LedShrinkSaveState::WAIT_OFF:
      {
        doInitBusses = false;

        if (ledShrinkOffConfirmedAt == 0) {
          if (bri == 0 && strip.getBrightness() == 0) {
            ledShrinkOffConfirmedAt = now;

          } else if (now - ledShrinkOffRequestedAt >= LED_REINIT_OFF_CONFIRM_TIMEOUT_MS) {
            // Do not deadlock configuration forever. Keep rendering OFF and
            // start the conservative settle window even if the runtime
            // brightness report did not reach zero as expected.
            ledShrinkOffConfirmedAt = now;
            strip.trigger();

            Serial.printf(
              "[CoreS3_Power][LED] WARNING: bus re-init OFF timeout bri=%u strip=%u\n",
              bri,
              strip.getBrightness()
            );
          }

          return;
        }

        // Time alone is not enough. Earlier testing showed internal
        // brightness=0 did not prove that a complete OLD-range BLACK frame
        // had actually passed through WLED's final show path.
        //
        // handleOverlayDraw() explicitly replaces the full OLD logical
        // range with BLACK immediately before WLED paints the frame to
        // BusManager and calls show(). Require several such frames.
        if (ledShrinkBlackOverlayFrames < LED_REINIT_REQUIRED_BLACK_FRAMES) {
          if (now - ledShrinkLastBlackTriggerAt >= LED_REINIT_BLACK_FRAME_TRIGGER_MS) {
            ledShrinkLastBlackTriggerAt = now;
            strip.trigger();
          }
          return;
        }

        // Keep a conservative OFF settle window in addition to actual
        // overlay/show-frame evidence.
        if (now - ledShrinkOffConfirmedAt < LED_REINIT_OFF_SETTLE_MS) return;

        strip.waitForIt();

        if (!waitForLedOutputIdle(LED_REINIT_OUTPUT_WAIT_MS)) {
          Serial.println(F("[CoreS3_Power][LED] WARNING: old LED output busy; bus re-init deferred"));
          return;
        }

        // NeoPixelBus/LCD output is asynchronous; preserve old buses briefly
        // after the final confirmed BLACK show before destroying them.
        delay(LED_REINIT_POST_BLACK_GUARD_MS);

        // The complete OLD range has now been explicitly BLACK in multiple
        // real WLED show frames. Freeze drawing, then release normal rebuild.
        strip.suspend();
        strip.waitForIt();

        // The old physical range is now safely BLACK.
        // Authorize wled.cpp to consume doInitBusses and call finalizeInit().
        coreS3PowerMarkBusReinitPrepared();
        doInitBusses = true;
        ledShrinkSaveState = LedShrinkSaveState::WAIT_REINIT_COMPLETE;

        return;
      }

      case LedShrinkSaveState::WAIT_REINIT_COMPLETE:
      {
        // WLED clears doInitBusses immediately before strip.finalizeInit().
        // false here means the previous main-loop iteration completed rebuild.
        if (doInitBusses) return;

        strip.resume();

        if (ledShrinkWasOn) {
          if (bri == 0) {
            toggleOnOff();
            stateUpdated(CALL_MODE_BUTTON);
          } else {
            // Defensive fallback if another component changed WLED state.
            bri = ledShrinkOriginalBrightness;
            briLast = ledShrinkOriginalBriLast;
            stateUpdated(CALL_MODE_BUTTON);
          }
        } else {
          // Preserve original OFF state.
          bri = 0;
          briLast = ledShrinkOriginalBriLast;
          stateUpdated(CALL_MODE_BUTTON);
        }

        strip.trigger();

        Serial.printf(
          "[CoreS3_Power][LED] Safe bus re-init complete: %u->%u state=%s\n",
          ledShrinkOldPhysical,
          strip.getLengthPhysical(),
          bri > 0 ? "ON" : "OFF"
        );

        ledShrinkSaveState = LedShrinkSaveState::IDLE;
        ledShrinkWasOn = false;
        ledShrinkOriginalBrightness = 0;
        ledShrinkOriginalBriLast = 0;
        ledShrinkOldPhysical = 0;
        ledShrinkTargetPhysical = 0;
        ledShrinkOffRequestedAt = 0;
        ledShrinkOffConfirmedAt = 0;
        ledShrinkBlackOverlayFrames = 0;
        ledShrinkLastBlackTriggerAt = 0;
        return;
      }
    }
  }

  void servicePhysicalPowerKey()
  {
    unsigned long now = millis();

    if (!powerKeyMonitorReady) {
      if (!runtimePowerKeyMonitorAttempted || now - lastRuntimeI2CFailureLog >= 1000) {
        if (configureRuntimePowerKeyMonitor()) {
          powerKeyMonitorReady = true;
          coreS3PowerSafeShutdownMonitorReadyState = true;
        }
        else {
          lastRuntimeI2CFailureLog = now;
        }
      }
      return;
    }

    if (now - lastPowerKeyPoll < POWER_KEY_POLL_INTERVAL_MS) {
      maintainSafeShutdownBlank(now);
      return;
    }

    lastPowerKeyPoll = now;
    uint8_t status = 0;

    if (!readRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_STATUS_1, status)) {
      if (now - lastRuntimeI2CFailureLog >= 1000) {
        lastRuntimeI2CFailureLog = now;
        Serial.println(F("[CoreS3_Power] Runtime PKEY status read FAILED on M5GFX I2C1"));
      }
      maintainSafeShutdownBlank(now);
      return;
    }

    uint8_t powerKeyStatus = status & AXP2101_PKEY_EVENT_MASK;

    if (powerKeyStatus != 0) {
      lastPowerKeyStatus = powerKeyStatus;

      Serial.printf(
        "[CoreS3_Power] PKEY IRQ: 0x%02X%s%s%s%s\n",
        powerKeyStatus,
        (powerKeyStatus & AXP2101_PKEY_POSITIVE_MASK) ? " RELEASE" : "",
        (powerKeyStatus & AXP2101_PKEY_NEGATIVE_MASK) ? " PRESS" : "",
        (powerKeyStatus & AXP2101_PKEY_LONG_MASK) ? " LONG" : "",
        (powerKeyStatus & AXP2101_PKEY_SHORT_MASK) ? " SHORT" : ""
      );

      writeRuntimeRegister(AXP2101_ADDR, AXP2101_REG_IRQ_STATUS_1, powerKeyStatus);
    }

    if (powerKeyStatus & AXP2101_PKEY_NEGATIVE_MASK) {
      powerKeyPressed = true;
      powerKeyPressedAt = now;
    }

    if ((powerKeyStatus & AXP2101_PKEY_LONG_MASK) && !safeShutdownBlankActive) {
      powerKeyPressed = true;
      if (powerKeyPressedAt == 0) powerKeyPressedAt = now;
      beginSafeShutdownBlank(powerKeyStatus);
    }

    if (
      powerKeyPressed &&
      !safeShutdownBlankActive &&
      powerKeyPressedAt > 0 &&
      now - powerKeyPressedAt >= SAFE_SHUTDOWN_FALLBACK_HOLD_MS
    ) {
      Serial.println(F("[CoreS3_Power] Safe shutdown: PRESS timer fallback"));
      beginSafeShutdownBlank(AXP2101_PKEY_NEGATIVE_MASK);
    }

    if (powerKeyStatus & AXP2101_PKEY_POSITIVE_MASK) {
      powerKeyPressed = false;
      powerKeyPressedAt = 0;
      cancelSafeShutdownBlank();
      return;
    }

    maintainSafeShutdownBlank(now);
  }

public:
  void setup() override
  {
    coreS3PowerInitializationCompleteState = false;
    coreS3PowerExternal5VReadyState = false;
    coreS3PowerSafeShutdownMonitorReadyState = false;
    coreS3PowerBusReinitPreparedState = false;

    bootResetReason = esp_reset_reason();

    Serial.println();
    Serial.println(F("[CoreS3_Power][BUILD] CoreS3 Power v0.1.0"));
    Serial.println(F("[CoreS3_Power] Initialization start"));
    Serial.printf("[CoreS3_Power] I2C SDA=%d SCL=%d\n", i2c_sda, i2c_scl);
    Serial.printf(
      "[CoreS3_Power][BOOT] ESP reset reason: %s (%d)\n",
      resetReasonText(bootResetReason),
      (int)bootResetReason
    );

    if (i2c_sda != 12 || i2c_scl != 11) {
      Serial.println(F("[CoreS3_Power] ERROR: Invalid CoreS3 I2C pins"));
      coreS3PowerInitializationCompleteState = true;
      return;
    }

    aw9523Found = probeI2C(AW9523B_ADDR);
    axp2101Found = probeI2C(AXP2101_ADDR);

    Serial.printf("[CoreS3_Power] AW9523B (0x58): %s\n", aw9523Found ? "FOUND" : "NOT FOUND");
    Serial.printf("[CoreS3_Power] AXP2101 (0x34): %s\n", axp2101Found ? "FOUND" : "NOT FOUND");

    if (!aw9523Found || !axp2101Found) {
      Serial.println(F("[CoreS3_Power] External 5V enable canceled"));
      coreS3PowerInitializationCompleteState = true;
      return;
    }

    captureBootAxpDiagnostics();

    external5VEnableSuccess = enableExternal5VAtStartup();
    coreS3PowerExternal5VReadyState = external5VEnableSuccess;

    powerKeyMonitorReady = false;
    runtimePowerKeyMonitorAttempted = false;

    Serial.println(F("[CoreS3_Power] Power key monitor: DEFERRED until M5GFX I2C1 is active"));
    Serial.println(F("[CoreS3_Power] Safe shutdown: AXP2101 LONG IRQ primary trigger"));
    Serial.printf("[CoreS3_Power] Safe shutdown: PRESS fallback >= %lu ms\n", SAFE_SHUTDOWN_FALLBACK_HOLD_MS);
    Serial.printf("[CoreS3_Power] External 5V: %s\n", external5VEnableSuccess ? "ENABLED" : "FAILED");

    coreS3PowerInitializationCompleteState = true;

    Serial.println(F("[CoreS3_Power] LED runtime re-init: READY - per-bus shrink guard"));
    Serial.println(F("[CoreS3_Power] Initialization complete"));
    Serial.println();
  }

  void loop() override
  {
    captureLedShrinkReinitRequest();
    serviceLedShrinkSaveOffCycle();

    servicePhysicalPowerKey();
    serviceRuntimeExternal5VEnable();
    serviceDcdc3StabilityMode();
    serviceRuntimePowerHealth();
  }

  // WLED calls this after effects render and immediately before the frame
  // is painted to BusManager/show(). This is both the second re-init capture
  // point and the final BLACK override for the complete old logical range.
  void handleOverlayDraw() override
  {
    captureLedShrinkReinitRequest();
    if (
      ledShrinkSaveState == LedShrinkSaveState::WAIT_OFF &&
      bri == 0 &&
      strip.getBrightness() == 0 &&
      ledShrinkOldPhysical > 0
    ) {
      strip.setRange(0, ledShrinkOldPhysical - 1, 0);

      if (ledShrinkBlackOverlayFrames < 255) {
        ledShrinkBlackOverlayFrames++;
      }

    }
  }

  void addToJsonInfo(JsonObject& root) override
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray i2cInfo = user.createNestedArray("CoreS3 I2C");
    i2cInfo.add((i2c_sda == 12 && i2c_scl == 11) ? "GPIO12 / GPIO11 OK" : "I2C PIN ERROR");

    JsonArray awInfo = user.createNestedArray("CoreS3 AW9523B");
    awInfo.add(aw9523Found ? "Found (0x58)" : "Not found");

    JsonArray axpInfo = user.createNestedArray("CoreS3 AXP2101");
    axpInfo.add(axp2101Found ? "Found (0x34)" : "Not found");

    JsonArray stabilityInfo = user.createNestedArray("CoreS3 Power Stability");
    stabilityInfo.add("DCDC3 Always-PWM");

    JsonArray ledReinitInfo = user.createNestedArray("CoreS3 LED Runtime Reinit");
    switch (ledShrinkSaveState) {
      case LedShrinkSaveState::IDLE:
        ledReinitInfo.add("READY - per-bus shrink guard");
        break;
      case LedShrinkSaveState::OFF_REQUEST_PENDING:
        ledReinitInfo.add("Bus shrink detected - preparing OFF");
        break;
      case LedShrinkSaveState::WAIT_OFF:
        ledReinitInfo.add("Clearing old LED range");
        break;
      case LedShrinkSaveState::WAIT_REINIT_COMPLETE:
        ledReinitInfo.add("Rebuilding LED buses");
        break;
    }

    JsonArray pwmInfo = user.createNestedArray("CoreS3 DCDC3 Mode");
    if (!dcdc3StabilityAttempted) {
      pwmInfo.add("PENDING");
    }
    else if (!dcdc3StabilityApplied) {
      pwmInfo.add("FAILED / NOT APPLIED");
    }
    else {
      pwmInfo.add((dcdcModeAfter & AXP2101_DCDC1_ALWAYS_PWM_MASK) ? "DCDC1 Always PWM" : "DCDC1 Auto PWM/PFM");
      pwmInfo.add((dcdcModeAfter & AXP2101_DCDC3_ALWAYS_PWM_MASK) ? "DCDC3 Always PWM" : "DCDC3 Auto PWM/PFM");
      pwmInfo.add("OVP protection unchanged");
    }

    JsonArray shutdownInfo = user.createNestedArray("CoreS3 Safe Shutdown");
    if (!powerKeyMonitorReady) {
      shutdownInfo.add(runtimePowerKeyMonitorAttempted ? "Runtime M5GFX I2C1 monitor unavailable" : "Runtime M5GFX I2C1 monitor pending");
    }
    else if (safeShutdownBlankActive) {
      shutdownInfo.add("BLACK output active - waiting for PMIC off");
    }
    else if (safeShutdownLastCanceled) {
      shutdownInfo.add("ARMED - last shutdown hold canceled");
    }
    else if (safeShutdownEverTriggered) {
      shutdownInfo.add("ARMED - shutdown BLACK previously triggered");
    }
    else {
      shutdownInfo.add("ARMED");
    }

    shutdownInfo.add("Trigger: AXP2101 Long Press IRQ");

    char fallbackText[48];
    snprintf(fallbackText, sizeof(fallbackText), "PRESS fallback: %lu ms", SAFE_SHUTDOWN_FALLBACK_HOLD_MS);
    shutdownInfo.add(fallbackText);

    char lastIrqText[40];
    snprintf(lastIrqText, sizeof(lastIrqText), "Last IRQ status: 0x%02X", lastPowerKeyStatus);
    shutdownInfo.add(lastIrqText);

    char triggerIrqText[40];
    snprintf(triggerIrqText, sizeof(triggerIrqText), "Last BLACK trigger: 0x%02X", lastSafeShutdownTriggerStatus);
    shutdownInfo.add(triggerIrqText);

    char irqText[48];
    snprintf(irqText, sizeof(irqText), "IRQEN1 0x%02X -> 0x%02X", axpIrqEnableBefore, axpIrqEnableAfter);
    shutdownInfo.add(irqText);

    JsonArray powerInfo = user.createNestedArray("CoreS3 Ext 5V");
    if (!external5VEnableAttempted) {
      powerInfo.add("Enable not attempted");
    }
    else if (external5VEnableSuccess) {
      powerInfo.add("ENABLED - ON");
    }
    else {
      powerInfo.add("Enable FAILED");
    }

    JsonArray comparisonInfo = user.createNestedArray("CoreS3 Ext 5V Runtime");
    if (!runtimeExternal5VEnableAttempted) {
      comparisonInfo.add("Startup enabled; runtime confirmation pending");
    }
    else if (runtimeExternal5VEnableSuccess && busEnabled && boostEnabled) {
      comparisonInfo.add("ACTIVE - BUS_EN ON / BOOST_EN ON");
    }
    else {
      comparisonInfo.add("FAILED / external 5V path changed");
    }

    JsonArray busInfo = user.createNestedArray("CoreS3 BUS_EN");
    busInfo.add(busEnabled ? "ON" : "OFF");

    JsonArray boostInfo = user.createNestedArray("CoreS3 BOOST_EN");
    boostInfo.add(boostEnabled ? "ON" : "OFF");

    char p0Text[32];
    snprintf(p0Text, sizeof(p0Text), "0x%02X -> 0x%02X", p0Before, p0After);
    JsonArray p0Info = user.createNestedArray("CoreS3 AW P0");
    p0Info.add(p0Text);

    char p1Text[32];
    snprintf(p1Text, sizeof(p1Text), "0x%02X -> 0x%02X", p1Before, p1After);
    JsonArray p1Info = user.createNestedArray("CoreS3 AW P1");
    p1Info.add(p1Text);

    JsonArray resetInfo = user.createNestedArray("CoreS3 Last ESP Reset");
    resetInfo.add(resetReasonText(bootResetReason));

    JsonArray offInfo = user.createNestedArray("CoreS3 Last PMIC Off");
    if (bootPowerOffStatusValid) {
      char offText[24];
      snprintf(offText, sizeof(offText), "PWROFF 0x%02X", bootPowerOffStatus);
      offInfo.add(offText);
    }
    else {
      offInfo.add("Unavailable");
    }

    JsonArray onInfo = user.createNestedArray("CoreS3 PMIC Power On");
    if (bootPowerOnStatusValid) {
      char onText[24];
      snprintf(onText, sizeof(onText), "PWRON 0x%02X", bootPowerOnStatus);
      onInfo.add(onText);
    }
    else {
      onInfo.add("Unavailable");
    }
  }
};

static CoreS3PowerUsermod coreS3PowerUsermod;
REGISTER_USERMOD(coreS3PowerUsermod);
