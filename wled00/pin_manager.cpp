#include "wled.h"
#include "pin_manager.h"

#ifdef ARDUINO_ARCH_ESP32
  #ifdef bitRead
    // Arduino variants assume 32 bit values
    #undef bitRead
    #undef bitSet
    #undef bitClear
    #define bitRead(var,bit)      (((unsigned long long)(var)>>(bit))&0x1ULL)
    #define bitSet(var,bit)       ((var)|=(1ULL<<(bit)))
    #define bitClear(var,bit)     ((var)&=(~(1ULL<<(bit))))
  #endif
#endif

// Pin management state variables
#ifdef ESP8266
static uint32_t pinAlloc = 0UL;     // 1 bit per pin, we use first 17bits
#else
static uint64_t pinAlloc = 0ULL;     // 1 bit per pin, we use 50 bits on ESP32-S3
static uint16_t ledcAlloc = 0;    // up to 16 LEDC channels (WLED_MAX_ANALOG_CHANNELS)
#endif
static uint8_t i2cAllocCount = 0; // allow multiple allocation of I2C bus pins but keep track of allocations
static uint8_t spiAllocCount = 0; // allow multiple allocation of SPI bus pins but keep track of allocations
static PinOwner ownerTag[WLED_NUM_PINS] = { PinOwner::None };

/// Actual allocation/deallocation routines
bool PinManager::deallocatePin(byte gpio, PinOwner tag)
{
  if (gpio == 0xFF) return true;           // explicitly allow clients to free -1 as a no-op
  if (!isPinOk(gpio, false)) return false; // but return false for any other invalid pin

  // if a non-zero ownerTag, only allow de-allocation if the owner's tag is provided
  if ((ownerTag[gpio] != PinOwner::None) && (ownerTag[gpio] != tag)) {
    DEBUG_PRINTF_P(PSTR("PIN DEALLOC: FAIL GPIO %d allocated by 0x%02X, but attempted de-allocation by 0x%02X.\n"), gpio, static_cast<int>(ownerTag[gpio]), static_cast<int>(tag));
    return false;
  }

  bitWrite(pinAlloc, gpio, false);
  ownerTag[gpio] = PinOwner::None;
  return true;
}

// support function for deallocating multiple pins
bool PinManager::deallocateMultiplePins(const uint8_t *pinArray, byte arrayElementCount, PinOwner tag)
{
  bool shouldFail = false;
  DEBUG_PRINTLN(F("MULTIPIN DEALLOC"));
  // first verify the pins are OK and allocated by selected owner
  for (int i = 0; i < arrayElementCount; i++) {
    byte gpio = pinArray[i];
    if (gpio == 0xFF) {
      // explicit support for io -1 as a no-op (no allocation of pin),
      // as this can greatly simplify configuration arrays
      continue;
    }
    if (isPinAllocated(gpio, tag)) {
      // if the current pin is allocated by selected owner it is possible to release it
      continue;
    }
    DEBUG_PRINTF_P(PSTR("PIN DEALLOC: FAIL GPIO %d allocated by 0x%02X, but attempted de-allocation by 0x%02X.\n"), gpio, static_cast<int>(ownerTag[gpio]), static_cast<int>(tag));
    shouldFail = true;
  }
  if (shouldFail) {
    return false; // no pins deallocated
  }
  if (tag==PinOwner::HW_I2C) {
    if (i2cAllocCount && --i2cAllocCount>0) {
      // no deallocation done until last owner releases pins
      return true;
    }
  }
  if (tag==PinOwner::HW_SPI) {
    if (spiAllocCount && --spiAllocCount>0) {
      // no deallocation done until last owner releases pins
      return true;
    }
  }
  for (int i = 0; i < arrayElementCount; i++) {
    deallocatePin(pinArray[i], tag);
  }
  return true;
}

bool PinManager::deallocateMultiplePins(const managed_pin_type * mptArray, byte arrayElementCount, PinOwner tag)
{
  uint8_t pins[arrayElementCount];
  for (int i=0; i<arrayElementCount; i++) pins[i] = mptArray[i].pin;
  return deallocateMultiplePins(pins, arrayElementCount, tag);
}

