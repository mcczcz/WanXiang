#include "Entry.h"

#include "core/config/ConfigManager.h"
#include "core/i18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#include "features/commands/AbilityCommand.h"
#include "features/player/AutoFishing.h"
#include "features/player/AutoReplenish.h"
#include "features/player/ContainerPreview.h"
#include "features/chat/ChatFormatter.h"
#include "features/world/CleanUnknownBlocks.h"
#include "features/client/ClientAchievements.h"
#include "features/client/ClientLevelName.h"
#include "features/client/CoResourcePack.h"
#include "features/commands/CommandPermission.h"
#include "features/combat/CustomKnockback.h"
#include "features/combat/CustomHurtInterval.h"
#include "features/entities/DroppedItemLifetime.h"
#include "features/entities/cleanup/EntityCleaner.h"
#include "features/entities/cleanup/EntityCleanerCommand.h"
#include "features/world/ExplosionControl.h"
#include "features/world/FastLeafDecay.h"
#include "features/world/QuickHarvest.h"
#include "FeatureDiagnostics.h"
#include "features/client/HideOpPermission.h"
#include "features/world/MobBlockDestruction.h"
#include "features/combat/MobEffectBlocker.h"
#include "features/commands/MsptCommand.h"
#include "features/world/piston/PistonControl.h"
#include "features/commands/ReloadCommand.h"
#include "features/commands/ScheduledCommand.h"
#include "features/server/ServerPong.h"
#include "features/sidebar/Sidebar.h"
#include "features/client/TrustedSkin.h"
#include "features/entities/cleanup/UnloadedEntitiesCleaner.h"
#include "features/player/Vanish.h"
#include "features/entities/XpOrbMerge.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <utility>

namespace my_mod {
ll::io::Logger& logger = Entry::getInstance().getSelf().getLogger();
Config&         config = ConfigManager::getInstance().get();

namespace {

struct Feature {
    char const* name;
    bool (*enable)();
    void (*disable)();
};

bool enablePistonWithBlockConfig() {
    if (!piston_control::enable()) {
        return false;
    }
    piston_control::applyBlockConfig();
    return true;
}

#define WANXIANG_FEATURE(module) {#module, module::enable, module::disable}

constexpr Feature loadFeatures[] = {
    WANXIANG_FEATURE(command_permission),
    WANXIANG_FEATURE(ability_command),
};

constexpr Feature runtimeFeatures[] = {
    WANXIANG_FEATURE(trusted_skin),
    WANXIANG_FEATURE(auto_fishing),
    WANXIANG_FEATURE(xp_orb_merge),
    WANXIANG_FEATURE(mob_effect_blocker),
    WANXIANG_FEATURE(fast_leaf_decay),
    WANXIANG_FEATURE(quick_harvest),
    WANXIANG_FEATURE(clean_unknown_blocks),
    WANXIANG_FEATURE(unloaded_entities_cleaner),
    WANXIANG_FEATURE(hide_op_permission),
    WANXIANG_FEATURE(scheduled_command),
    WANXIANG_FEATURE(server_pong),
    WANXIANG_FEATURE(explosion_control),
    WANXIANG_FEATURE(client_level_name),
    WANXIANG_FEATURE(dropped_item_lifetime),
    WANXIANG_FEATURE(entity_cleaner),
    WANXIANG_FEATURE(entity_cleaner_command),
    WANXIANG_FEATURE(mspt_command),
    WANXIANG_FEATURE(co_resource_pack),
    WANXIANG_FEATURE(mob_block_destruction),
    {"piston_control", enablePistonWithBlockConfig, piston_control::disable},
    WANXIANG_FEATURE(custom_knockback),
    WANXIANG_FEATURE(custom_hurt_interval),
    WANXIANG_FEATURE(sidebar),
    WANXIANG_FEATURE(reload_command),
    WANXIANG_FEATURE(vanish),
    WANXIANG_FEATURE(auto_replenish),
    WANXIANG_FEATURE(container_preview),
    WANXIANG_FEATURE(client_achievements),
    WANXIANG_FEATURE(chat_formatter),
};

// 命令注册、启动时的数据库清理和活塞方块规则只在启动阶段执行。
constexpr Feature reloadFeatures[] = {
    WANXIANG_FEATURE(trusted_skin),
    WANXIANG_FEATURE(auto_fishing),
    WANXIANG_FEATURE(auto_replenish),
    WANXIANG_FEATURE(container_preview),
    WANXIANG_FEATURE(xp_orb_merge),
    WANXIANG_FEATURE(mob_effect_blocker),
    WANXIANG_FEATURE(fast_leaf_decay),
    WANXIANG_FEATURE(quick_harvest),
    WANXIANG_FEATURE(clean_unknown_blocks),
    WANXIANG_FEATURE(hide_op_permission),
    WANXIANG_FEATURE(scheduled_command),
    WANXIANG_FEATURE(server_pong),
    WANXIANG_FEATURE(explosion_control),
    WANXIANG_FEATURE(client_level_name),
    WANXIANG_FEATURE(client_achievements),
    WANXIANG_FEATURE(dropped_item_lifetime),
    WANXIANG_FEATURE(entity_cleaner),
    WANXIANG_FEATURE(co_resource_pack),
    WANXIANG_FEATURE(mob_block_destruction),
    WANXIANG_FEATURE(piston_control),
    WANXIANG_FEATURE(custom_knockback),
    WANXIANG_FEATURE(custom_hurt_interval),
    WANXIANG_FEATURE(sidebar),
    WANXIANG_FEATURE(vanish),
    WANXIANG_FEATURE(chat_formatter),
};

#undef WANXIANG_FEATURE

void stopFeatures(std::span<Feature const> features) {
    for (auto it = features.rbegin(); it != features.rend(); ++it) {
        it->disable();
    }
}

bool startFeatures(std::span<Feature const> features) {
    for (std::size_t i = 0; i < features.size(); ++i) {
        auto const& feature = features[i];
        if (!feature_diagnostics::enable(feature.name, feature.enable)) {
            // 当前功能也可能已经安装了部分钩子或监听器。
            stopFeatures(features.first(i + 1));
            return false;
        }
    }
    return true;
}

bool loadFeaturesStarted = false;

bool startLoadFeatures() {
    if (!loadFeaturesStarted) {
        loadFeaturesStarted = startFeatures(loadFeatures);
    }
    return loadFeaturesStarted;
}

void stopLoadFeatures() {
    if (loadFeaturesStarted) {
        stopFeatures(loadFeatures);
        loadFeaturesStarted = false;
    }
}

} // namespace

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
    getSelf().getLogger().debug("Loading...");

