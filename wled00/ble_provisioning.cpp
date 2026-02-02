#include "ble_provisioning.h"

#ifdef WLED_ENABLE_BLE_PROVISIONING

#include <NimBLEDevice.h>

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
static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* pCharSSID = nullptr;
static NimBLECharacteristic* pCharPass = nullptr;
static NimBLECharacteristic* pCharStatus = nullptr;
static NimBLECharacteristic* pCharScan = nullptr;
static NimBLECharacteristic* pCharDevice = nullptr;

static bool deviceConnected = false;
static bool shouldTryConnect = false;
static unsigned long connectionAttemptTime = 0;

#define BLE_PROV_TIMEOUT 300000

// Forward declarations
static void updateStatusCharacteristic();
static void triggerWiFiScan();

// Server callbacks
class ProvServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    deviceConnected = true;
    bleProvState.lastActivity = millis();
    DEBUG_PRINTLN(F("BLE: Client connected"));
  }

  void onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    DEBUG_PRINTLN(F("BLE: Client disconnected"));
    if (!bleProvState.provisioningComplete && bleProvState.active) {
      NimBLEDevice::startAdvertising();
    }
  }
};

// Characteristic callbacks
class ProvCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) {
    bleProvState.lastActivity = millis();
    NimBLEUUID uuid = pChar->getUUID();
    std::string value = pChar->getValue();

    if (uuid.equals(NimBLEUUID(WLED_BLE_CHAR_SSID_UUID))) {
      size_t len = min(value.length(), sizeof(bleProvState.ssid) - 1);
      memcpy(bleProvState.ssid, value.c_str(), len);
      bleProvState.ssid[len] = '\0';
      DEBUG_PRINTF_P(PSTR("BLE: SSID: %s\n"), bleProvState.ssid);
    }
    else if (uuid.equals(NimBLEUUID(WLED_BLE_CHAR_PASS_UUID))) {
      size_t len = min(value.length(), sizeof(bleProvState.password) - 1);
      memcpy(bleProvState.password, value.c_str(), len);
      bleProvState.password[len] = '\0';
      DEBUG_PRINTLN(F("BLE: Password set"));
      if (strlen(bleProvState.ssid) > 0) {
        shouldTryConnect = true;
        bleProvState.status = BLE_PROV_STATUS_CONNECTING;
        updateStatusCharacteristic();
      }
    }
    else if (uuid.equals(NimBLEUUID(WLED_BLE_CHAR_DEVICE_UUID))) {
      size_t len = min(value.length(), sizeof(bleProvState.deviceName) - 1);
      memcpy(bleProvState.deviceName, value.c_str(), len);
      bleProvState.deviceName[len] = '\0';
      DEBUG_PRINTF_P(PSTR("BLE: Name: %s\n"), bleProvState.deviceName);
    }
    else if (uuid.equals(NimBLEUUID(WLED_BLE_CHAR_SCAN_UUID))) {
      triggerWiFiScan();
    }
  }
};

static ProvServerCallbacks serverCB;
static ProvCharCallbacks charCB;

static void updateStatusCharacteristic() {
  if (pCharStatus) {
    char status[8];
    snprintf(status, sizeof(status), "%d", bleProvState.status);
    pCharStatus->setValue((uint8_t*)status, strlen(status));
    pCharStatus->notify();
  }
}

static void triggerWiFiScan() {
  DEBUG_PRINTLN(F("BLE: WiFi scan"));
  bleProvState.status = BLE_PROV_STATUS_SCANNING;
  updateStatusCharacteristic();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks(false, false, false, 300);
  bleProvState.scanCount = (n > 0) ? n : 0;

  char buf[512];
  size_t len = getWiFiScanResultsJSON(buf, sizeof(buf));
  if (pCharScan && len > 0) {
    pCharScan->setValue((uint8_t*)buf, len);
    pCharScan->notify();
  }

  bleProvState.status = BLE_PROV_STATUS_IDLE;
  updateStatusCharacteristic();
}

