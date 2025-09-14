#include "ota_release_check.h"
#include "wled.h"

#ifdef ESP32
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#endif

// Same hash function used at compile time (must match wled_custom_desc.cpp)
static uint32_t djb2_hash(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

bool extractReleaseFromCustomDesc(const uint8_t* binaryData, size_t dataSize, char* extractedRelease) {
    if (!binaryData || !extractedRelease || dataSize < 64) {
        return false;
    }

    // Search in first 8KB only - ESP32 .rodata.wled_desc and ESP8266 .ver_number 
    // sections appear early in binary. 8KB should be sufficient for metadata discovery
    // while minimizing processing time for large firmware files.
    const size_t search_limit = min(dataSize, (size_t)8192);
    
    for (size_t offset = 0; offset <= search_limit - sizeof(wled_custom_desc_t); offset++) {
        const wled_custom_desc_t* custom_desc = (const wled_custom_desc_t*)(binaryData + offset);
        
        // Check for magic number
        if (custom_desc->magic == WLED_CUSTOM_DESC_MAGIC) {
            // Found potential match, validate version
            if (custom_desc->version != WLED_CUSTOM_DESC_VERSION) {
                DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but version mismatch: %u\n"), 
                              offset, custom_desc->version);
                continue;
            }
            
            // Validate hash using same algorithm as compile-time
            uint32_t expected_hash = djb2_hash(custom_desc->release_name);
            if (custom_desc->crc32 != expected_hash) {
                DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but hash mismatch\n"), offset);
                continue;
            }
            
            // Valid structure found
            strncpy(extractedRelease, custom_desc->release_name, WLED_RELEASE_NAME_MAX_LEN - 1);
            extractedRelease[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
            
            #ifdef ESP32
            DEBUG_PRINTF_P(PSTR("Extracted ESP32 release name from .rodata.wled_desc section at offset %u: '%s'\n"), 
                          offset, extractedRelease);
            #else
            DEBUG_PRINTF_P(PSTR("Extracted ESP8266 release name from .ver_number section at offset %u: '%s'\n"), 
                          offset, extractedRelease);
            #endif
            return true;
        }
    }
    
    DEBUG_PRINTLN(F("No WLED custom description found in binary"));
    return false;
}

bool validateReleaseCompatibility(const char* extractedRelease) {
  if (!extractedRelease || strlen(extractedRelease) == 0) {
    return false;
  }
  
  // Simple string comparison - releases must match exactly
  bool match = strcmp(releaseString, extractedRelease) == 0;
  
  DEBUG_PRINTF_P(PSTR("Release compatibility check: current='%s', uploaded='%s', match=%s\n"), 
                 releaseString, extractedRelease, match ? "YES" : "NO");
  
  return match;
}

bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, bool ignoreReleaseCheck, char* errorMessage) {
  // Clear error message
  if (errorMessage) {
    errorMessage[0] = '\0';
  }

  // Ensure our custom description structure is referenced (prevents optimization)
  const wled_custom_desc_t* local_desc = getWledCustomDesc();
  (void)local_desc; // Suppress unused variable warning

  // If user chose to ignore release check, allow OTA
  if (ignoreReleaseCheck) {
    DEBUG_PRINTLN(F("OTA release check bypassed by user"));
    return true;
  }

  // Try to extract release name directly from binary data
  char extractedRelease[WLED_RELEASE_NAME_MAX_LEN];
  bool hasCustomDesc = extractReleaseFromCustomDesc(binaryData, dataSize, extractedRelease);

  if (!hasCustomDesc) {
    // No custom description - this could be a legacy binary
    if (errorMessage) {
      strcpy(errorMessage, "Binary has no release compatibility metadata. Check 'Ignore validation' to proceed.");
    }
    DEBUG_PRINTLN(F("OTA blocked: No custom description found"));
    return false;
  }

  // Validate compatibility using extracted release name
  if (!validateReleaseCompatibility(extractedRelease)) {
    if (errorMessage) {
      snprintf(errorMessage, 127, "Release mismatch: current='%s', uploaded='%s'. Check 'Ignore validation' to proceed.", 
               releaseString, extractedRelease);
    }
    DEBUG_PRINTF_P(PSTR("OTA blocked: Release mismatch current='%s', uploaded='%s'\n"), 
                   releaseString, extractedRelease);
    return false;
  }

  DEBUG_PRINTLN(F("OTA allowed: Release names match"));
  return true;
}