#pragma once

#include "SocialNetworkStrategy.h"
#include "SocialNetworkTypes.h"
#include <Arduino.h>
#include <HTTPClient.h>

class TwitchStrategy : public SocialNetworkStrategy
{
public:
  bool fetchFollowerCount(const String &link, int &count) override
  {
    // Implementação simplificada para evitar dependências

    // Para demonstração, retornamos um número aleatório
    count = random(1000, 100000);
    return true;
  }

  String getName() override
  {
    return "Twitch";
  }

  int getType() override
  {
    return SOCIAL_COUNTER_TWITCH;
  }

private:
  String extractUsername(const String &link)
  {
    // Extrai o nome de usuário de uma URL da Twitch
    // Exemplo: https://www.twitch.tv/username -> username
    int start = link.indexOf("twitch.tv/");
    if (start == -1)
      return link; // Se não for URL, assume que é o nome do usuário

    start += 10; // Comprimento de "twitch.tv/"
    int end = link.indexOf("/", start);

    if (end == -1)
    {
      return link.substring(start);
    }
    else
    {
      return link.substring(start, end);
    }
  }
};