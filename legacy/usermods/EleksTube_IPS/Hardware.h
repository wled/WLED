/*
 * Define the hardware for the EleksTube IPS clock.  Mostly pin definitions
 */
#ifndef ELEKSTUBEHAX_HARDWARE_H
#define ELEKSTUBEHAX_HARDWARE_H

#include <stdint.h> 
#include <Arduino.h> // for HIGH and LOW

// Common indexing scheme, used to identify the digit
#define SECONDS_ONES (0)
#define SECONDS_TENS (1)
#define MINUTES_ONES (2)
#define MINUTES_TENS (3)
#define HOURS_ONES   (4)
#define HOURS_TENS   (5)
#define NUM_DIGITS   (6)

#define SECONDS_ONES_MAP (0x01 << SECONDS_ONES)
#define SECONDS_TENS_MAP (0x01 << SECONDS_TENS)
#define MINUTES_ONES_MAP (0x01 << MINUTES_ONES)
#define MINUTES_TENS_MAP (0x01 << MINUTES_TENS)
#define HOURS_ONES_MAP   (0x01 << HOURS_ONES)
#define HOURS_TENS_MAP   (0x01 << HOURS_TENS)

// WS2812 (or compatible) LEDs on the back of the display modules.
#define BACKLIGHTS_PIN (12)

// Buttons, active low, externally pulled up (with actual resistors!)
#define BUTTON_LEFT_PIN (33)
#define BUTTON_MODE_PIN (32)
#define BUTTON_RIGHT_PIN (35)
#define BUTTON_POWER_PIN (34)

// I2C to DS3231 RTC.
#define RTC_SCL_PIN (22)
#define RTC_SDA_PIN (21)

// Chip Select shift register, to select the display
#define CSSR_DATA_PIN (14)
#define CSSR_CLOCK_PIN (16)
#define CSSR_LATCH_PIN (17)

// SPI to displays
// DEFINED IN User_Setup.h
// Look for: TFT_MOSI, TFT_SCLK, TFT_CS, TFT_DC, and TFT_RST

// Power for all TFT displays are grounded through a MOSFET so they can all be turned off.
// Active HIGH.
#define TFT_ENABLE_PIN (27)

#endif // ELEKSTUBEHAX_HARDWARE_H
