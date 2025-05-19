#include "wled.h"
#include "BLE.h"

#ifdef WLED_DEBUG
void BLEUsermod::DEBUG_STATUS()
{
    DEBUG_PRINTF_P(PSTR("WLED_CONNECTED: %d\n"), WLED_CONNECTED);
    DEBUG_PRINTF_P(PSTR("WiFi.isConnected: %d\n"), WiFi.isConnected());
    DEBUG_PRINTF_P(PSTR("apBehavior: %d\n"), apBehavior);
    DEBUG_PRINTF_P(PSTR("apActive: %d\n"), apActive);
    DEBUG_PRINTF_P(PSTR("WiFi status: %d\n"), WiFi.status());
    DEBUG_PRINTF_P(PSTR("BLE enabled: %d\n"), enabled);
    if(initDone)
    {
        DEBUG_PRINTF_P(PSTR("BLE advertising: %d\n"), isAdvertising());
        DEBUG_PRINTF_P(PSTR("BLE connected devices: %d\n"), pServer->getConnectedCount());
    }
}
#else
void BLEUsermod::DEBUG_STATUS(){}
#endif

void BLEUsermod::shutdownWiFi()
{
  if(apActive) {
    WLED::instance().shutdownAP();
  }
  if (WiFi.isConnected()) {
    WiFi.disconnect();
  }
  wifiEnabled = false;
  WiFi.mode(WIFI_OFF);
}
void BLEUsermod::test(NimBLEAdvertising* pAdvertising)
{
    DEBUG_PRINTLN(F("BLEUsermod::test() called"));
}

void BLEUsermod::setup()
{
    DEBUG_PRINTLN(F("BLEUsermod::setup called"));
    DEBUG_STATUS();

    if(!enabled) return;
    DEBUG_PRINTLN(F("Starting NimBLE Server************************************************************"));
    
    if(!initDone) {
        //shutdownWiFi();
       //TODO: this is wrong.  it reports true for ETH also
        // if(WLED_CONNECTED | apActive) {
        //     DEBUG_PRINTLN(F("waiting for WiFi to disconnect"));
        //     return;
        // }
        // apBehavior = AP_BEHAVIOR_BUTTON_ONLY;
        /** Initialize NimBLE and set the device name */
        if(NimBLEDevice::init(serverDescription))
        {
            NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_SC);
            DEBUG_PRINTLN(F("NimBLE init'd"));
            pServer = NimBLEDevice::createServer();
            if(pServer)
            {
                DEBUG_PRINTLN(F("BLE server created"));
                pServer->setCallbacks(this);
                pService = pServer->createService(WLED_BLE_SERVICE_UUID);
                if(pService)
                {
                    DEBUG_PRINTLN(F("BLE service created"));
                    pCharacteristic = pService->createCharacteristic(WLED_BLE_CHARACTERISTIC_UUID,
                                                                        NIMBLE_PROPERTY::READ |
                                                                        NIMBLE_PROPERTY::WRITE //|
                                                                        /** Require a secure connection for read and write access */
                                                                        // NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
                                                                        // NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
                                                                    );
                    if(pCharacteristic)
                    {
                        DEBUG_PRINTLN(F("BLE characteristic created"));
                        pCharacteristic->setValue("WLED");
                        pCharacteristic->setCallbacks(this);
                        if(pService->start())
                        {
                            DEBUG_PRINTLN(F("BLE service started"));
                            initDone = true;
                        } else {
                            DEBUG_PRINTLN(F("Unable to start BLE service"));
                        }
                    } else {
                        DEBUG_PRINTLN(F("Unable to create BLE characteristic"));
                    }
                } else {
                    DEBUG_PRINTLN(F("Unable to create BLE service"));
                }
            } else {
                DEBUG_PRINTLN(F("Unable to create BLE server"));
            }
        } else {
            DEBUG_PRINTLN(F("Unable to initialize NimBLE"));
        }

        if(pService->isStarted() && !isAdvertising() && !apActive && !WiFi.isConnected())
        {
            NimBLEAdvertising* pAdvertising = pServer->getAdvertising();
            pAdvertising->setName(serverDescription);
            pAdvertising->addServiceUUID(WLED_BLE_SERVICE_UUID);
            DEBUG_PRINTLN(F("BLE about to pAdvertising->enableScanResponse(true)"));
            pAdvertising->enableScanResponse(true);
            DEBUG_PRINTLN(F("BLE starting advertising"));
            pAdvertising->setAdvertisingCompleteCallback(std::bind(&BLEUsermod::test, this, std::placeholders::_1));

            if(pAdvertising->start())
            {
                DEBUG_PRINTLN(F("BLE advertising started"));
            } else {
                DEBUG_PRINTLN(F("Unable to start BLE advertising"));
            }
        } else {
            DEBUG_PRINTLN(F("BLE advertising not started"));
            DEBUG_STATUS();
        }
    }
}
  
