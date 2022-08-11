#pragma once

#include <WiFi.h>
#include <WiFiClient.h>
#include <Update.h>

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

class WifiUpdater {
  public:
    String host = "kwater.kelectronics.net";
    String bin = "/api/getfirmware/firmwareLarge.bin";
    int port = 80;

    void web_update() {
        WiFiClient client;
        long fileSize = 0;
        String contentType = "";

        if (!WiFi.isConnected()) {
            Serial.println("Not on WiFi");
            return;
        }

        Serial.println("Connecting");
        if (!client.connect(host.c_str(), port)) {
            Serial.println("Connect failed");
            client.stop();
            return;
        }
        // Get the contents of the bin file
        client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
            "Host: " + host + "\r\n" +
            "Cache-Control: no-cache\r\n" +
            "Connection: close\r\n\r\n");

        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                Serial.println("Client Timeout !");
                client.stop();
                return;
            }
        }

        Serial.println("Reading headers");
        while (client.available()) {
            // read line till /n - if the line is empty, it's the end of the headers.
            String line = client.readStringUntil('\n');
            line.trim();
            if (!line.length()) break;

            // Check if the HTTP Response is 200
            if (line.startsWith("HTTP/1.1")) {
                if (line.indexOf("200") < 0) {
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    client.flush();
                    return;
                }
            }

            // Read the file size from Content-Length
            if (line.startsWith("Content-Length: ")) {
                fileSize = atol((getHeaderValue(line, "Content-Length: ")).c_str());
                Serial.println(line);
            }

            // Read the content type from Content-Type
            if (line.startsWith("Content-Type: ")) {
                String contentType = getHeaderValue(line, "Content-Type: ");
                Serial.println(line);
            }
        }

        if (fileSize == 0 || contentType != "application/octet-stream") {
            Serial.println("Must get a valid Content-Type and Content-Length header.");
            client.flush();
            return;
        }
      
        Serial.println("Beginning update");
        if (!Update.begin(fileSize)) {
            Serial.println("Cannot do the update");
            return;
        };
        Update.writeStream(client);
        if (!Update.end()) {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
        }

        if (Update.isFinished()) {
            Serial.println("Update successfully completed. Rebooting.");
            ESP.restart();
        } else {
            Serial.println("Update not finished? Something went wrong!");
        }

        client.flush();
        return;
    }
};