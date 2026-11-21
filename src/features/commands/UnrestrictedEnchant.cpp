#include "features/commands/UnrestrictedEnchant.h"

#ifdef LL_PLAT_S

#include "core/FeatureDiagnostics.h"
#include "core/Global.h"

#include "core/memory/FunctionSignature.h"
#include "ll/api/base/ScopedValue.h"
#include "ll/api/memory/Hook.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/standard/EnchantCommand.h"
#include "mc/world/item/enchanting/EnchantResult.h"
#include "mc/world/item/enchanting/ItemEnchants.h"

#include <atomic>
#include <cstdint>
#include <limits>

namespace my_mod::unrestricted_enchant {
namespace {

std::atomic_bool  enabled{false};
thread_local bool executingEnchantCommand = false;
std::uintptr_t    enchantExecuteAddress{};

using namespace ll::literals;

// IDA: ItemEnchants::_toList writes lvl as ShortTag; _fromList sign-extends it.
constexpr int maxStoredLevel = std::numeric_limits<std::int16_t>::max();

bool isStoredLevel(int level) { return level >= 1 && level <= maxStoredLevel; }

EnchantResult unrestrictedResult(ItemEnchants const& enchants, EnchantmentInstance const& enchant) {
    EnchantResult result{};
    auto const    type = static_cast<unsigned>(enchant.mEnchantType);
    if (type >= static_cast<unsigned>(Enchant::Type::NumEnchantments) || !isStoredLevel(enchant.mLevel)) {
        result.result = EnchantResultType::Fail;
        return result;
    }
    result.level = enchant.mLevel;
    // IDA: both the command preflight and applyEnchant call ItemEnchants::canEnchant.
    // The engine groups stored entries by activation; Increment uses the index within that group.
    for (auto const& group : *enchants.mItemEnchants) {
        for (size_t index = 0; index < group.size(); ++index) {
            if (group[index].mEnchantType == enchant.mEnchantType) {
                result.result     = EnchantResultType::Increment;
                result.enchantIdx = index;
                return result;
            }
        }
    }
    result.result = EnchantResultType::Enchant;
    return result;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    EnchantExecuteHook,
    ll::memory::HookPriority::Normal,
    EnchantCommand,
    enchantExecuteAddress,
    void,
    CommandOrigin const& originContext,
    CommandOutput&       output
) {
    if (!enabled.load(std::memory_order_relaxed) || !config.unrestrictedEnchantEnable) {
        origin(originContext, output);
        return;
    }
    if (!isStoredLevel(mLevel)) {
        output.error(tr("unrestricted_enchant.invalid_level", maxStoredLevel));
        return;
    }
    ll::ScopedValue scope(executingEnchantCommand, true);
    origin(originContext, output);
}

LL_STATIC_HOOK(
    EnchantLevelRangeHook,
    ll::memory::HookPriority::Normal,
    (static_cast<bool (*)(int, int, int, CommandOutput&)>(&Command::validRange)),
    bool,
    int            input,
    int            low,
    int            high,
    CommandOutput& output
) {
    if (executingEnchantCommand) {
        return origin(input, 1, maxStoredLevel, output);
    }
    return origin(input, low, high, output);
}

LL_TYPE_INSTANCE_HOOK(
    EnchantCompatibilityHook,
    ll::memory::HookPriority::Normal,
    ItemEnchants,
    &ItemEnchants::canEnchant,
    EnchantResult,
    EnchantmentInstance enchant,
    bool                allowNonVanilla
) {
    if (executingEnchantCommand) {
        return unrestrictedResult(*this, enchant);
    }
    return origin(enchant, allowNonVanilla);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.unrestrictedEnchantEnable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    // Keep the validated address across reloads: the installed hook changes entry bytes.
    if (!enchantExecuteAddress) {
        enchantExecuteAddress = reinterpret_cast<std::uintptr_t>(function_signature::resolveUnique(
            "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 98 04 00 00 48 8D AC 24 ?? ?? ?? ?? 0F 29 B5 ?? ?? ?? ?? 48 C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 4D 89 C5 48 89 CB"_sig
        ));
        if (!enchantExecuteAddress) {
            return feature_diagnostics::fail(tr("unrestricted_enchant.unsupported"));
        }
    }
    if (!feature_diagnostics::checkHook(
            "unrestricted_enchant",
            "EnchantCompatibilityHook",
            EnchantCompatibilityHook::hook()
        )) {
        return false;
    }
    if (!feature_diagnostics::checkHook(
            "unrestricted_enchant",
            "EnchantLevelRangeHook",
            EnchantLevelRangeHook::hook()
        )) {
        EnchantCompatibilityHook::unhook();
        return false;
    }
    if (!feature_diagnostics::checkHook("unrestricted_enchant", "EnchantExecuteHook", EnchantExecuteHook::hook())) {
        EnchantLevelRangeHook::unhook();
        EnchantCompatibilityHook::unhook();
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    EnchantExecuteHook::unhook();
    EnchantLevelRangeHook::unhook();
    EnchantCompatibilityHook::unhook();
}

} // namespace my_mod::unrestricted_enchant

#else

namespace my_mod::unrestricted_enchant {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::unrestricted_enchant

#endif