bool PinManager::allocateMultiplePins(const managed_pin_type * mptArray, byte arrayElementCount, PinOwner tag)
{
  bool shouldFail = false;
  // first verify the pins are OK and not already allocated
  for (int i = 0; i < arrayElementCount; i++) {
    byte gpio = mptArray[i].pin;
    if (gpio == 0xFF) {
      // explicit support for io -1 as a no-op (no allocation of pin),
      // as this can greatly simplify configuration arrays
      continue;
    }
    if (!isPinOk(gpio, mptArray[i].isOutput)) {
      DEBUG_PRINTF_P(PSTR("PIN ALLOC: FAIL Invalid pin attempted to be allocated: GPIO %d as %s\n."), gpio, mptArray[i].isOutput ? PSTR("output"): PSTR("input"));
      shouldFail = true;
    }
    if ((tag==PinOwner::HW_I2C || tag==PinOwner::HW_SPI) && isPinAllocated(gpio, tag)) {
      // allow multiple "allocations" of HW I2C & SPI bus pins
      continue;
    } else if (isPinAllocated(gpio)) {
      DEBUG_PRINTF_P(PSTR("PIN ALLOC: FAIL GPIO %d already allocated by 0x%02X.\n"), gpio, static_cast<int>(ownerTag[gpio]));
      shouldFail = true;
    }
  }
  if (shouldFail) {
    return false;
  }

  if (tag==PinOwner::HW_I2C) i2cAllocCount++;
  if (tag==PinOwner::HW_SPI) spiAllocCount++;

  // all pins are available .. track each one
  for (int i = 0; i < arrayElementCount; i++) {
    byte gpio = mptArray[i].pin;
    if (gpio == 0xFF) {
      // allow callers to include -1 value as non-requested pin
      // as this can greatly simplify configuration arrays
      continue;
    }
    if (gpio >= WLED_NUM_PINS)
      continue; // other unexpected GPIO => avoid array bounds violation

    bitWrite(pinAlloc, gpio, true);
    ownerTag[gpio] = tag;
    DEBUG_PRINTF_P(PSTR("PIN ALLOC: Pin %d allocated by 0x%02X.\n"), gpio, static_cast<int>(tag));
  }
  DEBUG_PRINTF_P(PSTR("PIN ALLOC: 0x%014llX.\n"), (unsigned long long)pinAlloc);
  return true;
}

bool PinManager::allocatePin(byte gpio, bool output, PinOwner tag)
{
  // HW I2C & SPI pins have to be allocated using allocateMultiplePins variant since there is always SCL/SDA pair
  // DMX_INPUT pins have to be allocated using allocateMultiplePins variant since there is always RX/TX/EN triple
  if (!isPinOk(gpio, output) || (gpio >= WLED_NUM_PINS) || tag==PinOwner::HW_I2C || tag==PinOwner::HW_SPI
      || tag==PinOwner::DMX_INPUT) {
    #ifdef WLED_DEBUG
    if (gpio < 255) {  // 255 (-1) is the "not defined GPIO"
      if (!isPinOk(gpio, output)) {
        DEBUG_PRINTF_P(PSTR("PIN ALLOC: FAIL for owner 0x%02X: GPIO %d "), static_cast<int>(tag), gpio);
        if (output) DEBUG_PRINTLN(F(" cannot be used for i/o on this MCU."));
        else DEBUG_PRINTLN(F(" cannot be used as input on this MCU."));
      } else {
        DEBUG_PRINTF_P(PSTR("PIN ALLOC: FAIL GPIO %d - HW I2C & SPI pins have to be allocated using allocateMultiplePins.\n"), gpio);
      }
    }
    #endif
    return false;
  }
  if (isPinAllocated(gpio)) {
    DEBUG_PRINTF_P(PSTR("PIN ALLOC: FAIL Pin %d already allocated by 0x%02X.\n"), gpio, static_cast<int>(ownerTag[gpio]));
    return false;
  }

  bitWrite(pinAlloc, gpio, true);
  ownerTag[gpio] = tag;
  DEBUG_PRINTF_P(PSTR("PIN ALLOC: Pin %d successfully allocated by 0x%02X.\n"), gpio, static_cast<int>(ownerTag[gpio]));
  DEBUG_PRINTF_P(PSTR("PIN ALLOC: 0x%014llX.\n"), (unsigned long long)pinAlloc);

  return true;
}

