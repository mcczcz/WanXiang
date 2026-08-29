#include "mod/UnloadedEntitiesCleaner.h"

#ifdef LL_PLAT_S

#include "mod/Entry.h"
#include "mod/Global.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/server/ServerStartedEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/storage/DBStorage.h"
#include "mc/world/level/storage/db_helpers/Category.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace my_mod::unloaded_entities_cleaner {
namespace {

constexpr std::string_view ActorPrefix       = "actorprefix";
constexpr std::string_view MissingIdentifier = "无identifier";
constexpr std::string_view InvalidKey        = "键长度异常";

struct CleanupStats {
    int                        deletedCount = 0;
    int                        skippedCount = 0;
    std::map<std::string, int> cleanedEntityTypeCounts;
    std::map<std::string, int> skippedEntityTypeCounts;
};

ll::event::ListenerPtr listener;
std::atomic_bool       hasRun{false};

std::string toHexString(std::string_view value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned char byte : value) {
        output << std::setw(2) << static_cast<int>(byte);
    }
    return output.str();
}

std::string currentTimeString() {
    auto const now   = std::chrono::system_clock::now();
    auto const nowTt = std::chrono::system_clock::to_time_t(now);
    std::tm    localTime{};
    localtime_s(&localTime, &nowTt);

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

std::string buildCleanupResult(CleanupStats const& stats, UnloadedEntitiesCleanerConfig const& settings) {
    std::ostringstream output;
    output << tr("unloaded_entities_cleaner.completed", stats.deletedCount, stats.skippedCount);

    if (settings.logCleanedEntityTypeCounts) {
        output << '\n' << tr("unloaded_entities_cleaner.cleaned_stats");
        if (stats.cleanedEntityTypeCounts.empty()) {
            output << '\n' << tr("unloaded_entities_cleaner.none");
        } else {
            for (auto const& [entityType, count] : stats.cleanedEntityTypeCounts) {
                output << "\n- " << entityType << ": " << count;
            }
        }
    }

    if (settings.logSkippedEntityTypeCounts) {
        output << '\n' << tr("unloaded_entities_cleaner.skipped_stats");
        if (stats.skippedEntityTypeCounts.empty()) {
            output << '\n' << tr("unloaded_entities_cleaner.none");
        } else {
            for (auto const& [entityType, count] : stats.skippedEntityTypeCounts) {
                output << "\n- " << entityType << ": " << count;
            }
        }
    }

    return output.str();
}

void logCleanupResult(CleanupStats const& stats, UnloadedEntitiesCleanerConfig const& settings) {
    logger.info(tr("unloaded_entities_cleaner.completed", stats.deletedCount, stats.skippedCount));

    if (settings.logCleanedEntityTypeCounts) {
        logger.info(tr("unloaded_entities_cleaner.cleaned_stats"));
        if (stats.cleanedEntityTypeCounts.empty()) {
            logger.info(tr("unloaded_entities_cleaner.none"));
        } else {
            for (auto const& [entityType, count] : stats.cleanedEntityTypeCounts) {
                logger.info("- {}: {}", entityType, count);
            }
        }
    }

    if (!settings.logSkippedEntityTypeCounts) {
        return;
    }

    logger.info(tr("unloaded_entities_cleaner.skipped_stats"));
    if (stats.skippedEntityTypeCounts.empty()) {
        logger.info(tr("unloaded_entities_cleaner.none"));
        return;
    }

    for (auto const& [entityType, count] : stats.skippedEntityTypeCounts) {
        logger.info("- {}: {}", entityType, count);
    }
}

void writeCleanupResultToTextLog(
    std::string const&        fileName,
    std::string const&        cleanupResult,
    ll::mod::NativeMod const& mod
) {
    if (fileName.empty()) {
        logger.warn(tr("unloaded_entities_cleaner.text_log_empty"));
        return;
    }

    std::filesystem::path logPath(fileName);
    if (logPath.is_relative()) {
        logPath = mod.getConfigDir() / logPath;
    }

    auto const parentPath = logPath.parent_path();
    if (!parentPath.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parentPath, error);
        if (error) {
            logger.warn(
                tr("unloaded_entities_cleaner.text_log_directory_failed", parentPath.string(), error.message())
            );
            return;
        }
    }

    std::ofstream file(logPath, std::ios::app);
    if (!file.is_open()) {
        logger.warn(tr("unloaded_entities_cleaner.text_log_open_failed", logPath.string()));
        return;
    }

    file << "[" << currentTimeString() << "]\n" << cleanupResult << "\n\n";
    logger.info(tr("unloaded_entities_cleaner.text_log_written", logPath.string()));
}

