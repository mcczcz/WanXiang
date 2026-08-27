#include "mod/ServerPong.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "catalyst/event/server/ServerPongEvent.h"
#include "ll/api/event/EventBus.h"

namespace my_mod::server_pong {
namespace {

ll::event::ListenerPtr listener;

void onServerPong(Catalyst::ServerPongBeforeEvent& event) {
    auto const& serverPongConfig = config.serverPong;
    if (!serverPongConfig.enable) {
        return;
    }
    if (serverPongConfig.motdEnable) {
        event.motd() = serverPongConfig.motd;
    }
    if (serverPongConfig.playerCountEnable) {
        event.playerCount() = serverPongConfig.playerCount;
    }
    if (serverPongConfig.maxPlayerCountEnable) {
        event.maxPlayerCount() = serverPongConfig.maxPlayerCount;
    }
}

} // namespace

bool enable() {
    if (!config.serverPong.enable || listener) {
        return true;
    }

    listener = ll::event::EventBus::getInstance().emplaceListener<Catalyst::ServerPongBeforeEvent>(onServerPong);
    return listener != nullptr;
}

void disable() {
    if (!listener) {
        return;
    }

    ll::event::EventBus::getInstance().removeListener<Catalyst::ServerPongBeforeEvent>(listener);
    listener.reset();
}

} // namespace my_mod::server_pong

#else

namespace my_mod::server_pong {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::server_pong

#endif
