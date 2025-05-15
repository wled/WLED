#include "wled.h"
#include "social_network_types.h"
#include "strategies/social_network_strategy.h"
#include "social_network_factory.h"
#include <memory>

#define SOCIAL_SEGMENT_INDEX 0

/*
// wire layout: FABCDEG
//           -------
//         /   A   /
//        / F     / B
//       /       /
//       -------
//     /   G   /
//    / E     / C
//   /       /
//   -------
//      D
*/

class SocialCounterUsermod : public Usermod
{

private:
  bool enabled = false;
  bool initDone = false;
  unsigned long lastTime = 0;
  unsigned long lastAPICall = 0;
  unsigned long apiCallInterval = 0;
  int lastDisplayedCount = -1;
  bool isAnimating = false;
  int animationStep = 0;
  const int totalAnimationSteps = 10;
  unsigned long animationStartTime = 0;
  int previousCount = 0;
  int selectedMetric = METRIC_FOLLOWERS;
  int maxDigits = 0;

  String link = "";
  int social = SOCIAL_COUNTER_MOCK;
  int followerCount = 0;
  int updateIntervalSeconds = 3;

  // Informações sobre o segmento atual
  uint16_t segmentStart = 0;
  uint16_t segmentLength = 0;
  bool segmentValid = false;

  // Configurações do display de 7 segmentos
  int ledsPerSegment = 2;       // Quantidade de LEDs por segmento
  int digitSpacing = 0;         // Espaço entre dígitos
  bool showLeadingZeros = true; // Controla se zeros à esquerda devem ser exibidos (alterado para true por padrão)

  // Posições dos segmentos no hardware (wire layout: FABCDEG)
  typedef enum
  {
    SEG_F = 0,
    SEG_A = 1,
    SEG_B = 2,
    SEG_C = 3,
    SEG_D = 4,
    SEG_E = 5,
    SEG_G = 6
  } SegmentPosition;

  // Definição dos segmentos ativos para cada dígito
  // Usando array bidimensional: [dígito][segmento]
  // 1 = segmento ligado, 0 = segmento desligado
  const bool digitSegments[10][7] = {
      // F, A, B, C, D, E, G (FABCDEG - ordem física)
      {1, 1, 1, 1, 1, 1, 0}, // 0
      {0, 0, 1, 1, 0, 0, 0}, // 1
      {0, 1, 1, 0, 1, 1, 1}, // 2
      {0, 1, 1, 1, 1, 0, 1}, // 3
      {1, 0, 1, 1, 0, 0, 1}, // 4
      {1, 1, 0, 1, 1, 0, 1}, // 5
      {1, 1, 0, 1, 1, 1, 1}, // 6
      {0, 1, 1, 1, 0, 0, 0}, // 7
      {1, 1, 1, 1, 1, 1, 1}, // 8
      {1, 1, 1, 1, 1, 0, 1}  // 9
  };

  std::unique_ptr<SocialNetworkStrategy> strategy;

  void updateStrategy()
  {
    strategy = SocialNetworkFactory::createStrategy(social);
  }

  /**
   * Detecta e configura informações sobre o segmento atual
   * @return true se o segmento foi encontrado e configurado com sucesso
   */
  bool detectSegment()
  {
    if (strip.getActiveSegmentsNum() == 0)
    {
      Serial.println("[ERRO] Nenhum segmento WLED encontrado!");
      segmentValid = false;
      return false;
    }

    // Verifica se existe pelo menos um segmento
    if (SOCIAL_SEGMENT_INDEX >= strip.getActiveSegmentsNum())
    {
      Serial.printf("[ALERTA] Não há segmento %d, mas encontrados %d segmentos\n",
                    SOCIAL_SEGMENT_INDEX, strip.getActiveSegmentsNum());
      segmentValid = false;
      return false;
    }

    // Obtém as informações do segmento
    Segment &seg = strip.getSegment(SOCIAL_SEGMENT_INDEX);
    segmentStart = seg.start;

    // Calcula o comprimento do segmento, sem adicionar +1
    // Isso assume que seg.stop é o índice após o último LED (exclusivo), não o último índice (inclusivo)
    segmentLength = seg.stop - seg.start;

    Serial.printf("[INFO] Segmento %d configurado: LEDs %d a %d (total: %d)\n",
                  SOCIAL_SEGMENT_INDEX, segmentStart, seg.stop - 1, segmentLength);

    int digitWidth = 7 * ledsPerSegment + digitSpacing;
    maxDigits = segmentLength / digitWidth;

    Serial.printf("[INFO] Capacidade máxima de dígitos: %d\n", maxDigits);

    segmentValid = true;
    return true;
  }

