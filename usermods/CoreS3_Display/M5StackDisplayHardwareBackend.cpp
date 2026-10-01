#include "wled.h"

#include "M5StackDisplayHardwareBackend.h"

#include <driver/i2c.h>

#include <cstring>

#if defined(WLED_M5STACK_CORES3) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include <soc/gpio_reg.h>
#include <soc/gpio_sig_map.h>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/panel/Panel_ILI9342.hpp>

// ===========================================================
// Phase I2C0-R1-C CoreS3 display runtime
//
// CoreS3 Display / Touch / backlight use WLED's already initialized global
// Wire / I2C0 bus. M5GFX board autodetection is intentionally bypassed so
// the usermod does not create, own, or reinitialize a separate I2C bus.
//
// The panel, touch, and backlight behavior remains aligned with the
// M5GFX 0.2.26 CoreS3 implementation while I2C access stays on global Wire.
// ===========================================================

namespace {

static constexpr uint8_t CORES3_GLOBAL_WIRE_AW9523B_ADDR = 0x58;
static constexpr uint8_t CORES3_GLOBAL_WIRE_AXP2101_ADDR = 0x34;
static constexpr uint8_t CORES3_GLOBAL_WIRE_FT5X06_ADDR  = 0x38;

static constexpr uint8_t CORES3_AW9523B_OUTPUT_P0 = 0x02;
static constexpr uint8_t CORES3_AW9523B_OUTPUT_P1 = 0x03;
static constexpr uint8_t CORES3_AW9523B_CONFIG_P0 = 0x04;
static constexpr uint8_t CORES3_AW9523B_CONFIG_P1 = 0x05;
static constexpr uint8_t CORES3_AW9523B_LEDMODE_P0 = 0x12;

static constexpr uint8_t CORES3_AW9523B_TOUCH_RST_MASK = 0x01;
static constexpr uint8_t CORES3_AW9523B_LCD_RST_MASK = 0x02;
static constexpr uint8_t CORES3_AW9523B_TOUCH_INT_MASK = 0x04;

static constexpr uint8_t CORES3_AXP2101_LDO_ENABLE_REG = 0x90;
static constexpr uint8_t CORES3_AXP2101_DLDO1_ENABLE_MASK = 0x80;
static constexpr uint8_t CORES3_AXP2101_DLDO1_VOLTAGE_REG = 0x99;

static bool coreS3GlobalWireReadBytes(
  uint8_t address,
  uint8_t reg,
  uint8_t* data,
  size_t length
) {
  if ( data == nullptr || length == 0 ) {
    return false;
  }

  Wire.beginTransmission( address );
  Wire.write( reg );

  if ( Wire.endTransmission( false ) != 0 ) {
    return false;
  }

  const size_t received = Wire.requestFrom( address, length, true );

  if ( received != length ) {
    while ( Wire.available() ) {
      Wire.read();
    }

    return false;
  }

  for ( size_t index = 0; index < length; ++index ) {
    if ( !Wire.available() ) {
      return false;
    }

    data[index] = (uint8_t)Wire.read();
  }

  return true;
}

static bool coreS3GlobalWireReadRegister8(
  uint8_t address,
  uint8_t reg,
  uint8_t& value
) {
  return coreS3GlobalWireReadBytes(
    address,
    reg,
    &value,
    1
  );
}

static bool coreS3GlobalWireWriteRegister8(
  uint8_t address,
  uint8_t reg,
  uint8_t value
) {
  Wire.beginTransmission( address );
  Wire.write( reg );
  Wire.write( value );

  return Wire.endTransmission() == 0;
}

static bool coreS3GlobalWireUpdateRegister8(
  uint8_t address,
  uint8_t reg,
  uint8_t mask,
  uint8_t value
) {
  uint8_t current = 0;

  if ( !coreS3GlobalWireReadRegister8( address, reg, current ) ) {
    return false;
  }

  const uint8_t updated =
    ( current & static_cast<uint8_t>( ~mask ) ) |
    ( value & mask );

  if ( updated == current ) {
    return true;
  }

  return coreS3GlobalWireWriteRegister8(
    address,
    reg,
    updated
  );
}


class CoreS3PanelGlobalWire : public lgfx::Panel_ILI9342 {
  public:

  CoreS3PanelGlobalWire() {
    _cfg.pin_cs = GPIO_NUM_3;
    _cfg.invert = true;
    _cfg.offset_rotation = 3;

    _rotation = 1;
  }

  protected:

  void rst_control( bool level ) override {
    coreS3GlobalWireUpdateRegister8(
      CORES3_GLOBAL_WIRE_AW9523B_ADDR,
      CORES3_AW9523B_OUTPUT_P1,
      CORES3_AW9523B_LCD_RST_MASK,
      level ? CORES3_AW9523B_LCD_RST_MASK : 0
    );
  }

