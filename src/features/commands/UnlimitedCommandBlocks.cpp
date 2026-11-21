#include "features/commands/UnlimitedCommandBlocks.h"

#ifdef LL_PLAT_S

#include "core/Global.h"

#include "core/memory/FunctionSignature.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/memory/Signature.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <span>

// Clang 23 requires these SDK/EnTT headers to precede the diagnostics header here.
#include "core/FeatureDiagnostics.h"

namespace my_mod::unlimited_command_blocks {
namespace {

using namespace ll::literals;

// IDA analysis and instruction evidence: docs/clone-fill-limit-analysis.md.
// NOP + JMP rel32 has the same length/target as Jcc rel32: the displacement stays intact.
constexpr std::array<std::byte, 2> unlimitedJump{std::byte{0x90}, std::byte{0xE9}};

struct JumpPatch {
    std::byte*               address{};
    std::array<std::byte, 6> original{};
    std::array<std::byte, 6> patched{};
    bool                     installed{};

    bool write(std::span<std::byte const, 2> bytes) const {
        bool flushed{};
        ll::memory::modify(address, bytes.size(), [&] {
            std::memcpy(address, bytes.data(), bytes.size());
            // Flush before modify() resumes other threads.
            flushed = FlushInstructionCache(GetCurrentProcess(), address, bytes.size()) != 0;
        });
        return flushed && std::memcmp(address, bytes.data(), bytes.size()) == 0;
    }

    bool install() {
        if (!address || std::memcmp(address, original.data(), original.size()) != 0) {
            return false;
        }
        // Record ownership before writing so a failure can be rolled back.
        installed = true;
        return write(unlimitedJump);
    }

    bool restore() {
        if (!installed) {
            return true;
        }
        if (std::memcmp(address, original.data(), original.size()) == 0) {
            installed = false;
            return true;
        }
        // Another plugin may have changed this instruction. Never overwrite its bytes.
        if (std::memcmp(address, patched.data(), patched.size()) != 0 || !write(std::span{original}.first<2>())) {
            return false;
        }
        installed = false;
        return true;
    }
};

JumpPatch fillPatch;
JumpPatch clonePatch;

bool prepare(JumpPatch& patch, void* function, ll::memory::SignatureView signature, size_t branchOffset) {
    if (!function || patch.installed) {
        return false;
    }
    DWORD64 imageBase{};
    auto*   entry = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(function), &imageBase, nullptr);
    if (!entry || imageBase + entry->BeginAddress != reinterpret_cast<DWORD64>(function)
        || entry->EndAddress <= entry->BeginAddress) {
        return false;
    }
    // Limit matching to the resolved function's PE unwind range, never the whole executable.
    auto code  = std::span{static_cast<std::byte*>(function), size_t(entry->EndAddress - entry->BeginAddress)};
    auto match = [&](auto first) {
        return std::search(first, code.end(), signature.begin(), signature.end(), [](auto byte, auto element) {
            return element == byte;
        });
    };
    auto found = match(code.begin());
    if (found == code.end() || match(found + 1) != code.end()) {
        return false;
    }
    auto*        branch = std::to_address(found) + branchOffset;
    std::int32_t displacement{};
    std::memcpy(&displacement, branch + 2, sizeof(displacement));
    auto const targetOffset = branch - code.data() + 6 + std::int64_t(displacement);
    if (displacement <= 0 || targetOffset >= static_cast<std::int64_t>(code.size())) {
        return false;
    }
    patch.address = branch;
    std::memcpy(patch.original.data(), branch, patch.original.size());
    patch.patched = patch.original;
    std::copy(unlimitedJump.begin(), unlimitedJump.end(), patch.patched.begin());
    return true;
}

} // namespace

bool enable() {
    if (!config.unlimitedCommandBlocksEnable) {
        // A previous failed restoration must not be reported as a successful disable on reload.
        return !fillPatch.installed && !clonePatch.installed;
    }
    if (fillPatch.installed || clonePatch.installed) {
        return fillPatch.installed && clonePatch.installed
            && std::memcmp(fillPatch.address, fillPatch.patched.data(), fillPatch.patched.size()) == 0
            && std::memcmp(clonePatch.address, clonePatch.patched.data(), clonePatch.patched.size()) == 0;
    }
    // fill: cmp esi,32769; jl allowed. clone: cmp [volumeLimit],eax; jge allowed.
    auto* fillFunction = function_signature::resolveUnique(
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 58 03 00 00 48 8D AC 24 ?? ?? ?? ?? 44 0F 29 85 ?? ?? ?? ?? 0F 29 BD ?? ?? ?? ?? 0F 29 B5 ?? ?? ?? ?? 48 C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 4C 89 85 ?? ?? ?? ?? 49 89 D7"_sig
    );
    auto* cloneFunction = function_signature::resolveUnique(
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC D8 02 00 00 48 8D AC 24 ?? ?? ?? ?? 44 0F 29 BD ?? ?? ?? ?? 44 0F 29 B5 ?? ?? ?? ?? 44 0F 29 AD ?? ?? ?? ?? 44 0F 29 A5 ?? ?? ?? ?? 44 0F 29 9D ?? ?? ?? ?? 44 0F 29 95 ?? ?? ?? ?? 44 0F 29 8D ?? ?? ?? ?? 44 0F 29 85 ?? ?? ?? ?? 0F 29 BD ?? ?? ?? ?? 0F 29 B5 ?? ?? ?? ?? 48 C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 48 89 8D"_sig
    );
    if (!prepare(fillPatch, fillFunction, "81 FE 01 80 00 00 0F 8C ?? ?? ?? ?? 48 8D 8D 50 01 00 00"_sig, 6)
        || !prepare(
            clonePatch,
            cloneFunction,
            "F3 41 0F 2C C4 39 05 ?? ?? ?? ?? 0F 8D ?? ?? ?? ?? 48 8B 8D B8 00 00 00 C7 01 01 00 00 00 C6 41 04 01"_sig,
            11
        )) {
        return feature_diagnostics::fail(tr("unlimited_command_blocks.unsupported"));
    }
    try {
        if (fillPatch.install() && clonePatch.install()) {
            return true;
        }
    } catch (std::exception const& exception) {
        feature_diagnostics::fail(tr("unlimited_command_blocks.patch_error", exception.what()));
    }
    disable();
    if (feature_diagnostics::lastError.empty()) {
        return feature_diagnostics::fail(tr("unlimited_command_blocks.enable_failed"));
    }
    return false;
}

void disable() {
    bool const cloneRestored = clonePatch.restore();
    bool const fillRestored  = fillPatch.restore();
    if (!cloneRestored || !fillRestored) {
        logger.error(tr("unlimited_command_blocks.restore_failed"));
    }
}

} // namespace my_mod::unlimited_command_blocks

#else

namespace my_mod::unlimited_command_blocks {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::unlimited_command_blocks

#endif
