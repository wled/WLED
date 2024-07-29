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
// #define RELAY_DEBUGGING
#define TESTING_NODE_ID 0

#define CURRENT_NODE_VERSION 2
#define BROADCAST_ADDR ESPNOW_BROADCAST_ADDRESS 

#define UPLINK_TIMEOUT 20000      // Time at which uplink is presumed lost
#define REBROADCAST_TIME 30000    // Time at which followers are presumed re-uplinked
#define WIFI_CHECK_RATE 2000     // Time at which we should check wifi status again

#pragma pack(push,4) // set packing for consist transport across network
// ideally this would have been pack 1, so we're actually wasting a
// number of bytes across the network, but we've already shipped...

typedef uint16_t MeshId;

typedef struct {
    MeshId id = 0;
    MeshId uplinkId = 0;
    uint8_t version = CURRENT_NODE_VERSION;
} MeshNodeHeader;

typedef enum{
    RECIPIENTS_ALL=0,  // Send to all neighbors; non-followers will ignore
    RECIPIENTS_ROOT=1, // Send to root for rebroadcasting downward, all will see
    RECIPIENTS_INFO=2, // Send to all neighbors "FYI"; none will ignore
} MessageRecipients;

#define MESSAGE_DATA_SIZE 64

typedef struct {
    MeshNodeHeader header;
    MessageRecipients recipients;
    uint32_t timebase;
    CommandId command;
    byte data[MESSAGE_DATA_SIZE] = {0};
} NodeMessage;

#pragma pack(pop)

typedef struct {
    uint8_t status;
    char message[40];
} NodeInfo;

const char *command_name(CommandId command) {
    switch (command) {
        case COMMAND_STATE:
            return "UPDATE";
        case COMMAND_OPTIONS:
            return "OPTIONS";
        case COMMAND_ACTION:
            return "ACTION";
        case COMMAND_INFO:
            return "INFO";
        case COMMAND_BEATS:
            return "BEATS";
        default:
            return "?COMMAND?";
    }
}

class MessageReceiver {
  public:
    virtual bool onCommand(CommandId command, void *data) {
      // Abstract: subclasses must define
      return false;
    }  
    virtual bool onButton(uint8_t button_id) {
      // Abstract: subclasses must define
      return false;
    }  
};

class LightNode {
  public:
    static LightNode* instance;

    MessageReceiver *receiver;
    MeshNodeHeader header;

    typedef enum{
        NODE_STATUS_QUIET=0,
        NODE_STATUS_STARTING=1,
        NODE_STATUS_STARTED=2,
    } NodeStatus;
    NodeStatus status = NODE_STATUS_QUIET;

    PGM_P status_code() {
        switch (status) {
        case NODE_STATUS_QUIET:
            return PSTR(" (quiet)");
        case NODE_STATUS_STARTING:
            return PSTR(" (starting)");
        case NODE_STATUS_STARTED:
            return PSTR("");
        default:
            return PSTR("??");
        }
    }

    char node_name[20];

    Timer statusTimer;  // Use this timer to initialize and check wifi status
    Timer uplinkTimer; // When this timer ends, assume uplink is lost.
    Timer rebroadcastTimer; // Until this timer ends, re-broadcast messages from uplink

    LightNode(MessageReceiver *r) : receiver(r) {
        LightNode::instance = this;
    }

    void onWifiConnect() {
        if (status == NODE_STATUS_QUIET)
            return;
            
        Serial.println("WiFi connected: stop broadcasting");
        quickEspNow.stop();
        status = NODE_STATUS_QUIET;
        rebroadcastTimer.stop();
        statusTimer.start(WIFI_CHECK_RATE);
    }

    void onWifiDisconnect() {
        if (status != NODE_STATUS_QUIET)
            return;

        Serial.println("WiFi disconnected: start broadcasting");
        WiFi.mode (WIFI_MODE_STA);
        WiFi.disconnect(false, true);
        quickEspNow.begin(1, WIFI_IF_STA);
        start();
    }

    void onMeshChange() {
        sprintf(node_name,
            "Tube %03X:%03X",
            header.id,
            header.uplinkId
        );
        configuredAP();
    }

    void configuredAP() {
#ifdef DEFAULT_WIFI
        strcpy(clientSSID, DEFAULT_WIFI);
        strcpy(clientPass, DEFAULT_WIFI_PASSWORD);
#else
        // Don't connect to any networks.
        strcpy(clientSSID, "");
        strcpy(clientPass, "");
#endif

        // By default, we don't want these visible.
        apBehavior = AP_BEHAVIOR_BUTTON_ONLY; // Must press button for 6 seconds to get AP
    }

