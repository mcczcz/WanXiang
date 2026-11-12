#include "mod/CustomKnockback.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/actor/Mob.h"

#include <atomic>
#include <cmath>

namespace my_mod::custom_knockback {
namespace {

std::atomic_bool enabled{false};

bool isValid(KnockbackMultiplierConfig const& multiplier) {
    return std::isfinite(multiplier.horizontal) && multiplier.horizontal >= 0.0F
        && std::isfinite(multiplier.vertical) && multiplier.vertical >= 0.0F;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    MobKnockbackHook,
    ll::memory::HookPriority::Normal,
    Mob,
    &Mob::$knockback,
    void,
    Actor*                     source,
    float                      damage,
    float                      xd,
    float                      zd,
    KnockbackParameters const& parameters
) {
    auto const& knockbackConfig = config.customKnockback;
    if (!enabled.load(std::memory_order_relaxed) || !knockbackConfig.enable || source == nullptr
        || !source->isPlayer()) {
        origin(source, damage, xd, zd, parameters);
        return;
    }

    auto const& multiplier = isPlayer() ? knockbackConfig.player : knockbackConfig.mob;
    if (multiplier.horizontal == 1.0F && multiplier.vertical == 1.0F) {
        origin(source, damage, xd, zd, parameters);
        return;
    }

    Vec3 const previous = getPosDelta();
    origin(source, damage, xd, zd, parameters);

    // 只缩放这次击退造成的速度变化，不放大实体原有的移动、跳跃或下落速度。
    // 在 Mob 层调整，让 ServerPlayer 的后续速度同步使用调整后的结果。
    Vec3& motion = mBuiltInComponents->mStateVectorComponent->mPosDelta;
    motion.x = previous.x + (motion.x - previous.x) * multiplier.horizontal;
    motion.y = previous.y + (motion.y - previous.y) * multiplier.vertical;
    motion.z = previous.z + (motion.z - previous.z) * multiplier.horizontal;
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    auto const& knockbackConfig = config.customKnockback;
    if (!knockbackConfig.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (!isValid(knockbackConfig.player) || !isValid(knockbackConfig.mob)) {
        logger.error(tr("custom_knockback.invalid_multiplier"));
        return false;
    }
    if (MobKnockbackHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    MobKnockbackHook::unhook();
}

} // namespace my_mod::custom_knockback

#else

namespace my_mod::custom_knockback {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::custom_knockback

#endif
