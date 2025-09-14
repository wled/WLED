#ifndef WLED_OTA_RELEASE_CHECK_H
#define WLED_OTA_RELEASE_CHECK_H

/*
 * OTA Release Compatibility Checking
 * Functions to extract and validate release names from uploaded binary files
 */

#include <Arduino.h>

/**
 * Extract release name from ESP32/ESP8266 binary data
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedRelease Buffer to store extracted release name (should be at least 64 bytes)
 * @return true if release name was extracted successfully, false otherwise
 */
bool extractReleaseNameFromBinary(const uint8_t* binaryData, size_t dataSize, char* extractedRelease);

/**
 * Validate if extracted release name matches current release
 * @param extractedRelease Release name from uploaded binary
 * @return true if releases match (OTA should proceed), false if they don't match
 */
bool validateReleaseCompatibility(const char* extractedRelease);

/**
 * Check if OTA should be allowed based on release compatibility
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param ignoreReleaseCheck If true, skip release validation
 * @param errorMessage Buffer to store error message if validation fails (should be at least 128 bytes)
 * @return true if OTA should proceed, false if it should be blocked
 */
bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, bool ignoreReleaseCheck, char* errorMessage);

#endif // WLED_OTA_RELEASE_CHECK_H