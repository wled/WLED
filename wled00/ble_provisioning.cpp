#include "ble_provisioning.h"

#ifdef WLED_ENABLE_BLE_PROVISIONING

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Global state
BLEProvState bleProvState = {
  .active = false,
  .provisioningComplete = false,
  .status = BLE_PROV_STATUS_IDLE,
  .ssid = {0},
  .password = {0},
  .deviceName = {0},
  .scanCount = 0,
  .lastActivity = 0
};

// BLE objects
static BLEServer* pServer = nullptr;
static BLEService* pService = nullptr;
static BLECharacteristic* pCharSSID = nullptr;
static BLECharacteristic* pCharPass = nullptr;
static BLECharacteristic* pCharStatus = nullptr;
static BLECharacteristic* pCharScan = nullptr;
static BLECharacteristic* pCharDevice = nullptr;

static bool deviceConnected = false;
static bool shouldTryConnect = false;
static unsigned long connectionAttemptTime = 0;

// Timeout for BLE provisioning (5 minutes)
#define BLE_PROV_TIMEOUT 300000

// Helper to update status characteristic
static void updateStatusCharacteristic() {
  if (pCharStatus) {
    char status[8];
    snprintf(status, sizeof(status), "%d", bleProvState.status);
    pCharStatus->setValue(status);
    pCharStatus->notify();
  }
}

// Helper to trigger WiFi scan
static void triggerWiFiScan() {
  DEBUG_PRINTLN(F("BLE: Starting WiFi scan"));
  bleProvState.status = BLE_PROV_STATUS_SCANNING;

  // Update status
  if (pCharStatus) {
    pCharStatus->setValue("1"); // Scanning
    pCharStatus->notify();
  }

  // Perform WiFi scan
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks(false, false, false, 300);
  bleProvState.scanCount = (n > 0) ? n : 0;

  DEBUG_PRINTF_P(PSTR("BLE: Found %d networks\n"), bleProvState.scanCount);

  // Build JSON response
  char scanResults[1024];
  size_t len = getWiFiScanResultsJSON(scanResults, sizeof(scanResults));

  // Update scan characteristic with results
  if (pCharScan && len > 0) {
    pCharScan->setValue((uint8_t*)scanResults, len);
    pCharScan->notify();
  }

  bleProvState.status = BLE_PROV_STATUS_IDLE;
  if (pCharStatus) {
    pCharStatus->setValue("0"); // Idle
    pCharStatus->notify();
  }
}

// Callbacks for BLE server events
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    bleProvState.lastActivity = millis();
    DEBUG_PRINTLN(F("BLE: Client connected"));
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    DEBUG_PRINTLN(F("BLE: Client disconnected"));

    // Resume advertising if provisioning not complete
    if (!bleProvState.provisioningComplete && bleProvState.active) {
      pServer->startAdvertising();
    }
  }
};

// Callbacks for SSID characteristic
class SSIDCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    bleProvState.lastActivity = millis();
    std::string value = pCharacteristic->getValue();

    size_t len = min(value.length(), sizeof(bleProvState.ssid) - 1);
    memcpy(bleProvState.ssid, value.c_str(), len);
    bleProvState.ssid[len] = '\0';
    DEBUG_PRINTF_P(PSTR("BLE: SSID set to: %s\n"), bleProvState.ssid);
  }
};

// Callbacks for Password characteristic
class PassCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    bleProvState.lastActivity = millis();
    std::string value = pCharacteristic->getValue();

    size_t len = min(value.length(), sizeof(bleProvState.password) - 1);
    memcpy(bleProvState.password, value.c_str(), len);
    bleProvState.password[len] = '\0';
    DEBUG_PRINTLN(F("BLE: Password received"));

    // If SSID and password are set, try to connect
    if (strlen(bleProvState.ssid) > 0) {
      shouldTryConnect = true;
      bleProvState.status = BLE_PROV_STATUS_CONNECTING;
      updateStatusCharacteristic();
    }
  }
};

// Callbacks for Device Name characteristic
class DeviceCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    bleProvState.lastActivity = millis();
    std::string value = pCharacteristic->getValue();

    size_t len = min(value.length(), sizeof(bleProvState.deviceName) - 1);
    memcpy(bleProvState.deviceName, value.c_str(), len);
    bleProvState.deviceName[len] = '\0';
    DEBUG_PRINTF_P(PSTR("BLE: Device name set to: %s\n"), bleProvState.deviceName);
  }
};

// Callbacks for Scan characteristic
class ScanCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    bleProvState.lastActivity = millis();
    triggerWiFiScan();
  }
};

static ServerCallbacks serverCallbacks;
static SSIDCallback ssidCallback;
static PassCallback passCallback;
static DeviceCallback deviceCallback;
static ScanCallback scanCallback;

