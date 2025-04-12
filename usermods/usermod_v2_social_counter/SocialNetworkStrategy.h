#pragma once

#include <Arduino.h>

class SocialNetworkStrategy
{
public:
  virtual ~SocialNetworkStrategy() = default;

  // Métodos que toda rede social deve implementar
  virtual bool fetchFollowerCount(const String &link, int &count) = 0;
  virtual String getName() = 0;
  virtual int getType() = 0;
};