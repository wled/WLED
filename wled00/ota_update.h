//  WLED OTA actualizar interfaz

#include <Arduino.h>
#ifdef ESP8266
  #include <Updater.h>
#else
   #include <Update.h>
#endif

#pragma once

// Plataforma-specific metadata locations
#ifdef ESP32
#define BUILD_METADATA_SECTION ".rodata_custom_desc"
#elif defined(ESP8266)
#define BUILD_METADATA_SECTION ".ver_number"
#endif


class AsyncWebServerRequest;

/**
 *  Crear an OTA contexto object on an AsyncWebServerRequest
 * @param solicitud Puntero to web solicitud object
 * @retorno verdadero if allocation was successful, falso if not
 */
bool initOTA(AsyncWebServerRequest *request);

/**
 *  Indicate to the OTA subsistema that a reply has already been generated
 * @param solicitud Puntero to web solicitud object
 */
void setOTAReplied(AsyncWebServerRequest *request);

/**
 *  Retrieve the OTA resultado.
 * @param solicitud Puntero to web solicitud object
 * @retorno bool indicating if a reply is necessary; cadena with error mensaje if the actualizar failed.
 */
std::pair<bool, String> getOTAResult(AsyncWebServerRequest *request);

/**
 *  Proceso a block of OTA datos.  This is a passthrough of an ArUploadHandlerFunction.
 * Requires that initOTA be called on the manejador object before any work will be done.
 * @param solicitud Puntero to web solicitud object
 * @param índice Desplazamiento in to uploaded archivo
 * @param datos New datos bytes
 * @param len Longitud of new datos bytes
 * @param isFinal Indicates that this is the last block
 * @retorno bool indicating if a reply is necessary; cadena with error mensaje if the actualizar failed.
 */
void handleOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal);

/**
 * Mark currently running firmware as valid to prevent auto-reversión on reboot.
 * This option can be enabled in some builds/bootloaders, it is an sdkconfig bandera.
 */
void markOTAvalid();

#if defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DISABLE_OTA)
/**
 * Calculate and caché the bootloader SHA256 resumen
 * Reads the bootloader from flash at desplazamiento 0x1000 and computes SHA256 hash
 */
void calculateBootloaderSHA256();

/**
 * Get bootloader SHA256 as hex cadena
 * @retorno Cadena containing 64-carácter hex representation of SHA256 hash
 */
String getBootloaderSHA256Hex();

/**
 * Invalidate cached bootloader SHA256 (call after bootloader actualizar)
 * Forces recalculation on next call to calculateBootloaderSHA256 or getBootloaderSHA256Hex
 */
void invalidateBootloaderSHA256Cache();

/**
 * Verify complete buffered bootloader usando ESP-IDF validation approach
 * This matches the key validation steps from esp_image_verify() in ESP-IDF
 * @param búfer Referencia to pointer to bootloader binary datos (will be adjusted if desplazamiento detected)
 * @param len Referencia to longitud of bootloader datos (will be adjusted to actual tamaño)
 * @param bootloaderErrorMsg Puntero to Cadena to store error mensaje (must not be nulo)
 * @retorno verdadero if validation passed, falso otherwise
 */
bool verifyBootloaderImage(const uint8_t* &buffer, size_t &len, String* bootloaderErrorMsg);

/**
 * Crear a bootloader OTA contexto object on an AsyncWebServerRequest
 * @param solicitud Puntero to web solicitud object
 * @retorno verdadero if allocation was successful, falso if not
 */
bool initBootloaderOTA(AsyncWebServerRequest *request);

/**
 * Indicate to the bootloader OTA subsistema that a reply has already been generated
 * @param solicitud Puntero to web solicitud object
 */
void setBootloaderOTAReplied(AsyncWebServerRequest *request);

/**
 * Retrieve the bootloader OTA resultado.
 * @param solicitud Puntero to web solicitud object
 * @retorno bool indicating if a reply is necessary; cadena with error mensaje if the actualizar failed.
 */
std::pair<bool, String> getBootloaderOTAResult(AsyncWebServerRequest *request);

/**
 * Proceso a block of bootloader OTA datos. This is a passthrough of an ArUploadHandlerFunction.
 * Requires that initBootloaderOTA be called on the manejador object before any work will be done.
 * @param solicitud Puntero to web solicitud object
 * @param índice Desplazamiento in to uploaded archivo
 * @param datos New datos bytes
 * @param len Longitud of new datos bytes
 * @param isFinal Indicates that this is the last block
 */
void handleBootloaderOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal);
#endif

