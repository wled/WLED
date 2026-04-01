#pragma once

// FeaturePadder is consumed by bus_wrapper.h (via PixelBusAllocator) to inject
// protocol-specific byte prefixes (e.g. TM1814 current config) before pixel data.
// It is not used directly by any WLEDpixelBus driver.

#include <stdint.h>
#include <string.h>


namespace WLEDpixelBus {

/**
 * Handle chip-specific prefix padding before the pixel data starts.
 * For example:
 * TM1814 requires a global 4-byte current configuration before the pixel data.
 * TM1914 requires a 14-byte configuration setting before the pixel data.
 */
class FeaturePadder {
public:
  FeaturePadder() : _prefixLen(0) {}

  /**
   * Set up the prefix for TM1814
   * Format is: 4 bytes of config + 4 bytes of inverted config
   * Data: R_mA, G_mA, B_mA, W_mA (typically 225 = 0xE1 each for 22.5mA)
   */
  void setupTM1814(uint8_t r_mA = 225, uint8_t g_mA = 225, uint8_t b_mA = 225, uint8_t w_mA = 225) {
    _prefix[0] = r_mA;
    _prefix[1] = g_mA;
    _prefix[2] = b_mA;
    _prefix[3] = w_mA;
    _prefix[4] = ~r_mA;
    _prefix[5] = ~g_mA;
    _prefix[6] = ~b_mA;
    _prefix[7] = ~w_mA;
    _prefixLen = 8;
  }

  /**
   * Set up the prefix for TM1914
   * Mode settings configuration.
   * Mode 1: DIN/FDIN auto-switch (Default WLED setup)
   * Format is typically 14 bytes (7 normal, 7 inverted)
   */
  void setupTM1914() {
    // Mode: DinFdinAutoSwitch
    uint8_t modeData = 0x01; 
    
    for (int i = 0; i < 7; i++) {
      _prefix[i] = modeData;
      _prefix[i + 7] = ~modeData;
    }
    _prefixLen = 14;
  }

  bool hasPrefix() const {
    return _prefixLen > 0;
  }

  uint8_t getPrefixLen() const {
    return _prefixLen;
  }

  const uint8_t* getPrefixData() const {
    return _prefix;
  }

private:
  uint8_t _prefix[16];
  uint8_t _prefixLen;
};

} // namespace WLEDpixelBus
