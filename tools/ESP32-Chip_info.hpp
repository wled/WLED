#pragma once

/* A tool that print some ESP32 hardware information 
 * to serial monitor.
 * main function: showRealSpeed();
 */


/* code below is based on https://github.com/Jason2866/ESP32_Show_Info
 * that was created by Jason2866, subject to the GNU General Public License v3.0 
 * detailed license conditions: https://github.com/Jason2866/ESP32_Show_Info/blob/main/LICENSE
 */
#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32

#include "soc/spi_reg.h"
#include <soc/efuse_reg.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  #include "esp_chip_info.h"  // esp-idf v4.4.x
#else
  #include "esp_system.h"     // esp-idf v3.x
#endif

#if CONFIG_IDF_TARGET_ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  #include "esp32/rom/spi_flash.h"
  #include "esp32/spiram.h"
  #include "spiram_psram.h"
#else
  #include "esp_spi_flash.h"
  #include "rom/spi_flash.h"
  #include "esp_spiram.h"
  //#include "spiram_psram.h"
#endif
#elif CONFIG_IDF_TARGET_ESP32S2  // ESP32-S2
  #include "esp32s2/rom/spi_flash.h"
#elif CONFIG_IDF_TARGET_ESP32S3  // ESP32-S3
  #include "esp32s3/rom/spi_flash.h"
#elif CONFIG_IDF_TARGET_ESP32C3  // ESP32-C3
  #include "esp32c3/rom/spi_flash.h"
#endif

#include <sdkconfig.h>
#include <rom/cache.h>

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
  #ifndef REG_SPI_BASE
  #define REG_SPI_BASE(i)     (DR_REG_SPI1_BASE + (((i)>1) ? (((i)* 0x1000) + 0x20000) : (((~(i)) & 1)* 0x1000 )))
  #endif // REG_SPI_BASE
#endif // TARGET

uint32_t my_ESP_getFlashChipId(void)
{
  uint32_t id = g_rom_flashchip.device_id;
  id = ((id & 0xff) << 16) | ((id >> 16) & 0xff) | (id & 0xff00);
  return id;
}

uint32_t my_ESP_getFlashChipRealSize(void)
{
  uint32_t id = (my_ESP_getFlashChipId() >> 16) & 0xFF;
  return 2 << (id - 1);
}

String my_ESP_getFlashChipMode(void) {
#if CONFIG_IDF_TARGET_ESP32S2
const uint32_t spi_ctrl = REG_READ(PERIPHS_SPI_FLASH_CTRL);
#else
const uint32_t spi_ctrl = REG_READ(SPI_CTRL_REG(0));
#endif
/* Not all of the following constants are already defined in older versions of spi_reg.h, so do it manually for now*/
if (spi_ctrl & BIT(24)) { //SPI_FREAD_QIO
    return F("QIO");
} else if (spi_ctrl & BIT(20)) { //SPI_FREAD_QUAD
    return F("QOUT");
} else if (spi_ctrl &  BIT(23)) { //SPI_FREAD_DIO
    return F("DIO");
} else if (spi_ctrl & BIT(14)) { // SPI_FREAD_DUAL
    return F("DOUT");
} else if (spi_ctrl & BIT(13)) { //SPI_FASTRD_MODE
    return F("Fast");
} else {
    return F("Slow");
}
return F("DOUT");
}


//******** Flash Chip Speed is NOT correct !!!! *****
uint32_t my_ESP_getFlashChipSpeed(void)
{
  const uint32_t spi_clock = REG_READ(SPI_CLOCK_REG(0));
  if (spi_clock & BIT(31)) {
    // spi_clk is equal to system clock
    return getApbFrequency();
  }
  return spiClockDivToFrequency(spi_clock);
}

