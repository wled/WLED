/*
 This functions are used to use WLED together with "Safe Bootbootloader"
 https://github.com/wled-install/SafeBootloader
*/
#ifdef  WLED_ENABLE_SAFE_BOOT
#include "safe_boot_functions.h"
#include "wled.h"
#include <Arduino.h>

#define SPIFFS_CRC_MAGIC 0x53504653

typedef struct {
    uint32_t magic;
    uint32_t crc_spiffs;    // CRC SPIFFS / Backup
    uint32_t size_spiffs;   // Größe SPIFFS / Backup
} crc_group_t;

uint32_t calc_crc(const esp_partition_t* part)
{
    const size_t bufsize = 4096;
    uint8_t buf[bufsize];

    uint32_t crc = 0;
    size_t offset = 0;

    while (offset < part->size) {
        size_t toread = bufsize;
        if (offset + toread > part->size)
            toread = part->size - offset;

        esp_partition_read(part, offset, buf, toread);

        crc = crc32_le(crc, buf, toread);
        offset += toread;
    }

    return crc;
}

bool update_spiffs_crc()
{
    const esp_partition_t* spiffs =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
            "spiffs");

    const esp_partition_t* crcpart =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            static_cast<esp_partition_subtype_t>(0x41),   //  crc subtype
            "crc0");

    if (!spiffs || !crcpart) {
        DEBUG_PRINTLN(F("SPIFFS or CRC partition not found!"));
        return false;
    }
     

    uint32_t crc = calc_crc(spiffs);

    crc_group_t stored = {};
    esp_partition_read(crcpart, 0, &stored, sizeof(stored));

    // prüfen ob identisch
    if (stored.magic == SPIFFS_CRC_MAGIC &&
        stored.crc_spiffs == crc &&
        stored.size_spiffs == spiffs->size)
    {
        // nichts zu tun
        DEBUG_PRINTLN(F("SPIFFS CRC matches stored value, no update needed."));
        return true;
    }

    stored.magic = SPIFFS_CRC_MAGIC;
    stored.crc_spiffs = crc;
    stored.size_spiffs = spiffs->size;
    DEBUG_PRINTLN(F("Updating SPIFFS CRC partition with new values."));
    esp_partition_erase_range(crcpart, 0, 4096);
    esp_partition_write(crcpart, 0, &stored, sizeof(stored));
    DEBUG_PRINTLN(F("SPIFFS CRC partition updated."));

    return true;
}

#endif