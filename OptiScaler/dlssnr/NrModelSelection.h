#pragma once

#include <Config.h>
#include <Util.h>
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <optional>

namespace DlssNr
{
inline unsigned int SelectedModel()
{
    return std::min(Config::Instance()->DlssNrModelLibrary.value_or_default(), 1u);
}

inline const char* ModelFileName(unsigned int = SelectedModel()) { return "nvngx_dlssnr.dll"; }
inline const char* ModelForwarderName(unsigned int = SelectedModel()) { return "nvngx.dll_dlssnr.dll"; }

inline std::filesystem::path ModelDirectory()
{
    const auto& configured = Config::Instance()->MainDllPath;
    const auto root = configured.has_value()
                          ? std::filesystem::path(configured.value())
                          : Util::DllPath().parent_path() / "OptiScaler";
    return std::filesystem::absolute(root / "NR-310.9.16").lexically_normal();
}

// Exact paths only. Recursive searches could silently select the new model for the old slot.
inline std::optional<std::filesystem::path> FindModelFile(const char* name,
                                                        unsigned int model = SelectedModel())
{
    auto exact = [&](const std::filesystem::path& directory) -> std::optional<std::filesystem::path>
    {
        std::error_code ec;
        const auto path = std::filesystem::absolute(directory / name, ec).lexically_normal();
        if (!ec && std::filesystem::is_regular_file(path, ec) && !ec)
            return path;
        return std::nullopt;
    };
    if (model == 1)
        return exact(ModelDirectory()); // Never fall back to the old model or renamed R12 files.
    if (auto path = exact(Util::DllPath().parent_path()))
        return path;
    const auto& configured = Config::Instance()->MainDllPath;
    if (configured.has_value())
        if (auto path = exact(std::filesystem::path(configured.value())))
            return path;
    return exact(Util::ExePath().parent_path());
}

inline std::optional<std::filesystem::path> FindSelectedModel(unsigned int model = SelectedModel())
{
    return FindModelFile(ModelFileName(model), model);
}

inline HMODULE LoadModelForwarder(const std::filesystem::path& path)
{
    // Identical basenames in different directories must remain separate module instances.
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    HMODULE module = GetModuleHandleW(absolute.c_str());
    if (!module)
        module = LoadLibraryExW(absolute.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module)
        return nullptr;
    wchar_t actual[32768] = {};
    const DWORD length = GetModuleFileNameW(module, actual, (DWORD)std::size(actual));
    std::error_code ec;
    if (!length || length >= std::size(actual) ||
        !std::filesystem::equivalent(absolute, std::filesystem::path(actual), ec) || ec)
        return nullptr; // Refuse an aliased module; never execute another slot's callbacks.
    return module;
}
}