  /**
   * Obtém o índice do LED inicial para uma posição específica de segmento
   * @param digit Posição do dígito (0 = dígito mais à direita)
   * @param segPos Posição do segmento (0-6 = F,A,B,C,D,E,G)
   * @return Índice do primeiro LED do segmento
   */
  uint16_t getSegmentLedIndex(int digit, int segPos)
  {
    // Calcula a posição base do dígito
    int digitWidth = 7 * ledsPerSegment + digitSpacing;
    int digitPosition = digitWidth * digit;

    // Calcula o índice do LED
    return segmentStart + digitPosition + (segPos * ledsPerSegment);
  }

  /**
   * Define o estado dos LEDs para um segmento específico
   * @param digit Posição do dígito
   * @param segPos Posição do segmento (0-6)
   * @param on Estado do segmento (true = ligado, false = desligado)
   */
  void setSegmentLeds(int digit, int segPos, bool on)
  {
    uint16_t ledIndex = getSegmentLedIndex(digit, segPos);

    for (int i = 0; i < ledsPerSegment; i++)
    {
      if (on)
      {
        uint32_t color = strip.getColor(); // cor global do WLED
        strip.setPixelColor(ledIndex + i, color);
      }
      else
      {
        strip.setPixelColor(ledIndex + i, 0); // desliga o LED
      }
    }
  }

  /**
   * Exibe um número no display de 7 segmentos
   * @param number Número a ser exibido
   */
  void displayNumber(int number)
  {
    if (!segmentValid || maxDigits <= 0)
      return;

    // Limita o valor ao número máximo de dígitos
    int maxValue = pow(10, maxDigits);
    number %= maxValue;

    // Converte número em string
    String numStr = String(number);

    // Preenche com zeros à esquerda, se necessário
    while (numStr.length() < maxDigits)
      numStr = "0" + numStr;

    bool skippingLeading = !showLeadingZeros;

    for (int digit = 0; digit < maxDigits; digit++)
    {
      int index = maxDigits - 1 - digit; // começa do dígito menos significativo
      char c = numStr.charAt(index);

      // Se estamos ignorando zeros à esquerda
      if (skippingLeading && c == '0')
      {
        // Apaga os segmentos desse dígito
        for (int seg = 0; seg < 7; seg++)
          setSegmentLeds(digit, seg, false);
        continue;
      }

      skippingLeading = false; // encontrou um dígito significativo

      int digitValue = c - '0';

      for (int seg = 0; seg < 7; seg++)
      {
        bool segOn = digitSegments[digitValue][seg];
        setSegmentLeds(digit, seg, segOn);
      }
    }

    strip.show();
  }

