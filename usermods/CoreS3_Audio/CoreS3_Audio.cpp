#include "wled.h"
#include <M5GFX.h>
#include <driver/i2c.h>

// ===========================================================
// M5Stack CoreS3 Audio Usermod
//
// CoreS3 Audio Production Baseline
//
// Responsibilities
//   - Access the CoreS3 internal I2C bus through the M5GFX
//     I2C_NUM_1 owner shared with Display / Touch.
//   - Detect the AXP2101 PMU and ES7210 audio codec.
//   - Configure and verify the built-in ES7210 dual microphones.
//   - Reserve the fixed internal audio pins used by WLED.
//   - Publish codec readiness to the AudioReactive usermod.
//   - Report hardware and integration status to WLED Info.
//
// Ownership
//   CoreS3_Audio
//     - GPIO0 / GPIO14 reservation
//     - ES7210 configuration
//     - shared internal I2C access
//     - codec-ready signal
//
//   AudioReactive
//     - I2S_NUM_1
//     - PCM sampling
//     - AGC / FFT
//     - audio-reactive effects
//
// This usermod intentionally does not install or read I2S, run FFT,
// change LED state, modify Display / Touch behavior, change power rails,
// or reinitialize the CoreS3 internal I2C bus.
//
// CoreS3 internal audio hardware
//   ES7210 I2C : 0x40
//   I2C SDA    : GPIO12
//   I2C SCL    : GPIO11
//   I2S MCLK   : GPIO0
//   I2S BCLK   : GPIO34
//   I2S WS     : GPIO33
//   I2S DATA   : GPIO14
//   I2S Port   : I2S_NUM_1
//   Channels   : Stereo (MIC1 / MIC2)
//
// M5GFX owns the CoreS3 internal I2C bus after Display initialization.
// Audio therefore uses lgfx::i2c transactions on I2C_NUM_1 and never
// calls Wire.begin(), i2c_driver_install(), lgfx::i2c::init(), or
// release() on that shared bus.
//
// The ES7210 register configuration and pin mapping follow the
// M5Stack/M5Unified CoreS3 microphone implementation.
// ===========================================================

static volatile bool coreS3AudioCodecReadyState = false;
static volatile bool coreS3AudioInitializationFinishedState = false;

extern "C" bool coreS3AudioCodecReady()
{
  return coreS3AudioCodecReadyState;
}

extern "C" bool coreS3AudioInitializationFinished()
{
  return coreS3AudioInitializationFinishedState;
}

#if defined(WLED_M5STACK_CORES3_AUDIO)
extern "C" bool coreS3AudioReactiveSourceReady();
#endif

class CoreS3AudioUsermod : public Usermod
{
private:
  // ---------------------------------------------------------
  // CoreS3 hardware constants
  // ---------------------------------------------------------

  static constexpr uint8_t ES7210_ADDR = 0x40;
  static constexpr uint8_t AXP2101_ADDR = 0x34;
  static constexpr int CORES3_I2C_SDA = 12;
  static constexpr int CORES3_I2C_SCL = 11;
  static constexpr i2c_port_t CORES3_INTERNAL_I2C_PORT = I2C_NUM_1;
  static constexpr uint32_t CORES3_INTERNAL_I2C_FREQUENCY = 400000;

  static constexpr int AUDIO_MCLK_PIN = 0;
  static constexpr int AUDIO_BCLK_PIN = 34;
  static constexpr int AUDIO_WS_PIN = 33;
  static constexpr int AUDIO_DATA_IN_PIN = 14;
  static constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;
  static constexpr unsigned long AUDIO_INIT_DELAY_MS = 2500;
  static constexpr unsigned long AUDIO_INIT_RETRY_MS = 1000;
  static constexpr uint8_t AUDIO_INIT_MAX_ATTEMPTS = 5;

  // ---------------------------------------------------------
  // Runtime state
  // ---------------------------------------------------------

  bool coreS3PinsValid = false;
  bool audioPinsReserved = false;
  bool es7210Found = false;
  bool es7210Configured = false;
  bool initializationFinished = false;
  uint8_t initAttemptCount = 0;

  unsigned long setupStartMs = 0;
  unsigned long lastInitAttemptMs = 0;

  bool axp2101Found = false;
  bool axp2101RegistersRead = false;
  uint8_t axp2101Reg90 = 0;
  uint8_t axp2101Reg93 = 0;
  uint8_t es7210ProbeReg00 = 0;

  void finishInitialization()
  {
    initializationFinished = true;
    coreS3AudioInitializationFinishedState = true;
  }

