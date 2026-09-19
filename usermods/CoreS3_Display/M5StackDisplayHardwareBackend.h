#pragma once

#include <M5GFX.h>
#include <Wire.h>

// M5Stack display hardware boundary.
//
// This class contains only board-dependent Display / Touch / brightness
// access and Core2-family read-only hardware diagnostics.
// UI/WLED behavior remains in CoreS3_Display.cpp.
struct M5StackBatteryStatus {
  bool available = false;
  bool present = false;
  bool charging = false;
  uint8_t level = 0;
};

class M5StackDisplayHardwareBackend {
  private:

  enum M5StackHardwareProbeState : uint8_t {
    M5STACK_HARDWARE_PROBE_NOT_REQUIRED = 0,
    M5STACK_HARDWARE_PROBE_NOT_RUN,
    M5STACK_HARDWARE_PROBE_COMPLETE,
    M5STACK_HARDWARE_PROBE_FAILED
  };

  enum M5StackDetectedPmu : uint8_t {
    M5STACK_DETECTED_PMU_UNKNOWN = 0,
    M5STACK_DETECTED_PMU_AXP192,
    M5STACK_DETECTED_PMU_AXP2101
  };

  enum M5StackDetectedImu : uint8_t {
    M5STACK_DETECTED_IMU_UNKNOWN = 0,
    M5STACK_DETECTED_IMU_MPU6886,
    M5STACK_DETECTED_IMU_BMI270
  };

  enum M5StackDetectedVariant : uint8_t {
    M5STACK_DETECTED_VARIANT_UNKNOWN = 0,
    M5STACK_DETECTED_VARIANT_CORE2_LEGACY,
    M5STACK_DETECTED_VARIANT_CORE2_V1_1,
    M5STACK_DETECTED_VARIANT_CORE2_AWS_LEGACY,
    M5STACK_DETECTED_VARIANT_CORE2_AWS_V1_3
  };

  struct HardwareProbeResult {
    M5StackHardwareProbeState state = M5STACK_HARDWARE_PROBE_NOT_RUN;
    M5StackDetectedPmu pmu = M5STACK_DETECTED_PMU_UNKNOWN;
    M5StackDetectedImu imu = M5STACK_DETECTED_IMU_UNKNOWN;
    M5StackDetectedVariant variant = M5STACK_DETECTED_VARIANT_UNKNOWN;

    bool address34 = false;
    bool address35 = false;
    bool address38 = false;
    bool address40 = false;
    bool address51 = false;
    bool address68 = false;

    uint8_t imuChipId = 0;
  };

  static constexpr int CORE2_INTERNAL_I2C_SDA = 21;
  static constexpr int CORE2_INTERNAL_I2C_SCL = 22;
  static constexpr uint32_t CORE2_INTERNAL_I2C_FREQUENCY = 400000;

  M5GFX& display;
  HardwareProbeResult hardwareProbe;

  bool probeI2CAddress( TwoWire& wire, uint8_t address );
  bool readI2CRegister8( TwoWire& wire, uint8_t address, uint8_t reg, uint8_t& value );

  void classifyCore2HardwareProbe( TwoWire& wire );
  void printCore2HardwareProbeResult();

  public:

  explicit M5StackDisplayHardwareBackend( M5GFX& displayRef );

  const char* profileName() const;
  const char* revisionName() const;

  bool isDisplayRuntimeEnabled() const;
  bool isCore2FamilyProfile() const;
  bool isCore2DiagnosticOnlyMode() const;

  const char* runtimeModeName() const;
  const char* portStatusName() const;

  const char* probeStateName() const;
  const char* detectedPmuName() const;
  const char* detectedImuName() const;
  const char* detectedVariantName() const;
  const char* detectedRevisionName() const;

  bool isProbeComplete() const;
  bool hasI2CAddress( uint8_t address ) const;

  void runDiagnostics();

  bool initializeDisplay(
    int16_t& screenWidth,
    int16_t& screenHeight,
    bool& touchReady
  );

  bool readTouch( int16_t& touchX, int16_t& touchY );
  void writeBrightness( uint8_t value );

  // Read the current physical LCD contents as RGB565.
  // The caller owns the buffer; this backend keeps panel access out of the
  // UI/WLED-state layer.
  bool readDisplayRgb565(
    uint16_t* pixels,
    int16_t width,
    int16_t height
  );

  // Read battery state through the active board-specific hardware backend.
  // The UI consumes only this generic status object; PMIC/register details
  // stay outside CoreS3_Display.cpp.
  bool readBatteryStatus( M5StackBatteryStatus& status );
};
