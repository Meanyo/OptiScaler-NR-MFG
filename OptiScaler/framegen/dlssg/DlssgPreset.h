#pragma once

#include <windows.h>
#include <cstdint>

// DLSS-G's preset is a provider initialization setting, independent of the frame multiplier.
// A new selection is saved for the next process; cached programs are never switched in flight.
namespace DlssgPreset
{
struct Status
{
    bool SelectionLatched = false;
    unsigned int LaunchPreset = 0;
    unsigned int ProvidersSeen = 0;
    unsigned int ReadersInstalled = 0;
    unsigned int PresetCProviders = 0;
    uint64_t Reads = 0;
    uint64_t Overrides = 0;
    unsigned int ObservedPreset = 0;
    bool ObservedValid = false;
};

void Observe(HMODULE module);
Status LastStatus();
const char* Name(unsigned int preset);
} // namespace DlssgPreset