bool initBLEProvisioning(const char* deviceName) {
  if (bleProvState.active) {
    DEBUG_PRINTLN(F("BLE: Already active"));
    return true;
  }

  DEBUG_PRINTLN(F("BLE: Initializing provisioning"));
  DEBUG_PRINTF_P(PSTR("BLE: Device name: %s\n"), deviceName);
  DEBUG_PRINTF_P(PSTR("BLE: Free heap before init: %u\n"), ESP.getFreeHeap());

  // Initialize BLE
  BLEDevice::init(deviceName);

  // Create server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  // Create WLED provisioning service
  pService = pServer->createService(WLED_BLE_SERVICE_UUID);

  // Create characteristics
  // SSID - Write only
  pCharSSID = pService->createCharacteristic(
    WLED_BLE_CHAR_SSID_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharSSID->setCallbacks(&ssidCallback);

  // Password - Write only
  pCharPass = pService->createCharacteristic(
    WLED_BLE_CHAR_PASS_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharPass->setCallbacks(&passCallback);

  // Status - Read and Notify
  pCharStatus = pService->createCharacteristic(
    WLED_BLE_CHAR_STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharStatus->addDescriptor(new BLE2902());
  pCharStatus->setValue("0"); // Initial: Idle

  // WiFi Scan - Write to trigger, Read/Notify for results
  pCharScan = pService->createCharacteristic(
    WLED_BLE_CHAR_SCAN_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharScan->addDescriptor(new BLE2902());
  pCharScan->setCallbacks(&scanCallback);

  // Device Name - Write only (optional)
  pCharDevice = pService->createCharacteristic(
    WLED_BLE_CHAR_DEVICE_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharDevice->setCallbacks(&deviceCallback);

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(WLED_BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // iOS connection parameter hint
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  bleProvState.active = true;
  bleProvState.status = BLE_PROV_STATUS_IDLE;
  bleProvState.lastActivity = millis();

  DEBUG_PRINTF_P(PSTR("BLE: Initialized. Free heap: %u\n"), ESP.getFreeHeap());
  return true;
}

bool handleBLEProvisioning() {
  if (!bleProvState.active) {
    return false;
  }

  // Check for timeout
  if (millis() - bleProvState.lastActivity > BLE_PROV_TIMEOUT) {
    DEBUG_PRINTLN(F("BLE: Provisioning timeout"));
    stopBLEProvisioning();
    return false;
  }

  // Handle WiFi connection attempt
  if (shouldTryConnect) {
    shouldTryConnect = false;
    connectionAttemptTime = millis();

    DEBUG_PRINTF_P(PSTR("BLE: Attempting to connect to: %s\n"), bleProvState.ssid);

    // Update status
    bleProvState.status = BLE_PROV_STATUS_CONNECTING;
    if (pCharStatus) {
      pCharStatus->setValue("2"); // Connecting
      pCharStatus->notify();
    }

    // Store credentials in WLED's config
    strlcpy(multiWiFi[0].clientSSID, bleProvState.ssid, sizeof(multiWiFi[0].clientSSID));
    strlcpy(multiWiFi[0].clientPass, bleProvState.password, sizeof(multiWiFi[0].clientPass));

    // Set device name if provided
    if (strlen(bleProvState.deviceName) > 0) {
      strlcpy(serverDescription, bleProvState.deviceName, sizeof(serverDescription));
    }

    // Trigger WiFi connection
    forceReconnect = true;
  }

  // Check WiFi connection status after attempt
  if (bleProvState.status == BLE_PROV_STATUS_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      DEBUG_PRINTLN(F("BLE: WiFi connected successfully!"));
      bleProvState.status = BLE_PROV_STATUS_SUCCESS;
      bleProvState.provisioningComplete = true;

      // Notify client
      if (pCharStatus) {
        // Send IP address as success message
        char ipStr[32];
        snprintf(ipStr, sizeof(ipStr), "5:%s", WiFi.localIP().toString().c_str());
        pCharStatus->setValue(ipStr);
        pCharStatus->notify();
      }

      // Save configuration
      serializeConfigToFS();

      // Give client time to receive notification
      delay(1000);

      // Stop BLE provisioning
      stopBLEProvisioning();
      return false;
    }
    else if (millis() - connectionAttemptTime > 30000) {
      // Connection timeout (30 seconds)
      DEBUG_PRINTLN(F("BLE: WiFi connection failed"));
      bleProvState.status = BLE_PROV_STATUS_FAILED;

      if (pCharStatus) {
        pCharStatus->setValue("4"); // Failed
        pCharStatus->notify();
      }

      // Reset for another attempt
      bleProvState.status = BLE_PROV_STATUS_IDLE;
    }
  }

  return true;
}

void stopBLEProvisioning() {
  if (!bleProvState.active) {
    return;
  }

  DEBUG_PRINTLN(F("BLE: Stopping provisioning"));
  DEBUG_PRINTF_P(PSTR("BLE: Free heap before stop: %u\n"), ESP.getFreeHeap());

  // Deinitialize BLE
  BLEDevice::deinit(true);

  bleProvState.active = false;
  pServer = nullptr;
  pService = nullptr;
  pCharSSID = nullptr;
  pCharPass = nullptr;
  pCharStatus = nullptr;
  pCharScan = nullptr;
  pCharDevice = nullptr;

  DEBUG_PRINTF_P(PSTR("BLE: Stopped. Free heap: %u\n"), ESP.getFreeHeap());
}

bool isBLEProvisioningActive() {
  return bleProvState.active;
}

void releaseBLEMemoryIfNotNeeded() {
  // The ESP32 Arduino BLE library doesn't provide a way to release
  // BLE memory before initialization like NimBLE does.
  // This function is kept as a stub for API compatibility.
}

size_t getWiFiScanResultsJSON(char* buffer, size_t maxLen) {
  if (!buffer || maxLen < 32) return 0;

  int n = WiFi.scanComplete();
  if (n < 0) {
    // Scan not complete or failed
    return snprintf(buffer, maxLen, "{\"networks\":[]}");
  }

  size_t pos = 0;
  pos += snprintf(buffer + pos, maxLen - pos, "{\"networks\":[");

  for (int i = 0; i < n && pos < maxLen - 50; i++) {
    if (i > 0) {
      pos += snprintf(buffer + pos, maxLen - pos, ",");
    }
    pos += snprintf(buffer + pos, maxLen - pos,
      "{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true"
    );
  }

  pos += snprintf(buffer + pos, maxLen - pos, "]}");

  WiFi.scanDelete(); // Clean up scan results

  return pos;
}

#endif // WLED_ENABLE_BLE_PROVISIONING
