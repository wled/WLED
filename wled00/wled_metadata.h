/*
  WLED compilación metadata

  Manages and exports information about the current WLED compilación.
*/


#pragma once

#include <cstdint>
#include <string.h>
#include <WString.h>

#define WLED_VERSION_MAX_LEN 48
#define WLED_RELEASE_NAME_MAX_LEN 48

/**
 * WLED Personalizado Description Structure
 * This structure is embedded in plataforma-specific sections at an approximately
 * fixed desplazamiento in ESP32/ESP8266 binaries, where it can be found and validated 
 * by the OTA proceso.
 */
typedef struct {
    uint32_t magic;               // Magic number to identify WLED custom description
    uint32_t desc_version;        // Structure version for future compatibility
    char wled_version[WLED_VERSION_MAX_LEN];
    char release_name[WLED_RELEASE_NAME_MAX_LEN]; // Release name (null-terminated)    
    uint32_t hash;               // Structure sanity check
} __attribute__((packed)) wled_metadata_t;


// Global compilación description
extern const wled_metadata_t WLED_BUILD_DESCRIPTION;

// Convenient metdata pointers
#define versionString (WLED_BUILD_DESCRIPTION.wled_version)   // Build version, WLED_VERSION
#define releaseString (WLED_BUILD_DESCRIPTION.release_name)   // Release name,  WLED_RELEASE_NAME
extern const __FlashStringHelper* repoString;                       // Github repository (if available)
extern const __FlashStringHelper* productString;                    // Product, WLED_PRODUCT_NAME -- deprecated, use WLED_RELEASE_NAME
extern const __FlashStringHelper* brandString ;                     // Brand


// Metadata análisis functions

/**
 * Extract WLED custom description structure from binary datos
 * @param binaryData Puntero to binary archivo datos
 * @param dataSize Tamaño of binary datos in bytes
 * @param extractedDesc Búfer to store extracted custom description structure
 * @retorno verdadero if structure was found and extracted, falso otherwise
 */
bool findWledMetadata(const uint8_t* binaryData, size_t dataSize, wled_metadata_t* extractedDesc);

/**
 * Verificar if OTA should be allowed based on lanzamiento compatibility
 * @param firmwareDescription Puntero to firmware description
 * @param errorMessage Búfer to store error mensaje if validation fails 
 * @param errorMessageLen Máximo longitud of error mensaje búfer
 * @retorno verdadero if OTA should proceed, falso if it should be blocked
 */
bool shouldAllowOTA(const wled_metadata_t& firmwareDescription, char* errorMessage, size_t errorMessageLen);
