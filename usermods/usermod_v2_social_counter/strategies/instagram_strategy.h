#pragma once

#include "social_network_strategy.h"
#include "../social_network_types.h"
#include <Arduino.h>
#include <HTTPClient.h>

// Variável global para simular o contador
static int mockCounter = -1;

class InstagramStrategy : public SocialNetworkStrategy
{
public:
  bool fetchMetric(int metric, const String &link, int &count) override
  {
    // Simula diferentes métricas para o Instagram (ainda que iguais por enquanto)
    switch (metric)
    {
    case METRIC_FOLLOWERS:
    case METRIC_VIEWS:
    case METRIC_LIVE:
    case METRIC_SUBSCRIBERS:
    default:
      return simulateMockFollowerCount(count);
    }
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
  /**
   * Simula a contagem de seguidores: 111111, 222222, ...
   */
  bool simulateMockFollowerCount(int &count)
  {
    mockCounter = (mockCounter + 1) % 10;

    int repeatedDigits = 0;
    for (int i = 0; i < 6; i++)
    {
      repeatedDigits = repeatedDigits * 10 + mockCounter;
    }

    count = repeatedDigits;

    Serial.printf("[MOCK] Instagram seguidores simulados: %d\n", count);
    return true;
  }

  /**
   * Extrai o nome de usuário a partir de uma URL do Instagram
   */
  String extractUsername(const String &link)
  {
    int start = link.indexOf("instagram.com/");
    if (start == -1)
      return link;

    start += 14;
    int end = link.indexOf("/", start);
    return (end == -1) ? link.substring(start) : link.substring(start, end);
  }
};
