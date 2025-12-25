//  WLED OTA update interface

#include <Arduino.h>
#ifdef ESP8266
  #include <Updater.h>
#else
   #include <Update.h>
#endif

#pragma once

// Platform-specific metadata locations
#ifdef ESP32
#define BUILD_METADATA_SECTION ".rodata_custom_desc"
#elif defined(ESP8266)
#define BUILD_METADATA_SECTION ".ver_number"
#endif


class AsyncWebServerRequest;

/**
 *  Create an OTA context object on an AsyncWebServerRequest
 * @param request Pointer to web request object
 * @return true if allocation was successful, false if not
 */
bool initOTA(AsyncWebServerRequest *request);

/**
 *  Indicate to the OTA subsystem that a reply has already been generated
 * @param request Pointer to web request object
 */
void setOTAReplied(AsyncWebServerRequest *request);

/**
 *  Retrieve the OTA result.
 * @param request Pointer to web request object
 * @return bool indicating if a reply is necessary; string with error message if the update failed.
 */
std::pair<bool, String> getOTAResult(AsyncWebServerRequest *request);

/**
 *  Process a block of OTA data.  This is a passthrough of an ArUploadHandlerFunction.
 * Requires that initOTA be called on the handler object before any work will be done.
 * @param request Pointer to web request object
 * @param index Offset in to uploaded file
 * @param data New data bytes
 * @param len Length of new data bytes
 * @param isFinal Indicates that this is the last block
 * @return bool indicating if a reply is necessary; string with error message if the update failed.
 */
void handleOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal);

/**
 * Mark currently running firmware as valid to prevent auto-rollback on reboot.
 * This option can be enabled in some builds/bootloaders, it is an sdkconfig flag.
 */
void markOTAvalid();

#if defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DISABLE_OTA)
/**
 * Calculate and cache the bootloader SHA256 digest
 * Reads the bootloader from flash at offset 0x1000 and computes SHA256 hash
 */
void calculateBootloaderSHA256();

/**
 * Get bootloader SHA256 as hex string
 * @return String containing 64-character hex representation of SHA256 hash
 */
String getBootloaderSHA256Hex();

/**
 * Invalidate cached bootloader SHA256 (call after bootloader update)
 * Forces recalculation on next call to calculateBootloaderSHA256 or getBootloaderSHA256Hex
 */
void invalidateBootloaderSHA256Cache();

/**
 * Verify complete buffered bootloader using ESP-IDF validation approach
 * This matches the key validation steps from esp_image_verify() in ESP-IDF
 * @param buffer Reference to pointer to bootloader binary data (will be adjusted if offset detected)
 * @param len Reference to length of bootloader data (will be adjusted to actual size)
 * @param bootloaderErrorMsg Pointer to String to store error message (must not be null)
 * @return true if validation passed, false otherwise
 */
bool verifyBootloaderImage(const uint8_t* &buffer, size_t &len, String* bootloaderErrorMsg);

/**
 * Create a bootloader OTA context object on an AsyncWebServerRequest
 * @param request Pointer to web request object
 * @return true if allocation was successful, false if not
 */
bool initBootloaderOTA(AsyncWebServerRequest *request);

/**
 * Indicate to the bootloader OTA subsystem that a reply has already been generated
 * @param request Pointer to web request object
 */
void setBootloaderOTAReplied(AsyncWebServerRequest *request);

/**
 * Retrieve the bootloader OTA result.
 * @param request Pointer to web request object
 * @return bool indicating if a reply is necessary; string with error message if the update failed.
 */
std::pair<bool, String> getBootloaderOTAResult(AsyncWebServerRequest *request);

/**
 * Process a block of bootloader OTA data. This is a passthrough of an ArUploadHandlerFunction.
 * Requires that initBootloaderOTA be called on the handler object before any work will be done.
 * @param request Pointer to web request object
 * @param index Offset in to uploaded file
 * @param data New data bytes
 * @param len Length of new data bytes
 * @param isFinal Indicates that this is the last block
 */
void handleBootloaderOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal);
#endif