  // ---------------------------------------------------------
  // CoreS3 internal audio pin reservation
  //
  // GPIO0  = ES7210 MCLK (output)
  // GPIO14 = ES7210 DIN  (input)
  //
  // GPIO33/34 are already unavailable to WLED on this CoreS3 build
  // and appear as "System" in Pin Info, so only GPIO0/14 need an
  // explicit PinManager reservation.
  //
  // PinOwner::UM_Audioreactive is used intentionally so WLED Pin Info
  // reports these as Usermod-owned and all normal pin selectors treat
  // them as unavailable.
  // ---------------------------------------------------------

  void neutralizePersistedGpio0Button()
  {
    if (PinManager::getPinOwner(AUDIO_MCLK_PIN) == PinOwner::Button) {
      PinManager::deallocatePin(AUDIO_MCLK_PIN, PinOwner::Button);
    }

    for (auto& button : buttons) {
      if (button.pin == AUDIO_MCLK_PIN) {
        button.pin = -1;
        button.type = BTN_TYPE_NONE;
        button.pressedBefore = false;
        button.longPressed = false;
        button.pressedTime = 0;
        button.waitTime = 0;
      }
    }
  }

  bool reserveInternalAudioPins()
  {
    neutralizePersistedGpio0Button();

    if (PinManager::isPinAllocated(AUDIO_MCLK_PIN, PinOwner::UM_Audioreactive) &&
        PinManager::isPinAllocated(AUDIO_DATA_IN_PIN, PinOwner::UM_Audioreactive)) {
      return true;
    }

    if (PinManager::isPinAllocated(AUDIO_MCLK_PIN) ||
        PinManager::isPinAllocated(AUDIO_DATA_IN_PIN)) {
      Serial.printf(
        "[CoreS3_Audio] ERROR: internal audio pin conflict MCLK0=%s DIN14=%s\n",
        PinManager::getPinOwnerName(AUDIO_MCLK_PIN),
        PinManager::getPinOwnerName(AUDIO_DATA_IN_PIN)
      );
      return false;
    }

    const managed_pin_type audioPins[] = {
      { AUDIO_MCLK_PIN, true  },  // ES7210 master clock output
      { AUDIO_DATA_IN_PIN, false } // ES7210 PCM data input
    };

    if (!PinManager::allocateMultiplePins(
          audioPins,
          sizeof(audioPins) / sizeof(audioPins[0]),
          PinOwner::UM_Audioreactive
        )) {
      Serial.println(F("[CoreS3_Audio] ERROR: failed to reserve GPIO0/GPIO14 for internal audio"));
      return false;
    }

    return true;
  }

  // ---------------------------------------------------------
  // Shared CoreS3 internal I2C helpers
  //
  // IMPORTANT:
  // M5GFX owns the CoreS3 internal bus as I2C_NUM_1 on GPIO12/11.
  // Audio must use that same owner after Display initialization.
  // Do not call Wire.begin(), i2c_driver_install(), lgfx::i2c::init(),
  // release(), or otherwise reinitialize the bus here.
  // ---------------------------------------------------------

