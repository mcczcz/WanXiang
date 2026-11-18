#include "mod/DroppedItemLifetime.h"

#ifdef LL_PLAT_S

#include "mod/FeatureDiagnostics.h"
#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BedrockSpawner.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <string>

namespace my_mod::dropped_item_lifetime {
namespace {

constexpr std::int64_t TicksPerSecond = 20;

std::atomic_bool enabled{false};

struct RegexRule {
    std::regex pattern;
    int        seconds;
};

std::map<std::string, RegexRule> regexRules;

void loadRegexRules() {
    regexRules.clear();
    for (auto const& [key, seconds] : config.droppedItemLifetime.items) {
        if (key.size() < 2 || key.front() != '/' || key.back() != '/') {
            continue;
        }
        try {
            regexRules.emplace(key, RegexRule{std::regex(key.begin() + 1, key.end() - 1), seconds});
        } catch (std::regex_error const& error) {
            logger.warn(tr("dropped_item_lifetime.invalid_regex", key, error.what()));
        }
    }
}

int getLifetimeSeconds(ItemStack const& item) {
    auto const& lifetimeConfig = config.droppedItemLifetime;
    if (lifetimeConfig.items.empty()) {
        return lifetimeConfig.defaultSeconds;
    }
    auto const& typeName = item.getTypeName();
    auto const  override = lifetimeConfig.items.find(typeName);
    if (override != lifetimeConfig.items.end()) {
        return override->second;
    }
    for (auto const& [key, rule] : regexRules) {
        if (std::regex_match(typeName, rule.pattern)) {
            return rule.seconds;
        }
    }
    return lifetimeConfig.defaultSeconds;
}

void applyLifetime(ItemActor& itemActor, int seconds) {
    auto const clampedSeconds = std::max(seconds, 0);
    auto const lifetimeTicks  = std::min(
        static_cast<std::int64_t>(clampedSeconds) * TicksPerSecond,
        static_cast<std::int64_t>(std::numeric_limits<int>::max())
    );
    auto const initialAge = std::clamp(
        static_cast<std::int64_t>(itemActor.lifeTime()) - lifetimeTicks,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())
    );
    itemActor.age() = static_cast<int>(initialAge);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    BedrockSpawnerSpawnItemHook,
    ll::memory::HookPriority::Normal,
    BedrockSpawner,
    &BedrockSpawner::$spawnItem,
    ItemActor*,
    BlockSource&     region,
    ItemStack const& item,
    Actor*           spawner,
    Vec3 const&      position,
    int              throwTime
) {
    ItemActor* itemActor = origin(region, item, spawner, position, throwTime);
    if (enabled.load(std::memory_order_relaxed) && itemActor != nullptr) {
        applyLifetime(*itemActor, getLifetimeSeconds(item));
    }
    return itemActor;
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.droppedItemLifetime.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    loadRegexRules();
    if (!feature_diagnostics::checkHook(
            "dropped_item_lifetime",
            "BedrockSpawnerSpawnItemHook",
            BedrockSpawnerSpawnItemHook::hook()
        )) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    BedrockSpawnerSpawnItemHook::unhook();
}

} // namespace my_mod::dropped_item_lifetime

#else

namespace my_mod::dropped_item_lifetime {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::dropped_item_lifetime

#endif
