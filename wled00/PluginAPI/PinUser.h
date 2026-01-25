/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

#include <array>

//--------------------------------------------------------------------------------------------------

/// Type of GPIO pin.
enum class PinType : uint8_t
{
  undefined = 0,
  Digital_in,
  Digital_out,
  Analog_in,
  PWM_out,
  I2C_scl,
  I2C_sda,
  SPI_sclk,
  SPI_mosi,
  SPI_miso,
  OneWire
};

const char *getPinName(PinType pinType);

/** Properties of a GPIO pin that a plugin wants to use.
 * @note With the current design of WLED, the PinUser (i.e. the usermod) is responsible for
 * obtaining the pin numbers from the UI. This "wanted pin to use" shall be specified here.
 * In a future optimization, this burden can be eliminated completely: \n
 * The PinUser leaves the pin number uninitialized. An appropriate pin number will be assigned by
 * the PluginManager upon registration of the PinUser. All the UI interaction will then be under
 * full control of the PluginManager; a PinUser won't have to care about that anymore. \n
 * Changes in the pin configuration (via UI) will be stored directly inside the PinUser's PinConfig
 * (since the PluginManager keeps track of them), and announced via \c onPinConfigurationChanged()
 * This callback acts as a trigger for the PinUser to re-initialize with the updated pin numbers
 * from inside its PinConfig.
 */
struct PinConfig
{
  PinConfig(PinType pinType_ = PinType::undefined, const char *pinName_ = nullptr)
      : pinType{pinType_}, pinName{pinName_} {}

  /// The designated pin number.
  uint8_t pinNr = 0xFF;

  /// Type of the requested pin.
  PinType pinType;

  /** Name of the pin (optional).
   * To be displayed in UI configuration page; it is derived from \c pinType when omitted.
   * @note Pin names are not deeply copied.
   * They must remain valid as long as the plugin is registered.
   */
  const char *pinName;

  /** Pins are marked as invalid when the plugin registration fails.
   * @note The PinUser must then assign a different pin number and try to register again.
   */
  bool isPinValid() const { return pinNr != 0xFF; }
  void invalidatePin() { pinNr = 0xFF; }
};

/// Array with multiple pin configurations.
template <std::size_t NUM_PINS>
using PinConfigs = std::array<PinConfig, NUM_PINS>;

/// Interface of a plugin that wants to use GPIO pins.
class PinUser
{
#if (0) // only with PluginManager's UI optimization for pin selection
  /// The pin configuration (from the UI) has changed. Not implemented yet.
  virtual void onPinConfigurationChanged() {}
#else
  // Intentionally empty.
  // Any pin users must just have this as base class.
#endif
};

//--------------------------------------------------------------------------------------------------