  void animateNumberTransition(int from, int to)
  {
    if (animationStep >= totalAnimationSteps)
    {
      isAnimating = false;
      displayNumber(to);
      lastDisplayedCount = to;
      return;
    }

    // Aplica um efeito simples: piscar ou fade nos segmentos (temporário)
    float progress = (float)animationStep / totalAnimationSteps;
    uint8_t fade = 255 * progress;

    // Por enquanto, vamos apenas mostrar o novo número com brilho parcial
    // Depois podemos simular o deslocamento real com mapeamento avançado
    // Calcula cor com fade antes do loop
    uint32_t baseColor = strip.getColor(strip.getSegment(0).colors[0]);
    uint8_t r = R(baseColor) * progress;
    uint8_t g = G(baseColor) * progress;
    uint8_t b = B(baseColor) * progress;

    for (int digit = 0; digit < maxDigits; digit++)
    {
      int fromDigit = (from / (int)pow(10, digit)) % 10;
      int toDigit = (to / (int)pow(10, digit)) % 10;

      int valueToShow = (progress < 0.5) ? fromDigit : toDigit;

      for (int seg = 0; seg < 7; seg++)
      {
        bool on = digitSegments[valueToShow][seg];
        uint16_t ledIndex = getSegmentLedIndex(digit, seg);

        for (int i = 0; i < ledsPerSegment; i++)
        {
          if (on)
          {
            strip.setPixelColor(ledIndex + i, WS2812FX::Color(r, g, b));
          }
          else
          {
            strip.setPixelColor(ledIndex + i, 0);
          }
        }
      }
    }

    strip.show();
    animationStep++;
  }

public:
  static const char _name[];
  static const char _enabled[];

  void enable(bool enable)
  {
    Serial.printf("[INFO] Social Counter %s\n", enable ? "ativo" : "inativo");
    enabled = enable;
  }

  bool isEnabled()
  {
    return enabled;
  }

  void setup() override
  {
    Serial.println("[INFO] Iniciando Social Counter usermod");
    updateStrategy();
    updateIntervalTimer();
    detectSegment(); // Detecta o segmento durante a inicialização
    initDone = true;
    counter = 0;
  }

  void updateIntervalTimer()
  {
    // Usa o valor configurado pelo usuário
    apiCallInterval = ((unsigned long)updateIntervalSeconds) * 1000;

    Serial.print("Setting update interval to: ");
    Serial.print(updateIntervalSeconds);
    Serial.print(" seconds (");
    Serial.print(apiCallInterval);
    Serial.println(" ms)");
  }

  void connected() override
  {
    Serial.println("[INFO] Conectado ao WiFi!");
    Serial.print("[INFO] Endereço IP local: ");
    Serial.println(WiFi.localIP());

    // Atualiza as informações do segmento após a conexão
    detectSegment();

    if (enabled)
    {
      updateFollowerCount();
    }
  }

  void loop() override
  {
    if (!enabled || strip.isUpdating())
      return;

    if (millis() - lastTime > 10000) // Log a cada 10 segundos apenas
    {
      lastTime = millis();
    }

    unsigned long currentMillis = millis();

    if (WLED_CONNECTED && (currentMillis - lastAPICall >= apiCallInterval))
    {
      updateFollowerCount();
      lastAPICall = currentMillis;
    }

    // Só redesenha se o valor exibido mudou
    if (enabled && segmentValid)
    {
      if (followerCount != lastDisplayedCount && !isAnimating)
      {
        previousCount = lastDisplayedCount;
        isAnimating = true;
        animationStep = 0;
        animationStartTime = millis();
      }

      if (isAnimating && millis() - animationStartTime > 50)
      { // 50ms por frame
        animationStartTime = millis();
        animateNumberTransition(previousCount, followerCount);
      }
    }
  }

