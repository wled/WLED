#pragma once

#include <Arduino.h>
#include "global_state.h"
#include "espnow_broadcast.h"

// #define NODE_DEBUGGING
// #define RELAY_DEBUGGING
#define TESTING_NODE_ID 0

#define CURRENT_NODE_VERSION 2

#pragma pack(push,4) // set packing for consist transport across network
// ideally this would have been pack 1, so we're actually wasting a
// number of bytes across the network, but we've already shipped...

typedef enum{
    RECIPIENTS_ALL=0,  // Send to all neighbors; non-followers will ignore
    RECIPIENTS_ROOT=1, // Send to root for rebroadcasting downward, all will see
    RECIPIENTS_INFO=2, // Send to all neighbors "FYI"; none will ignore
} MessageRecipients;

typedef uint16_t MeshId;

typedef struct {
    MeshId id = 0;
    MeshId uplinkId = 0;
    uint8_t version = CURRENT_NODE_VERSION;
} MeshNodeHeader;

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
        NODE_STATUS_RECEIVING,
        NODE_STATUS_STARTED,
        NODE_STATUS_MAX,        
    } NodeStatus;
    NodeStatus status = NODE_STATUS_QUIET;

    PGM_P status_code() {
        switch (status) {
        case NODE_STATUS_QUIET:
            return PSTR(" (quiet)");
        case NODE_STATUS_RECEIVING:
            return PSTR(" (receiving)");
        case NODE_STATUS_STARTED:
            return PSTR(" (started)");
        default:
            return PSTR("??");
        }
    }

    char node_name[20];

    LightNode(MessageReceiver *r) : receiver(r) {
        instance = this;
    }

  protected:

    const uint32_t STATUS_TIMEOUT_BASE =  3000;    // Base time to wait to send broadcasts
    const uint32_t UPLINK_TIMEOUT      = 20000;    // Time at which uplink is presumed lost
    const uint32_t REBROADCAST_TIME    = 30000;    // Time at which followers are presumed re-uplinked

    Timer statusTimer;      // Use this timer to initialize and check wifi status
    Timer uplinkTimer;      // When this timer ends, assume uplink is lost.
    Timer rebroadcastTimer; // Until this timer ends, re-broadcast messages from uplink

    void onMeshChange() {
        sprintf(node_name,
            "Tube %03X:%03X",
            header.id,
            header.uplinkId
        );

        configureAP();
    }

    void configureAP() {
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

    void onPeerPing(const MeshNodeHeader& node) {
        // When receiving a message, if the IDs match, it's a conflict
        // Reset to create a new ID.
        if (node.id == header.id) {
            Serial.println("Detected an ID conflict.");
            reset();
        }

        // If the message arrives from a higher ID, switch into follower mode
        if (node.id > header.uplinkId && node.id > header.id) {
#ifdef RELAY_DEBUGGING
          // When debugging relay, pretend not to see any nodes above 0x800
          if (node->id < 0x800)
#endif
            follow(&node);
        }

        // If the message arrived from our uplink, track that we're still linked.
        if (node.id == header.uplinkId) {
            uplinkTimer.start(UPLINK_TIMEOUT);
        }

        // If a message indicates that another node is following this one, or
        // should be (it's not following anything, but this node's ID is higher)
        // enter or continue re-broadcasting mode.
        if (node.uplinkId == header.id
            || (node.uplinkId == 0 && node.id < header.id)) {
            Serial.printf("        %03X/%03X is following me", node.id, node.uplinkId);
            rebroadcastTimer.start(REBROADCAST_TIME);
        }
    }

    void printMessage(const NodeMessage* message, signed int rssi) {
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

    void onPeerData(const uint8_t* address, const NodeMessage* message, uint8_t len, signed int rssi, bool broadcast) {
        // Ignore this message if it isn't a valid message payload.
        if (len != sizeof(*message))
            return;

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
        onPeerPing(message->header);

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
                const_cast<uint8_t*>(message->data)
            );
            Serial.println();

            if (!valid)
                return;
        }

        // Re-broadcast the message if appropriate
        if (!rebroadcastTimer.ended() && message->recipients != RECIPIENTS_INFO) {
            static NodeMessage msg;
            memcpy(&msg, &message, len);
            msg.header = header;
            if (!isFollowing()) {
                msg.recipients = RECIPIENTS_ALL;
            }
#ifdef NODE_DEBUGGING
            Serial.println("rebroadcast");
#endif
            broadcastMessage(&msg, true);
        }
    }

    void broadcastMessage(NodeMessage *message, bool is_rebroadcast=false) {
        // Don't broadcast anything if this node isn't active.
#ifdef NODE_DEBUGGING
        Serial.printf("broadcastMessage() - %s %s\n", status_code(), statusTimer.ended() ? "True" : "False");
#endif

        if (status != NODE_STATUS_STARTED) {
            if (status == NODE_STATUS_RECEIVING && statusTimer.ended()) {
                status = NODE_STATUS_STARTED;
                statusTimer.stop();
                Serial.printf("LightNode %s\n", status_code());
            } else {
                Serial.printf("broadcastMessage() - not started - %s\n", status_code());
                return;
            }
        }
        message->timebase = strip.timebase + millis();
        
#ifdef NODE_DEBUGGING
        Serial.print("  <<< ");
        printMessage(message, 0);
        Serial.println();
#endif

        __attribute__((unused)) auto success = espnowBroadcast.send((const uint8_t*)message, sizeof(*message));
#ifdef NODE_DEBUGGING
        if (!success) {
            Serial.println("espnowBroadcast.send() failed!");
        } else {
            Serial.println("successful broadcast");
        }
#endif

    }

  public:

    void sendCommand(CommandId command, void *data, uint8_t len) {
        // if (!ESP_NOW.isStarted()) {
        //     Serial.println("SendCommand ESP Not Started!");
        //     return;
        // }
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
#ifdef NODE_DEBUGGING
        Serial.println("sendCommand");
#endif        
        broadcastMessage(&message);
    }

    void setup() {
#ifdef NODE_DEBUGGING
        reset(TESTING_NODE_ID);
#else
        reset();
#endif


#ifdef NODE_DEBUGGING
        delay(2000);
#endif

        espnowBroadcast.registerCallback(onEspNowMessage);

        Serial.println("setup: ok");
    }

    void update() {

        //process any wifi events to turn on/off ESPNode
        checkESPNowState();

        // Check the last time we heard from the uplink node
        if (isFollowing() && uplinkTimer.ended()) {
            follow(NULL);
        }

    }

    void reset(MeshId id = 0) {
        if (id == 0) {
            id = random(256, 4000);  // Leave room at bottom and top of 12 bits
        }
        header.id = id;
        follow(NULL);
    }

    void follow(const MeshNodeHeader* node) {
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

protected:

    void checkESPNowState() {
        auto state = espnowBroadcast.getState();
        static auto prev = espnowBroadcast.STOPPED;
        switch(state) {
            case ESPNOWBroadcast::STOPPED:
                if (NODE_STATUS_QUIET != status) {
                    Serial.printf("checkESPNowState() - %d node_status:%s\n", state, status_code());
                    status = NODE_STATUS_QUIET;
                    rebroadcastTimer.stop();
                    Serial.printf("LightNode %s\n", status_code());
                }
                break;
            case ESPNOWBroadcast::STARTING: {}
                if ( state != prev ) {
                    Serial.printf("checkESPNowState() - %d node_status:%s\n", state, status_code());
                }
                break;
            case ESPNOWBroadcast::STARTED:
                if (NODE_STATUS_QUIET == status) {
                    Serial.printf("checkESPNowState() - %d node_status:%s\n", state, status_code());
                    status = NODE_STATUS_RECEIVING;
                    statusTimer.start(STATUS_TIMEOUT_BASE - header.id / 2);
                    Serial.printf("LightNode %s\n", status_code());
                }
                break;
            default:
                break;
        }
        prev = state;
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

    void onWizmote(const uint8_t* address, const wizmote_message* data, uint8_t len) {
        // First make sure this is a WizMote message.
        if (len != sizeof(wizmote_message) || data->byte8 != 1 || data->byte9 != 100 || data->byte5 != 32)
            return;

        static uint32_t last_seq = 0;
        uint32_t cur_seq = data->seq[0] | (data->seq[1] << 8) | (data->seq[2] << 16) | (data->seq[3] << 24);
        if (cur_seq == last_seq)
            return;
        last_seq = cur_seq;

        receiver->onButton(data->button);
    }

    static void onEspNowMessage(const uint8_t *address, const uint8_t *msg, uint8_t len, int8_t rssi) {
        if (msg) {
            if(len == sizeof(NodeMessage)) {
                instance->onPeerData(address, (const NodeMessage*)msg, len, rssi, true);
                instance->onWizmote(address, (const wizmote_message*)msg, len);
            } else {
#ifdef NODE_DEBUGGING
                Serial.printf("wrong size QueueNodeMessage received %d\n", len);
#endif
            }
        }
    }

};

LightNode* LightNode::instance = nullptr;

