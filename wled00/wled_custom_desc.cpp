#include "ota_release_check.h"
#include "wled.h"

// Simple compile-time hash function for release name validation
constexpr uint32_t djb2_hash(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

// Single structure definition for both platforms
#ifdef ESP32
const wled_custom_desc_t __attribute__((section(".rodata.wled_desc"))) wled_custom_description = {
#elif defined(ESP8266)
const wled_custom_desc_t __attribute__((section(".ver_number"))) wled_custom_description = {
#endif
    WLED_CUSTOM_DESC_MAGIC,                   // magic
    WLED_CUSTOM_DESC_VERSION,                 // version  
    WLED_RELEASE_NAME,                        // release_name
    djb2_hash(WLED_RELEASE_NAME)             // crc32 - computed at compile time
};

// Single reference to ensure it's not optimized away
const wled_custom_desc_t* __attribute__((used)) wled_custom_desc_ref = &wled_custom_description;

// Function to ensure the structure is referenced by code
const wled_custom_desc_t* getWledCustomDesc() {
    return &wled_custom_description;
}