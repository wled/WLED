#include "pin_manager.h"
#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
#include <soc/soc_caps.h> // WLEDMM
#endif
#endif

#ifdef WLED_DEBUG
static void DebugPrintOwnerTag(PinOwner tag)
{
  uint32_t q = static_cast<uint8_t>(tag);
  if (q) {
    DEBUG_PRINT(pinManager.getOwnerText(tag)); DEBUG_PRINT(F(" = ")); // WLEDMM
    DEBUG_PRINTF("0x%02x (%d)", q, q);
  } else {
    DEBUG_PRINT(F("(no owner)"));
  }
}
#endif

// WLEDMM begin

String PinManagerClass::getPinOwnerText(int gpio) {
  if ((gpio < 0) || (gpio == 0xFF)) return(F(""));
  //if (gpio >= GPIO_PIN_COUNT) return(F("n/a"));
  if (!isPinOk(gpio, false)) return(F("n/a"));
  if (!isPinAllocated(gpio)) return(F("./."));
  return(getOwnerText(getPinOwner(gpio)));
}

String PinManagerClass::getOwnerText(PinOwner tag) {
  switch(tag) {
    case PinOwner::None       : return(F("no owner")); break;       // unknown - no owner
    case PinOwner::DebugOut   : return(F("debug output")); break;   // 'Dbg'  == debug output always IO1
    case PinOwner::Ethernet   : return(F("Ethernet")); break;       // Ethernet
    case PinOwner::BusDigital : return(F("LEDs (digital)")); break; // Digital LEDs
    case PinOwner::BusPwm     : return(F("LEDs (PWM)")); break;     // PWM output using BusPwm
    case PinOwner::BusOnOff   : return(F("LEDs (on-off)")); break;  // 
    case PinOwner::Button     : return(F("Button")); break;         // 'Butn' == button from configuration
    case PinOwner::IR         : return(F("IR Receiver")); break;    // 'IR'   == IR receiver pin from configuration
    case PinOwner::Relay      : return(F("Relay")); break;          // 'Rly'  == Relay pin from configuration
#if defined(ARDUINO_ESP32_PICO)
    case PinOwner::SPI_RAM    : return(F("SPI FLASH")); break;      // PICO boards use gpio 16+17 for embedded flash, not for PSRAM
#else
    case PinOwner::SPI_RAM    : return(F("PSRAM")); break;          // 'SpiR' == SPI RAM (aka PSRAM)
#endif
    case PinOwner::DMX        : return(F("DMX out")); break;        // 'DMX'  == hard-coded to IO2
    case PinOwner::HW_I2C     : return(F("I2C (hw)")); break;            // 'I2C'  == hardware I2C pins (4&5 on ESP8266, 21&22 on ESP32)
    case PinOwner::HW_SPI     : return(F("SPI (hw)")); break;            // 'SPI'  == hardware (V)SPI pins (13,14&15 on ESP8266, 5,18&23 on ESP32)
    case PinOwner::DMX_INPUT  : return(F("DMX Input")); break;            
    case PinOwner::HUB75      : return(F("Hub75")); break;          // 'Hub75' == Hub75 driver 

    case PinOwner::UM_Audioreactive     : return(F("AudioReactive (UM)")); break;     // audioreactive usermod - analog or digital audio input
    case PinOwner::UM_Temperature       : return(F("Temperature (UM)")); break;       // "usermod_temperature.h"
    case PinOwner::UM_PIR               : return(F("PIR (UM)")); break;               // "usermod_PIR_sensor_switch.h"
    case PinOwner::UM_IMU               : return(F("IMU mpu6050 (UM)")); break;       // "usermod_mpu6050_imu.h"
    case PinOwner::UM_FourLineDisplay   : return(F("4Line Display (UM)")); break;     // "usermod_v2_four_line_display.h -- May use "standard" HW_I2C pins
    case PinOwner::UM_RotaryEncoderUI   : return(F("Rotary Enc. (UM)")); break;       // "usermod_v2_rotary_encoder_ui.h"
    case PinOwner::UM_MultiRelay        : return(F("Multi Relay (UM)")); break;       // "usermod_multi_relay.h"
    case PinOwner::UM_AnimatedStaircase : return(F("Anim.Staircase (UM)")); break;    // "Animated_Staircase.h"
    case PinOwner::UM_RGBRotaryEncoder  : return(F("RGB Rotary Enc. (UM)")); break;   // "rgb-rotary-encoder.h"
    case PinOwner::UM_QuinLEDAnPenta    : return(F("QuinLEDAnPenta (UM)")); break;    // "quinled-an-penta.h"
    case PinOwner::UM_BME280            : return(F("BME280 (UM)")); break;            // "usermod_bme280.h" -- Uses "standard" HW_I2C pins
    case PinOwner::UM_BH1750            : return(F("BH1750 (UM)")); break;            // "usermod_bh1750.h" -- Uses "standard" HW_I2C pins
    case PinOwner::UM_SdCard            : return(F("SD-Card (UM)")); break;           // "usermod_sd_card.h" -- Uses SPI pins
    case PinOwner::UM_PWM_OUTPUTS       : return(F("PWM Output (UM)")); break;        // "usermod_pwm_outputs.h"
    case PinOwner::UM_Battery           : return(F("Battery (UM)")); break;           // "usermod_battery.h"
    case PinOwner::UM_LDR_DUSK_DAWN     : return(F("LDR dusk/dawn (UM)")); break;     // "usermod_LDR_Dusk_Dawn_v2.h"

    case PinOwner::UM_Example      : return(F("example (UM)")); break;            // unspecified usermod
    case PinOwner::UM_Unspecified  : return(F("usermod (UM)")); break;            // unspecified usermod
  }
  return(F("other")); // should not happen
}

