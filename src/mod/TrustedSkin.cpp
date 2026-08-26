#include "mod/TrustedSkin.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/world/actor/player/SerializedSkinRef.h"

#include <atomic>

namespace my_mod::trusted_skin {
namespace {

std::atomic_bool enabled{false};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    DecideIfSkinIsTrustedHook,
    ll::memory::HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::_decideIfSkinIsTrusted,
    void,
    SerializedSkinRef& skin
) {
    origin(skin);
    if (enabled.load(std::memory_order_relaxed)) {
        skin.setIsTrustedSkin(true);
    }
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.trustedSkinEnable) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (DecideIfSkinIsTrustedHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    DecideIfSkinIsTrustedHook::unhook();
}

} // namespace my_mod::trusted_skin

#else

namespace my_mod::trusted_skin {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::trusted_skin

#endif
