#pragma once

#include <Arduino.h>
#if defined ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#elif defined ESP8266
#include <ESP8266WiFi.h>
#define WIFI_MODE_STA WIFI_STA 
#else
#error "Unsupported platform"
#endif //ESP32
#include <QuickEspNow.h>

#include "global_state.h"

#define CURRENT_NODE_VERSION 1
#define BROADCAST_ADDR ESPNOW_BROADCAST_ADDRESS 

#define BROADCAST_RATE 3000       // Rate at which to broadcast as leader
#define REBROADCAST_RATE 7000    // Rate at which to re-broadcast as follower
#define UPLINK_TIMEOUT 17000      // Time at which uplink is presumed lost

typedef uint16_t MeshId;

typedef struct {
    MeshId id = 0;
    MeshId uplinkId = 0;
    uint8_t version = CURRENT_NODE_VERSION;
} MeshNodeHeader;

typedef struct {
    MeshNodeHeader header;
    TubeState current;
    TubeState next;
} NodeMessage;

void onDataReceived (uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast);


class MessageReceiver {
  public:
    virtual void onCommand(CommandId command, MeshNodeHeader* header, void *data) {
      // Abstract: subclasses must define
    }  
};


#define NODE_STATUS_INIT 0
#define NODE_STATUS_BROADCASTING 1
#define NODE_STATUS_QUIET 2


class LightNode {
  public:
    static LightNode* instance;

    MessageReceiver *receiver;
    MeshNodeHeader header;

    char node_name[20];

    uint8_t status = NODE_STATUS_INIT;
    bool meshChanged = false;

    Timer uplinkTimer;
    Timer broadcastTimer;

    LightNode(MessageReceiver *receiver) {
        LightNode::instance = this;

        this->receiver = receiver;
    }

    void configure_ap() {
        strcpy(clientSSID, "");
        strcpy(clientPass, "");
        strcpy(apSSID, "");
        apBehavior = AP_BEHAVIOR_BOOT_NO_CONN;
    }

    void onWifiConnect() {
        if (this->status == NODE_STATUS_BROADCASTING) {
            Serial.println("Stop Broadcasting");
            quickEspNow.stop();
        }
        
        Serial.println("Stop broadcasting");
        this->status = NODE_STATUS_QUIET;
    }

    void onWifiDisconnect() {
        if (this->status == NODE_STATUS_BROADCASTING)
            return;
        
        WiFi.mode (WIFI_MODE_STA);
        WiFi.disconnect(false, true);
        quickEspNow.begin(1, WIFI_IF_STA);

        Serial.println("Broadcasting");
        this->status = NODE_STATUS_BROADCASTING;
    }

    void onMeshChange() {
        sprintf(this->node_name,
            "Tube %03X:%03X",
            this->header.id,
            this->header.uplinkId
        );
    }

    void onPeerData(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
        // Ignore this packet if it couldn't be a mesh report.
        if (len != sizeof(NodeMessage))
            return;

        NodeMessage* message = (NodeMessage*)data;
        // Serial.printf(">> Received %db ", len);
        // Serial.printf("from %03X/%03X ", message->header.id, message->header.uplinkId);
        // Serial.printf("at " MACSTR, MAC2STR(address));
        // Serial.printf("@ %ddBm: ", rssi);

        // Ignore this packet if wrong version
        if (message->header.version != this->header.version) {
            // Serial.printf(" <version\n", rssi);        
            return;
        }

        // Ignore this packet if not from (at least) my uplink
        if (message->header.id <= this->header.uplinkId) {
            // Serial.printf(" ignoring\n", rssi);        
            return;
        }

        // Serial.printf(" listening\n", rssi);        
        this->onPeerPing(&message->header);

        // Execute the received command
        MeshId fromId = message->header.uplinkId;
        if (!fromId) fromId = message->header.id;

        this->receiver->onCommand(
            COMMAND_UPDATE,
            &message->header,
            &message->current
        );
        this->receiver->onCommand(
            COMMAND_NEXT,
            &message->header,
            &message->next
        );
    }

    void onPeerPing(MeshNodeHeader* node) {
        if (node->id == this->header.id) {
            Serial.println("Detected an ID conflict.");
            this->reset();
        }

        if (node->id > this->header.uplinkId && node->id > this->header.id) {
            Serial.printf("Following %03X:%03X\n",
                node->id,
                node->uplinkId
            );

            this->follow(node->id);
        }

        if (node->id == this->header.uplinkId) {
            this->uplinkTimer.start(UPLINK_TIMEOUT);
        }
    }

    void setup() {
        configure_ap();

        this->broadcastTimer.start(BROADCAST_RATE);
        this->status = NODE_STATUS_INIT;

        this->reset();
        this->onMeshChange();

        quickEspNow.onDataRcvd(onDataReceived);
        
        Serial.println("Node: ok");
    }

    void broadcast(TubeState &current, TubeState &next) {
        // Don't broadcast if not in broadcast mode
        if (this->status != NODE_STATUS_BROADCASTING)
            return;

        NodeMessage message = {
            .header = this->header,
            .current = current,
            .next = next,
        };

        auto err = quickEspNow.send (ESPNOW_BROADCAST_ADDRESS,
            (uint8_t*)&message, sizeof(message));
        if (err)
            Serial.printf(">> Broadcast error %d\n", err);

        if (this->is_following()) {
            this->broadcastTimer.snooze(REBROADCAST_RATE);
        } else {
            this->broadcastTimer.snooze(BROADCAST_RATE);
        }
    }

    void update(TubeState &current, TubeState &next) {
        // Don't do anything for the first second, to allow Wifi to settle
        if (millis() < 1000)
            return;

        // Check the last time we heard from the uplink node
        if (is_following() && this->uplinkTimer.ended()) {
            Serial.println("Uplink lost");
            this->follow(0);
        }

        if (this->meshChanged) {
            this->onMeshChange();
            this->meshChanged = false;
        }

        if (this->broadcastTimer.ended()) {
            if (WiFi.isConnected())
                this->onWifiConnect();
            else
                this->onWifiDisconnect();

            this->broadcast(current, next);
        }
    }

    void reset(MeshId id = 0) {
        if (id == 0)
            id = random(256, 4000);  // Leave room at bottom and top of 12 bits
        this->header.id = id;
        this->follow(0);
        this->meshChanged = true;
    }

    void follow(MeshId uplinkId) {
        // Update uplink ID
        if (this->header.uplinkId == uplinkId)
            return;

        // Following zero means you have no uplink
        this->header.uplinkId = uplinkId;
        this->meshChanged = true;
    }

    bool is_following() {
        return this->header.uplinkId != 0;
    }
};

LightNode* LightNode::instance = nullptr;

void onDataReceived (uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
    LightNode::instance->onPeerData(address, data, len, rssi, broadcast);
}