String PinManagerClass::getPinSpecialText(int gpio) {  // special purpose PIN info
  if ((gpio == 0xFF) || (gpio < 0)) return(F(""));      // explicitly allow -1 as a no-op

#ifdef USERMOD_AUDIOREACTIVE
  // audioreactive settings - unfortunately, these are hidden inside usermod now :-(
  // if((gpio == audioPin) && (dmType == 0)) return(F("analog audio in"));
  // if((gpio == i2ssdPin) && (dmType > 0)) return(F("I2S SD"));
  // if((gpio == i2swsPin) && (dmType > 0)) return(F("I2S WS"));
  // if((gpio == i2sckPin) && (dmType > 0) && (dmType != 5)) return(F("I2S SCK"));
  // if((gpio == mclkPin) && ((dmType == 2) || (dmType == 4))) return(F("I2S MCLK"));
  #ifdef I2S_SDPIN
    if (gpio == I2S_SDPIN) return(F("(default) I2S SD"));
  #endif
  #ifdef I2S_WSPIN
    if (gpio == I2S_WSPIN) return(F("(default) I2S WS"));
  #endif
  #ifdef I2S_CKPIN
    if (gpio == I2S_CKPIN) return(F("(default) I2S SCK"));
  #endif
  #ifdef MCLK_PIN
    if (gpio == MCLK_PIN) return(F("(default) I2S MCLK"));
  #endif
#endif

  // hardware special purpose PINS. part1 - assigned pins
  if (gpio == hardwareTX) return(F("Serial TX"));   // Serial (debug monitor) TX pin (usually GPIO1)
  if (gpio == hardwareRX) return(F("Serial RX"));   // Serial (debug monitor) RX pin (usually GPIO3)
  if (isPinAllocated(gpio)) {
    if ((gpio == i2c_sda)  && (getPinOwner(gpio) == PinOwner::HW_I2C)) return(F("I2C SDA"));
    if ((gpio == i2c_scl)  && (getPinOwner(gpio) == PinOwner::HW_I2C)) return(F("I2C SCL"));
    if ((gpio == spi_sclk) && (getPinOwner(gpio) == PinOwner::HW_SPI)) return(F("SPI SLK  / SCK"));
    if ((gpio == spi_mosi) && (getPinOwner(gpio) == PinOwner::HW_SPI)) return(F("SPI PICO / MOSI"));
    if ((gpio == spi_miso) && (getPinOwner(gpio) == PinOwner::HW_SPI)) return(F("SPI POCI / MISO"));
  }
  // MCU special PINS
  #ifdef ARDUINO_ARCH_ESP32
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      // ESP32-S3
      if (gpio > 18 && gpio < 21) return (F("USB (CDC) or JTAG"));
      #if CONFIG_ESPTOOLPY_FLASHMODE_OPI || (CONFIG_SPIRAM_MODE_OCT && defined(BOARD_HAS_PSRAM))
        if (gpio > 32 && gpio < 38)  return (F("(reserved) Octal PSRAM or Octal Flash"));
      #endif
      //if (gpio == 0 || gpio == 3 || gpio == 45 || gpio == 46) return (F("(strapping pin)"));
      #ifdef ARDUINO_TTGO_T7_S3
      // experimental: a few special pins of the T7-S3 board
      if (gpio == 2) return  (F("(reserved) _VBAT voltage monitoring"));
      if (gpio == 17) return (F("onboard LED"));
      //if (gpio == 3) return  (F("(cross-connected to pin  3-1)")); // WLEDMM experimental
      //if (gpio == 12) return (F("(cross-connected to pin 12-1)")); // WLEDMM experimental
      #endif

    #elif defined(CONFIG_IDF_TARGET_ESP32S2)
      // ESP32-S2
      //if (gpio > 38 && gpio < 43) return (F("USB (CDC) / JTAG"));  // note to self: this seems to be wrong. need to fix later.
      if (gpio == 46) return (F("pulled-down, input only"));
      //if (gpio == 0 || gpio == 45 || gpio == 46) return (F("(strapping pin)"));

    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      // ESP32-C3
      if (gpio > 17 && gpio < 20) return (F("USB (CDC) or JTAG"));
      //if (gpio == 2 || gpio == 8 || gpio == 9) return (F("(strapping pin)"));

    #else
      // "classic" ESP32, or ESP32 PICO-D4
      //if (gpio == 0 || gpio == 2 || gpio == 5) return (F("(strapping pin)"));
      //if (gpio == 12) return (F("(strapping pin - MTDI)"));
      //if (gpio == 15) return (F("(strapping pin - MTDO)"));
      //if (gpio > 11 && gpio < 16) return (F("(optional) JTAG debug probe"));
      #if defined(BOARD_HAS_PSRAM)
        if (gpio == 16 || gpio == 17) return (F("(reserved) PSRAM"));
      #endif
      #if defined(ARDUINO_TTGO_T7_V14_Mini32) || defined(ARDUINO_LOLIN_D32_PRO) || defined(ARDUINO_ADAFRUIT_FEATHER_ESP32_V2)
        if (gpio == 35) return (F("(reserved) _VBAT voltage monitoring"));  // WLEDMM experimental
      #endif
      #if (defined(ARDUINO_TTGO_T7_V14_Mini32) || defined(ARDUINO_TTGO_T7_V15_Mini32)) && defined(BOARD_HAS_PSRAM)
        if (gpio == 25) return (F("cross-connected to pin 16")); // WLEDMM experimental
        if (gpio == 27) return (F("Cross-connected to pin 17")); // WLEDMM experimental
      #endif
    #endif
  #else
    // ESP 8266
      if ((gpio == A0) || (gpio == 17)) return (F("analog-in (A0)"));  // 17 seems to be an alias for "A0" on 8266

  #endif

  #if defined(STATUSLED)
    if (gpio == STATUSLED) return(F("WLED Status LED"));
  #endif

  // hardware special purpose PINS. part2 - default pins
  if (gpio == i2c_sda)   return(F("(default) I2C SDA"));
  if (gpio == i2c_scl)   return(F("(default) I2C SCL"));
  if (gpio == spi_sclk)  return(F("(default) SPI SLK  / SCK"));
  if (gpio == spi_mosi)  return(F("(default) SPI PICO / MOSI"));
  if (gpio == spi_miso)  return(F("(default) SPI POCI / MISO"));
  //if ((gpio == spi_cs)   || ((gpio == HW_PIN_CS) && (spi_cs < 0)))         return(F("(default) SPI CS   / SS"));
#if defined(WLED_USE_SD_MMC) || defined(WLED_USE_SD_SPI) || defined(SD_ADAPTER)
  if ((gpio == HW_PIN_CSSPI)) return(F("(default) SPI CS  / SS"));  // no part of usermod default settings, currently only needed by SD_CARD usermod
#endif

  // Arduino and WLED special PINS
  #if !defined(ARDUINO_ARCH_ESP32)  // these only make sense on 8266
    #if defined(LED_BUILTIN) || defined(BUILTIN_LED)
      if (gpio == LED_BUILTIN) return(F("(onboard LED)"));
    #endif
  #endif

  #ifdef LEDPIN
    if (gpio == LEDPIN) return(F("(default) LED pin"));
  #endif

  #if defined(BTNPIN)
    if (gpio == BTNPIN) return(F("(default) Button pin"));
  #endif
  #if defined(RLYPIN)
    if (gpio == RLYPIN) return(F("(default) Relay pin"));
  #endif
  #if !defined(WLED_DISABLE_INFRARED) && defined(IRPIN)
    if (gpio == IRPIN) return(F("(default) IR receiver pin"));
  #endif

  #ifdef WLED_ENABLE_DMX
    if (gpio == 2) return(F("hardcoded DMX output pin"));
  #endif

  //
  // usermod PINS
  //
  #ifdef USERMOD_ROTARY_ENCODER_UI
    #ifdef ENCODER_DT_PIN
      if (gpio == ENCODER_DT_PIN) return(F("(default) Rotary DT  pin"));
    #else
      if (gpio == 18) return(F("(default) Rotary DT  pin"));
    #endif
    #ifdef ENCODER_CLK_PIN
      if (gpio == ENCODER_CLK_PIN) return(F("(default) Rotary CLK pin"));
    #else
      if (gpio == 5) return(F("(default) Rotary CLK pin"));
    #endif
    #ifdef ENCODER_SW_PIN
      if (gpio == ENCODER_SW_PIN) return(F("(default) Rotary SW  pin"));
    #else
      if (gpio == 19) return(F("(default) Rotary SW  pin"));
    #endif
  #endif

  #if defined(USERMOD_FOUR_LINE_DISPLAY)
    #if defined(FLD_PIN_SDA) && defined(FLD_PIN_SDA)
      if (gpio == FLD_PIN_SDA) return(F("(default) 4lines disp. I2C SDA"));
      if (gpio == FLD_PIN_SCL) return(F("(default) 4lines disp. I2C SCL"));
    #endif
    #if defined(FLD_PIN_CLOCKSPI) && defined(FLD_PIN_MOSISPI) //WLEDMM renamed from HW_PIN_DATASPI
      if (gpio == FLD_PIN_CLOCKSPI) return(F("(default) 4lines disp. SPI SCLK"));
      if (gpio == FLD_PIN_MOSISPI)  return(F("(default) 4lines disp. SPI DATA"));
    #endif
    #if defined(FLD_PIN_CS)
      if (gpio == FLD_PIN_CS) return(F("(default) 4lines disp. SPI CS"));
    #endif
    #if defined(FLD_PIN_DC) && defined(FLD_PIN_RESET)
      if (gpio == FLD_PIN_DC) return(F("(default) 4lines disp. DC"));
      if (gpio == FLD_PIN_RESET) return(F("(default) 4lines disp. RESET"));
    #endif
  #endif
  #ifdef USERMOD_DALLASTEMPERATURE
    #ifdef USERMOD_DHT_PIN
      if (gpio == USERMOD_DHT_PIN) return(F("(default) DHT temperature pin"));
    #else
      #ifdef ARDUINO_ARCH_ESP32
        if (gpio == 21) return(F("(default) DHT temperature pin"));
      #else
        if (gpio == 4)  return(F("(default) DHT temperature pin"));
      #endif
    #endif
  #endif
  #if defined(USERMOD_MPU6050_IMU)
    #ifdef MPU6050_INT_GPIO
      if (gpio == MPU6050_INT_GPIO) return(F("(default) mpu6050 INT pin"));
    #else
      if (gpio == 15) return(F("(default) mpu6050 INT pin"));
    #endif
  #endif

  // Not-OK PINS
  if (!isPinOk(gpio, false)) return(F(""));

#if 0
  // analog pin infos - experimental !
  #ifdef ARDUINO_ARCH_ESP32
  // ADC PINs - not for 8266
  if (digitalPinToAnalogChannel(gpio) >= 0) {  // ADC pin
  #ifdef SOC_ADC_CHANNEL_NUM
    if (digitalPinToAnalogChannel(gpio) < SOC_ADC_CHANNEL_NUM(0)) return(F("ADC-1")); // for ESP32-S3, ESP32-S2, ESP32-C3 
  #else
    if (digitalPinToAnalogChannel(gpio) < 8) return(F("ADC-1"));   // for classic ESP32
  #endif
    else return(F("ADC-2"));
  } 
  #endif
#endif

  return(F("")); // default - nothing special to say
}

