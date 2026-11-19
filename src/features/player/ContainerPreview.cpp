#include "ContainerPreview.h"

#ifdef LL_PLAT_S

#include "core/FeatureDiagnostics.h"
#include "core/Global.h"

#include "catalyst/event/server/ReceivePacketEvent.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/entity/components/PackedItemUseLegacyInventoryTransaction.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/packet/BlockActorDataPacket.h"
#include "mc/network/packet/ContainerClosePacket.h"
#include "mc/network/packet/ContainerOpenPacket.h"
#include "mc/network/packet/InventoryContentPacket.h"
#include "mc/network/packet/InventoryTransactionPacket.h"
#include "mc/network/packet/ItemStackRequestPacket.h"
#include "mc/network/packet/ItemStackResponseContainerInfo.h"
#include "mc/network/packet/ItemStackResponseInfo.h"
#include "mc/network/packet/ItemStackResponsePacket.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/network/packet/UpdateBlockPacket.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

namespace my_mod::container_preview {
namespace {

using ContainerType = SharedTypes::Legacy::ContainerType;

struct Preview {
    DimensionType           dimension;
    BlockPos                main;
    std::optional<BlockPos> paired;
    ContainerID             id = ContainerID::None;
    std::vector<ItemStack>  items;
    int                     ticksUntilOpen = 2;
};

std::unordered_map<ActorUniqueID, Preview> previews;
ll::event::ListenerPtr                     receiveListener;
ll::event::ListenerPtr                     tickListener;

void sendBlock(Player& player, BlockPos const& pos, Block const& block) {
    auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::UpdateBlock);
    auto& update = static_cast<UpdateBlockPacket&>(*packet);
    update.mPos         = pos;
    update.mLayer       = 0;
    update.mUpdateFlags = 3;
    update.mRuntimeId   = block.mNetworkId;
    player.sendNetworkPacket(update);
}

void sendChestActor(
    Player&                        player,
    BlockPos const&                pos,
    std::optional<BlockPos> const& paired,
    bool                           lead
) {
    BlockActorDataPacketPayload payload{};
    payload.mPos = pos;
    auto& data = payload.mData.get();
    data.putString("id", "Chest");
    data.putInt("x", pos.x);
    data.putInt("y", pos.y);
    data.putInt("z", pos.z);
    data.putString("CustomName", tr("container_preview.title"));
    if (paired) {
        data.putInt("pairx", paired->x);
        data.putInt("pairz", paired->z);
        data.putByte("pairlead", lead);
    }
    BlockActorDataPacket packet(std::move(payload));
    packet.mSerializationMode = SerializationMode::CerealOnly;
    player.sendNetworkPacket(packet);
}

void restoreBlock(Player& player, BlockPos const& pos) {
    auto& region = player.getDimensionBlockSource();
    if (!region.hasChunksAt(pos, 0, true)) {
        return;
    }
    sendBlock(player, pos, region.getBlock(pos));
    if (auto* actor = region.getBlockEntity(pos)) {
        if (auto packet = actor->_getUpdatePacket(region)) {
            player.sendNetworkPacket(*packet);
        }
    }
}

void closePreview(Player& player, Preview const& preview, bool serverInitiated) {
    if (preview.id != ContainerID::None) {
        auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::ContainerClose);
        auto& close = static_cast<ContainerClosePacket&>(*packet);
        close.mContainerId          = preview.id;
        close.mContainerType        = ContainerType::Container;
        close.mServerInitiatedClose = serverInitiated;
        player.sendNetworkPacket(close);
    }

    if (static_cast<int>(player.getDimensionId()) == static_cast<int>(preview.dimension)) {
        restoreBlock(player, preview.main);
        if (preview.paired) {
            restoreBlock(player, *preview.paired);
        }
    }
}

std::optional<BlockPos> findPreviewPosition(Player& player, bool doubleChest) {
    auto const& region = player.getDimensionBlockSourceConst();
    auto const  feet   = player.getFeetBlockPos();
    // 双箱的两个方块放进同一个区块，避开玩家站立和行走的位置。
    auto const x = doubleChest && (feet.x & 15) == 15 ? feet.x - 1 : feet.x;
    for (int offset : std::array{-4, -5, 3, 4}) {
        BlockPos const pos{x, feet.y + offset, feet.z};
        BlockPos const paired{pos.x + 1, pos.y, pos.z};
        if (pos.y >= region.getMinHeight() && pos.y < region.getMaxHeight()
            && region.hasChunksAt(pos, 0, true) && !region.getBlockEntity(pos)
            && (!doubleChest || !region.getBlockEntity(paired))) {
            return pos;
        }
    }
    return std::nullopt;
}

