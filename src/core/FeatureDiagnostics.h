#pragma once

#include "Global.h"

#include <exception>
#include <string>
#include <utility>

namespace my_mod::feature_diagnostics {

inline thread_local std::string lastError;

inline bool fail(std::string message) {
    lastError = std::move(message);
    logger.error(lastError);
    return false;
}

inline std::string name(char const* feature) { return tr(std::string(feature) + ".name"); }

inline bool checkHook(char const* feature, char const* hook, int result) {
    return result == 0 || fail(tr("feature.hook_failed", name(feature), hook, result));
}

inline bool checkEvent(char const* feature, char const* event, bool registered) {
    return registered || fail(tr("feature.event_failed", name(feature), event));
}

inline bool exception(char const* feature, char const* step, char const* detail) {
    return fail(tr("feature.exception", name(feature), tr(step), detail));
}

inline bool enable(char const* feature, bool (*start)()) {
    lastError.clear();
    try {
        if (start()) {
            return true;
        }
    } catch (std::exception const& error) {
        return exception(feature, "feature.step.initialize", error.what());
    } catch (...) {
        return fail(tr("feature.unknown_exception", name(feature)));
    }
    if (lastError.empty()) {
        return fail(tr("feature.reason_unavailable", name(feature)));
    }
    return false;
}

} // namespace my_mod::feature_diagnostics