String PinManagerClass::getPinConflicts(int gpio) {
  if ((gpio == 0xFF) || (gpio < 0)) return(F(""));      // explicitly allow -1 as a no-op
  if (!isPinOk(gpio, false)) return(F(""));             // invalid GPIO

  if ((ownerConflict[gpio] == PinOwner::None) || (ownerTag[gpio] == ownerConflict[gpio])) { // no conflict, or "fake" conflict with current owner
    return(F(""));             // no conflict fot this GPIO
  } else {                     // found previous conflict!
    return String("!! Conflict with ") + getOwnerText(ownerConflict[gpio]) + String(" !!");
  }
}
// WLEDMM end

/// Actual allocation/deallocation routines
bool PinManagerClass::deallocatePin(byte gpio, PinOwner tag)
{
  if (gpio == 0xFF) return true;           // explicitly allow clients to free -1 as a no-op
  if (!isPinOk(gpio, false)) return false; // but return false for any other invalid pin

  // if a non-zero ownerTag, only allow de-allocation if the owner's tag is provided
  if ((ownerTag[gpio] != PinOwner::None) && (ownerTag[gpio] != tag)) {
    #ifdef WLED_DEBUG
    DEBUG_PRINT(F("PIN DEALLOC: IO "));
    DEBUG_PRINT(gpio);
    DEBUG_PRINT(F(" allocated by "));
    DebugPrintOwnerTag(ownerTag[gpio]);
    DEBUG_PRINT(F(", but attempted de-allocation by "));
    DebugPrintOwnerTag(tag);
    #endif
    return false;
  }

  byte by = gpio >> 3;
  byte bi = gpio - 8*by;
  bitWrite(pinAlloc[by], bi, false);
  ownerTag[gpio] = PinOwner::None;
  // ownerConflict[gpio] = PinOwner::None;  // WLEDMM clear conflict (if any)
  return true;
}

