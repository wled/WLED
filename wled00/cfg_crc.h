#pragma once

#ifdef WLED_ENABLE_CRC_FOR_CONFIG
static uint32_t crc32_update(uint32_t crc, uint8_t data);
uint32_t crc32_file(const char* path);
bool saveCRC(const char* path, uint32_t crc);
bool loadCRC(const char* path, uint32_t &crc);
#endif