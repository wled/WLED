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

#define UPDATE_RATE 3000      // Rate at which uplink is queried for data
#define UPLINK_TIMEOUT 10000  // Time at which uplink is presumed lost

typedef uint16_t MeshId;

typedef struct {
    MeshId id = 0;
    MeshId uplinkId = 0;
    uint8_t version = CURRENT_NODE_VERSION;
} MeshNodeHeader;


void onDataSent (uint8_t* address, uint8_t status) {
    Serial.printf (">> Message sent to " MACSTR ", status: %d\n", MAC2STR (address), status);
}

void onDataReceived (uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
    Serial.printf (">> Received %d bytes ", len);
    Serial.printf ("\"%.*s\" ", len, data);
    Serial.printf ("%s", broadcast ? "broadcast" : "unicast");
    Serial.printf ("@ %d dBm  ", rssi);
    Serial.printf ("from " MACSTR "\n" , MAC2STR(address));
}


class MessageReceiver {
  public:

  virtual void onCommand(MeshId fromId, CommandId command, void *data) {
    // Abstract: subclasses must define
  }
};


#define NODE_STATUS_INIT 0
#define NODE_STATUS_BROADCASTING 1
#define NODE_STATUS_QUIET 2


class LightNode {
  public:
    MeshNodeHeader header;
    char node_name[20];

    uint8_t status = NODE_STATUS_INIT;
    bool meshChanged = false;

    Timer uplinkTimer;
    Timer updateTimer;

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

    void onUplinkAlive() {
        // Track the last time we received a message from our uplink
        this->uplinkTimer.start(UPLINK_TIMEOUT);
    }

    void setup() {
        configure_ap();

        this->updateTimer.start(UPDATE_RATE);
        this->status = NODE_STATUS_INIT;

        this->reset();
        this->onMeshChange();

        quickEspNow.onDataRcvd(onDataReceived);
        quickEspNow.onDataSent(onDataSent);
        
        Serial.println("Node: ok");
    }

    void broadcast() {
        if (this->status != NODE_STATUS_BROADCASTING) {
            Serial.println(">> BC NO");
            return;
        }

        static unsigned int counter = 0;
        static const String msg = "Hello! ";
        
        String message = String (msg) + " " + String (counter++);
        // WiFi.disconnect (false, true);
        if (!quickEspNow.send (ESPNOW_BROADCAST_ADDRESS, (uint8_t*)message.c_str (), message.length ())) {
            Serial.printf (">>>>>>>>>> Message sent: %s\n", message.c_str ());
        } else {
            Serial.printf (">>>>>>>>>> Message not sent\n");
        }
    }

    void update() {
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

        if (this->updateTimer.ended()) {
            if (WiFi.isConnected())
                this->onWifiConnect();
            else
                this->onWifiDisconnect();

            this->broadcast();
            this->updateTimer.snooze(UPDATE_RATE);
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