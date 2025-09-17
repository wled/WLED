#include "ota_release_check.h"
#include "wled.h"

#ifdef ESP32
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#endif

bool extractWledCustomDesc(const uint8_t* binaryData, size_t dataSize, wled_custom_desc_t* extractedDesc) {
    if (!binaryData || !extractedDesc || dataSize < 64) {
        return false;
    }

    // Search in first 8KB only. This range was chosen because:
    // - ESP32 .rodata.wled_desc sections appear early in the binary (typically within first 2-4KB)
    // - ESP8266 .ver_number sections also appear early (typically within first 1-2KB)
    // - 8KB provides ample coverage for metadata discovery while minimizing processing time
    // - Larger firmware files (>1MB) would take significantly longer to process with full search
    // - Real-world testing shows all valid metadata appears well within this range
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
            
            // Validate hash using runtime function
            uint32_t expected_hash = djb2_hash_runtime(custom_desc->release_name);
            if (custom_desc->crc32 != expected_hash) {
                DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but hash mismatch\n"), offset);
                continue;
            }
            
            // Valid structure found - copy entire structure
            memcpy(extractedDesc, custom_desc, sizeof(wled_custom_desc_t));
            
            #ifdef ESP32
            DEBUG_PRINTF_P(PSTR("Extracted ESP32 WLED structure from .rodata.wled_desc section at offset %u: '%s'\n"), 
                          offset, extractedDesc->release_name);
            #else
            DEBUG_PRINTF_P(PSTR("Extracted ESP8266 WLED structure from .ver_number section at offset %u: '%s'\n"), 
                          offset, extractedDesc->release_name);
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

bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, bool skipValidation, char* errorMessage) {
  // Clear error message
  if (errorMessage) {
    errorMessage[0] = '\0';
  }

  // Ensure our custom description structure is referenced (prevents optimization)
  const wled_custom_desc_t* local_desc = getWledCustomDesc();
  (void)local_desc; // Suppress unused variable warning

  // If user chose to skip validation, allow OTA immediately
  if (skipValidation) {
    DEBUG_PRINTLN(F("OTA release check bypassed by user"));
    return true;
  }

  // Try to extract WLED structure directly from binary data
  wled_custom_desc_t extractedDesc;
  bool hasCustomDesc = extractWledCustomDesc(binaryData, dataSize, &extractedDesc);

  if (!hasCustomDesc) {
    // No custom description - this could be a legacy binary
    if (errorMessage) {
      strcpy(errorMessage, "This firmware file is missing compatibility metadata. Enable 'Ignore firmware validation' to proceed anyway.");
    }
    DEBUG_PRINTLN(F("OTA declined: No custom description found"));
    return false;
  }

  // Validate compatibility using extracted release name
  if (!validateReleaseCompatibility(extractedDesc.release_name)) {
    if (errorMessage) {
      snprintf(errorMessage, 127, "Firmware compatibility mismatch: current='%s', uploaded='%s'. Enable 'Ignore firmware validation' to proceed anyway.", 
               releaseString, extractedDesc.release_name);
    }
    DEBUG_PRINTF_P(PSTR("OTA declined: Release mismatch current='%s', uploaded='%s'\n"), 
                   releaseString, extractedDesc.release_name);
    return false;
  }

  DEBUG_PRINTLN(F("OTA allowed: Release names match"));
  return true;
}