// support function for deallocating multiple pins
bool PinManagerClass::deallocateMultiplePins(const uint8_t *pinArray, byte arrayElementCount, PinOwner tag)
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
    #ifdef WLED_DEBUG
    DEBUG_PRINT(F("PIN DEALLOC: IO "));
    DEBUG_PRINT(gpio);
    DEBUG_PRINT(F(" allocated by "));
    DebugPrintOwnerTag(ownerTag[gpio]);
    DEBUG_PRINT(F(", but attempted de-allocation by "));
    DebugPrintOwnerTag(tag);
    #endif
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

bool PinManagerClass::deallocateMultiplePins(const managed_pin_type * mptArray, byte arrayElementCount, PinOwner tag)
{
  uint8_t pins[arrayElementCount];
  for (int i=0; i<arrayElementCount; i++) pins[i] = mptArray[i].pin;
  return deallocateMultiplePins(pins, arrayElementCount, tag);
}

bool PinManagerClass::allocateMultiplePins(const managed_pin_type * mptArray, byte arrayElementCount, PinOwner tag)
{
  bool shouldFail = false;
  // first verify the pins are OK and not already allocated
  for (int i = 0; i < arrayElementCount; i++) {
    byte gpio = mptArray[i].pin;
    if (gpio == 0xFF) {
      // explicit support for io -1 as a no-op (no allocation of pin),
      // as this can greatly simplify configuration arrays
      //if (tag==PinOwner::HW_I2C) USER_PRINTF("I2C alloc attempted for %d\n", gpio);
      continue;
    }
    if (!isPinOk(gpio, mptArray[i].isOutput)) {
      #ifdef WLED_DEBUG
      DEBUG_PRINT(F("PIN ALLOC: Invalid pin attempted to be allocated: GPIO "));
      DEBUG_PRINT(gpio);
      DEBUG_PRINT(" as "); DEBUG_PRINT(mptArray[i].isOutput ? "output": "input");
      DEBUG_PRINTLN(F(""));
      #else  // WLEDMM
      USER_PRINTF("PIN ALLOC: invalid pin - cannot use GPIO%d for %s.\n", gpio, mptArray[i].isOutput ? "output": "input");
      #endif
      if ((gpio < WLED_NUM_PINS) && (gpio >= 0) && (tag != PinOwner::None)) {
        ownerConflict[gpio] = tag; // WLEDMM record conflict
      }
      shouldFail = true;
    }
    if ((tag==PinOwner::HW_I2C || tag==PinOwner::HW_SPI) && isPinAllocated(gpio, tag)) {
      // allow multiple "allocations" of HW I2C & SPI bus pins
      continue;
    } else if (isPinAllocated(gpio)) {
      ownerConflict[gpio] = tag; // WLEDMM record conflict
      #ifdef WLED_DEBUG
      DEBUG_PRINT(F("PIN ALLOC: FAIL: IO "));
      DEBUG_PRINT(gpio);
      DEBUG_PRINT(F(" already allocated by "));
      DebugPrintOwnerTag(ownerTag[gpio]);
      DEBUG_PRINTLN(F(""));
      #else  // WLEDMM
      USER_PRINTF("PIN ALLOC: failed to assign GPIO%d to %s - already in use for %s.\n", gpio, getOwnerText(tag).c_str(), getPinOwnerText(gpio).c_str());
      #endif
      shouldFail = true;
    }
  }
  if (shouldFail) {
    return false;
  }

  if (tag==PinOwner::HW_I2C) i2cAllocCount++;
  //if (tag==PinOwner::HW_I2C) DEBUG_PRINTF("I2C alloc counter %d\n", int(i2cAllocCount));
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

    byte by = gpio >> 3;
    byte bi = gpio - 8*by;
    bitWrite(pinAlloc[by], bi, true);
    ownerTag[gpio] = tag;
    // ownerConflict[gpio] = PinOwner::None; // WLEDMM clear conflict (if any)
    #ifdef WLED_DEBUG
    DEBUG_PRINT(F("PIN ALLOC: Pin "));
    DEBUG_PRINT(gpio);
    DEBUG_PRINT(F(" allocated by "));
    DebugPrintOwnerTag(tag);
    DEBUG_PRINTLN(F(""));
    #endif
  }
  return true;
}

