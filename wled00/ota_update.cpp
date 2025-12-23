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
// Cache for bootloader SHA256 digest as hex string
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
    if (esp_flash_read(NULL, buffer, BOOTLOADER_OFFSET + offset, readSize) == ESP_OK) {
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
  bootloaderSHA256HexCache = String(hex);
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

// Verify complete buffered bootloader using ESP-IDF validation approach
// This matches the key validation steps from esp_image_verify() in ESP-IDF
// Returns the actual bootloader data pointer and length via the buffer and len parameters
bool verifyBootloaderImage(const uint8_t* &buffer, size_t &len, String* bootloaderErrorMsg) {
  size_t availableLen = len;
  if (!bootloaderErrorMsg) {
    DEBUG_PRINTLN(F("bootloaderErrorMsg is null"));
    return false;
  }
  // ESP32 image header structure (based on esp_image_format.h)
  // Offset 0: magic (0xE9)
  // Offset 1: segment_count
  // Offset 2: spi_mode
  // Offset 3: spi_speed (4 bits) + spi_size (4 bits)
  // Offset 4-7: entry_addr (uint32_t)
  // Offset 8: wp_pin
  // Offset 9-11: spi_pin_drv[3]
  // Offset 12-13: chip_id (uint16_t, little-endian)
  // Offset 14: min_chip_rev
  // Offset 15-22: reserved[8]
  // Offset 23: hash_appended

  const size_t MIN_IMAGE_HEADER_SIZE = 24;

  // 1. Validate minimum size for header
  if (len < MIN_IMAGE_HEADER_SIZE) {
    *bootloaderErrorMsg = "Bootloader too small - invalid header";
    return false;
  }

  // Check if the bootloader starts at offset 0x1000 (common in partition table dumps)
  // This happens when someone uploads a complete flash dump instead of just the bootloader
  if (len > BOOTLOADER_OFFSET + MIN_IMAGE_HEADER_SIZE &&
      buffer[BOOTLOADER_OFFSET] == 0xE9 &&
      buffer[0] != 0xE9) {
    DEBUG_PRINTF_P(PSTR("Bootloader magic byte detected at offset 0x%04X - adjusting buffer\n"), BOOTLOADER_OFFSET);
    // Adjust buffer pointer to start at the actual bootloader
    buffer = buffer + BOOTLOADER_OFFSET;
    len = len - BOOTLOADER_OFFSET;

    // Re-validate size after adjustment
    if (len < MIN_IMAGE_HEADER_SIZE) {
      *bootloaderErrorMsg = "Bootloader at offset 0x1000 too small - invalid header";
      return false;
    }
  }

  // 2. Magic byte check (matches esp_image_verify step 1)
  if (buffer[0] != 0xE9) {
    *bootloaderErrorMsg = "Invalid bootloader magic byte (expected 0xE9, got 0x" + String(buffer[0], HEX) + ")";
    return false;
  }

  // 3. Segment count validation (matches esp_image_verify step 2)
  uint8_t segmentCount = buffer[1];
  if (segmentCount == 0 || segmentCount > 16) {
    *bootloaderErrorMsg = "Invalid segment count: " + String(segmentCount);
    return false;
  }

  // 4. SPI mode validation (basic sanity check)
  uint8_t spiMode = buffer[2];
  if (spiMode > 3) {  // Valid modes are 0-3 (QIO, QOUT, DIO, DOUT)
    *bootloaderErrorMsg = "Invalid SPI mode: " + String(spiMode);
    return false;
  }

  // 5. Chip ID validation (matches esp_image_verify step 3)
  uint16_t chipId = buffer[12] | (buffer[13] << 8);  // Little-endian

  // Known ESP32 chip IDs from ESP-IDF:
  // 0x0000 = ESP32
  // 0x0002 = ESP32-S2
  // 0x0005 = ESP32-C3
  // 0x0009 = ESP32-S3
  // 0x000C = ESP32-C2
  // 0x000D = ESP32-C6
  // 0x0010 = ESP32-H2

  #if defined(CONFIG_IDF_TARGET_ESP32)
    if (chipId != 0x0000) {
      *bootloaderErrorMsg = "Chip ID mismatch - expected ESP32 (0x0000), got 0x" + String(chipId, HEX);
      return false;
    }
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    if (chipId != 0x0002) {
      *bootloaderErrorMsg = "Chip ID mismatch - expected ESP32-S2 (0x0002), got 0x" + String(chipId, HEX);
      return false;
    }
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    if (chipId != 0x0005) {
      *bootloaderErrorMsg = "Chip ID mismatch - expected ESP32-C3 (0x0005), got 0x" + String(chipId, HEX);
      return false;
    }
    *bootloaderErrorMsg = "ESP32-C3 update not supported yet";
    return false;
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    if (chipId != 0x0009) {
      *bootloaderErrorMsg = "Chip ID mismatch - expected ESP32-S3 (0x0009), got 0x" + String(chipId, HEX);
      return false;
    }
    *bootloaderErrorMsg = "ESP32-S3 update not supported yet";
    return false;
  #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    if (chipId != 0x000D) {
      *bootloaderErrorMsg = "Chip ID mismatch - expected ESP32-C6 (0x000D), got 0x" + String(chipId, HEX);
      return false;
    }
    *bootloaderErrorMsg = "ESP32-C6 update not supported yet";
    return false;
  #else
    // Generic validation - chip ID should be valid
    if (chipId > 0x00FF) {
      *bootloaderErrorMsg = "Invalid chip ID: 0x" + String(chipId, HEX);
      return false;
    }
   *bootloaderErrorMsg = "Unknown ESP32 target - bootloader update not supported";
   return false;
  #endif

  // 6. Entry point validation (should be in valid memory range)
  uint32_t entryAddr = buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24);
  // ESP32 bootloader entry points are typically in IRAM range (0x40000000 - 0x40400000)
  // or ROM range (0x40000000 and above)
  if (entryAddr < 0x40000000 || entryAddr > 0x50000000) {
    *bootloaderErrorMsg = "Invalid entry address: 0x" + String(entryAddr, HEX);
    return false;
  }

  // 7. Basic segment structure validation
  // Each segment has a header: load_addr (4 bytes) + data_len (4 bytes)
  size_t offset = MIN_IMAGE_HEADER_SIZE;
  size_t actualBootloaderSize = MIN_IMAGE_HEADER_SIZE;

  for (uint8_t i = 0; i < segmentCount && offset + 8 <= len; i++) {
    uint32_t segmentSize = buffer[offset + 4] | (buffer[offset + 5] << 8) |
                           (buffer[offset + 6] << 16) | (buffer[offset + 7] << 24);

    // Segment size sanity check
    // ESP32 classic bootloader segments can be larger, C3 are smaller
    if (segmentSize > 0x20000) {  // 128KB max per segment (very generous)
      *bootloaderErrorMsg = "Segment " + String(i) + " too large: " + String(segmentSize) + " bytes";
      return false;
    }

    offset += 8 + segmentSize;  // Skip segment header and data
  }

  actualBootloaderSize = offset;

  // 8. Check for appended SHA256 hash (byte 23 in header)
  // If hash_appended != 0, there's a 32-byte SHA256 hash after the segments
  uint8_t hashAppended = buffer[23];
  if (hashAppended != 0) {
    actualBootloaderSize += 32;
    if (actualBootloaderSize > availableLen) {
      *bootloaderErrorMsg = "Bootloader missing SHA256 trailer";
      return false;
    }
    DEBUG_PRINTF_P(PSTR("Bootloader has appended SHA256 hash\n"));
  }

  // 9. The image may also have a 1-byte checksum after segments/hash
  // Check if there's at least one more byte available
  if (actualBootloaderSize + 1 <= availableLen) {
    // There's likely a checksum byte
    actualBootloaderSize += 1;
  } else if (actualBootloaderSize > availableLen) {
    *bootloaderErrorMsg = "Bootloader truncated before checksum";
    return false;
  }

  // 10. Align to 16 bytes (ESP32 requirement for flash writes)
  // The bootloader image must be 16-byte aligned
  if (actualBootloaderSize % 16 != 0) {
    size_t alignedSize = ((actualBootloaderSize + 15) / 16) * 16;
    // Make sure we don't exceed available data
    if (alignedSize <= len) {
      actualBootloaderSize = alignedSize;
    }
  }

  DEBUG_PRINTF_P(PSTR("Bootloader validation: %d segments, actual size %d bytes (buffer size %d bytes, hash_appended=%d)\n"),
                 segmentCount, actualBootloaderSize, len, hashAppended);

  // 11. Verify we have enough data for all segments + hash + checksum
  if (actualBootloaderSize > availableLen) {
    *bootloaderErrorMsg = "Bootloader truncated - expected at least " + String(actualBootloaderSize) + " bytes, have " + String(availableLen) + " bytes";
    return false;
  }

  if (offset > availableLen) {
    *bootloaderErrorMsg = "Bootloader truncated - expected at least " + String(offset) + " bytes, have " + String(len) + " bytes";
    return false;
  }

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

  DEBUG_PRINTLN(F("Bootloader Update Start - initializing buffer"));
  #if WLED_WATCHDOG_TIMEOUT > 0
  WLED::instance().disableWatchdog();
  #endif
  lastEditTime = millis(); // make sure PIN does not lock during update
  strip.suspend();
  strip.resetSegments();

  // Check available heap before attempting allocation
  size_t freeHeap = getFreeHeapSize();
  DEBUG_PRINTF_P(PSTR("Free heap before bootloader buffer allocation: %d bytes (need %d bytes)\n"), freeHeap, context->maxBootloaderSize);

  context->buffer = (uint8_t*)malloc(context->maxBootloaderSize);
  if (!context->buffer) {
    size_t freeHeapNow = getFreeHeapSize();
    DEBUG_PRINTF_P(PSTR("Failed to allocate %d byte bootloader buffer! Free heap: %d bytes\n"), context->maxBootloaderSize, freeHeapNow);
    context->errorMessage = "Out of memory! Free heap: " + String(freeHeapNow) + " bytes, need: " + String(context->maxBootloaderSize) + " bytes";
    strip.resume();
    #if WLED_WATCHDOG_TIMEOUT > 0
    WLED::instance().enableWatchdog();
    #endif
    return false;
  }

  context->bytesBuffered = 0;
  return true;
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
      if (!verifyBootloaderImage(bootloaderData, bootloaderSize, &context->errorMessage)) {
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
