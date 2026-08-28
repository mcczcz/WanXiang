#include "ConfigTextPatch.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace my_mod::config_text {

namespace {

constexpr auto Npos = std::string_view::npos;

struct ObjectRange {
    std::size_t openBrace;
    std::size_t closeBrace;
};

struct MemberSpan {
    std::string_view key;
    std::size_t      valueStart;
    std::size_t      valueEnd; // 值最后一个字符之后的位置
};

// ---------------------------------------------------------------------------
// 文本扫描
//
// 配置文件里有用户写的注释和排版，所以补齐缺失键是以「文本插入」而非「重新序列化整个文档」
// 的方式完成的。下面这组辅助函数只需要理解到足以定位插入点的程度。
// ---------------------------------------------------------------------------

std::size_t skipTrivia(std::string_view text, std::size_t pos, std::size_t limit) {
    while (pos < limit) {
        const char ch = text[pos];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == ',') {
            ++pos;
            continue;
        }
        if (ch == '/' && pos + 1 < limit) {
            if (text[pos + 1] == '/') {
                while (pos < limit && text[pos] != '\n') {
                    ++pos;
                }
                continue;
            }
            if (text[pos + 1] == '*') {
                pos += 2;
                while (pos + 1 < limit && !(text[pos] == '*' && text[pos + 1] == '/')) {
                    ++pos;
                }
                pos = std::min(pos + 2, limit);
                continue;
            }
        }
        break;
    }
    return pos;
}

// `start` 必须指向起始引号，返回收尾引号之后的位置。
std::size_t scanString(std::string_view text, std::size_t start, std::size_t limit) {
    std::size_t pos = start + 1;
    while (pos < limit) {
        const char ch = text[pos];
        if (ch == '\\') {
            pos += 2;
            continue;
        }
        if (ch == '"') {
            return pos + 1;
        }
        ++pos;
    }
    return Npos;
}

// `start` 必须指向 '{' 或 '['，返回配对的收尾括号之后的位置。
std::size_t scanBracketed(std::string_view text, std::size_t start, std::size_t limit) {
    const char  opener = text[start];
    const char  closer = opener == '{' ? '}' : ']';
    int         depth  = 0;
    std::size_t pos    = start;

    while (pos < limit) {
        const char ch = text[pos];
        if (ch == '"') {
            pos = scanString(text, pos, limit);
            if (pos == Npos) {
                return Npos;
            }
            continue;
        }
        if (ch == '/' && pos + 1 < limit && (text[pos + 1] == '/' || text[pos + 1] == '*')) {
            const std::size_t next = skipTrivia(text, pos, limit);
            if (next == pos) {
                ++pos;
            } else {
                pos = next;
            }
            continue;
        }
        if (ch == opener) {
            ++depth;
        } else if (ch == closer) {
            --depth;
            if (depth == 0) {
                return pos + 1;
            }
        }
        ++pos;
    }
    return Npos;
}

std::size_t scanValue(std::string_view text, std::size_t start, std::size_t limit) {
    if (start >= limit) {
        return Npos;
    }

    const char ch = text[start];
    if (ch == '"') {
        return scanString(text, start, limit);
    }
    if (ch == '{' || ch == '[') {
        return scanBracketed(text, start, limit);
    }

    // 标量：一直读到分隔符，再回退掉尾部空白。
    std::size_t pos = start;
    while (pos < limit && text[pos] != ',' && text[pos] != '}' && text[pos] != ']' && text[pos] != '\n'
           && text[pos] != '/') {
        ++pos;
    }
    while (pos > start && (text[pos - 1] == ' ' || text[pos - 1] == '\t' || text[pos - 1] == '\r')) {
        --pos;
    }
    return pos;
}

std::optional<ObjectRange> findRootObject(std::string_view text) {
    const std::size_t open = skipTrivia(text, 0, text.size());
    if (open >= text.size() || text[open] != '{') {
        return std::nullopt;
    }
    const std::size_t end = scanBracketed(text, open, text.size());
    if (end == Npos) {
        return std::nullopt;
    }
    return ObjectRange{open, end - 1};
}