bool preparePreview(Player& player, ChestBlockActor const& chest, bool doubleChest) {
    auto const fakeChest = Block::tryGetFromRegistry(VanillaBlockTypeIds::Chest());
    auto const pos = findPreviewPosition(player, doubleChest);
    if (!fakeChest || !pos) {
        return false;
    }

    Preview preview{player.getDimensionId(), *pos};
    auto const size = chest.getContainerSize();
    preview.items.reserve(size);
    for (int slot = 0; slot < size; ++slot) {
        preview.items.push_back(chest.getItem(slot));
    }

    if (doubleChest) {
        preview.paired = BlockPos{pos->x + 1, pos->y, pos->z};
    }
    sendBlock(player, preview.main, *fakeChest);
    if (preview.paired) {
        sendBlock(player, *preview.paired, *fakeChest);
        sendChestActor(player, *preview.paired, preview.main, false);
    }
    sendChestActor(player, preview.main, preview.paired, true);

    previews.emplace(player.getOrCreateUniqueID(), std::move(preview));
    return true;
}

void openPreview(ServerPlayer& player, Preview& preview) {
    // 从玩家原版窗口编号范围内取下一个编号，避免与下次打开的真实容器冲突。
    auto const nextId = static_cast<int>(player.mContainerCounter) + 1;
    preview.id = nextId >= static_cast<int>(ContainerID::First) && nextId <= static_cast<int>(ContainerID::Last)
                   ? static_cast<ContainerID>(nextId)
                   : ContainerID::First;
    player.mContainerCounter = preview.id;

    auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::ContainerOpen);
    auto& open = static_cast<ContainerOpenPacket&>(*packet);
    open.mContainerId    = preview.id;
    open.mType           = ContainerType::Container;
    open.mPos            = preview.main;
    open.mEntityUniqueID = ActorUniqueID::INVALID_ID();
    player.sendNetworkPacket(open);

    FullContainerName const name{ContainerEnumName::LevelEntityContainer, std::nullopt};
    InventoryContentPacket content(
        InventoryContentPacketPayload{preview.id, preview.items, name, ItemStack::EMPTY_ITEM()}
    );
    content.mSerializationMode = SerializationMode::CerealOnly;
    player.sendNetworkPacket(content);
    preview.items.clear();
}

LL_TYPE_INSTANCE_HOOK(
    ChestUseHook,
    ll::memory::HookPriority::Lowest,
    GameMode,
    &GameMode::$useItemOn,
    InteractionResult,
    ItemStack&      item,
    BlockPos const& pos,
    uchar           face,
    Vec3 const&     hit,
    HandSlot        hand,
    Block const*    targetBlock,
    bool            firstEvent
) {
    Player& player = mPlayer;
    auto& region = player.getDimensionBlockSource();
    auto const* actor = player.getDimensionBlockSourceConst().getBlockEntity(pos);
    if (!actor || actor->getType() != BlockActorType::Chest || player.isSpectator()
        || (player.isSneaking() && !item.isNull()) || player.mContainerManager.get()) {
        return origin(item, pos, face, hit, hand, targetBlock, firstEvent);
    }

    auto const& chest = static_cast<ChestBlockActor const&>(*actor);
    auto const* paired = chest.mLargeChestPaired;
    if (!chest._canOpenThis(region) || (paired && !paired->_canOpenThis(region))) {
        if (previews.contains(player.getOrCreateUniqueID()) || preparePreview(player, chest, paired != nullptr)) {
            return {.mSuccess = true, .mSwing = false};
        }
    }
    return origin(item, pos, face, hit, hand, targetBlock, firstEvent);
}

ItemStackResponseInfo rejectRequest(ItemStackRequestId const& id) {
    ItemStackResponseInfo response{};
    response.mResult          = ItemStackNetResult::Error;
    response.mClientRequestId = id;
    return response;
}