  void cs_control( bool level ) override {
    lgfx::Panel_ILI9342::cs_control( level );

    // CoreS3 shares GPIO35 between LCD MISO and LCD D/C.
    // Preserve the same GPIO-matrix switching used by M5GFX 0.2.26.
    *(volatile uint32_t*)GPIO_FUNC35_OUT_SEL_CFG_REG =
      level ? FSPIQ_OUT_IDX : SIG_GPIO_OUT_IDX;

    *(volatile uint32_t*)(
      level
        ? GPIO_ENABLE1_W1TC_REG
        : GPIO_ENABLE1_W1TS_REG
    ) = 1u << ( GPIO_NUM_35 & 31 );
  }
};


class CoreS3LightGlobalWire : public lgfx::ILight {
  public:

  bool init( uint8_t brightness ) override {
    setBrightness( brightness );

    return true;
  }

  void setBrightness( uint8_t brightness ) override {
    uint8_t ldoEnable = 0;

    if (
      !coreS3GlobalWireReadRegister8(
        CORES3_GLOBAL_WIRE_AXP2101_ADDR,
        CORES3_AXP2101_LDO_ENABLE_REG,
        ldoEnable
      )
    ) {
      return;
    }

    uint8_t dldo1Voltage = 0;

    if ( brightness > 0 ) {
      ldoEnable |= CORES3_AXP2101_DLDO1_ENABLE_MASK;
      dldo1Voltage = (uint8_t)( ( brightness + 641u ) >> 5 );
    }
    else {
      ldoEnable &= (uint8_t)~CORES3_AXP2101_DLDO1_ENABLE_MASK;
    }

    if (
      !coreS3GlobalWireWriteRegister8(
        CORES3_GLOBAL_WIRE_AXP2101_ADDR,
        CORES3_AXP2101_LDO_ENABLE_REG,
        ldoEnable
      )
    ) {
      return;
    }

    coreS3GlobalWireWriteRegister8(
      CORES3_GLOBAL_WIRE_AXP2101_ADDR,
      CORES3_AXP2101_DLDO1_VOLTAGE_REG,
      dldo1Voltage
    );
  }
};


class CoreS3TouchGlobalWire : public lgfx::ITouch {
  private:

  static constexpr uint8_t FT5X06_TOUCH_DATA_REG = 0x02;
  static constexpr uint8_t FT5X06_CIPHER_REG = 0xA3;
  static constexpr uint8_t FT5X06_INTMODE_REG = 0xA4;
  static constexpr uint8_t FT5X06_POWER_REG = 0xA5;

  static constexpr uint8_t FT5X06_MONITOR = 0x01;
  static constexpr uint8_t FT5X06_SLEEP_IN = 0x03;

  static constexpr uint8_t MAX_TOUCH_POINTS = 5;
  static constexpr size_t MAX_TOUCH_FRAME_LENGTH =
    MAX_TOUCH_POINTS * 6 - 1;

  bool released = false;

  bool writeRegister( uint8_t reg, uint8_t value ) {
    return coreS3GlobalWireWriteRegister8(
      CORES3_GLOBAL_WIRE_FT5X06_ADDR,
      reg,
      value
    );
  }

  bool readRegisters(
    uint8_t reg,
    uint8_t* data,
    size_t length
  ) {
    return coreS3GlobalWireReadBytes(
      CORES3_GLOBAL_WIRE_FT5X06_ADDR,
      reg,
      data,
      length
    );
  }

  size_t readTouchFrame( uint8_t* data ) {
    uint8_t pointCount = 0;

    if (
      data == nullptr ||
      !readRegisters(
        FT5X06_TOUCH_DATA_REG,
        &pointCount,
        1
      )
    ) {
      return 0;
    }

    uint8_t points = pointCount & 0x0F;

    if ( points > MAX_TOUCH_POINTS ) {
      points = MAX_TOUCH_POINTS;
    }

    if ( points == 0 ) {
      data[0] = pointCount;

      return 1;
    }

    const size_t frameLength =
      static_cast<size_t>( points ) * 6 - 1;

    if (
      !readRegisters(
        FT5X06_TOUCH_DATA_REG,
        data,
        frameLength
      )
    ) {
      return 0;
    }

    return frameLength;
  }

  void clearAw9523Interrupt() {
    uint8_t ignored = 0;

    coreS3GlobalWireReadRegister8(
      CORES3_GLOBAL_WIRE_AW9523B_ADDR,
      0x00,
      ignored
    );

    coreS3GlobalWireReadRegister8(
      CORES3_GLOBAL_WIRE_AW9523B_ADDR,
      0x01,
      ignored
    );
  }

