#include "mod/CommandPermission.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CommandRegistry.h"

#include <atomic>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace my_mod::command_permission {
namespace {

std::atomic_bool                                        enabled{false};
std::unordered_map<std::string, CommandPermissionLevel> permissionOverrides;

std::string normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
    return normalized;
}

std::string normalizeCommandName(std::string_view command) {
    if (!command.empty() && command.front() == '/') {
        command.remove_prefix(1);
    }
    return normalize(command);
}

std::optional<CommandPermissionLevel> parsePermissionLevel(std::string_view value) {
    const std::string normalized = normalize(value);
    if (normalized == "any") {
        return CommandPermissionLevel::Any;
    }
    if (normalized == "gamedirectors") {
        return CommandPermissionLevel::GameDirectors;
    }
    if (normalized == "admin") {
        return CommandPermissionLevel::Admin;
    }
    if (normalized == "host") {
        return CommandPermissionLevel::Host;
    }
    if (normalized == "owner") {
        return CommandPermissionLevel::Owner;
    }
    if (normalized == "internal") {
        return CommandPermissionLevel::Internal;
    }
    return std::nullopt;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    CommandRegistryRegisterCommandHook,
    ll::memory::HookPriority::Normal,
    CommandRegistry,
    &CommandRegistry::registerCommand,
    void,
    std::string const&     name,
    char const*            description,
    CommandPermissionLevel requirement,
    CommandFlag            flag1,
    CommandFlag            flag2
) {
    if (enabled.load(std::memory_order_acquire)) {
        const auto override = permissionOverrides.find(normalizeCommandName(name));
        if (override != permissionOverrides.end()) {
            requirement = override->second;
        }
    }
    origin(name, description, requirement, flag1, flag2);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.commandPermissions.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }

    std::unordered_map<std::string, CommandPermissionLevel> nextOverrides;
    for (auto const& rule : config.commandPermissions.rules) {
        const std::string command = normalizeCommandName(rule.command);
        if (command.empty()) {
            logger.warn(tr("command_permission.empty_command"));
            continue;
        }

        const auto permissionLevel = parsePermissionLevel(rule.permissionLevel);
        if (!permissionLevel.has_value()) {
            logger.warn(tr("command_permission.invalid_level", rule.permissionLevel, rule.command));
            continue;
        }
        nextOverrides[command] = *permissionLevel;
    }

    if (nextOverrides.empty()) {
        logger.warn(tr("command_permission.no_valid_rules"));
        return true;
    }

    permissionOverrides = std::move(nextOverrides);
    if (CommandRegistryRegisterCommandHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_release);
    logger.info(tr("command_permission.registered", permissionOverrides.size()));
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    CommandRegistryRegisterCommandHook::unhook();
}

} // namespace my_mod::command_permission

#else

namespace my_mod::command_permission {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::command_permission

#endif