  void updateFollowerCount()
  {
    Serial.println("[INFO] Atualizando contagem de seguidores");

    if (!strategy)
    {
      updateStrategy();
    }

    if (strategy && !link.isEmpty())
    {
      int count = 0;
      bool success = strategy->fetchMetric(selectedMetric, link, count);

      if (success)
      {
        followerCount = count;
        Serial.printf("[INFO] %s followers: %d\n", strategy->getName().c_str(), followerCount);
      }
      else
      {
        Serial.println("[ERRO] Falha ao buscar contagem de seguidores");
      }
    }
    else
    {
      Serial.println("[ERRO] Impossível atualizar - estratégia inválida ou link vazio");
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
    usermod["metric"] = selectedMetric;
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

      bool newEnabled = usermod["enabled"] | enabled;
      if (newEnabled != enabled)
      {
        enable(newEnabled);
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

    // Configurações do display de 7 segmentos
    top["ledsPerSegment"] = ledsPerSegment;
    top["digitSpacing"] = digitSpacing;
    top["showLeadingZeros"] = showLeadingZeros;
    top["metric"] = selectedMetric;
  }

  bool readFromConfig(JsonObject &root) override
  {
    JsonObject top = root[FPSTR(_name)];

    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);

    int oldSocial = social;
    configComplete &= getJsonValue(top["social"], social, SOCIAL_COUNTER_MOCK);

    if (oldSocial != social)
    {
      updateStrategy();
    }

    configComplete &= getJsonValue(top["link"], link, "");
    configComplete &= getJsonValue(top["updateInterval"], updateIntervalSeconds, updateIntervalSeconds);

    // Configurações do display de 7 segmentos
    configComplete &= getJsonValue(top["ledsPerSegment"], ledsPerSegment, 2);
    configComplete &= getJsonValue(top["digitSpacing"], digitSpacing, 0);
    configComplete &= getJsonValue(top["showLeadingZeros"], showLeadingZeros, true);
    configComplete &= getJsonValue(top["metric"], selectedMetric, METRIC_FOLLOWERS);

    updateIntervalTimer();

    return configComplete;
  }

  void appendConfigData() override
  {
    // Link input info
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":link"));
    oappend(F("',1,'<i>Enter the profile link</i>');"));

    // Update interval info
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":updateInterval"));
    oappend(F("',1,'<i>Delay between updates (in seconds, default: 3)</i>');"));

    // LEDs per segment info
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":ledsPerSegment"));
    oappend(F("',1,'<i>LEDs per segment (default: 2)</i>');"));

    // Digit spacing info
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":digitSpacing"));
    oappend(F("',1,'<i>Spacing between digits (default: 0)</i>');"));

    // Leading zeros info
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":showLeadingZeros"));
    oappend(F("',1,'<i>Show leading zeros (default: true)</i>');"));

    // Social network dropdown
    oappend(F("dds=addDropdown('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F("','social');"));
    oappend(F("addOption(dds,'Mock for Testing',0);"));
    oappend(F("addOption(dds,'Instagram',1);"));
    oappend(F("addOption(dds,'TikTok',2);"));
    oappend(F("addOption(dds,'Twitch',3);"));
    oappend(F("addOption(dds,'YouTube',4);"));

    // Metric dropdown
    oappend(F("ddm=addDropdown('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F("','metric');"));
    oappend(F("addOption(ddm,'Followers',0);"));
    oappend(F("addOption(ddm,'Views',1);"));
    oappend(F("addOption(ddm,'Live',2);"));
    oappend(F("addOption(ddm,'Subscribers',3);"));
  }

  // void handleOverlayDraw() override
  // {
  //   // Exibe o número de seguidores no display de 7 segmentos
  //   if (enabled && initDone && segmentValid)
  //   {
  //     displayNumber(followerCount);
  //   }
  // }

  uint16_t getId() override
  {
    return USERMOD_ID_SOCIAL_COUNTER;
  }

  /**
   * Chamado quando o estado do WLED é alterado (por exemplo, quando segmentos são modificados)
   */
  void onStateChange(uint8_t callMode) override
  {
    // Atualiza as informações do segmento quando o estado muda
    if (callMode == CALL_MODE_DIRECT_CHANGE ||
        callMode == CALL_MODE_BUTTON ||
        callMode == CALL_MODE_NOTIFICATION)
    {
      Serial.printf("[INFO] Estado WLED alterado (modo: %d), verificando segmento\n", callMode);
      detectSegment();
    }
  }
};

// Definição das variáveis estáticas
const char SocialCounterUsermod::_name[] = "Social Counter";
const char SocialCounterUsermod::_enabled[] = "enabled";

static SocialCounterUsermod social_counter;
REGISTER_USERMOD(social_counter);
