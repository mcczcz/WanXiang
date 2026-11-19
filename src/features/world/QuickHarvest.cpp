#include "QuickHarvest.h"

#include "core/FeatureDiagnostics.h"
#include "core/Global.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/util/Random.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/block/ResourceDrops.h"
#include "mc/world/level/block/ResourceDropsContext.h"

#include <string_view>

namespace my_mod::quick_harvest {
namespace {

using ll::event::PlayerInteractBlockEvent;

struct Crop {
    std::string_view block;
    char const*      ageState;
    int              matureAge;
    std::string_view seed;
};

constexpr Crop crops[] = {
    {"minecraft:wheat",       "growth", 7, "minecraft:wheat_seeds"   },
    {"minecraft:carrots",     "growth", 7, "minecraft:carrot"        },
    {"minecraft:potatoes",    "growth", 7, "minecraft:potato"        },
    {"minecraft:beetroot",    "growth", 7, "minecraft:beetroot_seeds"},
    {"minecraft:nether_wart", "age",    3, "minecraft:nether_wart"   },
};

ll::event::ListenerPtr interactListener;

Block const* replant(BlockSource const& region, BlockPos const& pos, Block const& block, std::string_view& seed) {
    auto const& name = block.getTypeName();
    if (name == "minecraft:torchflower"
        && region.getBlock({pos.x, pos.y - 1, pos.z}).getTypeName() == "minecraft:farmland") {
        seed = "minecraft:torchflower_seeds";
        return Block::tryGetFromRegistry(HashedString{"minecraft:torchflower_crop"}).as_ptr();
    }
    for (auto const& crop : crops) {
        if (name != crop.block) {
            continue;
        }
        auto const& states = block.mSerializationId->at("states").get<CompoundTag>();
        if (states.contains(crop.ageState) && states.at(crop.ageState).get<int>() == crop.matureAge) {
            seed = crop.seed;
            return block.getBlockType().mDefaultState;
        }
        break;
    }
    return nullptr;
}

bool harvestBlock(
    Player&          player,
    BlockSource&     region,
    BlockPos const&  pos,
    ItemStack const& tool,
    Block const*     replacement,
    std::string_view seed = {}
) {
    if (!region.checkBlockDestroyPermissions(player, pos, tool, false)) {
        return false;
    }

    auto const& block = region.getBlock(pos);
    ResourceDropsContext const
         dropsContext{ResourceDropsCause::PlayerMining, 0.0F, tool, pos, region.getDimensionId(), region};
    auto drops = block.getBlockType().getResourceDrops(block, player.getLevel().getRandom(), dropsContext);

    BlockChangeContext changeContext{};
    *changeContext.mContextSource           = ActorChangeContext{&player};
    changeContext.mTickSchedulingEnabled    = true;
    constexpr int updateNeighborsAndClients = 3;
    bool const    changed = replacement
                              ? region.setBlock(pos, *replacement, updateNeighborsAndClients, nullptr, changeContext)
                              : region.removeBlock(pos, changeContext);
    if (!changed) {
        return false;
    }

    for (auto& drop : *drops.mItems) {
        if (drop.mCount > 0 && !seed.empty() && drop.getTypeName() == seed) {
            drop.remove(1);
            seed = {};
        }
        if (drop.mCount > 0) {
            BlockType::popResource(region, pos, drop, &player);
        }
    }
    return true;
}

bool harvestColumn(
    Player&          player,
    BlockSource&     region,
    BlockPos const&  pos,
    ItemStack const& tool,
    std::string_view name
) {
    auto bottom = pos;
    while (bottom.y > region.getMinHeight() && region.getBlock({pos.x, bottom.y - 1, pos.z}).getTypeName() == name) {
        --bottom.y;
    }
    auto top = pos;
    while (top.y + 1 < region.getMaxHeight() && region.getBlock({pos.x, top.y + 1, pos.z}).getTypeName() == name) {
        ++top.y;
    }

    // 从上往下收割，避免上方的方块因失去支撑而自行掉落。
    bool harvested = false;
    for (; top.y > bottom.y; --top.y) {
        if (!harvestBlock(player, region, top, tool, nullptr)) {
            break;
        }
        harvested = true;
    }
    return harvested;
}

void onInteract(PlayerInteractBlockEvent& event) {
    auto& player = event.self();
    if (event.isCancelled() || player.getLevel().isClientSide() || player.isSpectator() || player.isSneaking()) {
        return;
    }

    auto&       region = player.getDimensionBlockSource();
    auto const& pos    = event.blockPos();
    auto const& block  = region.getBlock(pos);
    auto const& name   = block.getTypeName();
    if (!region.canDoBlockDrops(&player)) {
        return;
    }

    std::string_view seed;
    if (auto const* replacement = replant(region, pos, block, seed)) {
        if (harvestBlock(player, region, pos, event.item(), replacement, seed)) {
            event.cancel();
        }
    } else if (
        name == "minecraft:reeds" || name == "minecraft:cactus" || name == "minecraft:bamboo"
        || name == "minecraft:kelp"
    ) {
        if (harvestColumn(player, region, pos, event.item(), name)) {
            event.cancel();
        }
    }
}

} // namespace

bool enable() {
    if (!config.quickHarvestEnable || interactListener) {
        return true;
    }
    interactListener = ll::event::EventBus::getInstance().emplaceListener<PlayerInteractBlockEvent>(
        onInteract,
        ll::event::EventPriority::Lowest
    );
    return feature_diagnostics::checkEvent("quick_harvest", "PlayerInteractBlockEvent", interactListener != nullptr);
}

void disable() {
    if (interactListener) {
        ll::event::EventBus::getInstance().removeListener<PlayerInteractBlockEvent>(interactListener);
        interactListener.reset();
    }
}

} // namespace my_mod::quick_harvest