// 按源码顺序列出 `object` 的直接成员。
std::vector<MemberSpan> parseMembers(std::string_view text, ObjectRange const& object) {
    std::vector<MemberSpan> members;
    std::size_t             pos = object.openBrace + 1;

    while (true) {
        pos = skipTrivia(text, pos, object.closeBrace);
        if (pos >= object.closeBrace || text[pos] != '"') {
            break;
        }

        const std::size_t keyEnd = scanString(text, pos, object.closeBrace);
        if (keyEnd == Npos) {
            break;
        }
        const std::string_view key = text.substr(pos + 1, keyEnd - pos - 2);

        pos = skipTrivia(text, keyEnd, object.closeBrace);
        if (pos >= object.closeBrace || text[pos] != ':') {
            break;
        }

        const std::size_t valueStart = skipTrivia(text, pos + 1, object.closeBrace);
        const std::size_t valueEnd   = scanValue(text, valueStart, object.closeBrace);
        if (valueEnd == Npos || valueEnd <= valueStart) {
            break;
        }

        members.push_back({key, valueStart, valueEnd});
        pos = valueEnd;
    }
    return members;
}

// 从根对象出发沿 `path` 逐级下钻，返回最后一段对应的对象范围。空 path 即根对象。
std::optional<ObjectRange> findObjectAtPath(std::string_view text, std::vector<std::string> const& path) {
    auto current = findRootObject(text);
    if (!current.has_value()) {
        return std::nullopt;
    }

    for (std::string const& segment : path) {
        const auto members = parseMembers(text, *current);
        const auto found =
            std::find_if(members.begin(), members.end(), [&](MemberSpan const& m) { return m.key == segment; });
        if (found == members.end() || text[found->valueStart] != '{') {
            return std::nullopt;
        }
        current = ObjectRange{found->valueStart, found->valueEnd - 1};
    }
    return current;
}

std::string detectLineEnding(std::string_view text) { return text.find("\r\n") != Npos ? "\r\n" : "\n"; }

std::string detectIndent(std::string_view text, ObjectRange const& object) {
    std::size_t lineStart = object.openBrace + 1;
    while (lineStart < object.closeBrace) {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == Npos || lineEnd > object.closeBrace) {
            lineEnd = object.closeBrace;
        }

        const auto  line = text.substr(lineStart, lineEnd - lineStart);
        std::size_t i    = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        // i == 0 说明这个成员跟 '{' 挤在同一行，对缩进风格没有参考价值，继续往下找。
        if (i > 0 && i < line.size() && line[i] == '"') {
            return std::string(line.substr(0, i));
        }

        if (lineEnd == object.closeBrace) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return "    ";
}

} // namespace

std::optional<std::string> applyInserts(std::string_view content, std::vector<PendingInsert> const& inserts) {
    std::string result(content);
    if (inserts.empty()) {
        return result;
    }

    // 按父路径分组，保持首次出现的顺序。
    std::vector<std::pair<std::vector<std::string>, std::vector<PendingInsert const*>>> groups;
    for (PendingInsert const& insert : inserts) {
        if (insert.path.empty()) {
            return std::nullopt;
        }
        std::vector<std::string> parent(insert.path.begin(), insert.path.end() - 1);
        const auto               found =
            std::find_if(groups.begin(), groups.end(), [&](auto const& group) { return group.first == parent; });
        if (found == groups.end()) {
            groups.emplace_back(std::move(parent), std::vector<PendingInsert const*>{&insert});
        } else {
            found->second.push_back(&insert);
        }
    }

    const std::string lineEnding = detectLineEnding(result);

    for (auto const& [parentPath, entries] : groups) {
        const auto parent = findObjectAtPath(result, parentPath);
        if (!parent.has_value()) {
            return std::nullopt;
        }

        const auto        members = parseMembers(result, *parent);
        const std::string indent  = detectIndent(result, *parent);

        // 插在最后一个成员之后，而不是收尾括号之前，这样分隔逗号会落在上一行末尾，
        // 而不是自己单独占一行。
        const std::size_t insertPos = members.empty() ? parent->openBrace + 1 : members.back().valueEnd;

        std::string patch;
        if (!members.empty()) {
            patch += ',';
        }
        patch += lineEnding;

        for (std::size_t i = 0; i < entries.size(); ++i) {
            patch += indent;
            patch += '"';
            patch += entries[i]->path.back();
            patch += "\": ";
            patch += entries[i]->value;
            if (i + 1 < entries.size()) {
                patch += ',';
                patch += lineEnding;
            }
        }
        if (members.empty()) {
            // 对象原本是空的，补一个换行让收尾括号回到自己的行上。
            patch += lineEnding;
        }

        result.insert(insertPos, patch);
    }

    return result;
}

bool writeFileAtomically(std::filesystem::path const& target, std::string_view content) {
    std::filesystem::path temp = target;
    temp += ".tmp";

    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            out.close();
            std::error_code discard;
            std::filesystem::remove(temp, discard);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        std::error_code discard;
        std::filesystem::remove(temp, discard);
        return false;
    }
    return true;
}

} // namespace my_mod::config_text
