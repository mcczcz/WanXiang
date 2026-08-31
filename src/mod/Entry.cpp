#include "mod/Entry.h"

#include "Config/ConfigManager.h"
#include "I18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mod/AbilityCommand.h"
#include "mod/AutoFishing.h"
#include "mod/CleanUnknownBlocks.h"
#include "mod/ClientLevelName.h"
#include "mod/CommandPermission.h"
#include "mod/DroppedItemLifetime.h"
#include "mod/EntityCleaner.h"
#include "mod/EntityCleanerCommand.h"
#include "mod/ExplosionControl.h"
#include "mod/FastLeafDecay.h"
#include "mod/HideOpPermission.h"
#include "mod/MobEffectBlocker.h"
#include "mod/MsptCommand.h"
#include "mod/ScheduledCommand.h"
#include "mod/ServerPong.h"
#include "mod/TrustedSkin.h"
#include "mod/UnloadedEntitiesCleaner.h"
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

    if (!command_permission::enable()) {
        logger.error(tr("command_permission.enable_failed"));
        return false;
    }

    if (!ability_command::enable()) {
        command_permission::disable();
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
    if (!fast_leaf_decay::enable()) {
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("fast_leaf_decay.enable_failed"));
        return false;
    }
    if (!clean_unknown_blocks::enable()) {
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("clean_unknown_blocks.enable_failed"));
        return false;
    }
    if (!unloaded_entities_cleaner::enable()) {
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("unloaded_entities_cleaner.enable_failed"));
        return false;
    }
    if (!hide_op_permission::enable()) {
        unloaded_entities_cleaner::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("hide_op_permission.enable_failed"));
        return false;
    }
    if (!scheduled_command::enable()) {
        unloaded_entities_cleaner::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("scheduled_command.enable_failed"));
        return false;
    }
    if (!server_pong::enable()) {
        unloaded_entities_cleaner::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("server_pong.enable_failed"));
        return false;
    }
    if (!explosion_control::enable()) {
        unloaded_entities_cleaner::disable();
        server_pong::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error("Failed to enable explosion control");
        return false;
    }
    if (!client_level_name::enable()) {
        unloaded_entities_cleaner::disable();
        explosion_control::disable();
        server_pong::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("client_level_name.enable_failed"));
        return false;
    }
    if (!dropped_item_lifetime::enable()) {
        unloaded_entities_cleaner::disable();
        client_level_name::disable();
        explosion_control::disable();
        server_pong::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("dropped_item_lifetime.enable_failed"));
        return false;
    }
    if (!entity_cleaner::enable()) {
        dropped_item_lifetime::disable();
        unloaded_entities_cleaner::disable();
        client_level_name::disable();
        explosion_control::disable();
        server_pong::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("entity_cleaner.enable_failed"));
        return false;
    }
    if (!entity_cleaner_command::enable()) {
        entity_cleaner::disable();
        dropped_item_lifetime::disable();
        unloaded_entities_cleaner::disable();
        client_level_name::disable();
        explosion_control::disable();
        server_pong::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("entity_cleaner_command.enable_failed"));
        return false;
    }
    if (!mspt_command::enable()) {
        entity_cleaner_command::disable();
        entity_cleaner::disable();
        dropped_item_lifetime::disable();
        unloaded_entities_cleaner::disable();
        client_level_name::disable();
        explosion_control::disable();
        server_pong::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        logger.error(tr("mspt_command.enable_failed"));
        return false;
    }

    logger.info(tr("plugin.enabled"));
    return true;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    entity_cleaner::disable();
    dropped_item_lifetime::disable();
    unloaded_entities_cleaner::disable();
    client_level_name::disable();
    explosion_control::disable();
    scheduled_command::disable();
    server_pong::disable();
    hide_op_permission::disable();
    clean_unknown_blocks::disable();
    fast_leaf_decay::disable();
    mob_effect_blocker::disable();
    xp_orb_merge::disable();
    auto_fishing::disable();
    trusted_skin::disable();
    ability_command::disable();
    entity_cleaner_command::disable();
    mspt_command::disable();
    command_permission::disable();
    logger.info(tr("plugin.unloaded"));
    return true;
}

bool Entry::unload() {
    getSelf().getLogger().debug("Unloading...");
    entity_cleaner::disable();
    dropped_item_lifetime::disable();
    unloaded_entities_cleaner::disable();
    client_level_name::disable();
    explosion_control::disable();
    scheduled_command::disable();
    server_pong::disable();
    hide_op_permission::disable();
    clean_unknown_blocks::disable();
    fast_leaf_decay::disable();
    mob_effect_blocker::disable();
    xp_orb_merge::disable();
    auto_fishing::disable();
    trusted_skin::disable();
    ability_command::disable();
    entity_cleaner_command::disable();
    mspt_command::disable();
    command_permission::disable();
    return true;
}

LL_REGISTER_MOD(my_mod::Entry, my_mod::Entry::getInstance());
} // namespace my_mod