    void start() {
        // Initialization timer: wait for a bit before trying to broadcast.
        // If this node's ID is high, it's more likely to be the leader, so wait less.
        status = NODE_STATUS_STARTING;
        statusTimer.start(3000 - header.id / 2);
        rebroadcastTimer.stop();
    }

    void onPeerPing(MeshNodeHeader* node) {
        // When receiving a message, if the IDs match, it's a conflict
        // Reset to create a new ID.
        if (node->id == header.id) {
            Serial.println("Detected an ID conflict.");
            reset();
        }

        // If the message arrives from a higher ID, switch into follower mode
        if (node->id > header.uplinkId && node->id > header.id) {            
#ifdef RELAY_DEBUGGING
          // When debugging relay, pretend not to see any nodes above 0x800
          if (node->id < 0x800)
#endif
            follow(node);
        }

        // If the message arrived from our uplink, track that we're still linked.
        if (node->id == header.uplinkId) {
            uplinkTimer.start(UPLINK_TIMEOUT);
        }

        // If a message indicates that another node is following this one, or
        // should be (it's not following anything, but this node's ID is higher)
        // enter or continue re-broadcasting mode.
        if (node->uplinkId == header.id
            || (node->uplinkId == 0 && node->id < header.id)) {
            Serial.printf("        %03X/%03X is following me\n", node->id, node->uplinkId);
            rebroadcastTimer.start(REBROADCAST_TIME);
        }
    }

    void printMessage(NodeMessage* message, signed int rssi) {
        Serial.printf("%03X/%03X %s",
            message->header.id,
            message->header.uplinkId,
            command_name(message->command)
        );
        if (message->recipients == RECIPIENTS_ROOT)
            Serial.printf(":ROOT");
        if (rssi)
            Serial.printf(" %ddB ", rssi);
    }

    void onPeerData(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
        // Ignore this message if it isn't a valid message payload.
        if (len != sizeof(NodeMessage))
            return;

        NodeMessage* message = (NodeMessage*)data;
        // Ignore this message if it's the wrong version.
        if (message->header.version != header.version) {
#ifdef NODE_DEBUGGING
            Serial.print("  -- !version ");
            printMessage(message, rssi);
            Serial.println();
#endif
            return;
        }

        // Track that another node exists, updating this node's understanding of the mesh.
        onPeerPing(&message->header);

        bool ignore = false;
        switch (message->recipients) {
            case RECIPIENTS_ALL:
                // Ignore this message if not from the uplink
                ignore = (message->header.id != header.uplinkId);
                break;

            case RECIPIENTS_ROOT:
                // Ignore this message if not from one of this node's downlinks
                ignore = (message->header.uplinkId != header.id);
                break;

            case RECIPIENTS_INFO:
                ignore = false;
                break;

            default:
                // ignore this!
                ignore = true;
                break;
        }

        if (ignore) {
#ifdef NODE_DEBUGGING
            Serial.print("  -- ignored ");
            printMessage(message, rssi);
            Serial.println();
#endif
            return;
        }

        // Execute the received command
        if (message->recipients != RECIPIENTS_ROOT || !isFollowing()) {
            Serial.print("  >> ");
            printMessage(message, rssi);
            Serial.print(" ");

            // Adjust the timebase to match uplink
            // But only if it's drifting, else animations get jittery
            uint32_t new_timebase = message->timebase - millis() + 3; // Factor for network delay
            int32_t diff = new_timebase - strip.timebase;
            if (diff < -10 || diff > 10)
                strip.timebase = new_timebase;

            // Execute the command
            auto valid = receiver->onCommand(
                message->command,
                &message->data
            );
            Serial.println();

            if (!valid)
                return;
        }

        // Re-broadcast the message if appropriate
        if (!rebroadcastTimer.ended() && message->recipients != RECIPIENTS_INFO) {
            message->header = header;
            if (!isFollowing())
                message->recipients = RECIPIENTS_ALL;
            broadcastMessage(message, true);
        }
    }

