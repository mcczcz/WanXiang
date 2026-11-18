#pragma once

#include "ll/api/mod/NativeMod.h"

#include <string>

namespace my_mod {

class Entry {

public:
    static Entry& getInstance();

    Entry() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool reload();

    [[nodiscard]] std::string const& getLastReloadError() const { return mLastReloadError; }

    bool disable();

    bool unload();

private:
    ll::mod::NativeMod& mSelf;
    std::string         mLastReloadError;
};

} // namespace my_mod