String my_GetDeviceHardware(void) {
  // https://www.espressif.com/en/products/socs

/*
Source: esp-idf esp_system.h and esptool

typedef enum {
    CHIP_ESP32  = 1, //!< ESP32
    CHIP_ESP32S2 = 2, //!< ESP32-S2
    CHIP_ESP32S3 = 9, //!< ESP32-S3
    CHIP_ESP32C3 = 5, //!< ESP32-C3
    CHIP_ESP32H2 = 6, //!< ESP32-H2
    CHIP_ESP32C2 = 12, //!< ESP32-C2
} esp_chip_model_t;

// Chip feature flags, used in esp_chip_info_t
#define CHIP_FEATURE_EMB_FLASH      BIT(0)      //!< Chip has embedded flash memory
#define CHIP_FEATURE_WIFI_BGN       BIT(1)      //!< Chip has 2.4GHz WiFi
#define CHIP_FEATURE_BLE            BIT(4)      //!< Chip has Bluetooth LE
#define CHIP_FEATURE_BT             BIT(5)      //!< Chip has Bluetooth Classic
#define CHIP_FEATURE_IEEE802154     BIT(6)      //!< Chip has IEEE 802.15.4
#define CHIP_FEATURE_EMB_PSRAM      BIT(7)      //!< Chip has embedded psram


// The structure represents information about the chip
typedef struct {
    esp_chip_model_t model;  //!< chip model, one of esp_chip_model_t
    uint32_t features;       //!< bit mask of CHIP_FEATURE_x feature flags
    uint8_t cores;           //!< number of CPU cores
    uint8_t revision;        //!< chip revision number
} esp_chip_info_t;

*/
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  uint32_t chip_model = chip_info.model;
  uint32_t chip_revision = chip_info.revision;
//  uint32_t chip_revision = ESP.getChipRevision();
  bool rev3 = (3 == chip_revision);
//  bool single_core = (1 == ESP.getChipCores());
  bool single_core = (1 == chip_info.cores);

  if (chip_model < 2) {  // ESP32
#ifdef CONFIG_IDF_TARGET_ESP32
/* esptool:
    def get_pkg_version(self):
        word3 = self.read_efuse(3)
        pkg_version = (word3 >> 9) & 0x07
        pkg_version += ((word3 >> 2) & 0x1) << 3
        return pkg_version
*/
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_version = chip_ver & 0x7;

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("HDW: ESP32 Model %d, Revision %d, Core %d, Package %d"), chip_info.model, chip_revision, chip_info.cores, chip_ver);

    switch (pkg_version) {
      case 0:
        if (single_core) { return F("ESP32-S0WDQ6"); }     // Max 240MHz, Single core, QFN 6*6
        else if (rev3)   { return F("ESP32-D0WDQ6-V3"); }  // Max 240MHz, Dual core, QFN 6*6
        else {             return F("ESP32-D0WDQ6"); }     // Max 240MHz, Dual core, QFN 6*6
      case 1:
        if (single_core) { return F("ESP32-S0WD"); }       // Max 160MHz, Single core, QFN 5*5, ESP32-SOLO-1, ESP32-DevKitC
        else if (rev3)   { return F("ESP32-D0WD-V3"); }    // Max 240MHz, Dual core, QFN 5*5, ESP32-WROOM-32E, ESP32_WROVER-E, ESP32-DevKitC
        else {             return F("ESP32-D0WD"); }       // Max 240MHz, Dual core, QFN 5*5, ESP32-WROOM-32D, ESP32_WROVER-B, ESP32-DevKitC
      case 2:              return F("ESP32-D2WD");         // Max 160MHz, Dual core, QFN 5*5, 2MB embedded flash
      case 3:
        if (single_core) { return F("ESP32-S0WD-OEM"); }   // Max 160MHz, Single core, QFN 5*5, Xiaomi Yeelight
        else {             return F("ESP32-D0WD-OEM"); }   // Max 240MHz, Dual core, QFN 5*5
      case 4:
        if (single_core) { return F("ESP32-U4WDH-S"); }    // Max 160MHz, Single core, QFN 5*5, 4MB embedded flash, ESP32-MINI-1, ESP32-DevKitM-1
        else {             return F("ESP32-U4WDH-D"); }    // Max 240MHz, Dual core, QFN 5*5, 4MB embedded flash
      case 5:
        if (rev3)        { return F("ESP32-PICO-V3"); }    // Max 240MHz, Dual core, LGA 7*7, ESP32-PICO-V3-ZERO, ESP32-PICO-V3-ZERO-DevKit
        else {             return F("ESP32-PICO-D4"); }    // Max 240MHz, Dual core, LGA 7*7, 4MB embedded flash, ESP32-PICO-KIT
      case 6:              return F("ESP32-PICO-V3-02");   // Max 240MHz, Dual core, LGA 7*7, 8MB embedded flash, 2MB embedded PSRAM, ESP32-PICO-MINI-02, ESP32-PICO-DevKitM-2
      case 7:              return F("ESP32-D0WDR2-V3");    // Max 240MHz, Dual core, QFN 5*5, ESP32-WROOM-32E, ESP32_WROVER-E, ESP32-DevKitC
    }
#endif  // CONFIG_IDF_TARGET_ESP32
    return F("ESP32");
  }
  else if (2 == chip_model) {  // ESP32-S2
#ifdef CONFIG_IDF_TARGET_ESP32S2
/* esptool:
    def get_flash_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version

    def get_psram_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 28) & 0x0F
        return pkg_version
*/
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_FLASH_VERSION);
    uint32_t psram_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PSRAM_VERSION);
    uint32_t pkg_version = (chip_ver & 0xF) + ((psram_ver & 0xF) * 100);

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("HDW: ESP32 Model %d, Revision %d, Core %d, Package %d"), chip_info.model, chip_revision, chip_info.cores, chip_ver);

    switch (pkg_version) {
      case 0:              return F("ESP32-S2");           // Max 240MHz, Single core, QFN 7*7, ESP32-S2-WROOM, ESP32-S2-WROVER, ESP32-S2-Saola-1, ESP32-S2-Kaluga-1
      case 1:              return F("ESP32-S2FH2");        // Max 240MHz, Single core, QFN 7*7, 2MB embedded flash, ESP32-S2-MINI-1, ESP32-S2-DevKitM-1
      case 2:              return F("ESP32-S2FH4");        // Max 240MHz, Single core, QFN 7*7, 4MB embedded flash
      case 3:              return F("ESP32-S2FN4R2");      // Max 240MHz, Single core, QFN 7*7, 4MB embedded flash, 2MB embedded PSRAM, , ESP32-S2-MINI-1U, ESP32-S2-DevKitM-1U
      case 100:            return F("ESP32-S2R2");
      case 102:            return F("ESP32-S2FNR2");       // Max 240MHz, Single core, QFN 7*7, 4MB embedded flash, 2MB embedded PSRAM, , Lolin S2 mini
    }
