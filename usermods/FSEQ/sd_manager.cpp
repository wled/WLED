#include "sd_manager.h"
#include "usermod_fseq.h"

bool SDManager::begin() {
#ifdef WLED_USE_SD_SPI
  if (!SD_ADAPTER.begin(WLED_PIN_SS, spiPort))
    return false;
#elif defined(WLED_USE_SD_MMC)
  if (!SD_ADAPTER.begin())
    return false;
#endif
  return true;
}

void SDManager::end() { SD_ADAPTER.end(); }

String SDManager::listFiles(const char *dirname) {
  String result = "";
  File root = SD_ADAPTER.open(dirname);
  if (!root) {
    result += "<li>Failed to open directory: ";
    result += dirname;
    result += "</li>";
    return result;
  }
  if (!root.isDirectory()) {
    result += "<li>Not a directory: ";
    result += dirname;
    result += "</li>";
    return result;
  }
  File file = root.openNextFile();
  while (file) {
    result += "<li>";
    result += file.name();
    result += " (" + String(file.size()) + " bytes)</li>";
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return result;
}

bool SDManager::deleteFile(const char *path) { return SD_ADAPTER.remove(path); }