  bool prepareTouchHardware() {
    // CoreS3 routes FT6336U TOUCH_RST through AW9523B P0_0.
    // Reproduce the touch-specific state without creating/reinitializing
    // another I2C controller. WLED global Wire / I2C0 remains the owner.
    if (
      !coreS3GlobalWireUpdateRegister8(
        CORES3_GLOBAL_WIRE_AW9523B_ADDR,
        CORES3_AW9523B_CONFIG_P0,
        CORES3_AW9523B_TOUCH_RST_MASK,
        0
      )
    ) {
      return false;
    }

    if (
      !coreS3GlobalWireUpdateRegister8(
        CORES3_GLOBAL_WIRE_AW9523B_ADDR,
        CORES3_AW9523B_LEDMODE_P0,
        CORES3_AW9523B_TOUCH_RST_MASK,
        CORES3_AW9523B_TOUCH_RST_MASK
      )
    ) {
      return false;
    }

    if (
      !coreS3GlobalWireUpdateRegister8(
        CORES3_GLOBAL_WIRE_AW9523B_ADDR,
        CORES3_AW9523B_CONFIG_P1,
        CORES3_AW9523B_TOUCH_INT_MASK,
        CORES3_AW9523B_TOUCH_INT_MASK
      )
    ) {
      return false;
    }

    // FT6336U requires RESET low for at least 5 ms and at least 300 ms
    // between reset release and the first I2C access.
    // Pulse reset explicitly so cold, warm, and software boots behave
    // consistently while WLED global Wire / I2C0 remains active.
    if (
      !coreS3GlobalWireUpdateRegister8(
        CORES3_GLOBAL_WIRE_AW9523B_ADDR,
        CORES3_AW9523B_OUTPUT_P0,
        CORES3_AW9523B_TOUCH_RST_MASK,
        0
      )
    ) {
      return false;
    }

    delay( 10 );

    if (
      !coreS3GlobalWireUpdateRegister8(
        CORES3_GLOBAL_WIRE_AW9523B_ADDR,
        CORES3_AW9523B_OUTPUT_P0,
        CORES3_AW9523B_TOUCH_RST_MASK,
        CORES3_AW9523B_TOUCH_RST_MASK
      )
    ) {
      return false;
    }

    delay( 350 );

    return true;
  }

  public:

  bool isReady() const {
    return _inited;
  }

  CoreS3TouchGlobalWire() {
    _cfg.pin_int = GPIO_NUM_21;
    _cfg.pin_rst = -1;

    _cfg.pin_sda = GPIO_NUM_12;
    _cfg.pin_scl = GPIO_NUM_11;
    _cfg.i2c_addr = CORES3_GLOBAL_WIRE_FT5X06_ADDR;
    _cfg.i2c_port = I2C_NUM_0;
    _cfg.freq = 400000;

    _cfg.x_min = 0;
    _cfg.x_max = 319;
    _cfg.y_min = 0;
    _cfg.y_max = 239;

    _cfg.offset_rotation = 0;
    _cfg.bus_shared = false;
  }

  bool init() override {
    _inited = false;
    released = false;

    if ( _cfg.pin_int >= 0 ) {
      pinMode( _cfg.pin_int, INPUT_PULLUP );
    }

    if ( !prepareTouchHardware() ) {
      return false;
    }

    const bool workModeOk = writeRegister( 0x00, 0x00 );

    uint8_t identity[6] = { 0 };
    const bool identityReadOk = readRegisters(
      FT5X06_CIPHER_REG,
      identity,
      sizeof( identity )
    );

    const bool intModeOk = writeRegister(
      FT5X06_INTMODE_REG,
      0x00
    );

    const bool vendorIdPresent =
      identityReadOk &&
      identity[5] != 0;

    _inited =
      workModeOk &&
      identityReadOk &&
      intModeOk &&
      vendorIdPresent;

    return _inited;
  }

  void wakeup() override {
    if ( !_inited && !init() ) {
      return;
    }

    if ( _cfg.pin_int >= 0 ) {
      pinMode( _cfg.pin_int, INPUT_PULLDOWN );
      delayMicroseconds( 512 );
      pinMode( _cfg.pin_int, INPUT_PULLUP );
    }

    writeRegister(
      FT5X06_POWER_REG,
      FT5X06_MONITOR
    );
  }

  void sleep() override {
    if ( !_inited && !init() ) {
      return;
    }

    writeRegister(
      FT5X06_POWER_REG,
      FT5X06_SLEEP_IN
    );
  }

  uint_fast8_t getTouchRaw(
    lgfx::touch_point_t* points,
    uint_fast8_t count
  ) override {
    if (
      points == nullptr ||
      count == 0 ||
      !_inited
    ) {
      return 0;
    }

    if ( _cfg.pin_int >= 0 ) {
      const bool currentReleased =
        digitalRead( _cfg.pin_int ) != LOW;

      if ( released != currentReleased ) {
        released = currentReleased;

        writeRegister(
          FT5X06_INTMODE_REG,
          0x00
        );
      }

      if ( released ) {
        return 0;
      }
    }

    if ( count > MAX_TOUCH_POINTS ) {
      count = MAX_TOUCH_POINTS;
    }

    uint8_t frame[2][MAX_TOUCH_FRAME_LENGTH] = {};
    size_t frameLength[2] = {
      readTouchFrame( frame[0] ),
      0
    };

    if ( frameLength[0] <= 1 ) {
      clearAw9523Interrupt();

      return 0;
    }

    bool stableFrame = false;

    for ( uint8_t retry = 0; retry < 5; ++retry ) {
      frameLength[1] = readTouchFrame( frame[1] );

      if (
        frameLength[0] == frameLength[1] &&
        frameLength[0] > 1 &&
        memcmp(
          frame[0],
          frame[1],
          frameLength[0]
        ) == 0
      ) {
        stableFrame = true;
        break;
      }

      if (
        frameLength[1] > 0 &&
        frameLength[1] <= MAX_TOUCH_FRAME_LENGTH
      ) {
        memcpy(
          frame[0],
          frame[1],
          frameLength[1]
        );

        frameLength[0] = frameLength[1];
      }
    }

    if ( !stableFrame ) {
      clearAw9523Interrupt();

      return 0;
    }

    uint8_t detectedPoints = frame[0][0] & 0x0F;

    if ( detectedPoints > MAX_TOUCH_POINTS ) {
      detectedPoints = MAX_TOUCH_POINTS;
    }

    if ( count > detectedPoints ) {
      count = detectedPoints;
    }

    for ( uint_fast8_t index = 0; index < count; ++index ) {
      const uint8_t* data =
        &frame[0][static_cast<size_t>( index ) * 6];

      points[index].size = 1;
      points[index].x =
        ( ( data[1] & 0x0F ) << 8 ) |
        data[2];

      points[index].y =
        ( ( data[3] & 0x0F ) << 8 ) |
        data[4];

      points[index].id =
        data[3] >> 4;
    }

    return count;
  }
};


class CoreS3DisplayGlobalWireRuntime {
  private:

  lgfx::Bus_SPI bus;
  CoreS3PanelGlobalWire panel;
  CoreS3TouchGlobalWire touch;
  CoreS3LightGlobalWire light;

  public:

  CoreS3DisplayGlobalWireRuntime() {
    auto busConfig = bus.config();

    busConfig.spi_host = SPI2_HOST;
    busConfig.dma_channel = SPI_DMA_CH_AUTO;

    busConfig.freq_write = 40000000;
    busConfig.freq_read = 16000000;

    busConfig.pin_mosi = GPIO_NUM_37;
    busConfig.pin_miso = GPIO_NUM_35;
    busConfig.pin_sclk = GPIO_NUM_36;
    busConfig.pin_dc = GPIO_NUM_35;

    busConfig.spi_mode = 0;
    busConfig.spi_3wire = true;

    bus.config( busConfig );

    panel.bus( &bus );
    panel.setLight( &light );
    panel.setTouch( &touch );
  }

  bool begin( M5GFX& display ) {
    return display.init( &panel );
  }

  bool isTouchReady() const {
    return touch.isReady();
  }
};


static CoreS3DisplayGlobalWireRuntime&
coreS3DisplayGlobalWireRuntime() {
  static CoreS3DisplayGlobalWireRuntime runtime;

  return runtime;
}

} // namespace
#endif


// ===========================================================
// Compile-time M5Stack hardware profile
//
// CoreS3 remains the default verified runtime.
// Core2 / Core2 for AWS remain diagnostic-only until their
// Display / Touch / Power paths are verified on real hardware.
// ===========================================================

#define WLED_M5STACK_DISPLAY_PROFILE_CORES3        0
#define WLED_M5STACK_DISPLAY_PROFILE_CORE2         1
#define WLED_M5STACK_DISPLAY_PROFILE_CORE2_AWS     2

#define WLED_M5STACK_DISPLAY_REVISION_UNKNOWN      0
#define WLED_M5STACK_DISPLAY_REVISION_V1_0        10
#define WLED_M5STACK_DISPLAY_REVISION_V1_1        11
#define WLED_M5STACK_DISPLAY_REVISION_V1_3        13

#ifndef WLED_M5STACK_DISPLAY_PROFILE
  #define WLED_M5STACK_DISPLAY_PROFILE WLED_M5STACK_DISPLAY_PROFILE_CORES3
#endif

#ifndef WLED_M5STACK_DISPLAY_REVISION
  #define WLED_M5STACK_DISPLAY_REVISION WLED_M5STACK_DISPLAY_REVISION_UNKNOWN
#endif

#ifndef WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY
  #define WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY 0
#endif

static_assert(
  WLED_M5STACK_DISPLAY_PROFILE >= WLED_M5STACK_DISPLAY_PROFILE_CORES3 &&
  WLED_M5STACK_DISPLAY_PROFILE <= WLED_M5STACK_DISPLAY_PROFILE_CORE2_AWS,
  "Invalid WLED_M5STACK_DISPLAY_PROFILE"
);

static_assert(
  WLED_M5STACK_DISPLAY_REVISION == WLED_M5STACK_DISPLAY_REVISION_UNKNOWN ||
  WLED_M5STACK_DISPLAY_REVISION == WLED_M5STACK_DISPLAY_REVISION_V1_0 ||
  WLED_M5STACK_DISPLAY_REVISION == WLED_M5STACK_DISPLAY_REVISION_V1_1 ||
  WLED_M5STACK_DISPLAY_REVISION == WLED_M5STACK_DISPLAY_REVISION_V1_3,
  "Invalid WLED_M5STACK_DISPLAY_REVISION"
);

static_assert(
  WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY == 0 ||
  WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY == 1,
  "WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY must be 0 or 1"
);

static_assert(
  WLED_M5STACK_DISPLAY_PROFILE == WLED_M5STACK_DISPLAY_PROFILE_CORES3 ||
  WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY == 1,
  "Core2-family profiles require WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY=1"
);

