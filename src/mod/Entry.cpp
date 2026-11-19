#include "mod/Entry.h"

#include "Config/ConfigManager.h"
#include "I18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mod/AbilityCommand.h"
#include "mod/AutoFishing.h"
#include "mod/AutoReplenish.h"
#include "mod/ChatFormatter.h"
#include "mod/CleanUnknownBlocks.h"
#include "mod/ClientAchievements.h"
#include "mod/ClientLevelName.h"
#include "mod/CoResourcePack.h"
#include "mod/CommandPermission.h"
#include "mod/CustomKnockback.h"
#include "mod/CustomHurtInterval.h"
#include "mod/DroppedItemLifetime.h"
#include "mod/EntityCleaner.h"
#include "mod/EntityCleanerCommand.h"
#include "mod/ExplosionControl.h"
#include "mod/FastLeafDecay.h"
#include "mod/FeatureDiagnostics.h"
#include "mod/HideOpPermission.h"
#include "mod/MobBlockDestruction.h"
#include "mod/MobEffectBlocker.h"
#include "mod/MsptCommand.h"
#include "mod/PistonControl.h"
#include "mod/ReloadCommand.h"
#include "mod/ScheduledCommand.h"
#include "mod/ServerPong.h"
#include "mod/Sidebar.h"
#include "mod/TrustedSkin.h"
#include "mod/UnloadedEntitiesCleaner.h"
#include "mod/Vanish.h"
#include "mod/XpOrbMerge.h"

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
    WANXIANG_FEATURE(client_achievements),
    WANXIANG_FEATURE(chat_formatter),
};

// 命令注册、启动时的数据库清理和活塞方块规则只在启动阶段执行。
constexpr Feature reloadFeatures[] = {
    WANXIANG_FEATURE(trusted_skin),
    WANXIANG_FEATURE(auto_fishing),
    WANXIANG_FEATURE(auto_replenish),
    WANXIANG_FEATURE(xp_orb_merge),
    WANXIANG_FEATURE(mob_effect_blocker),
    WANXIANG_FEATURE(fast_leaf_decay),
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
