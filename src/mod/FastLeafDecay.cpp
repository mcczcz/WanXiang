#include "mod/FastLeafDecay.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/coro/Executor.h"
#include "ll/api/data/CancellableCallback.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/deps/game_refs/WeakRef.h"
#include "mc/util/Random.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockProperty.h"
#include "mc/world/level/block/LeavesBlock.h"
#include "mc/world/level/block/LogBlock.h"
#include "mc/world/level/block/block_events/BlockRandomTickEvent.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

namespace my_mod::fast_leaf_decay {
namespace {

// 一个游戏刻。executeAfter 收的是真实时间，所以配置里的“刻”要在这里换算。
constexpr auto TickDuration = std::chrono::milliseconds{50};

std::atomic_bool enabled{false};

// 已经排队等待检查的树叶。存在的意义有两个：
//   1. 去重。一棵树倒下时，同一片叶子会被相邻的多个方块反复唤醒，没有这张表的话
//      每片叶子会被排上几十次队，连锁腐烂会指数级放大成一场自我 DoS。
//   2. 让 disable() 能把还没触发的回调全部取消 —— 回调持有 WeakRef，插件卸载后
//      再触发就是往已经析构的世界里写方块。
struct PendingKey {
    DimensionType dimension;
    int           x;
    int           y;
    int           z;

    bool operator==(PendingKey const& other) const noexcept {
        return dimension == other.dimension && x == other.x && y == other.y && z == other.z;
    }
};

struct PendingKeyHash {
    std::size_t operator()(PendingKey const& key) const noexcept {
        std::size_t seed = std::hash<DimensionType>{}(key.dimension);
        for (int const value : {key.x, key.y, key.z}) {
            seed ^= std::hash<int>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
        }
        return seed;
    }
};

std::mutex pendingMutex;
std::unordered_map<PendingKey, std::shared_ptr<ll::data::CancellableCallback>, PendingKeyHash> pending;

int randomDelayTicks() {
    auto const& decayConfig = config.fastLeafDecay;

    int const low  = std::max(decayConfig.minDelayTicks, 0);
    int const high = std::max(decayConfig.maxDelayTicks, low);
    if (low == high) {
        return low;
    }

    // 只有服务端主线程会走到这里，但 thread_local 不要钱，省得以后被别的线程调用时踩坑。
    static thread_local std::mt19937 engine{std::random_device{}()};
    return std::uniform_int_distribution<int>{low, high}(engine);
}

bool isLeaves(Block const& block) { return block.hasProperty(BlockProperty::Leaves); }

// 手动补一次随机刻。原版腐烂完全依赖随机刻抽中这一格，概率极低，所以砍完树要等很久；
// 这里不改判定逻辑，只是把“抽中”这件事直接送到叶子面前，原版的“六格内有没有原木”
// 判定照常执行，该留的叶子还是会留下。
void forceDecayCheck(BlockSource& region, BlockPos const& pos) {
    Block const& block = region.getBlock(pos);
    if (!isLeaves(block)) {
        return;
    }

    // hasProperty(Leaves) 只在 LeavesBlock 及其子类（MangroveLeavesBlock 等）上成立，
    // 而 randomTick 是非虚成员函数，所以这里的下行转换是安全的。
    auto const& leaves = static_cast<LeavesBlock const&>(block.getBlockType());

    BlockEvents::BlockRandomTickEvent event{pos, region, Random::mThreadLocalRandom()};
    leaves.randomTick(event);
}

void scheduleDecayCheck(WeakRef<BlockSource> const& regionRef, DimensionType dimension, BlockPos const& pos) {
    PendingKey const key{dimension, pos.x, pos.y, pos.z};

    {
        std::lock_guard const lock{pendingMutex};
        if (pending.contains(key)) {
            return;
        }
        // 先占位再排队：executeAfter 的回调有可能在本函数返回前就跑完（延迟为 0 刻时），
        // 那时它会来 erase 这个 key，所以占位必须发生在排队之前。
        pending.emplace(key, nullptr);
    }

    auto handle = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
        [regionRef, key, pos] {
            {
                std::lock_guard const lock{pendingMutex};
                // 先摘掉自己，再去 randomTick。腐烂会递归触发 onRemove -> scheduleDecayCheck，
                // 那条路径同样要拿这把锁，绝不能在持锁状态下调进去。
                pending.erase(key);
            }

            if (!enabled.load(std::memory_order_relaxed)) {
                return;
            }

            // 维度可能在这几刻里被卸载。GME 那份实现直接把 BlockSource& 捕获进 lambda，
            // 世界一关就是野指针；这里改成每次重新解引用。
            auto region = regionRef.lock();
            if (!region) {
                return;
            }

            forceDecayCheck(*region, pos);
        },
        TickDuration * randomDelayTicks()
    );