  bool readRegister(uint8_t address, uint8_t reg, uint8_t& value)
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

  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value)
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

  // ---------------------------------------------------------
  // ES7210 microphone configuration
  //
  // MIC1 / MIC2 are enabled for the two built-in microphones.
  // MIC3 / MIC4 remain powered down.
  // ---------------------------------------------------------

  bool configureES7210()
  {
    struct RegisterValue {
      uint8_t reg;
      uint8_t value;
    };

    // Reset before applying the CoreS3 microphone profile.
    if (!writeRegister(ES7210_ADDR, 0x00, 0xFF)) {
      return false;
    }

    delay(1);

    static constexpr RegisterValue registers[] = {
      { 0x00, 0x41 },
      { 0x01, 0x1F },
      { 0x06, 0x00 },
      { 0x07, 0x20 },
      { 0x08, 0x10 },
      { 0x09, 0x30 },
      { 0x0A, 0x30 },
      { 0x20, 0x0A },
      { 0x21, 0x2A },
      { 0x22, 0x0A },
      { 0x23, 0x2A },
      { 0x02, 0xC1 },
      { 0x04, 0x01 },
      { 0x05, 0x00 },
      { 0x11, 0x60 },
      { 0x40, 0x42 },
      { 0x41, 0x70 },
      { 0x42, 0x70 },
      { 0x43, 0x1B },
      { 0x44, 0x1B },
      { 0x45, 0x00 },
      { 0x46, 0x00 },
      { 0x47, 0x00 },
      { 0x48, 0x00 },
      { 0x49, 0x00 },
      { 0x4A, 0x00 },
      { 0x4B, 0x00 },
      { 0x4C, 0xFF },
      { 0x01, 0x14 }
    };

    for (const auto& item : registers) {
      if (!writeRegister(ES7210_ADDR, item.reg, item.value)) {
        return false;
      }
    }

    // Verify a few stable key values instead of assuming the writes succeeded.
    uint8_t clockControl = 0;
    uint8_t mic1Gain = 0;
    uint8_t mic2Gain = 0;
    uint8_t mic12Power = 0;
    uint8_t mic34Power = 0;

    if (!readRegister(ES7210_ADDR, 0x01, clockControl) ||
        !readRegister(ES7210_ADDR, 0x43, mic1Gain) ||
        !readRegister(ES7210_ADDR, 0x44, mic2Gain) ||
        !readRegister(ES7210_ADDR, 0x4B, mic12Power) ||
        !readRegister(ES7210_ADDR, 0x4C, mic34Power)) {
      return false;
    }

    return clockControl == 0x14 &&
           mic1Gain == 0x1B &&
           mic2Gain == 0x1B &&
           mic12Power == 0x00 &&
           mic34Power == 0xFF;
  }

  // ---------------------------------------------------------
  // Deferred initialization
  //
  // Audio initialization is delayed until all WLED usermods have
  // completed setup. This avoids depending on usermod registration
  // order while the existing CoreS3 Display / Power startup remains
  // unchanged.
  // ---------------------------------------------------------

  void attemptInitialization()
  {
    if (!audioPinsReserved) {
      Serial.println(F("[CoreS3_Audio] ERROR: audio pin reservation unavailable; codec initialization blocked"));
      finishInitialization();
      return;
    }

    initAttemptCount++;

    Serial.printf(
      "[CoreS3_Audio] Initialization attempt %u/%u\n",
      initAttemptCount,
      AUDIO_INIT_MAX_ATTEMPTS
    );

    if (i2c_sda != CORES3_I2C_SDA || i2c_scl != CORES3_I2C_SCL) {
      Serial.printf(
        "[CoreS3_Audio] ERROR: invalid CoreS3 I2C pins SDA=%d SCL=%d\n",
        i2c_sda,
        i2c_scl
      );

      coreS3PinsValid = false;
      finishInitialization();
      return;
    }

    coreS3PinsValid = true;

    // Read-only PMU diagnostics. Power-rail ownership remains
    // separate from this usermod. Reading AXP2101 register 0x90 also
    // verifies that the shared M5GFX I2C_NUM_1 bus is reachable.
    axp2101RegistersRead = false;
    axp2101Found = readRegister(AXP2101_ADDR, 0x90, axp2101Reg90);

    if (axp2101Found) {
      axp2101RegistersRead =
        readRegister(AXP2101_ADDR, 0x93, axp2101Reg93);

      if (!axp2101RegistersRead) {
        Serial.println(F("[CoreS3_Audio] WARNING: AXP2101 REG93 unavailable on M5GFX I2C1"));
      }
    }
    else {
      Serial.println(F("[CoreS3_Audio] WARNING: AXP2101 unavailable on M5GFX I2C1"));
    }

    // ES7210 register 0x00 is RESET_CTL and is safe to read.
    // This probe does not modify the codec or its power rail.
    es7210Found = readRegister(ES7210_ADDR, 0x00, es7210ProbeReg00);

    Serial.printf(
      "[CoreS3_Audio] ES7210: %s\n",
      es7210Found ? "FOUND" : "NOT FOUND"
    );

    if (!es7210Found) {
      if (initAttemptCount >= AUDIO_INIT_MAX_ATTEMPTS) {
        Serial.println(
          F("[CoreS3_Audio] ES7210 unavailable after retries; microphone initialization stopped")
        );
        finishInitialization();
      }

      return;
    }

    es7210Configured = configureES7210();

    Serial.printf(
      "[CoreS3_Audio] ES7210 configuration: %s\n",
      es7210Configured ? "VERIFIED" : "FAILED"
    );

    if (!es7210Configured) {
      finishInitialization();
      return;
    }

    coreS3AudioCodecReadyState = true;
    finishInitialization();

    Serial.println(F("[CoreS3_Audio] READY - waiting for AudioReactive I2S1"));
  }

  const char* getAudioStatusName() const
  {
    if (!initializationFinished) {
      return "INITIALIZING";
    }

    if (!audioPinsReserved) {
      return "AUDIO PIN RESERVATION ERROR";
    }

    if (!coreS3PinsValid) {
      return "I2C PIN ERROR";
    }

    if (!es7210Found) {
      return "ES7210 NOT FOUND";
    }

    if (!es7210Configured) {
      return "ES7210 CONFIG FAILED";
    }

#if defined(WLED_M5STACK_CORES3_AUDIO)
    if (coreS3AudioReactiveSourceReady()) {
      return "READY - AudioReactive";
    }

    return "CODEC READY - waiting AudioReactive";
#else
    return "AUDIOREACTIVE BUILD FLAG MISSING";
#endif
  }