// if tag is set to PinOwner::None, checks for ANY owner of the pin.
// if tag is set to any other value, checks if that tag is the current owner of the pin.
bool PinManager::isPinAllocated(byte gpio, PinOwner tag)
{
  if (!isPinOk(gpio, false)) return true;
  if ((tag != PinOwner::None) && (ownerTag[gpio] != tag)) return false;
  return bitRead(pinAlloc, gpio);
}

/* see https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html
 * The ESP32-S3 chip features 45 physical GPIO pins (GPIO0 ~ GPIO21 and GPIO26 ~ GPIO48). Each pin can be used as a general-purpose I/O
 * Strapping pins: GPIO0, GPIO3, GPIO45 and GPIO46 are strapping pins. For more infomation, please refer to ESP32-S3 datasheet.
 * Serial TX = GPIO43, RX = GPIO44; LED BUILTIN is usually GPIO39
 * USB-JTAG: GPIO 19 and 20 are used by USB-JTAG by default. In order to use them as GPIOs, USB-JTAG will be disabled by the drivers.
 * SPI0/1: GPIO26-32 are usually used for SPI flash and PSRAM and not recommended for other uses.
 * When using Octal Flash or Octal PSRAM or both, GPIO33~37 are connected to SPIIO4 ~ SPIIO7 and SPIDQS. Therefore, on boards embedded with ESP32-S3R8 / ESP32-S3R8V chip, GPIO33~37 are also not recommended for other uses.
 *
 * see https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/peripherals/adc.html
 *     https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc_oneshot.html
 * ADC1: GPIO1  - GPIO10 (channel 0..9)
 * ADC2: GPIO11 - GPIO20 (channel 0..9)
 * adc_power_acquire(): Please do not use the interrupt of GPIO36 and GPIO39 when using ADC or Wi-Fi and Bluetooth with sleep mode enabled. As a workaround, call adc_power_acquire() in the APP.
 * Since the ADC2 module is also used by the Wi-Fi, reading operation of adc2_get_raw() may fail between esp_wifi_start() and esp_wifi_stop(). Use the return code to see whether the reading is successful.
 */

