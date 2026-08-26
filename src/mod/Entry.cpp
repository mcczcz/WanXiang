#include "mod/Entry.h"

#include "Config/ConfigManager.h"
#include "I18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mod/AbilityCommand.h"
#include "mod/AutoFishing.h"
#include "mod/MobEffectBlocker.h"
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

    if (!trusted_skin::enable()) {
        logger.error(tr("trusted_skin.enable_failed"));
        return false;
    }
    if (!auto_fishing::enable()) {
        trusted_skin::disable();
        logger.error(tr("auto_fishing.enable_failed"));
        return false;
    }
    if (!xp_orb_merge::enable()) {
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("xp_orb_merge.enable_failed"));
        return false;
    }
    if (!mob_effect_blocker::enable()) {
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("mob_effect_blocker.enable_failed"));
        return false;
    }

    logger.info(tr("plugin.enabled"));
    return true;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    mob_effect_blocker::disable();
    xp_orb_merge::disable();
    auto_fishing::disable();
    trusted_skin::disable();
    ability_command::disable();
    logger.info(tr("plugin.unloaded"));
    return true;
}

bool Entry::unload() {
    getSelf().getLogger().debug("Unloading...");
    mob_effect_blocker::disable();
    xp_orb_merge::disable();
    auto_fishing::disable();
    trusted_skin::disable();
    ability_command::disable();
    return true;
}

LL_REGISTER_MOD(my_mod::Entry, my_mod::Entry::getInstance());
} // namespace my_mod
