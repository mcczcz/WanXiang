#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>

namespace my_mod {

struct XpOrbMergeConfig {
    bool  enable        = true;
    float radius        = 8.0F;
    int   intervalTicks = 20;
};

struct MobEffectBlockerConfig {
    bool enable      = true;
    bool weaving     = true;
    bool infested    = true;
    bool oozing      = true;
    bool windCharged = false;
};

struct Config {
    int                    version           = 2;
    ll::io::LogLevel       logLevel          = ll::io::LogLevel::Info;
    std::string            language          = "en_US";
    bool                   trustedSkinEnable = true;
    bool                   autoFishingEnable = true;
    bool                   abilityCommandEnable = true;
    XpOrbMergeConfig       xpOrbMerge{};
    MobEffectBlockerConfig mobEffectBlocker{};
};

} // namespace my_mod
