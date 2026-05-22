#pragma once

#include "wled.h"

#if defined(WLED_USE_SD_SPI)
  #include <SD.h>
  #include <SPI.h>
#elif defined(WLED_USE_SD_MMC)
  #include <SD_MMC.h>
#endif

#ifndef CARD_NONE
#define CARD_NONE 0
#endif

class FSEQSdAdapterCompat {
public:
  uint8_t cardType() const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return backend().cardType();
  #else
    return CARD_NONE;
  #endif
  }

  bool exists(const char* path) const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return backend().exists(path);
  #else
    (void)path;
    return false;
  #endif
  }

  bool exists(const String& path) const {
    return exists(path.c_str());
  }

  File open(const char* path, const char* mode = FILE_READ) const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return backend().open(path, mode);
  #else
    (void)path;
    (void)mode;
    return File();
  #endif
  }

  File open(const String& path, const char* mode = FILE_READ) const {
    return open(path.c_str(), mode);
  }

  bool remove(const char* path) const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return backend().remove(path);
  #else
    (void)path;
    return false;
  #endif
  }

  bool remove(const String& path) const {
    return remove(path.c_str());
  }

  uint64_t totalBytes() const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return backend().totalBytes();
  #else
    return 0;
  #endif
  }

  uint64_t usedBytes() const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return backend().usedBytes();
  #else
    return 0;
  #endif
  }

  bool available() const {
  #if defined(WLED_USE_SD_SPI) || defined(WLED_USE_SD_MMC)
    return cardType() != CARD_NONE;
  #else
    return false;
  #endif
  }

private:
#if defined(WLED_USE_SD_SPI)
  static decltype(SD)& backend() { return SD; }
#elif defined(WLED_USE_SD_MMC)
  static decltype(SD_MMC)& backend() { return SD_MMC; }
#endif
};

inline const FSEQSdAdapterCompat& fseqSdAdapter() {
  static const FSEQSdAdapterCompat instance;
  return instance;
}

#ifndef SD_ADAPTER
  #define SD_ADAPTER fseqSdAdapter()
#endif
