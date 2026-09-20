#include "mod/MobBlockDestruction.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "catalyst/event/actor/ActorDestroyBlockEvent.h"
#include "catalyst/event/actor/WitherDestroyEvent.h"
#include "ll/api/event/EventBus.h"

#include <string_view>

namespace my_mod::mob_block_destruction {
namespace {

constexpr std::string_view RavagerType = "minecraft:ravager";

ll::event::ListenerPtr actorDestroyBlockListener;
ll::event::ListenerPtr witherDestroyBlocksListener;

void onActorDestroyBlock(Catalyst::ActorDestroyBlockBeforeEvent& event) {
    auto const& destructionConfig = config.mobBlockDestruction;
    if (destructionConfig.enable && destructionConfig.ravager && event.self().getTypeName() == RavagerType) {
        event.cancel();
    }
}

void onWitherDestroyBlocks(Catalyst::WitherDestroyBlocksBeforeEvent& event) {
    auto const& destructionConfig = config.mobBlockDestruction;
    if (destructionConfig.enable && destructionConfig.wither) {
        event.cancel();
    }
}

} // namespace

bool enable() {
    auto const& destructionConfig = config.mobBlockDestruction;
    if (!destructionConfig.enable || actorDestroyBlockListener || witherDestroyBlocksListener) {
        return true;
    }

    actorDestroyBlockListener =
        ll::event::EventBus::getInstance().emplaceListener<Catalyst::ActorDestroyBlockBeforeEvent>(onActorDestroyBlock);
    if (!actorDestroyBlockListener) {
        return false;
    }

    witherDestroyBlocksListener =
        ll::event::EventBus::getInstance().emplaceListener<Catalyst::WitherDestroyBlocksBeforeEvent>(
            onWitherDestroyBlocks
        );
    if (!witherDestroyBlocksListener) {
        ll::event::EventBus::getInstance().removeListener<Catalyst::ActorDestroyBlockBeforeEvent>(
            actorDestroyBlockListener
        );
        actorDestroyBlockListener.reset();
        return false;
    }

    return true;
}

void disable() {
    auto& eventBus = ll::event::EventBus::getInstance();
    if (witherDestroyBlocksListener) {
        eventBus.removeListener<Catalyst::WitherDestroyBlocksBeforeEvent>(witherDestroyBlocksListener);
        witherDestroyBlocksListener.reset();
    }
    if (actorDestroyBlockListener) {
        eventBus.removeListener<Catalyst::ActorDestroyBlockBeforeEvent>(actorDestroyBlockListener);
        actorDestroyBlockListener.reset();
    }
}

} // namespace my_mod::mob_block_destruction

#else

namespace my_mod::mob_block_destruction {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::mob_block_destruction

#endif
