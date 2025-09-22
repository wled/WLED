#include "ota_update.h"
#include "wled.h"

#ifdef ESP32
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#endif


#define WLED_CUSTOM_DESC_MAGIC 0x57535453  // "WSTS" (WLED System Tag Structure)
#define WLED_CUSTOM_DESC_VERSION 1
#define WLED_RELEASE_NAME_MAX_LEN 48

// Compile-time validation that release name doesn't exceed maximum length
static_assert(sizeof(WLED_RELEASE_NAME) <= WLED_RELEASE_NAME_MAX_LEN, 
              "WLED_RELEASE_NAME exceeds maximum length of WLED_RELEASE_NAME_MAX_LEN characters");

// Platform-specific metadata locations
#ifdef ESP32
#define WLED_CUSTOM_DESC_SECTION ".rodata.wled_desc"
constexpr size_t METADATA_OFFSET = 0;          // ESP32: metadata appears at beginning
#define UPDATE_ERROR errorString
#elif defined(ESP8266)
#define WLED_CUSTOM_DESC_SECTION ".ver_number"
constexpr size_t METADATA_OFFSET = 0x1000;     // ESP8266: metadata appears at 4KB offset
#define UPDATE_ERROR getErrorString
#endif
constexpr size_t METADATA_SEARCH_RANGE = 512;  // bytes


/**
 * DJB2 hash function (C++11 compatible constexpr)
 * Used for compile-time hash computation to validate structure contents
 * Recursive for compile time: not usable at runtime due to stack depth
 * 
 * Note that this only works on strings; there is no way to produce a compile-time
 * hash of a struct in C++11 without explicitly listing all the struct members.
 * So for now, we hash only the release name.  This suffices for a "did you find 
 * valid structure" check.
 * 
 */
constexpr uint32_t djb2_hash_constexpr(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : djb2_hash_constexpr(str + 1, ((hash << 5) + hash) + *str);
}

/**
 * Runtime DJB2 hash function for validation
 */
inline uint32_t djb2_hash_runtime(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}


/**
 * WLED Custom Description Structure
 * This structure is embedded in platform-specific sections at a fixed offset
 * in ESP32/ESP8266 binaries, allowing extraction without modifying the binary format
 */
typedef struct {
    uint32_t magic;               // Magic number to identify WLED custom description
    uint32_t version;             // Structure version for future compatibility
    char release_name[WLED_RELEASE_NAME_MAX_LEN]; // Release name (null-terminated)
    uint32_t hash;              // Hash of ONLY release name, for struct validation.  
} __attribute__((packed)) wled_custom_desc_t;


// Structure instantiation for this build 
const wled_custom_desc_t __attribute__((section(WLED_CUSTOM_DESC_SECTION))) wled_custom_description = {
    WLED_CUSTOM_DESC_MAGIC,                   // magic
    WLED_CUSTOM_DESC_VERSION,                 // version  
    WLED_RELEASE_NAME,                        // release_name
    std::integral_constant<uint32_t, djb2_hash_constexpr(WLED_RELEASE_NAME)>::value, // hash - computed at compile time; integral_constant enforces this
};


//extern int __attribute__ ((alias ("wled_custom_description.version"))) ver_alias;

/**
 * Extract WLED custom description structure from binary
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedDesc Buffer to store extracted custom description structure
 * @return true if structure was found and extracted, false otherwise
 */
static bool extractWledCustomDesc(const uint8_t* binaryData, size_t dataSize, wled_custom_desc_t* extractedDesc) {
    if (!binaryData || !extractedDesc || dataSize < sizeof(wled_custom_desc_t)) {
        return false;
    }

    for (size_t offset = 0; offset <= dataSize - sizeof(wled_custom_desc_t); offset++) {
        const wled_custom_desc_t* custom_desc = (const wled_custom_desc_t*)(binaryData + offset);
        
        // Check for magic number
        if (custom_desc->magic == WLED_CUSTOM_DESC_MAGIC) {
            // Found potential match, validate version
            if (custom_desc->version != WLED_CUSTOM_DESC_VERSION) {
                DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but version mismatch: %u\n"), 
                              offset, custom_desc->version);
                continue;
            }
            
            // Validate hash using runtime function
            uint32_t expected_hash = djb2_hash_runtime(custom_desc->release_name);
            if (custom_desc->hash != expected_hash) {
                DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but hash mismatch\n"), offset);
                continue;
            }
            
            // Valid structure found - copy entire structure
            memcpy(extractedDesc, custom_desc, sizeof(wled_custom_desc_t));
            
            DEBUG_PRINTF_P(PSTR("Extracted WLED structure at offset %u: '%s'\n"), 
                          offset, extractedDesc->release_name);
            return true;
        }
    }
    
    DEBUG_PRINTLN(F("No WLED custom description found in binary"));
    return false;
}