static_assert(
  WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY == 0 ||
  WLED_M5STACK_DISPLAY_PROFILE == WLED_M5STACK_DISPLAY_PROFILE_CORE2 ||
  WLED_M5STACK_DISPLAY_PROFILE == WLED_M5STACK_DISPLAY_PROFILE_CORE2_AWS,
  "Diagnostic-only mode is reserved for Core2-family profiles"
);

enum M5StackDisplayHardwareProfile : uint8_t {
  M5STACK_DISPLAY_HARDWARE_CORES3 = WLED_M5STACK_DISPLAY_PROFILE_CORES3,
  M5STACK_DISPLAY_HARDWARE_CORE2 = WLED_M5STACK_DISPLAY_PROFILE_CORE2,
  M5STACK_DISPLAY_HARDWARE_CORE2_AWS = WLED_M5STACK_DISPLAY_PROFILE_CORE2_AWS
};

enum M5StackDisplayHardwareRevision : uint8_t {
  M5STACK_DISPLAY_REVISION_UNKNOWN = WLED_M5STACK_DISPLAY_REVISION_UNKNOWN,
  M5STACK_DISPLAY_REVISION_V1_0 = WLED_M5STACK_DISPLAY_REVISION_V1_0,
  M5STACK_DISPLAY_REVISION_V1_1 = WLED_M5STACK_DISPLAY_REVISION_V1_1,
  M5STACK_DISPLAY_REVISION_V1_3 = WLED_M5STACK_DISPLAY_REVISION_V1_3
};

static constexpr M5StackDisplayHardwareProfile ACTIVE_M5STACK_DISPLAY_PROFILE =
  static_cast<M5StackDisplayHardwareProfile>( WLED_M5STACK_DISPLAY_PROFILE );

static constexpr M5StackDisplayHardwareRevision ACTIVE_M5STACK_DISPLAY_REVISION =
  static_cast<M5StackDisplayHardwareRevision>( WLED_M5STACK_DISPLAY_REVISION );

static constexpr bool ACTIVE_M5STACK_CORE2_DIAGNOSTIC_ONLY =
  ( WLED_M5STACK_CORE2_DIAGNOSTIC_ONLY == 1 );

struct M5StackDisplayHardwareCapabilities {
  const char* name;
  uint8_t displayRotation;
  bool displayRuntimeEnabled;
  bool touchRuntimeEnabled;
  bool brightnessRuntimeEnabled;
  bool batteryRuntimeEnabled;
  bool diagnosticProbeEnabled;
};

static constexpr M5StackDisplayHardwareCapabilities M5STACK_HARDWARE_CAPABILITIES[] = {
  {
    "M5Stack CoreS3",
    1,
    true,
    true,
    true,
    true,
    false
  },
  {
    "M5Stack Core2",
    1,
    false,
    false,
    false,
    false,
    true
  },
  {
    "M5Stack Core2 for AWS",
    1,
    false,
    false,
    false,
    false,
    true
  }
};

static constexpr const M5StackDisplayHardwareCapabilities&
  ACTIVE_M5STACK_HARDWARE_CAPABILITIES =
    M5STACK_HARDWARE_CAPABILITIES[WLED_M5STACK_DISPLAY_PROFILE];

// ===========================================================
// CoreS3 runtime battery telemetry
//
// CoreS3 uses AXP2101 at 0x34 on the same WLED-owned global Wire / I2C0
// bus used by CoreS3 Power, Display / Touch, and ES7210 Audio.
//
// AXP2101:
//   0x00 bit3  = battery present
//   0x01 6:5   = battery current direction (01 = charging)
//   0xA4       = fuel-gauge battery percentage
// ===========================================================

static constexpr uint8_t CORES3_AXP2101_ADDR = 0x34;
static constexpr uint8_t AXP2101_REG_PMU_STATUS1 = 0x00;
static constexpr uint8_t AXP2101_REG_PMU_STATUS2 = 0x01;
static constexpr uint8_t AXP2101_REG_BATTERY_PERCENT = 0xA4;

static bool readCoreS3Axp2101Register( uint8_t reg, uint8_t& value ) {
  Wire.beginTransmission( CORES3_AXP2101_ADDR );
  Wire.write( reg );

  if ( Wire.endTransmission( false ) != 0 ) {
    return false;
  }

  const size_t received =
    Wire.requestFrom( CORES3_AXP2101_ADDR, (size_t)1, true );

  if ( received != 1 || !Wire.available() ) {
    return false;
  }

  value = (uint8_t)Wire.read();

  return true;
}

bool M5StackDisplayHardwareBackend::probeI2CAddress( TwoWire& wire, uint8_t address ) {
    wire.beginTransmission( address );

    return wire.endTransmission() == 0;
  }

bool M5StackDisplayHardwareBackend::readI2CRegister8( TwoWire& wire, uint8_t address, uint8_t reg, uint8_t& value ) {
    wire.beginTransmission( address );
    wire.write( reg );

    if ( wire.endTransmission( false ) != 0 ) {
      return false;
    }

    size_t received = wire.requestFrom( address, (size_t)1, true );

    if ( received != 1 || !wire.available() ) {
      return false;
    }

    value = (uint8_t)wire.read();

    return true;
  }

