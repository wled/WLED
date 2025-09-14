#include "ota_release_check.h"
#include "wled.h"

#ifdef ESP32

// Create the custom description structure with current release name
// This will be embedded in a standard rodata section for ESP32 binary
// Using a simpler section name that the linker will accept
const wled_custom_desc_t __attribute__((section(".rodata.wled_desc"))) wled_custom_description = {
    WLED_CUSTOM_DESC_MAGIC,     // magic
    WLED_CUSTOM_DESC_VERSION,   // version  
    WLED_RELEASE_NAME,          // release_name
    0,                          // crc32 - computed at runtime
    {0}                         // reserved
};

// Reference to ensure it's not optimized away
const wled_custom_desc_t* __attribute__((used)) wled_custom_desc_ref = &wled_custom_description;

// Function to ensure the structure is referenced by code
const wled_custom_desc_t* getWledCustomDesc() {
    return &wled_custom_description;
}

#elif defined(ESP8266)

// For ESP8266, use the .ver_number section to store release metadata
// This section is located at a known offset in ESP8266 binaries
const wled_custom_desc_t __attribute__((section(".ver_number"))) wled_esp8266_version_info = {
    WLED_CUSTOM_DESC_MAGIC,     // magic
    WLED_CUSTOM_DESC_VERSION,   // version
    WLED_RELEASE_NAME,          // release_name  
    0,                          // crc32 - computed at runtime
    {0}                         // reserved
};

// Reference the structure in code to prevent linker from discarding it
// This ensures the version info is always present in the binary
const wled_custom_desc_t* __attribute__((used)) wled_esp8266_version_ref = &wled_esp8266_version_info;

// Function to ensure the structure is referenced by code  
const wled_custom_desc_t* getWledCustomDesc() {
    return &wled_esp8266_version_info;
}

#endif // ESP32/ESP8266