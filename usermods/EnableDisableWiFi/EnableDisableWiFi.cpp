#include "EnableDisableWiFi.h"

void EnableDisableWiFiUsermod::setup()
{
    DEBUG_PRINT(F("wifi : "));
    if(wifiEnabled)
        enableWiFi();
    else
        disableWiFi();
    initDone = true;
    DEBUG_PRINTLN(wifiEnabled ? F("enabled") : F("disabled"));
}

bool EnableDisableWiFiUsermod::readFromConfig(JsonObject& root)
{
    DEBUG_PRINT(FPSTR(_name));
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    bool newWifiEnabledSetting = top[FPSTR(_wifiEnabled)] | wifiEnabled;
    DEBUG_PRINTLN(F(": config loaded"));
    if(wifiEnabled != newWifiEnabledSetting)
    {
        DEBUG_PRINTLN(F("  toggling WiFi setting"));
        wifiEnabled = newWifiEnabledSetting;
        setup();
    }
    return true;
}

void EnableDisableWiFiUsermod::addToConfig(JsonObject &root)
{
    DEBUG_PRINTLN(F("addToConfig called"));
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_wifiEnabled)] = wifiEnabled;
}

void EnableDisableWiFiUsermod::disableWiFi()
{
    DEBUG_PRINTLN(F("disableWiFi called"));
    apBehavior = AP_BEHAVIOR_BUTTON_ONLY;
    wifiEnabled = false;
}

void EnableDisableWiFiUsermod::enableWiFi()
{
    DEBUG_PRINTLN(F("enableWiFi called"));
    // need to disable other radio needs (BLE, mesh, etc)
    //apBehavior = AP_BEHAVIOR_NO_CONN;
    wifiEnabled = true;
}

// strings to reduce flash memory usage (used more than twice)
const char EnableDisableWiFiUsermod::_name[]        PROGMEM = "Enable/Disable-WiFi-mod";
const char EnableDisableWiFiUsermod::_wifiEnabled[] PROGMEM = "WiFi-enabled";

static EnableDisableWiFiUsermod usermod_EnableDisableWiFi;
REGISTER_USERMOD(usermod_EnableDisableWiFi);