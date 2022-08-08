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

// #define NODE_DEBUGGING
#define TESTING_NODE_ID 100

#define CURRENT_NODE_VERSION 1
#define BROADCAST_ADDR ESPNOW_BROADCAST_ADDRESS 

#define BROADCAST_RATE 2000       // Rate at which to broadcast state updates as leader
#define UPLINK_TIMEOUT 17000      // Time at which uplink is presumed lost
#define REBROADCAST_TIME 15000    // Time at which followers are presumed re-uplinked

typedef uint16_t MeshId;

typedef struct {
    MeshId id = 0;
    MeshId uplinkId = 0;
    uint8_t version = CURRENT_NODE_VERSION;
} MeshNodeHeader;

typedef struct {
    TubeState current;
    TubeState next;
} NodeUpdate;

typedef struct {
    MeshNodeHeader header;
    CommandId command;
    union {
        NodeUpdate update;
    } data;
} NodeMessage;

void onDataReceived (uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast);


class MessageReceiver {
  public:
    virtual void onCommand(CommandId command, void *data) {
      // Abstract: subclasses must define
    }  
};

typedef enum{
    NODE_STATUS_QUIET=0,
    NODE_STATUS_STARTING=1,
    NODE_STATUS_STARTED=2,
} NodeStatus;


class LightNode {
  public:
    static LightNode* instance;

    MessageReceiver *receiver;
    MeshNodeHeader header;
    NodeStatus status = NODE_STATUS_QUIET;

    bool meshStarted = false;

    char node_name[20];

    Timer uplinkTimer; // When this timer ends, assume uplink is lost.
    Timer broadcastTimer; // When this timer ends, send a status update
    Timer rebroadcastTimer; // Until this timer ends, re-broadcast messages from uplink

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
        if (this->meshStarted) {
            Serial.println("WiFi connected: stop broadcasting");
            quickEspNow.stop();
            this->meshStarted = false;
        }

