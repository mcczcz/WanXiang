#include "mod/XpOrbMerge.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDataIDs.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/DataItem.h"
#include "mc/world/actor/SynchedActorData.h"
#include "mc/world/actor/SynchedActorDataEntityWrapper.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace my_mod::xp_orb_merge {
namespace {

constexpr ushort XpValueDataId = static_cast<ushort>(ActorDataIDs::Value);

ll::event::ListenerPtr tickListener;
std::uint64_t          elapsedTicks = 0;

int getXp(Actor const& orb) { return orb.getEntityData().getInt(XpValueDataId); }

bool setXp(Actor& orb, std::int64_t value) {
    constexpr auto minXp = static_cast<std::int64_t>(std::numeric_limits<int>::min());
    constexpr auto maxXp = static_cast<std::int64_t>(std::numeric_limits<int>::max());
    int            xp    = static_cast<int>(std::clamp(value, minXp, maxXp));

    auto&     entityData = orb.getEntityData();
    DataItem* item       = entityData._get()->_find(XpValueDataId);
    if (item == nullptr || !item->setData(std::move(xp))) {
        return false;
    }

    entityData.markDirty(XpValueDataId);
    return true;
}

void mergeGroup(std::vector<Actor*>& orbs, float radiusSquared) {
    std::vector<bool> consumed(orbs.size(), false);

    for (std::size_t i = 0; i < orbs.size(); ++i) {
        Actor* keeper = orbs[i];
        if (consumed[i] || keeper == nullptr || keeper->isRemoved()) {
            continue;
        }

        Vec3 const keeperPosition = keeper->getPosition();
        Vec3       positionSum{
            keeperPosition.x,
            keeperPosition.y,
            keeperPosition.z,
        };
        std::int64_t        totalXp = getXp(*keeper);
        std::size_t         orbCount{1};
        std::vector<Actor*> toRemove;

        for (std::size_t j = i + 1; j < orbs.size(); ++j) {
            Actor* candidate = orbs[j];
            if (consumed[j] || candidate == nullptr || candidate->isRemoved()) {
                continue;
            }

            Vec3 const  candidatePosition = candidate->getPosition();
            float const dx                = keeperPosition.x - candidatePosition.x;
            float const dy                = keeperPosition.y - candidatePosition.y;
            float const dz                = keeperPosition.z - candidatePosition.z;
            if (dx * dx + dy * dy + dz * dz > radiusSquared) {
                continue;
            }

            positionSum.x += candidatePosition.x;
            positionSum.y += candidatePosition.y;
            positionSum.z += candidatePosition.z;
            totalXp       += getXp(*candidate);
            ++orbCount;
            consumed[j] = true;
            toRemove.emplace_back(candidate);
        }

        if (toRemove.empty()) {
            continue;
        }

        float const inverseCount = 1.0F / static_cast<float>(orbCount);
        Vec3 const  center{
            positionSum.x * inverseCount,
            positionSum.y * inverseCount,
            positionSum.z * inverseCount,
        };

        if (!setXp(*keeper, totalXp)) {
            continue;
        }

        keeper->teleport(center, keeper->getDimensionId());

        for (Actor* candidate : toRemove) {
            candidate->remove();
        }
    }
}

void mergeAll(Level& level) {
    auto const& mergeConfig = config.xpOrbMerge;
    if (!mergeConfig.enable || !std::isfinite(mergeConfig.radius) || mergeConfig.radius < 0.0F) {
        return;
    }

    std::unordered_map<DimensionType, std::vector<Actor*>> groups;
    for (Actor* actor : level.getRuntimeActorList()) {
        if (actor == nullptr || actor->isRemoved() || !actor->isType(ActorType::Experience)) {
            continue;
        }
        groups[actor->getDimensionId()].emplace_back(actor);
    }

    float const radiusSquared = mergeConfig.radius * mergeConfig.radius;
    for (auto& [dimension, orbs] : groups) {
        (void)dimension;
        mergeGroup(orbs, radiusSquared);
    }
}

void onServerTick(ll::event::ServerLevelTickEvent& event) {
    auto const interval = static_cast<std::uint64_t>(std::max(config.xpOrbMerge.intervalTicks, 1));
    if (++elapsedTicks < interval) {
        return;
    }

    elapsedTicks = 0;
    mergeAll(event.level());
}

} // namespace

bool enable() {
    if (!config.xpOrbMerge.enable || tickListener) {
        return true;
    }

    elapsedTicks = 0;
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    return tickListener != nullptr;
}

void disable() {
    if (!tickListener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(tickListener);
    tickListener.reset();
    elapsedTicks = 0;
}

} // namespace my_mod::xp_orb_merge

#else

namespace my_mod::xp_orb_merge {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::xp_orb_merge

#endif
