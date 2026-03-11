#pragma once

#include "wled.h"
#include <unzipLIB.h>

#ifdef WLED_USE_SD_SPI
  #include <SD.h>
  #include <SPI.h>
#elif defined(WLED_USE_SD_MMC)
  #include <SD_MMC.h>
#endif

class XLZUnzip {
public:
  // Unpacks one .xlz archive to the SD card.
  // On success, the .xlz file is deleted and the final .fseq path is returned in outFile.
  static bool unpackAndDelete(const String& archivePath, String* outFile = nullptr);

  // Optional helper: scan the SD root and unpack all .xlz files.
  static uint8_t processAllPendingXLZ();

private:
  struct FsHandle {
    File file;
    int32_t pos = 0;
  };

  static void* openZip(const char* filename, int32_t* size);
  static void closeZip(void* p);
  static int32_t readZip(void* p, uint8_t* buffer, int32_t length);
  static int32_t seekZip(void* p, int32_t position, int iType);

  static bool unpackArchive(const String& archivePath, String& finalOutputPath);
  static bool unpackCurrentFile(UNZIP& zip, const String& outputPath, uint32_t expectedSize);
  static String sanitizeEntryName(const char* rawName);
  static bool hasXLZExtension(const String& path);
};
