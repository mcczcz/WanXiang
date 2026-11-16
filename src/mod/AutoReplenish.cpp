#include "mod/AutoReplenish.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#include "mc/world/inventory/transaction/ItemUseOnActorInventoryTransaction.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"

#include <optional>
#include <unordered_map>
#include <utility>

namespace my_mod::auto_replenish {
namespace {

struct RefillRequest {
    int   slot;
    short itemId;
    short aux;
    bool  damageable;
};

std::unordered_map<ActorUniqueID, RefillRequest> pendingRefills;
ll::event::ListenerPtr                         tickListener;
size_t                                         installedHooks = 0;

std::optional<RefillRequest> capture(Player const& player) {
    auto const& item = player.getSelectedItem();
    if (item.isNull()) {
        return std::nullopt;
    }
    return RefillRequest{player.getSelectedItemSlot(), item.getId(), item.getAuxValue(), item.isDamageableItem()};
}

void queueIfEmpty(Player& player, std::optional<RefillRequest> const& request) {
    if (request && player.getSelectedItemSlot() == request->slot && player.getSelectedItem().isNull()) {
        pendingRefills.insert_or_assign(player.getOrCreateUniqueID(), *request);
    }
}

void onServerTick(ll::event::ServerLevelTickEvent& event) {
    // 等原版使用动作和背包事务结束后再移动物品，避免后续写回覆盖补充结果。
    auto requests = std::move(pendingRefills);
    pendingRefills.clear();
    for (auto const& [playerId, request] : requests) {
        auto* player = event.level().getPlayer(playerId);
        if (!player || !player->isAlive() || player->getSelectedItemSlot() != request.slot
            || !player->getSelectedItem().isNull()) {
            continue;
        }

        auto& inventory = player->getInventory();
        for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
            auto const& item = inventory.getItem(slot);
            if (slot == request.slot || item.isNull() || item.getId() != request.itemId
                || (!request.damageable && item.getAuxValue() != request.aux)) {
                continue;
            }

            ItemStack replacement{item};
            inventory.setItem(slot, ItemStack::EMPTY_ITEM());
            inventory.setItem(request.slot, replacement);
            player->sendInventory(false);
            break;
        }
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    ItemUseHook,
    ll::memory::HookPriority::Normal,
    ItemUseInventoryTransaction,
    &ItemUseInventoryTransaction::$handle,
    InventoryTransactionError,
    Player& player,
    bool    isSenderAuthority
) {
    auto const request = mHand == HandSlot::Mainhand && mSlot == player.getSelectedItemSlot()
                           ? capture(player)
                           : std::nullopt;
    auto const result = origin(player, isSenderAuthority);
    if (result == InventoryTransactionError::NoError) {
        queueIfEmpty(player, request);
    }
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ItemUseOnActorHook,
    ll::memory::HookPriority::Normal,
    ItemUseOnActorInventoryTransaction,
    &ItemUseOnActorInventoryTransaction::$handle,
    InventoryTransactionError,
    Player& player,
    bool    isSenderAuthority
) {
    auto const request = mHand == HandSlot::Mainhand && mSlot == player.getSelectedItemSlot()
                           ? capture(player)
                           : std::nullopt;
    auto const result = origin(player, isSenderAuthority);
    if (result == InventoryTransactionError::NoError) {
        queueIfEmpty(player, request);
    }
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    CompleteUsingItemHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::completeUsingItem,
    void
) {
    auto const request = capture(*this);
    origin();
    queueIfEmpty(*this, request);
}

LL_TYPE_INSTANCE_HOOK(
    ReleaseUsingItemHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::releaseUsingItem,
    void
) {
    auto const request = capture(*this);
    origin();
    queueIfEmpty(*this, request);
}

LL_TYPE_INSTANCE_HOOK(
    ItemBreakHook,
    ll::memory::HookPriority::Normal,
    ItemStackBase,
    &ItemStackBase::hurtAndBreak,
    bool,
    int    deltaDamage,
    Actor* owner
) {
    Player* player = owner && owner->isPlayer() ? static_cast<Player*>(owner) : nullptr;
    auto const request = player && static_cast<ItemStackBase const*>(this) == &player->getSelectedItem()
                           ? capture(*player)
                           : std::nullopt;
    bool const broken = origin(deltaDamage, owner);
    if (broken && request) {
        // 耐久函数的调用方可能尚未清空主手，在下一刻确认槽位为空后补充。
        pendingRefills.insert_or_assign(player->getOrCreateUniqueID(), *request);
    }
    return broken;
}
#pragma clang diagnostic pop

struct Hook {
    int (*install)();
    void (*remove)();
};

Hook const hooks[] = {
    {[] { return ItemUseHook::hook(); }, [] { ItemUseHook::unhook(); }},
    {[] { return ItemUseOnActorHook::hook(); }, [] { ItemUseOnActorHook::unhook(); }},
    {[] { return CompleteUsingItemHook::hook(); }, [] { CompleteUsingItemHook::unhook(); }},
    {[] { return ReleaseUsingItemHook::hook(); }, [] { ReleaseUsingItemHook::unhook(); }},
    {[] { return ItemBreakHook::hook(); }, [] { ItemBreakHook::unhook(); }},
};

} // namespace

bool enable() {
    if (!config.autoReplenishEnable || tickListener) {
        return true;
    }
    for (auto const& hook : hooks) {
        if (hook.install() != 0) {
            disable();
            return false;
        }
        ++installedHooks;
    }
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    if (!tickListener) {
        disable();
        return false;
    }
    return true;
}

void disable() {
    if (tickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(tickListener);
        tickListener.reset();
    }
    while (installedHooks > 0) {
        hooks[--installedHooks].remove();
    }
    pendingRefills.clear();
}

} // namespace my_mod::auto_replenish

#else

namespace my_mod::auto_replenish {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::auto_replenish

#endif