void processActorRecord(
    DBStorage&                      db,
    CleanupStats&                   stats,
    std::vector<std::string> const& entityTypesToClean,
    bool                            filterEnabled,
    std::string_view                key
) {
    std::string fullKey(ActorPrefix);
    fullKey.append(key.data(), key.size());

    if (key.size() == sizeof(std::int64_t)) {
        auto nbt = db.getCompoundTag(fullKey, DBHelpers::Category::Actor);
        if (nbt && nbt->contains("identifier", Tag::Type::String)) {
            std::string const identifier   = static_cast<std::string>((*nbt)["identifier"]);
            bool const        shouldDelete = !filterEnabled
                                          || std::find(entityTypesToClean.begin(), entityTypesToClean.end(), identifier)
                                                 != entityTypesToClean.end();

            if (shouldDelete) {
                db.deleteData(fullKey, DBHelpers::Category::Actor);
                ++stats.deletedCount;
                ++stats.cleanedEntityTypeCounts[identifier];
            } else {
                ++stats.skippedCount;
                ++stats.skippedEntityTypeCounts[identifier];
            }
        } else {
            logger.warn(tr("unloaded_entities_cleaner.missing_identifier", toHexString(fullKey)));
            if (!filterEnabled) {
                db.deleteData(fullKey, DBHelpers::Category::Actor);
                ++stats.deletedCount;
                ++stats.cleanedEntityTypeCounts[std::string(MissingIdentifier)];
            } else {
                ++stats.skippedCount;
                ++stats.skippedEntityTypeCounts[std::string(MissingIdentifier)];
            }
        }
        return;
    }

    logger.warn(tr("unloaded_entities_cleaner.invalid_key", key.size(), toHexString(key)));
    if (!filterEnabled) {
        db.deleteData(fullKey, DBHelpers::Category::Actor);
        ++stats.deletedCount;
        ++stats.cleanedEntityTypeCounts[std::string(InvalidKey)];
    } else {
        ++stats.skippedCount;
        ++stats.skippedEntityTypeCounts[std::string(InvalidKey)];
    }
}

void cleanUnloadedEntities() {
    auto dbStorage = ll::service::getDBStorage();
    if (!dbStorage) {
        logger.warn(tr("unloaded_entities_cleaner.storage_unavailable"));
        return;
    }

    auto const&  settings           = config.unloadedEntitiesCleaner;
    auto const&  entityTypesToClean = settings.entityTypeNamesToClean;
    bool const   filterEnabled      = !entityTypesToClean.empty();
    CleanupStats stats;

    logger.info(tr("unloaded_entities_cleaner.started"));
    if (filterEnabled) {
        logger.info(tr("unloaded_entities_cleaner.filter_enabled"));
        for (auto const& typeName : entityTypesToClean) {
            logger.info("- {}", typeName);
        }
    } else {
        logger.info(tr("unloaded_entities_cleaner.filter_disabled"));
    }

    auto& db = *dbStorage;
    db.forEachKeyWithPrefix(
        ActorPrefix,
        DBHelpers::Category::Actor,
        [&db, &stats, &entityTypesToClean, filterEnabled](std::string_view key, std::string_view) {
            processActorRecord(db, stats, entityTypesToClean, filterEnabled, key);
        }
    );

    logCleanupResult(stats, settings);
    if (settings.writeCleanupResultToTextLog) {
        writeCleanupResultToTextLog(
            settings.cleanupResultTextLogFile,
            buildCleanupResult(stats, settings),
            Entry::getInstance().getSelf()
        );
    }
}

void onServerStarted(ll::event::ServerStartedEvent&) {
    if (hasRun.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    try {
        cleanUnloadedEntities();
    } catch (std::exception const& exception) {
        logger.error(tr("unloaded_entities_cleaner.failed", exception.what()));
    }
}

} // namespace

bool enable() {
    if (!config.unloadedEntitiesCleaner.enable || listener) {
        return true;
    }

    hasRun.store(false, std::memory_order_release);
    listener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerStartedEvent>(onServerStarted);
    return listener != nullptr;
}

void disable() {
    if (listener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerStartedEvent>(listener);
        listener.reset();
    }
    hasRun.store(false, std::memory_order_release);
}

} // namespace my_mod::unloaded_entities_cleaner

#else

namespace my_mod::unloaded_entities_cleaner {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::unloaded_entities_cleaner

#endif
