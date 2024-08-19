#if 0
#pragma once

// THIS FILE ISN'T USED ANY MORE

#include <NimBLEDevice.h>
#include <esp_coexist.h>
#include "global_state.h"


#include "node.h"

#define MAX_CONNECTED_CLIENTS 3

#define DATA_UPDATE_SERVICE  "D00B"

typedef struct {
    MeshNodeHeader header;
    TubeState current;
    TubeState next;
} MeshStorage;

// Asynchronous queue handling
typedef struct {
    MeshId id;
    NimBLEAddress address;
} MeshUpdateRequest;

static TaskHandle_t xUpdaterTaskHandle;
QueueHandle_t UpdaterQueue = xQueueCreate(5, sizeof(MeshUpdateRequest));
void procUpdaterTask(void* pvParameters);

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("Client connected");
        if (pServer->getConnectedCount() < MAX_CONNECTED_CLIENTS)
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

        ESP.restart();
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


NimBLEClient* connectToServer(NimBLEAddress &peer_address) {
    NimBLEClient* pClient = nullptr;
    bool known_server = false;

    // Check if we have a client we should reuse
    if (NimBLEDevice::getClientListSize()) {
        // If we already know this peer, send false as the second argument in connect()
        // to prevent refreshing the service database. This saves considerable time and power.
        pClient = NimBLEDevice::getClientByPeerAddress(peer_address);
        if (pClient)
            known_server = true;
        else
            pClient = NimBLEDevice::getDisconnectedClient();

        if (pClient) {
            if (pClient->connect(peer_address, known_server))
                return pClient;

            // Failed to connect. Just drop the client rather than deleting it.
            return NULL;
        }
    }

    // No client to reuse; create a new one - if we can.
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
        Serial.println("Max clients reached - no more connections available");
        return NULL;
    }
    pClient = NimBLEDevice::createClient();

    // Set up this client and attempt to connect.
    pClient->setConnectionParams(12,12,0,51);
    pClient->setConnectTimeout(5);
    if (!pClient->connect(peer_address)) {
        // Created a client but failed to connect, don't need to keep it as it has no data.
        NimBLEDevice::deleteClient(pClient);
        return NULL;
    }

    return pClient;
}


class BLEMeshNode: public NimBLEAdvertisedDeviceCallbacks {
  public:
    bool alive = false;                            // true if radio booted up
    bool changed = false;

    MeshNodeHeader ids;

    uint16_t serviceUUID = 0xD00F;

    NimBLEServer* pServer = nullptr;
    NimBLEService* pService = nullptr;
    NimBLEScan* pScanner = nullptr;
    NimBLEAddress uplink_address;

    MessageReceiver *receiver = nullptr;

    BLEMeshNode(MessageReceiver *receiver) {
        this->receiver = receiver;
    }

    void advertise() {
        auto service_data = std::string((char *)&ids, sizeof(ids));

#ifdef BLE_MESH
        if (!pService)
            return;

        // Add the services to the advertisement data
        auto pAdvertising = NimBLEDevice::getAdvertising();
        if (!pAdvertising)
            return;


        // Reset the device name:
        // NimBLEDevice::deinit(false);
        // NimBLEDevice::init(node_name);

        // Set advertisement
        pAdvertising->stop();
        pAdvertising->setServiceData(NimBLEUUID(serviceUUID), service_data);
        pAdvertising->start();
#endif        

        Serial.printf("Advertising %s\n", node_name);
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
        pServer->advertiseOnDisconnect(true);

        pService = pServer->createService(DATA_UPDATE_SERVICE);
        NimBLECharacteristic* pCharacteristic = pService->createCharacteristic(
            "FEED",
            NIMBLE_PROPERTY::READ
        );
        pCharacteristic->setValue("");
        pCharacteristic->setCallbacks(&chrCallbacks);

        pCharacteristic = pService->createCharacteristic(
            "F00D",
            NIMBLE_PROPERTY::WRITE
        );
        pCharacteristic->setCallbacks(&chrCallbacks);

        /** Start the services when finished creating all Characteristics and Descriptors */
        pService->start();

        /** Add the services to the advertisement data **/
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(pService->getUUID());
        pAdvertising->setScanResponse(false);
        pAdvertising->setAppearance(0x07C6);  // Multi-color LED array
        pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_UND);

