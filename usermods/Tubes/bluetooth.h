#pragma once

#include <NimBLEDevice.h>
#include <esp_coexist.h>


typedef uint16_t MeshId;

typedef struct {
  MeshId id = 0;
  MeshId uplinkId = 0;
} MeshIds;


#define UPLINK_TIMEOUT 10000  // Time at which uplink is presumed lost

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("Client connected");
        Serial.println("Multi-connect support: start advertising");
        NimBLEDevice::startAdvertising();
    };

    /** Alternative onConnect() method to extract details of the connection.
     *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
     */
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        Serial.print("Client address: ");
        Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        /** We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments, try for 5x interval time for best results.
         */
        pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
    };

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("Client disconnected - start advertising");
        NimBLEDevice::startAdvertising();
    };

    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
    };

/********************* Security handled here **********************
****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest(){
        Serial.println("Server Passkey Request");
        /** This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key){
        Serial.print("The passkey YES/NO number: ");Serial.println(pass_key);
        /** Return false if passkeys don't match. */
        return true;
    };

    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        /** Check that encryption was successful, if not we disconnect the client */
        if(!desc->sec_state.encrypted) {
            NimBLEDevice::getServer()->disconnect(desc->conn_handle);
            Serial.println("Encrypt connection failed - disconnecting client");
            return;
        }
        Serial.println("Starting BLE work!");
    };
};

/** Handler class for characteristic actions */
class CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic){
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onRead(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
    };

    void onWrite(NimBLECharacteristic* pCharacteristic) {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onWrite(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
    };

    /** Called before notification or indication is sent,
     *  the value can be changed here before sending if desired.
     */
    void onNotify(NimBLECharacteristic* pCharacteristic) {
        Serial.println("Sending notification to clients");
    };


    /** The status returned in status is defined in NimBLECharacteristic.h.
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, Status status, int code) {
        String str = ("Notification/Indication status code: ");
        str += status;
        str += ", return code: ";
        str += code;
        str += ", ";
        str += NimBLEUtils::returnCodeToString(code);
        Serial.println(str);
    };

    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
        String str = "Client ID: ";
        str += desc->conn_handle;
        str += " Address: ";
        str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
        if(subValue == 0) {
            str += " Unsubscribed to ";
        }else if(subValue == 1) {
            str += " Subscribed to notfications for ";
        } else if(subValue == 2) {
            str += " Subscribed to indications for ";
        } else if(subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID()).c_str();

        Serial.println(str);
    };
};

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor) {
        std::string dscVal = pDescriptor->getValue();
        Serial.print("Descriptor witten value:");
        Serial.println(dscVal.c_str());
    };

    void onRead(NimBLEDescriptor* pDescriptor) {
        Serial.print(pDescriptor->getUUID().toString().c_str());
        Serial.println(" Descriptor read");
    };
};

/** Define callback instances globally to use for multiple Charateristics \ Descriptors */
static DescriptorCallbacks dscCallbacks;
static CharacteristicCallbacks chrCallbacks;


class BLEMeshNode: public NimBLEAdvertisedDeviceCallbacks {
  public:
    bool alive = false;                            // true if radio booted up

    MeshIds ids;
    byte buffer[100];
    char node_name[20];

    uint16_t serviceUUID = 0xD00F;

    NimBLEServer* pServer = NULL;
    NimBLEService* pService = NULL;
    NimBLEScan* pScanner = NULL;

    Timer uplinkTimer;

    MeshId newMeshId() {
        return random(0, 4000);
    }

    void advertise() {
        if (!pService)
            return;

        // Add the services to the advertisement data
        auto pAdvertising = NimBLEDevice::getAdvertising();
        if (!pAdvertising)
            return;

        auto service_data = std::string((char *)&ids, sizeof(ids));
        if (ids.uplinkId)
            sprintf(node_name, "Tube %03X:%03X", ids.id, ids.uplinkId);
        else
            sprintf(node_name, "Tube %03X", ids.id);

        // // // Reset the device name
        // NimBLEDevice::deinit(false);
        // NimBLEDevice::init(node_name);

        // Set advertisement
        pAdvertising->stop();
        pAdvertising->setServiceData(NimBLEUUID(serviceUUID), service_data);
        pAdvertising->start();

        Serial.printf("Advertising %s", node_name);
        Serial.println();
    }

    void init_service() {
        /** Optional: set the transmit power, default is 3db */
    #ifdef ESP_PLATFORM
        NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
    #else
        NimBLEDevice::setPower(9); /** +9db */
    #endif

        /** Set the IO capabilities of the device, each option will trigger a different pairing method.
         *  BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
         *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
         *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
         */
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        NimBLEDevice::setSecurityAuth(false, false, true);

        pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(new ServerCallbacks());

        pService = pServer->createService("D00B");
        NimBLECharacteristic* pCharacteristic = pService->createCharacteristic(
            "FEED",
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::BROADCAST
        );

        pCharacteristic->setValue("Test");
        pCharacteristic->setCallbacks(&chrCallbacks);

        /** Start the services when finished creating all Characteristics and Descriptors */
        pService->start();

        /** Add the services to the advertisement data **/
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(pService->getUUID());
        pAdvertising->setScanResponse(false);
        pAdvertising->setAppearance(0x07C6);  // Multi-color LED array
        pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_UND);

        advertise();
    }

    void init_scanner() {
        NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE);
        NimBLEDevice::setScanDuplicateCacheSize(200);

        pScanner = NimBLEDevice::getScan(); //create new scan
        // Set the callback for when devices are discovered, no duplicates.
        pScanner->setAdvertisedDeviceCallbacks(this, false);
        pScanner->setActiveScan(false); // Don't request data (it uses more energy)
        pScanner->setInterval(200); // How often the scan occurs / switches channels; in milliseconds,
        pScanner->setWindow(80);  // How long to scan during the interval; in milliseconds.
        pScanner->setMaxResults(0); // do not store the scan results, use callback only.
    }

