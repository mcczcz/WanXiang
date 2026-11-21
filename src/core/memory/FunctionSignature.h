#pragma once

#include "ll/api/memory/Signature.h"
#include "ll/api/utils/SystemUtils.h"

#include <Windows.h>

#include <algorithm>
#include <span>

namespace my_mod::function_signature {

// Temporary fallback for unavailable MCNAPI exports. Accept exactly one match in
// executable sections of the server image, and require a PE unwind function start.
inline void* resolveUnique(ll::memory::SignatureView signature) {
    auto const image = ll::sys_utils::getImageRange();
    if (image.size() < sizeof(IMAGE_NT_HEADERS64) || signature.size() == 0) {
        return nullptr;
    }
    auto const* dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(image.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0
        || size_t(dos->e_lfanew) > image.size() - sizeof(IMAGE_NT_HEADERS64)) {
        return nullptr;
    }
    auto const* nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(image.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return nullptr;
    }
    auto const* sections      = IMAGE_FIRST_SECTION(nt);
    auto const  sectionOffset = reinterpret_cast<std::byte const*>(sections) - image.data();
    if (size_t(sectionOffset) > image.size()
        || nt->FileHeader.NumberOfSections > (image.size() - sectionOffset) / sizeof(IMAGE_SECTION_HEADER)) {
        return nullptr;
    }
    std::byte* result{};
    for (size_t index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        auto const& section = sections[index];
        if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
            continue;
        }
        if (section.VirtualAddress > image.size() || section.Misc.VirtualSize > image.size() - section.VirtualAddress) {
            return nullptr;
        }
        auto code   = image.subspan(section.VirtualAddress, section.Misc.VirtualSize);
        auto search = [&](auto begin) {
            return std::search(begin, code.end(), signature.begin(), signature.end(), [](auto byte, auto element) {
                return element == byte;
            });
        };
        auto found = search(code.begin());
        if (found == code.end()) {
            continue;
        }
        if (result || search(found + 1) != code.end()) {
            return nullptr;
        }
        result = std::to_address(found);
    }
    if (!result) {
        return nullptr;
    }
    DWORD64     base{};
    auto const* entry = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(result), &base, nullptr);
    if (!entry || base != reinterpret_cast<DWORD64>(image.data())
        || base + entry->BeginAddress != reinterpret_cast<DWORD64>(result) || entry->EndAddress <= entry->BeginAddress
        || entry->EndAddress > image.size()) {
        return nullptr;
    }
    return result;
}

} // namespace my_mod::function_signature
