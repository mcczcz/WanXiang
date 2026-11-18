#include "mod/Sidebar.h"

#ifdef LL_PLAT_S

#include "mod/Entry.h"
#include "mod/FeatureDiagnostics.h"
#include "mod/Global.h"
#include "sidebar/DatabaseManager.h"
#include "sidebar/SidebarManager.h"
#include "sidebar/sidebar.h"

#include "ll/api/command/Command.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Player.h"

#include <exception>
#include <filesystem>

namespace my_mod::sidebar {
namespace {

bool commandRegistered = false;
bool commandActive     = false;
bool running           = false;

void setStatus(CommandOrigin const& origin, CommandOutput& output, bool enabled) {
    if (!running) {
        output.error(tr("sidebar.disabled"));
        return;
    }
    auto* entity = origin.getEntity();
    if (!entity || !entity->isPlayer()) {
        output.error(tr("sidebar.player_only"));
        return;
    }
    auto* player = static_cast<Player*>(entity);
    auto  uuid   = player->getUuid();
    if (!DatabaseManager::getInstance().setPlayerSidebarStatus(uuid, enabled)) {
        output.error(tr("sidebar.status_failed"));
        return;
    }
    SidebarManager::getInstance().setPlayerSidebarStatus(uuid.asString(), enabled);
    if (!enabled) {
        removeSidebar(player);
    }
    output.success(tr(enabled ? "sidebar.on" : "sidebar.off"));
}

void registerCommands() {
    if (commandRegistered) {
        return;
    }
    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "sidebar", tr("sidebar.description"), CommandPermissionLevel::Any
    );
    command.overload<ll::command::EmptyParam>().text("on").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { setStatus(origin, output, true); }
    );
    command.overload<ll::command::EmptyParam>().text("off").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { setStatus(origin, output, false); }
    );
    command.overload<ll::command::EmptyParam>().text("reload").execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors) {
                output.error(tr("reload_command.op_only"));
                return;
            }
            if (!commandActive) {
                output.error(tr("reload_command.disabled"));
                return;
            }
            try {
                if (Entry::getInstance().reload()) {
                    output.success(tr("reload_command.success"));
                    output.success(tr("reload_command.restart_hint"));
                } else {
                    auto const& reason = Entry::getInstance().getLastReloadError();
                    output.error(
                        reason.empty() ? tr("reload_command.failed") : tr("reload_command.failed_detail", reason)
                    );
                }
            } catch (std::exception const& exception) {
                auto const message = tr("reload_command.exception", exception.what());
                logger.error(message);
                output.error(message);
            }
        }
    );
    commandRegistered = true;
}

} // namespace

bool enable() {
    char const* step = "feature.step.command";
    try {
        registerCommands();
        commandActive = true;
        if (!config.sidebar.enable || running) {
            return true;
        }
        auto const dataDir = Entry::getInstance().getSelf().getDataDir();
        step               = "feature.step.directory";
        std::filesystem::create_directories(dataDir);
        step           = "feature.step.database";
        auto& database = DatabaseManager::getInstance();
        if (!database.openDatabase((dataDir / "sidebar.db").string()) || !database.createTable()) {
            disable();
            return false;
        }
        step = "feature.step.sidebar";
        SidebarManager::getInstance().start();
        running = true;
        return true;
    } catch (std::exception const& exception) {
        disable();
        return feature_diagnostics::exception("sidebar", step, exception.what());
    }
}

void disable() {
    commandActive = false;
    running       = false;
    SidebarManager::getInstance().stop();
    DatabaseManager::getInstance().closeDatabase();
}

} // namespace my_mod::sidebar

#else

namespace my_mod::sidebar {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::sidebar

#endif
