#include "mod/Entry.h"

#include "Config/ConfigManager.h"
#include "I18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mod/AbilityCommand.h"
#include "mod/AutoFishing.h"
#include "mod/CleanUnknownBlocks.h"
#include "mod/FastLeafDecay.h"
#include "mod/MobEffectBlocker.h"
#include "mod/ResourcePackOverlay.h"
#include "mod/ScheduledCommand.h"
#include "mod/TrustedSkin.h"
#include "mod/XpOrbMerge.h"
#include <filesystem>

namespace my_mod {
ll::io::Logger& logger = Entry::getInstance().getSelf().getLogger();
Config&         config = ConfigManager::getInstance().get();

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
    configPath /= "config.json";
    configPath.make_preferred();

    if (!ConfigManager::getInstance().load(configPath.string())) {
        getSelf().getLogger().error(tr("config.load_error"));
        return false;
    }

    // 根据配置更新语言
    I18n::getInstance().setLanguage(config.language);

    if (!ability_command::enable()) {
        logger.error(tr("ability_command.enable_failed"));
        return false;
    }

    logger.info(tr("plugin.loaded"));
    return true;
}

bool Entry::enable() {
    getSelf().getLogger().debug("Enabling...");

    if (!resource_pack_overlay::enable()) {
        logger.error(tr("resource_pack_overlay.enable_failed"));
        return false;
    }
    if (!trusted_skin::enable()) {
        resource_pack_overlay::disable();
        logger.error(tr("trusted_skin.enable_failed"));
        return false;
    }
    if (!auto_fishing::enable()) {
        trusted_skin::disable();
        resource_pack_overlay::disable();
        logger.error(tr("auto_fishing.enable_failed"));
        return false;
    }
    if (!xp_orb_merge::enable()) {
        auto_fishing::disable();
        trusted_skin::disable();
        resource_pack_overlay::disable();
        logger.error(tr("xp_orb_merge.enable_failed"));
        return false;
    }
    if (!mob_effect_blocker::enable()) {
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        resource_pack_overlay::disable();
        logger.error(tr("mob_effect_blocker.enable_failed"));
        return false;
    }
    if (!fast_leaf_decay::enable()) {
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        resource_pack_overlay::disable();
        logger.error(tr("fast_leaf_decay.enable_failed"));
        return false;
    }
    if (!clean_unknown_blocks::enable()) {
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        resource_pack_overlay::disable();
        logger.error(tr("clean_unknown_blocks.enable_failed"));
        return false;
    }
    if (!scheduled_command::enable()) {
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        resource_pack_overlay::disable();
        logger.error(tr("scheduled_command.enable_failed"));
        return false;
    }

    logger.info(tr("plugin.enabled"));
    return true;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    scheduled_command::disable();
    clean_unknown_blocks::disable();
    fast_leaf_decay::disable();
    mob_effect_blocker::disable();
    xp_orb_merge::disable();
    auto_fishing::disable();
    trusted_skin::disable();
    resource_pack_overlay::disable();
    ability_command::disable();
    logger.info(tr("plugin.unloaded"));
    return true;
}

bool Entry::unload() {
    getSelf().getLogger().debug("Unloading...");
    scheduled_command::disable();
    clean_unknown_blocks::disable();
    fast_leaf_decay::disable();
    mob_effect_blocker::disable();
    xp_orb_merge::disable();
    auto_fishing::disable();
    trusted_skin::disable();
    resource_pack_overlay::disable();
    ability_command::disable();
    return true;
}

LL_REGISTER_MOD(my_mod::Entry, my_mod::Entry::getInstance());
} // namespace my_mod