#endif  // CONFIG_IDF_TARGET_ESP32S2
    return F("ESP32-S2");
  }
  else if (9 == chip_model) {  // ESP32-S3
#ifdef CONFIG_IDF_TARGET_ESP32S3
    // no variants for now
#endif  // CONFIG_IDF_TARGET_ESP32S3
    return F("ESP32-S3");                                  // Max 240MHz, Dual core, QFN 7*7, ESP32-S3-WROOM-1, ESP32-S3-DevKitC-1
  }
  else if (5 == chip_model) {  // ESP32-C3
#ifdef CONFIG_IDF_TARGET_ESP32C3
/* esptool:
    def get_pkg_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version
*/
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PKG_VERSION);
    uint32_t pkg_version = chip_ver & 0x7;
//    uint32_t pkg_version = esp_efuse_get_pkg_ver();

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("HDW: ESP32 Model %d, Revision %d, Core %d, Package %d"), chip_info.model, chip_revision, chip_info.cores, chip_ver);

    switch (pkg_version) {
      case 0:              return F("ESP32-C3");           // Max 160MHz, Single core, QFN 5*5, ESP32-C3-WROOM-02, ESP32-C3-DevKitC-02
      case 1:              return F("ESP32-C3FH4");        // Max 160MHz, Single core, QFN 5*5, 4MB embedded flash, ESP32-C3-MINI-1, ESP32-C3-DevKitM-1
    }
