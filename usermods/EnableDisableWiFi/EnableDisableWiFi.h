#include "wled.h"
#ifndef USERMOD_ENABLE_DISABLE_WIFI
    #define USERMOD_ENABLE_DISABLE_WIFI
#endif

class EnableDisableWiFiUsermod : public Usermod {

    private:
    bool initDone = false;
    bool wifiEnabled = true;
  
    static const char _name[];
    static const char _wifiEnabled[];
    
    public:
    void setup() override;
    void loop() override {}
    bool readFromConfig(JsonObject &root) override;
    void addToConfig(JsonObject &root) override;
    void disableWiFi();
    void enableWiFi();
    inline bool isEnabled() override {return wifiEnabled;}
    inline bool isWiFiEnabled() { return wifiEnabled; }
    inline uint16_t getId() override {return USERMOD_ID_ENABLE_DISABLE_WIFI;}
};
