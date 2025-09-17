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
    // - Analysis of typical WLED binary layouts shows metadata appears well within this range
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
            
            DEBUG_PRINTF_P(PSTR("Extracted WLED structure at offset %u: '%s'\n"), 
                          offset, extractedDesc->release_name);
            return true;
        }
    }
    
    DEBUG_PRINTLN(F("No WLED custom description found in binary"));
    return false;
}

bool validateReleaseCompatibility(const char* extractedRelease) {
  if (!extractedRelease) {
    return false;
  }
  
  // Ensure extractedRelease is properly null terminated (guard against fixed-length buffer issues)
  char safeRelease[WLED_RELEASE_NAME_MAX_LEN];
  strncpy(safeRelease, extractedRelease, WLED_RELEASE_NAME_MAX_LEN - 1);
  safeRelease[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
  
  if (strlen(safeRelease) == 0) {
    return false;
  }
  
  // Simple string comparison - releases must match exactly
  bool match = strcmp(releaseString, safeRelease) == 0;
  
  DEBUG_PRINTF_P(PSTR("Release compatibility check: current='%s', uploaded='%s', match=%s\n"), 
                 releaseString, safeRelease, match ? "YES" : "NO");
  
  return match;
}

bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, char* errorMessage, size_t errorMessageLen) {
  // Clear error message
  if (errorMessage && errorMessageLen > 0) {
    errorMessage[0] = '\0';
  }

  // Ensure our custom description structure is referenced (prevents optimization)
  const wled_custom_desc_t* local_desc = getWledCustomDesc();
  (void)local_desc; // Suppress unused variable warning

  // Try to extract WLED structure directly from binary data
  wled_custom_desc_t extractedDesc;
  bool hasCustomDesc = extractWledCustomDesc(binaryData, dataSize, &extractedDesc);

  if (!hasCustomDesc) {
    // No custom description - this could be a legacy binary
    if (errorMessage && errorMessageLen > 0) {
      strncpy_P(errorMessage, PSTR("This firmware file is missing compatibility metadata. Enable 'Ignore firmware validation' to proceed anyway."), errorMessageLen - 1);
      errorMessage[errorMessageLen - 1] = '\0';
    }
    return false;
  }

  // Validate compatibility using extracted release name
  if (!validateReleaseCompatibility(extractedDesc.release_name)) {
    if (errorMessage && errorMessageLen > 0) {
      snprintf_P(errorMessage, errorMessageLen, PSTR("Firmware compatibility mismatch: current='%s', uploaded='%s'. Enable 'Ignore firmware validation' to proceed anyway."), 
               releaseString, extractedDesc.release_name);
      errorMessage[errorMessageLen - 1] = '\0'; // Ensure null termination
    }
    return false;
  }

  return true;
}