void M5StackDisplayHardwareBackend::classifyCore2HardwareProbe( TwoWire& wire ) {
    if ( hardwareProbe.address68 ) {
      uint8_t chipId = 0;

      // BMI270 CHIP_ID register 0x00 returns 0x24.
      if ( readI2CRegister8( wire, 0x68, 0x00, chipId ) && chipId == 0x24 ) {
        hardwareProbe.imu = M5STACK_DETECTED_IMU_BMI270;
        hardwareProbe.imuChipId = chipId;
      }
      else {
        // MPU6886 WHO_AM_I register 0x75 returns 0x19.
        if ( readI2CRegister8( wire, 0x68, 0x75, chipId ) && chipId == 0x19 ) {
          hardwareProbe.imu = M5STACK_DETECTED_IMU_MPU6886;
          hardwareProbe.imuChipId = chipId;
        }
      }
    }

    if ( ACTIVE_M5STACK_DISPLAY_PROFILE == M5STACK_DISPLAY_HARDWARE_CORE2_AWS ) {
      if ( hardwareProbe.address34 ) {
        // Both documented Core2 for AWS generations use AXP192.
        hardwareProbe.pmu = M5STACK_DETECTED_PMU_AXP192;
      }

      if ( hardwareProbe.imu == M5STACK_DETECTED_IMU_BMI270 ) {
        hardwareProbe.variant = M5STACK_DETECTED_VARIANT_CORE2_AWS_V1_3;
      }
      else if ( hardwareProbe.imu == M5STACK_DETECTED_IMU_MPU6886 ) {
        // Official legacy Core2 for AWS documentation identifies MPU6886,
        // but does not assign the v1.0 / v1.1 name here. Keep it generic.
        hardwareProbe.variant = M5STACK_DETECTED_VARIANT_CORE2_AWS_LEGACY;
      }
    }
    else if ( ACTIVE_M5STACK_DISPLAY_PROFILE == M5STACK_DISPLAY_HARDWARE_CORE2 ) {
      if ( hardwareProbe.address34 && hardwareProbe.address40 ) {
        // Core2 v1.1 signature: AXP2101 + INA3221.
        hardwareProbe.pmu = M5STACK_DETECTED_PMU_AXP2101;
        hardwareProbe.variant = M5STACK_DETECTED_VARIANT_CORE2_V1_1;
      }
      else if ( hardwareProbe.address34 ) {
        hardwareProbe.pmu = M5STACK_DETECTED_PMU_AXP192;
        hardwareProbe.variant = M5STACK_DETECTED_VARIANT_CORE2_LEGACY;
      }
    }
  }

void M5StackDisplayHardwareBackend::printCore2HardwareProbeResult() {
    DEBUG_PRINTF(
      "[CoreS3_Display] Hardware probe: %s, Variant=%s, PMU=%s, IMU=%s",
      probeStateName(),
      detectedVariantName(),
      detectedPmuName(),
      detectedImuName()
    );

    if ( hardwareProbe.imuChipId > 0 ) {
      DEBUG_PRINTF( " (ID=0x%02X)", hardwareProbe.imuChipId );
    }

    DEBUG_PRINTLN("");

    DEBUG_PRINTF(
      "[CoreS3_Display] Detected revision: %s\n",
      detectedRevisionName()
    );

    DEBUG_PRINTF(
      "[CoreS3_Display] Core2 I2C signature: "
      "34=%s 35=%s 38=%s 40=%s 51=%s 68=%s\n",
      hardwareProbe.address34 ? "YES" : "NO",
      hardwareProbe.address35 ? "YES" : "NO",
      hardwareProbe.address38 ? "YES" : "NO",
      hardwareProbe.address40 ? "YES" : "NO",
      hardwareProbe.address51 ? "YES" : "NO",
      hardwareProbe.address68 ? "YES" : "NO"
    );
  }

M5StackDisplayHardwareBackend::M5StackDisplayHardwareBackend( M5GFX& displayRef )
  : display( displayRef ) {
  }

const char* M5StackDisplayHardwareBackend::profileName() const {
    return ACTIVE_M5STACK_HARDWARE_CAPABILITIES.name;
  }

const char* M5StackDisplayHardwareBackend::revisionName() const {
    switch ( ACTIVE_M5STACK_DISPLAY_REVISION ) {
      case M5STACK_DISPLAY_REVISION_V1_0:
        return "v1.0";

      case M5STACK_DISPLAY_REVISION_V1_1:
        return "v1.1";

      case M5STACK_DISPLAY_REVISION_V1_3:
        return "v1.3";

      case M5STACK_DISPLAY_REVISION_UNKNOWN:
      default:
        return "UNKNOWN";
    }
  }

bool M5StackDisplayHardwareBackend::isDisplayRuntimeEnabled() const {
    return ACTIVE_M5STACK_HARDWARE_CAPABILITIES.displayRuntimeEnabled;
  }

bool M5StackDisplayHardwareBackend::isCore2FamilyProfile() const {
    return ACTIVE_M5STACK_HARDWARE_CAPABILITIES.diagnosticProbeEnabled;
  }

bool M5StackDisplayHardwareBackend::isCore2DiagnosticOnlyMode() const {
    return isCore2FamilyProfile() && ACTIVE_M5STACK_CORE2_DIAGNOSTIC_ONLY;
  }

