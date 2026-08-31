#include "mod/MsptCommand.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/command/Command.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "mc/profile/ProfilerLite.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"

#include <algorithm>
#include <chrono>
#include <exception>

namespace my_mod::mspt_command {
namespace {

bool commandRegistered = false;

double currentMspt() {
    auto const tickTime = ProfilerLite::gProfilerLiteInstance().getServerTickTime();
    if (tickTime <= std::chrono::nanoseconds::zero()) {
        return 0.0;
    }
    return std::chrono::duration<double, std::milli>(tickTime).count();
}

void executeCommand(CommandOutput& output) {
    auto const mspt = currentMspt();
    auto const tps  = mspt > 0.0 ? std::min(20.0, 1000.0 / mspt) : 20.0;
    output.success(tr("mspt_command.result", mspt, tps));
}

bool registerCommand() {
    if (commandRegistered) {
        return true;
    }

    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "mspt",
        tr("mspt_command.description"),
        CommandPermissionLevel::Any
    );
    command.overload<ll::command::EmptyParam>().execute(
        [](CommandOrigin const&, CommandOutput& output) { executeCommand(output); }
    );
    commandRegistered = true;
    logger.info(tr("mspt_command.registered"));
    return true;
}

} // namespace

bool enable() {
    if (!config.msptCommandEnable || commandRegistered) {
        return true;
    }

    try {
        return registerCommand();
    } catch (std::exception const& exception) {
        logger.error("MSPT command registration failed: {}", exception.what());
        return false;
    }
}

void disable() {
    commandRegistered = false;
}

} // namespace my_mod::mspt_command

#else

namespace my_mod::mspt_command {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::mspt_command

#endif