        this->status = NODE_STATUS_QUIET;
    }

    void onWifiDisconnect() {
        if (!this->meshStarted) {
            Serial.println("WiFi disconnected: start broadcasting");
            WiFi.mode (WIFI_MODE_STA);
            WiFi.disconnect(false, true);
            quickEspNow.begin(1, WIFI_IF_STA);
            this->meshStarted = true;
        }

        if (this->status == NODE_STATUS_QUIET)
            this->status = NODE_STATUS_STARTING;
    }

    void onMeshChange() {
        sprintf(this->node_name,
            "Tube %03X:%03X",
            this->header.id,
            this->header.uplinkId
        );
    }

    void onPeerPing(MeshNodeHeader* node) {
        // When receiving a message, if the IDs match, it's a conflict
        // Reset to create a new ID.
        if (node->id == this->header.id) {
            Serial.println("Detected an ID conflict.");
            this->reset();
        }

        // If the message arrives from a higher ID, switch into follower mode
        if (node->id > this->header.uplinkId && node->id > this->header.id) {            
            if (this->header.id != TESTING_NODE_ID || node->id < 0x800)
                this->follow(node);
        }

        // If the message arrived from our uplink, track that we're still linked.
        if (node->id == this->header.uplinkId) {
            this->uplinkTimer.start(UPLINK_TIMEOUT);
        }

        // If a message indicates that another node is following this one,
        // enter or continue re-broadcasting mode (unless already LEAD)
        if (node->uplinkId == this->header.id) {
            Serial.printf("%03X/%03X is following me\n", node->id, node->uplinkId);
            this->rebroadcastTimer.start(REBROADCAST_TIME);
        }
    }

    void onPeerData(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
        // Ignore this message if it isn't a valid message payload.
        if (len != sizeof(NodeMessage))
            return;

        NodeMessage* message = (NodeMessage*)data;
#ifdef NODE_DEBUGGING        
        Serial.printf(">> Received %db ", len);
        Serial.printf("from %03X/%03X ", message->header.id, message->header.uplinkId);
        Serial.printf("at " MACSTR, MAC2STR(address));
        Serial.printf("@ %ddBm: ", rssi);
#endif

        // Ignore this message if it's the wrong version.
        if (message->header.version != this->header.version) {
#ifdef NODE_DEBUGGING
            Serial.printf(" <version\n");        
#endif
            return;
        }

        // Track that another node exists, updating this node's understanding of the mesh.
        this->onPeerPing(&message->header);

        // Ignore this message if not from my uplink
        if (message->header.id != this->header.uplinkId) {
#ifdef NODE_DEBUGGING            
            Serial.printf(" ignoring\n");        
#endif
            return;
        }

#ifdef NODE_DEBUGGING
        Serial.printf(" listening\n");        
#endif        

        // Execute the received command
        Serial.printf("From %03X/%03X: ", message->header.id, message->header.uplinkId);
        this->receiver->onCommand(
            message->command,
            &message->data
        );

        // Re-broadcast the message if appropriate
        if (!this->rebroadcastTimer.ended()) {
            message->header = this->header;
            this->broadcast(message, true);
        }
    }

    void broadcast(NodeMessage *message, bool is_rebroadcast=false) {
        // Don't broadcast anything if this node isn't active.
        if (this->status != NODE_STATUS_STARTED)
            return;
        
        auto err = quickEspNow.send(
            ESPNOW_BROADCAST_ADDRESS,
            (uint8_t*)message, sizeof(*message)
        );
        if (err)
            Serial.printf(">> Broadcast error %d\n", err);
    }

    void setup() {
        configure_ap();

#ifdef NODE_DEBUGGING
        this->reset(TESTING_NODE_ID);
#else
        this->reset();
#endif
        this->broadcastTimer.stop();

        quickEspNow.onDataRcvd(onDataReceived);
        
        Serial.println("Node: ok");
    }

    void set_timer() {
        // Timer in QUIET mode determines how often we'll check WiFi status
        if (this->status == NODE_STATUS_QUIET) {
            this->broadcastTimer.start(BROADCAST_RATE);
            this->rebroadcastTimer.stop();
            return;
        }

        // Initial timer: wait for a bit before trying to broadcast.
        // If this node's ID is high, it's more likely to be the leader, so wait less.
        if (this->status == NODE_STATUS_STARTING) {
            auto next_time = 4000 - this->header.id/2;
            this->broadcastTimer.start(next_time);
            this->rebroadcastTimer.start(REBROADCAST_TIME);
            return;
        }

        // If following, only rebroadcast every 5 cycles
        if (this->is_following()) {
            auto next_time = 5 * BROADCAST_RATE;

            // Randomize a bit so not everyone is rebroadcasting at the same time
            next_time += random(0, 2000) - 1000;

            this->broadcastTimer.start(next_time);
            return;
        }

        this->broadcastTimer.start(BROADCAST_RATE);
    }

    void update(TubeState &current, TubeState &next) {
        // Check the last time we heard from the uplink node
        if (is_following() && this->uplinkTimer.ended()) {
            this->follow(NULL);
        }

        if (this->broadcastTimer.ended()) {
            // The broadcast timer doubles as a timer for startup delay
            // Once the initial timer has ended, mark this node as started
            if (this->status == NODE_STATUS_STARTING)
                this->status = NODE_STATUS_STARTED;

            // Check WiFi status and update node status if wifi changed
            if (WiFi.isConnected())
                this->onWifiConnect();
            else
                this->onWifiDisconnect();

            // Set the next time and reset the rebroadcast monitoring
            this->update_status(current, next);
            this->set_timer();
        }
    }

    void update_status(TubeState &current, TubeState &next) {
        // Broadcast (or rebroadcast) the current state
        NodeMessage message = {
            .header = this->header,
            .command = COMMAND_UPDATE,
            .data = {
                .update = {
                    .current = current,
                    .next = next
                }
            }
        };
        this->broadcast(&message);
    }

    void reset(MeshId id = 0) {
        if (id == 0)
            id = random(256, 4000);  // Leave room at bottom and top of 12 bits
        this->header.id = id;
        this->status = NODE_STATUS_STARTING;
        this->follow(NULL);
        this->onMeshChange();
    }

    void follow(MeshNodeHeader* node) {
        if (node == NULL) {
            if (this->header.uplinkId != 0) {
                Serial.println("Uplink lost");
            }

            // Unfollow: following zero means you have no uplink
            this->header.uplinkId = 0;
            this->onMeshChange();
            return;
        }

        // Already following? ignore
        if (this->header.uplinkId == node->id)
            return;

        // Follow
        Serial.printf("Following %03X:%03X\n",
            node->id,
            node->uplinkId
        );
        this->header.uplinkId = node->id;
        this->onMeshChange();
    }

    bool is_following() {
        return this->header.uplinkId != 0;
    }
};

LightNode* LightNode::instance = nullptr;

void onDataReceived (uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
    LightNode::instance->onPeerData(address, data, len, rssi, broadcast);
}
