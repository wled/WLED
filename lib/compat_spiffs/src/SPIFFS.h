#pragma once

#include <LittleFS.h>

// Compatibility shim for libraries that still include SPIFFS.h.
// Placed in lib/compat_spiffs/src so PlatformIO adds it to the include path
// automatically when scanning libraries.
namespace fs {
using SPIFFSFS = LittleFSFS;
}

inline fs::LittleFSFS SPIFFS;
