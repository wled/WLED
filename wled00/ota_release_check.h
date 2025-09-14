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

/**
 * WLED Custom Description Structure
 * This structure is embedded in the .rodata_custom_desc section at a fixed offset
 * in ESP32 binaries, allowing extraction without modifying the binary format
 */
typedef struct {
    uint32_t magic;               // Magic number to identify WLED custom description
    uint32_t version;             // Structure version for future compatibility
    char release_name[WLED_RELEASE_NAME_MAX_LEN]; // Release name (null-terminated)
    uint32_t crc32;              // CRC32 of the above fields for integrity check
    uint8_t reserved[12];        // Reserved for future use, must be zero
} __attribute__((packed)) wled_custom_desc_t;

/**
 * Extract release name from binary using ESP-IDF custom description section
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedRelease Buffer to store extracted release name (should be at least WLED_RELEASE_NAME_MAX_LEN bytes)
 * @return true if release name was found and extracted, false otherwise
 */
bool extractReleaseFromCustomDesc(const uint8_t* binaryData, size_t dataSize, char* extractedRelease);

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
 * @param ignoreReleaseCheck If true, skip release validation
 * @param errorMessage Buffer to store error message if validation fails (should be at least 128 bytes)
 * @return true if OTA should proceed, false if it should be blocked
 */
bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, bool ignoreReleaseCheck, char* errorMessage);

#endif // WLED_OTA_RELEASE_CHECK_H