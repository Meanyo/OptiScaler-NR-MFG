#pragma once

#include <Config.h>
#include <Util.h>
#include <algorithm>
#include <filesystem>
#include <optional>

namespace DlssNr
{
// Separate forwarder DLL identities keep each model's cached entry points and initialization
// isolated. Never unload them: retired GPU features can still call the previous model.
inline unsigned int SelectedModel()
{
    return std::min(Config::Instance()->DlssNrModelLibrary.value_or_default(), 1u);
}

inline const char* ModelFileName(unsigned int model = SelectedModel())
{
    return model == 1 ? "nvngx_dlssnr_310_9_16.dll" : "nvngx_dlssnr.dll";
}

inline const char* ModelForwarderName(unsigned int model = SelectedModel())
{
    return model == 1 ? "nvngx.dll_dlssnr_310_9_16.dll" : "nvngx.dll_dlssnr.dll";
}

inline std::optional<std::filesystem::path> FindModelFile(const char* name)
{
    auto path = Util::FindFilePath(Util::DllPath().remove_filename(), name);
    if (!path)
        path = Util::FindFilePath(Util::ExePath().remove_filename(), name);
    return path;
}

inline std::optional<std::filesystem::path> FindSelectedModel(unsigned int model = SelectedModel())
{
    return FindModelFile(ModelFileName(model));
}
}
