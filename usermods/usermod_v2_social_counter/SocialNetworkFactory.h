#pragma once

#include "strategies/SocialNetworkStrategy.h"
#include "SocialNetworkTypes.h"
#include "strategies/InstagramStrategy.h"
#include "strategies/YouTubeStrategy.h"
#include "strategies/TikTokStrategy.h"
#include "strategies/TwitchStrategy.h"
#include <memory>

class SocialNetworkFactory
{
public:
  static std::unique_ptr<SocialNetworkStrategy> createStrategy(int socialType)
  {
    switch (socialType)
    {
    case SOCIAL_COUNTER_INSTAGRAM:
      return std::unique_ptr<SocialNetworkStrategy>(new InstagramStrategy());

    case SOCIAL_COUNTER_YOUTUBE:
      return std::unique_ptr<SocialNetworkStrategy>(new YouTubeStrategy());

    case SOCIAL_COUNTER_TIKTOK:
      return std::unique_ptr<SocialNetworkStrategy>(new TikTokStrategy());

    case SOCIAL_COUNTER_TWITCH:
      return std::unique_ptr<SocialNetworkStrategy>(new TwitchStrategy());

      // Você pode adicionar outras redes sociais à medida que implementá-las

    default:
      // Se o tipo não for reconhecido, usa Instagram como fallback
      return std::unique_ptr<SocialNetworkStrategy>(new InstagramStrategy());
    }
  }
};