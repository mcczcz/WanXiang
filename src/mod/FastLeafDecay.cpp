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
#include "mc/world/level/block/LeavesBlock.h"
#include "mc/world/level/block/LogBlock.h"
#include "mc/world/level/block/VanillaBlockTags.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace my_mod::fast_leaf_decay {
namespace {

constexpr int MaxRadius      = 16;
constexpr int MaxLogDistance = 6;

// Hard bounds so that pathological input (mass block removal, a giant hand-built leaf
// structure) degrades gracefully instead of stalling the tick.
constexpr std::size_t MaxPendingDecays = 4096;
constexpr std::size_t MaxGraphNodes    = 8192;

constexpr int UnsupportedDistance = std::numeric_limits<int>::max();

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

struct PendingDecay {
    WeakRef<BlockSource> region;
    BlockPos             pos;
};

struct PendingKey {
    DimensionType dimension;
    BlockPos      pos;
};

struct PendingKeyHash {
    std::size_t operator()(PendingKey const& key) const noexcept {
        std::size_t seed = BlockPosHash{}(key.pos);
        seed ^= std::hash<DimensionType>{}(key.dimension) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

struct PendingKeyEqual {
    bool operator()(PendingKey const& lhs, PendingKey const& rhs) const noexcept {
        return lhs.dimension == rhs.dimension && BlockPosEqual{}(lhs.pos, rhs.pos);
    }
};

std::atomic_bool          enabled{false};
ll::event::ListenerPtr    tickListener;
std::vector<PendingDecay> pendingDecays;

std::unordered_set<PendingKey, PendingKeyHash, PendingKeyEqual> pendingKeys;

// Set while we are destroying leaves ourselves, so the leaf-removal hook below can tell our own
// cascade apart from a genuine player action.
bool inDecayPass = false;

struct DecayPassGuard {
    DecayPassGuard() { inDecayPass = true; }
    ~DecayPassGuard() { inDecayPass = false; }

    DecayPassGuard(DecayPassGuard const&)            = delete;
    DecayPassGuard& operator=(DecayPassGuard const&) = delete;
};

void queueDecay(BlockSource& region, BlockPos const& pos) {
    if (!enabled.load(std::memory_order_relaxed)) {
        return;
    }
    // Every leaf we destroy re-enters this hook. Queuing those would schedule a fresh cluster
    // scan per destroyed leaf -- the amplification that let one felled tree spawn hundreds of
    // overlapping scans.
    if (inDecayPass) {
        return;
    }
    if (pendingDecays.size() >= MaxPendingDecays) {
        return;
    }
    // A single blast can hit thousands of blocks in one tick, most of them in the same cluster.
    // Deduplicating here keeps that down to one scan per distinct origin.
    if (!pendingKeys.emplace(PendingKey{region.getDimensionId(), pos}).second) {
        return;
    }
    pendingDecays.push_back({region.getWeakRef(), pos});
}

bool isLeaf(Block const& block) { return block.hasProperty(BlockProperty::Leaves); }

bool isPersistentLeaf(Block const& block) {
    return block.getState<bool>(VanillaStates::PersistentBit()).value_or(false);
}

bool isLeafSupportingWood(Block const& block) {
    return block.hasTag(VanillaBlockTags::Log()) || block.hasTag(VanillaBlockTags::Wood());
}

// Reusable scratch for the cluster walk. The tick handler is the only caller, and it runs one
// pass at a time, so keeping the capacity around avoids re-allocating these on every scan.
struct DecayScratch {
    std::vector<BlockPos>                                                   positions;
    std::vector<int>                                                        distance;
    std::vector<std::uint8_t>                                               isCandidate;
    std::unordered_map<BlockPos, std::uint32_t, BlockPosHash, BlockPosEqual> index;
    std::vector<std::uint32_t>                                              frontier;
    std::vector<std::uint32_t>                                              nextFrontier;

    void clear() {
        positions.clear();
        distance.clear();
        isCandidate.clear();
        index.clear();
        frontier.clear();
        nextFrontier.clear();
    }
};

DecayScratch scratch;

// Decays the leaves left unsupported by the block removed at `origin`.
//
// The whole cluster is analysed in a single linear pass rather than the old
// "scan a (2r+1)^3 cube, then run a fresh BFS per leaf found" approach:
//
//   1. Walk the connected leaf cluster once, out to radius + MaxLogDistance. Leaves within
//      `radius` are decay candidates; the outer shell only carries support inwards, so support
//      arriving from just outside the decay box is still seen.
//   2. Multi-source BFS from every leaf touching wood, propagating up to MaxLogDistance - 1.
//   3. Destroy the candidates the BFS never reached.
//
// Cost is O(cluster size) instead of O(r^3 x BFS).
void decayUnsupportedLeaves(BlockSource& region, BlockPos const& origin, int radius) {
    int const reach = radius + MaxLogDistance;

    // Support checks reach beyond the configured decay radius; never load chunks for this feature.
    if (!region.hasChunksAt(origin, reach, false)) {
        return;
    }

    // The furthest leaf that can still carry support to a candidate sits MaxLogDistance - 1 steps
    // out from `radius`; its wood neighbour is then exactly at `reach`. Flooding no further keeps
    // every getBlock inside the range hasChunksAt just vouched for.
    int const floodExtent = reach - 1;

    DecayScratch& s = scratch;
    s.clear();

    auto const withinBox = [&](BlockPos const& pos, int extent) {
        return std::abs(pos.x - origin.x) <= extent && std::abs(pos.y - origin.y) <= extent
            && std::abs(pos.z - origin.z) <= extent;
    };

    auto const addNode = [&](BlockPos const& pos) {
        auto const [entry, inserted] = s.index.try_emplace(pos, static_cast<std::uint32_t>(s.positions.size()));
        if (inserted) {
            s.positions.push_back(pos);
            s.distance.push_back(UnsupportedDistance);
            s.isCandidate.push_back(withinBox(pos, radius) ? 1 : 0);
        }
    };

    // Step 1: seed from the leaves touching the removed block, then flood the cluster.
    for (BlockPos const& offset : NeighborOffsets) {
        BlockPos const start{origin.x + offset.x, origin.y + offset.y, origin.z + offset.z};
        if (isLeaf(region.getBlock(start))) {
            addNode(start);
        }
    }

    for (std::size_t i = 0; i < s.positions.size(); ++i) {
        // s.positions may reallocate inside the loop, so work from a copy.
        BlockPos const current = s.positions[i];
        for (BlockPos const& offset : NeighborOffsets) {
            BlockPos const neighbor{current.x + offset.x, current.y + offset.y, current.z + offset.z};
            Block const&   block = region.getBlock(neighbor);

            if (isLeafSupportingWood(block)) {
                s.distance[i] = 0; // Touching wood: a support source for step 2.
                continue;
            }
            if (!isLeaf(block) || !withinBox(neighbor, floodExtent) || s.positions.size() >= MaxGraphNodes) {
                continue;
            }
            addNode(neighbor);
        }
    }

    // Step 2: spread support outwards from the wood-adjacent leaves.
    for (std::size_t i = 0; i < s.positions.size(); ++i) {
        if (s.distance[i] == 0) {
            s.frontier.push_back(static_cast<std::uint32_t>(i));
        }
    }

    for (int step = 0; step < MaxLogDistance - 1 && !s.frontier.empty(); ++step) {
        s.nextFrontier.clear();
        for (std::uint32_t const i : s.frontier) {
            BlockPos const current = s.positions[i];
            for (BlockPos const& offset : NeighborOffsets) {
                BlockPos const neighbor{current.x + offset.x, current.y + offset.y, current.z + offset.z};
                auto const     entry = s.index.find(neighbor);
                if (entry == s.index.end() || s.distance[entry->second] != UnsupportedDistance) {
                    continue;
                }
                s.distance[entry->second] = step + 1;
                s.nextFrontier.push_back(entry->second);
            }
        }
        s.frontier.swap(s.nextFrontier);
    }

    // Step 3: whatever support never reached is gone.
    DecayPassGuard const guard;
    for (std::size_t i = 0; i < s.positions.size(); ++i) {
        if (s.isCandidate[i] == 0 || s.distance[i] != UnsupportedDistance) {
            continue;
        }

        BlockPos const& leafPos = s.positions[i];
        Block const&    block   = region.getBlock(leafPos);
        if (!isLeaf(block) || isPersistentLeaf(block)) {
            continue;
        }
        region.getLevel().destroyBlock(region, leafPos, true, BlockChangeContext::environmentChange(leafPos));
    }
}

void onServerTick(ll::event::ServerLevelTickEvent&) {
    if (pendingDecays.empty()) {
        return;
    }

    auto pending = std::move(pendingDecays);
    pendingDecays.clear();
    pendingKeys.clear();

    int const radius = std::clamp(config.fastLeafDecay.radius, 1, MaxRadius);

    for (auto const& task : pending) {
        auto region = task.region.lock();
        if (!region || isLeafSupportingWood(region->getBlock(task.pos))) {
            continue;
        }
        decayUnsupportedLeaves(*region, task.pos, radius);
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
    queueDecay(region, pos);
}

LL_TYPE_INSTANCE_HOOK(
    LeavesRemovedHook,
    ll::memory::HookPriority::Normal,
    LeavesBlock,
    &LeavesBlock::$onRemove,
    void,
    BlockSource&    region,
    BlockPos const& pos
) {
    origin(region, pos);
    queueDecay(region, pos);
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
    if (LeavesRemovedHook::hook() != 0) {
        LogRemovedHook::unhook();
        return false;
    }

    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    if (!tickListener) {
        LogRemovedHook::unhook();
        LeavesRemovedHook::unhook();
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
    LeavesRemovedHook::unhook();
    if (tickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(tickListener);
        tickListener.reset();
    }
    pendingDecays.clear();
    pendingKeys.clear();
    scratch.clear();
}

} // namespace my_mod::fast_leaf_decay

#else

namespace my_mod::fast_leaf_decay {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::fast_leaf_decay

#endif
