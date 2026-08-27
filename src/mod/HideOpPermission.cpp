#include "mod/HideOpPermission.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "catalyst/event/server/SendMultiplePacketEvent.h"
#include "catalyst/event/server/SendPacketEvent.h"
#include "ll/api/event/EventBus.h"
#include "mc/network/NetworkIdentifierWithSubId.h"
#include "mc/network/packet/UpdateAbilitiesPacket.h"
#include "mc/server/ServerPlayer.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/PlayerPermissionLevel.h"
#include "mc/world/actor/player/LayeredAbilities.h"

#include <atomic>

namespace my_mod::hide_op_permission {
namespace {

std::atomic_bool       enabled{false};
ll::event::ListenerPtr listener;
ll::event::ListenerPtr multipleListener;
thread_local bool      resendingSanitizedPacket = false;

UpdateAbilitiesPacket const* getOperatorAbilitiesPacket(Packet const& packet) {
    if (packet.getId() != MinecraftPacketIds::UpdateAbilitiesPacket) {
        return nullptr;
    }

    auto const* abilitiesPacket = static_cast<UpdateAbilitiesPacket const*>(&packet);
    if (abilitiesPacket->mData->mPlayerPermissions != PlayerPermissionLevel::Operator) {
        return nullptr;
    }
    return abilitiesPacket;
}

UpdateAbilitiesPacket makeSanitizedPacket(UpdateAbilitiesPacket const& packet) {
    LayeredAbilities abilities;
    packet.mData->fillIn(abilities);
    abilities.setPlayerPermissions(PlayerPermissionLevel::Member);
    abilities.setCommandPermissions(CommandPermissionLevel::Any);
    return UpdateAbilitiesPacket{packet.mData->getTargetPlayer(), abilities};
}

void onSendPacket(Catalyst::SendPacketBeforeEvent& event) {
    if (resendingSanitizedPacket || event.player() == nullptr || event.player()->isOperator()) {
        return;
    }

    auto const* packet = getOperatorAbilitiesPacket(event.packet());
    if (packet == nullptr) {
        return;
    }

    auto sanitized = makeSanitizedPacket(*packet);
    event.cancel();

    resendingSanitizedPacket = true;
    sanitized.sendToClient(event.networkId(), event.subClientId());
    resendingSanitizedPacket = false;
}

void onSendMultiplePacket(Catalyst::SendMultiplePacketBeforeEvent& event) {
    if (resendingSanitizedPacket) {
        return;
    }

    auto const* packet = getOperatorAbilitiesPacket(event.packet());
    if (packet == nullptr) {
        return;
    }

    auto const& ids       = event.ids();
    auto const& players   = event.players();
    auto        sanitized = makeSanitizedPacket(*packet);

    // The event normally keeps IDs and players aligned. If it does not, keep
    // the original send rather than risking a dropped packet for any client.
    if (ids.size() != players.size()) {
        event.setCancelled(false);
        return;
    }

    event.cancel();
    resendingSanitizedPacket = true;
    for (std::size_t index = 0; index < ids.size() && index < players.size(); ++index) {
        if (players[index] != nullptr && !players[index]->isOperator()) {
            sanitized.sendToClient(ids[index]);
        } else {
            packet->sendToClient(ids[index]);
        }
    }
    resendingSanitizedPacket = false;
}

} // namespace

bool enable() {
    if (!config.hideOpPermissionEnable || listener) {
        return true;
    }

    auto& eventBus   = ll::event::EventBus::getInstance();
    listener         = eventBus.emplaceListener<Catalyst::SendPacketBeforeEvent>(onSendPacket);
    multipleListener = eventBus.emplaceListener<Catalyst::SendMultiplePacketBeforeEvent>(onSendMultiplePacket);
    if (!listener || !multipleListener) {
        if (listener) {
            eventBus.removeListener<Catalyst::SendPacketBeforeEvent>(listener);
            listener.reset();
        }
        if (multipleListener) {
            eventBus.removeListener<Catalyst::SendMultiplePacketBeforeEvent>(multipleListener);
            multipleListener.reset();
        }
        return false;
    }

    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!listener) {
        enabled.store(false, std::memory_order_relaxed);
        return;
    }

    auto& eventBus = ll::event::EventBus::getInstance();
    eventBus.removeListener<Catalyst::SendPacketBeforeEvent>(listener);
    listener.reset();
    if (multipleListener) {
        eventBus.removeListener<Catalyst::SendMultiplePacketBeforeEvent>(multipleListener);
        multipleListener.reset();
    }
    enabled.store(false, std::memory_order_relaxed);
}

} // namespace my_mod::hide_op_permission

#else

namespace my_mod::hide_op_permission {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::hide_op_permission

#endif