#endif  // CONFIG_IDF_TARGET_ESP32C3
    return F("ESP32-C3");
  }
  else if (6 == chip_model) {  // ESP32-S3(beta3)
    return F("ESP32-S3");
  }
  else if (7 == chip_model) {  // ESP32-C6(beta)
#ifdef CONFIG_IDF_TARGET_ESP32C6
/* esptool:
    def get_pkg_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version
*/
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PKG_VERSION);
    uint32_t pkg_version = chip_ver & 0x7;
//    uint32_t pkg_version = esp_efuse_get_pkg_ver();

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("HDW: ESP32 Model %d, Revision %d, Core %d, Package %d"), chip_info.model, chip_revision, chip_info.cores, chip_ver);

    switch (pkg_version) {
      case 0:              return F("ESP32-C6");
    }
#endif  // CONFIG_IDF_TARGET_ESP32C6
    return F("ESP32-C6");
  }
  else if (10 == chip_model) {  // ESP32-H2
#ifdef CONFIG_IDF_TARGET_ESP32H2
/* esptool:
    def get_pkg_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version
*/
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PKG_VERSION);
    uint32_t pkg_version = chip_ver & 0x7;
//    uint32_t pkg_version = esp_efuse_get_pkg_ver();

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("HDW: ESP32 Model %d, Revision %d, Core %d, Package %d"), chip_info.model, chip_revision, chip_info.cores, chip_ver);

    switch (pkg_version) {
      case 0:              return F("ESP32-H2");
    }
#endif  // CONFIG_IDF_TARGET_ESP32H2
    return F("ESP32-H2");
  }
  return F("ESP32");
}

String my_GetDeviceHardwareRevision(void) {
  // ESP32-S2
  // ESP32-D0WDQ6 rev.1
  // ESP32-C3 rev.2
  // ESP32-C3 rev.3
  String result = my_GetDeviceHardware();   // ESP32-C3

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  char revision[24] = { 0 };
  if (chip_info.revision) {
    snprintf_P(revision, sizeof(revision), PSTR(" rev.%d (0x%x)"), chip_info.revision, chip_info.revision);
  }
  result += revision;                    // ESP32-C3 rev.3
#if CONFIG_ESP32_ECO3_CACHE_LOCK_FIX
  result += soc_has_cache_lock_bug() ? F(", chip has cache lock bug") : F(", free of cache lock bug");
#endif

  return result;
}

static void my_show_chip_info(void) {
  #ifndef CHIP_FEATURE_IEEE802154
  #define CHIP_FEATURE_IEEE802154     BIT(6)      //!< Chip has IEEE 802.15.4
  #endif
  #ifndef CHIP_FEATURE_EMB_PSRAM
  #define CHIP_FEATURE_EMB_PSRAM      BIT(7)      //!< Chip has embedded psram
  #endif

  esp_chip_info_t my_info;
  //Serial.println("");
  esp_chip_info(&my_info);
#if 0
  Serial.println("ESP Chip Info:");
  switch ((int)my_info.model) {
    case 1:  Serial.print("ESP32 "); break;
    case 2:  Serial.print("ESP32-S2 "); break;
    case 9:  Serial.print("ESP32-S3 "); break;
    case 5:  Serial.print("ESP32-C3 "); break;
    case 6:  Serial.print("ESP32-H2/H4 "); break; // ESP-IFD 4.x
    case 12: Serial.print("ESP32-C2 "); break;
    case 13: Serial.print("ESP32-C6 "); break;
    case 16: Serial.print("ESP32-H2 "); break; // ESP-IFD 5.x
    case 17: Serial.print("ESP32-C5 beta3 "); break;
    case 18: Serial.print("ESP32-P4 "); break;
    case 999: Serial.print("POSIX/Linux simulator "); break;
    default: Serial.print("(unknown) 0x"); Serial.print((int)my_info.model, HEX); Serial.print(" "); break;
  }
  Serial.print(" Rev ");
  Serial.print(my_info.revision);
  Serial.print(", ");
  Serial.print(my_info.cores);
  Serial.println(" cores.");
  Serial.flush();
#endif

  Serial.print("Chip feature flags: 0b");
  Serial.println(my_info.features, BIN);
  if (my_info.features & CHIP_FEATURE_EMB_FLASH)  Serial.println("  * 0b0000001 Chip has embedded FLASH memory");
  if (my_info.features & CHIP_FEATURE_WIFI_BGN)   Serial.println("  * 0b0000010 Chip has 2.4GHz WiFi");
  if (my_info.features & CHIP_FEATURE_BLE)        Serial.println("  * 0b0001000 Chip has Bluetooth LE");
  if (my_info.features & CHIP_FEATURE_BT)         Serial.println("  * 0b0010000 Chip has Bluetooth Classic");
  if (my_info.features & CHIP_FEATURE_IEEE802154) Serial.println("  * 0b0100000 Chip has IEEE 802.15.4");
  if (my_info.features & CHIP_FEATURE_EMB_PSRAM)  Serial.println("  * 0b1000000 Chip has embedded PSRAM");
  Serial.println();
}