const char* M5StackDisplayHardwareBackend::runtimeModeName() const {
    return isCore2DiagnosticOnlyMode() ? "CORE2 DIAGNOSTIC ONLY" : "DISPLAY ACTIVE";
  }

const char* M5StackDisplayHardwareBackend::portStatusName() const {
    if ( ACTIVE_M5STACK_HARDWARE_CAPABILITIES.displayRuntimeEnabled ) {
      return "CORES3 VERIFIED DISPLAY RUNTIME";
    }

    if ( isCore2DiagnosticOnlyMode() ) {
      return "CORE2 PORT PREPARED - DIAGNOSTIC ONLY";
    }

    return "HARDWARE RUNTIME BLOCKED";
  }

const char* M5StackDisplayHardwareBackend::probeStateName() const {
    switch ( hardwareProbe.state ) {
      case M5STACK_HARDWARE_PROBE_NOT_REQUIRED:
        return "NOT REQUIRED";

      case M5STACK_HARDWARE_PROBE_COMPLETE:
        return "COMPLETE";

      case M5STACK_HARDWARE_PROBE_FAILED:
        return "FAILED";

      case M5STACK_HARDWARE_PROBE_NOT_RUN:
      default:
        return "NOT RUN";
    }
  }

const char* M5StackDisplayHardwareBackend::detectedPmuName() const {
    switch ( hardwareProbe.pmu ) {
      case M5STACK_DETECTED_PMU_AXP192:
        return "AXP192 signature";

      case M5STACK_DETECTED_PMU_AXP2101:
        return "AXP2101 signature";

      case M5STACK_DETECTED_PMU_UNKNOWN:
      default:
        return "UNKNOWN";
    }
  }

const char* M5StackDisplayHardwareBackend::detectedImuName() const {
    switch ( hardwareProbe.imu ) {
      case M5STACK_DETECTED_IMU_MPU6886:
        return "MPU6886";

      case M5STACK_DETECTED_IMU_BMI270:
        return "BMI270";

      case M5STACK_DETECTED_IMU_UNKNOWN:
      default:
        return "UNKNOWN";
    }
  }

const char* M5StackDisplayHardwareBackend::detectedVariantName() const {
    switch ( hardwareProbe.variant ) {
      case M5STACK_DETECTED_VARIANT_CORE2_LEGACY:
        return "Core2 legacy signature";

      case M5STACK_DETECTED_VARIANT_CORE2_V1_1:
        return "Core2 v1.1 signature";

      case M5STACK_DETECTED_VARIANT_CORE2_AWS_LEGACY:
        return "Core2 for AWS MPU6886 generation";

      case M5STACK_DETECTED_VARIANT_CORE2_AWS_V1_3:
        return "Core2 for AWS v1.3 signature";

      case M5STACK_DETECTED_VARIANT_UNKNOWN:
      default:
        return "UNKNOWN";
    }
  }

const char* M5StackDisplayHardwareBackend::detectedRevisionName() const {
    switch ( hardwareProbe.variant ) {
      case M5STACK_DETECTED_VARIANT_CORE2_V1_1:
        return "v1.1 signature";

      case M5STACK_DETECTED_VARIANT_CORE2_AWS_V1_3:
        return "v1.3 signature";

      case M5STACK_DETECTED_VARIANT_CORE2_AWS_LEGACY:
        return "Legacy MPU6886 generation; exact revision unknown";

      case M5STACK_DETECTED_VARIANT_CORE2_LEGACY:
        return "Legacy generation; exact revision unknown";

      case M5STACK_DETECTED_VARIANT_UNKNOWN:
      default:
        return "UNKNOWN";
    }
  }

bool M5StackDisplayHardwareBackend::isProbeComplete() const {
    return hardwareProbe.state == M5STACK_HARDWARE_PROBE_COMPLETE;
  }

bool M5StackDisplayHardwareBackend::hasI2CAddress( uint8_t address ) const {
    switch ( address ) {
      case 0x34:
        return hardwareProbe.address34;

      case 0x35:
        return hardwareProbe.address35;

      case 0x38:
        return hardwareProbe.address38;

      case 0x40:
        return hardwareProbe.address40;

      case 0x51:
        return hardwareProbe.address51;

      case 0x68:
        return hardwareProbe.address68;

      default:
        return false;
    }
  }

void M5StackDisplayHardwareBackend::runDiagnostics() {
    hardwareProbe = HardwareProbeResult();

    if ( !ACTIVE_M5STACK_HARDWARE_CAPABILITIES.diagnosticProbeEnabled ) {
      hardwareProbe.state = M5STACK_HARDWARE_PROBE_NOT_REQUIRED;

      return;
    }

    TwoWire probeWire( 1 );

    if ( !probeWire.begin( CORE2_INTERNAL_I2C_SDA, CORE2_INTERNAL_I2C_SCL, CORE2_INTERNAL_I2C_FREQUENCY ) ) {
      hardwareProbe.state = M5STACK_HARDWARE_PROBE_FAILED;

      DEBUG_PRINTLN( F( "[CoreS3_Display] ERROR: Core2 diagnostic I2C start failed" ) );

      return;
    }

    delay( 10 );

    hardwareProbe.address34 = probeI2CAddress( probeWire, 0x34 );
    hardwareProbe.address35 = probeI2CAddress( probeWire, 0x35 );
    hardwareProbe.address38 = probeI2CAddress( probeWire, 0x38 );
    hardwareProbe.address40 = probeI2CAddress( probeWire, 0x40 );
    hardwareProbe.address51 = probeI2CAddress( probeWire, 0x51 );
    hardwareProbe.address68 = probeI2CAddress( probeWire, 0x68 );

    classifyCore2HardwareProbe( probeWire );

    probeWire.end();

    hardwareProbe.state = M5STACK_HARDWARE_PROBE_COMPLETE;

    printCore2HardwareProbeResult();
  }

