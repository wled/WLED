#include "ota_update.h"
#include "wled.h"

#ifdef ESP32
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>
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
#define BOOTLOADER_OTA_UNSUPPORTED    // still needs validation on these platforms.
#elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32C5)
constexpr size_t BOOTLOADER_OFFSET = 0x2000; // (future support) esp32-P4 and esp32-C5
constexpr size_t BOOTLOADER_SIZE   = 0x8000; // 32KB, typical bootloader size
#define BOOTLOADER_OTA_UNSUPPORTED    // still needs testing on these platforms
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
  backupConfig(); // backup current config in case the update ends badly
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

void markOTAvalid() {
  #ifndef ESP8266
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback(); // only needs to be called once, it marks the ota_state as ESP_OTA_IMG_VALID
      DEBUG_PRINTLN(F("Current firmware validated"));
    }
  }
  #endif
}

#if defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DISABLE_OTA)

// Class for computing the expected bootloader data size given a stream of the data.
// If the image includes an SHA256 appended after the data stream, we do not consider it here.
class BootloaderImageSizer {
public:

  bool feed(const uint8_t* data, size_t len) {
    if (error) return false;

    //DEBUG_PRINTF("Feed %d\n", len);

    if (imageSize == 0) {
      // Parse header first
      if (len < sizeof(esp_image_header_t)) {
        error = true;
        return false;
      }

      esp_image_header_t header;
      memcpy(&header, data, sizeof(esp_image_header_t));

      if (header.segment_count == 0) {
        error = true;
        return false;
      }

      imageSize = sizeof(esp_image_header_t);
      segmentsLeft = header.segment_count;
      data += sizeof(esp_image_header_t);
      len -= sizeof(esp_image_header_t);
      //DEBUG_PRINTF("BLS parsed image header, segment count %d, is %d\n", segmentsLeft, imageSize);
    }

    while (len && segmentsLeft) {
      if (segmentHeaderBytes < sizeof(esp_image_segment_header_t)) {
        size_t headerBytes = std::min(len, sizeof(esp_image_segment_header_t) - segmentHeaderBytes);
        memcpy(reinterpret_cast<uint8_t*>(&segmentHeader) + segmentHeaderBytes, data, headerBytes);
        segmentHeaderBytes += headerBytes;
        if (segmentHeaderBytes < sizeof(esp_image_segment_header_t)) {
          return true;  // needs more bytes for the header
        }

        //DEBUG_PRINTF("BLS parsed segment [%08X %08X=%d], segment count %d, is %d\n", segmentHeader.load_addr, segmentHeader.data_len, segmentHeader.data_len, segmentsLeft, imageSize);        

        // Validate segment size
        if (segmentHeader.data_len > BOOTLOADER_SIZE) {
          error = true;
          return false;
        }

        data += headerBytes;
        len -= headerBytes;
        imageSize += sizeof(esp_image_segment_header_t) + segmentHeader.data_len;
        --segmentsLeft;
        if (segmentsLeft == 0) {
          // all done, actually; we don't need to read any more
 
          // Round up to nearest 16 bytes.
          // Always add 1 to account for the checksum byte.
          imageSize = ((imageSize/ 16) + 1) * 16;

          //DEBUG_PRINTF("BLS complete, is %d\n", imageSize);        
          return false;
        }        
      }
      
      // If we don't have enough bytes ...
      if (len < segmentHeader.data_len) {
        //DEBUG_PRINTF("Needs more bytes\n");
        segmentHeader.data_len -= len;
        return true;  // still in this segment
      }

      // Segment complete
      len -= segmentHeader.data_len;
      data += segmentHeader.data_len;
      segmentHeaderBytes = 0;
      //DEBUG_PRINTF("Segment complete: len %d\n", len);
    }

    return !error;
  }

