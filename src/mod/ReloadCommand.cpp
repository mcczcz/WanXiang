#include "mod/ReloadCommand.h"

#ifdef LL_PLAT_S

#include "mod/Entry.h"
#include "mod/FeatureDiagnostics.h"
#include "mod/Global.h"

#include "ll/api/command/Command.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"

#include <exception>

namespace my_mod::reload_command {
namespace {

bool commandRegistered = false;
bool active = false;

void executeCommand(CommandOrigin const& origin, CommandOutput& output) {
    // Permission overrides may lower the registry requirement; enforce it here too.
    if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors) {
        output.error(tr("reload_command.op_only"));
        return;
    }
    if (!active) {
        output.error(tr("reload_command.disabled"));
        return;
    }

    try {
        if (Entry::getInstance().reload()) {
            output.success(tr("reload_command.success"));
            output.success(tr("reload_command.restart_hint"));
        } else {
            auto const& reason = Entry::getInstance().getLastReloadError();
            output.error(reason.empty() ? tr("reload_command.failed") : tr("reload_command.failed_detail", reason));
        }
    } catch (std::exception const& exception) {
        auto const message = tr("reload_command.exception", exception.what());
        logger.error(message);
        output.error(message);
    }
}

} // namespace

bool enable() {
    if (commandRegistered) {
        active = true;
        return true;
    }

    try {
        auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
            "wanxiang",
            tr("reload_command.description"),
            CommandPermissionLevel::GameDirectors
        );
        command.overload<ll::command::EmptyParam>().text("reload").execute(
            [](CommandOrigin const& origin, CommandOutput& output) { executeCommand(origin, output); }
        );
        commandRegistered = true;
        active = true;
        return true;
    } catch (std::exception const& exception) {
        return feature_diagnostics::exception("reload_command", "feature.step.command", exception.what());
    }
}

void disable() {
    active = false;
}

} // namespace my_mod::reload_command

#else

namespace my_mod::reload_command {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::reload_command

#endif
