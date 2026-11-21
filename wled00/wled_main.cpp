#include <Arduino.h>
/*
 * WLED Arduino IDE compatibility file.
 * (this is the former wled00.ino)
 * 
 * Where has everything gone?
 * 
 * In April 2020, the project's structure underwent a major change.
 * We now use the platformIO build system, and building WLED in Arduino IDE is not supported any more.
 * Global variables are now found in file "wled.h"
 * Global function declarations are found in "fcn_declare.h"
 * 
 * Usermod compatibility: Existing wled06_usermod.ino mods should continue to work. Delete usermod.cpp.
 * New usermods should use usermod.cpp instead.
 */
#include "wled.h"

// debug support - report USB configuration of this build
#if defined(WLED_DEBUG)
  #if ARDUINO_USB_CDC_ON_BOOT
    #warning ARDUINO_USB_CDC_ON_BOOT=1
  #elif defined(ARDUINO_USB_CDC_ON_BOOT) && !ARDUINO_USB_CDC_ON_BOOT
    #warning ARDUINO_USB_CDC_ON_BOOT=0
  #endif  // ARDUINO_USB_CDC_ON_BOOT
  #if ARDUINO_USB_MODE
    #warning ARDUINO_USB_MODE=1
  #elif defined(ARDUINO_USB_MODE) && !ARDUINO_USB_MODE
    #warning ARDUINO_USB_MODE=0
  #endif  // ARDUINO_USB_MODE

  #if ARDUINO_USB_DFU_ON_BOOT
    #warning ARDUINO_USB_DFU_ON_BOOT
  #endif  // ARDUINO_USB_DFU_ON_BOOT
  #if ARDUINO_USB_MSC_ON_BOOT
    #warning ARDUINO_USB_MSC_ON_BOOT
  #endif  // ARDUINO_USB_MSC_ON_BOOT
#endif // WLED_DEBUG

void setup() {
  WLED::instance().setup();
}

void loop() {
  WLED::instance().loop();
}