// see https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/ResetReason/ResetReason.ino
/* Print last reset reason of ESP32
*  =================================
*  Use either of the methods print_reset_reason
*  or verbose_print_reset_reason to display the
*  cause for the last reset of this device.
*
*  Public Domain License.
*  Author:
*  Evandro Luis Copercini - 2017
*/
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include <esp32/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S2
#include <esp32s2/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S3
#include <esp32s3/rom/rtc.h>
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include <rom/rtc.h>
#endif

#if 0
// ESP8266 does this a bit differently
//
// include <user_interface.h>
// 
//  struct rst_info * rst_info;
//  system_rtc_mem_read(0, &rst_info, sizeof(rst_info));
//  reason = rst_info->reason;
//     or
//  reason = ESP.getResetInfoPtr()->reason;
// 
//    REASON_DEFAULT_RST      = 0,    /* normal startup by power on */
//    REASON_WDT_RST          = 1,    /* hardware watch dog reset */
//    REASON_EXCEPTION_RST    = 2,    /* exception reset, GPIO status won’t change */
//    REASON_SOFT_WDT_RST     = 3,    /* software watch dog reset, GPIO status won’t change */
//    REASON_SOFT_RESTART     = 4,    /* software restart ,system_restart , GPIO status won’t change */
//    REASON_DEEP_SLEEP_AWAKE = 5,    /* wake up from deep-sleep */
//    REASON_EXT_SYS_RST      = 6     /* external system reset */
//    reason > 6 = user-defined reason codes
#endif

