#pragma once

#include "social_network_strategy.h"
#include "../social_network_types.h"
#include <Arduino.h>

// Simula um contador incremental por métrica
class MockStrategy : public SocialNetworkStrategy
{
public:
  bool fetchMetric(int metric, const String &link, int &count) override
  {
    static int values[4] = {0, 1000, 5000, 100};

    // Simula cada métrica com incremento independente
    values[metric] += (metric + 1) * 111; // muda a taxa por tipo
    if (values[metric] > 999999)
      values[metric] = 0;

    count = values[metric];

    Serial.printf("[MOCK] Métrica %d simulada: %d\n", metric, count);
    return true;
  }

  String getName() override
  {
    return "Mock";
  }

  int getType() override
  {
    return SOCIAL_COUNTER_MOCK;
  }
};
