#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>

namespace my_mod {

struct XpOrbMergeConfig {
    bool  enable        = true;
    float radius        = 8.0F;
    int   intervalTicks = 20;
};

struct Config {
    int              version           = 1;
    ll::io::LogLevel logLevel          = ll::io::LogLevel::Info;
    std::string      language          = "en_US";
    bool             autoFishingEnable = true;
    XpOrbMergeConfig xpOrbMerge{};
};

} // namespace my_mod
