#include "mod/MobEffectBlocker.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectIds.h"

#include <atomic>

namespace my_mod::mob_effect_blocker {
namespace {

std::atomic_bool enabled{false};

bool hasEffectId(uint effectId, MobEffectIds target) { return effectId == static_cast<uint>(target); }

bool shouldSuppressOnDeath(MobEffect const& effect) {
    auto const& blockerConfig = config.mobEffectBlocker;
    auto const  effectId      = effect.mId;

    return (blockerConfig.weaving && hasEffectId(effectId, MobEffectIds::Weaving))
        || (blockerConfig.oozing && hasEffectId(effectId, MobEffectIds::Oozing))
        || (blockerConfig.windCharged && hasEffectId(effectId, MobEffectIds::WindCharged));
}

bool shouldSuppressOnHurt(MobEffect const& effect) {
    return config.mobEffectBlocker.infested && hasEffectId(effect.mId, MobEffectIds::Infested);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    EffectOnActorDiedHook,
    ll::memory::HookPriority::Normal,
    MobEffect,
    &MobEffect::$onActorDied,
    void,
    Actor& actor,
    int   amplifier
) {
    if (enabled.load(std::memory_order_relaxed) && shouldSuppressOnDeath(*this)) {
        return;
    }
    origin(actor, amplifier);
}

LL_TYPE_INSTANCE_HOOK(
    EffectOnActorHurtHook,
    ll::memory::HookPriority::Normal,
    MobEffect,
    &MobEffect::$onActorHurt,
    void,
    Actor&                   actor,
    int                      amplifier,
    ActorDamageSource const& source,
    float                    damage
) {
    if (enabled.load(std::memory_order_relaxed) && shouldSuppressOnHurt(*this)) {
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
    if (EffectOnActorDiedHook::hook() != 0) {
        return false;
    }
    if (EffectOnActorHurtHook::hook() != 0) {
        EffectOnActorDiedHook::unhook();
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    EffectOnActorHurtHook::unhook();
    EffectOnActorDiedHook::unhook();
}

} // namespace my_mod::mob_effect_blocker

#else

namespace my_mod::mob_effect_blocker {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::mob_effect_blocker

#endif
