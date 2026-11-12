#include "DatabaseManager.h"

#ifdef LL_PLAT_S
#include "mod/Global.h"
#include "sqlite3.h"

namespace my_mod::sidebar {

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager() {
    closeDatabase();
}

bool DatabaseManager::openDatabase(const std::string& dbPath) {
    dbFilePath = dbPath;
    int rc = sqlite3_open(dbFilePath.c_str(), &db);
    if (rc) {
        logger.error("无法打开侧边栏数据库: {}", sqlite3_errmsg(db));
        closeDatabase();
        return false;
    }
    logger.info("成功打开数据库: {}", dbFilePath);
    return true;
}

void DatabaseManager::closeDatabase() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        logger.info("数据库已关闭: {}", dbFilePath);
    }
}

bool DatabaseManager::createTable() {
    char* errMsg = nullptr;
    const char* sql =
        "CREATE TABLE IF NOT EXISTS player_sidebar_status ("
        "uuid TEXT PRIMARY KEY NOT NULL,"
        "enabled INTEGER NOT NULL DEFAULT 0"
        ");";

    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        logger.error("创建表失败: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    logger.debug("表 'player_sidebar_status' 已创建或已存在。");
    return true;
}

bool DatabaseManager::setPlayerSidebarStatus(const mce::UUID& playerUuid, bool enabled) {
    char* errMsg = nullptr;
    std::string uuidStr = playerUuid.asString();
    std::string sql =
        "INSERT OR REPLACE INTO player_sidebar_status (uuid, enabled) VALUES ('" +
        uuidStr + "', " + std::to_string(enabled ? 1 : 0) + ");";

    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        logger.error("设置玩家 {} 侧边栏状态失败: {}", uuidStr, errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    logger.debug("玩家 {} 侧边栏状态已设置为: {}", uuidStr, (enabled ? "开启" : "关闭"));
    return true;
}

bool DatabaseManager::getPlayerSidebarStatus(const mce::UUID& playerUuid) {
    sqlite3_stmt* stmt;
    std::string uuidStr = playerUuid.asString();
    std::string sql = "SELECT enabled FROM player_sidebar_status WHERE uuid = ?;";
    bool status = false;

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.error("查询玩家 {} 侧边栏状态失败 (prepare): {}", uuidStr, sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, uuidStr.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        status = (sqlite3_column_int(stmt, 0) == 1);
        logger.debug("玩家 {} 侧边栏状态为: {}", uuidStr, (status ? "开启" : "关闭"));
    } else if (rc == SQLITE_DONE) {
        logger.debug("玩家 {} 的侧边栏状态未找到，默认为关闭。", uuidStr);
        status = false; // Default to disabled if not found
    } else {
        logger.error("查询玩家 {} 侧边栏状态失败 (step): {}", uuidStr, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return status;
}

bool DatabaseManager::playerExists(const mce::UUID& playerUuid) {
    sqlite3_stmt* stmt;
    std::string uuidStr = playerUuid.asString();
    std::string sql = "SELECT 1 FROM player_sidebar_status WHERE uuid = ?;";
    bool exists = false;

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.error("检查玩家 {} 是否存在失败 (prepare): {}", uuidStr, sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, uuidStr.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        exists = true;
        logger.debug("玩家 {} 存在于数据库中。", uuidStr);
    } else if (rc == SQLITE_DONE) {
        logger.debug("玩家 {} 不存在于数据库中。", uuidStr);
        exists = false;
    } else {
        logger.error("检查玩家 {} 是否存在失败 (step): {}", uuidStr, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return exists;
}

} // namespace my_mod::sidebar

#endif
