#ifndef WLED_OTA_RELEASE_CHECK_H
#define WLED_OTA_RELEASE_CHECK_H

/*
 * OTA Release Compatibility Checking using ESP-IDF Custom Description Section
 * Functions to extract and validate release names from uploaded binary files using embedded metadata
 */

#include <Arduino.h>

#ifdef ESP32
#include <esp_app_format.h>
#endif

#define WLED_CUSTOM_DESC_MAGIC 0x57535453  // "WSTS" (WLED System Tag Structure)
#define WLED_CUSTOM_DESC_VERSION 1
#define WLED_RELEASE_NAME_MAX_LEN 48

// Platform-specific metadata offset in binary file
#ifdef ESP32
#define METADATA_OFFSET 0          // ESP32: metadata appears at beginning
#elif defined(ESP8266)
#define METADATA_OFFSET 0x1000     // ESP8266: metadata appears at 4KB offset
#endif

/**
 * DJB2 hash function (C++11 compatible constexpr)
 * Used for compile-time hash computation of release names
 */
constexpr uint32_t djb2_hash_constexpr(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : djb2_hash_constexpr(str + 1, ((hash << 5) + hash) + *str);
}

/**
 * Runtime DJB2 hash function for validation
 */
inline uint32_t djb2_hash_runtime(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

/**
 * WLED Custom Description Structure
 * This structure is embedded in platform-specific sections at a fixed offset
 * in ESP32/ESP8266 binaries, allowing extraction without modifying the binary format
 */
typedef struct {
    uint32_t magic;               // Magic number to identify WLED custom description
    uint32_t version;             // Structure version for future compatibility
    char release_name[WLED_RELEASE_NAME_MAX_LEN]; // Release name (null-terminated)
    uint32_t crc32;              // CRC32 of the above fields for integrity check
} __attribute__((packed)) wled_custom_desc_t;

/**
 * Extract WLED custom description structure from binary
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedDesc Buffer to store extracted custom description structure
 * @return true if structure was found and extracted, false otherwise
 */
bool extractWledCustomDesc(const uint8_t* binaryData, size_t dataSize, wled_custom_desc_t* extractedDesc);

/**
 * Validate if extracted release name matches current release
 * @param extractedRelease Release name from uploaded binary
 * @return true if releases match (OTA should proceed), false if they don't match
 */
bool validateReleaseCompatibility(const char* extractedRelease);

/**
 * Check if OTA should be allowed based on release compatibility using custom description
 * @param binaryData Pointer to binary file data (not modified)
 * @param dataSize Size of binary data in bytes
 * @param errorMessage Buffer to store error message if validation fails 
 * @param errorMessageLen Maximum length of error message buffer
 * @return true if OTA should proceed, false if it should be blocked
 */
bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, char* errorMessage, size_t errorMessageLen);

/**
 * Get pointer to the embedded custom description structure
 * This ensures the structure is referenced and not optimized out
 * @return pointer to the custom description structure
 */
const wled_custom_desc_t* getWledCustomDesc();

#endif // WLED_OTA_RELEASE_CHECK_H