  bool hasError() const { return error; }
  bool isSizeKnown() const { return !error && imageSize != 0 && segmentsLeft == 0; }
  size_t totalSize() const {
    if (!isSizeKnown()) return 0;
    return imageSize;
  }

private:
  size_t imageSize = 0;
  size_t segmentsLeft = 0;
  esp_image_segment_header_t segmentHeader;
  size_t segmentHeaderBytes = 0;
  bool error = false;
};

static bool bootloaderSHA256CacheValid = false;
static uint8_t bootloaderSHA256Cache[32];

/**
 * Calculate and cache the bootloader SHA256 digest
 * Reads the bootloader from flash and computes SHA256 hash
 * 
 * Strictly speaking, most bootloader images already contain a hash at the end of the image; 
 * we could in theory just read it.  The trouble is that we have to parse the structure anyways
 * to find the actual endpoint, so we might as well always calculate it ourselves rather than
 * handle a special case if the hash isn't stored.
 * 
 */
static void calculateBootloaderSHA256() {
  // Calculate SHA256
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 0 = SHA256 (not SHA224)

  const size_t chunkSize = 256;
  alignas(esp_image_header_t) uint8_t buffer[chunkSize];
  size_t bootloaderSize = BOOTLOADER_SIZE;
  BootloaderImageSizer sizer;
  size_t totalHashLen = 0;

  for (uint32_t offset = 0; offset < bootloaderSize; offset += chunkSize) {
    size_t readSize = min((size_t)(bootloaderSize - offset), chunkSize);
    if (esp_flash_read(NULL, buffer, BOOTLOADER_OFFSET + offset, readSize) == ESP_OK) {
      sizer.feed(buffer, readSize);

      size_t hashLen = readSize;
      if (sizer.isSizeKnown()) {
        size_t totalSize = sizer.totalSize();
        if (totalSize > 0 && totalSize <= BOOTLOADER_SIZE) {
          bootloaderSize = totalSize;
          if (offset + readSize > totalSize) {
            hashLen = (totalSize > offset) ? (totalSize - offset) : 0;
          }
        }
      }

      if (hashLen > 0) {
        totalHashLen += hashLen;
        mbedtls_sha256_update(&ctx, buffer, hashLen);
      }
    }
  }

  mbedtls_sha256_finish(&ctx, bootloaderSHA256Cache);
  mbedtls_sha256_free(&ctx);

  bootloaderSHA256CacheValid = true;
}

// Get bootloader SHA256 as hex string
String getBootloaderSHA256Hex() {
  if (!bootloaderSHA256CacheValid) {
    calculateBootloaderSHA256();
  }

  // Convert to hex string
  String result;
  result.reserve(65);
  for (int i = 0; i < 32; i++) {
    char b1 = bootloaderSHA256Cache[i];
    char b2 = b1 >> 4;
    b1 &= 0x0F;
    b1 += '0'; b2 += '0';
    if (b1 > '9') b1 += 39;
    if (b2 > '9') b2 += 39;
    result.concat(b2);
    result.concat(b1);
  }
  return result;
}

/**
 * Invalidate cached bootloader SHA256 (call after bootloader update)
 * Forces recalculation on next call to calculateBootloaderSHA256 or getBootloaderSHA256Hex
 */
static void invalidateBootloaderSHA256Cache() {
  bootloaderSHA256CacheValid = false;
}

/**
 * Verify complete buffered bootloader using ESP-IDF validation approach
 * This matches the key validation steps from esp_image_verify() in ESP-IDF
 * @param buffer Reference to pointer to bootloader binary data (will be adjusted if offset detected)
 * @param len Reference to length of bootloader data (will be adjusted to actual size)
 * @param bootloaderErrorMsg Pointer to String to store error message (must not be null)
 * @return true if validation passed, false otherwise
 */
