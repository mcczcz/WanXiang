#include "mod/CoResourcePack.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/network/packet/PackInfoData.h"
#include "mc/network/packet/ResourcePacksInfoPacket.h"
#include "mc/network/packet/StartGamePacket.h"

#include <atomic>
#include <cstddef>
#include <vector>

namespace my_mod::co_resource_pack {
namespace {

std::atomic_bool enabled{false};
std::atomic_bool packsInfoHooked{false};
std::atomic_bool startGameHooked{false};

// 两个封包各自携带一个「材质包是否必需」的标志，原版里它们同源，于是
// 「强制客户端下载服务器材质包」和「禁用玩家自己的材质包」被绑成了一件事。
// 共存要做的就是把这两个标志拆开：ResourcePacksInfoPacket 说必需，
// StartGamePacket 里的 LevelSettings 说不必需。
//
// 三处改动都是「改成员 -> 调 origin -> 改回来」。还原不是为了封包本身，
// 而是为了 disable() 之后不留下痕迹：BDS 是否复用同一个 packet 对象随版本而变，
// 还原后两种情况下行为一致。

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    ResourcePacksInfoWriteHook,
    ll::memory::HookPriority::Normal,
    ResourcePacksInfoPacket,
    &ResourcePacksInfoPacket::$write,
    void,
    BinaryStream& stream
) {
    auto const& setting = config.coResourcePack;
    if (!enabled.load(std::memory_order_relaxed) || !setting.enable) {
        return origin(stream);
    }

    auto& data  = *mData;
    auto& packs = *data.mResourcePacks;

    bool const flippedRequired = setting.forceRequired && !data.mResourcePackRequired;
    if (flippedRequired) {
        data.mResourcePackRequired = true;
    }

    // 只记录被翻转过的下标：包里本来就是 addon 的那些必须保持原值，
    // 一律置回 false 会把真正的 addon 包标错。
    std::vector<std::size_t> flippedAddon;
    if (setting.markAsAddonPack) {
        for (std::size_t i = 0; i < packs.size(); ++i) {
            if (!packs[i].mIsAddonPack) {
                packs[i].mIsAddonPack = true;
                flippedAddon.push_back(i);
            }
        }
    }

    origin(stream);

    for (auto const index : flippedAddon) {
        packs[index].mIsAddonPack = false;
    }
    if (flippedRequired) {
        data.mResourcePackRequired = false;
    }
}

LL_TYPE_INSTANCE_HOOK(
    StartGameTexturePacksHook,
    ll::memory::HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    auto const& setting = config.coResourcePack;
    if (!enabled.load(std::memory_order_relaxed) || !setting.enable || !setting.allowClientPacks) {
        return origin(stream);
    }

    bool const original              = mSettings->mTexturePacksRequired;
    mSettings->mTexturePacksRequired = false;
    origin(stream);
    mSettings->mTexturePacksRequired = original;
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    auto const& setting = config.coResourcePack;
    if (!setting.enable) {
        return true;
    }
    if (enabled.load(std::memory_order_relaxed)) {
        return true;
    }

    bool const needPacksInfo = setting.forceRequired || setting.markAsAddonPack;
    bool const needStartGame = setting.allowClientPacks;
    if (!needPacksInfo && !needStartGame) {
        // 三个开关全关，等同于功能没启用，不必占用 Hook。
        return true;
    }

    if (needPacksInfo) {
        if (ResourcePacksInfoWriteHook::hook() != 0) {
            return false;
        }
        packsInfoHooked.store(true, std::memory_order_relaxed);
    }
    if (needStartGame) {
        if (StartGameTexturePacksHook::hook() != 0) {
            if (packsInfoHooked.exchange(false, std::memory_order_relaxed)) {
                ResourcePacksInfoWriteHook::unhook();
            }
            return false;
        }
        startGameHooked.store(true, std::memory_order_relaxed);
    }

    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    if (startGameHooked.exchange(false, std::memory_order_relaxed)) {
        StartGameTexturePacksHook::unhook();
    }
    if (packsInfoHooked.exchange(false, std::memory_order_relaxed)) {
        ResourcePacksInfoWriteHook::unhook();
    }
}

} // namespace my_mod::co_resource_pack

#else

namespace my_mod::co_resource_pack {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::co_resource_pack

#endif
