#include "mod/FastLeafDecay.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/game_refs/WeakRef.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/block/BlockProperty.h"
#include "mc/world/level/block/LogBlock.h"
#include "mc/world/level/block/VanillaBlockTags.h"
#include "mc/world/level/block/VanillaStates.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace my_mod::fast_leaf_decay {
namespace {

constexpr int MaxRadius      = 16;
constexpr int MaxLogDistance = 6;

constexpr std::array<BlockPos, 6> NeighborOffsets{
    BlockPos{1,  0,  0 },
    BlockPos{-1, 0,  0 },
    BlockPos{0,  1,  0 },
    BlockPos{0,  -1, 0 },
    BlockPos{0,  0,  1 },
    BlockPos{0,  0,  -1},
};

struct BlockPosHash {
    std::size_t operator()(BlockPos const& pos) const noexcept {
        std::size_t seed  = std::hash<int>{}(pos.x);
        seed             ^= std::hash<int>{}(pos.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed             ^= std::hash<int>{}(pos.z) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

struct BlockPosEqual {
    bool operator()(BlockPos const& lhs, BlockPos const& rhs) const noexcept {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }
};

struct SearchNode {
    BlockPos pos;
    int      distance;
};

struct PendingDecay {
    WeakRef<BlockSource> region;
    BlockPos             pos;
};

std::atomic_bool          enabled{false};
ll::event::ListenerPtr    tickListener;
std::vector<PendingDecay> pendingDecays;

bool isLeaf(Block const& block) { return block.hasProperty(BlockProperty::Leaves); }

bool isPersistentLeaf(Block const& block) {
    return block.getState<bool>(VanillaStates::PersistentBit()).value_or(false);
}

bool isSupportedByLog(BlockSource& region, BlockPos const& start) {
    std::vector<SearchNode> queue;
    queue.reserve(128);
    queue.push_back({start, 0});

    std::unordered_set<BlockPos, BlockPosHash, BlockPosEqual> visited;
    visited.reserve(128);
    visited.insert(start);

    for (std::size_t index = 0; index < queue.size(); ++index) {
        SearchNode const current = queue[index];
        for (BlockPos const& offset : NeighborOffsets) {
            BlockPos const neighbor{
                current.pos.x + offset.x,
                current.pos.y + offset.y,
                current.pos.z + offset.z,
            };
            Block const& neighborBlock = region.getBlock(neighbor);
            if (neighborBlock.hasTag(VanillaBlockTags::Log())) {
                return true;
            }

            int const nextDistance = current.distance + 1;
            if (nextDistance >= MaxLogDistance || !isLeaf(neighborBlock) || visited.contains(neighbor)) {
                continue;
            }

            visited.insert(neighbor);
            queue.push_back({neighbor, nextDistance});
        }
    }
    return false;
}

void decayUnsupportedLeaves(BlockSource& region, BlockPos const& pos) {
    int const radius = std::clamp(config.fastLeafDecay.radius, 1, MaxRadius);

    // Support checks reach beyond the configured decay radius; never load chunks for this feature.
    if (!region.hasChunksAt(pos, radius + MaxLogDistance, false)) {
        return;
    }

    std::vector<BlockPos> leavesToDestroy;
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            for (int z = -radius; z <= radius; ++z) {
                BlockPos const leafPos{pos.x + x, pos.y + y, pos.z + z};
                Block const&   block = region.getBlock(leafPos);
                if (!isLeaf(block) || isPersistentLeaf(block) || isSupportedByLog(region, leafPos)) {
                    continue;
                }
                leavesToDestroy.push_back(leafPos);
            }
        }
    }

    for (BlockPos const& leafPos : leavesToDestroy) {
        if (!isLeaf(region.getBlock(leafPos))) {
            continue;
        }
        region.getLevel().destroyBlock(region, leafPos, true, BlockChangeContext::environmentChange(leafPos));
    }
}

void onServerTick(ll::event::ServerLevelTickEvent&) {
    auto pending = std::move(pendingDecays);
    pendingDecays.clear();

    for (auto const& task : pending) {
        auto region = task.region.lock();
        if (!region || region->getBlock(task.pos).hasTag(VanillaBlockTags::Log())) {
            continue;
        }
        decayUnsupportedLeaves(*region, task.pos);
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    LogRemovedHook,
    ll::memory::HookPriority::Normal,
    LogBlock,
    &LogBlock::$onRemove,
    void,
    BlockSource&    region,
    BlockPos const& pos
) {
    origin(region, pos);

    if (!enabled.load(std::memory_order_relaxed)) {
        return;
    }
    pendingDecays.push_back({region.getWeakRef(), pos});
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.fastLeafDecay.enable) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (LogRemovedHook::hook() != 0) {
        return false;
    }

    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    if (!tickListener) {
        LogRemovedHook::unhook();
        return false;
    }

    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    LogRemovedHook::unhook();
    if (tickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(tickListener);
        tickListener.reset();
    }
    pendingDecays.clear();
}

} // namespace my_mod::fast_leaf_decay

#else

namespace my_mod::fast_leaf_decay {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::fast_leaf_decay

#endif
