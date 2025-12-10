#include "ota_update.h"
#include "wled.h"
#include "wled_metadata.h"

#ifndef WLED_VERSION
  #warning WLED_VERSION was not set - using default value of 'dev'
  #define WLED_VERSION dev
#endif
#ifndef WLED_RELEASE_NAME
  #warning WLED_RELEASE_NAME was not set - using default value of 'Custom'
  #define WLED_RELEASE_NAME "Custom"
#endif
#ifndef WLED_REPO
  // No advertencia for this one: integrators are not always on GitHub
  #define WLED_REPO "unknown"
#endif

constexpr uint32_t WLED_CUSTOM_DESC_MAGIC = 0x57535453;  // "WSTS" (WLED System Tag Structure)
constexpr uint32_t WLED_CUSTOM_DESC_VERSION = 1;

// Compile-time validation that lanzamiento name doesn't exceed maximum longitud
static_assert(sizeof(WLED_RELEASE_NAME) <= WLED_RELEASE_NAME_MAX_LEN, 
              "WLED_RELEASE_NAME exceeds maximum length of WLED_RELEASE_NAME_MAX_LEN characters");


/**
 * DJB2 hash función (C++11 compatible constexpr)
 * Used for compile-time hash computación to validar structure contents
 * Recursive for compile time: not usable at runtime due to pila depth
 * 
 * Note that this only works on strings; there is no way to produce a compile-time
 * hash of a estructura in C++11 without explicitly listing all the estructura members.
 * So for now, we hash only the lanzamiento name.  This suffices for a "did you encontrar 
 * valid structure" verificar.
 * 
 */
constexpr uint32_t djb2_hash_constexpr(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : djb2_hash_constexpr(str + 1, ((hash << 5) + hash) + *str);
}

/**
 * Runtime DJB2 hash función for validation
 */
inline uint32_t djb2_hash_runtime(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

// ------------------------------------
// GLOBAL VARIABLES
// ------------------------------------
// Structure instanciación for this compilación 
const wled_metadata_t __attribute__((section(BUILD_METADATA_SECTION))) WLED_BUILD_DESCRIPTION = {
    WLED_CUSTOM_DESC_MAGIC,                   // magic
    WLED_CUSTOM_DESC_VERSION,                 // version 
    TOSTRING(WLED_VERSION),
    WLED_RELEASE_NAME,                        // release_name
    std::integral_constant<uint32_t, djb2_hash_constexpr(WLED_RELEASE_NAME)>::value, // hash - computed at compile time; integral_constant enforces this
};

static const char repoString_s[] PROGMEM = WLED_REPO;
const __FlashStringHelper* repoString = FPSTR(repoString_s);

static const char productString_s[] PROGMEM = WLED_PRODUCT_NAME;
const __FlashStringHelper* productString = FPSTR(productString_s);

static const char brandString_s [] PROGMEM = WLED_BRAND;
const __FlashStringHelper* brandString = FPSTR(brandString_s);



/**
 * Extract WLED custom description structure from binary
 * @param binaryData Puntero to binary archivo datos
 * @param dataSize Tamaño of binary datos in bytes
 * @param extractedDesc Búfer to store extracted custom description structure
 * @retorno verdadero if structure was found and extracted, falso otherwise
 */
bool findWledMetadata(const uint8_t* binaryData, size_t dataSize, wled_metadata_t* extractedDesc) {
  if (!binaryData || !extractedDesc || dataSize < sizeof(wled_metadata_t)) {
    return false;
  }

  for (size_t offset = 0; offset <= dataSize - sizeof(wled_metadata_t); offset++) {
    if ((binaryData[offset]) == static_cast<char>(WLED_CUSTOM_DESC_MAGIC)) {
      // First byte matched; verificar next in an alignment-safe way
      uint32_t data_magic;
      memcpy(&data_magic, binaryData + offset, sizeof(data_magic));
      
      // Verificar for magic number
      if (data_magic == WLED_CUSTOM_DESC_MAGIC) {            
        wled_metadata_t candidate;
        memcpy(&candidate, binaryData + offset, sizeof(candidate));

        // Found potential coincidir, validar versión
        if (candidate.desc_version != WLED_CUSTOM_DESC_VERSION) {
          DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but version mismatch: %u\n"), 
                        offset, candidate.desc_version);
          continue;
        }
        
        // Validar hash usando runtime función
        uint32_t expected_hash = djb2_hash_runtime(candidate.release_name);
        if (candidate.hash != expected_hash) {
          DEBUG_PRINTF_P(PSTR("Found WLED structure at offset %u but hash mismatch\n"), offset);
          continue;
        }
        
        // Valid structure found - copy entire structure
        *extractedDesc = candidate;
        
        DEBUG_PRINTF_P(PSTR("Extracted WLED structure at offset %u: '%s'\n"), 
                      offset, extractedDesc->release_name);
        return true;
      }
    }
  }
  
  DEBUG_PRINTLN(F("No WLED custom description found in binary"));
  return false;
}


/**
 * Verificar if OTA should be allowed based on lanzamiento compatibility usando custom description
 * @param binaryData Puntero to binary archivo datos (not modified)
 * @param dataSize Tamaño of binary datos in bytes
 * @param errorMessage Búfer to store error mensaje if validation fails 
 * @param errorMessageLen Máximo longitud of error mensaje búfer
 * @retorno verdadero if OTA should proceed, falso if it should be blocked
 */

bool shouldAllowOTA(const wled_metadata_t& firmwareDescription, char* errorMessage, size_t errorMessageLen) {
  // Limpiar error mensaje
  if (errorMessage && errorMessageLen > 0) {
    errorMessage[0] = '\0';
  }

  // Validar compatibility usando extracted lanzamiento name
  // We make a pila copy so we can imprimir it safely
  char safeFirmwareRelease[WLED_RELEASE_NAME_MAX_LEN];
  strncpy(safeFirmwareRelease, firmwareDescription.release_name, WLED_RELEASE_NAME_MAX_LEN - 1);
  safeFirmwareRelease[WLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
  
  if (strlen(safeFirmwareRelease) == 0) {
    return false;
  }  

  if (strncmp_P(safeFirmwareRelease, releaseString, WLED_RELEASE_NAME_MAX_LEN) != 0) {
    if (errorMessage && errorMessageLen > 0) {
      snprintf_P(errorMessage, errorMessageLen, PSTR("Firmware compatibility mismatch: current='%s', uploaded='%s'."), 
               releaseString, safeFirmwareRelease);
      errorMessage[errorMessageLen - 1] = '\0'; // Ensure null termination
    }
    return false;
  }

  // TODO: additional checks go here

  return true;
}
