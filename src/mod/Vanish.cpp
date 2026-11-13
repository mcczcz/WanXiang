#include "mod/Vanish.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/command/Command.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/packet/AddPlayerPacket.h"
#include "mc/network/packet/RemoveActorPacket.h"
#include "mc/server/ServerPlayer.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/level/Level.h"

#include <exception>
#include <unordered_set>
#include <utility>

namespace my_mod::vanish {
namespace {

bool                              commandRegistered = false;
bool                              active            = false;
std::unordered_set<ActorUniqueID> vanishedPlayers;
ll::event::ListenerPtr            disconnectListener;

void hidePlayer(Player& player) {
    RemoveActorPacket packet;
    packet.mEntityId          = player.getOrCreateUniqueID();
    packet.mSerializationMode = SerializationMode::CerealOnly;
    player.getLevel().forEachPlayer([&](Player& viewer) {
        if (&viewer != &player) {
            viewer.sendNetworkPacket(packet);
        }
        return true;
    });
}

void showPlayer(Player& player) {
    AddPlayerPacket packet(player);
    auto const&     id = player.getOrCreateUniqueID();
    player.getLevel().forEachPlayer([&](Player& viewer) {
        if (&viewer != &player && viewer.getDimensionId() == player.getDimensionId()) {
            // 只恢复仍在追踪该玩家的客户端，避免在视距外创建残留实体。
            auto& nearbyActors = *static_cast<ServerPlayer&>(viewer).mNearbyActors;
            auto  it           = nearbyActors.find(id);
            if (it != nearbyActors.end() && it->second.state == ServerPlayer::NearbyActor::State::Exist) {
                viewer.sendNetworkPacket(packet);
            }
        }
        return true;
    });
}

void executeCommand(CommandOrigin const& origin, CommandOutput& output) {
    if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors) {
        output.error(tr("vanish.op_only"));
        return;
    }
    if (!active) {
        output.error(tr("vanish.disabled"));
        return;
    }
    auto* entity = origin.getEntity();
    if (!entity || !entity->isPlayer()) {
        output.error(tr("vanish.player_only"));
        return;
    }
    auto&       player = *static_cast<Player*>(entity);
    auto const& id     = player.getOrCreateUniqueID();
    if (vanishedPlayers.erase(id) != 0) {
        showPlayer(player);
        output.success(tr("vanish.off"));
    } else {
        vanishedPlayers.insert(id);
        hidePlayer(player);
        output.success(tr("vanish.on"));
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
// SDK 未导出 Player::$tryCreateAddActorPacket，直接解析其对应的原版虚函数符号。
LL_TYPE_INSTANCE_HOOK(
    PlayerTryCreateAddActorPacketHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$tryCreateAddActorPacket,
    std::unique_ptr<Packet>
) {
    if (vanishedPlayers.contains(getOrCreateUniqueID())) {
        return nullptr;
    }
    return origin();
}


} // namespace

bool enable() {
    if (active) {
        return true;
    }
    try {
        if (!commandRegistered) {
            auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
                "vanish",
                tr("vanish.description"),
                CommandPermissionLevel::GameDirectors
            );
            command.overload<ll::command::EmptyParam>().execute([](CommandOrigin const& origin, CommandOutput& output) {
                executeCommand(origin, output);
            });
            commandRegistered = true;
        }
        if (!config.vanishEnable) {
            return true;
        }
        if (PlayerTryCreateAddActorPacketHook::hook() != 0) {
            return false;
        }
        active             = true;
        disconnectListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerDisconnectEvent>(
            [](ll::event::PlayerDisconnectEvent& event) { vanishedPlayers.erase(event.self().getOrCreateUniqueID()); }
        );
        if (!disconnectListener) {
            disable();
            return false;
        }
        return true;
    } catch (std::exception const& exception) {
        disable();
        logger.error(tr("error.generic", exception.what()));
        return false;
    }
}

void disable() {
    if (!active) {
        return;
    }
    active = false;
    PlayerTryCreateAddActorPacketHook::unhook();
    if (disconnectListener) {
        ll::event::EventBus::getInstance().removeListener(disconnectListener);
        disconnectListener.reset();
    }
    auto players = std::move(vanishedPlayers);
    vanishedPlayers.clear();
    if (auto level = ll::service::getLevel(); level && !players.empty()) {
        level->forEachPlayer([&](Player& player) {
            if (players.contains(player.getOrCreateUniqueID())) {
                showPlayer(player);
            }
            return true;
        });
    }
}

} // namespace my_mod::vanish

#else

namespace my_mod::vanish {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::vanish

#endif
