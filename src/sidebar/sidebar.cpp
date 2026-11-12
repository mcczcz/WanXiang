#include "sidebar.h"

#ifdef LL_PLAT_S

#include "mc/network/packet/ChangeFakePlayerScore.h"
#include "mc/network/packet/RemoveObjectivePacket.h"
#include "mc/network/packet/RemoveObjectivePacketPayload.h"
#include "mc/network/packet/SetDisplayObjectivePacket.h"
#include "mc/network/packet/SetDisplayObjectivePacketPayload.h"
#include "mc/network/packet/SetScorePacket.h"
#include "mc/network/packet/SetScorePacketPayload.h"
#include "mc/server/ServerPlayer.h"

#include <variant>

namespace my_mod::sidebar {

const std::string FAKE_OBJECTIVE_NAME = "cz-sidebar";

bool setSidebar(
    Player*                                         player,
    const std::string&                              title,
    const std::vector<std::pair<std::string, int>>& data,
    ObjectiveSortOrder                              sortOrder
) {
    if (!player) {
        return false;
    }

    // 移除旧的侧边栏以强制刷新
    removeSidebar(player);

    // 1. 创建并发送数据包，用于在侧边栏显示目标
    SetDisplayObjectivePacket displayPacket;
    displayPacket.mDisplaySlotName      = "sidebar";
    displayPacket.mObjectiveName        = FAKE_OBJECTIVE_NAME;
    displayPacket.mObjectiveDisplayName = title;
    displayPacket.mCriteriaName         = "dummy";
    displayPacket.mSortOrder            = sortOrder;
    displayPacket.mSerializationMode    = SerializationMode::CerealOnly;
    player->sendNetworkPacket(displayPacket);

    // 2. 新版协议使用不同的分数变更类型，侧边栏文本对应虚拟玩家。
    std::vector<std::variant<RemoveScore, ChangePlayerScore, ChangeEntityScore, ChangeFakePlayerScore>> scoreInfos;
    scoreInfos.reserve(data.size());
    for (const auto& line : data) {
        ChangeFakePlayerScore info;
        info.mScoreboardId->mRawID       = line.second; // 使用分数作为ID
        info.mScoreboardId->mIdentityDef = nullptr;
        info.mObjectiveName              = FAKE_OBJECTIVE_NAME;
        info.mScoreValue                 = line.second;
        info.mFakePlayerName             = line.first;
        scoreInfos.emplace_back(info);
    }

    // 3. 创建并发送数据包以设置分数
    if (!scoreInfos.empty()) {
        SetScorePacket setScorePacket(SetScorePacketPayload::change(std::move(scoreInfos)));
        player->sendNetworkPacket(setScorePacket);
    }

    return true;
}

bool removeSidebar(Player* player) {
    if (!player) {
        return false;
    }

    // 移除目标以清除侧边栏
    RemoveObjectivePacket removePacket;
    removePacket.mObjectiveName     = FAKE_OBJECTIVE_NAME;
    removePacket.mSerializationMode = SerializationMode::CerealOnly;
    player->sendNetworkPacket(removePacket);

    return true;
}

} // namespace my_mod::sidebar

#endif