bool PinManagerClass::allocatePin(byte gpio, bool output, PinOwner tag)
{
  // HW I2C & SPI pins have to be allocated using allocateMultiplePins variant since there is always SCL/SDA pair
  // DMX_INPUT pins have to be allocated using allocateMultiplePins variant since there is always RX/TX/EN triple
  if (!isPinOk(gpio, output) || (gpio >= WLED_NUM_PINS) || tag==PinOwner::HW_I2C || tag==PinOwner::HW_SPI
      || tag==PinOwner::DMX_INPUT) {
    #ifdef WLED_DEBUG
    if (gpio < 255) {  // 255 (-1) is the "not defined GPIO"
      if (!isPinOk(gpio, output)) {
        if ((gpio < WLED_NUM_PINS) && (gpio >= 0) && (tag != PinOwner::None)) {
          ownerConflict[gpio] = tag; // WLEDMM record conflict
        }
        DEBUG_PRINT(F("PIN ALLOC: FAIL for owner "));
        DebugPrintOwnerTag(tag);
        DEBUG_PRINT(F(": GPIO ")); DEBUG_PRINT(gpio);
        if (output) {DEBUG_PRINTLN(F(" cannot be used for i/o on this MCU."));}
        else DEBUG_PRINTLN(F(" cannot be used as input on this MCU."));
      } else {
        DEBUG_PRINT(F("PIN ALLOC: FAIL: GPIO ")); DEBUG_PRINT(gpio);
        DEBUG_PRINTLN(F(" - HW I2C & SPI pins have to be allocated using allocateMultiplePins()"));
      }
    }
    #else  // WLEDMM
      if (gpio < 255) { 
        USER_PRINTF("PIN ALLOC: invalid pin - cannot use GPIO%d for %s.\n", gpio, output ? "output": "input");
      }
    #endif
    return false;
  }
  if (isPinAllocated(gpio)) {
    ownerConflict[gpio] = tag; // WLEDMM record conflict
    #ifdef WLED_DEBUG
    DEBUG_PRINT(F("PIN ALLOC: Pin "));
    DEBUG_PRINT(gpio);
    DEBUG_PRINT(F(" already allocated by "));
    DebugPrintOwnerTag(ownerTag[gpio]);
    DEBUG_PRINTLN(F(""));
    #else  // WLEDMM
      USER_PRINTF("PIN ALLOC: failed to assign GPIO%d to %s - already in use for %s.\n", gpio, getOwnerText(tag).c_str(), getPinOwnerText(gpio).c_str());
    #endif
    return false;
  }

  byte by = gpio >> 3;
  byte bi = gpio - 8*by;
  bitWrite(pinAlloc[by], bi, true);
  ownerTag[gpio] = tag;
  // ownerConflict[gpio] = PinOwner::None; // WLEDMM clear conflict (if any)
  #ifdef WLED_DEBUG
  DEBUG_PRINT(F("PIN ALLOC: Pin "));
  DEBUG_PRINT(gpio);
  DEBUG_PRINT(F(" successfully allocated by "));
  DebugPrintOwnerTag(tag);
  DEBUG_PRINTLN(F(""));
  #endif

  return true;
}

