#include "mod/AutoFishing.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/FishingHook.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/ItemStack.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace my_mod::auto_fishing {
namespace {

std::atomic_bool enabled{false};
std::mutex       hookTimersMutex;
std::unordered_map<FishingHook const*, int> previousHookTimers;

bool hasJustBeenHooked(FishingHook const* hook, int timer) {
    std::scoped_lock lock{hookTimersMutex};
    auto const [it, inserted] = previousHookTimers.emplace(hook, timer);
    if (inserted) {
        return false;
    }
    bool const hooked = it->second > 0 && timer <= 0;
    it->second        = timer;
    return hooked;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    FishingHookPostNormalTickHook,
    ll::memory::HookPriority::Normal,
    FishingHook,
    &FishingHook::postNormalTick,
    void
) {
    origin();
    if (!enabled.load(std::memory_order_relaxed) || !hasJustBeenHooked(this, mTimeUntilHooked)) {
        return;
    }

    Player* player = getPlayerOwner();
    if (player == nullptr) {
        return;
    }

    if (!player->mGameMode) {
        return;
    }
    GameMode& gameMode = *player->mGameMode;
    gameMode.baseUseItem(const_cast<ItemStack&>(player->getSelectedItem()));


    auto& recast = const_cast<ItemStack&>(player->getSelectedItem());
    if (!recast.isNull()) {
        gameMode.baseUseItem(recast);
    }
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.autoFishingEnable) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (FishingHookPostNormalTickHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    FishingHookPostNormalTickHook::unhook();
    std::scoped_lock lock{hookTimersMutex};
    previousHookTimers.clear();
}

} // namespace my_mod::auto_fishing

#else

namespace my_mod::auto_fishing {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::auto_fishing

#endif
