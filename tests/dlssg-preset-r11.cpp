#define NOMINMAX
#include "../OptiScaler/framegen/dlssg/DlssgPresetPolicy.h"
#include "../OptiScaler/framegen/dlssg/MfgMidpoint.h"
#include "../OptiScaler/include/detours/detours.h"
#include <cstdio>
#include <string>
#include <vector>

static_assert(DlssgPreset::Policy::Valid(0) && DlssgPreset::Policy::Valid(3));
static_assert(!DlssgPreset::Policy::Valid(4) && !DlssgPreset::Policy::Valid(0xFFFFFFFF));
static_assert(DlssgPreset::Policy::Effective(3, false) == 0);
static_assert(DlssgPreset::Policy::Effective(3, true) == 3);
static_assert(DlssgPreset::Policy::Effective(1, false) == 1);
static_assert(DlssgPreset::Policy::Effective(2, false) == 2);
static_assert(DlssgPreset::Policy::SupportsC(310, 9, 16));
static_assert(!DlssgPreset::Policy::SupportsC(310, 9, 1));
static_assert(!DlssgPreset::Policy::Supported(310, 9, 17));

using Reader = bool (*)(uint32_t, uint32_t*);
Reader originalReader = nullptr;
unsigned int selectedPreset = 0;
bool HookedReader(uint32_t key, uint32_t* value)
{
    if (key == DlssgPreset::Policy::SettingId && value && selectedPreset)
    {
        *value = selectedPreset;
        return true;
    }
    return originalReader(key, value);
}

bool CheckDetour(void* reader)
{
    if (!reader) return false;
    unsigned char before[32];
    std::memcpy(before, reader, sizeof(before));
    originalReader = reinterpret_cast<Reader>(reader);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&originalReader), reinterpret_cast<PVOID>(HookedReader));
    if (DetourTransactionCommit() != NO_ERROR) return false;
    bool ok = true;
    const auto entry = reinterpret_cast<Reader>(reader);
    for (selectedPreset = 1; selectedPreset <= 3; ++selectedPreset)
    {
        uint32_t value = 0xDEADBEEF;
        ok &= entry(DlssgPreset::Policy::SettingId, &value) && value == selectedPreset;
    }
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(reinterpret_cast<PVOID*>(&originalReader), reinterpret_cast<PVOID>(HookedReader));
    ok &= DetourTransactionCommit() == NO_ERROR;
    ok &= std::memcmp(before, reader, sizeof(before)) == 0;
    std::printf("preset reader ABI detour + A/B/C forwarding + byte restoration: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

size_t CountPattern(HMODULE module, const std::vector<int>& pattern)
{
    const auto base = reinterpret_cast<const uint8_t*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto sections = IMAGE_FIRST_SECTION(nt);
    size_t count = 0;
    for (unsigned int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        const auto& section = sections[i];
        if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const auto start = base + section.VirtualAddress;
        for (size_t off = 0; off + pattern.size() <= section.Misc.VirtualSize; ++off)
        {
            bool matches = true;
            for (size_t j = 0; j < pattern.size(); ++j)
                matches &= pattern[j] < 0 || start[off + j] == pattern[j];
            count += matches;
        }
    }
    return count;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) return 2;
    int failures = 0;
    for (int i = 1; i < argc; ++i)
    {
        // Map/relocate the image without initializing NGX, CUDA, the driver or a graphics device.
        const auto module = LoadLibraryExW(argv[i], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        if (!module) { std::printf("FAIL: map image error %lu\n", GetLastError()); ++failures; continue; }
        const auto reader = DlssgPreset::Policy::FindReader(module);
        const auto advertise = CountPattern(module, {0x81,0xFD,0xB0,1,0,0,0x0F,0x8C,-1,-1,-1,-1,0xBF,5,0,0,0});
        const auto validate = CountPattern(module, {0x3D,0xB0,1,0,0,0x0F,0x93,0xC0});
        std::printf("%ls\npreset reader RVA: %llX; MFG advertise hits: %zu; validate hits: %zu\n", argv[i],
                    reader ? reinterpret_cast<uintptr_t>(reader) - reinterpret_cast<uintptr_t>(module) : 0,
                    advertise, validate);
        if (!reader || advertise != 1 || validate != 1) ++failures;
        if (!CheckDetour(reader)) ++failures;
        std::vector<mfgunlock::midpoint::Patch> patches;
        void* allocation = nullptr;
        std::string detail;
        const bool temporal = mfgunlock::midpoint::Apply(module, patches, allocation, detail);
        std::printf("temporal correction: %s; %s\n", temporal ? "PASS" : "FAIL", detail.c_str());
        if (!temporal || patches.empty()) ++failures;
        mfgunlock::midpoint::Restore(patches, allocation);
        FreeLibrary(module);
    }
    std::printf("%s (%d failures); no GPU execution performed\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
