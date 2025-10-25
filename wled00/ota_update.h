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
