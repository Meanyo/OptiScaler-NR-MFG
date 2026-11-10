#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <nvsdk_ngx.h>
#include <spdlog/spdlog.h>

namespace DlssNr
{
inline void LogSizeSelection(bool vulkan, bool after, NVSDK_NGX_Parameter* params,
                             uint64_t textureWidth, unsigned int textureHeight,
                             unsigned int width, unsigned int height,
                             unsigned int guideWidth, unsigned int guideHeight)
{
    if (!spdlog::should_log(spdlog::level::info))
        return;
    unsigned int parameterWidth = 0, parameterHeight = 0;
    params->Get(NVSDK_NGX_Parameter_OutWidth, &parameterWidth);
    params->Get(NVSDK_NGX_Parameter_OutHeight, &parameterHeight);
    const std::array<uint64_t, 10> sizes { vulkan, after, textureWidth, textureHeight,
        parameterWidth, parameterHeight, width, height, guideWidth, guideHeight };
    static thread_local std::array<uint64_t, 10> lastSizes {};
    static thread_local std::chrono::steady_clock::time_point lastLog {};
    const auto now = std::chrono::steady_clock::now();
    if (sizes == lastSizes || (lastLog != std::chrono::steady_clock::time_point{} &&
                              now - lastLog < std::chrono::seconds(1)))
        return;
    lastSizes = sizes;
    lastLog = now;
    spdlog::info("DLSS-NR sizing [{} {}]: texture {}x{}, parameter OutWidth/OutHeight {}x{} "
                 "(diagnostic only), selected {}x{}, guides {}x{}, source={}",
                 vulkan ? "Vulkan" : "D3D12/bridge", after ? "SR -> NR" : "NR -> SR",
                 textureWidth, textureHeight, parameterWidth, parameterHeight,
                 width, height, guideWidth, guideHeight,
                 after ? "full-output-resource" : "active-render-rectangle");
}
}
