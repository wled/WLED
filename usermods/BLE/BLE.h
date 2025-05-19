#ifndef USERMOD_BLE
    #define USERMOD_BLE
#endif
#define CONFIG_BT_ENABLED                   1
#define CONFIG_BTDM_CTRL_MODE_BLE_ONLY      1
#define CONFIG_BT_BLUEDROID_ENABLED         0
#define CONFIG_BT_NIMBLE_ENABLED            1
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED 
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER

#define WLED_BLE_SERVICE_UUID        "01FA0001-46C9-4507-84BB-F2BE3F24C47A"
#define WLED_BLE_CHARACTERISTIC_UUID "01FA0002-46C9-4507-84BB-F2BE3F24C47A"
#include "wled.h"
#include <NimBLEDevice.h>
// #  include "NimBLEServer.h"
// #  include "NimBLEService.h"
// #  include "NimBLECharacteristic.h"
// #  include "NimBLEDescriptor.h"

class BLEUsermod : public Usermod
                        , NimBLEServerCallbacks
                        , NimBLECharacteristicCallbacks
                        , NimBLEDescriptorCallbacks
{
    private:    
    bool                  deviceConnected     = false;
    bool                  oldDeviceConnected  = false;
    bool                  enabled             = false;
    bool                  initDone            = false;
    unsigned long         lastTime            = 0;
  
    static const char _name[];
    static const char _enabled[];
  
    // Private class members. You can declare variables and functions only accessible to your usermod here
    NimBLEServer*         pServer             = nullptr;
    NimBLEService*        pService            = nullptr;
    NimBLECharacteristic* pCharacteristic     = nullptr;
 
    void shutdownWiFi();
    void DEBUG_STATUS();
    void test(NimBLEAdvertising* pAdvertising);

    public:
    void setup() override;
    void loop() override;
    void connected() override;
    void addToJsonState(JsonObject& root) override;
    void readFromJsonState(JsonObject& root) override;
    bool readFromConfig(JsonObject& root) override;
    void addToConfig(JsonObject &root) override;
    //bool handleButton(uint8_t b) override;
    uint16_t getId() override {return USERMOD_ID_BLE;}
    void start()
    {
        if(enabled)
        {
            DEBUG_PRINTLN(F("starting BLE"));
            NimBLEDevice::startAdvertising();
        }
    }
    void stop()
    {
        DEBUG_PRINTLN(F("stopping BLE..."));
        NimBLEDevice::stopAdvertising();
        while (isAdvertising());
        DEBUG_PRINTLN(F("BLE stopped"));
    }
    void enable(bool enable) { enabled = enable; }
    bool isEnabled() {return enabled; }
    bool isAdvertising(){return NimBLEDevice::getAdvertising()->isAdvertising();}

    // NimBLEServer Callbacks
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override;
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override;

    // NimBLECharacteristic Callbacks
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;

    // NimBLEDescriptor Callbacks
    void onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override;
};
