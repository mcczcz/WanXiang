#include "ConfigManager.h"
#include "ConfigSerialization.h"
#include "ConfigTextPatch.h"
#include "I18n/I18n.h"
#include "mod/Entry.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace my_mod {

namespace {

struct MissingEntry {
    std::vector<std::string> path;
    nlohmann::json           value;
};

std::string joinPath(std::vector<std::string> const& path) {
    std::string joined;
    for (std::string const& segment : path) {
        if (!joined.empty()) {
            joined += '.';
        }
        joined += segment;
    }
    return joined;
}

void collectMissingKeys(
    nlohmann::json const&      defaults,
    nlohmann::json const&      user,
    std::vector<std::string>&  prefix,
    std::vector<MissingEntry>& out
) {
    for (auto const& item : defaults.items()) {
        const auto found = user.find(item.key());
        prefix.push_back(item.key());

        if (found == user.end()) {
            // 整棵子树一起补，不再往下递归。
            out.push_back({prefix, item.value()});
        } else if (item.value().is_object() && found->is_object()) {
            collectMissingKeys(item.value(), *found, prefix, out);
        }

        prefix.pop_back();
    }
}

// 把缺失键写回用户配置。任一环节失败都保持原文件不动。
bool patchMissingKeys(std::filesystem::path const& path, std::vector<MissingEntry> const& missing) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::vector<config_text::PendingInsert> inserts;
    inserts.reserve(missing.size());
    for (MissingEntry const& entry : missing) {
        inserts.push_back({entry.path, entry.value.dump()});
    }

    const auto patched = config_text::applyInserts(content, inserts);
    if (!patched.has_value()) {
        return false;
    }
    return config_text::writeFileAtomically(path, *patched);
}

} // namespace

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() : mConfig(std::make_unique<Config>()) {}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::load(const std::string& path) {
    return load(path, true);
}

bool ConfigManager::reload() {
    return load(mConfigPath, false);
}

bool ConfigManager::load(const std::string& path, bool createIfMissing) {
    mConfigPath = path;

    auto& modLogger = Entry::getInstance().getSelf().getLogger();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (!createIfMissing) {
            modLogger.error(tr("config.reload_read_error", path));
            return false;
        }
        // 文件不存在，创建默认配置
        modLogger.info(tr("config.not_found", path));
        return save();
    }

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    nlohmann::json userJson;
    detail::configFieldErrors().clear();
    try {
        // 解析用户配置，允许注释
        userJson = nlohmann::json::parse(content, nullptr, true, true);
    } catch (const nlohmann::json::exception& e) {
        modLogger.error(tr("config.parse_error", e.what()));
        return false;
    }

    if (!userJson.is_object()) {
        modLogger.error(tr("config.not_an_object"));
        return false;
    }

    // 填充内存中的配置对象。缺失或类型不符的键会回退到结构体定义里的默认值。
    Config nextConfig;
    userJson.get_to(nextConfig);

    for (std::string const& fieldError : detail::configFieldErrors()) {
        modLogger.warn(tr("config.field_error", fieldError));
    }
    detail::configFieldErrors().clear();

    const nlohmann::json defaultJson = Config{};

    std::vector<std::string>  prefix;
    std::vector<MissingEntry> missing;
    collectMissingKeys(defaultJson, userJson, prefix, missing);

    if (!missing.empty()) {
        modLogger.warn(tr("config.missing_keys"));
        modLogger.warn(tr("config.missing_keys_hint"));
        for (MissingEntry const& entry : missing) {
            modLogger.warn(tr("config.missing_key_item", joinPath(entry.path)));
        }

        if (patchMissingKeys(path, missing)) {
            modLogger.info(tr("config.updated_missing_keys", missing.size()));
        } else {
            modLogger.warn(tr("config.update_missing_keys_failed"));
            modLogger.warn(tr("config.regenerate_hint"));
        }
    }

    // Keep the object address stable: modules hold a reference to this configuration.
    *mConfig = std::move(nextConfig);
    modLogger.setLevel(mConfig->logLevel);

    return true; // 不再整体重写，以保留用户文件的格式和注释
}

bool ConfigManager::save() {
    auto& modLogger = Entry::getInstance().getSelf().getLogger();

    std::string serialized;
    try {
        serialized = nlohmann::json(*mConfig).dump(4);
    } catch (const nlohmann::json::exception& e) {
        modLogger.error(tr("config.save_error", e.what()));
        return false;
    }

    const std::filesystem::path target(mConfigPath);
    if (target.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
    }

    if (!config_text::writeFileAtomically(target, serialized)) {
        modLogger.error(tr("config.save_io_error", mConfigPath));
        return false;
    }
    return true;
}

Config& ConfigManager::get() { return *mConfig; }

const Config& ConfigManager::get() const { return *mConfig; }

} // namespace my_mod
