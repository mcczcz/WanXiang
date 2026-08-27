#include "mod/CleanUnknownBlocks.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/ChunkBlockPos.h"
#include "mc/world/level/ChunkLocalHeight.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/chunk/LevelChunkEventManagerProxy.h"
#include "mc/world/level/dimension/Dimension.h"

#include <atomic>
#include <memory>
#include <string_view>

namespace my_mod::clean_unknown_blocks {
namespace {

std::atomic_bool enabled{false};

Block const* resolveAirBlock() {
    static Block const* airBlock = []() -> Block const* {
        auto air = Block::tryGetFromRegistry(HashedString{std::string_view{"minecraft:air"}});
        return air ? air.as_ptr() : nullptr;
    }();
    return airBlock;
}

bool shouldClean(Block const& block) { return block.getBlockType().mReturnDefaultBlockOnUnidentifiedBlockState; }

int cleanChunk(LevelChunk& levelChunk) {
    Block const* airBlock = resolveAirBlock();
    if (airBlock == nullptr) {
        return 0;
    }

    int const totalHeight = static_cast<int>(levelChunk.mDimension.getHeightInSubchunks()) * 16;
    if (totalHeight <= 0) {
        return 0;
    }

    BlockChangeContext const changeContext = BlockChangeContext::structureChange();
    int                      cleanedCount  = 0;

    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            for (int y = totalHeight - 1; y >= 0; --y) {
                ChunkBlockPos const pos{
                    static_cast<uchar>(x),
                    ChunkLocalHeight{static_cast<short>(y)},
                    static_cast<uchar>(z),
                };

                Block const& block = levelChunk.getBlock(pos);
                if (!shouldClean(block)) {
                    continue;
                }

                levelChunk.setBlock(pos, *airBlock, nullptr, std::shared_ptr<BlockActor>{}, changeContext);
                ++cleanedCount;
            }
        }
    }

    return cleanedCount;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    CleanerUnknownBlockHook,
    ll::memory::HookPriority::Normal,
    LevelChunkEventManagerProxy,
    &LevelChunkEventManagerProxy::$setOnChunkLoadedCalled,
    bool,
    LevelChunk& levelChunk
) {
    int const cleanedCount = cleanChunk(levelChunk);
    if (cleanedCount > 0) {
        ChunkPos const& chunkPos = levelChunk.getPosition();
        logger.info(tr("clean_unknown_blocks.cleaned", cleanedCount, chunkPos.x, chunkPos.z));
    }
    return origin(levelChunk);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.cleanUnknownBlocksEnable) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (CleanerUnknownBlockHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    CleanerUnknownBlockHook::unhook();
}

} // namespace my_mod::clean_unknown_blocks

#else

namespace my_mod::clean_unknown_blocks {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::clean_unknown_blocks

#endif
