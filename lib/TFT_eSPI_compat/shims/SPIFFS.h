#pragma once
// Stub: TFT_eSPI includes SPIFFS.h when SMOOTH_FONT is defined, but the
// usermods that use it don't load fonts from a filesystem.  This shim
// satisfies the include and the `fs::FS &fontFS = SPIFFS` declaration in
// Smooth_font.h without pulling in the SPIFFS Arduino library.
// SPIFFS-backed font loading will silently do nothing at runtime.
//
// shims/ is appended (not prepended) to CPPPATH in configure.py, so this is
// only reached by the compiler's #include search when no real SPIFFS.h
// exists earlier on the include path.
#include <FS.h>

class SPIFFSStub : public fs::FS {
public:
  SPIFFSStub() : fs::FS(nullptr) {}
};

inline SPIFFSStub SPIFFS;
