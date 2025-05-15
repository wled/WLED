#pragma once

#include "strategies/social_network_strategy.h"
#include "social_network_types.h"
#include "strategies/mock_strategy.h"
#include "strategies/instagram_strategy.h"
#include "strategies/youtube_strategy.h"
#include "strategies/tiktok_strategy.h"
#include "strategies/twitch_strategy.h"
#include <memory>

class SocialNetworkFactory
{
public:
  static std::unique_ptr<SocialNetworkStrategy> createStrategy(int socialType)
  {
    switch (socialType)
    {
    case SOCIAL_COUNTER_MOCK:
      return std::unique_ptr<SocialNetworkStrategy>(new MockStrategy());

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