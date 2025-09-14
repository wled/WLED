#ifndef WLED_OTA_RELEASE_CHECK_H
#define WLED_OTA_RELEASE_CHECK_H

/*
 * OTA Release Compatibility Checking with Metadata Headers
 * Functions to extract and validate release names from uploaded binary files with metadata headers
 */

#include <Arduino.h>

#define WLED_META_HEADER_SIZE 64
#define WLED_META_PREFIX "WLED_META:"

/**
 * Extract and remove metadata header from binary data
 * @param binaryData Pointer to binary file data (will be modified)
 * @param dataSize Size of binary data in bytes
 * @param extractedRelease Buffer to store extracted release name (should be at least 64 bytes)
 * @param actualBinarySize Pointer to store the size of actual firmware binary (without header)
 * @return true if metadata header was found and extracted, false if no metadata header present
 */
bool extractMetadataHeader(uint8_t* binaryData, size_t dataSize, char* extractedRelease, size_t* actualBinarySize);

/**
 * Validate if extracted release name matches current release
 * @param extractedRelease Release name from uploaded binary
 * @return true if releases match (OTA should proceed), false if they don't match
 */
bool validateReleaseCompatibility(const char* extractedRelease);

/**
 * Check if OTA should be allowed based on release compatibility using metadata headers
 * @param binaryData Pointer to binary file data (will be modified if metadata header present)
 * @param dataSize Size of binary data in bytes
 * @param ignoreReleaseCheck If true, skip release validation
 * @param errorMessage Buffer to store error message if validation fails (should be at least 128 bytes)
 * @param actualBinarySize Pointer to store the size of actual firmware binary (without header)
 * @return true if OTA should proceed, false if it should be blocked
 */
bool shouldAllowOTA(uint8_t* binaryData, size_t dataSize, bool ignoreReleaseCheck, char* errorMessage, size_t* actualBinarySize);

#endif // WLED_OTA_RELEASE_CHECK_H