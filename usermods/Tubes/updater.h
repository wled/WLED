#pragma once

#include "wled.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <Update.h>
#include "timer.h"

#define RELEASE_VERSION 12

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

typedef enum UpdateWorkflowStatus: uint8_t {
  Idle=0,
  Ready=10,
  Started=50,
  Connected=60,
  Received=80,  
  Complete=100,
  Failed=101,
} UpdateWorkflowStatus;

typedef struct AutoUpdateOffer {
    int version = RELEASE_VERSION;
    char ssid[25] = "Fish Tank";
    char password[25] = "Fish Tank";
    IPAddress host = IPAddress(192,168,0,146);
} AutoUpdateOffer;

class AutoUpdater {
  public:
    AutoUpdateOffer current_version;

    // For now, hardcode it all
    String host_name = "brcac.com";
    String path = "/firmware.bin";
    int port = 80;

    long fileSize = 0;

    String _storedSSID = "";
    String _storedPass = "";
    String _storedAPSSID = "";
    String _storedAPPass = "";

    int progress = 0;

    WiFiClient _client;
    UpdateWorkflowStatus status = Idle;
    Timer timeoutTimer;
    Timer displayStatusTimer;

    void update() {
        switch (this->status) {
            case Complete:
                if (this->displayStatusTimer.ended()) {
                    doReboot = true;
                    this->status = Idle;
                }
                return;

            case Failed:
                if (this->displayStatusTimer.ended())
                    this->status = Idle;
                return;

            case Ready:
            case Idle:
            case Received:
                return;

            case Started:
                this->do_connect();
                return;

            case Connected:
                this->do_request();
                return;
        }
    }

    void start(AutoUpdateOffer *new_version = nullptr) {
        if (this->status != Idle) {
            log("update already in progress.");
            return;
        }

        if (new_version && new_version->version <= current_version.version) {
            log("don't need to update to that version.");
            return;
        }

        // The auto-update process might break the current connection
        _storedSSID = String(clientSSID);
        _storedPass = String(clientPass);
        _storedAPSSID = String(apSSID);
        _storedAPPass = String(apPass);
        otaLock = false;

        if (new_version) {
            memcpy((byte*)&this->current_version, new_version, sizeof(this->current_version));
        }

        log("starting autoupdate");
        this->status = Started;
        this->displayStatusTimer.stop();
    }

    void ready() {
        log("ready for update - turning on updater AP");
        strcpy(apSSID, "WLED-UPDATE");
        strcpy(apPass, "update1234");
        auto tmp = apBehavior;
        apBehavior = AP_BEHAVIOR_ALWAYS;
        WLED::instance().initAP();
        apBehavior = tmp;
        this->status = Ready;
    }

    void stop() {
        this->_client.stop();
        strcpy(clientSSID, _storedSSID.c_str());
        strcpy(clientPass, _storedPass.c_str());
        strcpy(apSSID, _storedAPSSID.c_str());
        strcpy(apPass, _storedAPPass.c_str());
        WiFi.softAPdisconnect(true);
        apActive = false;
        WiFi.disconnect(false, true);
        WLED::instance().enableWatchdog();
        this->status = Idle;
    }

    void handleOverlayDraw() {
        CRGB c;
        switch (this->status) {
            case Ready:
                // Once the updater connects, no need to show ready
                if (WiFi.softAPgetStationNum())
                    return;
                c = CRGB::Purple;
                break;

            case Started:
            case Connected:
            case Received:
                c = CRGB::Yellow;
                if (millis() % 1000 < 500) {
                    c = CRGB::Black;
                }
                break;

            case Failed:
                c = CRGB::Red;
                break;

            case Complete:
                c = CRGB::Green;
                break;

            case Idle:
            default:
                return;
        }
        for (int i = 0; i < 10; i++) {
            strip.setPixelColor(i, c);
        }
    } 
    
  private:
    void log(const char *message) {
        Serial.printf("OTA: %s\n", message);
    }

    void log(String message) {
        log(message.c_str());
    }

    void abort(const char *message) {
        log(message);
        this->stop();
        this->status = Failed;
        this->displayStatusTimer.start(30000);
    }

