// Provider layout facts and selection semantics adapted from dashdogy/RTX40MFG-Unlock.
// Copyright (c) 2026 Michael Robles. MIT; see LICENSE-dashdogy.txt.
#pragma once

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace DlssgPreset::Policy
{
inline constexpr uint32_t SettingId = 0x10E41DF1;

constexpr bool Valid(unsigned int preset) { return preset <= 3; }
constexpr bool SupportsC(unsigned int major, unsigned int minor, unsigned int patch)
{
    return major == 310 && minor == 9 && patch == 16;
}
constexpr bool Supported(unsigned int major, unsigned int minor, unsigned int patch)
{
    return major == 310 && minor == 9 && (patch == 0 || patch == 1 || patch == 16);
}
constexpr unsigned int Effective(unsigned int requested, bool supportsC)
{
    return Valid(requested) && (requested != 3 || supportsC) ? requested : 0;
}

// A preset-key call must resolve to a single, known reader at its unwind function entry.
// No fixed RVA or first-match fallback: an unfamiliar provider remains untouched.
inline void* FindReader(HMODULE module)
{
    if (!module)
        return nullptr;
    auto base = reinterpret_cast<const uint8_t*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000)
        return nullptr;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->FileHeader.NumberOfSections == 0 || nt->FileHeader.NumberOfSections > 96)
        return nullptr;
    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    auto sections = IMAGE_FIRST_SECTION(nt);
    if (reinterpret_cast<const uint8_t*>(sections + nt->FileHeader.NumberOfSections) > base + imageSize)
        return nullptr;
    constexpr uint8_t prefix[] = {0xB9, 0xF1, 0x1D, 0xE4, 0x10, 0xE8};
    constexpr uint8_t suffix[] = {0x84, 0xC0, 0x0F, 0x84};
    constexpr uint8_t prologue[] = {0x48,0x89,0x5C,0x24,0x10,0x89,0x4C,0x24,0x08,
                                    0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xFA,0x8B,0xD9};
    auto executable = [module, base, imageSize](uintptr_t address, size_t bytes) {
        const uintptr_t begin = reinterpret_cast<uintptr_t>(base);
        if (address < begin || address - begin >= imageSize || bytes > imageSize - (address - begin))
            return false;
        MEMORY_BASIC_INFORMATION memory {};
        return VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) &&
               memory.AllocationBase == module && memory.State == MEM_COMMIT && memory.Type == MEM_IMAGE &&
               !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
               (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
    };
    void* found = nullptr;
    for (unsigned int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        const auto& section = sections[i];
        if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE) || section.VirtualAddress >= imageSize)
            continue;
        const size_t bytes = std::min<size_t>(section.Misc.VirtualSize, imageSize - section.VirtualAddress);
        auto start = base + section.VirtualAddress;
        for (size_t offset = 0; offset + 18 <= bytes; ++offset)
        {
            const auto p = start + offset;
            if (std::memcmp(p, prefix, sizeof(prefix)) || std::memcmp(p + 10, suffix, sizeof(suffix)))
                continue;
            int32_t displacement = 0, branchDisplacement = 0;
            std::memcpy(&displacement, p + 6, 4);
            std::memcpy(&branchDisplacement, p + 14, 4);
            const auto target = reinterpret_cast<uintptr_t>(p + 10) + displacement;
            const auto branch = reinterpret_cast<uintptr_t>(p + 18) + branchDisplacement;
            if (!executable(target, sizeof(prologue)) || !executable(branch, 1) ||
                std::memcmp(reinterpret_cast<void*>(target), prologue, sizeof(prologue)))
                return nullptr;
            DWORD64 imageBase = 0;
            const auto function = RtlLookupFunctionEntry(target, &imageBase, nullptr);
            if (!function || imageBase != reinterpret_cast<DWORD64>(module) ||
                imageBase + function->BeginAddress != target || function->EndAddress <= function->BeginAddress ||
                function->EndAddress > imageSize || (found && found != reinterpret_cast<void*>(target)))
                return nullptr;
            found = reinterpret_cast<void*>(target);
        }
    }
    return found;
}
} // namespace DlssgPreset::Policy
