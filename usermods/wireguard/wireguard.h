#pragma once

#include <WireGuard-ESP32.h>

#include "wled.h"

class WireguardUsermod : public Usermod {
   public:
    void setup() { configTzTime(posix_tz, ntpServerName); }

    void connected() {
        if (wg.is_initialized()) {
            wg.end();
        }
    }

    void loop() {
        if (millis() - lastTime > 5000) {
            if (is_enabled && WLED_CONNECTED) {
                if (!wg.is_initialized()) {
                    struct tm timeinfo;
                    if (getLocalTime(&timeinfo, 0)) {
                        if (strlen(preshared_key) < 1) {
                            wg.begin(local_ip, private_key, endpoint_address, public_key, endpoint_port, NULL);
                        } else {
                            wg.begin(local_ip, private_key, endpoint_address, public_key, endpoint_port, preshared_key);
                        }
                    }
                }
            }

            lastTime = millis();
        }
    }

    void addToJsonInfo(JsonObject& root) {
        JsonObject user = root["u"];
        if (user.isNull()) user = root.createNestedObject("u");

        JsonArray infoArr = user.createNestedArray(F("WireGuard"));
        String uiDomString;

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 0)) {
            uiDomString = "Time out of sync!";
        } else {
            if (wg.is_initialized()) {
                uiDomString = "netif up!";
            } else {
                uiDomString = "netif down :(";
            }
        }
        if (is_enabled) infoArr.add(uiDomString);
    }

    void appendConfigData() {
        oappend(SET_F("addInfo('WireGuard:host',1,'Server Hostname');"));           // 0 is field type, 1 is actual field
        oappend(SET_F("addInfo('WireGuard:port',1,'Server Port');"));               // 0 is field type, 1 is actual field
        oappend(SET_F("addInfo('WireGuard:ip',1,'Device IP');"));                   // 0 is field type, 1 is actual field
        oappend(SET_F("addInfo('WireGuard:psk',1,'Pre Shared Key (optional)');"));  // 0 is field type, 1 is actual field
        oappend(SET_F("addInfo('WireGuard:pem',1,'Private Key');"));                // 0 is field type, 1 is actual field
        oappend(SET_F("addInfo('WireGuard:pub',1,'Public Key');"));                 // 0 is field type, 1 is actual field
        oappend(SET_F("addInfo('WireGuard:tz',1,'POSIX timezone string');"));       // 0 is field type, 1 is actual field
    }

    void addToConfig(JsonObject& root) {
        JsonObject top = root.createNestedObject(F("WireGuard"));
        top[F("host")] = endpoint_address;
        top[F("port")] = endpoint_port;
        top[F("ip")] = local_ip.toString();
        top[F("psk")] = preshared_key;
        top[F("pem")] = private_key;
        top[F("pub")] = public_key;
        top[F("tz")] = posix_tz;
    }

    bool readFromConfig(JsonObject& root) {
        JsonObject top = root[F("WireGuard")];

        if (top["host"].isNull() || top["port"].isNull() || top["ip"].isNull() || top["pem"].isNull() || top["pub"].isNull() || top["tz"].isNull()) {
            is_enabled = false;
            return false;
        } else {
            const char* host = top["host"];
            strncpy(endpoint_address, host, 100);

            const char* ip_s = top["ip"];
            uint8_t ip[4];
            sscanf(ip_s, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
            local_ip = IPAddress(ip[0], ip[1], ip[2], ip[3]);

            const char* pem = top["pem"];
            strncpy(private_key, pem, 45);

            const char* pub = top["pub"];
            strncpy(public_key, pub, 45);

            const char* tz = top["tz"];
            strncpy(posix_tz, tz, 150);

            endpoint_port = top["port"];

            if (!top["psk"].isNull()) {
                const char* psk = top["psk"];
                strncpy(preshared_key, psk, 45);
            }

            is_enabled = true;
        }

        return is_enabled;
    }

    uint16_t getId() { return USERMOD_ID_WIREGUARD; }

   private:
    WireGuard wg;
    char preshared_key[45];
    char private_key[45];
    IPAddress local_ip;
    char public_key[45];
    char endpoint_address[100];
    char posix_tz[150];
    int endpoint_port = 0;
    bool is_enabled = false;
    unsigned long lastTime = 0;
};