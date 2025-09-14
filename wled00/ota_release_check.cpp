#include "ota_release_check.h"
#include "wled.h"

bool extractMetadataHeader(uint8_t* binaryData, size_t dataSize, char* extractedRelease, size_t* actualBinarySize) {
  if (!binaryData || !extractedRelease || !actualBinarySize || dataSize < WLED_META_HEADER_SIZE) {
    *actualBinarySize = dataSize;
    return false;
  }

  // Check if the binary starts with our metadata header
  if (memcmp(binaryData, WLED_META_PREFIX, strlen(WLED_META_PREFIX)) != 0) {
    // No metadata header found, this is a legacy binary
    *actualBinarySize = dataSize;
    DEBUG_PRINTLN(F("No WLED metadata header found - legacy binary"));
    return false;
  }

  DEBUG_PRINTLN(F("Found WLED metadata header"));

  // Extract release name from header
  const char* releaseStart = (const char*)(binaryData + strlen(WLED_META_PREFIX));
  size_t maxReleaseLen = WLED_META_HEADER_SIZE - strlen(WLED_META_PREFIX) - 1;
  
  // Copy release name (it should be null-terminated within the header)
  strncpy(extractedRelease, releaseStart, maxReleaseLen);
  extractedRelease[maxReleaseLen] = '\0'; // Ensure null termination

  // Remove metadata header by shifting binary data
  size_t firmwareSize = dataSize - WLED_META_HEADER_SIZE;
  memmove(binaryData, binaryData + WLED_META_HEADER_SIZE, firmwareSize);
  *actualBinarySize = firmwareSize;

  DEBUG_PRINTF_P(PSTR("Extracted release name from metadata: '%s', firmware size: %zu bytes\n"), 
                 extractedRelease, firmwareSize);

  return true;
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

bool shouldAllowOTA(uint8_t* binaryData, size_t dataSize, bool ignoreReleaseCheck, char* errorMessage, size_t* actualBinarySize) {
  // Clear error message
  if (errorMessage) {
    errorMessage[0] = '\0';
  }
  
  // Initialize actual binary size to full size by default
  if (actualBinarySize) {
    *actualBinarySize = dataSize;
  }

  // If user chose to ignore release check, allow OTA
  if (ignoreReleaseCheck) {
    DEBUG_PRINTLN(F("OTA release check bypassed by user"));
    // Still need to extract metadata header if present to get clean binary
    char dummyRelease[64];
    extractMetadataHeader(binaryData, dataSize, dummyRelease, actualBinarySize);
    return true;
  }

  // Try to extract metadata header
  char extractedRelease[64];
  bool hasMetadata = extractMetadataHeader(binaryData, dataSize, extractedRelease, actualBinarySize);

  if (!hasMetadata) {
    // No metadata header - this could be a legacy binary or a binary without our metadata
    // We cannot determine compatibility for such binaries
    if (errorMessage) {
      strcpy(errorMessage, "Binary has no release compatibility metadata. Check 'Ignore release name check' to proceed.");
    }
    DEBUG_PRINTLN(F("OTA blocked: No metadata header found"));
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