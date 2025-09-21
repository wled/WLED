//  WLED OTA update interface

#include <Arduino.h>

#pragma once

class AsyncWebServerRequest;

// Start a new OTA session.
void beginOTA(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal);

// Handle a block of OTA data from an AsyncWebServerRequest.
void handleOTAData(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool isFinal);
