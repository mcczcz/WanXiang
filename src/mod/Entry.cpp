#include "mod/Entry.h"
#include "mod/FeatureDiagnostics.h"

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
#include <exception>
#include <filesystem>
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
    configPath /= "config.jsonc";
    configPath.make_preferred();

    if (!ConfigManager::getInstance().load(configPath.string())) {
        getSelf().getLogger().error(tr("config.load_error"));
        return false;
    }

    // 根据配置更新语言
    I18n::getInstance().setLanguage(config.language);

    if (!feature_diagnostics::enable("command_permission", command_permission::enable)) {
        return false;
    }

    if (!feature_diagnostics::enable("ability_command", ability_command::enable)) {
        command_permission::disable();
        return false;
    }
    logger.info(tr("plugin.loaded"));
    return true;
}

bool Entry::enable() {
    getSelf().getLogger().debug("Enabling...");

    if (!feature_diagnostics::enable("trusted_skin", trusted_skin::enable)) {
        return false;
    }
    if (!feature_diagnostics::enable("auto_fishing", auto_fishing::enable)) {
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("xp_orb_merge", xp_orb_merge::enable)) {
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("mob_effect_blocker", mob_effect_blocker::enable)) {
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("fast_leaf_decay", fast_leaf_decay::enable)) {
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("clean_unknown_blocks", clean_unknown_blocks::enable)) {
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("unloaded_entities_cleaner", unloaded_entities_cleaner::enable)) {
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("hide_op_permission", hide_op_permission::enable)) {
        unloaded_entities_cleaner::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("scheduled_command", scheduled_command::enable)) {
        unloaded_entities_cleaner::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("server_pong", server_pong::enable)) {
        unloaded_entities_cleaner::disable();
        scheduled_command::disable();
        hide_op_permission::disable();
        clean_unknown_blocks::disable();
        fast_leaf_decay::disable();
        mob_effect_blocker::disable();
        xp_orb_merge::disable();
        auto_fishing::disable();
        trusted_skin::disable();
        return false;
    }
    if (!feature_diagnostics::enable("explosion_control", explosion_control::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("client_level_name", client_level_name::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("dropped_item_lifetime", dropped_item_lifetime::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("entity_cleaner", entity_cleaner::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("entity_cleaner_command", entity_cleaner_command::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("mspt_command", mspt_command::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("co_resource_pack", co_resource_pack::enable)) {
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
        return false;
    }
    if (!feature_diagnostics::enable("mob_block_destruction", mob_block_destruction::enable)) {
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
        return false;
    }

    if (!feature_diagnostics::enable("piston_control", piston_control::enable)) {
        disable();
        return false;
    }
    piston_control::applyBlockConfig();

    if (!feature_diagnostics::enable("custom_knockback", custom_knockback::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("custom_hurt_interval", custom_hurt_interval::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("sidebar", sidebar::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("reload_command", reload_command::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("vanish", vanish::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("auto_replenish", auto_replenish::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("client_achievements", client_achievements::enable)) {
        disable();
        return false;
    }

    if (!feature_diagnostics::enable("chat_formatter", chat_formatter::enable)) {
        disable();
        return false;
    }

    logger.info(tr("plugin.enabled"));
    return true;
}

bool Entry::reload() {
    mLastReloadError.clear();
    feature_diagnostics::lastError.clear();
    struct Feature {
        bool (*enable)();
        void (*disable)();
        const char* name;
    };
    // Command registration, startup database cleanup and piston block rules require a server restart.
    static constexpr Feature features[] = {
        {trusted_skin::enable,          trusted_skin::disable,          "trusted_skin"         },
        {auto_fishing::enable,          auto_fishing::disable,          "auto_fishing"         },
        {auto_replenish::enable,        auto_replenish::disable,        "auto_replenish"       },
        {xp_orb_merge::enable,          xp_orb_merge::disable,          "xp_orb_merge"         },
        {mob_effect_blocker::enable,    mob_effect_blocker::disable,    "mob_effect_blocker"   },
        {fast_leaf_decay::enable,       fast_leaf_decay::disable,       "fast_leaf_decay"      },
        {clean_unknown_blocks::enable,  clean_unknown_blocks::disable,  "clean_unknown_blocks" },
        {hide_op_permission::enable,    hide_op_permission::disable,    "hide_op_permission"   },
        {scheduled_command::enable,     scheduled_command::disable,     "scheduled_command"    },
        {server_pong::enable,           server_pong::disable,           "server_pong"          },
        {explosion_control::enable,     explosion_control::disable,     "explosion_control"    },
        {client_level_name::enable,     client_level_name::disable,     "client_level_name"    },
        {client_achievements::enable,   client_achievements::disable,   "client_achievements"  },
        {dropped_item_lifetime::enable, dropped_item_lifetime::disable, "dropped_item_lifetime"},
        {entity_cleaner::enable,        entity_cleaner::disable,        "entity_cleaner"       },
        {co_resource_pack::enable,      co_resource_pack::disable,      "co_resource_pack"     },
        {mob_block_destruction::enable, mob_block_destruction::disable, "mob_block_destruction"},
        {piston_control::enable,        piston_control::disable,        "piston_control"       },
        {custom_knockback::enable,      custom_knockback::disable,      "custom_knockback"     },
        {custom_hurt_interval::enable,  custom_hurt_interval::disable,  "custom_hurt_interval" },
        {sidebar::enable,               sidebar::disable,               "sidebar"              },
        {vanish::enable,                vanish::disable,                "vanish"               },
        {chat_formatter::enable,        chat_formatter::disable,        "chat_formatter"       },
    };

    auto stopFeatures = [&] {
        for (auto it = std::rbegin(features); it != std::rend(features); ++it) {
            it->disable();
        }
    };
    auto startFeatures = [&] {
        for (auto const& feature : features) {
            if (!feature_diagnostics::enable(feature.name, feature.enable)) {
                return false;
            }
        }
        return true;
    };

    Config     previousConfig   = config;
    auto const previousLanguage = I18n::getInstance().getLanguage();
    if (!ConfigManager::getInstance().reload()) {
        mLastReloadError = feature_diagnostics::lastError;
        return false;
    }

    stopFeatures();
    I18n::getInstance().setLanguage(config.language);
    if (startFeatures()) {
        logger.info(tr("reload_command.success"));
        return true;
    }

    mLastReloadError = feature_diagnostics::lastError;
    stopFeatures();
    config = std::move(previousConfig);
    logger.setLevel(config.logLevel);
    I18n::getInstance().setLanguage(previousLanguage);
    if (!startFeatures()) {
        stopFeatures();
        mLastReloadError += "\n" + tr("reload_command.restore_failed") + "\n" + feature_diagnostics::lastError;
        logger.error(tr("reload_command.restore_failed"));
    } else {
        mLastReloadError += "\n" + tr("reload_command.restored");
    }
    return false;
}

bool Entry::disable() {
    getSelf().getLogger().debug("Disabling...");
    chat_formatter::disable();
    client_achievements::disable();
    auto_replenish::disable();
    vanish::disable();
    sidebar::disable();
    reload_command::disable();
    custom_hurt_interval::disable();
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
    chat_formatter::disable();
    client_achievements::disable();
    auto_replenish::disable();
    vanish::disable();
    sidebar::disable();
    reload_command::disable();
    custom_hurt_interval::disable();
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
