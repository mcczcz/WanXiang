#include "mod/ClientAchievements.h"

#ifdef LL_PLAT_S

#include "mod/FeatureDiagnostics.h"
#include "mod/Global.h"

#include "catalyst/event/server/SendMultiplePacketEvent.h"
#include "catalyst/event/server/SendPacketEvent.h"
#include "ll/api/base/ScopedValue.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/network/NetworkIdentifierWithSubId.h"
#include "mc/network/packet/SetCommandsEnabledPacket.h"
#include "mc/network/packet/StartGamePacket.h"

#include <atomic>

namespace my_mod::client_achievements {
namespace {

ll::event::ListenerPtr listener;
ll::event::ListenerPtr multipleListener;
thread_local bool     resendingPacket = false;
std::atomic_bool      enabled{false};

template <class Write>
void writeStartGame(StartGamePacket const& packet, Write&& write) {
    if (!enabled.load(std::memory_order_relaxed) || !config.clientAchievementsEnable) {
        return write();
    }

    auto& settings = const_cast<LevelSettings&>(*packet.mSettings);
    logger.debug(
        "Client achievements: encoding StartGame (cheats={}, commands={}, achievementsDisabled={}) as false/false/false",
        settings.mCheatsEnabled,
        settings.mCommandsEnabled,
        settings.mAchievementsDisabled
    );
    ll::ScopedValue cheats(settings.mCheatsEnabled, false);
    ll::ScopedValue commands(settings.mCommandsEnabled, false);
    ll::ScopedValue achievements(settings.mAchievementsDisabled, false);
    write();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    StartGameWriteHook,
    ll::memory::HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    writeStartGame(*this, [&] { origin(stream); });
}

LL_TYPE_INSTANCE_HOOK(
    StartGameSerializationHook,
    ll::memory::HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$writeWithSerializationMode,
    void,
    BinaryStream& stream,
    cereal::ReflectionCtx const& reflectionCtx,
    std::optional<SerializationMode> overrideMode
) {
    writeStartGame(*this, [&] { origin(stream, reflectionCtx, overrideMode); });
}
#pragma clang diagnostic pop

bool shouldModify(Packet const& packet) {
    return !resendingPacket && enabled.load(std::memory_order_relaxed) && config.clientAchievementsEnable
        && packet.getId() == MinecraftPacketIds::SetCommandsEnabled;
}

template <class Send>
void sendWithClientAchievements(Packet const& packet, Send&& send) {
    ll::ScopedValue resending(resendingPacket, true);
    auto& commandsEnabled = const_cast<SetCommandsEnabledPacket&>(
        static_cast<SetCommandsEnabledPacket const&>(packet)
    ).mCommandsEnabled;
    ll::ScopedValue commands(commandsEnabled, false);
    send();
}

void onSendPacket(Catalyst::SendPacketBeforeEvent& event) {
    if (!shouldModify(event.packet())) {
        return;
    }
    event.cancel();
    sendWithClientAchievements(event.packet(), [&] {
        event.packet().sendToClient(event.networkId(), event.subClientId());
    });
}

void onSendMultiplePacket(Catalyst::SendMultiplePacketBeforeEvent& event) {
    if (!shouldModify(event.packet())) {
        return;
    }
    event.cancel();
    sendWithClientAchievements(event.packet(), [&] {
        for (auto const& id : event.ids()) {
            event.packet().sendToClient(id);
        }
    });
}

} // namespace

bool enable() {
    if (!config.clientAchievementsEnable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (!feature_diagnostics::checkHook("client_achievements", "StartGameWriteHook", StartGameWriteHook::hook())) {
        return false;
    }
    if (!feature_diagnostics::checkHook(
            "client_achievements",
            "StartGameSerializationHook",
            StartGameSerializationHook::hook()
        )) {
        StartGameWriteHook::unhook();
        return false;
    }

    enabled.store(true, std::memory_order_relaxed);
    auto& eventBus   = ll::event::EventBus::getInstance();
    listener         = eventBus.emplaceListener<Catalyst::SendPacketBeforeEvent>(onSendPacket);
    multipleListener = eventBus.emplaceListener<Catalyst::SendMultiplePacketBeforeEvent>(onSendMultiplePacket);
    if (!listener || !multipleListener) {
        feature_diagnostics::checkEvent("client_achievements", "Catalyst::SendPacketBeforeEvent", listener != nullptr);
        feature_diagnostics::checkEvent(
            "client_achievements",
            "Catalyst::SendMultiplePacketBeforeEvent",
            multipleListener != nullptr
        );
        disable();
        return false;
    }
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    auto& eventBus = ll::event::EventBus::getInstance();
    if (multipleListener) {
        eventBus.removeListener<Catalyst::SendMultiplePacketBeforeEvent>(multipleListener);
        multipleListener.reset();
    }
    if (listener) {
        eventBus.removeListener<Catalyst::SendPacketBeforeEvent>(listener);
        listener.reset();
    }
    StartGameSerializationHook::unhook();
    StartGameWriteHook::unhook();
}

} // namespace my_mod::client_achievements

#else

namespace my_mod::client_achievements {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::client_achievements

#endif
