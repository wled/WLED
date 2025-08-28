#include "wled.h"

#include "rest_json_client.h"

RestJsonClient::RestJsonClient()
  : lastFetchMs_(static_cast<unsigned long>(-static_cast<long>(RATE_LIMIT_MS)))
  , doc_(MAX_JSON_SIZE) {
}

void RestJsonClient::resetRateLimit() {
  // pretend we fetched RATE_LIMIT_MS ago (allow immediate next call)
  lastFetchMs_ = millis() - RATE_LIMIT_MS;
}

DynamicJsonDocument* RestJsonClient::getJson(const char* url) {
  // enforce a basic rate limit to prevent runaway software from making bursts
  // of API calls (looks like DoS and get's our API key turned off ...)
  unsigned long now_ms = millis();
  // compute elapsed using unsigned arithmetic to avoid signed underflow
  unsigned long elapsed = now_ms - lastFetchMs_;
  if (elapsed < RATE_LIMIT_MS) {
    unsigned long remaining = RATE_LIMIT_MS - elapsed;
    DEBUG_PRINTF("SkyStrip: RestJsonClient::getJson: RATE LIMITED (%lu ms remaining)\n", remaining);
    return nullptr;
  }
  lastFetchMs_ = now_ms;

  // Determine whether to use HTTP or HTTPS based on URL scheme
  bool is_https = (strncmp(url, "https://", 8) == 0);
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  WiFiClient* client = nullptr;
  if (is_https) {
    secureClient.setInsecure();
    client = &secureClient;
  } else {
    client = &plainClient;
  }

  // Begin request
  if (!http_.begin(*client, url)) {
    http_.end();
    DEBUG_PRINTLN(F("SkyStrip: RestJsonClient::getJson: trouble initiating request"));
    return nullptr;
  }
  DEBUG_PRINTF("SkyStrip: RestJsonClient::getJson: free heap before GET: %u\n", ESP.getFreeHeap());
  int code = http_.GET();
  // Treat network errors (<=0) and non-2xx statuses as failures.
  // Optionally consider 204 (No Content) as failure since there is no body to parse.
  if (code <= 0 || code < 200 || code >= 300 || code == 204) {
    http_.end();
    DEBUG_PRINTF("SkyStrip: RestJsonClient::getJson: HTTP error/status: %d\n", code);
    return nullptr;
  }

  int len = http_.getSize();
  DEBUG_PRINTF("SkyStrip: RestJsonClient::getJson: expecting up to %d bytes, free heap before deserialization: %u\n", len, ESP.getFreeHeap());
  doc_.clear();
  auto err = deserializeJson(doc_, http_.getStream());
  http_.end();
  if (err) {
    DEBUG_PRINTF("SkyStrip: RestJsonClient::getJson: deserialization error: %s; free heap: %u\n", err.c_str(), ESP.getFreeHeap());
    return nullptr;
  }
  return &doc_;
}