// Check if supplied GPIO is ok to use
bool PinManager::isPinOk(byte gpio, bool output)
{
  if (gpio >= WLED_NUM_PINS) return false;     // catch error case, to avoid array out-of-bounds access
#ifdef ARDUINO_ARCH_ESP32
  if (digitalPinIsValid(gpio)) {
  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    // strapping pins: 2, 8, & 9
    if (gpio > 11 && gpio < 18) return false;     // 11-17 SPI FLASH
    #if ARDUINO_USB_CDC_ON_BOOT == 1 || ARDUINO_USB_DFU_ON_BOOT == 1
    if (gpio > 17 && gpio < 20) return false;     // 18-19 USB-JTAG
    #endif
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // 00 to 18 are for general use. Be careful about straping pins GPIO0 and GPIO3 - these may be pulled-up or pulled-down on your board.
    #if ARDUINO_USB_CDC_ON_BOOT == 1 || ARDUINO_USB_DFU_ON_BOOT == 1
    if (gpio > 18 && gpio < 21) return false;     // 19 + 20 = USB-JTAG. Not recommended for other uses.
    #endif
    if (gpio > 21 && gpio < 33) return false;     // 22 to 32: not connected + SPI FLASH
    #if CONFIG_ESPTOOLPY_FLASHMODE_OPI            // 33-37: never available if using _octal_ Flash (opi_opi)
    if (gpio > 32 && gpio < 38) return false;
    #endif
    #if CONFIG_SPIRAM_MODE_OCT                    // 33-37: not available if using _octal_ PSRAM (qio_opi), but free to use on _quad_ PSRAM (qio_qspi)
    if (gpio > 32 && gpio < 38) return !psramFound();
    #endif
    // 38 to 48 are for general use. Be careful about straping pins GPIO45 and GPIO46 - these may be pull-up or pulled-down on your board.
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // strapping pins: 0, 45 & 46
    if (gpio > 21 && gpio < 33) return false;     // 22 to 32: not connected + SPI FLASH
    // JTAG: GPIO39-42 are usually used for inline debugging
    // GPIO46 is input only and pulled down
  #else

    if ((strncmp_P(PSTR("ESP32-U4WDH"), ESP.getChipModel(), 11) == 0) ||    // this is the correct identifier, but....
        (strncmp_P(PSTR("ESP32-PICO-D2"), ESP.getChipModel(), 13) == 0)) {  // https://github.com/espressif/arduino-esp32/issues/10683
      // this chip has 4 MB of internal Flash and different packaging, so available pins are different!
      if (((gpio > 5) && (gpio < 9)) || (gpio == 11))
        return false;
    } else {
      // for classic ESP32 (non-mini) modules, these are the SPI flash pins
      if (gpio > 5 && gpio < 12) return false;      //SPI flash pins
    }

    if (((strncmp_P(PSTR("ESP32-PICO"), ESP.getChipModel(), 10) == 0) ||
         (strncmp_P(PSTR("ESP32-U4WDH"), ESP.getChipModel(), 11) == 0))
        && (gpio == 16 || gpio == 17)) return false; // PICO-D4/U4WDH: gpio16+17 are in use for onboard SPI FLASH
    if (gpio == 16 || gpio == 17) return !psramFound(); //PSRAM pins on ESP32 (these are IO)
  #endif
    if (output) return digitalPinCanOutput(gpio);
    else        return true;
  }
#else
  if (gpio <  6) return true;
  if (gpio < 12) return false; //SPI flash pins
  if (gpio < 17) return true;
#endif
  return false;
}

bool PinManager::isReadOnlyPin(byte gpio)
{
#ifdef ARDUINO_ARCH_ESP32
  if (gpio < WLED_NUM_PINS) return (digitalPinIsValid(gpio) && !digitalPinCanOutput(gpio));
#endif
  return false;
}

PinOwner PinManager::getPinOwner(byte gpio)
{
  if (!isPinOk(gpio, false)) return PinOwner::None;
  return ownerTag[gpio];
}

#ifdef ARDUINO_ARCH_ESP32
byte PinManager::allocateLedc(byte channels)
{
  if (channels > WLED_MAX_ANALOG_CHANNELS || channels == 0) return 255;
  unsigned ca = 0;
  for (unsigned i = 0; i < WLED_MAX_ANALOG_CHANNELS; i++) {
    if (bitRead(ledcAlloc, i)) { //found occupied pin
      ca = 0;
    } else {
      // if we have PWM CCT bus allocation (2 channels) we need to make sure both channels share the same timer
      // for phase shifting purposes (otherwise phase shifts may not be accurate)
      if (channels == 2) { // will skip odd channel for first channel for phase shifting
        if (ca == 0 && i % 2 == 0) ca++;  // even LEDC channels is 1st PWM channel
        if (ca == 1 && i % 2 == 1) ca++;  // odd LEDC channel is 2nd PWM channel
      } else
        ca++;
    }
    if (ca >= channels) { //enough free channels
      unsigned in = (i + 1) - ca;
      for (unsigned j = 0; j < ca; j++) {
        bitWrite(ledcAlloc, in+j, true);
      }
      return in;
    }
  }
  return 255; //not enough consecutive free LEDC channels
}

void PinManager::deallocateLedc(byte pos, byte channels)
{
  for (unsigned j = pos; j < pos + channels && j < WLED_MAX_ANALOG_CHANNELS; j++) {
    bitWrite(ledcAlloc, j, false);
  }
}
#endif
