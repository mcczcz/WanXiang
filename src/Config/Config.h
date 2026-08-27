#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>
#include <vector>

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

struct FastLeafDecayConfig {
    bool enable = true;
    int  radius = 8;
};

struct ScheduledCommandTaskConfig {
    std::string cron            = "0 */5 * * * *";
    std::string command         = "list";
    bool        outputToConsole = true;
};

struct ScheduledCommandsConfig {
    bool                                    enable = false;
    std::vector<ScheduledCommandTaskConfig> tasks{};
};

struct Config {
    int                     version                  = 7;
    ll::io::LogLevel        logLevel                 = ll::io::LogLevel::Info;
    std::string             language                 = "en_US";
    bool                    trustedSkinEnable        = true;
    bool                    autoFishingEnable        = true;
    bool                    abilityCommandEnable     = true;
    bool                    cleanUnknownBlocksEnable = true;
    XpOrbMergeConfig        xpOrbMerge{};
    MobEffectBlockerConfig  mobEffectBlocker{};
    FastLeafDecayConfig     fastLeafDecay{};
    ScheduledCommandsConfig scheduledCommands{};
    bool                    markPacksAsAddon          = true;
    bool                    forceResourcePackRequired = false;
};

} // namespace my_mod
