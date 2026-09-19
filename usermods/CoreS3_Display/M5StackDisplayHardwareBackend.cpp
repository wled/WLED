#include "wled.h"

#include "M5StackDisplayHardwareBackend.h"

#include <driver/i2c.h>

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
// CoreS3 uses AXP2101 at 0x34. After Display initialization the internal
// GPIO12/GPIO11 bus is owned by M5GFX I2C_NUM_1, matching the verified
// CoreS3 Power/Audio runtime architecture.
//
// AXP2101:
//   0x00 bit3  = battery present
//   0x01 6:5   = battery current direction (01 = charging)
//   0xA4       = fuel-gauge battery percentage
// ===========================================================

static constexpr i2c_port_t CORES3_BATTERY_I2C_PORT = I2C_NUM_1;
static constexpr uint32_t CORES3_BATTERY_I2C_FREQUENCY = 400000;
static constexpr uint8_t CORES3_AXP2101_ADDR = 0x34;
static constexpr uint8_t AXP2101_REG_PMU_STATUS1 = 0x00;
static constexpr uint8_t AXP2101_REG_PMU_STATUS2 = 0x01;
static constexpr uint8_t AXP2101_REG_BATTERY_PERCENT = 0xA4;

static bool readCoreS3Axp2101Register( uint8_t reg, uint8_t& value ) {
  auto result = lgfx::i2c::transactionWriteRead(
    CORES3_BATTERY_I2C_PORT,
    CORES3_AXP2101_ADDR,
    &reg,
    1,
    &value,
    1,
    CORES3_BATTERY_I2C_FREQUENCY
  );

  return result.has_value();
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
    Serial.printf(
      "[CoreS3_Display] Hardware probe: %s, Variant=%s, PMU=%s, IMU=%s",
      probeStateName(),
      detectedVariantName(),
      detectedPmuName(),
      detectedImuName()
    );

    if ( hardwareProbe.imuChipId > 0 ) {
      Serial.printf( " (ID=0x%02X)", hardwareProbe.imuChipId );
    }

    Serial.println();

    Serial.printf(
      "[CoreS3_Display] Detected revision: %s\n",
      detectedRevisionName()
    );

    Serial.printf(
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

      Serial.println( F( "[CoreS3_Display] ERROR: Core2 diagnostic I2C start failed" ) );

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
      Serial.printf(
        "[CoreS3_Display] Hardware profile not enabled for Display runtime: %s (%s)\n",
        profileName(),
        revisionName()
      );

      return false;
    }

    display.begin();

    display.setRotation( ACTIVE_M5STACK_HARDWARE_CAPABILITIES.displayRotation );

    screenWidth = display.width();
    screenHeight = display.height();

    Serial.printf( "[CoreS3_Display] " "Display size: %d x %d\n", screenWidth, screenHeight );

    if ( screenWidth <= 0 || screenHeight <= 0 ) {
      Serial.println( F( "[CoreS3_Display] " "ERROR: Display not detected" ) );

      return false;
    }

    touchReady = ( display.touch() != nullptr );

    Serial.printf( "[CoreS3_Display] " "Touch: %s\n", touchReady ? "READY" : "NOT FOUND" );

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