        changed = true;
    }

    void init_scanner() {
        NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE);
        NimBLEDevice::setScanDuplicateCacheSize(20);

        pScanner = NimBLEDevice::getScan(); //create new scan
        // Set the callback for when devices are discovered, no duplicates.
        pScanner->setAdvertisedDeviceCallbacks(this, false);
        pScanner->setActiveScan(false); // Don't request data (it uses more energy)
        pScanner->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
        pScanner->setWindow(37);  // How long to scan during the interval; in milliseconds.
        pScanner->setMaxResults(0); // do not store the scan results, use callback only.
    }

    void init_updater() {
        xTaskCreate(
            procUpdaterTask, /* Function to implement the task */
            "UpdaterTask", /* Name of the task */
            3840, /* Stack size in bytes */
            this, /* Task input parameter */
            tskIDLE_PRIORITY+1, /* Priority of the task */
            &xUpdaterTaskHandle /* Task handle. */
        );
    }

    void init() {
        WiFi.mode(WIFI_AP_STA);
        
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

#ifdef BLE_MESH
        esp_coex_preference_set(ESP_COEX_PREFER_BT);
        NimBLEDevice::init(std::string("Tube"));
        init_scanner();
        init_service();
        init_updater();
#endif
        this->alive = true;
    }

    void onPeerPing(MeshNodeHeader* pRemoteNode, NimBLEAdvertisedDevice* pAdvertisedDevice) {
        Serial.printf("Found %03X/%03X at %s\n",
            pRemoteNode->id,
            pRemoteNode->uplinkId,
            std::string(pAdvertisedDevice->getAddress()).c_str()
        );

        if (pRemoteNode->id == ids.id) {
            Serial.println("Detected an ID conflict.");
            this->reset();
        }

        if (pRemoteNode->id > ids.id && pRemoteNode->id > ids.uplinkId) {
            follow(pRemoteNode->id, pAdvertisedDevice);
        }

        if (pRemoteNode->id == ids.uplinkId) {
            this->onUplinkAlive();
        }
    }

    void setup() {
        Serial.println("Mesh: ok");
    }

    void update() {
        // Don't do anything for the first second to avoid crashing WiFi        
        if (millis() < 1000)
            return;

        if (!this->alive) {
            this->init();
        }

#ifdef BLE_MESH
            MeshUpdateRequest request = {
                .id = this->ids.uplinkId,
                .address = this->uplink_address
            };
            if (xQueueSend(UpdaterQueue, &request, 0) != pdTRUE) {
                Serial.println("Update queue is full!");
            }
#endif
        }

        // If any actions caused the service to change, re-advertise with new values
        if (changed) {
            advertise();
            changed = false;
        }

        if (this->pScanner && !this->pScanner->isScanning()) {
            // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
            this->pScanner->start(0, nullptr, false);
        }
    }

    void update_node_storage(TubeState &current, TubeState &next) {
#ifdef BLE_MESH
        // Broadcast the current effect state to every connected client

        if (!pServer || pServer->getConnectedCount() == 0)
            return;

        if (!pService)
            return;

        NimBLECharacteristic* pCharacteristic = pService->getCharacteristic("FEED");
        if(!pCharacteristic)
            return;
        
        // Store this data in the characteristic
        MeshStorage storage = {
            .header = this->ids,
            .current = current,
            .next = next
        };
        pCharacteristic->setValue(storage);
#endif
    }

    // ====== CALLBACKS =======
    void onResult(NimBLEAdvertisedDevice* pAdvertisedDevice) {
        // Discovered a peer via scanning.

        if (!pAdvertisedDevice->isAdvertisingService(NimBLEUUID("D00B")))
            return;

        // Make sure it's booted up and advertising Mesh IDs
        auto data = pAdvertisedDevice->getServiceData(NimBLEUUID(serviceUUID));
        if (data.length() != sizeof(MeshNodeHeader))
            return;
        MeshNodeHeader* pRemoteNode = (MeshNodeHeader *)data.c_str();
        if (pRemoteNode->version != this->ids.version)
            return;

        this->onPeerPing(pRemoteNode, pAdvertisedDevice);
    }

    void onUpdateData(MeshUpdateRequest &request, MeshStorage &storage) {
        if (request.id != storage.header.id) {
            Serial.println("Uplink is invalid!");
            // The remote server has changed its ID.  We need to adapt.
            follow(0);
            changed = true;
            return;
        }
        this->onUplinkAlive();

        // Process the command
        this->receiver->onCommand(
            storage.header.id,
            COMMAND_STATE,
            &storage.current
        );
        this->receiver->onCommand(
            storage.header.id,
            COMMAND_NEXT,
            &storage.next
        );
    }

    void onUplinkAlive() {
        // Track the last time we received a message from our uplink
        this->uplinkTimer.start(UPLINK_TIMEOUT);
    }

};


// UPDATER
// This is an async task handler that awaits requests to update data,
// then connects to the requested server and fetches the data.
void procUpdaterTask(void* pvParameters) {
    BLEMeshNode *pNode = (BLEMeshNode*)pvParameters;
    MeshUpdateRequest request;

    for (;;) {
        // Wait to be told to update (the queue blocks)
        xQueueReceive(UpdaterQueue, &request, portMAX_DELAY);

        // Got a request to update, so try to connect and pull down data.
        auto uplink_address = request.address;
        auto pClient = connectToServer(uplink_address);
        if (!pClient)
            continue;

        auto pService = pClient->getService(DATA_UPDATE_SERVICE);
        if (pService) {
            auto pCharacteristic = pService->getCharacteristic("FEED");
            MeshStorage storage = pCharacteristic->readValue<MeshStorage>();
            pNode->onUpdateData(request, storage);
        }
        pClient->disconnect();
    }

}
#endif