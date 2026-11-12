#pragma once

#include "ll/api/io/LogLevel.h"
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

enum class MovementType : unsigned char;
enum class StickyType : unsigned char;

namespace my_mod {

struct PistonBlockConfig {
    MovementType movementType = static_cast<MovementType>(0); // PushPull
    StickyType   stickyType   = static_cast<StickyType>(0);   // None
};

struct PistonControlConfig {
    bool                                    enable = false;
    std::map<std::string, PistonBlockConfig> customPistonBlocks{};
    std::set<std::string>                    pistonImmuneEntities{};
};

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
    bool enable      = false;
    bool weaving     = true;
    bool infested    = true;
    bool oozing      = true;
    bool windCharged = false;
};

struct FastLeafDecayConfig {
    bool enable = false;
    // 每片被唤醒的树叶在 [minDelayTicks, maxDelayTicks] 个游戏刻后才检查腐烂。
    // 随机化是刻意的：整棵树同刻消失既难看，也会把工作量堆在同一 tick 上。
    int minDelayTicks = 2;
    int maxDelayTicks = 8;
};

struct UnloadedEntitiesCleanerConfig {
    bool                     enable                      = false;
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

// 资源包共存：让服务器材质包和玩家自己启用的材质包同时生效。
//
// 原版把「必须下载服务器材质包」和「进服后禁用玩家自己的材质包」绑成了一件事，
// 三个开关分别对应拆开它所需要改的三个封包字段，可以单独关掉任意一项：
//   forceRequired    -> ResourcePacksInfoPacket 里的 mResourcePackRequired，
//                       置 true 让客户端必须下载服务器的包。
//   markAsAddonPack  -> 每个 PackInfoData 的 mIsAddonPack，置 true 让客户端
//                       把它们当作服务器附带内容，而不是玩家自选的材质包。
//   allowClientPacks -> StartGamePacket 里 LevelSettings 的 mTexturePacksRequired，
//                       置 false 让客户端不要因为「世界要求材质包」而踢掉玩家自己的包。
struct CoResourcePackConfig {
    bool enable           = false;
    bool forceRequired    = true;
    bool markAsAddonPack  = true;
    bool allowClientPacks = true;
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

struct MobBlockDestructionConfig {
    bool enable  = false;
    bool wither  = true;
    bool ravager = true;
};

struct EntityCleanerScheduleConfig {
    bool        enable = false;
    std::string cron   = "0 */10 * * * *"; // 每 10 分钟
};

struct EntityCleanerThresholdConfig {
    bool enable = true;
    // 阈值比较的是「候选实体数」而非世界里的实体总数：后者含玩家和所有受保护实体，
    // 用它做判据会让「2500 个受保护村民 + 500 只怪」的服务器永远触发、永远清不到目标。
    int maxEntities        = 1000;
    int targetEntities     = 500;
    int checkIntervalTicks = 100; // 5 秒检查一次
    int cooldownSeconds    = 300; // 两次阈值清理之间的最小间隔
    // 最近一次服务端 tick 的耗时上限，单位为毫秒；0 或负数表示关闭 MSPT 触发。
    double maxMspt = 0.0;
};

struct EntityCleanerDroppedItemConfig {
    // 掉落物 age 大于此值时视为刚生成并予以保护；age 从 6000 向下递减。0 表示关闭。
    int protectAboveAge = 5900;
    // 完整物品标识白名单（例如 minecraft:diamond），白名单内物品始终保留。
    std::vector<std::string> whitelist = {
        "minecraft:netherite_helmet",   "minecraft:netherite_sword",    "minecraft:netherite_chestplate",
        "minecraft:diamond_helmet",     "minecraft:netherite_leggings", "minecraft:undyed_shulker_box",
        "minecraft:netherite_boots",    "minecraft:shulker_box",        "minecraft:elytra",
        "minecraft:dragon_egg",         "minecraft:nether_star",        "minecraft:diamond_sword",
        "minecraft:diamond_chestplate", "minecraft:diamond_boots",      "minecraft:diamond_leggings",
        "minecraft:diamond_hoe",        "minecraft:netherite_hoe",      "minecraft:diamond_axe",
        "minecraft:netherite_axe",      "minecraft:netherite_pickaxe",  "minecraft:diamond_pickaxe"
    };
};

// mode 语义：
//   "whitelist" -> 只清理名单内的类型/family
//   "blacklist" -> 名单内受保护，其余全部清理
// 默认取 whitelist：名单写错或清空只会导致「什么都不删」，而 blacklist 的同类错误
// 意味着「删除一切未列出的」。
struct EntityCleanerFilterConfig {
    std::string              mode      = "whitelist";
    // 生存服默认只清理经验球和箭矢；掉落物按原版时间消失，生物不自动清理。
    std::vector<std::string> typeNames = {"minecraft:xp_orb", "minecraft:arrow"};
    std::vector<std::string> families  = {};
};

// 按实体的运行时状态保护，与类型无关，两种 mode 下都生效。
struct EntityCleanerProtectionConfig {
    bool  named               = true;
    bool  tamed               = true;
    bool  leashed             = true;
    bool  vehicles            = true;
    bool  persistent          = true;
    bool  villagers           = true;
    bool  builds              = true;
    bool  bosses              = true;
    bool  fallingBlocks       = true;
    bool  movingBlocks        = true;
    bool  fishingHooks        = true;
    bool  ominousItemSpawners = true;
    float playerRadius        = 32.0F; // 0 = 关闭
};

struct EntityCleanerWarningConfig {
    bool             enable        = true;
    std::vector<int> secondsBefore = {15};
    // 空字符串使用当前语言的默认提示；支持 {seconds} / {count} 和 Minecraft 颜色代码。
    std::string message         = {};
    bool        broadcastResult = true;
    std::string resultMessage   = {};
};

struct EntityCleanerConfig {
    bool                           enable = true;
    bool                           dryRun = false; // 只统计并记录日志，不实际删除
    EntityCleanerScheduleConfig    schedule{};
    EntityCleanerThresholdConfig   threshold{};
    EntityCleanerDroppedItemConfig droppedItem{};
    EntityCleanerFilterConfig      filter{};
    EntityCleanerProtectionConfig  protection{};
    EntityCleanerWarningConfig     warning{};
    bool                           logResult = true;
};

struct Config {
    int                           version                  = 23;
    ll::io::LogLevel              logLevel                 = ll::io::LogLevel::Info;
    std::string                   language                 = "zh_CN";
    bool                          trustedSkinEnable        = true;
    bool                          autoFishingEnable        = false;
    bool                          abilityCommandEnable     = true;
    bool                          msptCommandEnable        = true;
    bool                          cleanUnknownBlocksEnable = false;
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
    CoResourcePackConfig          coResourcePack{};
    ExplosionControlConfig         explosionControl{};
    MobBlockDestructionConfig      mobBlockDestruction{};
    PistonControlConfig            pistonControl{};
    EntityCleanerConfig           entityCleaner{};
};

} // namespace my_mod