    std::lock_guard const lock{pendingMutex};
    auto const            slot = pending.find(key);
    if (slot == pending.end()) {
        // 回调已经跑完并清掉了占位，没有什么可取消的了。
        return;
    }
    slot->second = std::move(handle);
}

// 唤醒被破坏方块周围 3x3x3 内的所有树叶。腐烂掉的叶子自己也会触发 LeavesBlock::onRemove，
// 于是外圈叶子被继续唤醒，整棵树以扩散的方式掉光 —— 不需要在这里做 BFS。
void wakeNeighbourLeaves(BlockSource& region, BlockPos const& origin) {
    if (!enabled.load(std::memory_order_relaxed)) {
        return;
    }

    Dimension& dimension = region.getDimension();
    // 只处理主区块源。结构生成 / 传送门搭建等临时 BlockSource 上的方块变动跟玩家砍树无关，
    // 而且回调里拿到的永远是主区块源，对不上就不该排队。
    if (&region != &dimension.getBlockSourceFromMainChunkSource()) {
        return;
    }

    WeakRef<BlockSource> const regionRef = region.getWeakRef();
    DimensionType const      dimensionId  = region.getDimensionId();

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) {
                    continue;
                }

                BlockPos const pos{origin.x + dx, origin.y + dy, origin.z + dz};
                if (!isLeaves(region.getBlock(pos))) {
                    continue;
                }
                scheduleDecayCheck(regionRef, dimensionId, pos);
            }
        }
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    LeavesBlockOnRemoveHook,
    ll::memory::HookPriority::Normal,
    LeavesBlock,
    &LeavesBlock::$onRemove,
    void,
    ::BlockSource&   region,
    ::BlockPos const& pos
) {
    origin(region, pos);
    wakeNeighbourLeaves(region, pos);
}

LL_TYPE_INSTANCE_HOOK(
    LogBlockOnRemoveHook,
    ll::memory::HookPriority::Normal,
    LogBlock,
    &LogBlock::$onRemove,
    void,
    ::BlockSource&   region,
    ::BlockPos const& pos
) {
    origin(region, pos);
    wakeNeighbourLeaves(region, pos);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.fastLeafDecay.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }

    if (LeavesBlockOnRemoveHook::hook() != 0) {
        return false;
    }
    if (LogBlockOnRemoveHook::hook() != 0) {
        LeavesBlockOnRemoveHook::unhook();
        return false;
    }

    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }

    LogBlockOnRemoveHook::unhook();
    LeavesBlockOnRemoveHook::unhook();

    // 取消所有还没触发的回调。它们捕获了 WeakRef，放着不管虽然不会崩，
    // 但插件卸载后继续改方块显然不是我们想要的行为。
    std::vector<std::shared_ptr<ll::data::CancellableCallback>> outstanding;
    {
        std::lock_guard const lock{pendingMutex};
        outstanding.reserve(pending.size());
        for (auto& [key, handle] : pending) {
            if (handle) {
                outstanding.emplace_back(std::move(handle));
            }
        }
        pending.clear();
    }
    for (auto const& handle : outstanding) {
        handle->cancel();
    }
}

} // namespace my_mod::fast_leaf_decay

#else

namespace my_mod::fast_leaf_decay {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::fast_leaf_decay

#endif