void PinManagerClass::manageDebugTXPin()
{
  #ifdef WLED_DEBUG_HOST
    if (netDebugEnabled) deallocatePin(hardwareTX, PinOwner::DebugOut);
    #ifdef WLED_DEBUG
      else                 allocatePin(hardwareTX, true, PinOwner::DebugOut);
    #endif
  #else
    #ifdef WLED_DEBUG
      pinManager.allocatePin(hardwareTX, true, PinOwner::DebugOut); // TX (GPIO1 on ESP32) reserved for debug output
    #endif
  #endif
}

// if tag is set to PinOwner::None, checks for ANY owner of the pin.
// if tag is set to any other value, checks if that tag is the current owner of the pin.
bool PinManagerClass::isPinAllocated(byte gpio, PinOwner tag)
{  
  if (!isPinOk(gpio, false)) return true;
  if (gpio == 0xFF) {
    DEBUG_PRINT(F(" isPinAllocated: -1 is never allocacted! ")); 
    return false; // WLEDMM bugfix - avoid invalid index to array
  }

  if ((tag != PinOwner::None) && (ownerTag[gpio] != tag)) {
    if ((ownerTag[gpio] != PinOwner::None) && (tag != PinOwner::HW_I2C) && (tag != PinOwner::HW_SPI)) ownerConflict[gpio] = tag; // WLEDMM record conflict
    return false;
  }
  byte by = gpio >> 3;
  byte bi = gpio - (by<<3);
  return bitRead(pinAlloc[by], bi);
}

//
// WLEDMM: central handling of I2C startup (global Wire #0)
//

bool PinManagerClass::joinWire() {    // shortcut in case no parameters provided
    return joinWire(i2c_sda, i2c_scl);
}