#if 0  // duplicate - this info is also printed by getCoreResetReason()
void my_print_reset_reason(int reason)
{
  Serial.print(reason);
  switch (reason)
  {
    case 0 : Serial.print ("  NO_MEAN"); break;
    case 1 : Serial.print ("  POWERON_RESET");break;          /**<1,  Vbat power on reset*/
    case 2 : Serial.print ("  unknown exception"); break;     /**<2,  this code is not defined on ESP32 */
    case 3 : Serial.print ("  SW_RESET");break;               /**<3,  Software reset digital core*/
    case 4 : Serial.print ("  OWDT_RESET");break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : Serial.print ("  DEEPSLEEP_RESET");break;        /**<5,  Deep Sleep wakeup reset digital core*/
    case 6 : Serial.print ("  SDIO_RESET");break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : Serial.print ("  TG0WDT_SYS_RESET");break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : Serial.print ("  TG1WDT_SYS_RESET");break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : Serial.print ("  RTCWDT_SYS_RESET");break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : Serial.print(" INTRUSION_RESET");break;         /**<10, Instrusion tested to reset CPU*/
    case 11 : Serial.print(" TGWDT_CPU_RESET");break;         /**<11, Time Group reset CPU*/
    case 12 : Serial.print(" SW_CPU_RESET");break;            /**<12, Software reset CPU*/
    case 13 : Serial.print(" RTCWDT_CPU_RESET");break;        /**<13, RTC Watch dog Reset CPU*/
    case 14 : Serial.print(" EXT_CPU_RESET");break;           /**<14, for APP CPU, reset by PRO CPU*/
    case 15 : Serial.print(" RTCWDT_BROWN_OUT_RESET");break;  /**<15, Reset when the vdd voltage is not stable*/
    case 16 : Serial.print(" RTCWDT_RTC_RESET");break;        /**<16, RTC Watch dog reset digital core and rtc module*/
    case 17 : Serial.print(" TG1WDT_CPU_RESET");break;        /**<17, Time Group1 reset CPU*/
    case 18 : Serial.print(" SUPER_WDT_RESET");break;         /**<18, super watchdog reset digital core and rtc module*/
    case 19 : Serial.print(" GLITCH_RTC_RESET");break;        /**<19, glitch reset digital core and rtc module*/
    case 20 : Serial.print(" EFUSE_RESET");break;             /**<20, efuse reset digital core*/
    case 21 : Serial.print(" USB_UART_CHIP_RESET");break;     /**<21, usb uart reset digital core */
    case 22 : Serial.print(" USB_JTAG_CHIP_RESET");break;     /**<22, usb jtag reset digital core */
    case 23 : Serial.print(" POWER_GLITCH_RESET");break;      /**<23, power glitch reset digital core and rtc module*/
    default : Serial.print (" NO_MEAN");
  }
}
void my_verbose_print_reset_reason(int reason)
{
  switch (reason)
  {
    case 0  : Serial.print ("none"); break;
    case 1  : Serial.print ("Vbat power on reset");break;
    case 3  : Serial.print ("Software reset digital core");break;
    case 4  : Serial.print ("Legacy watch dog reset digital core");break;
    case 5  : Serial.print ("Deep Sleep reset digital core");break;
    case 6  : Serial.print ("Reset by SLC module, reset digital core");break;
    case 7  : Serial.print ("Timer Group0 Watch dog reset digital core");break;
    case 8  : Serial.print ("Timer Group1 Watch dog reset digital core");break;
    case 9  : Serial.print ("RTC Watch dog Reset digital core");break;
    case 10 : Serial.print ("Instrusion tested to reset CPU");break;
    case 11 : Serial.print ("Time Group reset CPU");break;
    case 12 : Serial.print ("Software reset CPU");break;
    case 13 : Serial.print ("RTC Watch dog Reset CPU");break;
    case 14 : Serial.print ("APP CPU reset by PRO CPU");break;
    case 15 : Serial.print ("Reset when the vdd voltage is not stable");break;
    case 16 : Serial.print ("RTC Watch dog reset digital core and rtc module");break;
    default : Serial.print ("other "); Serial.print(reason);
  }
}
#endif

/* 
 * parts below were created by softhack007, licenced under EUPL-1.2
 */

void show_psram_info_part1(void)
{
#if defined(BOARD_HAS_PSRAM) || defined(WLED_USE_PSRAM)
  //if (esp_spiram_is_initialized() == false) esp_spiram_init();
  Serial.println(psramFound() ? "ESP32 PSRAM: found.": "ESP32 PSRAM: not found!"); 
  if (!psramFound()) return;
  //psramInit();  // already doe by arduino framework

  // the next part only works on "classic" ESP32
  #if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32S2)  && !defined(CONFIG_IDF_TARGET_ESP32C3)
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    esp_spiram_size_t my_psram_size;
    if (esp_spiram_is_initialized() == false) esp_spiram_init();
    my_psram_size = esp_spiram_get_chip_size();
    switch (my_psram_size) {
      case ESP_SPIRAM_SIZE_16MBITS: Serial.println("* SPI RAM Chip availeable: 16MBits = 2MBytes"); break;
      case ESP_SPIRAM_SIZE_32MBITS: Serial.println("* SPI RAM Chip availeable: 32MBits = 4MBytes"); break;
      case ESP_SPIRAM_SIZE_64MBITS: Serial.println("* SPI RAM Chip availeable: 64MBits = 8MBytes"); break;
      case ESP_SPIRAM_SIZE_INVALID: Serial.println("* SPI RAM size is invalid"); break;
      default: Serial.print("* ?SPI RAM size is unknown (0x"); Serial.print(my_psram_size, HEX); Serial.println(")"); break;
    }
  #endif
  #endif
#endif
}

