#include "ota_release_check.h"
#include "wled.h"

// Maximum size to scan in binary (we don't need to scan the entire file)
#define MAX_SCAN_SIZE 32768

/**
 * Find a string in binary data
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
 * Check if a string looks like a valid WLED release name
 */
static bool isValidReleaseNameFormat(const char* name, size_t len) {
  if (len < 3 || len > 63) return false;
  
  // Should contain at least one letter and be mostly alphanumeric with underscores/dashes
  bool hasLetter = false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (isalpha(c)) {
      hasLetter = true;
    } else if (!isdigit(c) && c != '_' && c != '-') {
      return false; // Invalid character
    }
  }
  
  return hasLetter; // Must have at least one letter
}

/**
 * Extract release name by searching for null-terminated strings that look like release names
 */
static bool extractByGenericStringSearch(const uint8_t* data, size_t dataSize, char* extractedRelease) {
  // Search for null-terminated strings that could be release names
  for (size_t i = 0; i < dataSize - 4; i++) {
    // Look for potential start of a string (printable character)
    if (isalpha(data[i])) {
      // Find the end of this string (null terminator)
      size_t j = i;
      while (j < dataSize && data[j] != 0) {
        j++;
      }
      
      if (j < dataSize) { // Found null terminator
        size_t len = j - i;
        if (len >= 3 && len <= 63) { // reasonable length for a release name
          char candidate[64];
          strncpy(candidate, (const char*)(data + i), len);
          candidate[len] = '\0';
          
          // Check if this looks like a valid release name
          if (isValidReleaseNameFormat(candidate, len)) {
            // Additional heuristics: common WLED release name patterns
            if (strstr(candidate, "ESP") != NULL ||     // Contains ESP
                strstr(candidate, "WLED") != NULL ||    // Contains WLED  
                strstr(candidate, "Custom") != NULL) {  // Custom build
              
              strcpy(extractedRelease, candidate);
              DEBUG_PRINTF_P(PSTR("Found release name by generic search: %s\n"), extractedRelease);
              return true;
            }
          }
        }
      }
    }
  }
  
  return false;
}

bool extractReleaseNameFromBinary(const uint8_t* binaryData, size_t dataSize, char* extractedRelease) {
  if (!binaryData || !extractedRelease || dataSize == 0) {
    return false;
  }
  
  // Limit scan size to avoid performance issues with large binaries
  size_t scanSize = (dataSize > MAX_SCAN_SIZE) ? MAX_SCAN_SIZE : dataSize;
  
  // First, try to find the exact current release string in the binary
  // This is the most reliable method since we know what we're looking for
  int pos = findStringInBinary(binaryData, scanSize, releaseString);
  if (pos >= 0) {
    // Verify it's properly null-terminated
    size_t releaseLen = strlen(releaseString);
    if (pos + releaseLen < scanSize && binaryData[pos + releaseLen] == 0) {
      strcpy(extractedRelease, releaseString);
      DEBUG_PRINTF_P(PSTR("Found exact current release string in binary: %s\n"), extractedRelease);
      return true;
    }
  }
  
  // Fallback: Search for any string that looks like a release name
  // This handles the case where the binary has a different but valid release name
  if (extractByGenericStringSearch(binaryData, scanSize, extractedRelease)) {
    return true;
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