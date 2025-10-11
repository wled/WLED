#include "ota_update.h"
#include "wled.h"

#ifdef ESP32
#include <esp_ota_ops.h>
#endif

// Platform-specific metadata locations
#ifdef ESP32
constexpr size_t METADATA_OFFSET = 256;          // ESP32: metadata appears after Espressif metadata
#define UPDATE_ERROR errorString
#elif defined(ESP8266)
constexpr size_t METADATA_OFFSET = 0x1000;     // ESP8266: metadata appears at 4KB offset
#define UPDATE_ERROR getErrorString
#endif
constexpr size_t METADATA_SEARCH_RANGE = 512;  // bytes


/**
 * Check if OTA should be allowed based on release compatibility using custom description
 * @param binaryData Pointer to binary file data (not modified)
 * @param dataSize Size of binary data in bytes
 * @param errorMessage Buffer to store error message if validation fails 
 * @param errorMessageLen Maximum length of error message buffer
 * @return true if OTA should proceed, false if it should be blocked
 */

static bool validateOTA(const uint8_t* binaryData, size_t dataSize, char* errorMessage, size_t errorMessageLen) {
  // Clear error message
  if (errorMessage && errorMessageLen > 0) {
    errorMessage[0] = '\0';
  }

  // Try to extract WLED structure directly from binary data
  wled_metadata_t extractedDesc;
  bool hasDesc = findWledMetadata(binaryData, dataSize, &extractedDesc);

  if (hasDesc) {
    return shouldAllowOTA(extractedDesc, errorMessage, errorMessageLen);
  } else {
    // No custom description - this could be a legacy binary
    if (errorMessage && errorMessageLen > 0) {
      strncpy_P(errorMessage, PSTR("This firmware file is missing compatibility metadata."), errorMessageLen - 1);
      errorMessage[errorMessageLen - 1] = '\0';
    }
    return false;
  }
}

struct UpdateContext {
  // State flags
  // FUTURE: the flags could be replaced by a state machine
  bool replySent = false;
  bool needsRestart = false;
  bool updateStarted = false;
  bool uploadComplete = false;
  bool releaseCheckPassed = false;
  String errorMessage;

  // Buffer to hold block data across posts, if needed
  std::vector<uint8_t> releaseMetadataBuffer;  
};


static void endOTA(AsyncWebServerRequest *request) {
  UpdateContext* context = reinterpret_cast<UpdateContext*>(request->_tempObject);
  request->_tempObject = nullptr;

  DEBUG_PRINTF_P(PSTR("EndOTA %x --> %x (%d)\n"), (uintptr_t)request,(uintptr_t) context, context ? context->uploadComplete : 0);
  if (context) {
    if (context->updateStarted) {  // We initialized the update
      // We use Update.end() because not all forms of Update() support an abort.
      // If the upload is incomplete, Update.end(false) should error out.
      if (Update.end(context->uploadComplete)) {
        // Update successful!
        doReboot = true;
        context->needsRestart = false;
      }
    }

    if (context->needsRestart) {
      strip.resume();
      UsermodManager::onUpdateBegin(false);
      #if WLED_WATCHDOG_TIMEOUT > 0
      WLED::instance().enableWatchdog();
      #endif
    }
    delete context;
  }
};

