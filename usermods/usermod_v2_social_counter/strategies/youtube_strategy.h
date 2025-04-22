#pragma once

#include "social_network_strategy.h"
#include "../social_network_types.h"
#include <Arduino.h>
#include <HTTPClient.h>

class YouTubeStrategy : public SocialNetworkStrategy
{
public:
  bool fetchFollowerCount(const String &link, int &count) override
  {
    // Implementação simplificada para evitar dependências

    // Simula a resposta da API
    count = random(10000, 1000000);
    return true;
  }

  String getName() override
  {
    return "YouTube";
  }

  int getType() override
  {
    return SOCIAL_COUNTER_YOUTUBE;
  }

private:
  String extractChannelId(const String &link)
  {
    // Extrai o ID do canal do YouTube a partir de um link
    // Exemplo: https://www.youtube.com/channel/UCxxxxxxx -> UCxxxxxxx

    if (link.indexOf("youtube.com/channel/") != -1)
    {
      int start = link.indexOf("youtube.com/channel/") + 20;
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
    else if (link.indexOf("youtube.com/c/") != -1)
    {
      // Para URLs personalizados, precisaríamos consultar a API para obter o ID real
      // Retornamos o nome do canal para simplicidade
      int start = link.indexOf("youtube.com/c/") + 14;
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

    // Se não for um formato reconhecido, retorna o link original
    return link;
  }
};