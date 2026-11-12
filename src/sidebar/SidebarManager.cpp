#include "SidebarManager.h"

#ifdef LL_PLAT_S

#include "Config/ConfigManager.h"
#include "DatabaseManager.h"
#include "PA/PlaceholderAPI.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mod/Global.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/level/Level.h"
#include "sidebar.h"

#include <algorithm>
#include <unordered_set>

namespace my_mod::sidebar {

namespace {

std::string replaceServerPlaceholders(PA::IPlaceholderService& service, std::string const& text) {
    // Avoid walking the placeholder parser for ordinary static text.
    if (text.find_first_of("{%") == std::string::npos) {
        return text;
    }
    return service.replaceServer(text);
}

std::string
replacePlayerPlaceholders(PA::IPlaceholderService& service, std::string const& text, PA::IContext const* context) {
    if (text.find_first_of("{%") == std::string::npos) {
        return text;
    }
    return service.replace(text, context);
}

} // namespace

SidebarManager::SidebarManager() : mRunning(false), mLastLineGroupSwitchTime(std::chrono::steady_clock::now()) {}

SidebarManager::~SidebarManager() { stop(); }

SidebarManager& SidebarManager::getInstance() {
    static SidebarManager instance;
    return instance;
}

void SidebarManager::setPlayerSidebarStatus(std::string const& uuid, bool enabled) {
    auto& state        = mPlayerStates[uuid];
    state.enabled      = enabled;
    state.statusLoaded = true;
    if (!enabled) {
        // The command sends the remove packet immediately. Do not send it a
        // second time on the next periodic refresh.
        state.rendered = false;
        state.title.clear();
        state.data.clear();
    }
}

void SidebarManager::updateSidebar() {
    auto& manager = *this;
    auto& config  = ConfigManager::getInstance().get().sidebar;
    if (!config.enable) {
        return;
    }

    auto level = ll::service::getLevel();
    if (level) {
        auto& sidebarConfig = config;

        if (sidebarConfig.dynamicLines.size() > 1) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - manager.mLastLineGroupSwitchTime).count()
                >= sidebarConfig.lineGroupSwitchInterval) {
                sidebarConfig.currentLineGroupIndex =
                    (sidebarConfig.currentLineGroupIndex + 1) % sidebarConfig.dynamicLines.size();
                manager.mLastLineGroupSwitchTime = now;
            }
        }

        const auto lineGroupIndex =
            sidebarConfig.dynamicLines.empty()
                ? 0u
                : static_cast<size_t>(sidebarConfig.currentLineGroupIndex) % sidebarConfig.dynamicLines.size();
        const std::vector<std::string>* configuredLines =
            sidebarConfig.dynamicLines.empty() ? nullptr : &sidebarConfig.dynamicLines[lineGroupIndex];

        auto paService = PA::PA_GetPlaceholderService();
        if (!paService) {
            return;
        }

        // Server placeholders are independent of the player. Resolve
        // them once per cycle instead of once per player and line.
        std::string configuredTitle = "Sidebar";
        if (!sidebarConfig.dynamicTitles.empty()) {
            auto titleIndex = lineGroupIndex % sidebarConfig.dynamicTitles.size();
            configuredTitle = replaceServerPlaceholders(*paService, sidebarConfig.dynamicTitles[titleIndex]);
        }

        std::vector<std::string> configuredServerLines;
        if (configuredLines) {
            configuredServerLines.reserve(configuredLines->size());
            for (auto const& line : *configuredLines) {
                configuredServerLines.emplace_back(replaceServerPlaceholders(*paService, line));
            }
        }

        std::unordered_set<std::string> activePlayers;
        level->forEachPlayer([&](Player& player) {
            if (player.isSimulated()) {
                return true;
            }

            auto uuid = player.getUuid().asString();
            activePlayers.insert(uuid);
            auto& state = manager.mPlayerStates[uuid];

            // A UUID can be reused by a later connection. Reset only the
            // rendered snapshot for the new actor session.
            auto sessionId = player.getOrCreateUniqueID().rawID;
            if (state.sessionId != sessionId) {
                state.sessionId = sessionId;
                state.rendered  = false;
                state.title.clear();
                state.data.clear();
            }

            if (!state.statusLoaded) {
                if (!DatabaseManager::getInstance().playerExists(player.getUuid())) {
                    state.enabled = config.defaultSidebarStatus;
                    DatabaseManager::getInstance().setPlayerSidebarStatus(player.getUuid(), state.enabled);
                } else {
                    state.enabled = DatabaseManager::getInstance().getPlayerSidebarStatus(player.getUuid());
                }
                state.statusLoaded = true;
            }

            if (!state.enabled) {
                if (state.rendered) {
                    removeSidebar(&player);
                    state.rendered = false;
                    state.title.clear();
                    state.data.clear();
                }
                return true;
            }

            auto playerCtx = PA::PlayerContext::factory(&player);
            if (!playerCtx) {
                return true;
            }

            std::string processedTitle = replacePlayerPlaceholders(*paService, configuredTitle, playerCtx.get());
            std::vector<std::pair<std::string, int>> sidebarData;
            sidebarData.reserve(configuredServerLines.size());
            int score = static_cast<int>(configuredServerLines.size());
            for (auto const& line : configuredServerLines) {
                sidebarData.emplace_back(replacePlayerPlaceholders(*paService, line, playerCtx.get()), score--);
            }

            if (!state.rendered || state.title != processedTitle || state.data != sidebarData) {
                setSidebar(&player, processedTitle, sidebarData, ObjectiveSortOrder::Descending);
                state.title    = std::move(processedTitle);
                state.data     = std::move(sidebarData);
                state.rendered = true;
            }
            return true;
        });

        // Do not retain per-session rendered strings for players that have
        // disconnected. Their persistent enabled flag remains in SQLite.
        for (auto it = manager.mPlayerStates.begin(); it != manager.mPlayerStates.end();) {
            if (!activePlayers.contains(it->first)) {
                it = manager.mPlayerStates.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void SidebarManager::scheduleUpdate(std::chrono::milliseconds delay) {
    mUpdateTask = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
        [this, generation = mUpdateGeneration] {
            // A callback already queued on the executor may outlive cancellation.
            if (!mRunning || generation != mUpdateGeneration) {
                return;
            }
            updateSidebar();
            if (mRunning && generation == mUpdateGeneration) {
                scheduleUpdate(
                    std::chrono::milliseconds(std::max(50, ConfigManager::getInstance().get().sidebar.updateInterval))
                );
            }
        },
        delay
    );
}

void SidebarManager::start() {
    if (mRunning.exchange(true)) {
        return;
    }
    logger.debug("Starting sidebar update task...");
    mLastLineGroupSwitchTime = std::chrono::steady_clock::now();
    scheduleUpdate(std::chrono::milliseconds::zero());
}

void SidebarManager::stop() {
    mRunning = false;
    ++mUpdateGeneration;
    if (mUpdateTask) {
        mUpdateTask->cancel();
        mUpdateTask.reset();
    }
    if (auto level = ll::service::getLevel()) {
        level->forEachPlayer([this](Player& player) {
            auto const state = mPlayerStates.find(player.getUuid().asString());
            if (state != mPlayerStates.end() && state->second.rendered) {
                removeSidebar(&player);
            }
            return true;
        });
    }
    mPlayerStates.clear();
}

} // namespace my_mod::sidebar

#endif
