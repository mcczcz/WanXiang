#include "mod/DroppedItemLifetime.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BedrockSpawner.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>

namespace my_mod::dropped_item_lifetime {
namespace {

constexpr std::int64_t TicksPerSecond = 20;

std::atomic_bool enabled{false};

int getLifetimeSeconds(ItemStack const& item) {
    auto const& lifetimeConfig = config.droppedItemLifetime;
    if (lifetimeConfig.items.empty()) {
        return lifetimeConfig.defaultSeconds;
    }
    auto const override = lifetimeConfig.items.find(item.getTypeName());
    if (override != lifetimeConfig.items.end()) {
        return override->second;
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
    if (BedrockSpawnerSpawnItemHook::hook() != 0) {
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
