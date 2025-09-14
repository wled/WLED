#include "ota_release_check.h"
#include "wled.h"

// Maximum size to scan in binary (we don't need to scan the entire file)
#define MAX_SCAN_SIZE 32768

/**
 * Find a string pattern in binary data using Boyer-Moore-like approach
 */
static int findStringInBinary(const uint8_t* data, size_t dataSize, const char* pattern) {
  size_t patternLen = strlen(pattern);
  if (patternLen == 0 || patternLen > dataSize) return -1;
  
  for (size_t i = 0; i <= dataSize - patternLen; i++) {
    if (memcmp(data + i, pattern, patternLen) == 0) {
      return i;
    }
  }
  return -1;
}

/**
 * Extract null-terminated string from binary data starting at position
 */
static int extractNullTerminatedString(const uint8_t* data, size_t dataSize, int position, char* output, size_t maxLen) {
  if (position < 0 || position >= dataSize) return 0;
  
  size_t len = 0;
  for (size_t i = position; i < dataSize && len < (maxLen - 1); i++) {
    if (data[i] == 0) break; // null terminator
    output[len++] = data[i];
  }
  output[len] = '\0';
  return len;
}

bool extractReleaseNameFromBinary(const uint8_t* binaryData, size_t dataSize, char* extractedRelease) {
  if (!binaryData || !extractedRelease || dataSize == 0) {
    return false;
  }
  
  // Limit scan size to avoid performance issues with large binaries
  size_t scanSize = (dataSize > MAX_SCAN_SIZE) ? MAX_SCAN_SIZE : dataSize;
  
  // Known WLED release name patterns - we'll look for these in the binary
  // Order by specificity (more specific first)
  const char* releasePatterns[] = {
    "ESP32_Ethernet",
    "ESP32_USERMODS", 
    "ESP32_WROVER",
    "ESP32-S3_16MB_opi",
    "ESP32-S3_8MB_opi",
    "ESP32-S3_WROOM-2",
    "ESP32-S3_4M_qspi",
    "ESP32-S3",
    "ESP32-S2",
    "ESP32-C3",
    "ESP32_V4",
    "ESP32_8M",
    "ESP32_16M", 
    "ESP32",
    "ESP8266_160",
    "ESP8266_compat",
    "ESP8266",
    "ESP02_compat",
    "ESP02_160",
    "ESP02",
    "ESP01_compat", 
    "ESP01_160",
    "ESP01",
    "Custom",
    NULL // sentinel
  };
  
  // First try to find the exact current releaseString in the binary
  // This is the most reliable method if the string exists  
  int pos = findStringInBinary(binaryData, scanSize, releaseString);
  if (pos >= 0) {
    strcpy(extractedRelease, releaseString);
    DEBUG_PRINTF_P(PSTR("Found exact current release string in binary: %s\n"), extractedRelease);
    return true;
  }
  
  // Search through the binary for known release patterns
  // We'll search through the entire scanSize, not just fixed positions
  for (int i = 0; releasePatterns[i] != NULL; i++) {
    pos = findStringInBinary(binaryData, scanSize, releasePatterns[i]);
    if (pos >= 0) {
      // Found a potential release name, but verify it's a standalone string
      // Check that it's null-terminated and not part of a longer string
      size_t patternLen = strlen(releasePatterns[i]);
      
      // Verify the string is properly null-terminated
      if (pos + patternLen < scanSize && binaryData[pos + patternLen] == 0) {
        // Also check that it's not preceded by alphanumeric character (to avoid partial matches)
        if (pos == 0 || !isalnum(binaryData[pos - 1])) {
          strcpy(extractedRelease, releasePatterns[i]);
          DEBUG_PRINTF_P(PSTR("Found release name pattern: %s\n"), extractedRelease);
          return true;
        }
      }
    }
  }
  
  DEBUG_PRINTLN(F("Could not extract release name from binary"));
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
  
  // If user chose to ignore release check, allow OTA
  if (ignoreReleaseCheck) {
    DEBUG_PRINTLN(F("OTA release check bypassed by user"));
    return true;
  }
  
  // Try to extract release name from binary
  char extractedRelease[64];
  if (!extractReleaseNameFromBinary(binaryData, dataSize, extractedRelease)) {
    if (errorMessage) {
      strcpy(errorMessage, "Could not determine release type of uploaded file. Check 'Ignore release name check' to proceed.");
    }
    DEBUG_PRINTLN(F("OTA blocked: Could not extract release name"));
    return false;
  }
  
  // Validate compatibility
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