public:
  // ---------------------------------------------------------
  // WLED Usermod setup
  // ---------------------------------------------------------

  void setup() override
  {
    coreS3AudioCodecReadyState = false;
    coreS3AudioInitializationFinishedState = false;
    initializationFinished = false;

    Serial.println();
    Serial.println(F("[CoreS3_Audio][BUILD] CoreS3 Audio v0.1.5"));
    Serial.println(F("[CoreS3_Audio] Initialization start"));

    audioPinsReserved = reserveInternalAudioPins();

    Serial.printf(
      "[CoreS3_Audio] Internal audio pins: %s (MCLK=GPIO0 DIN=GPIO14)\n",
      audioPinsReserved ? "READY" : "FAILED"
    );

    Serial.println(F("[CoreS3_Audio] Codec initialization deferred"));

    setupStartMs = millis();
    lastInitAttemptMs = setupStartMs;
  }

  // ---------------------------------------------------------
  // WLED Usermod loop
  // ---------------------------------------------------------

  void loop() override
  {
    const unsigned long now = millis();

    if (!initializationFinished) {
      if (now - setupStartMs < AUDIO_INIT_DELAY_MS) {
        return;
      }

      if (initAttemptCount == 0 ||
          now - lastInitAttemptMs >= AUDIO_INIT_RETRY_MS) {
        lastInitAttemptMs = now;
        attemptInitialization();
      }

      return;
    }

    // No periodic PCM work here.
    // Audio Reactive owns I2S1 sampling and FFT processing.
  }

  // ---------------------------------------------------------
  // WLED Info status
  // ---------------------------------------------------------

  void addToJsonInfo(JsonObject& root) override
  {
    JsonObject user = root["u"];

    if (user.isNull()) {
      user = root.createNestedObject("u");
    }

    JsonArray statusInfo = user.createNestedArray("CoreS3 Audio");
    statusInfo.add(getAudioStatusName());

    JsonArray i2cInfo = user.createNestedArray("CoreS3 Audio I2C");
    i2cInfo.add("M5GFX I2C1 GPIO12/GPIO11 400kHz");

    JsonArray codecInfo = user.createNestedArray("CoreS3 ES7210");

    if (!initializationFinished && !es7210Found) {
      codecInfo.add("Waiting for probe");
    }
    else if (!es7210Found) {
      codecInfo.add("Not found on M5GFX I2C1 (0x40)");
    }
    else if (!es7210Configured) {
      codecInfo.add("Found - config failed");
    }
    else {
      codecInfo.add("Found / configured / verified on I2C1");
    }

    JsonArray integrationInfo = user.createNestedArray("CoreS3 Audio Integration");

#if defined(WLED_M5STACK_CORES3_AUDIO)
    integrationInfo.add(
      coreS3AudioReactiveSourceReady()
        ? "AudioReactive source READY"
        : (coreS3AudioCodecReadyState
            ? "Codec ready / waiting I2S1 source"
            : "Waiting for ES7210 codec")
    );
#else
    integrationInfo.add("AudioReactive CoreS3 build flag missing");
#endif

    JsonArray i2sInfo = user.createNestedArray("CoreS3 Audio I2S");
    i2sInfo.add("AudioReactive owner: I2S1 Stereo 16bit 16000Hz");

    JsonArray pmuInfo = user.createNestedArray("CoreS3 Audio PMU");

    if (!initializationFinished && !axp2101Found) {
      pmuInfo.add("Waiting for probe");
    }
    else if (!axp2101Found) {
      pmuInfo.add("AXP2101 not found on M5GFX I2C1");
    }
    else if (!axp2101RegistersRead) {
      pmuInfo.add("AXP2101 found / registers unavailable");
    }
    else {
      char pmuText[48];

      snprintf(
        pmuText,
        sizeof(pmuText),
        "REG90=0x%02X REG93=0x%02X",
        axp2101Reg90,
        axp2101Reg93
      );

      pmuInfo.add(pmuText);
    }

    JsonArray pinReservationInfo =
      user.createNestedArray("CoreS3 Audio Pin Reservation");

    if (audioPinsReserved) {
      pinReservationInfo.add("GPIO0 MCLK / GPIO14 DIN RESERVED (Usermod)");
    }
    else {
      char reservationText[80];

      snprintf(
        reservationText,
        sizeof(reservationText),
        "FAILED: GPIO0=%s GPIO14=%s",
        PinManager::getPinOwnerName(AUDIO_MCLK_PIN),
        PinManager::getPinOwnerName(AUDIO_DATA_IN_PIN)
      );

      pinReservationInfo.add(reservationText);
    }

    JsonArray pinInfo = user.createNestedArray("CoreS3 Audio Pins");
    pinInfo.add("FIXED: MCLK0 BCLK34 WS33 DIN14");
  }
};

// -----------------------------------------------------------
// Register CoreS3 Audio Usermod with WLED
// -----------------------------------------------------------

static CoreS3AudioUsermod coreS3AudioUsermod;
REGISTER_USERMOD(coreS3AudioUsermod);
