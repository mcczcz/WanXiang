#include "mod/ClientLevelName.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/network/packet/StartGamePacket.h"

#include <atomic>
#include <string>
#include <utility>

namespace my_mod::client_level_name {
namespace {

std::atomic_bool enabled{false};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    StartGamePacketWriteHook,
    ll::memory::HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    auto const& setting = config.clientLevelName;
    if (!enabled.load(std::memory_order_relaxed) || !setting.enable || setting.name.empty()) {
        return origin(stream);
    }

    std::string originalName = *mLevelName;
    *mLevelName              = setting.name;
    origin(stream);
    *mLevelName = std::move(originalName);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.clientLevelName.enable || config.clientLevelName.name.empty()) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (StartGamePacketWriteHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    StartGamePacketWriteHook::unhook();
}

} // namespace my_mod::client_level_name

#else

namespace my_mod::client_level_name {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::client_level_name

#endif
