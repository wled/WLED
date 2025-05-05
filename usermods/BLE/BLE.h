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

#include "wled.h"
#include <NimBLEDevice.h>

class BLEUsermod : public Usermod {
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
    NimBLEAdvertising*    pAdvertising        = nullptr;
 
    void startServicesAndAdvertising();

    public:
    void setup() override;
    void loop() override;
    void connected() override;
    void addToJsonState(JsonObject& root) override;
    void readFromJsonState(JsonObject& root) override;
    bool readFromConfig(JsonObject& root) override;
    void addToConfig(JsonObject &root) override;
    bool handleButton(uint8_t b) override;
    uint16_t getId() override {return USERMOD_ID_BLE;}
    void disableBLE();
};
