#include "ota_release_check.h"
#include "wled.h"

#ifdef ESP32

// Simple hash function for validation (compile-time friendly)
constexpr uint32_t simple_hash(const char* str) {
    uint32_t hash = 5381;
    for (int i = 0; str[i]; ++i) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash;
}

// Create the custom description structure with current release name
// This will be embedded at a fixed offset in the ESP32 binary
const wled_custom_desc_t __attribute__((section(".rodata_custom_desc"))) wled_custom_description = {
    .magic = WLED_CUSTOM_DESC_MAGIC,
    .version = WLED_CUSTOM_DESC_VERSION,
    .release_name = WLED_RELEASE_NAME,
    .crc32 = simple_hash(WLED_RELEASE_NAME),  // Use simple hash for validation
    .reserved = {0}
};

#endif // ESP32