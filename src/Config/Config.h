#pragma once

#include "ll/api/io/LogLevel.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace my_mod {

struct XpOrbMergeConfig {
    bool  enable        = true;
    float radius        = 8.0F;
    int   intervalTicks = 20;
};

struct DroppedItemLifetimeConfig {
    bool                                 enable         = false;
    int                                  defaultSeconds = 300;
    std::unordered_map<std::string, int> items{};
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

struct CommandPermissionRuleConfig {
    std::string command         = {};
    std::string permissionLevel = "Any";
};

struct CommandPermissionsConfig {
    bool                                     enable = false;
    std::vector<CommandPermissionRuleConfig> rules{};
};

struct ServerPongConfig {
    bool        enable               = false;
    bool        motdEnable           = false;
    std::string motd                 = {};
    bool        playerCountEnable    = false;
    int         playerCount          = 0;
    bool        maxPlayerCountEnable = false;
    int         maxPlayerCount       = 0;
};

struct ClientLevelNameConfig {
    bool        enable = false;
    std::string name   = {};
};

struct ExplosionRuleConfig {
    std::string sourceType = "minecraft:tnt";
    bool        cancel     = false;

    std::optional<float> radius{};
    std::optional<bool>  fire{};
    std::optional<bool>  breaking{};
    std::optional<bool>  allowUnderwater{};
    std::optional<bool>  canToggleBlocks{};
    std::optional<float> damageScaling{};
    std::optional<bool>  ignoreBlockResistance{};
    std::optional<float> maxResistance{};
    std::optional<bool>  inWater{};
    std::optional<int>   totalDamage{};
    std::optional<float> knockbackScaling{};
};

struct ExplosionControlConfig {
    bool                             enable = false;
    std::vector<ExplosionRuleConfig> rules{};
};

struct Config {
    int                       version                  = 15;
    ll::io::LogLevel          logLevel                 = ll::io::LogLevel::Info;
    std::string               language                 = "en_US";
    bool                      trustedSkinEnable        = true;
    bool                      autoFishingEnable        = true;
    bool                      abilityCommandEnable     = true;
    bool                      cleanUnknownBlocksEnable = true;
    bool                      hideOpPermissionEnable   = true;
    DroppedItemLifetimeConfig droppedItemLifetime{};
    XpOrbMergeConfig          xpOrbMerge{};
    MobEffectBlockerConfig    mobEffectBlocker{};
    FastLeafDecayConfig       fastLeafDecay{};
    ScheduledCommandsConfig   scheduledCommands{};
    CommandPermissionsConfig  commandPermissions{};
    ServerPongConfig          serverPong{};
    ClientLevelNameConfig     clientLevelName{};
    ExplosionControlConfig    explosionControl{};
    bool                      markPacksAsAddon          = true;
    bool                      forceResourcePackRequired = false;
};

} // namespace my_mod