static bool beginOTA(AsyncWebServerRequest *request, UpdateContext* context)
{
  #ifdef ESP8266
  Update.runAsync(true);
  #endif  

  if (Update.isRunning()) {
      request->send(503);
      setOTAReplied(request);
      return false;
  }

  #if WLED_WATCHDOG_TIMEOUT > 0
  WLED::instance().disableWatchdog();
  #endif
  UsermodManager::onUpdateBegin(true); // notify usermods that update is about to begin (some may require task de-init)
  
  strip.suspend();
  strip.resetSegments();  // free as much memory as you can
  context->needsRestart = true;

  DEBUG_PRINTF_P(PSTR("OTA Update Start, %x --> %x\n"), (uintptr_t)request,(uintptr_t) context);

  auto skipValidationParam = request->getParam("skipValidation", true);
  if (skipValidationParam && (skipValidationParam->value() == "1")) {
    context->releaseCheckPassed = true;
    DEBUG_PRINTLN(F("OTA validation skipped by user"));
  }
  
  // Begin update with the firmware size from content length
  size_t updateSize = request->contentLength() > 0 ? request->contentLength() : ((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
  if (!Update.begin(updateSize)) {    
    context->errorMessage = Update.UPDATE_ERROR();
    DEBUG_PRINTF_P(PSTR("OTA Failed to begin: %s\n"), context->errorMessage.c_str());
    return false;
  }
  
  context->updateStarted = true;
  return true;
}

// Create an OTA context object on an AsyncWebServerRequest
// Returns true if successful, false on failure.
bool initOTA(AsyncWebServerRequest *request) {
  // Allocate update context
  UpdateContext* context = new (std::nothrow) UpdateContext {};  
  if (context) {
    request->_tempObject = context;
    request->onDisconnect([=]() { endOTA(request); });  // ensures we restart on failure
  };

  DEBUG_PRINTF_P(PSTR("OTA Update init, %x --> %x\n"), (uintptr_t)request,(uintptr_t) context);
  return (context != nullptr);
}

void setOTAReplied(AsyncWebServerRequest *request) {
  UpdateContext* context = reinterpret_cast<UpdateContext*>(request->_tempObject);
  if (!context) return;
  context->replySent = true;
};

// Returns pointer to error message, or nullptr if OTA was successful.
std::pair<bool, String> getOTAResult(AsyncWebServerRequest* request) {
  UpdateContext* context = reinterpret_cast<UpdateContext*>(request->_tempObject);
  if (!context) return { true, F("OTA context unexpectedly missing") };
  if (context->replySent) return { false, {} };
  if (context->errorMessage.length()) return { true, context->errorMessage };

  if (context->updateStarted) {
    // Release the OTA context now.
    endOTA(request);
    if (Update.hasError()) {
      return { true, Update.UPDATE_ERROR() };
    } else {
      return { true, {} };
    }
  }

  // Should never happen
  return { true, F("Internal software failure") };
}



void handleOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal)
{
  UpdateContext* context = reinterpret_cast<UpdateContext*>(request->_tempObject);
  if (!context) return;

  //DEBUG_PRINTF_P(PSTR("HandleOTAData: %d %d %d\n"), index, len, isFinal);

  if (context->replySent || (context->errorMessage.length())) return;

  if (index == 0) {
    if (!beginOTA(request, context)) return;
  }

  // Perform validation if we haven't done it yet and we have reached the metadata offset
  if (!context->releaseCheckPassed && (index+len) > METADATA_OFFSET) {
    // Current chunk contains the metadata offset
    size_t availableDataAfterOffset = (index + len) - METADATA_OFFSET;

    DEBUG_PRINTF_P(PSTR("OTA metadata check: %d in buffer, %d received, %d available\n"), context->releaseMetadataBuffer.size(), len, availableDataAfterOffset);

    if (availableDataAfterOffset >= METADATA_SEARCH_RANGE) {
      // We have enough data to validate, one way or another
      const uint8_t* search_data = data;
      size_t search_len = len;
      
      // If we have saved data, use that instead
      if (context->releaseMetadataBuffer.size()) {
        // Add this data
        context->releaseMetadataBuffer.insert(context->releaseMetadataBuffer.end(), data, data+len);
        search_data = context->releaseMetadataBuffer.data();
        search_len = context->releaseMetadataBuffer.size();
      }

      // Do the checking
      char errorMessage[128];
      bool OTA_ok = validateOTA(search_data, search_len, errorMessage, sizeof(errorMessage));
      
      // Release buffer if there was one
      context->releaseMetadataBuffer = decltype(context->releaseMetadataBuffer){};
      
      if (!OTA_ok) {
        DEBUG_PRINTF_P(PSTR("OTA declined: %s\n"), errorMessage);
        context->errorMessage = errorMessage;
        context->errorMessage += F(" Enable 'Ignore firmware validation' to proceed anyway.");
        return;
      } else {
        DEBUG_PRINTLN(F("OTA allowed: Release compatibility check passed"));
        context->releaseCheckPassed = true;
      }        
    } else {
      // Store the data we just got for next pass
      context->releaseMetadataBuffer.insert(context->releaseMetadataBuffer.end(), data, data+len);
    }
  }

  // Check if validation was still pending (shouldn't happen normally)
  // This is done before writing the last chunk, so endOTA can abort 
  if (isFinal && !context->releaseCheckPassed) {
    DEBUG_PRINTLN(F("OTA failed: Validation never completed"));
    // Don't write the last chunk to the updater: this will trip an error later
    context->errorMessage = F("Release check data never arrived?");
    return;
  }

  // Write chunk data to OTA update (only if release check passed or still pending)
  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      DEBUG_PRINTF_P(PSTR("OTA write failed on chunk %zu: %s\n"), index, Update.UPDATE_ERROR());
    }
  }

  if(isFinal) {
    DEBUG_PRINTLN(F("OTA Update End"));
    // Upload complete
    context->uploadComplete = true;
  }
}