#pragma once

#include <Config.h>
#include <Util.h>
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <optional>

namespace DlssNr
{
// Load one NR runtime; presets select the model inside it.
inline std::optional<std::filesystem::path> FindNrFile(const char* name)
{
    auto exact = [&](const std::filesystem::path& directory) -> std::optional<std::filesystem::path>
    {
        std::error_code ec;
        const auto path = std::filesystem::absolute(directory / name, ec).lexically_normal();
        if (!ec && std::filesystem::is_regular_file(path, ec) && !ec)
            return path;
        return std::nullopt;
    };
    if (auto path = exact(Util::DllPath().parent_path()))
        return path;
    const auto& configured = Config::Instance()->MainDllPath;
    if (configured.has_value())
        if (auto path = exact(std::filesystem::path(configured.value())))
            return path;
    return exact(Util::ExePath().parent_path());
}

inline std::optional<std::filesystem::path> FindNrModelFile()
{
    return FindNrFile("nvngx_dlssnr.dll");
}

inline HMODULE LoadNrForwarder(const std::filesystem::path& path)
{
    // Check the loaded module matches the requested absolute path.
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
        return nullptr; // Refuse an unexpected module path.
    return module;
}
}
