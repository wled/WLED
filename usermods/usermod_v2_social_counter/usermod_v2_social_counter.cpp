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

  String link = "";
  int social = SOCIAL_COUNTER_INSTAGRAM;
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
      if (!on)
      {
        // Se o segmento deve estar apagado, desligamos o LED
        strip.setPixelColor(ledIndex + i, 0);
      }
      // Se o segmento está ativo, não fazemos nada e mantemos os efeitos originais
    }
  }

  /**
   * Exibe um número no display de 7 segmentos
   * @param number Número a ser exibido
   */
  void displayNumber(int number)
  {
    if (!segmentValid)
    {
      return; // Não temos um segmento válido para exibir
    }

    // Calcula quantos dígitos podem caber no segmento
    int digitWidth = 7 * ledsPerSegment + digitSpacing;
    int maxDigits = segmentLength / digitWidth;

    if (maxDigits <= 0)
    {
      Serial.println("[ERRO] Segmento muito pequeno para exibir qualquer dígito!");
      return;
    }

    // Forçamos minDigits = 1 para garantir que pelo menos um dígito seja exibido
    // A exibição de zeros à esquerda é controlada por showLeadingZeros
    int minDigits = 1;

    // Limita o número ao máximo de dígitos disponíveis
    int maxValue = 1;
    for (int i = 0; i < maxDigits; i++)
    {
      maxValue *= 10;
    }
    number = number % maxValue;

    // Calcula quantos dígitos o número possui
    int tempNumber = number;
    int numDigits = 0;
    do
    {
      numDigits++;
      tempNumber /= 10;
    } while (tempNumber > 0);

    // Garantir que temos pelo menos o número mínimo de dígitos
    numDigits = max(numDigits, minDigits);

    // Exibe cada dígito - primeiro preparamos um mapa dos LEDs ativos
    bool ledActive[segmentLength];
    // Inicializa todos como inativos
    for (int i = 0; i < segmentLength; i++)
    {
      ledActive[i] = false;
    }

    // Marca os LEDs que devem estar ativos
    int digitsShown = 0;
    for (int digit = 0; digit < maxDigits; digit++)
    {
      int digitValue = number % 10;

      // Decide se mostra este dígito baseado nas configurações de zeros à esquerda
      bool showDigit = true;

      // Se este dígito é zero e está além dos dígitos significativos do número
      // e não estamos mostrando zeros à esquerda, pulamos este dígito
      if (digitValue == 0 && digit >= numDigits && !showLeadingZeros)
      {
        showDigit = false;
      }

      if (showDigit)
      {
        // Para cada segmento do dígito
        for (int seg = 0; seg < 7; seg++)
        {
          if (digitSegments[digitValue][seg])
          {
            // Este segmento deve estar ativo
            int ledStart = getSegmentLedIndex(digit, seg) - segmentStart;
            for (int i = 0; i < ledsPerSegment; i++)
            {
              if (ledStart + i < segmentLength)
              {
                ledActive[ledStart + i] = true;
              }
            }
          }
        }
      }

      digitsShown++;
      number /= 10;

      // Continua exibindo dígitos até atingir o mínimo de dígitos ou zerar o número
      if (number == 0 && digitsShown >= minDigits)
        break;
    }

    // Agora, desliga apenas os LEDs que devem estar inativos
    for (int i = 0; i < segmentLength; i++)
    {
      if (!ledActive[i])
      {
        strip.setPixelColor(segmentStart + i, 0);
      }
    }
  }

public:
  static const char _name[];
  static const char _enabled[];
  static int counter;

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
      lastAPICall = currentMillis; // Registra o tempo exato da chamada
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
      bool success = strategy->fetchFollowerCount(link, count);

      if (success)
      {
        followerCount = count;
        counter = followerCount; // Atualiza o contador estático para compatibilidade
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

    // Configurações do display de 7 segmentos
    configComplete &= getJsonValue(top["ledsPerSegment"], ledsPerSegment, 2);
    configComplete &= getJsonValue(top["digitSpacing"], digitSpacing, 0);
    configComplete &= getJsonValue(top["showLeadingZeros"], showLeadingZeros, true);

    updateIntervalTimer();

    return configComplete;
  }

  void appendConfigData() override
  {
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":link"));
    oappend(F("',1,'<i>Enter your link</i>');"));

    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":updateInterval"));
    oappend(F("',1,'<i>Delay in seconds between updates (default: 3)</i>');"));

    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":ledsPerSegment"));
    oappend(F("',1,'<i>LEDs por segmento (default: 2)</i>');"));

    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":digitSpacing"));
    oappend(F("',1,'<i>Espaço entre dígitos (default: 0)</i>');"));

    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":showLeadingZeros"));
    oappend(F("',1,'<i>Mostrar zeros à esquerda (default: true)</i>');"));

    oappend(F("dd=addDropdown('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F("','social');"));
    oappend(F("addOption(dd,'Instagram',0);"));
    oappend(F("addOption(dd,'Tiktok',1);"));
    oappend(F("addOption(dd,'Twitch',2);"));
    oappend(F("addOption(dd,'Youtube',3);"));
  }

  void handleOverlayDraw() override
  {
    // Exibe o número de seguidores no display de 7 segmentos
    if (enabled && initDone && segmentValid)
    {
      displayNumber(followerCount);
    }
  }

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
int SocialCounterUsermod::counter = 0;

static SocialCounterUsermod social_counter;
REGISTER_USERMOD(social_counter);