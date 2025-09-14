#include "ota_release_check.h"
#include "wled.h"

#ifdef ESP32
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#endif

bool extractReleaseFromCustomDesc(const uint8_t* binaryData, size_t dataSize, char* extractedRelease) {
    if (!binaryData || !extractedRelease || dataSize < sizeof(esp_image_header_t)) {
        return false;
    }

#ifdef ESP32
    // Look for ESP32 image header to find the custom description section
    const esp_image_header_t* header = (const esp_image_header_t*)binaryData;
    
    // Validate ESP32 image header
    if (header->magic != ESP_IMAGE_HEADER_MAGIC) {
        DEBUG_PRINTLN(F("Not a valid ESP32 image - missing magic header"));
        return false;
    }

    // The custom description section is located at a fixed offset after the image header
    // ESP-IDF places custom description at offset 0x20 in the binary for ESP32
    const size_t custom_desc_offset = 0x20;
    
    if (dataSize < custom_desc_offset + sizeof(wled_custom_desc_t)) {
        DEBUG_PRINTLN(F("Binary too small to contain custom description"));
        return false;
    }

    const wled_custom_desc_t* custom_desc = (const wled_custom_desc_t*)(binaryData + custom_desc_offset);
    
    // Validate magic number and version
    if (custom_desc->magic != WLED_CUSTOM_DESC_MAGIC) {
        DEBUG_PRINTLN(F("No WLED custom description found - no magic number"));
        return false;
    }

    if (custom_desc->version != WLED_CUSTOM_DESC_VERSION) {
        DEBUG_PRINTF_P(PSTR("Unsupported custom description version: %u\n"), custom_desc->version);
        return false;
    }

    // Validate simple hash checksum (using the same simple hash as in wled_custom_desc.cpp)
    auto simple_hash = [](const char* str) -> uint32_t {
        uint32_t hash = 5381;
        for (int i = 0; str[i]; ++i) {
            hash = ((hash << 5) + hash) + str[i];
        }
        return hash;
    };
    
    uint32_t expected_hash = simple_hash(custom_desc->release_name);
    if (custom_desc->crc32 != expected_hash) {
        DEBUG_PRINTF_P(PSTR("Custom description hash mismatch: expected 0x%08x, got 0x%08x\n"), 
                      expected_hash, custom_desc->crc32);
        return false;
    }

    // Extract release name (ensure null termination)
    strncpy(extractedRelease, custom_desc->release_name, WLED_RELEASE_NAME_MAX_LEN - 1);
    extractedRelease[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
    
    DEBUG_PRINTF_P(PSTR("Extracted release name from custom description: '%s'\n"), extractedRelease);
    return true;
#else
    // ESP8266 doesn't use ESP-IDF format, so we can't extract custom description
    DEBUG_PRINTLN(F("ESP8266 binaries do not support custom description extraction"));
    return false;
#endif
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

  // If user chose to ignore release check, allow OTA
  if (ignoreReleaseCheck) {
    DEBUG_PRINTLN(F("OTA release check bypassed by user"));
    return true;
  }

  // Try to extract release name from custom description section
  char extractedRelease[WLED_RELEASE_NAME_MAX_LEN];
  bool hasCustomDesc = extractReleaseFromCustomDesc(binaryData, dataSize, extractedRelease);

  if (!hasCustomDesc) {
    // No custom description - this could be a legacy binary or ESP8266 binary
    if (errorMessage) {
#ifdef ESP32
      strcpy(errorMessage, "Binary has no release compatibility metadata. Check 'Ignore release name check' to proceed.");
#else
      strcpy(errorMessage, "ESP8266 binaries do not support release checking. Check 'Ignore release name check' to proceed.");
#endif
    }
    DEBUG_PRINTLN(F("OTA blocked: No custom description found"));
    return false;
  }

  // Validate compatibility using extracted release name
  if (!validateReleaseCompatibility(extractedRelease)) {
    if (errorMessage) {
      snprintf(errorMessage, 127, "Release mismatch: current='%s', uploaded='%s'. Check 'Ignore release name check' to proceed.", 
               releaseString, extractedRelease);
    }
    DEBUG_PRINTF_P(PSTR("OTA blocked: Release mismatch current='%s', uploaded='%s'\n"), 
                   releaseString, extractedRelease);
    return false;
  }

  DEBUG_PRINTLN(F("OTA allowed: Release names match"));
  return true;
}