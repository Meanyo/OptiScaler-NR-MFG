#pragma once
#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>

namespace CompactMenuLayout
{
inline constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

struct Bounds { ImVec2 preferred, minimum, maximum; float margin; };
inline Bounds GetBounds(ImVec2 display, float scale)
{
    if (!std::isfinite(scale) || scale <= 0) scale = 1;
    const float margin = std::min(16.0f, std::max(0.0f, std::min(display.x, display.y) * .05f));
    const float availableX = std::max(1.0f, display.x - margin * 2);
    const float availableY = std::max(1.0f, display.y - margin * 2);
    Bounds b;
    b.margin = margin;
    b.maximum = ImVec2(std::min(1120.0f * scale, availableX), availableY);
    b.minimum = ImVec2(std::min(640.0f * scale, b.maximum.x), std::min(340.0f * scale, b.maximum.y));
    b.preferred = ImVec2(std::clamp(880.0f * scale, b.minimum.x, b.maximum.x),
                        std::clamp(760.0f * scale, b.minimum.y, b.maximum.y));
    return b;
}

// No AlwaysAutoResize: fill-width children and percentage-width controls must not size their parent.
// Initialize once, not every frame; user resizing survives tab/section changes and closing/reopening.
inline bool Begin(const char* title, float scale, bool resetSize)
{
    const auto display = ImGui::GetIO().DisplaySize;
    const auto b = GetBounds(display, scale);
    ImGui::SetNextWindowSizeConstraints(b.minimum, b.maximum);
    ImGui::SetNextWindowSize(b.preferred, resetSize ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(display.x * .5f, display.y * .5f), ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
    const bool visible = ImGui::Begin(title, nullptr, WindowFlags);
    const auto size = ImGui::GetWindowSize();
    const auto pos = ImGui::GetWindowPos();
    const ImVec2 clamped(std::clamp(pos.x, b.margin, std::max(b.margin, display.x - size.x - b.margin)),
                         std::clamp(pos.y, b.margin, std::max(b.margin, display.y - size.y - b.margin)));
    if (clamped.x != pos.x || clamped.y != pos.y) ImGui::SetWindowPos(clamped);
    return visible;
}

inline void NextButton(const char* label)
{
    ImGui::SameLine();
    const float width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
    if (ImGui::GetContentRegionAvail().x < width) ImGui::NewLine();
}

// Shared with the regression harness: use actual translated labels, not a stale English width.
template<class Save, class Close, class Docs>
inline void Actions(Save save, Close close, Docs docs)
{
    NextButton("保存设置");
    if (ImGui::Button("保存设置")) save();
    NextButton("关闭窗口");
    if (ImGui::Button("关闭窗口")) close();
    NextButton("打开使用文档");
    if (ImGui::Button("打开使用文档")) docs();
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("在默认浏览器打开使用说明、兼容性列表与常见问题。");
        ImGui::EndTooltip();
    }
}
}

