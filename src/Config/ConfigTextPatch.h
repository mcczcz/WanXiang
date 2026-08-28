#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace my_mod::config_text {

/// 一个待插入配置文件的键。
struct PendingInsert {
    /// 完整路径，最后一段是要插入的键名。例如 {"xpOrbMerge", "intervalTicks"}。
    std::vector<std::string> path;
    /// 已序列化好的 JSON 值文本。
    std::string value;
};

/// 把 `inserts` 里的每个键按文本方式插入 `content`，保留注释、原有顺序与排版。
///
/// 按父对象分组逐组处理，每组都从当前文本重新定位父对象，因此前面插入造成的偏移变化不会让
/// 后面的位置失效。支持任意嵌套深度。
///
/// 任一父对象无法在文本中定位（不存在、或不是一个对象字面量）时返回 nullopt，
/// 调用方应当此时保持原文件不变。
std::optional<std::string> applyInserts(std::string_view content, std::vector<PendingInsert> const& inserts);

/// 先写临时文件再 rename，避免 truncate 与 write 之间崩溃把目标文件截断成空文件。
bool writeFileAtomically(std::filesystem::path const& target, std::string_view content);

} // namespace my_mod::config_text