    void do_connect() {
        auto s = WiFi.status();
        switch (s) {
            case WL_DISCONNECTED:
                if (!strlen(clientSSID)) {
                    log("connecting to autoupdate server");
                    strcpy(clientSSID, this->current_version.ssid);
                    strcpy(clientPass, this->current_version.password);
                }
                return;

            case WL_NO_SSID_AVAIL:
                abort("Invalid auto-update SSID");
                return;

            case WL_CONNECT_FAILED:
                abort("Invalid auto-update password");
                return;

            case WL_CONNECTED:
                if (WiFi.SSID() != String(this->current_version.ssid)) {
                    log("disconnecting from WiFi");
                    WiFi.softAPdisconnect(true);
                    WiFi.disconnect(false, true);
                    apBehavior = AP_BEHAVIOR_BUTTON_ONLY;
                    return;
                }

                log("WiFi connected");
                this->status = Connected;
                return;

            case WL_IDLE_STATUS:
                EVERY_N_MILLIS(300) {
                    Serial.print("...");
                }
                break;

            default:
                Serial.printf("OTA: wifi %d", (int)s);
                break;
        }
    }

    void do_request() {
        WLED::instance().disableWatchdog();
        log("connecting");
        if (!this->_client.connect(this->host_name.c_str(), this->port)) { //  this->current_version.host
            abort("connect failed");
            return;
        }

        // Get the contents of the bin file
        log("requesting update package");
        auto request = String("GET ") + this->path + " HTTP/1.1\r\n" +
            "Host: " + this->host_name + "\r\n" +
            "Cache-Control: no-cache\r\n\r\n";
        Serial.println(request);
        this->_client.print(request);

        log("awaiting response");
        timeoutTimer.start(5000);
        while (!this->_client.available()) {
            vTaskDelay( 400 );
            if (timeoutTimer.ended()) {
                abort("timed out waiting for response");
                return;
            }
            log("waiting...");
        }

        String contentType = "";

        log("reading response");
        while (this->_client.available()) {
            // read line till /n - if the line is empty, it's the end of the headers.
            String line = this->_client.readStringUntil('\n');
            line.trim();
            if (!line.length()) break;

            // Check if the HTTP Response is 200
            if (line.startsWith("HTTP/")) {
                if (line.indexOf("200") < 0) {
                    abort(line.c_str());
                    return;
                }
            }

            // Read the file size from Content-Length
            if (line.startsWith("Content-Length: ")) {
                fileSize = atol((getHeaderValue(line, "Content-Length: ")).c_str());
                log(line.c_str());
            }

            // Read the content type from Content-Type
            if (line.startsWith("Content-Type: ")) {
                contentType = getHeaderValue(line, "Content-Type: ");
                log(line.c_str());
            }
            if (line.startsWith("Content-type: ")) {
                contentType = getHeaderValue(line, "Content-type: ");
                log(line.c_str());
            }

        }

        if (fileSize == 0 || contentType != "application/octet-stream") {
            abort("Must get a valid Content-Type and Content-Length header.");
            return;
        }

        log("found a valid OTA BIN");

        this->status = Received;
        this->do_update(this->_client);
    }

    void do_update(WiFiClient client) {
        if (!Update.begin(fileSize)) {
            abort("no room for the update");
            return;
        };

        WLED::instance().disableWatchdog();
        this->progress = 0;
        vTaskDelay(500);
        uint8_t buf[4096];
        int lr;
        while ((lr = client.read(buf, sizeof(buf))) > 0) {
            size_t written = Update.write(buf, lr);
            if (!written)
                break;

            this->progress += written;
            Serial.printf(" %d of %ld\n", this->progress, fileSize);

            // Give the server time to send some more data
            if (!client.available())
                vTaskDelay(100);
        }

        if (!Update.end()) {
            Serial.println("Error #: " + String(Update.getError()));
            abort("Error during streaming");
            return;
        }

        if (!Update.isFinished()) {
            abort("Error during finishing");
            return;
        }
        
        log("update successfully completed. Rebooting.");
        this->stop();
        this->status = Complete;
        this->displayStatusTimer.start(10000);
    }

};