static bool verifyBootloaderImage(const uint8_t* &buffer, size_t &len, String& bootloaderErrorMsg) {
  const size_t MIN_IMAGE_HEADER_SIZE = sizeof(esp_image_header_t);

  // 1. Validate minimum size for header
  if (len < MIN_IMAGE_HEADER_SIZE) {
    bootloaderErrorMsg = "Too small";
    return false;
  }

  // Check if the bootloader starts at offset 0x1000 (common in partition table dumps)
  // This happens when someone uploads a complete flash dump instead of just the bootloader
  if (len > BOOTLOADER_OFFSET + MIN_IMAGE_HEADER_SIZE &&
      buffer[BOOTLOADER_OFFSET] == ESP_IMAGE_HEADER_MAGIC &&
      buffer[0] != ESP_IMAGE_HEADER_MAGIC) {
    DEBUG_PRINTF_P(PSTR("Bootloader detected at offset\n"));
    // Adjust buffer pointer to start at the actual bootloader
    buffer = buffer + BOOTLOADER_OFFSET;
    len = len - BOOTLOADER_OFFSET;

    // Re-validate size after adjustment
    if (len < MIN_IMAGE_HEADER_SIZE) {
      bootloaderErrorMsg = "Too small";
      return false;
    }
  }

  size_t availableLen = len;
  esp_image_header_t imageHeader{};
  memcpy(&imageHeader, buffer, sizeof(imageHeader));

  // 2. Basic header sanity checks (matches early esp_image_verify checks)
  if (imageHeader.magic != ESP_IMAGE_HEADER_MAGIC ||
      imageHeader.segment_count == 0 || imageHeader.segment_count > 16 ||
      imageHeader.spi_mode > 3 ||
      imageHeader.entry_addr < 0x40000000 || imageHeader.entry_addr > 0x50000000) {
    bootloaderErrorMsg = "Invalid header";
    return false;
  }

  // 3. Chip ID validation (matches esp_image_verify step 3)
  if (imageHeader.chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
    bootloaderErrorMsg = "Chip ID mismatch";
    return false;
  }

  // 4. Validate image size
  BootloaderImageSizer sizer;
  sizer.feed(buffer, availableLen);
  if (!sizer.isSizeKnown()) {
    bootloaderErrorMsg = "Invalid image";
    return false;
  }
  size_t actualBootloaderSize = sizer.totalSize();

  // 5. SHA256 checksum (optional)
  if (imageHeader.hash_appended == 1) {
    actualBootloaderSize += 32;
  }
 
  if (actualBootloaderSize > len) {
    // Same as above
    bootloaderErrorMsg = "Too small";
    return false;
  }

  DEBUG_PRINTF_P(PSTR("Bootloader validation: %d segments, actual size %d bytes (buffer size %d bytes, hash_appended=%d)\n"),
                 imageHeader.segment_count, actualBootloaderSize, len, imageHeader.hash_appended);

  // Update len to reflect actual bootloader size (including hash and checksum, with alignment)
  // This is critical - we must write the complete image including checksums
  len = actualBootloaderSize;

  return true;
}

// Bootloader OTA context structure
struct BootloaderUpdateContext {
  // State flags
  bool replySent = false;
  bool uploadComplete = false;
  String errorMessage;

  // Buffer to hold bootloader data
  uint8_t* buffer = nullptr;
  size_t bytesBuffered = 0;
  const uint32_t bootloaderOffset = 0x1000;
  const uint32_t maxBootloaderSize = 0x10000; // 64KB buffer size
};

// Cleanup bootloader OTA context
static void endBootloaderOTA(AsyncWebServerRequest *request) {
  BootloaderUpdateContext* context = reinterpret_cast<BootloaderUpdateContext*>(request->_tempObject);
  request->_tempObject = nullptr;

  DEBUG_PRINTF_P(PSTR("EndBootloaderOTA %x --> %x\n"), (uintptr_t)request, (uintptr_t)context);
  if (context) {
    if (context->buffer) {
      free(context->buffer);
      context->buffer = nullptr;
    }

    // If update failed, restore system state
    if (!context->uploadComplete || !context->errorMessage.isEmpty()) {
      strip.resume();
      #if WLED_WATCHDOG_TIMEOUT > 0
      WLED::instance().enableWatchdog();
      #endif
    }

    delete context;
  }
}