void show_psram_info_part2(void)
{
#if defined(BOARD_HAS_PSRAM) || defined(WLED_USE_PSRAM)
  if (!psramFound()) return;

  // usually the next part won't work ...
  #ifdef PSRAM_MODE
    switch (PSRAM_MODE) {
      case PSRAM_VADDR_MODE_NORMAL:  Serial.println("*  PSRAM mode = PSRAM_VADDR_MODE_NORMAL"); break;
      case PSRAM_VADDR_MODE_LOWHIGH: Serial.println("*  PSRAM mode = SRAM_VADDR_MODE_LOWHIGH"); break;
      case PSRAM_VADDR_MODE_EVENODD: Serial.println("*  PSRAM mode = PSRAM_VADDR_MODE_EVENODD"); break;
    }
  #endif
  #ifdef PSRAM_SPEED
    switch (PSRAM_SPEED) {
      case PSRAM_CACHE_F40M_S40M: Serial.println("*  PSRAM speed = PSRAM_CACHE_F40M_S40M  -> speed=40 esptool_flash=40 (by config)"); break;
      case PSRAM_CACHE_F80M_S40M: Serial.println("*  PSRAM speed = PSRAM_CACHE_F80M_S40M  -> speed=40 esptool_flash=80 (by config)"); break;
      case PSRAM_CACHE_F80M_S80M: Serial.println("*  PSRAM speed = PSRAM_CACHE_F80M_S80M  -> speed=80 esptool_flash=80 (by config)"); break;
    }
  #endif

  #if 0  // this test makes the "max used PSRAM" info unusable
  // try to allocate PSRAM (one 640KB chunk so we can be sure it will not fit into DRAM)
  void * buff2 = ps_malloc(640 * 1024);
  uint8_t * buf = (uint8_t*)malloc(620 * 1024);
  Serial.print("* PSRAM free after malloc / ps_malloc : "); Serial.print(ESP.getFreePsram() / 1024.0, 2); Serial.println(" KB (1.2MB allocated)");
  if (buff2 == NULL)
    Serial.println("* can't allocate big memory with ps_malloc()");
  else { 
    Serial.println("* Can allocate big memory with ps_malloc()");
    free(buff2);
  }
  if (buf == NULL)
    Serial.println("* can't allocate big memory with malloc()");
  else { 
    free(buf);
    Serial.println("* Can allocate big memory with malloc()");
  }
  #endif

#endif
}

