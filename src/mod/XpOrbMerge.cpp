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
#include "mc/entity/components/SynchedActorDataComponent.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

namespace my_mod::xp_orb_merge {
namespace {

constexpr ushort XpValueDataId = static_cast<ushort>(ActorDataIDs::Value);

ll::event::ListenerPtr tickListener;
std::uint64_t          elapsedTicks = 0;

int getXp(Actor const& orb) { return orb.mEntityData->getInt(XpValueDataId); }

bool setXp(Actor& orb, std::int64_t value) {
    constexpr auto minXp = static_cast<std::int64_t>(std::numeric_limits<int>::min());
    constexpr auto maxXp = static_cast<std::int64_t>(std::numeric_limits<int>::max());
    int            xp    = static_cast<int>(std::clamp(value, minXp, maxXp));

    auto& entityData = *orb.mEntityData;
    auto& actorData  = *entityData.mData->mData;
    bool  updated    = false;
    for (auto& item : *actorData.mItemsArray) {
        if (item != nullptr && item->getId() == XpValueDataId) {
            if (!item->setData(static_cast<int&&>(xp))) {
                return false;
            }
            updated = true;
            break;
        }
    }
    if (!updated) {
        return false;
    }
    entityData.markDirty(XpValueDataId);
    return true;
}

// A single orb's state, snapshotted once so the merge loop never repeats the virtual
// getPosition()/getXp() calls per comparison.
struct OrbEntry {
    Actor* actor;
    Vec3   position;
    int    xp;
};

using CellKey = std::array<std::int64_t, 3>;

struct CellKeyHash {
    std::size_t operator()(CellKey const& key) const noexcept {
        std::size_t seed = 0;
        for (std::int64_t const value : key) {
            seed ^= std::hash<std::int64_t>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
        }
        return seed;
    }
};

CellKey cellOf(Vec3 const& position, float cellSize) {
    return {
        static_cast<std::int64_t>(std::floor(position.x / cellSize)),
        static_cast<std::int64_t>(std::floor(position.y / cellSize)),
        static_cast<std::int64_t>(std::floor(position.z / cellSize)),
    };
}

// Merges every orb within `radius` of a keeper. Orbs are bucketed into a uniform grid of
// cell size `radius`, so any partner is guaranteed to sit in one of the 27 neighbouring cells
// and each orb is compared against a bounded local neighbourhood instead of the whole
// dimension. This turns the old O(n^2) sweep -- 2M iterations for a 2000-orb grinder -- into
// roughly linear work.
void mergeGroup(std::vector<Actor*> const& orbs, float radius, float radiusSquared) {
    std::vector<OrbEntry> entries;
    entries.reserve(orbs.size());
    for (Actor* orb : orbs) {
        if (orb == nullptr || orb->mRemoved) {
            continue;
        }
        Vec3 const position = orb->getPosition();
        // A non-finite coordinate would make the cell index computation undefined.
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
            continue;
        }
        entries.emplace_back(OrbEntry{orb, position, getXp(*orb)});
    }
    if (entries.size() < 2) {
        return;
    }

    float const cellSize = std::max(radius, 1.0e-3F);

    std::unordered_map<CellKey, std::vector<std::uint32_t>, CellKeyHash> grid;
    grid.reserve(entries.size());
    for (std::uint32_t index = 0; index < entries.size(); ++index) {
        grid[cellOf(entries[index].position, cellSize)].emplace_back(index);
    }

    std::vector<std::uint8_t>  consumed(entries.size(), 0);
    std::vector<std::uint32_t> merged;

    for (std::uint32_t i = 0; i < entries.size(); ++i) {
        if (consumed[i] != 0) {
            continue;
        }

        OrbEntry const& keeper = entries[i];
        Vec3            positionSum{keeper.position.x, keeper.position.y, keeper.position.z};
        std::int64_t    totalXp = keeper.xp;
        std::size_t     orbCount{1};
        merged.clear();

        CellKey const base = cellOf(keeper.position, cellSize);
        for (std::int64_t dx = -1; dx <= 1; ++dx) {
            for (std::int64_t dy = -1; dy <= 1; ++dy) {
                for (std::int64_t dz = -1; dz <= 1; ++dz) {
                    auto const cell = grid.find(CellKey{base[0] + dx, base[1] + dy, base[2] + dz});
                    if (cell == grid.end()) {
                        continue;
                    }

                    for (std::uint32_t const j : cell->second) {
                        if (j == i || consumed[j] != 0) {
                            continue;
                        }

                        OrbEntry const& candidate = entries[j];
                        float const     ddx       = keeper.position.x - candidate.position.x;
                        float const     ddy       = keeper.position.y - candidate.position.y;
                        float const     ddz       = keeper.position.z - candidate.position.z;
                        if ((ddx * ddx) + (ddy * ddy) + (ddz * ddz) > radiusSquared) {
                            continue;
                        }

                        positionSum.x += candidate.position.x;
                        positionSum.y += candidate.position.y;
                        positionSum.z += candidate.position.z;
                        totalXp       += candidate.xp;
                        ++orbCount;
                        consumed[j] = 1;
                        merged.emplace_back(j);
                    }
                }
            }
        }

        if (merged.empty()) {
            continue;
        }

        if (!setXp(*keeper.actor, totalXp)) {
            // Release the partners again -- leaving them consumed would silently delete their
            // XP along with them, since nothing below will remove or re-merge them.
            for (std::uint32_t const j : merged) {
                consumed[j] = 0;
            }
            continue;
        }

        float const inverseCount = 1.0F / static_cast<float>(orbCount);
        Vec3 const  center{
            positionSum.x * inverseCount,
            positionSum.y * inverseCount,
            positionSum.z * inverseCount,
        };
        keeper.actor->teleport(center, keeper.actor->getDimensionId());

        for (std::uint32_t const j : merged) {
            entries[j].actor->remove();
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
        if (actor == nullptr || actor->mRemoved || !actor->isType(ActorType::Experience)) {
            continue;
        }
        groups[actor->getDimensionId()].emplace_back(actor);
    }

    float const radiusSquared = mergeConfig.radius * mergeConfig.radius;
    for (auto const& [dimension, orbs] : groups) {
        (void)dimension;
        mergeGroup(orbs, mergeConfig.radius, radiusSquared);
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
