#include "BLE.h"

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        printf("Client address: %s\n", connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        printf("Client disconnected - start advertising\n");
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }

    /********************* Security handled here *********************/
    uint32_t onPassKeyDisplay() override {
        printf("Server Passkey Display\n");
        /**
         * This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    }

    void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
        printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        /** Check that encryption was successful, if not we disconnect the client */
        if (!connInfo.isEncrypted()) {
            NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
            printf("Encrypt connection failed - disconnecting client\n");
            return;
        }

        printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
    }
} serverCallbacks;

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        printf("%s : onRead(), value: %s\n",
               pCharacteristic->getUUID().toString().c_str(),
               pCharacteristic->getValue().c_str());
    }

    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        printf("%s : onWrite(), value: %s\n",
               pCharacteristic->getUUID().toString().c_str(),
               pCharacteristic->getValue().c_str());
    }

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
    }

    /** Peer subscribed to notifications/indications */
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
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

        printf("%s\n", str.c_str());
    }
} characteristicCallbacks;

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
        std::string dscVal = pDescriptor->getValue();
        printf("Descriptor written value: %s\n", dscVal.c_str());
    }

    void onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
        printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
    }
} descriptorCallbacks;

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
        /*
        if(enabled)
        {
            DEBUG_PRINTLN(F("Initializing BLE..."));
            um_data = new um_data_t();
            um_data->u_size = 1;
            um_data->u_type = new um_types_t[um_data->u_size];
            um_data->u_data = new void*[um_data->u_size];
            um_data->u_data[0] = &deviceConnected;      //*used (New)
            um_data->u_type[0] = UMT_BYTE;

            DEBUG_PRINTLN(F("Calling ini..."));
            NimBLEDevice::init(serverDescription);

            // Create the BLE Server
            DEBUG_PRINTLN(F("creating server..."));
            _pServer = NimBLEDevice::createServer();
            DEBUG_PRINTLN(F("setting callbacks..."));
            _pServer->setCallbacks(new MyServerCallbacks());
            DEBUG_PRINTLN(F("disabe advertise on disconnect..."));
            _pServer->advertiseOnDisconnect(false);

            // Create the BLE Service
            DEBUG_PRINTLN(F("Creating service..."));
            _pService = _pServer->createService(SERVICE_UUID);

            // Create a BLE Characteristic
            DEBUG_PRINTLN(F("creating characteristic..."));
            _pCharacteristic = _pService->createCharacteristic( CHARACTERISTIC_UUID,
                                                                NIMBLE_PROPERTY::READ
                                                              | NIMBLE_PROPERTY::WRITE
                                                              | NIMBLE_PROPERTY::NOTIFY );

            // Start the service
            DEBUG_PRINTLN(F("starting service..."));
            _pService->start();

            // Start advertising
            DEBUG_PRINTLN(F("get advertising..."));
            _pAdvertising = NimBLEDevice::getAdvertising();
            _pAdvertising->addServiceUUID(SERVICE_UUID);
            _pAdvertising->enableScanResponse(false);
            _pAdvertising->setAdvertisingCompleteCallback(onAdvertisingComplete);
            DEBUG_PRINTLN(F("starting advertising..."));
            _pAdvertising->start();
            DEBUG_PRINTLN(F("Waiting a client connection to notify..."));
        }
        */

        printf("Starting NimBLE Server\n");

        /** Initialize NimBLE and set the device name */
        NimBLEDevice::init("NimBLE");

        /**
        * Set the IO capabilities of the device, each option will trigger a different pairing method.
        *  BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
        *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
        *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
        */
        // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
        // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

        /**
        *  2 different ways to set security - both calls achieve the same result.
        *  no bonding, no man in the middle protection, BLE secure connections.
        *
        *  These are the default values, only shown here for demonstration.
        */
        // NimBLEDevice::setSecurityAuth(false, false, true);

        NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
        pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(&serverCallbacks);

        NimBLEService*        pDeadService = pServer->createService("DEAD");
        NimBLECharacteristic* pBeefCharacteristic =
            pDeadService->createCharacteristic("BEEF",
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                                                    /** Require a secure connection for read and write access */
                                                    NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
                                                    NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
            );

        pBeefCharacteristic->setValue("Burger");
        pBeefCharacteristic->setCallbacks(&characteristicCallbacks);

        /**
        *  2902 and 2904 descriptors are a special case, when createDescriptor is called with
        *  either of those uuid's it will create the associated class with the correct properties
        *  and sizes. However we must cast the returned reference to the correct type as the method
        *  only returns a pointer to the base NimBLEDescriptor class.
        */
        NimBLE2904* pBeef2904 = pBeefCharacteristic->create2904();
        pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
        pBeef2904->setCallbacks(&descriptorCallbacks);

        NimBLEService*        pBaadService = pServer->createService("BAAD");
        NimBLECharacteristic* pFoodCharacteristic =
            pBaadService->createCharacteristic("F00D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

        pFoodCharacteristic->setValue("Fries");
        pFoodCharacteristic->setCallbacks(&characteristicCallbacks);

        /** Custom descriptor: Arguments are UUID, Properties, max length of the value in bytes */
        NimBLEDescriptor* pC01Ddsc =
            pFoodCharacteristic->createDescriptor("C01D",
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC,
                                                    20);
        pC01Ddsc->setValue("Send it back!");
        pC01Ddsc->setCallbacks(&descriptorCallbacks);

        /** Start the services when finished creating all Characteristics and Descriptors */
        pDeadService->start();
        pBaadService->start();

        /** Create an advertising instance and add the services to the advertised data */
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->setName("NimBLE-Server");
        pAdvertising->addServiceUUID(pDeadService->getUUID());
        pAdvertising->addServiceUUID(pBaadService->getUUID());
        /**
        *  If your device is battery powered you may consider setting scan response
        *  to false as it will extend battery life at the expense of less data sent.
        */
        pAdvertising->enableScanResponse(true);
        pAdvertising->start();

        printf("Advertising Started\n");

        initDone = true;
    }
  
    void loop()
    {
        /*
        // notify changed value
        if (deviceConnected) {
        pCharacteristic->setValue((uint8_t*)&value, 4);
        pCharacteristic->notify();
        value++;
        }

        if (!deviceConnected && oldDeviceConnected) {
            pAdvertising = NimBLEDevice::getAdvertising();
            if (NimBLEDevice::getWhiteListCount() > 0) {
                // Allow anyone to scan but only whitelisted can connect.
                pAdvertising->setScanFilter(false,true);
            }
            // advertise with whitelist for 30 seconds
            pAdvertising->start(30 * 1000);
            DEBUG_PRINTLN(F("start advertising"));
            oldDeviceConnected = deviceConnected;
        }

        if (deviceConnected && !oldDeviceConnected) {
            // do stuff here on connecting
            oldDeviceConnected = deviceConnected;
        }
        */
        if (pServer->getConnectedCount())
        {
            NimBLEService* pSvc = pServer->getServiceByUUID("BAAD");
            if (pSvc)
            {
                NimBLECharacteristic* pChr = pSvc->getCharacteristic("F00D");
                if (pChr)
                {
                    pChr->notify();
                }
            }
        }
    }

    bool getUMData(um_data_t **data) override
    {
        if (!data || !_enabled) return false; // no pointer provided by caller or not enabled -> exit
            *data = um_data;
        return true;
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
