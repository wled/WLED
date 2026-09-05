#pragma once

#include <LittleFS.h>

// Compatibility shim for libraries that still include SPIFFS.h.
// WLED uses LittleFS for filesystem access, so we map the legacy SPIFFS
// symbol to LittleFS to keep TFT_eSPI and similar libraries buildable.
namespace fs {
using SPIFFSFS = LittleFSFS;
}

inline fs::LittleFSFS SPIFFS;
