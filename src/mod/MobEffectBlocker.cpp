#include "mod/MobEffectBlocker.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/effect/InfestedMobEffect.h"
#include "mc/world/effect/OozingMobEffect.h"
#include "mc/world/effect/WeavingMobEffect.h"
#include "mc/world/effect/WindChargedMobEffect.h"

#include <atomic>

namespace my_mod::mob_effect_blocker {
namespace {

std::atomic_bool enabled{false};

// MobEffect 基类回调是 MCFOLD 空实现，共享地址可能被无关函数调用。
// 必须 Hook 各效果的具体实现，否则既会误拦截，也无法覆盖派生类的行为。
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    WeavingOnActorDiedHook,
    ll::memory::HookPriority::Normal,
    WeavingMobEffect,
    &WeavingMobEffect::$onActorDied,
    void,
    Actor& actor,
    int   amplifier
) {
    if (enabled.load(std::memory_order_relaxed) && config.mobEffectBlocker.weaving) {
        return;
    }
    origin(actor, amplifier);
}

LL_TYPE_INSTANCE_HOOK(
    OozingOnActorDiedHook,
    ll::memory::HookPriority::Normal,
    OozingMobEffect,
    &OozingMobEffect::$onActorDied,
    void,
    Actor& actor,
    int    amplifier
) {
    if (enabled.load(std::memory_order_relaxed) && config.mobEffectBlocker.oozing) {
        return;
    }
    origin(actor, amplifier);
}

LL_TYPE_INSTANCE_HOOK(
    WindChargedOnActorDiedHook,
    ll::memory::HookPriority::Normal,
    WindChargedMobEffect,
    &WindChargedMobEffect::$onActorDied,
    void,
    Actor& actor,
    int    amplifier
) {
    if (enabled.load(std::memory_order_relaxed) && config.mobEffectBlocker.windCharged) {
        return;
    }
    origin(actor, amplifier);
}

LL_TYPE_INSTANCE_HOOK(
    InfestedOnActorHurtHook,
    ll::memory::HookPriority::Normal,
    InfestedMobEffect,
    &InfestedMobEffect::$onActorHurt,
    void,
    Actor&                   actor,
    int                      amplifier,
    ActorDamageSource const& source,
    float                    damage
) {
    if (enabled.load(std::memory_order_relaxed) && config.mobEffectBlocker.infested) {
        return;
    }
    origin(actor, amplifier, source, damage);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.mobEffectBlocker.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (WeavingOnActorDiedHook::hook() != 0) {
        return false;
    }
    if (OozingOnActorDiedHook::hook() != 0) {
        WeavingOnActorDiedHook::unhook();
        return false;
    }
    if (WindChargedOnActorDiedHook::hook() != 0) {
        OozingOnActorDiedHook::unhook();
        WeavingOnActorDiedHook::unhook();
        return false;
    }
    if (InfestedOnActorHurtHook::hook() != 0) {
        WindChargedOnActorDiedHook::unhook();
        OozingOnActorDiedHook::unhook();
        WeavingOnActorDiedHook::unhook();
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    InfestedOnActorHurtHook::unhook();
    WindChargedOnActorDiedHook::unhook();
    OozingOnActorDiedHook::unhook();
    WeavingOnActorDiedHook::unhook();
}

} // namespace my_mod::mob_effect_blocker

#else

namespace my_mod::mob_effect_blocker {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::mob_effect_blocker

#endif
