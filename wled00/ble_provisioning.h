#ifndef WLED_BLE_PROVISIONING_H
#define WLED_BLE_PROVISIONING_H

/*
 * BLE WiFi Provisioning for WLED
 *
 * Enables WiFi credential provisioning over Bluetooth Low Energy (BLE).
 * This provides an alternative to the captive portal for iOS users,
 * avoiding the iOS captive portal popup issue.
 *
 * Supported chips: ESP32, ESP32-S3, ESP32-C3, ESP32-C6
 * NOT supported: ESP8266, ESP32-S2 (no BLE hardware)
 *
 * Memory management:
 * - BLE stack uses ~110KB RAM during provisioning
 * - Memory is released after provisioning via esp_bt_mem_release()
 * - On subsequent boots, BLE memory is released at startup if already provisioned
 */

#include "wled.h"

// Check for BLE-capable chips
// ESP32, ESP32-S3, ESP32-C3, ESP32-C6 have BLE
// ESP8266 and ESP32-S2 do NOT have BLE
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2)
  // Use built-in ESP32 BLE library (Bluedroid) unless disabled
  #if __has_include(<BLEDevice.h>) && !defined(WLED_DISABLE_BLE_PROVISIONING)
    #define WLED_ENABLE_BLE_PROVISIONING
  #endif
#endif

#ifdef WLED_ENABLE_BLE_PROVISIONING

// BLE Provisioning Service UUIDs
// Using ESP-IDF provisioning UUIDs for compatibility with Espressif apps
#define BLE_PROV_SERVICE_UUID        "021a9004-0382-4aea-bff4-6b3f1c5adfb4"
#define BLE_PROV_CHAR_WIFI_SCAN_UUID "021aff50-0382-4aea-bff4-6b3f1c5adfb4"
#define BLE_PROV_CHAR_WIFI_CFG_UUID  "021aff51-0382-4aea-bff4-6b3f1c5adfb4"
#define BLE_PROV_CHAR_WIFI_STATUS_UUID "021aff52-0382-4aea-bff4-6b3f1c5adfb4"

// Custom WLED-specific UUIDs for direct integration
#define WLED_BLE_SERVICE_UUID        "WLED0001-0000-1000-8000-00805F9B34FB"
#define WLED_BLE_CHAR_SSID_UUID      "WLED0002-0000-1000-8000-00805F9B34FB"
#define WLED_BLE_CHAR_PASS_UUID      "WLED0003-0000-1000-8000-00805F9B34FB"
#define WLED_BLE_CHAR_STATUS_UUID    "WLED0004-0000-1000-8000-00805F9B34FB"
#define WLED_BLE_CHAR_SCAN_UUID      "WLED0005-0000-1000-8000-00805F9B34FB"
#define WLED_BLE_CHAR_DEVICE_UUID    "WLED0006-0000-1000-8000-00805F9B34FB"

// Provisioning status codes
enum BLEProvStatus {
  BLE_PROV_STATUS_IDLE = 0,
  BLE_PROV_STATUS_SCANNING = 1,
  BLE_PROV_STATUS_CONNECTING = 2,
  BLE_PROV_STATUS_CONNECTED = 3,
  BLE_PROV_STATUS_FAILED = 4,
  BLE_PROV_STATUS_SUCCESS = 5
};

// BLE Provisioning state
struct BLEProvState {
  bool active;                    // Is BLE provisioning currently running
  bool provisioningComplete;      // Has provisioning completed successfully
  BLEProvStatus status;           // Current provisioning status
  char ssid[33];                  // SSID to connect to (max 32 chars + null)
  char password[65];              // WiFi password (max 64 chars + null)
  char deviceName[33];            // Device name to set (optional)
  uint8_t scanCount;              // Number of networks found in scan
  unsigned long lastActivity;     // Timestamp of last BLE activity
};

// Global BLE provisioning state
extern BLEProvState bleProvState;

/**
 * Initialize BLE provisioning
 * Should be called during setup() if WiFi is not configured
 *
 * @param deviceName The BLE device name to advertise (e.g., "WLED-XXXX")
 * @return true if BLE initialized successfully, false otherwise
 */
bool initBLEProvisioning(const char* deviceName);

/**
 * Handle BLE provisioning in the main loop
 * Processes BLE events and handles WiFi connection attempts
 *
 * @return true if provisioning is still active, false if complete or stopped
 */
bool handleBLEProvisioning();

/**
 * Stop BLE provisioning and release memory
 * Releases ~70KB of heap memory back to the system
 * WARNING: BLE cannot be restarted without a reboot after this!
 */
void stopBLEProvisioning();

/**
 * Check if BLE provisioning is active
 * @return true if BLE is currently running
 */
bool isBLEProvisioningActive();

/**
 * Release BLE memory at startup if not needed
 * Should be called early in setup() if WiFi is already configured
 * Frees ~30-70KB of memory reserved for BLE
 */
void releaseBLEMemoryIfNotNeeded();

/**
 * Get WiFi scan results as JSON for BLE transmission
 * @param buffer Output buffer for JSON
 * @param maxLen Maximum buffer length
 * @return Number of bytes written
 */
size_t getWiFiScanResultsJSON(char* buffer, size_t maxLen);

#else // WLED_ENABLE_BLE_PROVISIONING not defined

// Stub functions for non-BLE builds
inline bool initBLEProvisioning(const char* deviceName) { return false; }
inline bool handleBLEProvisioning() { return false; }
inline void stopBLEProvisioning() {}
inline bool isBLEProvisioningActive() { return false; }
inline void releaseBLEMemoryIfNotNeeded() {}

#endif // WLED_ENABLE_BLE_PROVISIONING

#endif // WLED_BLE_PROVISIONING_H
