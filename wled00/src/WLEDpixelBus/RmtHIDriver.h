/*-------------------------------------------------------------------------
WLEDpixelBus - high priority RMT interrupts for ESP32
by @willmmiles, 2026
-------------------------------------------------------------------------*/
#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "esp_idf_version.h"

// The high priority RMT driver is only available on the Xtensa-based ESP32, S2 and S3
// (not on the RISC-V C3) and requires ESP-IDF < 5.0
#if (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)) \
    && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define WPB_USE_RMTHI
#endif

#ifdef WPB_USE_RMTHI

#include <esp_err.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include <stddef.h>
#include <stdint.h>

namespace RmtHiDriver {
  // Install the driver for a specific channel, specifying timing properties
  esp_err_t Install(rmt_channel_t channel, uint32_t rmtBit0, uint32_t rmtBit1, uint32_t reset, uint8_t blocksToUse = 1);

  // Remove the driver on a specific channel
  esp_err_t Uninstall(rmt_channel_t channel);

  // Write a buffer of data to a specific channel.
  // Buffer reference is held until write completes.
  esp_err_t Write(rmt_channel_t channel, const uint8_t *src, size_t src_size);

  // Wait until transaction is complete.
  esp_err_t WaitForTxDone(rmt_channel_t channel, TickType_t wait_time);
};

#endif // WPB_USE_RMTHI
#endif // ARDUINO_ARCH_ESP32
