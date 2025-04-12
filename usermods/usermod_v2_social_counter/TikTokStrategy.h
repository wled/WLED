#pragma once

#include "SocialNetworkStrategy.h"
#include "SocialNetworkTypes.h"
#include <Arduino.h>
#include <HTTPClient.h>

class TikTokStrategy : public SocialNetworkStrategy
{
public:
  bool fetchFollowerCount(const String &link, int &count) override
  {
    // TikTok não oferece uma API pública facilmente acessível
    // Esta implementação é simplificada para demonstração

    String username = extractUsername(link);

    // Em uma implementação real, você poderia usar um serviço de terceiros
    // ou web scraping para obter a contagem de seguidores

    // Para demonstração, retornamos um número aleatório
    count = random(5000, 500000);
    return true;
  }

  String getName() override
  {
    return "TikTok";
  }

  int getType() override
  {
    return SOCIAL_COUNTER_TIKTOK;
  }

private:
  String extractUsername(const String &link)
  {
    // Extrai o nome de usuário de uma URL do TikTok
    // Exemplo: https://www.tiktok.com/@username/ -> username
    int start = link.indexOf("tiktok.com/@");
    if (start == -1)
    {
      // Tenta outro formato
      start = link.indexOf("tiktok.com/");
      if (start == -1)
        return link; // Se não for URL, assume que é o nome do usuário

      start += 11; // Comprimento de "tiktok.com/"
    }
    else
    {
      start += 12; // Comprimento de "tiktok.com/@"
    }

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