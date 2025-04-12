#include "wled.h"
#include "SocialNetworkTypes.h"
#include "SocialNetworkStrategy.h"
#include "SocialNetworkFactory.h"
#include <memory>

class SocialCounterUsermod : public Usermod
{

private:
  bool enabled = false;
  bool initDone = false;
  unsigned long lastTime = 0;
  unsigned long lastAPICall = 0;
  unsigned long apiCallInterval = 0;

  String link = "";
  int social = SOCIAL_COUNTER_INSTAGRAM;
  int followerCount = 0;
  int updateIntervalSeconds = 60;

  std::unique_ptr<SocialNetworkStrategy> strategy;

  void updateStrategy()
  {
    strategy = SocialNetworkFactory::createStrategy(social);
  }

public:
  static const char _name[];
  static const char _enabled[];
  static int counter;

  void enable(bool enable)
  {
    enabled = enable;
  }

  bool isEnabled()
  {
    return enabled;
  }

  void setup() override
  {
    Serial.println("Hello from my social counter!");
    updateStrategy();
    updateIntervalTimer();
    initDone = true;
    counter = 0;
  }

  void updateIntervalTimer()
  {
    apiCallInterval = ((unsigned long)updateIntervalSeconds) * 1000;
    Serial.print("Setting update interval to: ");
    Serial.print(updateIntervalSeconds);
    Serial.print(" seconds (");
    Serial.print(apiCallInterval);
    Serial.println(" ms)");
  }

  void connected() override
  {
    Serial.println("Connected to WiFi!");

    if (enabled)
    {
      updateFollowerCount();
    }
  }

  void loop() override
  {
    if (!enabled || strip.isUpdating())
      return;

    if (millis() - lastTime > 1000)
    {
      // Serial.println("I'm alive!");
      lastTime = millis();
    }

    unsigned long currentMillis = millis();
    if (WLED_CONNECTED && (currentMillis - lastAPICall >= apiCallInterval))
    {
      Serial.print("Timer triggered. Time since last call: ");
      Serial.print(currentMillis - lastAPICall);
      Serial.print("ms (configured for ");
      Serial.print(apiCallInterval);
      Serial.println("ms)");

      updateFollowerCount();
      lastAPICall = currentMillis; // Registra o tempo exato da chamada
    }
  }

  void updateFollowerCount()
  {
    if (!strategy)
    {
      updateStrategy();
    }

    if (strategy && !link.isEmpty())
    {
      int count = 0;
      bool success = strategy->fetchFollowerCount(link, count);

      if (success)
      {
        followerCount = count;
        counter = followerCount; // Atualiza o contador estático para compatibilidade
        Serial.print("Follower count for ");
        Serial.print(strategy->getName());
        Serial.print(": ");
        Serial.println(followerCount);
      }
    }
  }

  void addToJsonInfo(JsonObject &root) override
  {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    JsonArray socialArr = user.createNestedArray(FPSTR(_name));
    socialArr.add(followerCount);
    socialArr.add(F(" followers"));
  }

  void addToJsonState(JsonObject &root) override
  {
    if (!initDone || !enabled)
      return;

    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull())
      usermod = root.createNestedObject(FPSTR(_name));

    usermod["social"] = social;
    usermod["link"] = link;
    usermod["followers"] = followerCount;
    usermod["updateInterval"] = updateIntervalSeconds;
  }

  void readFromJsonState(JsonObject &root) override
  {
    if (!initDone)
      return;

    JsonObject usermod = root[FPSTR(_name)];
    if (!usermod.isNull())
    {
      int newSocial = usermod["social"] | social;
      if (newSocial != social)
      {
        social = newSocial;
        updateStrategy();
      }

      link = usermod["link"] | link;

      int newInterval = usermod["updateInterval"] | updateIntervalSeconds;
      if (newInterval != updateIntervalSeconds)
      {
        updateIntervalSeconds = newInterval;
        updateIntervalTimer();
      }
    }
  }

  void addToConfig(JsonObject &root) override
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top["social"] = social;
    top["link"] = link;
    top["updateInterval"] = updateIntervalSeconds;
  }

  bool readFromConfig(JsonObject &root) override
  {
    JsonObject top = root[FPSTR(_name)];

    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);

    int oldSocial = social;
    configComplete &= getJsonValue(top["social"], social, SOCIAL_COUNTER_INSTAGRAM);

    if (oldSocial != social)
    {
      updateStrategy();
    }

    configComplete &= getJsonValue(top["link"], link, "");
    configComplete &= getJsonValue(top["updateInterval"], updateIntervalSeconds, updateIntervalSeconds);
    updateIntervalTimer();

    return configComplete;
  }

  void appendConfigData() override
  {
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":great"));
    oappend(F("',1,'<i>(this is a great config value)</i>');"));
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":link"));
    oappend(F("',1,'enter your link');"));
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":updateInterval"));
    oappend(F("',1,'<i>Delay in seconds (default: 60)</i>');"));
    oappend(F("dd=addDropdown('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F("','social');"));
    oappend(F("addOption(dd,'Instagram',1);"));
    oappend(F("addOption(dd,'Tiktok',2);"));
    oappend(F("addOption(dd,'Twitch',3);"));
    oappend(F("addOption(dd,'Youtube',4);"));
  }

  void handleOverlayDraw() override
  {
    // Implementação para exibir o contador de seguidores na interface, se necessário
    // strip.setPixelColor(0, RGBW32(0,0,0,0))
  }

  uint16_t getId() override
  {
    return USERMOD_ID_SOCIAL_COUNTER;
  }
};

// Definição das variáveis estáticas
const char SocialCounterUsermod::_name[] = "Social Counter";
const char SocialCounterUsermod::_enabled[] = "enabled";
int SocialCounterUsermod::counter = 0;

static SocialCounterUsermod usermod_v2_social_counter;
REGISTER_USERMOD(usermod_v2_social_counter);