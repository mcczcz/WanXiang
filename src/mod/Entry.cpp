#include "mod/Entry.h"

#include "Config/ConfigManager.h"
#include "I18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mod/AbilityCommand.h"
#include "mod/AutoFishing.h"
#include "mod/CleanUnknownBlocks.h"
#include "mod/ClientLevelName.h"
#include "mod/CoResourcePack.h"
#include "mod/CommandPermission.h"
#include "mod/CustomKnockback.h"
#include "mod/DroppedItemLifetime.h"
#include "mod/EntityCleaner.h"
#include "mod/EntityCleanerCommand.h"
#include "mod/ExplosionControl.h"
#include "mod/FastLeafDecay.h"
#include "mod/HideOpPermission.h"
#include "mod/MobBlockDestruction.h"
#include "mod/MobEffectBlocker.h"
#include "mod/MsptCommand.h"
#include "mod/PistonControl.h"
#include "mod/ReloadCommand.h"
#include "mod/ScheduledCommand.h"
#include "mod/ServerPong.h"
#include "mod/TrustedSkin.h"
#include "mod/UnloadedEntitiesCleaner.h"
#include "mod/XpOrbMerge.h"
#include <filesystem>
#include <exception>
#include <iterator>
#include <utility>

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
    if (!co_resource_pack::enable()) {
        mspt_command::disable();
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
        logger.error(tr("co_resource_pack.enable_failed"));
        return false;
    }
    if (!mob_block_destruction::enable()) {
        co_resource_pack::disable();
        mspt_command::disable();
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
        logger.error(tr("mob_block_destruction.enable_failed"));
        return false;
    }

    if (!piston_control::enable()) {
        disable();
        logger.error(tr("piston_control.enable_failed"));
        return false;
    }
    piston_control::applyBlockConfig();

    if (!custom_knockback::enable()) {
        disable();
        logger.error(tr("custom_knockback.enable_failed"));
        return false;
    }

    if (!reload_command::enable()) {
        disable();
        logger.error(tr("reload_command.enable_failed"));
        return false;
    }

    logger.info(tr("plugin.enabled"));
    return true;
}

bool Entry::reload() {
    struct Feature {
        bool (*enable)();
        void (*disable)();
        const char* name;
    };
    // Command registration, startup database cleanup and piston block rules require a server restart.
    static constexpr Feature features[] = {
        {trusted_skin::enable, trusted_skin::disable, "trustedSkin"},
        {auto_fishing::enable, auto_fishing::disable, "autoFishing"},
        {xp_orb_merge::enable, xp_orb_merge::disable, "xpOrbMerge"},
        {mob_effect_blocker::enable, mob_effect_blocker::disable, "mobEffectBlocker"},
        {fast_leaf_decay::enable, fast_leaf_decay::disable, "fastLeafDecay"},
        {clean_unknown_blocks::enable, clean_unknown_blocks::disable, "cleanUnknownBlocks"},
        {hide_op_permission::enable, hide_op_permission::disable, "hideOpPermission"},
        {scheduled_command::enable, scheduled_command::disable, "scheduledCommands"},
        {server_pong::enable, server_pong::disable, "serverPong"},
        {explosion_control::enable, explosion_control::disable, "explosionControl"},
        {client_level_name::enable, client_level_name::disable, "clientLevelName"},
        {dropped_item_lifetime::enable, dropped_item_lifetime::disable, "droppedItemLifetime"},
        {entity_cleaner::enable, entity_cleaner::disable, "entityCleaner"},
        {co_resource_pack::enable, co_resource_pack::disable, "coResourcePack"},
        {mob_block_destruction::enable, mob_block_destruction::disable, "mobBlockDestruction"},
        {piston_control::enable, piston_control::disable, "pistonControl"},
        {custom_knockback::enable, custom_knockback::disable, "customKnockback"},
    };

    auto stopFeatures = [&] {
        for (auto it = std::rbegin(features); it != std::rend(features); ++it) {
            it->disable();
        }
    };
    auto startFeatures = [&] {
        for (auto const& feature : features) {
            try {
                if (feature.enable()) {
                    continue;
                }
                logger.error(tr("reload_command.feature_failed", feature.name));
            } catch (std::exception const& exception) {
                logger.error(tr("reload_command.feature_exception", feature.name, exception.what()));
            }
            return false;
        }
        return true;
    };

    Config previousConfig = config;
    auto const previousLanguage = I18n::getInstance().getLanguage();
    if (!ConfigManager::getInstance().reload()) {
        return false;
    }

    stopFeatures();
    I18n::getInstance().setLanguage(config.language);
    if (startFeatures()) {
        logger.info(tr("reload_command.success"));
        return true;
    }

    stopFeatures();
    config = std::move(previousConfig);
    logger.setLevel(config.logLevel);
    I18n::getInstance().setLanguage(previousLanguage);
    if (!startFeatures()) {
        stopFeatures();
        logger.error(tr("reload_command.restore_failed"));
    }
    return false;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    reload_command::disable();
    custom_knockback::disable();
    piston_control::disable();
    mob_block_destruction::disable();
    co_resource_pack::disable();
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
    reload_command::disable();
    custom_knockback::disable();
    piston_control::disable();
    mob_block_destruction::disable();
    co_resource_pack::disable();
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