    void broadcastMessage(NodeMessage *message, bool is_rebroadcast=false) {
        // Don't broadcast anything if this node isn't active.
        if (status != NODE_STATUS_STARTED)
            return;
        message->timebase = strip.timebase + millis();
        
#ifdef NODE_DEBUGGING
        Serial.print("  <<< ");
        printMessage(message, 0);
        Serial.println();
#endif

        auto err = quickEspNow.send(
            ESPNOW_BROADCAST_ADDRESS,
            (uint8_t*)message, sizeof(*message)
        );
        if (err)
            Serial.printf("  *** Broadcast error %d\n", err);
    }

    void sendCommand(CommandId command, void *data, uint8_t len) {
        if (len > MESSAGE_DATA_SIZE) {
            Serial.printf("Message is too big: %d vs %d\n",
                len, MESSAGE_DATA_SIZE);
            return;
        }

        NodeMessage message;
        message.header = header;
        if (command == COMMAND_INFO) {
            message.recipients = RECIPIENTS_INFO;
        } else if (command == COMMAND_STATE) {
            message.recipients = RECIPIENTS_ALL;
        } else if (isFollowing()) {
            // Follower nodes must request that the root re-sends this message
            message.recipients = RECIPIENTS_ROOT;
        } else {
            message.recipients = RECIPIENTS_ALL;
        }
        message.command = command;
        memcpy(&message.data, data, len);
        broadcastMessage(&message);
    }

    void setup() {
#ifdef NODE_DEBUGGING
        reset(TESTING_NODE_ID);
#else
        reset();
#endif
        statusTimer.stop();
        quickEspNow.onDataRcvd(onDataReceived);

        Serial.println("Mesh: ok");
    }

    void update() {
        // Check the last time we heard from the uplink node
        if (isFollowing() && uplinkTimer.ended()) {
            follow(NULL);
        }

        if (statusTimer.every(WIFI_CHECK_RATE)) {
            // The broadcast timer doubles as a timer for startup delay
            // Once the initial timer has ended, mark this node as started
            if (status == NODE_STATUS_STARTING)
                status = NODE_STATUS_STARTED;

            // Check WiFi status and update node status if wifi changed
            if (WiFi.isConnected())
                onWifiConnect();
            else
                onWifiDisconnect();
        }
    }

    void reset(MeshId id = 0) {
        if (id == 0) {
            id = random(256, 4000);  // Leave room at bottom and top of 12 bits
        }
        header.id = id;
        follow(NULL);
    }

    void follow(MeshNodeHeader* node) {
        if (node == NULL) {
            if (header.uplinkId != 0) {
                Serial.println("Uplink lost");
            }

            // Unfollow: following zero means you have no uplink
            header.uplinkId = 0;
            onMeshChange();
            return;
        }

        // Already following? ignore
        if (header.uplinkId == node->id)
            return;

        // Follow
        Serial.printf("Following %03X:%03X\n",
            node->id,
            node->uplinkId
        );
        header.uplinkId = node->id;
        onMeshChange();
    }

    bool isFollowing() {
        return header.uplinkId != 0;
    }

    typedef struct wizmote_message {
    uint8_t program;      // 0x91 for ON button, 0x81 for all others
    uint8_t seq[4];       // Incremetal sequence number 32 bit unsigned integer LSB first
    uint8_t byte5 = 32;   // Unknown
    uint8_t button;       // Identifies which button is being pressed
    uint8_t byte8 = 1;    // Unknown, but always 0x01
    uint8_t byte9 = 100;  // Unnkown, but always 0x64

    uint8_t byte10;  // Unknown, maybe checksum
    uint8_t byte11;  // Unknown, maybe checksum
    uint8_t byte12;  // Unknown, maybe checksum
    uint8_t byte13;  // Unknown, maybe checksum
    } wizmote_message;

    static void onWizmote(uint8_t* address, wizmote_message* data, uint8_t len) {
        // First make sure this is a WizMote message.
        if (len != sizeof(wizmote_message) || data->byte8 != 1 || data->byte9 != 100 || data->byte5 != 32)
            return;

        static uint32_t last_seq = 0;
        uint32_t cur_seq = data->seq[0] | (data->seq[1] << 8) | (data->seq[2] << 16) | (data->seq[3] << 24);
        if (cur_seq == last_seq)
            return;
        last_seq = cur_seq;

        instance->receiver->onButton(data->button);
    }

    static void onDataReceived(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
        instance->onPeerData(address, data, len, rssi, broadcast);
        onWizmote(address, (wizmote_message*)data, len);
    }
};

LightNode* LightNode::instance = nullptr;

