#pragma once

#include "social_network_strategy.h"
#include "../social_network_types.h"
#include <Arduino.h>
#include <HTTPClient.h>

// Variável global para o contador
static int mockCounter = -1;

class InstagramStrategy : public SocialNetworkStrategy
{
public:
  bool fetchFollowerCount(const String &link, int &count) override
  {
    // Incrementa o contador a cada chamada
    mockCounter = (mockCounter + 1) % 10; // Incrementa de 0 a 9 para cada dígito

    // Gera uma sequência de 6 dígitos repetidos (111111, 222222, etc)
    int repeatedDigits = 0;
    for (int i = 0; i < 6; i++)
    {
      repeatedDigits = repeatedDigits * 10 + mockCounter;
    }

    // Retorna o valor com dígitos repetidos
    count = repeatedDigits;

    Serial.printf("[MOCK] Instagram contador: %d\n", count);
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