    void init() {
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        esp_coex_preference_set(ESP_COEX_PREFER_BT);
        NimBLEDevice::init(std::string("Tube"));
        init_scanner();
        init_service();
        this->alive = true;
    }

    void uplink_ping() {
        // Track the last time we received a message from our master
        this->uplinkTimer.start(UPLINK_TIMEOUT);
    }

    void update() {
        // Don't do anything for the first half-second to avoid crashing WiFi
        if (millis() < 500)
            return;

        if (!this->alive) {
            this->init();
        }

        // Check the last time we heard from the uplink node
        if (is_following() && this->uplinkTimer.ended()) {
            Serial.println("Uplink lost");
            follow(0);
        }

        if (!this->pScanner->isScanning()) {
            // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
            this->pScanner->start(0, nullptr, false);
        }
    }

    void broadcast(byte *data, int size) {
        if (size > sizeof(buffer)) {
            Serial.println("Too much data");
            return;
        }

        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, data, size);

        if (!pServer)
            return;

        // Broadcast the current effect state to every connected client
        if (pServer->getConnectedCount() == 0)
            return;

        if (!pService)
            return;

        NimBLECharacteristic* pCharacteristic = pService->getCharacteristic("FEED");
        if(!pCharacteristic)
            return;
        
        // Update the characteristic        
        pCharacteristic->setValue(buffer);
        pCharacteristic->notify(true);
    }

    void reset(MeshId id = 0) {
        if (id == 0)
            id = newMeshId();
        this->ids.id = id;

        Serial.printf("My ID is %03X", this->ids.id);
        if (this->ids.id > this->ids.uplinkId)
            this->ids.uplinkId = 0;

        advertise();
    }

    void follow(MeshId uplinkId) {
        if (this->ids.uplinkId == uplinkId)
            return;

        this->ids.uplinkId = uplinkId;
        advertise();
    }

    bool is_following() {
        return this->ids.uplinkId != 0;
    }

    // ====== CALLBACKS =======
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (!advertisedDevice->isAdvertisingService(NimBLEUUID("D00B")))
            return;

        // Make sure it's booted up and advertising data
        auto data = advertisedDevice->getServiceData(NimBLEUUID(serviceUUID));
        if (!data.length())
            return;

        MeshIds data_ids;
        memcpy(&data_ids, data.c_str(), data.length());
        Serial.printf("%03X/%03X ", data_ids.id, data_ids.uplinkId);

        if (data_ids.id >= ids.uplinkId) {
            follow(data_ids.id);
            uplink_ping();
        }

        Serial.printf("Found: %s: %s\n",
            std::string(advertisedDevice->getAddress()).c_str(),
            std::string(advertisedDevice->getName()).c_str()
        );
    }

};
