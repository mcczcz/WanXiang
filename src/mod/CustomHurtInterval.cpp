#include "mod/CustomHurtInterval.h"

#ifdef LL_PLAT_S

#include "mod/FeatureDiagnostics.h"
#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorHurtResult.h"
#include "mc/world/actor/Mob.h"

#include <atomic>

namespace my_mod::custom_hurt_interval {
namespace {

std::atomic_bool enabled{false};

bool isPlayerAttack(ActorDamageSource const& source) {
    using Cause = SharedTypes::Legacy::ActorDamageCause;
    // 子实体伤害的 getEntityType() 指向攻击者，getDamagingEntityType() 指向投射物。
    return source.isEntitySource() && source.getEntityType() == ActorType::Player
        && (source.mCause == Cause::EntityAttack || source.mCause == Cause::Projectile
            || source.mCause == Cause::MaceSmash);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    MobHurtHook,
    ll::memory::HookPriority::Normal,
    Mob,
    &Mob::$_hurt,
    ActorHurtResult,
    ActorDamageSource const& source,
    float                    damage,
    HurtParameters const&    hurtParameters
) {
    auto const& intervalConfig = config.customHurtInterval;
    if (!enabled.load(std::memory_order_relaxed) || !intervalConfig.enable || !isPlayerAttack(source)) {
        return origin(source, damage, hurtParameters);
    }

    auto result = origin(source, damage, hurtParameters);
    // 被原版拒绝的攻击不刷新间隔，保留原版伤害和击退处理。
    if (result.mType == ActorHurtResult::Type::DamageProcessed) {
        mInvulnerableTime = isPlayer() ? intervalConfig.player : intervalConfig.mob;
    }
    return result;
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    auto const& intervalConfig = config.customHurtInterval;
    if (!intervalConfig.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (intervalConfig.player < 0 || intervalConfig.mob < 0) {
        return feature_diagnostics::fail(
            tr("custom_hurt_interval.invalid_ticks_detail", intervalConfig.player, intervalConfig.mob)
        );
    }
    if (!feature_diagnostics::checkHook("custom_hurt_interval", "MobHurtHook", MobHurtHook::hook())) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    MobHurtHook::unhook();
}

} // namespace my_mod::custom_hurt_interval

#else

namespace my_mod::custom_hurt_interval {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::custom_hurt_interval

#endif
