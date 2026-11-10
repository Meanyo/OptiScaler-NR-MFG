#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <imgui/imgui.h>

namespace ChineseUi
{
inline bool LoadFont(ImFontAtlas* atlas, float size, const wchar_t* customPath = nullptr)
{
    static const ImWchar glyphs[] = { 0x0020, 0x00FF, 0x2000, 0x206F, 0x2190, 0x21FF,
                                    0x3000, 0x30FF, 0x4E00, 0x9FFF, 0xFF00, 0xFFEF, 0 };
    wchar_t windows[MAX_PATH] {};
    GetWindowsDirectoryW(windows, MAX_PATH);
    const std::filesystem::path fonts = std::filesystem::path(windows) / L"Fonts";
    const std::filesystem::path candidates[] = {
        customPath ? std::filesystem::path(customPath) : fonts / L"msyh.ttc",
        fonts / L"msyh.ttc", fonts / L"simhei.ttf", fonts / L"simsun.ttc"
    };
    for (const auto& path : candidates)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) continue;
        const auto bytes = file.tellg();
        if (bytes <= 0 || bytes > 64 * 1024 * 1024) continue;
        void* data = IM_ALLOC(static_cast<size_t>(bytes));
        file.seekg(0);
        if (!file.read(static_cast<char*>(data), bytes)) { IM_FREE(data); continue; }
        ImFontConfig cfg;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        atlas->TexMinWidth = 2048;
        ImFont* font = atlas->AddFontFromMemoryTTF(data, static_cast<int>(bytes), size, &cfg,
                                                 glyphs);
        if (font) { ImGui::GetIO().FontDefault = font; return true; }
    }
    return false;
}

inline void ApplyGeometry(ImGuiStyle& style)
{
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 7.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;
    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.FramePadding = ImVec2(7.0f, 5.0f);
    style.ItemSpacing = ImVec2(9.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 12.0f;
}
}