// Initialize bootloader OTA context
bool initBootloaderOTA(AsyncWebServerRequest *request) {
  if (request->_tempObject) {
    return true; // Already initialized
  }

  BootloaderUpdateContext* context = new BootloaderUpdateContext();
  if (!context) {
    DEBUG_PRINTLN(F("Failed to allocate bootloader OTA context"));
    return false;
  }
  request->_tempObject = context;
  request->onDisconnect([=]() { endBootloaderOTA(request); });  // ensures cleanup on disconnect

#ifdef BOOTLOADER_OTA_UNSUPPORTED
  context->errorMessage = F("Bootloader update not supported on this chip");
  return false;
#else
  DEBUG_PRINTLN(F("Bootloader Update Start - initializing buffer"));
  #if WLED_WATCHDOG_TIMEOUT > 0
  WLED::instance().disableWatchdog();
  #endif
  lastEditTime = millis(); // make sure PIN does not lock during update
  strip.suspend();
  strip.resetSegments();

  // Check available heap before attempting allocation
  DEBUG_PRINTF_P(PSTR("Free heap before bootloader buffer allocation: %d bytes (need %d bytes)\n"), getContiguousFreeHeap(), context->maxBootloaderSize);

  context->buffer = (uint8_t*)malloc(context->maxBootloaderSize);
  if (!context->buffer) {
    size_t freeHeapNow = getContiguousFreeHeap();
    DEBUG_PRINTF_P(PSTR("Failed to allocate %d byte bootloader buffer! Contiguous heap: %d bytes\n"), context->maxBootloaderSize, freeHeapNow);
    context->errorMessage = "Out of memory! Contiguous heap: " + String(freeHeapNow) + " bytes, need: " + String(context->maxBootloaderSize) + " bytes";
    strip.resume();
    #if WLED_WATCHDOG_TIMEOUT > 0
    WLED::instance().enableWatchdog();
    #endif
    return false;
  }

  context->bytesBuffered = 0;
  return true;
#endif  
}

// Set bootloader OTA replied flag
void setBootloaderOTAReplied(AsyncWebServerRequest *request) {
  BootloaderUpdateContext* context = reinterpret_cast<BootloaderUpdateContext*>(request->_tempObject);
  if (context) {
    context->replySent = true;
  }
}

// Get bootloader OTA result
std::pair<bool, String> getBootloaderOTAResult(AsyncWebServerRequest *request) {
  BootloaderUpdateContext* context = reinterpret_cast<BootloaderUpdateContext*>(request->_tempObject);

  if (!context) {
    return std::make_pair(true, String(F("Internal error: No bootloader OTA context")));
  }

  bool needsReply = !context->replySent;
  String errorMsg = context->errorMessage;

  // If upload was successful, return empty string and trigger reboot
  if (context->uploadComplete && errorMsg.isEmpty()) {
    doReboot = true;
    endBootloaderOTA(request);
    return std::make_pair(needsReply, String());
  }

  // If there was an error, return it
  if (!errorMsg.isEmpty()) {
    endBootloaderOTA(request);
    return std::make_pair(needsReply, errorMsg);
  }

  // Should never happen
  return std::make_pair(true, String(F("Internal software failure")));
}

