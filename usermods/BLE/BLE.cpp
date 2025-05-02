#include "BLE.h"
#include "NimBLEDevice.h"

/** Handler class for server events */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        printf("Client connected:: %s\n", connInfo.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        printf("Client disconnected - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
        esp_deep_sleep_start();
    }
} serverCallbacks;

/** Callback class to handle advertising events */
class AdvertisingCallbacks : public NimBLEExtAdvertisingCallbacks {
    void onStopped(NimBLEExtAdvertising* pAdv, int reason, uint8_t instId) override {
        /* Check the reason advertising stopped, don't sleep if client is connecting */
        printf("Advertising instance %u stopped\n", instId);
        switch (reason) {
            case 0:
                printf("Client connecting\n");
                return;
            case BLE_HS_ETIMEOUT:
                printf("Time expired - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
                break;
            default:
                break;
        }

        esp_deep_sleep_start();
    }
} advertisingCallbacks;

class BLEUsermod : public Usermod {

    private:    
    bool                  deviceConnected     = false;
    bool                  oldDeviceConnected  = false;
    uint32_t              value               = 0;
    bool                  enabled             = false;
    bool                  initDone            = false;
  
    static const char _name[];
    static const char _enabled[];
  
    // Private class members. You can declare variables and functions only accessible to your usermod here
    NimBLEServer*         pServer             = nullptr;
    NimBLEService*        pService            = nullptr;
    NimBLECharacteristic* pCharacteristic     = nullptr;
    NimBLEAdvertising*    pAdvertising        = nullptr;

    void setup()
    {
        /** Initialize NimBLE and set the device name */
        NimBLEDevice::init("Extended advertiser");

        /** Create the server and add the services/characteristics/descriptors */
        NimBLEServer* pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(&serverCallbacks);

        NimBLEService*        pService = pServer->createService(SERVICE_UUID);
        NimBLECharacteristic* pCharacteristic =
            pService->createCharacteristic(CHARACTERISTIC_UUID,
                                        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

        pCharacteristic->setValue("Hello World");

        /** Start the service */
        pService->start();

        /**
         *  Create an extended advertisement with the instance ID 0 and set the PHY's.
         *  Multiple instances can be added as long as the instance ID is incremented.
         */
        NimBLEExtAdvertisement extAdv(primaryPhy, secondaryPhy);

        /** Set the advertisement as connectable */
        extAdv.setConnectable(true);

        /** As per Bluetooth specification, extended advertising cannot be both scannable and connectable */
        extAdv.setScannable(false); // The default is false, set here for demonstration.

        /** Extended advertising allows for 251 bytes (minus header bytes ~20) in a single advertisement or up to 1650 if chained */
        extAdv.setServiceData(NimBLEUUID(SERVICE_UUID),
                            std::string("Extended Advertising Demo.\r\n"
                                        "Extended advertising allows for "
                                        "251 bytes of data in a single advertisement,\r\n"
                                        "or up to 1650 bytes with chaining.\r\n"
                                        "This example message is 226 bytes long "
                                        "and is using CODED_PHY for long range."));

        extAdv.setName("Extended advertiser");

        /** When extended advertising is enabled `NimBLEDevice::getAdvertising` returns a pointer to `NimBLEExtAdvertising */
        NimBLEExtAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

        /** Set the callbacks for advertising events */
        pAdvertising->setCallbacks(&advertisingCallbacks);

        /**
         *  NimBLEExtAdvertising::setInstanceData takes the instance ID and
         *  a reference to a `NimBLEExtAdvertisement` object. This sets the data
         *  that will be advertised for this instance ID, returns true if successful.
         *
         *  Note: It is safe to create the advertisement as a local variable if setInstanceData
         *  is called before exiting the code block as the data will be copied.
         */
        if (pAdvertising->setInstanceData(0, extAdv)) {
            /**
             *  NimBLEExtAdvertising::start takes the advertisement instance ID to start
             *  and a duration in milliseconds or a max number of advertisements to send (or both).
             */
            if (pAdvertising->start(0, advTime)) {
                printf("Started advertising\n");
            } else {
                printf("Failed to start advertising\n");
            }
        } else {
            printf("Failed to register advertisement data\n");
        }

        esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000);
    }
  
    void loop()
    {
    }

    bool readFromConfig(JsonObject& root) override
    {
        JsonObject top = root[FPSTR(_name)];
        bool configComplete = !top.isNull();

        configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);

        return configComplete;
    }
  
    void addToConfig(JsonObject &root) override
    {
        JsonObject top = root.createNestedObject(FPSTR(_name));
        top[FPSTR(_enabled)] = enabled;
    }
  
    uint16_t getId(){return USERMOD_ID_BLE;}
};

// strings to reduce flash memory usage (used more than twice)
const char BLEUsermod::_name[]       PROGMEM = "BLE";
const char BLEUsermod::_enabled[]    PROGMEM = "enabled";

static BLEUsermod usermod_BLE;
REGISTER_USERMOD(usermod_BLE);
