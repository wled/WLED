#include "ota_update.h"
#include "wled.h"

#ifdef ESP32
#include <esp_ota_ops.h>
#include <esp_spi_flash.h>
#include <mbedtls/sha256.h>
#endif

// Platform-specific metadata locations
#ifdef ESP32
constexpr size_t METADATA_OFFSET = 256;          // ESP32: metadata appears after Espressif metadata
#define UPDATE_ERROR errorString

// Bootloader is at fixed offset 0x1000 (4KB), 0x0000 (0KB), or 0x2000 (8KB), and is typically 32KB
// Bootloader offsets for different MCUs => see https://github.com/wled/WLED/issues/5064
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
constexpr size_t BOOTLOADER_OFFSET = 0x0000; // esp32-S3, esp32-C3 and (future support) esp32-c6
constexpr size_t BOOTLOADER_SIZE   = 0x8000; // 32KB, typical bootloader size
#elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5)
constexpr size_t BOOTLOADER_OFFSET = 0x2000; // (future support) esp32-P4 and esp32-C5
constexpr size_t BOOTLOADER_SIZE   = 0x8000; // 32KB, typical bootloader size
#else
constexpr size_t BOOTLOADER_OFFSET = 0x1000; // esp32 and esp32-s2
constexpr size_t BOOTLOADER_SIZE   = 0x8000; // 32KB, typical bootloader size
#endif

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
        #ifndef ESP8266
        bootloopCheckOTA(); // let the bootloop-checker know there was an OTA update
        #endif
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
  backupConfig(); // backup current config in case the update ends badly

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

#if defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DISABLE_OTA)
static String bootloaderSHA256HexCache = "";

// Calculate and cache the bootloader SHA256 digest as hex string
void calculateBootloaderSHA256() {
  if (!bootloaderSHA256HexCache.isEmpty()) return;

  // Calculate SHA256
  uint8_t sha256[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 0 = SHA256 (not SHA224)

  const size_t chunkSize = 256;
  uint8_t buffer[chunkSize];

  for (uint32_t offset = 0; offset < BOOTLOADER_SIZE; offset += chunkSize) {
    size_t readSize = min((size_t)(BOOTLOADER_SIZE - offset), chunkSize);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    if (esp_flash_read(NULL, buffer, BOOTLOADER_OFFSET + offset, readSize) == ESP_OK) {    // use esp_flash_read for V4 framework (-S2, -S3, -C3)
#else
    if (spi_flash_read(BOOTLOADER_OFFSET + offset, buffer, readSize) == ESP_OK) {          // use spi_flash_read for old V3 framework (legacy esp32)
#endif
      mbedtls_sha256_update(&ctx, buffer, readSize);
    }
  }

  mbedtls_sha256_finish(&ctx, sha256);
  mbedtls_sha256_free(&ctx);

  // Convert to hex string and cache it
  char hex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(hex + (i * 2), "%02x", sha256[i]);
  }
  hex[64] = '\0';
  bootloaderSHA256HexCache = hex;
}

// Get bootloader SHA256 as hex string
String getBootloaderSHA256Hex() {
  calculateBootloaderSHA256();
  return bootloaderSHA256HexCache;
}

// Invalidate cached bootloader SHA256 (call after bootloader update)
void invalidateBootloaderSHA256Cache() {
  bootloaderSHA256HexCache = "";
}
#endif