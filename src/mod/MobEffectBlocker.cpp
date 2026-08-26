#include "mod/MobEffectBlocker.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectInstance.h"

#include <atomic>

namespace my_mod::mob_effect_blocker {
namespace {

std::atomic_bool enabled{false};

bool hasEffectId(MobEffect const* target, uint effectId) { return target != nullptr && target->getId() == effectId; }

bool shouldSuppressOnDeath(MobEffectInstance const& effect) {
    auto const& blockerConfig = config.mobEffectBlocker;
    auto const  effectId      = effect.getId();

    return (blockerConfig.weaving && hasEffectId(MobEffect::WEAVING(), effectId))
        || (blockerConfig.oozing && hasEffectId(MobEffect::OOZING(), effectId))
        || (blockerConfig.windCharged && hasEffectId(MobEffect::WIND_CHARGED(), effectId));
}

bool shouldSuppressOnHurt(MobEffectInstance const& effect) {
    return config.mobEffectBlocker.infested && hasEffectId(MobEffect::INFESTED(), effect.getId());
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    EffectOnActorDiedHook,
    ll::memory::HookPriority::Normal,
    MobEffectInstance,
    &MobEffectInstance::onActorDied,
    void,
    Actor& actor
) {
    if (enabled.load(std::memory_order_relaxed) && shouldSuppressOnDeath(*this)) {
        return;
    }
    origin(actor);
}

LL_TYPE_INSTANCE_HOOK(
    EffectOnActorHurtHook,
    ll::memory::HookPriority::Normal,
    MobEffectInstance,
    &MobEffectInstance::onActorHurt,
    void,
    Actor&                   actor,
    ActorDamageSource const& source,
    float                    damage
) {
    if (enabled.load(std::memory_order_relaxed) && shouldSuppressOnHurt(*this)) {
        return;
    }
    origin(actor, source, damage);
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