void BLEUsermod::loop()
{
    // if usermod is disabled or called during strip updating just exit
    // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
    if (!enabled || strip.isUpdating()) return;
    if (!initDone){
        DEBUG_PRINTLN(F("BLE init not completed, calling setup from loop()"));
        setup();
    }

    if (millis() - lastTime > 2000 / portTICK_PERIOD_MS) {
        if (pServer->getConnectedCount()) {
            NimBLEService* pSvc = pServer->getServiceByUUID(WLED_BLE_SERVICE_UUID);
            if (pSvc) {
                NimBLECharacteristic* pChr = pSvc->getCharacteristic(WLED_BLE_CHARACTERISTIC_UUID);
                if (pChr) {
                    pChr->notify();
                }
            }
        }
        lastTime = millis();
    }
}

void BLEUsermod::connected()
{
    DEBUG_PRINTLN(F("BLEUsermod::connected() called"));
    DEBUG_STATUS();

    if(wifiEnabled || WLED_CONNECTED) {
        // turn off bluetooth
        enabled = false;
    }
}

void BLEUsermod::addToJsonState(JsonObject& root)
{
    DEBUG_PRINTLN(F("BLEUsermod::addToJsonState() called"));
    DEBUG_STATUS();
     if (!initDone || !enabled) return;  // prevent crash on boot applyPreset()

    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

    usermod[FPSTR(_enabled)] = enabled;
}
  
void BLEUsermod::readFromJsonState(JsonObject& root) 
{
    DEBUG_PRINTLN(F("BLEUsermod::readFromJsonState() called"));
    DEBUG_STATUS();
     if (!initDone) return;  // prevent crash on boot applyPreset()
    
    JsonObject usermod = root[FPSTR(_name)];
    if (!usermod.isNull()) {
        enabled = usermod[FPSTR(_enabled)] | enabled;
    }
}

bool BLEUsermod::readFromConfig(JsonObject& root) 
{
    DEBUG_PRINTLN(F("BLEUsermod::readFromConfig() called"));
    DEBUG_STATUS();
     JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);

    return configComplete;
}

void BLEUsermod::addToConfig(JsonObject &root) 
{
    DEBUG_PRINTLN(F("BLEUsermod::addToConfig() called"));
    DEBUG_STATUS();
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
}

// NimBLEServerCallbacks overrides
void BLEUsermod::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo)
{
    DEBUG_PRINTF("Client address: %s\n", connInfo.getAddress().toString().c_str());
    pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
}

void BLEUsermod::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
    DEBUG_PRINTF("Client disconnected - start advertising\n");
    if(!wifiEnabled)
        NimBLEDevice::startAdvertising();
}

void BLEUsermod::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo)
{
    DEBUG_PRINTF("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
}

void BLEUsermod::onAuthenticationComplete(NimBLEConnInfo& connInfo)
{
    DEBUG_PRINTLN(F("onAuthenticationComplete called"));
    /** Check that encryption was successful, if not we disconnect the client */
    if (!connInfo.isEncrypted()) {
        NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
        DEBUG_PRINTF("Encrypt connection failed - disconnecting client\n");
        return;
    }

    DEBUG_PRINTF("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
}

// NimBLECharacteristicCallbacks overrides
void BLEUsermod::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    DEBUG_PRINTF("%s : onRead(), value: %s\n",
           pCharacteristic->getUUID().toString().c_str(),
           pCharacteristic->getValue().c_str());
}

void BLEUsermod::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    DEBUG_PRINTF("%s : onWrite(), value: %s\n",
           pCharacteristic->getUUID().toString().c_str(),
           pCharacteristic->getValue().c_str());
}

/**
 *  The value returned in code is the NimBLE host return code.
 */
void BLEUsermod::onStatus(NimBLECharacteristic* pCharacteristic, int code)
{
    DEBUG_PRINTF("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
    DEBUG_PRINTLN();
}

/** Peer subscribed to notifications/indications */
void BLEUsermod::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue)
{
    std::string str  = "Client ID: ";
    str             += connInfo.getConnHandle();
    str             += " Address: ";
    str             += connInfo.getAddress().toString();
    if (subValue == 0) {
        str += " Unsubscribed to ";
    } else if (subValue == 1) {
        str += " Subscribed to notifications for ";
    } else if (subValue == 2) {
        str += " Subscribed to indications for ";
    } else if (subValue == 3) {
        str += " Subscribed to notifications and indications for ";
    }
    str += std::string(pCharacteristic->getUUID());

    DEBUG_PRINTF("%s\n", str.c_str());
    DEBUG_PRINTLN();
}

// DescriptorCallbacks overrides
void BLEUsermod::onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo)
{
    std::string dscVal = pDescriptor->getValue();
    DEBUG_PRINTF("Descriptor written value: %s", dscVal.c_str());
    DEBUG_PRINTLN();
}

void BLEUsermod::onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo)
{
    DEBUG_PRINTF("%s Descriptor read", pDescriptor->getUUID().toString().c_str());
    DEBUG_PRINTLN();
}


const char BLEUsermod::_name[]       PROGMEM = "BLE";
const char BLEUsermod::_enabled[]    PROGMEM = "enabled";

static BLEUsermod usermod_BLE;
REGISTER_USERMOD(usermod_BLE);
