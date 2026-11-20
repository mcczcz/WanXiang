#include "RedstoneTickControl.h"

#ifdef LL_PLAT_S

#include "core/FeatureDiagnostics.h"
#include "core/Global.h"

#include "ll/api/memory/Hook.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/redstone/circuit/CircuitSystem.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>

namespace my_mod::redstone_tick_control {
namespace {

std::atomic_bool enabled{false};
using UpdateTime = std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>>;
std::unordered_map<int, UpdateTime> nextUpdates;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_INSTANCE_HOOK(
    CircuitSystemEvaluateHook,
    ll::memory::HookPriority::Normal,
    CircuitSystem,
    &CircuitSystem::evaluate,
    void,
    BlockSource* region
) {
    auto const& tickConfig = config.redstoneTickControl;
    if (enabled.load(std::memory_order_relaxed) && tickConfig.enable && region != nullptr) {
        auto const dimensionId = static_cast<int>(region->getDimensionId());
        auto const dimension   = tickConfig.dimensions.find(std::to_string(dimensionId));
        if (dimension != tickConfig.dimensions.end()) {
            auto const rate = dimension->second;
            if (rate == 0.0) {
                return;
            }
            if (rate > 0.0) {
                UpdateTime const now = std::chrono::steady_clock::now();
                auto& nextUpdate = nextUpdates.try_emplace(dimensionId, now).first->second;
                if (now < nextUpdate) {
                    return;
                }

                auto const interval = std::chrono::duration<double>{1.0 / rate};
                nextUpdate += interval;
                // 保持更新节奏；停顿过久时丢弃积压，不集中补跑红石刻。
                if (nextUpdate <= now) {
                    nextUpdate = now + interval;
                }
            }
        }
    }
    origin(region);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.redstoneTickControl.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    for (auto const& [dimension, rate] : config.redstoneTickControl.dimensions) {
        if (!std::isfinite(rate) || (rate < 0.0 && rate != -1.0)) {
            return feature_diagnostics::fail(tr("redstone_tick_control.invalid_rate_detail", dimension, rate));
        }
    }
    nextUpdates.clear();
    if (!feature_diagnostics::checkHook(
            "redstone_tick_control",
            "CircuitSystemEvaluateHook",
            CircuitSystemEvaluateHook::hook()
        )) {
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    CircuitSystemEvaluateHook::unhook();
    nextUpdates.clear();
}

} // namespace my_mod::redstone_tick_control

#else

namespace my_mod::redstone_tick_control {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::redstone_tick_control

#endif
