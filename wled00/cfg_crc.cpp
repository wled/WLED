#include "wled.h"

#ifdef WLED_ENABLE_CRC_FOR_CONFIG

static uint32_t crc32_update(uint32_t crc, uint8_t data)
{
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++) {
    if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
    else crc >>= 1;
  }
  return crc;
}

uint32_t crc32_file(const char* path)
{
  File f = WLED_FS.open(path, "r");
  if (!f) return UINT32_MAX;

  uint32_t crc = 0xFFFFFFFF;

  while (f.available())
    crc = crc32_update(crc, f.read());

  f.close();
  return ~crc;
}

bool saveCRC(const char* path, uint32_t crc)
{
  File f = WLED_FS.open(path, "w");
  if (!f) return false;

  f.printf("%lu", (unsigned long)crc);
  f.close();
  return true;
}

bool loadCRC(const char* path, uint32_t &crc)
{
  if (!WLED_FS.exists(path))
    return false;

  File f = WLED_FS.open(path, "r");
  if (!f)
    return false;

  crc = f.parseInt();
  f.close();
  return true;
}

#endif