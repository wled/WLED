#pragma once

#include "wled.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <Update.h>
#include "timer.h"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

typedef enum UpdateWorkflowStatus: uint8_t {
  Idle=0,
  Started=1,
  Connected=2,
  Received=4,  
  Complete=100,
  Failed=101,
} UpdateWorkflowStatus;

class AutoUpdater {
  public:
    String autoUpdateSSID = "<SSID>>";
    String autoUpdatePass = "<Password>>";
    String host = "<host IP>";
    String bin = "/firmware.bin";
    int port = 8000;
    long fileSize = 0;

    String _storedSSID = "";
    String _storedPass = "";

    int progress = 0;

    WiFiClient _client;
    UpdateWorkflowStatus status = Idle;
    Timer timeoutTimer;
    Timer progressTimer;

    void update() {
        switch (this->status) {
            case Complete:
            case Failed:
                if (this->progressTimer.ended())
                    this->status = Idle;
            case Idle:
                return;

            case Started:
                this->do_connect();
                return;

            case Connected:
                this->do_request(this->_client);
                return;
        }
    }

    void start() {
        if (this->status != Idle) {
            log("update already in progress.");
            return;
        }

        // The auto-update process might break the current connection
        _storedSSID = String(clientSSID);
        _storedPass = String(clientPass);

        WLED::instance().disableWatchdog();
        
        log("starting autoupdate");
        this->status = Started;
    }

    void stop() {
        this->_client.stop();

        strcpy(clientSSID, _storedSSID.c_str());
        strcpy(clientPass, _storedPass.c_str());
        WLED::instance().enableWatchdog();

        this->status = Idle;
    }

  private:
    void log(const char *message) {
        Serial.printf("OTA: %s\n", message);
    }

    void abort(const char *message) {
        log(message);
        this->status = Failed;
        this->progressTimer.start(10000);
        this->stop();
    }

    void do_connect() {
        auto s = WiFi.status();
        switch (s) {
            case WL_DISCONNECTED:
                log("connecting to autoupdate server");
                strcpy(clientSSID, autoUpdateSSID.c_str());
                strcpy(clientPass, autoUpdatePass.c_str());
                return;

            case WL_NO_SSID_AVAIL:
                abort("Invalid auto-update SSID");
                return;

            case WL_CONNECT_FAILED:
                abort("Invalid auto-update password");
                return;

            case WL_CONNECTED:
                if (WiFi.SSID() != autoUpdateSSID) {
                    log("disconnecting from WiFi");
                    WiFi.disconnect(true);
                    apBehavior = AP_BEHAVIOR_BUTTON_ONLY;
                    return;
                }

                log("WiFi connected");
                this->status = Connected;
                return;

            default:
                Serial.printf("OTA: wifi %d", (int)s);
                break;
        }
    }

    void do_request(WiFiClient client) {
        if (!client.connect(host.c_str(), port)) {
            log("connect failed");
            this->stop();
            return;
        }
        log("connected");

        // Get the contents of the bin file
        client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
            "Host: " + host + "\r\n" +
            "Cache-Control: no-cache\r\n\r\n");

        timeoutTimer.start(5000);
        while (!client.available()) {
            vTaskDelay( 200 );
            if (timeoutTimer.ended()) {
                abort("timed out waiting for response");
                return;
            }
        }

        String contentType = "";

        while (client.available()) {
            // read line till /n - if the line is empty, it's the end of the headers.
            String line = client.readStringUntil('\n');
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
        this->do_update(client);
    }

    void do_update(WiFiClient client) {
        if (!Update.begin(fileSize)) {
            abort("no room for the update");
            return;
        };

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
        doReboot = true;
        this->status = Complete;
        this->progressTimer.start(10000);
        this->stop();
    }
};