bool PinManagerClass::joinWire(int8_t pinSDA, int8_t pinSCL) {
  // reject PIN = -1, reject SDA=SCL, reject "forbidden" pins
  if (  (pinSDA < 0) || (pinSCL < 0) 
     || (pinSDA == pinSCL) 
     || !isPinOk(pinSDA, true) 
     || !isPinOk(pinSCL, true)) {
    DEBUG_PRINT(F("PIN Manager: invalid GPIO for I2C: SDA="));
    DEBUG_PRINTF("%d, SCL=%d !\n",pinSDA, pinSCL);
    return(false);
  } 

  if ((wire0PinSDA < 0) || (wire0PinSCL < 0)) wire0isStarted = false; // this should not happen

  // if wire already started, reject any other GPIO
  if ((wire0isStarted == true) && 
      (pinSDA != wire0PinSDA) && (pinSDA != wire0PinSCL) &&       // allow "swapped pins2, i.e. SDA <->SCL
      (pinSCL != wire0PinSCL) && (pinSCL != wire0PinSDA)) {
    DEBUG_PRINT(F("PIN Manager: invalid GPIO for I2C: SDA="));
    DEBUG_PRINTF("%d, SCL=%d. Wire already started with sda=%d and scl=%d!\n",pinSDA, pinSCL, wire0PinSDA, wire0PinSCL);
    return(false);
  }

  // make sure pins are allocated
  PinManagerPinType pins[2] = {{pinSCL, true}, {pinSDA, true}};
  if (!allocateMultiplePins(pins, 2, PinOwner::HW_I2C)) {    // this will only FAIL when pins are invalid, or used already for other purposes
    DEBUG_PRINT(F("PIN Manager: failed to allocate GPIO for I2C: SDA="));
    DEBUG_PRINTF("%d, SCL=%d !\n",pinSDA, pinSCL);
    return(false);
  }

  if(wire0isStarted == true) {
    DEBUG_PRINTLN(F("PIN Manager: all good, I2C already started, nothing to do :-)"));
    return(true);
  }

  // NOW do it - start Wire !!! fire ;-)

  bool wireIsOK = true;
  #ifdef ARDUINO_ARCH_ESP32         // ESP32 - i2c pins can be mapped to any GPIO
    wireIsOK = Wire.setPins(pinSDA, pinSCL);   // this will fail if Wire is initialised already (i.e. Wire.begin() called prior)
  #else // 8266 - I2C pins are fixed -> actually they are not.
    //if((pinSDA != 4) || (pinSCL != 5)) {     // fixed PINS: SDA = 4, SCL = 5
    // DEBUG_PRINT(F("PIN Manager: warning ESP8266 I2C pins are fixed. please use SDA="));
    //  DEBUG_PRINTF("%d, SCL=%d !\n",4, 5);
    //  return(false);
    //}
  #endif
  if (wireIsOK == false) {
    USER_PRINTLN(F("PIN Manager: warning - wire.setPins failed!"));
  }

  #ifdef ARDUINO_ARCH_ESP32
  #if defined(WLEDMM_FASTPATH)  // wledMM set I2C to 400Khz, to minimize I2C communication delays
    wireIsOK = Wire.begin(pinSDA, pinSCL, 400000UL);  // this will fail if wire is already running
  #else
    wireIsOK = Wire.begin(pinSDA, pinSCL);  // this will fail if wire is already running
  #endif
  #else
    Wire.begin(pinSDA, pinSCL);  // returns void on 8266
  #endif

  if (wireIsOK == false) {
    USER_PRINTLN(F("PIN Manager: warning - wire.begin failed!"));
  } else {
    USER_PRINT(F("PIN Manager: wire.begin successfull! "));
    USER_PRINT(F("I2C bus is active. SDA="));
    USER_PRINTF("%d SCL=%d.\n", pinSDA, pinSCL);
  }

#ifdef ARDUINO_ARCH_ESP32S3
  Wire.setTimeOut(50);   // workaround for wire timeout bug on -S3
  Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having wiring difficulties
#endif

  wire0isStarted = true;
  wire0PinSDA = pinSDA;
  wire0PinSCL = pinSCL;
  return(true);
}


  // WLEDMM more additions

  // returns true if gpio supports touch functions
  bool PinManagerClass::isPinTouch(int gpio) {
    #if defined(ARDUINO_ARCH_ESP32)
      if (digitalPinToTouchChannel(gpio) >= 0) return true;
    #endif
    return false;  // fall-through case
  }

  // returns true if gpio supports analogRead
  bool PinManagerClass::isPinAnalog(int gpio) {
    #if !defined(ARDUINO_ARCH_ESP32)
      if (gpio == A0) return true;   // for 8266
    #else                            // for ESP32 variants
      if (digitalPinToAnalogChannel(gpio) >= 0) return true;
    #endif
    return false;  // fall-through case
  }

  // returns true if gpio supports analogRead, and it belongs to ADC unit 1
  bool PinManagerClass::isPinADC1(int gpio) {
    if ((gpio < 0) || !isPinAnalog(gpio)) return false;

    #if !defined(ARDUINO_ARCH_ESP32)
      if (gpio == A0) return true;   // for 8266
    #else                            // for ESP32 variants
      #ifdef SOC_ADC_CHANNEL_NUM
        if (digitalPinToAnalogChannel(gpio) < SOC_ADC_CHANNEL_NUM(0)) return true; // ADC1 on ESP32-S3, ESP32-S2, ESP32-C3 
      #else
        if (digitalPinToAnalogChannel(gpio) < 8) return true;   // ADC1 on classic ESP32
      #endif
    #endif
    return false;  // fall-through case
  }

  // returns true if gpio supports analogRead, and it belongs to ADC unit 2
  bool PinManagerClass::isPinADC2(int gpio) {
    if ((gpio < 0) || !isPinAnalog(gpio)) return false; // catch errors

    #if !defined(ARDUINO_ARCH_ESP32)
      return false;   // for 8266 - no ADC2
    #else             // for ESP32 variants
      if (isPinADC1(gpio) == false) return true;   // analog but not ADC1 --> must be ADC2
    #endif
    return false;  // fall-through case
  }

  // returns GPIO number for ADC unit x, channel y. 255 = no such pin
  // see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#gpio-summary
  uint8_t PinManagerClass::getADCPin(AdcIdentifier adcUnit, uint8_t adcPort)
  {
    #if !defined(ARDUINO_ARCH_ESP32)
      if ((adcUnit == ADC1) && (adcPort == 0)) return A0;   // for 8266
      else return(PM_NO_PIN);

    #else                                                   // for ESP32 variants
      if ((adcUnit != ADC1) && (adcUnit != ADC2)) return(PM_NO_PIN); // catch errors

      #if defined(SOC_ADC_MAX_CHANNEL_NUM)                                   // for ESP32-S3, ESP32-S2, ESP32-C3
      int8_t analogChannel = (adcUnit == ADC1) ? adcPort : (SOC_ADC_MAX_CHANNEL_NUM + adcPort);
      if (adcPort >= SOC_ADC_MAX_CHANNEL_NUM) analogChannel = 255;
      #else                                                                  // for classic ESP32
      int8_t analogChannel = (adcUnit == ADC1) ? adcPort : (10 + adcPort);
      if ((adcUnit == ADC1) && (adcPort >= 8)) analogChannel = 127;
      if (adcPort >= 10) analogChannel = 127;
      #endif

      //int analogPin = analogChannelToDigitalPin(analogChannel);
      int analogPin = analogInputToDigitalPin(analogChannel);
      if (analogPin >= 0) return(analogPin);
      else return(PM_NO_PIN);
    #endif

    return(PM_NO_PIN);  // fall-through case
  }

  // WLEDMM end


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
bool PinManagerClass::isPinOk(byte gpio, bool output) const
{
#ifdef ESP32
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
    // #if CONFIG_SPIRAM_MODE_OCT && defined(BOARD_HAS_PSRAM)
    //   if (gpio > 32 && gpio < 38) return !psramFound(); // 33 to 37: not available if using _octal_ SPI Flash or _octal_ PSRAM
    // #endif
    // 38 to 48 are for general use. Be careful about straping pins GPIO45 and GPIO46 - these may be pull-up or pulled-down on your board.
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // strapping pins: 0, 45 & 46
    if (gpio > 18 && gpio < 21) return false;     // WLEDMM: 19 + 20 = USB HWCDC. Not recommended for other uses.
    if (gpio > 21 && gpio < 33) return false;     // 22 to 32: not connected + SPI FLASH
    // JTAG: GPIO39-42 are usually used for inline debugging
    // GPIO46 is input only and pulled down
  #else
    if ((gpio > 5 && gpio < 12) &&   // WLEDMM slightly faster to first check for "potentially reserved pins" and then call ESP.getChipModel()
        ((strncmp_P(PSTR("ESP32-U4WDH"), ESP.getChipModel(), 11) == 0) ||    // this is the correct identifier, but....
         (strncmp_P(PSTR("ESP32-PICO-D2"), ESP.getChipModel(), 13) == 0)))   // https://github.com/espressif/arduino-esp32/issues/10683
    {
      // this chip has 4 MB of internal Flash and different packaging, so available pins are different!
      if (((gpio > 5) && (gpio < 9)) || (gpio == 11))
        return false;
    } else {
      // for classic ESP32 (non-mini) modules, these are the SPI flash pins
      if (gpio > 5 && gpio < 12) return false;      //SPI flash pins
    }
    //WLEDMM gpio 16/17 (PSRAM or SPI FLASH) are handled differently
    // if (((strncmp_P(PSTR("ESP32-PICO"), ESP.getChipModel(), 10) == 0) ||
    //     (strncmp_P(PSTR("ESP32-U4WDH"), ESP.getChipModel(), 11) == 0))
    //    && (gpio == 16 || gpio == 17)) return false; // PICO-D4/U4WDH: gpio16+17 are in use for onboard SPI FLASH
    // if (gpio == 16 || gpio == 17) return !psramFound(); //PSRAM pins on ESP32 (these are IO)
  #endif
    if (output) return digitalPinCanOutput(gpio);
    else        return true;
  }
