#include "mod/AbilityCommand.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/server/commands/CommandRegistry.h"
#include "mc/server/commands/edu/AbilityCommand.h"
#include "mc/server/module/VanillaGameModuleServer.h"

#include <atomic>

namespace my_mod::ability_command {
namespace {

std::atomic_bool enabled{false};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    VanillaGameModuleServerSetupCommandsHook,
    ll::memory::HookPriority::Normal,
    VanillaGameModuleServer,
    &VanillaGameModuleServer::$setupCommands,
    void,
    CommandRegistry& registry
) {
    origin(registry);

    if (!enabled.load(std::memory_order_relaxed) || registry.findCommand("ability") != nullptr) {
        return;
    }

    const bool isEduMode = registry.mIsEduMode;
    registry.mIsEduMode  = true;
    AbilityCommand::setup(registry);
    registry.mIsEduMode = isEduMode;

    if (registry.findCommand("ability") != nullptr) {
        logger.info(tr("ability_command.registered"));
    } else {
        logger.error(tr("ability_command.register_failed"));
    }
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.abilityCommandEnable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }

    if (VanillaGameModuleServerSetupCommandsHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    VanillaGameModuleServerSetupCommandsHook::unhook();
}

} // namespace my_mod::ability_command

#else

namespace my_mod::ability_command {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::ability_command

#endif
