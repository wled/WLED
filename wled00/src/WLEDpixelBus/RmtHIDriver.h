/*-------------------------------------------------------------------------
WLEDpixelBus - high priority RMT interrupts for ESP32
by @willmmiles, 2026
-------------------------------------------------------------------------*/
#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "esp_idf_version.h"

// Use the fast refill driver on Xtensa targets (ESP32/S2/S3, assembly-bridged high-level ISR)
// and the normal-priority ISR path on RISC-V targets (C3/C5/C6/H2).
// C5 and H2 share the C6 RMT register layout; P4 uses a newer RMT IP and is not supported.
#if (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)))
#define WLEDPB_USE_RMTHI
#endif

#ifdef WLEDPB_USE_RMTHI

#include <esp_err.h>
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/rmt.h" // legacy driver, only used on IDF V4
#endif
#include "freertos/FreeRTOS.h"
#include <stddef.h>
#include <stdint.h>

namespace RmtHiDriver {
  // Install the driver for a specific channel, specifying timing properties
  esp_err_t Install(uint8_t channel, uint32_t rmtBit0, uint32_t rmtBit1, uint32_t reset, uint8_t blocksToUse = 1);

  // Remove the driver on a specific channel
  esp_err_t Uninstall(uint8_t channel);

  // Write a buffer of data to a specific channel.
  // Buffer reference is held until write completes.
  esp_err_t Write(uint8_t channel, const uint8_t *src, size_t srcSize);

  // Wait until transaction is complete.
  esp_err_t WaitForTxDone(uint8_t channel, TickType_t wait_time);
};

#endif // WLEDPB_USE_RMTHI
#endif // ARDUINO_ARCH_ESP32