#else //8266
  if (gpio <  6) return true;
  if (gpio < 12) return false; //SPI flash pins
  if (gpio <= NUM_DIGITAL_PINS) return true; //WLEDMM: include pin 17 / A0 / Audio in
#endif
  return false;
}

PinOwner PinManagerClass::getPinOwner(byte gpio) const {
  if (gpio >= WLED_NUM_PINS) return PinOwner::None; // catch error case, to avoid array out-of-bounds access
  if (!isPinOk(gpio, false)) return PinOwner::None;
  return ownerTag[gpio];
}

#ifdef ARDUINO_ARCH_ESP32
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define MAX_LED_CHANNELS 6
#else
  #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #define MAX_LED_CHANNELS 8
  #else
    #define MAX_LED_CHANNELS 16
  #endif
#endif
byte PinManagerClass::allocateLedc(byte channels)
{
  if (channels > MAX_LED_CHANNELS || channels == 0) return 255;
  byte ca = 0;
  for (byte i = 0; i < MAX_LED_CHANNELS; i++) {
    byte by = i >> 3;
    byte bi = i - 8*by;
    if (bitRead(ledcAlloc[by], bi)) { //found occupied pin
      ca = 0;
    } else {
      ca++;
    }
    if (ca >= channels) { //enough free channels
      byte in = (i + 1) - ca;
      for (byte j = 0; j < ca; j++) {
        byte bChan = in + j;
        byte byChan = bChan >> 3;
        byte biChan = bChan - 8*byChan;
        bitWrite(ledcAlloc[byChan], biChan, true);
      }
      return in;
    }
  }
  return 255; //not enough consecutive free LEDC channels
}

void PinManagerClass::deallocateLedc(byte pos, byte channels)
{
  for (byte j = pos; j < pos + channels; j++) {
    if (j > MAX_LED_CHANNELS) return;
    byte by = j >> 3;
    byte bi = j - 8*by;
    bitWrite(ledcAlloc[by], bi, false);
  }
}
#endif

PinManagerClass pinManager = PinManagerClass();