bool M5StackDisplayHardwareBackend::initializeDisplay( int16_t& screenWidth, int16_t& screenHeight, bool& touchReady ) {
    if ( !isDisplayRuntimeEnabled() ) {
      DEBUG_PRINTF(
        "[CoreS3_Display] Hardware profile not enabled for Display runtime: %s (%s)\n",
        profileName(),
        revisionName()
      );

      return false;
    }

#if defined(WLED_M5STACK_CORES3) && defined(CONFIG_IDF_TARGET_ESP32S3)
    CoreS3DisplayGlobalWireRuntime& coreS3Runtime =
      coreS3DisplayGlobalWireRuntime();

    DEBUG_PRINTLN(
      F( "[CoreS3_Display] Display backend: global Wire / I2C0" )
    );

    if ( !coreS3Runtime.begin( display ) ) {
      DEBUG_PRINTLN(
        F( "[CoreS3_Display] ERROR: CoreS3 global Wire display initialization failed" )
      );

      return false;
    }

    touchReady = coreS3Runtime.isTouchReady();
#else
    DEBUG_PRINTLN(
      F( "[CoreS3_Display] ERROR: Display runtime has no active hardware backend" )
    );

    return false;
#endif

    display.setRotation( ACTIVE_M5STACK_HARDWARE_CAPABILITIES.displayRotation );

    screenWidth = display.width();
    screenHeight = display.height();

    DEBUG_PRINTF( "[CoreS3_Display] " "Display size: %d x %d\n", screenWidth, screenHeight );

    if ( screenWidth <= 0 || screenHeight <= 0 ) {
      DEBUG_PRINTLN( F( "[CoreS3_Display] " "ERROR: Display not detected" ) );

      return false;
    }

    DEBUG_PRINTF( "[CoreS3_Display] " "Touch: %s\n", touchReady ? "READY" : "NOT FOUND" );

    return true;
  }

bool M5StackDisplayHardwareBackend::readTouch( int16_t& touchX, int16_t& touchY ) {
    if ( !ACTIVE_M5STACK_HARDWARE_CAPABILITIES.touchRuntimeEnabled ) {
      return false;
    }

    return ( display.getTouch( &touchX, &touchY ) > 0 );
  }

void M5StackDisplayHardwareBackend::writeBrightness( uint8_t value ) {
    if ( !ACTIVE_M5STACK_HARDWARE_CAPABILITIES.brightnessRuntimeEnabled ) {
      return;
    }

    display.setBrightness( value );
  }


bool M5StackDisplayHardwareBackend::readDisplayRgb565(
  uint16_t* pixels,
  int16_t width,
  int16_t height
) {
    if (
      !ACTIVE_M5STACK_HARDWARE_CAPABILITIES.displayRuntimeEnabled ||
      pixels == nullptr ||
      width <= 0 ||
      height <= 0
    ) {
      return false;
    }

    if ( width != display.width() || height != display.height() ) {
      return false;
    }

    display.readRect(
      0,
      0,
      width,
      height,
      pixels
    );

    return true;
  }


bool M5StackDisplayHardwareBackend::readBatteryStatus( M5StackBatteryStatus& status ) {
    status = M5StackBatteryStatus();

    if ( !ACTIVE_M5STACK_HARDWARE_CAPABILITIES.batteryRuntimeEnabled ) {
      return false;
    }

    if ( ACTIVE_M5STACK_DISPLAY_PROFILE != M5STACK_DISPLAY_HARDWARE_CORES3 ) {
      return false;
    }

    uint8_t status1 = 0;
    uint8_t status2 = 0;
    uint8_t batteryPercent = 0;

    if (
      !readCoreS3Axp2101Register( AXP2101_REG_PMU_STATUS1, status1 ) ||
      !readCoreS3Axp2101Register( AXP2101_REG_PMU_STATUS2, status2 ) ||
      !readCoreS3Axp2101Register( AXP2101_REG_BATTERY_PERCENT, batteryPercent )
    ) {
      return false;
    }

    status.available = true;
    status.present = ( status1 & 0x08 ) != 0;

    const uint8_t batteryDirection = ( status2 >> 5 ) & 0x03;
    status.charging = ( batteryDirection == 0x01 );

    // AXP2101 REG A4 is a direct percentage value. Treat values outside
    // the documented 0..100 range as invalid telemetry.
    if ( batteryPercent > 100 ) {
      status.available = false;
      return false;
    }

    status.level = batteryPercent;

    return true;
  }
