/*
 This functions are used to use WLED together with "Safe Bootbootloader"
 https://github.com/wled-install/SafeBootloader
*/

#pragma once

#ifdef  WLED_ENABLE_SAFE_BOOT
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include <rom/crc.h>
#include <esp_ota_ops.h>

bool update_spiffs_crc();
void update_ota_crc();

#endif