// Handle bootloader OTA data
void handleBootloaderOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal) {
  BootloaderUpdateContext* context = reinterpret_cast<BootloaderUpdateContext*>(request->_tempObject);

  if (!context) {
    DEBUG_PRINTLN(F("No bootloader OTA context - ignoring data"));
    return;
  }

  if (!context->errorMessage.isEmpty()) {
    return;
  }

  // Buffer the incoming data
  if (context->buffer && context->bytesBuffered + len <= context->maxBootloaderSize) {
    memcpy(context->buffer + context->bytesBuffered, data, len);
    context->bytesBuffered += len;
    DEBUG_PRINTF_P(PSTR("Bootloader buffer progress: %d / %d bytes\n"), context->bytesBuffered, context->maxBootloaderSize);
  } else if (!context->buffer) {
    DEBUG_PRINTLN(F("Bootloader buffer not allocated!"));
    context->errorMessage = "Internal error: Bootloader buffer not allocated";
    return;
  } else {
    size_t totalSize = context->bytesBuffered + len;
    DEBUG_PRINTLN(F("Bootloader size exceeds maximum!"));
    context->errorMessage = "Bootloader file too large: " + String(totalSize) + " bytes (max: " + String(context->maxBootloaderSize) + " bytes)";
    return;
  }

  // Only write to flash when upload is complete
  if (isFinal) {
    DEBUG_PRINTLN(F("Bootloader Upload Complete - validating and flashing"));

    if (context->buffer && context->bytesBuffered > 0) {
      // Prepare pointers for verification (may be adjusted if bootloader at offset)
      const uint8_t* bootloaderData = context->buffer;
      size_t bootloaderSize = context->bytesBuffered;

      // Verify the complete bootloader image before flashing
      // Note: verifyBootloaderImage may adjust bootloaderData pointer and bootloaderSize
      // for validation purposes only
      if (!verifyBootloaderImage(bootloaderData, bootloaderSize, context->errorMessage)) {
        DEBUG_PRINTLN(F("Bootloader validation failed!"));
        // Error message already set by verifyBootloaderImage
      } else {
        // Calculate offset to write to flash
        // If bootloaderData was adjusted (partition table detected), we need to skip it in flash too
        size_t flashOffset = context->bootloaderOffset;
        const uint8_t* dataToWrite = context->buffer;
        size_t bytesToWrite = context->bytesBuffered;

        // If validation adjusted the pointer, it means we have a partition table at the start
        // In this case, we should skip writing the partition table and write bootloader at 0x1000
        if (bootloaderData != context->buffer) {
          // bootloaderData was adjusted - skip partition table in our data
          size_t partitionTableSize = bootloaderData - context->buffer;
          dataToWrite = bootloaderData;
          bytesToWrite = bootloaderSize;
          DEBUG_PRINTF_P(PSTR("Skipping %d bytes of partition table data\n"), partitionTableSize);
        }

        DEBUG_PRINTF_P(PSTR("Bootloader validation passed - writing %d bytes to flash at 0x%04X\n"),
                       bytesToWrite, flashOffset);

        // Calculate erase size (must be multiple of 4KB)
        size_t eraseSize = ((bytesToWrite + 0xFFF) / 0x1000) * 0x1000;
        if (eraseSize > context->maxBootloaderSize) {
          eraseSize = context->maxBootloaderSize;
        }

        // Erase bootloader region
        DEBUG_PRINTF_P(PSTR("Erasing %d bytes at 0x%04X...\n"), eraseSize, flashOffset);
        esp_err_t err = esp_flash_erase_region(NULL, flashOffset, eraseSize);
        if (err != ESP_OK) {
          DEBUG_PRINTF_P(PSTR("Bootloader erase error: %d\n"), err);
          context->errorMessage = "Flash erase failed (error code: " + String(err) + ")";
        } else {
          // Write the validated bootloader data to flash
          err = esp_flash_write(NULL, dataToWrite, flashOffset, bytesToWrite);
          if (err != ESP_OK) {
            DEBUG_PRINTF_P(PSTR("Bootloader flash write error: %d\n"), err);
            context->errorMessage = "Flash write failed (error code: " + String(err) + ")";
          } else {
            DEBUG_PRINTF_P(PSTR("Bootloader Update Success - %d bytes written to 0x%04X\n"),
                           bytesToWrite, flashOffset);
            // Invalidate cached bootloader hash
            invalidateBootloaderSHA256Cache();
            context->uploadComplete = true;
          }
        }
      }
    } else if (context->bytesBuffered == 0) {
      context->errorMessage = "No bootloader data received";
    }
  }
}
#endif
