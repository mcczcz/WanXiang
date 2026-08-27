#include "mod/ResourcePackOverlay.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/network/packet/PackInfoData.h"
#include "mc/network/packet/ResourcePacksInfoPacket.h"

#include <atomic>
#include <cstddef>
#include <vector>

namespace my_mod::resource_pack_overlay {
namespace {

std::atomic_bool enabled{false};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    ResourcePacksInfoPacketWriteHook,
    ll::memory::HookPriority::Normal,
    ResourcePacksInfoPacket,
    &ResourcePacksInfoPacket::$write,
    void,
    BinaryStream& stream
) {
    if (!enabled.load(std::memory_order_relaxed) || (!config.forceResourcePackRequired && !config.markPacksAsAddon)) {
        return origin(stream);
    }

    const bool originalRequired      = mData->mResourcePackRequired;
    const bool originalHasAddonPacks = mData->mHasAddonPacks;
    auto&      packs                 = *mData->mResourcePacks;

    std::vector<bool> originalAddonFlags;
    if (config.markPacksAsAddon) {
        originalAddonFlags.reserve(packs.size());
        for (auto& pack : packs) {
            originalAddonFlags.push_back(pack.mIsAddonPack);
            pack.mIsAddonPack = true;
        }
    }

    if (config.markPacksAsAddon) {
        mData->mHasAddonPacks = true;
    }
    if (config.forceResourcePackRequired) {
        mData->mResourcePackRequired = true;
    }

    origin(stream);

    mData->mResourcePackRequired = originalRequired;
    mData->mHasAddonPacks        = originalHasAddonPacks;
    if (config.markPacksAsAddon) {
        for (std::size_t i = 0; i < packs.size(); ++i) {
            packs[i].mIsAddonPack = originalAddonFlags[i];
        }
    }
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.forceResourcePackRequired && !config.markPacksAsAddon) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (ResourcePacksInfoPacketWriteHook::hook() != 0) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    ResourcePacksInfoPacketWriteHook::unhook();
}

} // namespace my_mod::resource_pack_overlay

#else

namespace my_mod::resource_pack_overlay {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::resource_pack_overlay

#endif