bool initBLEProvisioning(const char* deviceName) {
  if (bleProvState.active) return true;

  DEBUG_PRINTF_P(PSTR("BLE: Init %s, heap=%u\n"), deviceName, ESP.getFreeHeap());

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(false, false, false);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCB);

  pService = pServer->createService(WLED_BLE_SERVICE_UUID);

  pCharSSID = pService->createCharacteristic(WLED_BLE_CHAR_SSID_UUID, NIMBLE_PROPERTY::WRITE);
  pCharSSID->setCallbacks(&charCB);

  pCharPass = pService->createCharacteristic(WLED_BLE_CHAR_PASS_UUID, NIMBLE_PROPERTY::WRITE);
  pCharPass->setCallbacks(&charCB);

  pCharStatus = pService->createCharacteristic(WLED_BLE_CHAR_STATUS_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pCharStatus->setValue("0");

  pCharScan = pService->createCharacteristic(WLED_BLE_CHAR_SCAN_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pCharScan->setCallbacks(&charCB);

  pCharDevice = pService->createCharacteristic(WLED_BLE_CHAR_DEVICE_UUID, NIMBLE_PROPERTY::WRITE);
  pCharDevice->setCallbacks(&charCB);

  pService->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(WLED_BLE_SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->start();

  bleProvState.active = true;
  bleProvState.lastActivity = millis();

  DEBUG_PRINTF_P(PSTR("BLE: Ready, heap=%u\n"), ESP.getFreeHeap());
  return true;
}

bool handleBLEProvisioning() {
  if (!bleProvState.active) return false;

  if (millis() - bleProvState.lastActivity > BLE_PROV_TIMEOUT) {
    DEBUG_PRINTLN(F("BLE: Timeout"));
    stopBLEProvisioning();
    return false;
  }

  if (shouldTryConnect) {
    shouldTryConnect = false;
    connectionAttemptTime = millis();
    DEBUG_PRINTF_P(PSTR("BLE: Connect to %s\n"), bleProvState.ssid);

    strlcpy(multiWiFi[0].clientSSID, bleProvState.ssid, sizeof(multiWiFi[0].clientSSID));
    strlcpy(multiWiFi[0].clientPass, bleProvState.password, sizeof(multiWiFi[0].clientPass));
    if (strlen(bleProvState.deviceName) > 0) {
      strlcpy(serverDescription, bleProvState.deviceName, sizeof(serverDescription));
    }
    forceReconnect = true;
  }

  if (bleProvState.status == BLE_PROV_STATUS_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      DEBUG_PRINTLN(F("BLE: WiFi OK!"));
      bleProvState.status = BLE_PROV_STATUS_SUCCESS;
      bleProvState.provisioningComplete = true;

      if (pCharStatus) {
        char ip[32];
        snprintf(ip, sizeof(ip), "5:%s", WiFi.localIP().toString().c_str());
        pCharStatus->setValue((uint8_t*)ip, strlen(ip));
        pCharStatus->notify();
      }

      serializeConfigToFS();
      delay(500);
      stopBLEProvisioning();
      return false;
    }
    else if (millis() - connectionAttemptTime > 30000) {
      DEBUG_PRINTLN(F("BLE: WiFi fail"));
      bleProvState.status = BLE_PROV_STATUS_FAILED;
      updateStatusCharacteristic();
      bleProvState.status = BLE_PROV_STATUS_IDLE;
    }
  }
  return true;
}

void stopBLEProvisioning() {
  if (!bleProvState.active) return;
  DEBUG_PRINTF_P(PSTR("BLE: Stop, heap=%u\n"), ESP.getFreeHeap());

  NimBLEDevice::deinit(true);

  bleProvState.active = false;
  pServer = nullptr;
  pService = nullptr;
  pCharSSID = nullptr;
  pCharPass = nullptr;
  pCharStatus = nullptr;
  pCharScan = nullptr;
  pCharDevice = nullptr;

  DEBUG_PRINTF_P(PSTR("BLE: Done, heap=%u\n"), ESP.getFreeHeap());
}

bool isBLEProvisioningActive() {
  return bleProvState.active;
}

void releaseBLEMemoryIfNotNeeded() {
  // NimBLE handles memory efficiently, no pre-release needed
}

size_t getWiFiScanResultsJSON(char* buffer, size_t maxLen) {
  if (!buffer || maxLen < 32) return 0;

  int n = WiFi.scanComplete();
  if (n < 0) return snprintf(buffer, maxLen, "{\"networks\":[]}");

  size_t pos = snprintf(buffer, maxLen, "{\"networks\":[");
  for (int i = 0; i < n && i < 10 && pos < maxLen - 60; i++) {
    if (i > 0) pos += snprintf(buffer + pos, maxLen - pos, ",");
    pos += snprintf(buffer + pos, maxLen - pos,
      "{\"s\":\"%s\",\"r\":%d,\"e\":%d}",
      WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  pos += snprintf(buffer + pos, maxLen - pos, "]}");
  WiFi.scanDelete();
  return pos;
}

#endif // WLED_ENABLE_BLE_PROVISIONING