void showRealSpeed() {
  //Serial.begin(115200);
  if (!Serial) return; // Avoid writing to unconnected USB-CDC

  Serial.flush();
  Serial.println(F("\n"));
  for(int aa=0; aa<65; aa++) Serial.print("="); Serial.println();
#if 0  // duplicate - same info is printed in wled.cpp
  Serial.print(  F("Chip info for ")); Serial.print(ESP.getChipModel());
  Serial.print(F(", ")); Serial.print(ESP.getChipCores()); Serial.print(F(" core(s)"));
  Serial.print(F(", ")); Serial.print(ESP.getCpuFreqMHz()); Serial.println(F("MHz."));
#endif
  Serial.print(  F("ESP32 device: ")); Serial.println(my_GetDeviceHardwareRevision());
  Serial.print(  F("SDK:          ")); Serial.println(ESP.getSdkVersion());
  for(int aa=0; aa<42; aa++) Serial.print("-"); Serial.println();

  my_show_chip_info();

  Serial.print(" XTAL FREQ: "); Serial.print(getXtalFrequencyMhz()); Serial.println("   MHz");
  Serial.print(" APB FREQ:  "); Serial.print(getApbFrequency() / 1000000.0, 1); Serial.println(" MHz");
  Serial.print(" CPU FREQ:  "); Serial.print(getCpuFrequencyMhz()); Serial.println("  MHz\n");
  for(int aa=0; aa<42; aa++) Serial.print("-"); Serial.println("\n");

  Serial.print("FLASH CHIP FREQ (magic): "); Serial.print(ESP.getFlashChipSpeed()/1000000.0, 1); Serial.println(" MHz");
  Serial.print("FLASH SIZE (magic byte): "); Serial.print(ESP.getFlashChipSize() / (1024.0 * 1024), 2); Serial.println(" MB");
  Serial.print("FLASH MODE (magic byte): "); Serial.print(ESP.getFlashChipMode()); Serial.println(" ;  0=QIO, 1=QOUT, 2=DIO, 3=DOUT or other\n");

  Serial.flush();
  Serial.print("FLASH CHIP ID:   0x"); Serial.println(my_ESP_getFlashChipId(), HEX);
#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  //Serial.print("FLASH CHIP FREQ: "); Serial.print(my_ESP_getFlashChipSpeed() / 1000000.0, 1); Serial.println(" MHz"); // this seems to crash on -S2
#endif
  Serial.print("FLASH REAL SIZE: "); Serial.print(my_ESP_getFlashChipRealSize() / (1024.0 * 1024), 2); Serial.println(" MB");
  Serial.print("FLASH REAL MODE: "); Serial.println(my_ESP_getFlashChipMode());

  for(int aa=0; aa<42; aa++) Serial.print("-"); Serial.println();
  Serial.flush();
  Serial.print(  "RAM HEAP SIZE:  "); Serial.print(ESP.getHeapSize() / 1024.0, 2); Serial.println(" KB");
  Serial.print(  " FREE RAM:      "); Serial.print(ESP.getFreeHeap() / 1024.0, 2); Serial.println(" KB");
  Serial.print(  " MAX RAM alloc: "); Serial.print(ESP.getMaxAllocHeap() / 1024.0, 2); Serial.println(" KB");

#if defined(BOARD_HAS_PSRAM) || defined(WLED_USE_PSRAM)
  Serial.println();
  show_psram_info_part1();
  if (psramFound()) {
    Serial.print("  total PSRAM:    "); Serial.print(ESP.getPsramSize() / 1024.0, 0); Serial.println(" KB");
    Serial.print("  FREE PSRAM:     "); Serial.print(ESP.getFreePsram() / 1024.0, 2); Serial.println(" KB");
    Serial.print("  MAX PSRAM alloc:"); Serial.print(ESP.getMaxAllocPsram() / 1024.0, 2); Serial.println(" KB");
    Serial.print("  used PSRAM:     "); Serial.print(ESP.getPsramSize() - ESP.getFreePsram()); Serial.println(" Bytes");
    Serial.println();
    show_psram_info_part2();
  }
  Serial.flush();
#endif

#if 0  // duplicate - this info is also printed by getCoreResetReason()
  Serial.println();
  Serial.print("CPU #0 - last reset reason = ");
  my_print_reset_reason(rtc_get_reset_reason(0)); Serial.print("\t => ");
  my_verbose_print_reset_reason(rtc_get_reset_reason(0));
  Serial.println();
  if (ESP.getChipCores() > 1) {
    Serial.print("CPU #1 - last reset reason = ");
    my_print_reset_reason(rtc_get_reset_reason(1));Serial.print("\t => ");
    my_verbose_print_reset_reason(rtc_get_reset_reason(1));
    Serial.println();
  }
#endif

  for(int aa=0; aa<42; aa++) Serial.print("="); Serial.println("\n");
  Serial.flush();
}


#else
  #error this tool only supports ESP32 chips
#endif