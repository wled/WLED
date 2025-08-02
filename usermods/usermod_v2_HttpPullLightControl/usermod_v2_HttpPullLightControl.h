#pragma once
/*
 * Usermod: HttpPullLightControl
 * Versie: 0.0.4
 * Repository: https://github.com/roelbroersma/WLED-usermodv2_HttpPullLightControl
 * Author: Roel Broersma
 * Website: https://www.roelbroersma.nl
 * Github author: github.com/roelbroersma
 * Description: This usermod for WLED will request a given URL to know which effects
 *              or individual lights it should turn on/off. So you can remote control a WLED
 *              installation without having access to it (if no port forward, vpn or public IP is available).
 * Use Case: Create a WLED 'Ring of Thought' christmas card. Sent a LED ring with 60 LEDs to 60 friends.
 *           When they turn it on and put it at their WiFi, it will contact your server. Now you can reply with a given
 *           number of lights that should turn on. Each light is a friend who did contact your server in the past 5 minutes.
 *           So on each of your friends LED rings, the number of lights will be the number of friends who have it turned on.
 * Features: It sends a unique ID (has of MAC and salt) to the URL, so you can define each client without a need to map their IP address.
 * Tested: Tested on WLED v0.14 with ESP32-S3 (M5Stack Atom S3 Lite), but should also workd for other ESPs and ESP8266.
 */

#include "wled.h"

// Use the following for SHA1 computation of our HASH, unfortunatelly PlatformIO doesnt recognize Hash.h while its already in the Core.
// We use Hash.h for ESP8266 (in the core) and mbedtls/sha256.h for ESP32 (in the core).
#ifdef ESP8266
  #include <Hash.h>
#endif
#ifdef ESP32
  #include "mbedtls/sha1.h"
#endif

#define HTTP_PULL_LIGHT_CONTROL_VERSION "0.0.4"

class HttpPullLightControl : public Usermod {
private:
  static const char _name[];
  static const char _enabled[];
  static const char _salt[];
  static const char _url[];

  bool enabled = true;

  #ifdef HTTP_PULL_LIGHT_CONTROL_INTERVAL
    uint16_t checkInterval = HTTP_PULL_LIGHT_CONTROL_INTERVAL;
  #else
    uint16_t checkInterval = 60;  // Default interval of 1 minute
  #endif

  #ifdef HTTP_PULL_LIGHT_CONTROL_URL
    String url = HTTP_PULL_LIGHT_CONTROL_URL;
  #else
    String url = "http://example.org/example.php";  // Default-URL (http only!), can also be url with IP address in it. HttpS urls are not supported (yet) because of AsyncTCP library
  #endif

  #ifdef HTTP_PULL_LIGHT_CONTROL_SALT
    String salt = HTTP_PULL_LIGHT_CONTROL_SALT;
  #else
    String salt = "1just_a_very-secret_salt2";  // Salt for generating a unique ID when requesting the URL (in this way you can give different answers based on the WLED device who does the request)
  #endif
  // NOTE THAT THERE IS ALSO A #ifdef HTTP_PULL_LIGHT_CONTROL_HIDE_URL and a HTTP_PULL_LIGHT_CONTROL_HIDE_SALT IF YOU DO NOT WANT TO SHOW THE OPTIONS IN THE USERMOD SETTINGS

  // Define constants
  static const uint8_t myLockId = USERMOD_ID_HTTP_PULL_LIGHT_CONTROL ; // Used for the requestJSONBufferLock(id) function
  static const int16_t ackTimeout = 9000;  // ACK timeout in milliseconds when doing the URL request
  static const uint16_t rxTimeout = 9000;  // RX timeout in milliseconds when doing the URL request
  static const unsigned long FNV_offset_basis = 2166136261;
  static const unsigned long FNV_prime = 16777619;
  static const unsigned long inactivityTimeout = 30000; // When the AsyncClient is inactive (hanging) for this many milliseconds, we kill it

  unsigned long lastCheck = 0;    // Timestamp of last check
  unsigned long lastActivityTime = 0; // Time of last activity of AsyncClient
  String host;                    // Host extracted from the URL
  String path;                    // Path extracted from the URL
  String uniqueId;                // Cached unique ID
  AsyncClient *client = nullptr;  // Used very often, beware of closing and freeing
  String generateUniqueId();

  void parseUrl();
  void updateSalt(String newSalt);  // Update the salt value and recalculate the unique ID
  void checkUrl();                  // Check the specified URL for light control instructions
  void handleResponse(String& response);
  void onClientConnect(AsyncClient *c);

public:
  void setup();
  void loop();
  bool readFromConfig(JsonObject& root);
  void addToConfig(JsonObject& root);
  uint16_t getId() { return USERMOD_ID_HTTP_PULL_LIGHT_CONTROL; }
  inline void enable(bool enable) { enabled = enable; }   // Enable or Disable the usermod
  inline bool isEnabled() { return enabled; }             // Get usermod enabled or disabled state
  virtual ~HttpPullLightControl() { 
   // Remove the cached client if needed
    if (client) {
      client->onDisconnect(nullptr);
      client->onError(nullptr);
      client->onTimeout(nullptr);
      client->onData(nullptr);
      client->onConnect(nullptr);
      // Now it is safe to delete the client.
      delete client; // This is safe even if client is nullptr.
      client = nullptr;
    }
  }
};