    // 先初始化 I18n（使用默认语言）
    auto langPath = getSelf().getLangDir();
    if (!I18n::getInstance().load(langPath.string(), "zh_CN")) {
        logger.warn(tr("i18n.load_partial"));
    }

    auto configPath = getSelf().getConfigDir();
    if (!std::filesystem::exists(configPath)) {
        std::filesystem::create_directories(configPath);
    }
    configPath /= "config.jsonc";
    configPath.make_preferred();

    if (!ConfigManager::getInstance().load(configPath.string())) {
        getSelf().getLogger().error(tr("config.load_error"));
        return false;
    }

    // 根据配置更新语言
    I18n::getInstance().setLanguage(config.language);

    if (!startLoadFeatures()) {
        return false;
    }
    logger.info(tr("plugin.loaded"));
    return true;
}

bool Entry::enable() {
    getSelf().getLogger().debug("Enabling...");

    if (!startLoadFeatures()) {
        return false;
    }
    if (!startFeatures(runtimeFeatures)) {
        stopLoadFeatures();
        return false;
    }

    logger.info(tr("plugin.enabled"));
    return true;
}

bool Entry::reload() {
    mLastReloadError.clear();
    feature_diagnostics::lastError.clear();

    Config     previousConfig   = config;
    auto const previousLanguage = I18n::getInstance().getLanguage();
    if (!ConfigManager::getInstance().reload()) {
        mLastReloadError = feature_diagnostics::lastError;
        return false;
    }

    stopFeatures(reloadFeatures);
    I18n::getInstance().setLanguage(config.language);
    if (startFeatures(reloadFeatures)) {
        logger.info(tr("reload_command.success"));
        return true;
    }

    mLastReloadError = feature_diagnostics::lastError;
    config = std::move(previousConfig);
    logger.setLevel(config.logLevel);
    I18n::getInstance().setLanguage(previousLanguage);
    if (!startFeatures(reloadFeatures)) {
        mLastReloadError += "\n" + tr("reload_command.restore_failed") + "\n" + feature_diagnostics::lastError;
        logger.error(tr("reload_command.restore_failed"));
    } else {
        mLastReloadError += "\n" + tr("reload_command.restored");
    }
    return false;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    stopFeatures(runtimeFeatures);
    stopLoadFeatures();
    return true;
}

bool Entry::unload() {
    getSelf().getLogger().debug("Unloading...");
    // 只完成 load() 就卸载时，LL 会跳过 disable()。
    stopLoadFeatures();
    logger.info(tr("plugin.unloaded"));
    return true;
}

LL_REGISTER_MOD(my_mod::Entry, my_mod::Entry::getInstance());
} // namespace my_mod
