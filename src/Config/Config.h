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
    // 每片被唤醒的树叶在 [minDelayTicks, maxDelayTicks] 个游戏刻后才检查腐烂。
    // 随机化是刻意的：整棵树同刻消失既难看，也会把工作量堆在同一 tick 上。
    int minDelayTicks = 2;
    int maxDelayTicks = 8;
};

struct UnloadedEntitiesCleanerConfig {
    bool                     enable                      = true;
    std::vector<std::string> entityTypeNamesToClean      = {};
    bool                     logCleanedEntityTypeCounts  = true;
    bool                     logSkippedEntityTypeCounts  = true;
    bool                     writeCleanupResultToTextLog = false;
    std::string              cleanupResultTextLogFile    = "cleanup-result.log";
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

struct EntityCleanerScheduleConfig {
    bool        enable = false;
    std::string cron   = "0 */10 * * * *"; // 每 10 分钟
};

struct EntityCleanerThresholdConfig {
    bool enable = false;
    // 阈值比较的是「候选实体数」而非世界里的实体总数：后者含玩家和所有受保护实体，
    // 用它做判据会让「2500 个受保护村民 + 500 只怪」的服务器永远触发、永远清不到目标。
    int maxEntities        = 3000;
    int targetEntities     = 2000;
    int checkIntervalTicks = 100; // 5 秒检查一次
    int cooldownSeconds    = 60;  // 两次阈值清理之间的最小间隔
};

// mode 语义：
//   "whitelist" -> 只清理名单内的类型/family
//   "blacklist" -> 名单内受保护，其余全部清理
// 默认取 whitelist：名单写错或清空只会导致「什么都不删」，而 blacklist 的同类错误
// 意味着「删除一切未列出的」。
struct EntityCleanerFilterConfig {
    std::string              mode      = "whitelist";
    std::vector<std::string> typeNames = {"minecraft:item", "minecraft:xp_orb", "minecraft:arrow"};
    std::vector<std::string> families  = {"monster"};
};

// 按实体的运行时状态保护，与类型无关，两种 mode 下都生效。
struct EntityCleanerProtectionConfig {
    bool  named                = true;
    bool  tamed                = true;
    bool  leashed              = true;
    bool  vehicles             = true;
    bool  persistent           = true;
    bool  villagers            = true;
    bool  builds               = true;
    bool  bosses               = true;
    bool  fallingBlocks        = true;
    bool  movingBlocks         = true;
    bool  fishingHooks         = true;
    bool  ominousItemSpawners  = true;
    float playerRadius         = 32.0F; // 0 = 关闭
};

struct EntityCleanerWarningConfig {
    bool             enable        = true;
    std::vector<int> secondsBefore = {10, 5, 3};
};

struct EntityCleanerConfig {
    bool                          enable = false;
    bool                          dryRun = false; // 只统计并记录日志，不实际删除
    EntityCleanerScheduleConfig   schedule{};
    EntityCleanerThresholdConfig  threshold{};
    EntityCleanerFilterConfig     filter{};
    EntityCleanerProtectionConfig protection{};
    EntityCleanerWarningConfig    warning{};
    bool                          logResult = true;
};

struct Config {
    int                           version                  = 17;
    ll::io::LogLevel              logLevel                 = ll::io::LogLevel::Info;
    std::string                   language                 = "en_US";
    bool                          trustedSkinEnable        = true;
    bool                          autoFishingEnable        = true;
    bool                          abilityCommandEnable     = true;
    bool                          cleanUnknownBlocksEnable = true;
    bool                          hideOpPermissionEnable   = true;
    DroppedItemLifetimeConfig     droppedItemLifetime{};
    XpOrbMergeConfig              xpOrbMerge{};
    MobEffectBlockerConfig        mobEffectBlocker{};
    FastLeafDecayConfig           fastLeafDecay{};
    UnloadedEntitiesCleanerConfig unloadedEntitiesCleaner{};
    ScheduledCommandsConfig       scheduledCommands{};
    CommandPermissionsConfig      commandPermissions{};
    ServerPongConfig              serverPong{};
    ClientLevelNameConfig         clientLevelName{};
    ExplosionControlConfig        explosionControl{};
    EntityCleanerConfig           entityCleaner{};
    bool                          markPacksAsAddon          = true;
    bool                          forceResourcePackRequired = false;
};

} // namespace my_mod
