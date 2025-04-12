#pragma once

#include "SocialNetworkStrategy.h"
#include "SocialNetworkTypes.h"
#include <Arduino.h>
#include <HTTPClient.h>

class InstagramStrategy : public SocialNetworkStrategy
{
public:
  bool fetchFollowerCount(const String &link, int &count) override
  {
    // Implementação para buscar contagem de seguidores do Instagram
    // Para evitar problemas de dependência, usamos um método simplificado
    // sem parse de JSON

    // Para demonstração, retorna um número aleatório
    count = random(1000, 10000);
    return true;
  }

  String getName() override
  {
    return "Instagram";
  }

  int getType() override
  {
    return SOCIAL_COUNTER_INSTAGRAM;
  }

private:
  String extractUsername(const String &link)
  {
    // Extrai o nome de usuário de uma URL do Instagram
    // Exemplo: https://www.instagram.com/username/ -> username
    int start = link.indexOf("instagram.com/");
    if (start == -1)
      return link; // Se não for URL, assume que é o nome do usuário

    start += 14; // Comprimento de "instagram.com/"
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