#include "mod/AutoFishing.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/FishingHook.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/ItemStack.h"

#include <atomic>

namespace my_mod::auto_fishing {
namespace {

std::atomic_bool enabled{false};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    FishingHookedHook,
    ll::memory::HookPriority::Normal,
    FishingHook,
    &FishingHook::_serverHooked,
    bool
) {
    const bool result = origin();
    if (!enabled.load(std::memory_order_relaxed) || !result || mTimeUntilHooked) {
        return result;
    }

    Player* player = getPlayerOwner();
    if (player == nullptr) {
        return result;
    }

    GameMode& gameMode = player->getGameMode();
    gameMode.baseUseItem(const_cast<ItemStack&>(player->getSelectedItem()));


    auto& recast = const_cast<ItemStack&>(player->getSelectedItem());
    if (!recast.isNull()) {
        gameMode.baseUseItem(recast);
    }
    return result;
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
    if (FishingHookedHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    FishingHookedHook::unhook();
}

} // namespace my_mod::auto_fishing

#else

namespace my_mod::auto_fishing {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::auto_fishing

#endif