/**
 * Validate if extracted release name matches current release
 * @param extractedRelease Release name from uploaded binary
 * @return true if releases match (OTA should proceed), false if they don't match
 */
static bool validateReleaseCompatibility(const char* extractedRelease) {
  if (!extractedRelease) {
    return false;
  }
  
  // Ensure extractedRelease is properly null terminated (guard against fixed-length buffer issues)
  char safeRelease[WLED_RELEASE_NAME_MAX_LEN];
  strncpy(safeRelease, extractedRelease, WLED_RELEASE_NAME_MAX_LEN - 1);
  safeRelease[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
  
  if (strlen(safeRelease) == 0) {
    return false;
  }
  
  // Simple string comparison - releases must match exactly
  bool match = strcmp(wled_custom_description.release_name, safeRelease) == 0;
  
  DEBUG_PRINTF_P(PSTR("Release compatibility check: current='%s', uploaded='%s', match=%s\n"), 
                 wled_custom_description.release_name, safeRelease, match ? "YES" : "NO");
  
  return match;
}

/**
 * Check if OTA should be allowed based on release compatibility using custom description
 * @param binaryData Pointer to binary file data (not modified)
 * @param dataSize Size of binary data in bytes
 * @param errorMessage Buffer to store error message if validation fails 
 * @param errorMessageLen Maximum length of error message buffer
 * @return true if OTA should proceed, false if it should be blocked
 */

static bool shouldAllowOTA(const uint8_t* binaryData, size_t dataSize, char* errorMessage, size_t errorMessageLen) {
  // Clear error message
  if (errorMessage && errorMessageLen > 0) {
    errorMessage[0] = '\0';
  }

  // Try to extract WLED structure directly from binary data
  wled_custom_desc_t extractedDesc;
  bool hasCustomDesc = extractWledCustomDesc(binaryData, dataSize, &extractedDesc);

  if (!hasCustomDesc) {
    // No custom description - this could be a legacy binary
    if (errorMessage && errorMessageLen > 0) {
      strncpy_P(errorMessage, PSTR("This firmware file is missing compatibility metadata."), errorMessageLen - 1);
      errorMessage[errorMessageLen - 1] = '\0';
    }
    return false;
  }

  // Validate compatibility using extracted release name
  if (!validateReleaseCompatibility(extractedDesc.release_name)) {
    if (errorMessage && errorMessageLen > 0) {
      snprintf_P(errorMessage, errorMessageLen, PSTR("Firmware compatibility mismatch: current='%s', uploaded='%s'."), 
               releaseString, extractedDesc.release_name);
      errorMessage[errorMessageLen - 1] = '\0'; // Ensure null termination
    }
    return false;
  }

  return true;
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

  if (request->hasParam("skipValidation", true)) {
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

  DEBUG_PRINTF_P(PSTR("HandleOTAData: %d %d %d\n"), index, len, isFinal);

  if (context->replySent || (context->errorMessage.length())) return;

  if (index == 0) {
    if (!beginOTA(request, context)) return;
  }

  // Perform validation if we haven't done it yet and we have reached the metadata offset
  if (!context->releaseCheckPassed && (index+len) > METADATA_OFFSET) {
    // Current chunk contains the metadata offset
    size_t availableDataAfterOffset = (index + len) - METADATA_OFFSET;

    DEBUG_PRINTF_P(PSTR("MetadataCheck: %d in buffer\n"), context->releaseMetadataBuffer.size());

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
      bool OTA_ok = shouldAllowOTA(search_data, search_len, errorMessage, sizeof(errorMessage));
      
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
      DEBUG_PRINTF_P(PSTR("OTA write failed on chunk %zu: %s\n"), index, Update.getErrorString().c_str());
    }
  }

  if(isFinal) {
    DEBUG_PRINTLN(F("OTA Update End"));
    // Upload complete
    context->uploadComplete = true;
  }
}