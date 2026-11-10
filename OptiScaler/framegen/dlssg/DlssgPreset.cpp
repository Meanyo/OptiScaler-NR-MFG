#include "pch.h"
#include "DlssgPreset.h"
#include "DlssgPresetPolicy.h"

#include <Util.h>
#include <proxies/KernelBase_Proxy.h>
#include <detours/detours.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <mutex>

namespace
{
using Reader = bool (*)(uint32_t, uint32_t*);
struct Provider
{
    HMODULE Module = nullptr;
    Reader Original = nullptr;
    unsigned int Preset = 0;
    bool SupportsC = false;
    std::atomic<bool> Ready {false};
    std::atomic<uint64_t> Reads {0};
    std::atomic<uint64_t> Overrides {0};
    std::atomic<unsigned int> Observed {0};
    std::atomic<bool> ObservedValid {false};
};
std::array<Provider, 32> providers;
std::mutex installMutex;
bool selectionLatched = false;
unsigned int launchPreset = 0;
thread_local bool observing = false;

template <size_t Index> __declspec(noinline) bool ReadPreset(uint32_t key, uint32_t* value)
{
    auto& provider = providers[Index];
    const bool ready = provider.Ready.load(std::memory_order_acquire);
    const auto original = provider.Original;
    if (!original)
        return false;
    if (!ready || key != DlssgPreset::Policy::SettingId || value == nullptr)
        return original(key, value);
    // Limit this override to this provider's own reads, including its report path.
    HMODULE caller = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(_ReturnAddress()), &caller);
    if (caller != provider.Module)
        return original(key, value);

    bool result = true;
    if (provider.Preset != 0)
    {
        *value = provider.Preset;
        provider.Overrides.fetch_add(1, std::memory_order_relaxed);
    }
    else
        result = original(key, value);
    provider.Observed.store(result ? *value : 0, std::memory_order_relaxed);
    provider.ObservedValid.store(result, std::memory_order_relaxed);
    if (provider.Reads.fetch_add(1, std::memory_order_relaxed) == 0)
        LOG_INFO("DLSSG preset: provider {:p} read {} (local override: {}, independent of MFG count)",
                 (void*) provider.Module, result ? DlssgPreset::Name(*value) : "not reported", provider.Preset != 0);
    return result;
}
template <size_t... Index> constexpr auto Thunks(std::index_sequence<Index...>)
{
    return std::array<Reader, sizeof...(Index)> {&ReadPreset<Index>...};
}
constexpr auto readers = Thunks(std::make_index_sequence<32> {});
} // namespace

const char* DlssgPreset::Name(unsigned int preset)
{
    switch (preset)
    {
    case 0: return "DEFAULT";
    case 1: return "A";
    case 2: return "B";
    case 3: return "C";
    default: return "UNKNOWN";
    }
}

void DlssgPreset::Observe(HMODULE module)
{
    // Version-resource loading can re-enter the library hooks. Never hold our lock across it.
    if (!module || observing)
        return;
    observing = true;
    struct Guard { ~Guard() { observing = false; } } guard;
    {
        std::lock_guard lock(installMutex);
        for (const auto& entry : providers)
            if (entry.Module == module)
                return;
    }
    if (!KernelBaseProxy::GetProcAddress_()(module, "NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl") ||
        KernelBaseProxy::GetProcAddress_()(module, "NVSDK_NGX_DirectSR_Create"))
        return;
    wchar_t path[32768] {};
    if (!GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path))))
        return;
    version_t file {}, product {};
    if (!Util::GetFileVersion(path, &file, &product))
        return;
    const bool supported = Policy::Supported(file.major, file.minor, file.patch);
    const bool supportsC = Policy::SupportsC(file.major, file.minor, file.patch);
    const auto target = supported ? Policy::FindReader(module) : nullptr;

    std::lock_guard lock(installMutex);
    for (size_t index = 0; index < providers.size(); ++index)
    {
        auto& provider = providers[index];
        if (provider.Module == module)
            return;
        if (provider.Module != nullptr)
            continue;
        if (!selectionLatched)
        {
            launchPreset = Config::Instance()->FGDLSSGModelPreset.value_or_default();
            if (!Policy::Valid(launchPreset))
                launchPreset = 0;
            selectionLatched = true;
        }
        // Cached function pointers and our per-provider thunks must never outlive the mapped image.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(module), &pinned) || pinned != module)
            return;
        provider.Module = module;
        provider.SupportsC = supportsC;
        provider.Preset = Policy::Effective(launchPreset, supportsC);
        LOG_INFO(L"DLSSG preset: provider path {}", path);
        if (!target)
        {
            LOG_WARN("DLSSG preset: {}.{}.{} has no verified preset reader; selection {} was NOT applied",
                     file.major, file.minor, file.patch, Name(launchPreset));
            return;
        }
        HMODULE hookOwner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(readers[index]), &hookOwner))
        {
            LOG_WARN("DLSSG preset: cannot retain hook owner; no preset override installed");
            return;
        }
        provider.Original = reinterpret_cast<Reader>(target);
        LONG error = DetourTransactionBegin();
        if (error == NO_ERROR)
        {
            error = DetourUpdateThread(GetCurrentThread());
            if (error == NO_ERROR)
                error = DetourAttach(reinterpret_cast<PVOID*>(&provider.Original),
                                     reinterpret_cast<PVOID>(readers[index]));
            if (error == NO_ERROR)
                error = DetourTransactionCommit();
            else
                DetourTransactionAbort();
        }
        provider.Ready.store(error == NO_ERROR, std::memory_order_release);
        LOG_INFO("DLSSG preset: {}.{}.{} reader RVA {:X}, hook {}, selected {}, effective {}, C supported {}",
                 file.major, file.minor, file.patch,
                 reinterpret_cast<uintptr_t>(target) - reinterpret_cast<uintptr_t>(module), error,
                 Name(launchPreset), Name(provider.Preset), supportsC);
        if (launchPreset == 3 && !supportsC)
            LOG_WARN("DLSSG preset: C requires verified 310.9.16 provider; this copy retains game/driver selection");
        return;
    }
    LOG_WARN("DLSSG preset: provider slot limit reached; no override installed");
}

DlssgPreset::Status DlssgPreset::LastStatus()
{
    std::lock_guard lock(installMutex);
    Status result {};
    result.SelectionLatched = selectionLatched;
    result.LaunchPreset = launchPreset;
    for (const auto& provider : providers)
    {
        if (!provider.Module)
            continue;
        ++result.ProvidersSeen;
        result.ReadersInstalled += provider.Ready.load(std::memory_order_acquire) ? 1 : 0;
        result.PresetCProviders += provider.SupportsC ? 1 : 0;
        const auto reads = provider.Reads.load(std::memory_order_relaxed);
        result.Reads += reads;
        result.Overrides += provider.Overrides.load(std::memory_order_relaxed);
        // Prefer evidence from a provider where the chosen override actually applied.
        if (reads && provider.ObservedValid.load(std::memory_order_relaxed) &&
            (!result.ObservedValid || provider.Overrides.load(std::memory_order_relaxed)))
        {
            result.ObservedPreset = provider.Observed.load(std::memory_order_relaxed);
            result.ObservedValid = true;
        }
    }
    return result;
}
