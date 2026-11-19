#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ll::data {
class CancellableCallback;
}

namespace my_mod::sidebar {

class SidebarManager {
public:
    SidebarManager(const SidebarManager&)            = delete;
    SidebarManager& operator=(const SidebarManager&) = delete;

    static SidebarManager& getInstance();

    void start();
    void stop();

    // Commands run on the server thread, so keep the runtime state in sync
    // with the value persisted by DatabaseManager.
    void setPlayerSidebarStatus(std::string const& uuid, bool enabled);

private:
    void updateSidebar();
    void scheduleUpdate(std::chrono::milliseconds delay);

    struct PlayerState {
        bool                                     statusLoaded = false;
        bool                                     enabled      = false;
        bool                                     rendered     = false;
        std::int64_t                             sessionId    = 0;
        std::string                              title;
        std::vector<std::pair<std::string, int>> data;
    };

    SidebarManager();
    ~SidebarManager();

    std::atomic<bool>                              mRunning;
    std::uint64_t                                  mUpdateGeneration = 0;
    std::shared_ptr<ll::data::CancellableCallback> mUpdateTask;
    std::chrono::steady_clock::time_point          mLastLineGroupSwitchTime;
    std::unordered_map<std::string, PlayerState>   mPlayerStates;
};

} // namespace my_mod::sidebar
