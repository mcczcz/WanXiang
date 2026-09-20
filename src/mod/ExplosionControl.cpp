#include "mod/ExplosionControl.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "catalyst/event/world/ExplosionEvent.h"
#include "ll/api/event/EventBus.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/Level.h"

#include <algorithm>

namespace my_mod::explosion_control {
namespace {

ll::event::ListenerPtr listener;

bool matchesSource(ExplosionRuleConfig const& rule, Actor const* source) {
    if (rule.sourceType == "*") {
        return true;
    }
    if (source == nullptr) {
        return rule.sourceType == "<none>";
    }
    return rule.sourceType == source->getTypeName();
}

void applyRule(Catalyst::ExplosionBeforeEvent& event, ExplosionRuleConfig const& rule) {
    if (rule.cancel) {
        event.cancel();
        return;
    }

    auto& explosion = event.explosion();
    if (rule.radius.has_value()) {
        event.radius() = std::max(*rule.radius, 0.0F);
    }
    if (rule.fire.has_value()) {
        event.fire() = *rule.fire;
    }
    if (rule.breaking.has_value()) {
        event.breaking() = *rule.breaking;
    }
    if (rule.allowUnderwater.has_value()) {
        explosion.mAllowUnderwater = *rule.allowUnderwater;
    }
    if (rule.canToggleBlocks.has_value()) {
        explosion.mCanToggleBlocks = *rule.canToggleBlocks;
    }
    if (rule.damageScaling.has_value()) {
        explosion.mDamageScaling = std::max(*rule.damageScaling, 0.0F);
    }
    if (rule.ignoreBlockResistance.has_value()) {
        explosion.mIgnoreBlockExplosionResistance = *rule.ignoreBlockResistance;
    }
    if (rule.maxResistance.has_value()) {
        explosion.mMaxResistance = std::max(*rule.maxResistance, 0.0F);
    }
    if (rule.inWater.has_value()) {
        explosion.mInWaterOverride = *rule.inWater;
    }
    if (rule.totalDamage.has_value()) {
        explosion.mTotalDamageOverride = std::max(*rule.totalDamage, 0);
    }
    if (rule.knockbackScaling.has_value()) {
        explosion.mKnockbackScaling = std::max(*rule.knockbackScaling, 0.0F);
    }
}

void onExplosion(Catalyst::ExplosionBeforeEvent& event) {
    auto const& explosionConfig = config.explosionControl;
    if (!explosionConfig.enable) {
        return;
    }

    auto const& explosion = event.explosion();
    auto const* source = event.blockSource().getLevel().fetchEntity(explosion.mSourceID, false);
    for (auto const& rule : explosionConfig.rules) {
        if (!matchesSource(rule, source)) {
            continue;
        }

        applyRule(event, rule);
        if (event.isCancelled()) {
            return;
        }
    }
}

} // namespace

bool enable() {
    if (!config.explosionControl.enable || listener) {
        return true;
    }

    listener = ll::event::EventBus::getInstance().emplaceListener<Catalyst::ExplosionBeforeEvent>(onExplosion);
    return listener != nullptr;
}

void disable() {
    if (!listener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener<Catalyst::ExplosionBeforeEvent>(listener);
    listener.reset();
}

} // namespace my_mod::explosion_control

#else

namespace my_mod::explosion_control {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::explosion_control

#endif
