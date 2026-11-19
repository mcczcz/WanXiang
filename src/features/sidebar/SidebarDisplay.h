#pragma once

#include <string>
#include <utility>
#include <vector>


#include "mc/world/scores/ObjectiveSortOrder.h"

class Player;

namespace my_mod::sidebar {

/**
 * @brief 为玩家设置一个自定义侧边栏
 *
 * @param player 要设置侧边栏的玩家对象
 * @param title 侧边栏的标题
 * @param data 一个包含字符串和整数的键值对向量，每个键值对代表侧边栏的一行（文本和分数）
 * @param sortOrder 分数的排序方式（升序或降序）
 * @return 操作是否成功
 */
bool setSidebar(
    Player*                                         player,
    const std::string&                              title,
    const std::vector<std::pair<std::string, int>>& data,
    ObjectiveSortOrder                              sortOrder = ObjectiveSortOrder::Descending
);

/**
 * @brief 移除玩家的自定义侧边栏
 *
 * @param player 要移除侧边栏的玩家对象
 * @return 操作是否成功
 */
bool removeSidebar(Player* player);

} // namespace my_mod::sidebar