void onReceivePacket(Catalyst::ReceivePacketBeforeEvent& event) {
    auto* player = event.player();
    if (!player || event.isCancelled()) {
        return;
    }
    auto it = previews.find(player->getOrCreateUniqueID());
    if (it == previews.end() || it->second.id == ContainerID::None) {
        return;
    }

    auto const& packet = event.packet();
    switch (packet.getId()) {
    case MinecraftPacketIds::ContainerClose: {
        auto const& close = static_cast<ContainerClosePacket const&>(packet);
        if (close.mContainerId == it->second.id) {
            closePreview(*player, it->second, false);
            previews.erase(it);
            event.cancel();
        }
        break;
    }
    case MinecraftPacketIds::ItemStackRequest: {
        auto const& request = static_cast<ItemStackRequestPacket const&>(packet);
        auto reply = MinecraftPackets::createPacket(MinecraftPacketIds::ItemStackResponse);
        auto& responses = static_cast<ItemStackResponsePacket&>(*reply).mResponses.get();
        for (auto const& entry : request.mRequests.get()) {
            responses.push_back(rejectRequest(entry.mClientRequestId));
        }
        player->sendNetworkPacket(*reply);
        event.cancel();
        break;
    }
    case MinecraftPacketIds::PlayerAuthInputPacket: {
        // 移动包也能携带物品请求，仅移除其中的物品请求并保留移动输入。
        auto& input = const_cast<PlayerAuthInputPacket&>(static_cast<PlayerAuthInputPacket const&>(packet));
        if (input.mItemStackRequest.get()) {
            auto reply = MinecraftPackets::createPacket(MinecraftPacketIds::ItemStackResponse);
            auto& responses = static_cast<ItemStackResponsePacket&>(*reply).mResponses.get();
            responses.push_back(rejectRequest(input.mItemStackRequest->mClientRequestId));
            input.mItemStackRequest.reset();
            auto const flag = static_cast<int>(PlayerAuthInputPacket::InputData::PerformItemStackRequest);
            input.mInputData->mContainer.set(flag, false);
            player->sendNetworkPacket(*reply);
        }
        break;
    }
    case MinecraftPacketIds::InventoryTransaction: {
        auto const& transaction = static_cast<InventoryTransactionPacket const&>(packet);
        if (std::holds_alternative<NormalTransactionData>(transaction.mVariantTransaction.get())) {
            player->sendInventory(false);
            event.cancel();
        }
        break;
    }
    default:
        break;
    }
}

void onServerTick(ll::event::ServerLevelTickEvent& event) {
    for (auto it = previews.begin(); it != previews.end();) {
        auto& preview = it->second;
        auto* player = event.level().getPlayer(it->first);
        if (!player || static_cast<int>(player->getDimensionId()) != static_cast<int>(preview.dimension)) {
            it = previews.erase(it);
            continue;
        }
        if (!player->isAlive() || player->mContainerManager.get()) {
            closePreview(*player, preview, true);
            it = previews.erase(it);
            continue;
        }
        // 客户端先建立虚拟箱子的方块实体，再处理开窗和物品内容。
        if (preview.id == ContainerID::None && --preview.ticksUntilOpen == 0) {
            openPreview(static_cast<ServerPlayer&>(*player), preview);
        }
        ++it;
    }
}

} // namespace

bool enable() {
    if (!config.containerPreviewEnable || receiveListener) {
        return true;
    }
    auto& bus = ll::event::EventBus::getInstance();
    receiveListener = bus.emplaceListener<Catalyst::ReceivePacketBeforeEvent>(onReceivePacket);
    tickListener = bus.emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    return feature_diagnostics::checkEvent("container_preview", "ReceivePacketBeforeEvent", receiveListener != nullptr)
        && feature_diagnostics::checkEvent("container_preview", "ServerLevelTickEvent", tickListener != nullptr)
        && feature_diagnostics::checkHook("container_preview", "ChestUseHook", ChestUseHook::hook());
}

void disable() {
    if (auto level = ll::service::getLevel()) {
        for (auto const& [id, preview] : previews) {
            if (auto* player = level->getPlayer(id)) {
                closePreview(*player, preview, true);
            }
        }
    }
    previews.clear();
    ChestUseHook::unhook();
    auto& bus = ll::event::EventBus::getInstance();
    if (receiveListener) {
        bus.removeListener<Catalyst::ReceivePacketBeforeEvent>(receiveListener);
        receiveListener.reset();
    }
    if (tickListener) {
        bus.removeListener<ll::event::ServerLevelTickEvent>(tickListener);
        tickListener.reset();
    }
}

} // namespace my_mod::container_preview

#else

namespace my_mod::container_preview {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::container_preview

#endif
