#include "ota_release_check.h"
#include "wled.h"

// Platform-specific section definition
#ifdef ESP32
#define WLED_CUSTOM_DESC_SECTION ".rodata.wled_desc"
#elif defined(ESP8266)
#define WLED_CUSTOM_DESC_SECTION ".ver_number"
#endif

// Single structure definition for both platforms
const wled_custom_desc_t __attribute__((section(WLED_CUSTOM_DESC_SECTION))) wled_custom_description = {
    WLED_CUSTOM_DESC_MAGIC,                   // magic
    WLED_CUSTOM_DESC_VERSION,                 // version  
    WLED_RELEASE_NAME,                        // release_name
    djb2_hash_constexpr(WLED_RELEASE_NAME)   // crc32 - computed at compile time
};

// Compile-time validation that release name doesn't exceed maximum length
static_assert(sizeof(WLED_RELEASE_NAME) <= WLED_RELEASE_NAME_MAX_LEN, 
              "WLED_RELEASE_NAME exceeds maximum length of WLED_RELEASE_NAME_MAX_LEN characters");

// Single reference to ensure it's not optimized away
const wled_custom_desc_t* __attribute__((used)) wled_custom_desc_ref = &wled_custom_description;

// Function to ensure the structure is referenced by code
const wled_custom_desc_t* getWledCustomDesc() {
    return &wled_custom_description;
}