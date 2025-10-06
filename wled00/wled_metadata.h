/*
  WLED build metadata

  Manages and exports information about the current WLED build.
*/


#pragma once

#include <cstdint>
#include <string.h>
#include <WString.h>

#define WLED_VERSION_MAX_LEN 48
#define WLED_RELEASE_NAME_MAX_LEN 48

/**
 * WLED Custom Description Structure
 * This structure is embedded in platform-specific sections at an approximately
 * fixed offset in ESP32/ESP8266 binaries, where it can be found and validated 
 * by the OTA process.
 */
typedef struct {
    uint32_t magic;               // Magic number to identify WLED custom description
    uint32_t desc_version;        // Structure version for future compatibility
    char wled_version[WLED_VERSION_MAX_LEN];
    char release_name[WLED_RELEASE_NAME_MAX_LEN]; // Release name (null-terminated)    
    uint32_t hash;               // Structure sanity check
} __attribute__((packed)) wled_metadata_t;


// Global build description
extern const wled_metadata_t WLED_BUILD_DESCRIPTION;

// Convenient metdata pointers
#define versionString (WLED_BUILD_DESCRIPTION.wled_version)   // Build version, WLED_VERSION
#define releaseString (WLED_BUILD_DESCRIPTION.release_name)   // Release name,  WLED_RELEASE_NAME
extern const __FlashStringHelper* repoString;                       // Github repository (if available)
extern const __FlashStringHelper* productString;                    // Product, WLED_PRODUCT_NAME -- deprecated, use WLED_RELEASE_NAME
extern const __FlashStringHelper* brandString ;                     // Brand


// Metadata analysis functions

/**
 * Extract WLED custom description structure from binary data
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedDesc Buffer to store extracted custom description structure
 * @return true if structure was found and extracted, false otherwise
 */
bool findWledMetadata(const uint8_t* binaryData, size_t dataSize, wled_metadata_t* extractedDesc);

/**
 * Check if OTA should be allowed based on release compatibility
 * @param firmwareDescription Pointer to firmware description
 * @param errorMessage Buffer to store error message if validation fails 
 * @param errorMessageLen Maximum length of error message buffer
 * @return true if OTA should proceed, false if it should be blocked
 */
bool shouldAllowOTA(const wled_metadata_t& firmwareDescription, char* errorMessage, size_t errorMessageLen);
