#include "ota_update.h"
#include "wled.h"
#include "wled_metadata.h"

#ifndef WLED_VERSION
  #warning WLED_VERSION was not set - using default value of 'dev'
  #define WLED_VERSION dev
#endif
#ifndef WLED_RELEASE_NAME
  #warning WLED_RELEASE_NAME was not set - using default value of 'Custom'
  #define WLED_RELEASE_NAME "Custom"
#endif
#ifndef WLED_REPO
  // No warning for this one: integrators are not always on GitHub
  #define WLED_REPO "unknown"
#endif

constexpr uint32_t WLED_CUSTOM_DESC_MAGIC = 0x57535453;  // "WSTS" (WLED System Tag Structure)
constexpr uint32_t WLED_CUSTOM_DESC_VERSION = 1;

// Compile-time validation that release name doesn't exceed maximum length
static_assert(sizeof(WLED_RELEASE_NAME) <= WLED_RELEASE_NAME_MAX_LEN, 
              "WLED_RELEASE_NAME exceeds maximum length of WLED_RELEASE_NAME_MAX_LEN characters");


/**
 * DJB2 hash function (C++11 compatible constexpr)
 * Used for compile-time hash computation to validate structure contents
 * Recursive for compile time: not usable at runtime due to stack depth
 * 
 * Note that this only works on strings; there is no way to produce a compile-time
 * hash of a struct in C++11 without explicitly listing all the struct members.
 * So for now, we hash only the release name.  This suffices for a "did you find 
 * valid structure" check.
 * 
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

// ------------------------------------
// GLOBAL VARIABLES
// ------------------------------------
// Structure instantiation for this build 
const wled_metadata_t __attribute__((section(BUILD_METADATA_SECTION))) WLED_BUILD_DESCRIPTION = {
    WLED_CUSTOM_DESC_MAGIC,                   // magic
    WLED_CUSTOM_DESC_VERSION,                 // version 
    TOSTRING(WLED_VERSION),
    WLED_RELEASE_NAME,                        // release_name
    std::integral_constant<uint32_t, djb2_hash_constexpr(WLED_RELEASE_NAME)>::value, // hash - computed at compile time; integral_constant enforces this
};

static const char repoString_s[] PROGMEM = WLED_REPO;
const __FlashStringHelper* repoString = FPSTR(repoString_s);

static const char productString_s[] PROGMEM = WLED_PRODUCT_NAME;
const __FlashStringHelper* productString = FPSTR(productString_s);

static const char brandString_s [] PROGMEM = WLED_BRAND;
const __FlashStringHelper* brandString = FPSTR(brandString_s);



/**
 * Extract WLED custom description structure from binary
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedDesc Buffer to store extracted custom description structure
 * @return true if structure was found and extracted, false otherwise
 */
bool findWledMetadata(const uint8_t* binaryData, size_t dataSize, wled_metadata_t* extractedDesc) {
  if (!binaryData || !extractedDesc || dataSize < sizeof(wled_metadata_t)) {
    return false;
  }

  for (size_t offset = 0; offset <= dataSize - sizeof(wled_metadata_t); offset++) {
    if ((binaryData[offset]) == static_cast<char>(WLED_CUSTOM_DESC_MAGIC)) {
      // First byte matched; check next in an alignment-safe way
      uint32_t data_magic;
      memcpy(&data_magic, binaryData + offset, sizeof(data_magic));
      
      // Check for magic number
      if (data_magic == WLED_CUSTOM_DESC_MAGIC) {            
        wled_metadata_t candidate;
        memcpy(&candidate, binaryData + offset, sizeof(candidate));

        // Found potential match, validate version
        if (candidate.desc_version != WLED_CUSTOM_DESC_VERSION) {
          DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but version mismatch: %u\n"), 
                        offset, candidate.desc_version);
          continue;
        }
        
        // Validate hash using runtime function
        uint32_t expected_hash = djb2_hash_runtime(candidate.release_name);
        if (candidate.hash != expected_hash) {
          DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but hash mismatch\n"), offset);
          continue;
        }
        
        // Valid structure found - copy entire structure
        *extractedDesc = candidate;
        
        DEBUG_PRINTF_P(PSTR("Extracted WLED structure at offset %u: '%s'\n"), 
                      offset, extractedDesc->release_name);
        return true;
      }
    }
  }
  
  DEBUG_PRINTLN(F("No WLED custom description found in binary"));
  return false;
}


/**
 * Normalize a release name by removing the trailing "_V4" suffix (if present).
 * This allows OTA compatibility checks to treat e.g. "ESP32" and "ESP32_V4" as equivalent.
 * @param input  Source release name string
 * @param output Buffer to receive normalized name (must be at least WLED_RELEASE_NAME_MAX_LEN bytes)
 */
static void normalizeReleaseName(const char* input, char* output) {
  strncpy(output, input, WLED_RELEASE_NAME_MAX_LEN - 1);
  output[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
  size_t len = strlen(output);
  // Strip "_V4" suffix to allow upgrading between IDF v4 and newer IDF builds
  if (len >= 4 && strcmp(output + len - 3, "_V4") == 0) {
    output[len - 3] = '\0';
  }
}

bool shouldAllowOTA(const wled_metadata_t& firmwareDescription, char* errorMessage, size_t errorMessageLen) {
  // Clear error message
  if (errorMessage && errorMessageLen > 0) {
    errorMessage[0] = '\0';
  }

  // Validate compatibility using extracted release name
  // We make a stack copy so we can print it safely
  char safeFirmwareRelease[WLED_RELEASE_NAME_MAX_LEN];
  strncpy(safeFirmwareRelease, firmwareDescription.release_name, WLED_RELEASE_NAME_MAX_LEN - 1);
  safeFirmwareRelease[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
  
  if (strlen(safeFirmwareRelease) == 0) {
    return false;
  }  

  if (strncmp_P(safeFirmwareRelease, releaseString, WLED_RELEASE_NAME_MAX_LEN) != 0) {
    // Exact match failed - check if the names are compatible after normalizing the "_V4" suffix.
    // This allows upgrading between e.g. "ESP32_V4" (IDF v4 build) and "ESP32" (newer IDF build).
    char normalizedFirmware[WLED_RELEASE_NAME_MAX_LEN];
    char normalizedCurrent[WLED_RELEASE_NAME_MAX_LEN];
    normalizeReleaseName(safeFirmwareRelease, normalizedFirmware);
    normalizeReleaseName(releaseString, normalizedCurrent);

    if (strcmp(normalizedFirmware, normalizedCurrent) != 0) {
      if (errorMessage && errorMessageLen > 0) {
        snprintf_P(errorMessage, errorMessageLen, PSTR("Firmware compatibility mismatch: current='%s', uploaded='%s'."), 
                 releaseString, safeFirmwareRelease);
        errorMessage[errorMessageLen - 1] = '\0'; // Ensure null termination
      }
      return false;
    }
  }

  // TODO: additional checks go here

  return true;
}
