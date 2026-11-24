#include "pch.h"
#include "DlssNrFeature_Vk.h"

#include "DlssNr.h"
#include "NrModelFiles.h"
#include "DlssNr_ExposureScan.h"


#include <Config.h>
#include <menu/menu_common.h>

#include <imgui/imgui.h>

#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace DlssNr
{

static const char* LocalizedHelp(const char* tip)
{
    struct Entry { const char* starts; const char* zh; };
    static const Entry entries[] = {
        { "Synthesises detail", "利用神经模型合成细节，并在帧生成之前完成处理。需要所选 NR 模型文件及其配套转发库，文件名见运行库版本选项。此功能为非官方实验集成。" },
        { "Whether the model's edit", "控制是否把模型修改合成到画面。关闭时模型仍会运行，适合配合“冻结 NR 输入帧”进行同一帧对比；正常使用请开启。" },
        { "The whole pass", "这里统计完整 NR 流程的耗时，包括颜色转换、模型执行和合成。可与窗口底部帧时间对照判断性能开销。" },
        { "The filter that averages", "把超采样模型结果缩小到显示分辨率。Lanczos3、Kaiser3 较锐利；Bicubic、Catmull-Rom 更柔和。它与输出缩放滤镜互相独立。" },
        { "What fraction of the frame", "模型工作分辨率。降低比例可显著减少耗时，但模型合成的细小结构会变软；原始画面本身不会被降低分辨率。高于 100% 为昂贵的超采样实验模式。" },
        { "How the model's work is brought", "低于 100% 时的还原方式。匹配残差只放大模型产生的差异，通常比经典合成更少色偏；100% 及以上两者没有区别。" },
        { "How far the frame moves", "细节合成强度。0 为超分原图，1 为模型正常结果；高于 1 会夸大变化，主要用于观察模型作用。" },
        { "Whether the model's colour", "色彩合成强度。0 尽量保留游戏原色，只采用亮度细节；1 使用模型色彩；高于 1 会增加饱和度。" },
        { "What the model is shown", "控制送入模型的代理曲线及其还原方式。混合代理 + 合成通常兼顾中间调、高光细节和稳定性；替换模式闪烁风险更高，适合测试。" },
        { "Default leaves the choice", "这是 NR 模型自身的预设，与 DLSS 超分或光线重建预设编号没有对应关系。自动会交给模型选择。" },
        { "The model's own processing profiles", "标准效果最强；自然更接近游戏原有肤色和色调；电影感会减弱油亮与过度处理。修改后会重建模型。" },
        { "The model's own strength", "模型内部强度，与模型运行后再进行的“细节混合强度”不同。" },
        { "-1 means follow local structure", "皮肤结构为 -1 时跟随局部结构，这是模型默认值；0 及以上表示独立控制皮肤。" },
        { "Lets the model find skin", "让模型自动识别皮肤区域，而不是对整幅画面统一处理。" },
        { "Where the number that divides", "白点决定线性 HDR 画面如何映射给模型。优先使用游戏曝光；游戏未提供时可让扫描器寻找曝光缓冲并通过锚点校准；固定曝光的游戏可仅用手动白点。" },
        { "The white point for the selected", "设置当前选中锚点的白点；没有锚点时设置初始手动白点。使用对数刻度以覆盖不同游戏的曝光范围。" },
        { "A multiplier on the scan", "对扫描计算出的白点进行微调。调好后新增锚点会把当前结果记录下来，并把微调复位为 1。" },
        { "A multiplier on the exposure", "对游戏提供的曝光结果进行微调。1.00 表示直接采用游戏数值。" },
        { "What the frame is divided", "送入模型前用于归一化线性 HDR 的白点。数值过低容易过曝，过高会使模型输入偏暗。" },
        { "The most the pass may move", "限制 NR 对像素增亮和压暗的最大倍率。2x 通常能保留细节并抑制灯光闪烁；只有高光明显受限时才建议提高。" },
        { "A lamp in the corner", "在屏幕角落显示扫描器是否随光照变化：暗处趋向红色，亮处趋向绿色。它只用于观察，不会改变画面。" },
        { "Make the picture look right", "先把当前场景调到观感正确，再添加锚点。第二个及后续锚点应在不同光照环境下创建，最多八个，可更准确拟合曝光变化。" },
        { "Flip this if", "若进入更亮或更暗场景时白点变化方向相反，请开启。添加第二个锚点后方向可由数据决定。" },
        { "Freezes the frame", "冻结 NR 的输入，便于在完全相同的一帧上比较设置。超分本身不会重新运行，HUD 和 NR 之后的游戏后处理仍可能更新。" },
        { "Shows the pass against", "把原始超分画面与 NR 结果同时显示。左右并排会压缩画面；滑动分割不缩放，更适合实时观察。" },
        { "Writes which side", "把“原图/NR”标签写入画面，截图后仍能分辨两侧。" },
        { "Puts the edited frame", "交换原图与 NR 结果的位置，减少左右视觉偏好对判断的影响。" },
        { "How much of the frame each", "控制并排对比的缩放：1 显示完整画面并留黑边，2 填满半屏并裁掉两侧。" },
        { "Where the wipe cuts", "设置滑动分割线位置：左侧为超分原图，右侧为 NR 结果。" },
        { "Proxy is the picture", "模型输入代理用于检查白点是否正确；差异视图会放大模型实际修改，接近纯灰表示变化很小。" }
    };
    for (const auto& entry : entries)
        if (std::strstr(tip, entry.starts) == tip)
            return entry.zh;
    return tip;
}

// The "(?)" marker every control carries, matching the rest of the menu.
static void HelpMarker(const char* tip)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0f);
        ImGui::TextUnformatted(LocalizedHelp(tip));
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// A slider that only writes its value when the handle is released.
//
// Some controls -- intensity, the structure and tone strengths -- are read by the model once, when
// the feature is built, so changing one rebuilds the whole feature. Writing on every pixel of a drag
// meant a rebuild per frame, felt as the picture hitching while you scrub. The slider still tracks
// live under the cursor; only the commit that triggers the rebuild waits for release. Cheap controls
// that are just shader constants (detail, colour, paper white) do not use this -- they can afford to
// apply live.
static bool DeferredSlider(const char* label, CustomOptional<float>* opt, float mn, float mx,
                           const char* fmt = "%.2f")
{
    static std::unordered_map<std::string, float> pending;

    auto it = pending.find(label);
    float value = it != pending.end() ? it->second : opt->value_or_default();
    bool changed = false;

    if (ImGui::SliderFloat(label, &value, mn, mx, fmt))
        pending[label] = value;

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        auto committed = pending.find(label);

        if (committed != pending.end())
        {
            *opt = std::clamp(committed->second, mn, mx);
            pending.erase(committed);
            changed = true;
        }
    }

    return changed;
}

// Separate stable IDs: never append a localized slider's ### label to another widget.
static void AdjustmentPreviewToggle(Config* config, const char* id, const char* label, uint32_t mode)
{
    bool enabled = config->DlssNrAdjustmentPreview.value_or_default() == mode;
    ImGui::PushID(id);
    if (ImGui::Checkbox(label, &enabled))
    {
        config->DlssNrAdjustmentPreview = enabled ? mode : 0u;
        if (enabled)
        {
            config->DlssNrEffectPreview = false;
            config->DlssNrDebugView = 0u;
        }
    }
    ImGui::PopID();
}

void RenderMenu(Config* config, float menuResScale)
{

    // DLSS Neural Rendering -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("神经渲染 NR###DLSS Neural Rendering"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool enabled = config->DlssNrEnabled.value_or_default();
        if (ImGui::Checkbox("启用神经渲染###Enable Neural Rendering", &enabled))
            config->DlssNrEnabled = enabled;

        HelpMarker("Synthesises detail in the upscaler's output, before frame generation sees it."
                       "\n\nNeeds two similarly named files beside OptiScaler, one character apart:"
                       "\n  nvngx_dlssnr.dll       NVIDIA's model (~165 MB) -- you supply it"
                       "\n  nvngx.dll_dlssnr.dll   the forwarder (~13 KB) -- ships in this package"
                       "\nUndocumented and driven directly, so none of this is officially supported.");

        static const char* nrPresetNames[] = { "自动（模型默认）", "标准模型（Preset 1）", "轻量模型（Preset 2）" };
        int preset = (int) config->DlssNrPreset.value_or_default();
        if (ImGui::Combo("NR 模型###Model preset", &preset, nrPresetNames, IM_ARRAYSIZE(nrPresetNames)))
            config->DlssNrPreset = (uint32_t) preset;
        HelpMarker("使用同一份新版 310.9.16 模型 DLL。标准和轻量分别对应 Preset 1 和 2；自动跟随模型默认。可在启用 NR 前选择，也可运行中切换；切换会重建全部 NR 层并重置历史，可能短暂停顿。40 系显卡需使用 Ada 专用模型补丁。保存设置可记住选择。");
        if (!FindNrModelFile())
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "缺少 nvngx_dlssnr.dll，请放到插件或游戏目录。");
        if (!FindNrFile("nvngx.dll_dlssnr.dll"))
            ImGui::TextWrapped("缺少配套转发库：nvngx.dll_dlssnr.dll");

        int order = config->DlssNrAfterUpscale.value_or_default() ? 1 : 0;
        static const char* orderNames[] = {
            "NR → SR：先增强低分辨率输入，再超分",
            "SR → NR：先超分，再增强完整输出"
        };
        if (ImGui::Combo("处理顺序###NR processing order", &order, orderNames, IM_ARRAYSIZE(orderNames)))
            config->DlssNrAfterUpscale = order == 1;
        HelpMarker("NR → SR 在游戏渲染分辨率运行模型，速度更快并把增强结果交给超分。"
                   "\nSR → NR 在实际 SR 输出资源的完整尺寸运行，通常更清晰但开销更高。"
                   "\n切换会重置时序历史；分辨率变化时模型会重建，可能短暂停顿。");

        const bool after = config->DlssNrAfterUpscale.value_or_default();
        ImGui::TextColored(ImVec4(0.36f, 0.90f, 0.76f, 1.0f), "%s",
                           after ? "低分辨率输入 → SR 超分 → NR 增强输出"
                                 : "低分辨率输入 → NR 增强 → SR 超分输出");

        // The toggle can be bound to a key, and nobody would think to look for it under Keybinds
        // unless told. Dimmed, because it is a note rather than a setting.
        ImGui::TextDisabled("可在“按键绑定”中设置神经渲染开关键。");

        bool applyModel = config->DlssNrApplyModel.value_or_default();
        if (ImGui::Checkbox("应用模型效果###Apply the model", &applyModel))
            config->DlssNrApplyModel = applyModel;

        HelpMarker("Whether the model's edit is applied. Off shows the clean upscaler frame while the"
                       "\npass keeps running -- so with Hold frame (under Compare) you can freeze a"
                       "\nframe and toggle this to see the same frozen frame with and without Neural"
                       "\nRendering. Leave it on for normal use.");

        // Either backend. The two keep separate state, and on a native Vulkan game the D3D12 side
        // is never touched -- so asking only that one reports "waiting for the upscaler" over a pass
        // that is demonstrably running.
        const bool vulkan = DlssNr::IsRunningVk();

        // Turning the pass off does not release the model, so the feature handle stays alive and
        // IsRunning keeps answering yes. Reporting a cost from that was wrong in the way that matters
        // most: the toggle is how anyone A/Bs this, so the one moment the number is read is the one
        // moment it describes the frame before last.
        if (!enabled)
        {
            ImGui::TextDisabled("已关闭；模型仍保留在显存中，再次开启可立即生效。");
        }
        else if (!DlssNr::IsRunning() && !vulkan)
        {
            const char* reason = DlssNr::FailureReason();

            if (reason[0] != 0)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "本次运行已停用：%s", reason);
                ImGui::SameLine();

                if (ImGui::SmallButton("重试###Retry"))
                    DlssNr::RetryAfterFailure();
            }
            else if (enabled)
                ImGui::TextUnformatted("等待超分辨率首次运行……");
        }
        else
        {
            // The cost belongs here rather than only in the upscaler's breakdown: that tooltip needs
            // OptiScaler's own upscaler to have run, and with native DLSS passing through there is
            // nothing in it to hang this off.
            // Either backend's timer. They measure the same thing by different means, and only one
            // of them is running.
            const auto ms = vulkan ? DlssNr::LastGpuTimeVk() : DlssNr::LastGpuTime();

            // With "应用模型效果###Apply the model" off the pass STILL RUNS (so Hold-frame A/B can toggle its edit on
            // a frozen frame) -- it only outputs the clean frame. So the cost is real, and saying so
            // stops the reading looking like a bug. Enable Neural Rendering off is what zeroes it.
            const char* runSuffix =
                !config->DlssNrApplyModel.value_or_default() ? "（模型仍运行，效果已隐藏）" : "";

            if (ms.has_value())
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "运行中%s · %.2f ms/帧%s",
                                   vulkan ? " natively on Vulkan" : "", ms.value(), runSuffix);
            else if (vulkan)
                // Measured but not yet read: the first few frames are still in the query ring.
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "原生 Vulkan 运行中 · %llu 帧%s",
                                   DlssNr::FramesVk(), runSuffix);
            else
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "运行中%s", runSuffix);

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("完整 NR 流程耗时：包含暂存复制、模型和最终合成。\n"
                                  "可与窗口底部帧时间对照判断实际开销。");
        }

        ImGui::Spacing();
        ImGui::PushItemWidth(220.0f * menuResScale);

        // Any percentage, rather than a handful of steps somebody chose in advance. The lower bound
        // is 25%: below that the model is working on so little of the picture that its answer no
        // longer survives being enlarged onto it.
        // Applied when the handle is let go, not while it is moving.
        //
        // Every distinct value here is a different working size, and a different working size tears
        // down the scratch textures and rebuilds the model. Writing it on each pixel of a drag meant
        // dozens of rebuilds in a second, which is felt as the whole frame hitching. The slider still
        // reads live; only the commit waits.
        static int pendingScale = -1;

        int scalePercent = pendingScale >= 0
                               ? pendingScale
                               : (int) lroundf(config->DlssNrWorkingScale.value_or_default() * 100.0f);

        if (ImGui::SliderInt("模型工作分辨率###Model resolution", &scalePercent, 25, 200, "%d%%"))
            pendingScale = scalePercent;

        if (ImGui::IsItemDeactivatedAfterEdit() && pendingScale >= 0)
        {
            config->DlssNrWorkingScale = std::clamp(pendingScale, 25, 200) / 100.0f;
            pendingScale = -1;
        }

        if (scalePercent > 100)
            ImGui::TextDisabled("NR 超采样 %.2fx：模型在高于目标分辨率下运行后再缩小。\n"
                                "这是高开销实验功能，耗时会随像素面积快速增加。",
                                scalePercent / 100.0f);

        if (scalePercent > 100)
        {
            static const char* dsNames[] = { "FSR1", "Bicubic", "Catmull-Rom", "Lanczos2",
                                             "Lanczos3", "Kaiser2", "Kaiser3", "MAGIC" };
            int ds = (int) config->DlssNrScalingDownscaler.value_or_default();
            if (ds < 0 || ds >= IM_ARRAYSIZE(dsNames))
                ds = (int) Scaler::Lanczos3;

            if (ImGui::Combo("NR 超采样缩小滤镜###Downscaler (NR)", &ds, dsNames, IM_ARRAYSIZE(dsNames)))
                config->DlssNrScalingDownscaler = (Scaler) ds;

            HelpMarker("The filter that averages the model's above-native answer back to display size --"
                           "\nthis is what turns supersampling into LESS noise rather than more. Sharper"
                           "\nfilters (Lanczos3, Kaiser3) keep the most detail; softer ones (Bicubic,"
                           "\nCatmull-Rom) are gentler on ringing. Independent of the Output Scaling"
                           "\ndownscaler, so the two can differ and run at the same time.");
        }

        HelpMarker("What fraction of the frame the model works at. Cost falls with the square of"
                       "\nthis, so half resolution is roughly a quarter of the time."
                       "\n\nThe frame is never reduced. Only the model's contribution is computed small"
                       "\nand enlarged, so the picture underneath is untouched whatever this says."
                       "\n\nWhat it trades: the shading the model adds is broad and survives enlargement;"
                       "\nthe fine structure it synthesises does not, and softens. Worth having when the"
                       "\npass costs more than you want to pay for the detail it returns."
                       "\n\nThe frame itself stays at full detail whatever this says -- only the"
                       "\nmodel's own work is done small.");

        // Meaningful only when the model runs BELOW the frame's size. At 100% -- and above, where
        // supersampling composites its down-legged answer at native -- the residual collapses to the
        // model's own picture and the two modes are identical, so the control says so by going grey.
        {
            const bool reduced = config->DlssNrWorkingScale.value_or_default() < 0.999f;

            if (!reduced)
                ImGui::BeginDisabled();

            static const char* enlargeNames[] = { "经典合成", "匹配残差" };
            int enlarge = config->DlssNrTransfer.value_or_default() == 1 ? 1 : 0;

            if (ImGui::Combo("低比例还原方式###Enlargement", &enlarge, enlargeNames, IM_ARRAYSIZE(enlargeNames)))
                config->DlssNrTransfer = (uint32_t) enlarge;

            if (!reduced)
                ImGui::EndDisabled();

            HelpMarker("How the model's work is brought back up when it ran below the frame's size."
                       "\n\nClassic composes the model's small picture directly against the full-size"
                       "\nframe. Those two disagree by the shrink's blur as well as by the model's edit,"
                       "\nand the composition cannot tell them apart -- it reads the blur as brightness"
                       "\nthe frame has and the model never saw. The lower the model resolution the"
                       "\nlarger that error, and it is the colour shift that shows up at 50%."
                       "\n\nMatched residual carries up only the model's difference and lays it on the"
                       "\nframe's own proxy, so both pictures being compared are full size and the only"
                       "\nthing that came from the small raster is the edit itself."
                       "\n\nNo effect at 100% or above: there is no residual to carry and the two are"
                       "\nidentical (supersampling brings its answer down to frame size before this)."
                       "\n\nFrom hhkbble's multi-pass work on this fork.");
        }

        ImGui::SeparatorText("特效保护（自动估算）");
        bool effectProtection = config->DlssNrEffectProtection.value_or_default();
        if (ImGui::Checkbox("启用特效保护###NR Effect Protection", &effectProtection))
        {
            config->DlssNrEffectProtection = effectProtection;
            LOG_INFO("DLSS-NR: automatic effect protection {} (source brightness/colour mask)",
                     effectProtection ? "enabled" : "disabled");
        }
        HelpMarker("根据 NR 输入原图的高亮、饱和色和局部亮度差估算光效区域，并柔化遮罩边缘。\n"
                   "被遮住的区域会减少 NR 修改，保留进入 NR 前的画面。\n"
                   "这是图像估算，未使用游戏专用技能遮罩，也不做跨帧跟踪；灯光、反射也可能被选中。\n"
                   "兼容 NR → SR 和 SR → NR；默认关闭。");
        ImGui::BeginDisabled(!effectProtection);
        float effectStrength = config->DlssNrEffectStrength.value_or_default();
        if (ImGui::SliderFloat("保护强度###NR Effect Strength", &effectStrength, 0.0f, 1.0f, "%.2f",
                               ImGuiSliderFlags_AlwaysClamp))
            config->DlssNrEffectStrength = effectStrength;
        HelpMarker("0 不改变 NR 结果，1 完全保护遮罩覆盖最强的区域。Ctrl + 单击可输入数值。");
        float effectSensitivity = config->DlssNrEffectSensitivity.value_or_default();
        if (ImGui::SliderFloat("检测灵敏度###NR Effect Sensitivity", &effectSensitivity, 0.0f, 1.0f, "%.2f",
                               ImGuiSliderFlags_AlwaysClamp))
            config->DlssNrEffectSensitivity = effectSensitivity;
        HelpMarker("越高越容易选中较暗的光效，也越容易误选衣服、天空和反射。\n"
                   "先打开遮罩预览，从 0.50 开始调整；HDR 输入的白点设置会影响检测。");
        bool effectPreview = config->DlssNrEffectPreview.value_or_default();
        if (ImGui::Checkbox("显示特效遮罩###NR Effect Preview", &effectPreview))
        {
            config->DlssNrEffectPreview = effectPreview;
            if (effectPreview)
            {
                config->DlssNrAdjustmentPreview = 0u;
                config->DlssNrDebugView = 0u;
            }
        }
        HelpMarker("在原图上用红色显示检测遮罩；越红，检测覆盖越强。预览不乘保护强度。\n"
                   "需要启用 NR 模型合成，并关闭其他诊断视图。NR → SR 时预览也会经过 SR。\n"
                   "调整完后关闭预览以查看正常画面。");
        ImGui::EndDisabled();

        ImGui::SeparatorText("调整范围预览");
        int adjustmentPreview = (int) std::min(config->DlssNrAdjustmentPreview.value_or_default(), 4u);
        static const char* previewNames[] = { "关闭", "模型总变化（非单项遮罩）", "细节亮度变化", "色彩混合影响", "亮暗保护生效区域" };
        if (ImGui::Combo("红色范围###NR Adjustment Preview", &adjustmentPreview, previewNames, IM_ARRAYSIZE(previewNames)))
        {
            config->DlssNrAdjustmentPreview = (uint32_t) adjustmentPreview;
            if (adjustmentPreview != 0)
            {
                config->DlssNrEffectPreview = false;
                config->DlssNrDebugView = 0u;
            }
        }
        HelpMarker("红色越浓，所选环节造成的像素变化越大。一次显示一种，不会改变参数本身。\n"
                   "总变化是最终 NR 输出与输入的差异，包含所有模型设置的共同作用；不是独立皮肤或结构遮罩。\n"
                   "细节预览显示合成环节的亮度变化；色彩预览比较色彩强度为 0 的结果；亮暗保护预览比较未限幅结果。\n"
                   "预览需启用“应用模型效果”。纯替换模式不使用细节/色彩混合及亮暗保护，这三种预览没有红色。\n"
                   "NR → SR 时红色预览也会经过 SR。");
        if (adjustmentPreview != 0)
        {
            float previewGain = config->DlssNrAdjustmentPreviewGain.value_or_default();
            if (ImGui::SliderFloat("预览放大###NR Adjustment Gain", &previewGain, 1.0f, 32.0f, "%.1fx",
                                   ImGuiSliderFlags_AlwaysClamp))
                config->DlssNrAdjustmentPreviewGain = previewGain;
            if (!config->DlssNrApplyModel.value_or_default())
                ImGui::TextDisabled("请先开启“应用模型效果”以显示红色预览。");
            if (adjustmentPreview > 1 && (config->DlssNrReversibleMode.value_or_default() == 2u ||
                                          config->DlssNrReversibleMode.value_or_default() == 4u))
                ImGui::TextDisabled("纯替换模式跳过所选合成环节，因此没有红色范围。");
        }

        ImGui::SeparatorText("合成强度");

        int layerCount = (int) std::clamp(config->DlssNrPasses.value_or_default(), 1u, 6u);
        if (ImGui::SliderInt("NR 计算层数###NR evaluation layers", &layerCount, 1, 6, "%d 层",
                             ImGuiSliderFlags_AlwaysClamp))
            config->DlssNrPasses = (uint32_t) layerCount;
        HelpMarker("每层都会完整运行一次 NR 模型，并把上一层输出作为下一层输入。\n"
                   "所有层复用第 1 层的预设、风格和强度参数，没有独立衰减。\n"
                   "GPU 开销与显存占用会随层数显著增加；建议先从 2 层测试。");

        float transfer = config->DlssNrTransferStrength.value_or_default();
        if (ImGui::SliderFloat("细节混合强度###Detail strength", &transfer, 0.0f, 2.0f, "%.2f"))
            config->DlssNrTransferStrength = transfer;

        HelpMarker("How far the frame moves toward the model's picture."
                       "\n\nThe model's answer is not added to the frame -- it is a complete picture of its"
                       "\nown, rescaled so its luminance sits where the original says it should. This"
                       "\nblends between the two, so both ends are real pictures and everything between"
                       "\nthem is one too."
                       "\n\n0 gives back exactly what the upscaler produced. 1 is the model's picture."
                       "\n\nAbove 1 carries on past it in the same direction, which is not something the"
                       "\nmodel asked for -- use it to see what it is doing, then come back down. This"
                       "\nis the control to push if you want more effect: Intensity belongs to the model"
                       "\nand it decides what to do with it.");

        AdjustmentPreviewToggle(config, "NRDetailPreview", "显示细节亮度变化（红色）", 2u);

        float colour = config->DlssNrColourStrength.value_or_default();
        if (ImGui::SliderFloat("色彩混合强度###Colour strength", &colour, 0.0f, 4.0f, "%.2f"))
            config->DlssNrColourStrength = colour;

        HelpMarker("Whether the model's colour arrives with its light."
                       "\n\n0 keeps the game's own hue exactly -- every pixel is the original colour with"
                       "\nonly its brightness carrying the model's verdict. Game-accurate colour, with"
                       "\nthe detail. 1 brings the model's colour as well, in its own hue, clamped into"
                       "\nAP1 so nothing unreachable is asked for."
                       "\n\nThis cannot shift hue on its own: it interpolates between two finished"
                       "\npictures rather than adding a colour difference to one, which is what used to"
                       "\nlet a warm subject come back green."
                       "\n\nAbove 1 it OVER-SATURATES: the colour keeps its hue but grows more vivid,"
                       "\nand rolls off at the edge of what the display can show rather than clipping"
                       "\ninto a flat blown patch. 1 is the model's own colour; push past it for punch.");

        AdjustmentPreviewToggle(config, "NRColourPreview", "显示色彩混合影响（红色）", 3u);

        // Experimental. 0 off (soft knee), 1 Neutwo + our composition, 2 Neutwo + pure-inverse replace,
        // 3 hybrid+composed, 4 hybrid+replace (identity midtones + unclipped highlights). Always shown.
        static const char* reversibleNames[] = { "关闭（柔和高光压缩）", "Neutwo 代理 + 合成",
                                                 "Neutwo 代理 + 替换", "混合代理 + 合成",
                                                 "混合代理 + 替换" };
        int reversible = (int) config->DlssNrReversibleMode.value_or_default();
        if (reversible < 0 || reversible > 4)
            reversible = 0;
        if (ImGui::Combo("可逆代理（实验性）###Reversible proxy (experimental)", &reversible, reversibleNames,
                         IM_ARRAYSIZE(reversibleNames)))
            config->DlssNrReversibleMode = (uint32_t) reversible;

        HelpMarker("What the model is shown, and how its answer comes back."
                       "\n\nOff (soft knee): the default. It rolls highlights off so hard the model"
                       "\ncannot resolve detail in them -- fine in soft-lit scenes, weak in bright ones."
                       "\n\nNeutwo composed: an unclipped curve so the model sees highlight detail, then"
                       "\neverything above (Detail/Colour strength, highlight guard, palette). It wins in"
                       "\nbright scenes, but the curve compresses MIDTONES too, so in soft-lit content it"
                       "\ncan be worse than Off. It also shifts paper white -- re-check it when you switch."
                       "\n\nHybrid composed: the best of both, and the one to use. Identity in the"
                       "\nmidtones -- as good as Off there -- and the unclipped roll only in the"
                       "\nhighlights, so it recovers the detail Off crushes without giving up the"
                       "\nmidtones Neutwo does. It barely shifts paper white."
                       "\n\nReplace: the raw model straight back through the exact inverse, none of the"
                       "\ncomposition -- no guard, no palette, no strengths. Gorgeous where there are no"
                       "\nbright lights, but they FLASH in motion. A reference, not a daily setting."
                       "\n\nHybrid replace: the raw model like Replace, but on the hybrid curve -- the"
                       "\ndecode is identity in the midtones, so the flashing is confined to genuine"
                       "\nbright highlights instead of everywhere. Most of Replace's detail, far more"
                       "\nstable. If you love the Replace look but the flicker bothers you, use this."
                       "\n\nOff is byte-identical to before.");

        ImGui::SeparatorText("模型高级设置");

        ImGui::TextUnformatted("以下参数在模型创建时读取；修改后会短暂重建模型。");
        AdjustmentPreviewToggle(config, "NRModelPreview", "显示模型总变化（红色，非单项遮罩）", 1u);
        HelpMarker("用于观察下方预设、风格、内部强度、局部结构、局部色调及皮肤设置的共同结果。\n"
                   "NR 模型没有提供这些参数各自的独立遮罩；此处显示最终输出对原图的总变化。\n"
                   "开启后一次调整一个参数，观察变化；红色不代表模型识别出的皮肤区域。");

        static const char* nrStyleNames[] = { "默认（标准）", "自然", "电影感" };
        int style = (int) config->DlssNrStyle.value_or_default();

        if (style > 2)
            style = 2;

        if (ImGui::Combo("处理风格###Style", &style, nrStyleNames, IM_ARRAYSIZE(nrStyleNames)))
            config->DlssNrStyle = (uint32_t) style;

        HelpMarker("The model's own processing profiles."
                   "\n\nDefault (standard): the strongest. Boosts local contrast and deepens"
                   "\nlighting, and can oversaturate or look stylised -- most of what reads as"
                   "\n'the model changed my game's look' is this profile."
                   "\n\nNatural: the same detail work with a gentler hand. Keeps skin tones and"
                   "\ntonal balance closer to what the game rendered."
                   "\n\nCinematic: tones down the shine and over-processing for a film-like look."
                   "\n\nRead when the model is built, so a change rebuilds it after a moment. The"
                   "\nnames come from community testing; NVIDIA ships no names in the binaries.");

        DeferredSlider("模型内部强度###Intensity", &config->DlssNrIntensity, 0.0f, 2.0f);

        HelpMarker("The model's own strength control, applied inside it. Distinct from detail"
                       "\nstrength above, which scales the result afterwards.");

        DeferredSlider("局部结构###Local structure", &config->DlssNrLocalStructure, 0.0f, 2.0f);

        DeferredSlider("局部色调###Local tone", &config->DlssNrLocalTone, 0.0f, 2.0f);


        DeferredSlider("皮肤结构###Skin structure", &config->DlssNrSkinStructure, -1.0f, 2.0f);

        HelpMarker("-1 means follow local structure, and is the model's own default -- it is not a"
                       "\nstrength of zero. 0 and above set skin independently of the rest of the frame.");

        bool autoMask = config->DlssNrAutoMask.value_or_default();
        if (ImGui::Checkbox("自动皮肤遮罩###Auto skin mask", &autoMask))
            config->DlssNrAutoMask = autoMask;

        HelpMarker("Lets the model find skin itself rather than treating the frame uniformly.");

        ImGui::SeparatorText("曝光与色彩");

        ImGui::TextDisabled("模型按完成色调映射的 sRGB 画面训练；超分输出通常是开放范围线性画面。\n"
                            "下列设置决定如何把它映射为模型可识别的输入。若游戏报告画面已经完成\n"
                            "色调映射，则会直接透传，不再重复转换。");

        // R8.3: restore the 0.1.2 exposure UI and behaviour. The newer source selector, live
        // exposure readout, scan diagnostics and multi-point anchors are intentionally hidden.
        {
            ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f),
                               "R10 延续 0.1.2 曝光路径：仅使用固定白点。");

            float wpScale = config->DlssNrWhitePointScale.value_or_default();
            if (ImGui::SliderFloat("白点###Paper white", &wpScale, 0.25f, 4.0f, "%.2fx"))
                config->DlssNrWhitePointScale = wpScale;

            HelpMarker("模型处理前用于缩放线性画面的固定白点。R10 不读取游戏曝光纹理，"
                       "\n也不运行曝光扫描或多点锚定；行为与 0.1.2 相同。"
                       "\n\n高于 1 会让送入模型的画面更暗，低于 1 则相反。"
                       "\n模型强度为 0 时，该设置不会改变最终画面。");

            float maxRatio = config->DlssNrMaxRatio.value_or_default();
            if (ImGui::SliderFloat("亮暗变化保护###Highlight guard", &maxRatio, 1.0f, 8.0f, "%.1fx"))
                config->DlssNrMaxRatio = maxRatio;

            HelpMarker("限制模型对单个像素造成的最大亮暗变化。2x 通常能保留细节，"
                       "\n同时抑制灯光区域的彩色伪影；仅在高亮被压得过重时提高。");
            AdjustmentPreviewToggle(config, "NRGuardPreview", "显示亮暗保护生效区域（红色）", 4u);
        }

        // Retained only so old INI fields remain source-compatible. This block is not executed in
        // R8.3 and can be deleted once the 0.2.0 exposure experiment is retired completely.
        if (false)
        {
        // Logarithmic, because the useful range is not linear. A quarter to 240: the low end because
        // a frame the game already tone mapped wants roughly 1, the high end because there is no
        // principled ceiling -- this is a divisor on an open-ended linear buffer, and how far up a
        // given game needs to go is a property of that game's exposure, not of anything we can bound.
        // One tester was still improving at 100. A linear slider over that span would spend nine
        // tenths of its travel on values nobody needs and never reach the ones they do.
        // One dropdown, because there is one answer.
        //
        // This was two checkboxes that could both be on, and every attempt to stop that was a patch
        // on a shape that should not have existed. Greying deadlocked -- each disabled the other, so
        // once both were set the only way out was a button the notice never mentioned. Clearing
        // worked but silently undid a setting somebody had made. Both were ways to stop an illegal
        // state being REACHED; a single choice cannot reach it, because there is only one value to
        // be in.
        //
        // Each option also says whether it can actually do anything in THIS game, in colour, so the
        // choice is made on what is available rather than on what sounds best.
        {
            const auto ex = DlssNr::GameExposureStatus();
            const bool vk = DlssNr::IsRunningVk();
            const bool haveExposure = vk ? DlssNr::ExposureOfferedVk() : ex.everOffered;

            const float anchorNow = DlssNr::ExposureScan::BestValue();
            const bool haveAnchor = !DlssNr::ExposureScan::Anchors().empty();

            static const char* sourceNames[] = { "仅手动白点", "游戏提供的曝光",
                                                 "扫描找到的曝光缓冲" };

            int source = (int) config->DlssNrWhitePointSource.value_or_default();

            if (source < 0 || source > 2)
                source = 0;

            if (ImGui::Combo("白点来源###White point from", &source, sourceNames, IM_ARRAYSIZE(sourceNames)))
            {
                config->DlssNrWhitePointSource = (uint32_t) source;

                // Nothing else to set. The scan asks the source whether it is wanted, so choosing
                // it here is the whole of switching it on -- there is no second flag to keep in
                // step, and so no way for the two to disagree.
            }

            HelpMarker("Where the number that divides the frame comes from."
                           "\n\nPaper white only -- the slider below and nothing else. Right for a"
                           "\ngame whose exposure never moves, wrong the moment it does: one"
                           "\nconstant cannot serve a cave and a field."
                           "\n\nThe game's own exposure -- read from the texture the game hands"
                           "\nthe upscaler. The best source there is, because it is decided"
                           "\nupstream and nothing this pass does can move it. Not every game"
                           "\nsupplies one."
                           "\n\nA buffer the scan found -- for games that compute an exposure and"
                           "\nnever pass it on. A GUESS: candidates are matched by shape, and in"
                           "\nGTA V the best one tracks the real exposure but at its own scale,"
                           "\nwhich the anchor's ratio cancels. Needs anchoring once, and checking"
                           "\nafterwards.");

            // Availability, in colour, for the option currently chosen.
            if (source == 1)
            {
                if (!vk && ex.seenFrames == 0)
                    ImGui::TextDisabled("等待有效画面……");
                else if (!haveExposure)
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.25f, 1.0f),
                                       "游戏没有提供曝光纹理，当前改用手动白点；可尝试曝光扫描。");
                else if (vk)
                    ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f),
                                       "已检测到游戏曝光纹理，正在读取。");
                else if (ex.exposure > 1e-6f)
                {
                    const float trim =
                        std::clamp(config->DlssNrWhitePointTrim.value_or_default(), 0.25f, 4.0f);
                    ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f),
                                       "游戏曝光 %.4f  →  白点 %.2f%s", ex.exposure,
                                       ex.preExposure / ex.exposure * trim,
                                       ex.offeredNow ? "" : "（本帧缺失，沿用上次值）");
                }
                else
                    ImGui::TextDisabled("正在读取曝光……");
            }
            else if (source == 2)
            {
                // "Nothing found" and "found several, none of them moving" are different states,
                // and this said the first for both. In GTA V the log carried eight candidates while
                // the panel claimed there were none, which reads as the scan being broken when what
                // it actually needs is for the light to change.
                if (anchorNow <= 0.0f)
                {
                    const unsigned int watching = (unsigned int) DlssNr::ExposureScan::Report().size();

                    if (watching == 0)
                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.25f, 1.0f),
                                           "尚未找到形状符合曝光数据的缓冲区。");
                    else
                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.25f, 1.0f),
                                           "正在观察 %u 个候选，但都未变化；请在明暗环境之间移动。",
                                           watching);
                }
                else if (!haveAnchor)
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.25f, 1.0f),
                                       "已找到候选。请把下方白点调到画面正常，再添加锚点。");
                // Once anchored, the scan -> white point readout sits above the sliders below; it is
                // not repeated up here.
            }
            else if (haveExposure)
            {
                ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f),
                                   "游戏提供了曝光纹理；可在上方切换为使用它。");
            }
        }






        // A measured suggestion for paper white used to sit here and has been withdrawn.
        //
        // It took the 90th percentile of per-tile peak luminance from the untouched frame, which is a
        // statement about scene content rather than about the buffer's scale. In Nioh 3, where the
        // right answer is about 240, it offered 8 -- because most tiles are shadow and the percentile
        // sits wherever most tiles are. The guard meant to catch that compared each tile against the
        // frame's own brightest, which is scale-free and therefore passes on a black screen: the same
        // relative-threshold mistake the white point meter was removed for, made a second time.
        //
        // A wrong number offered confidently is worse than no number, so nothing is offered. What
        // replaces it has to be a measurement of the game's own exposure rather than of its scenery:
        // the exposure texture where a game supplies one, and otherwise the ratio between the
        // scene-referred buffer and the finished frame, which is that exposure by definition.

        // Two controls, not one control with two meanings.
        //
        // These are different quantities. The manual path wants an absolute divisor on an open-ended
        // linear buffer -- Nioh 3 needs about 240 -- and the exposure path wants a multiplier on a
        // number the game already supplied, where 1 is correct and anything far from it says the read
        // is wrong rather than that somebody prefers it.
        //
        // They used to share one stored value, narrowed to 0.25..4 when the toggle was on. That kept
        // a ruinous value unreachable but left two worse problems: moving the slider in one mode
        // silently destroyed the number found in the other, and there was no way back to "just take
        // the game's answer" short of knowing that the number for it was 1. Separate values fix both.
        // Switching modes is now non-destructive in both directions.
        // The trim belongs to both automatic sources, since both end in "the game's number times a
        // little". Only the manual source gets the absolute slider.
        // One slider per source, each remembering its own number.
        //
        // A trim on the game's exposure and a trim on a buffer the scan found are trims on different
        // things, and a value found against one means nothing against the other. Sharing them meant
        // changing source silently carried a number across, so a picture that had been tuned came
        // back wrong for a reason nothing on screen explained.
        //
        // The scan before it is anchored is the exception, and it has to be: anchoring captures an
        // absolute white point, so there must be an absolute slider to set. Showing a trim there
        // asked people to "set paper white below" next to a control that was not paper white.
        const int wpSource = (int) config->DlssNrWhitePointSource.value_or_default();

        // Which anchor row the paper-white slider edits, or -1 for the live unanchored point. Menu-
        // local and not persisted; the anchor block below sets it when a row is clicked. Declared
        // here because both the slider (this block) and the table (below) read it in the same frame.
        static int selectedAnchor = -1;
        auto anchors = DlssNr::ExposureScan::Anchors();
        if (selectedAnchor >= (int) anchors.size())
            selectedAnchor = -1;

        if (wpSource == 2)
        {
            const bool editingRow = selectedAnchor >= 0 && selectedAnchor < (int) anchors.size();

            // The single scan -> white point readout, above the sliders it explains.
            if (!anchors.empty())
            {
                const float liveScan = DlssNr::ExposureScan::BestValue();

                if (liveScan > 0.0f)
                {
                    const float w = DlssNr::ExposureScan::AnchoredWhitePoint(
                        liveScan, config->DlssNrScanInverted.value_or_default(),
                        config->DlssNrScanTrim.value_or_default());

                    ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f),
                                       "扫描值 %.5f  →  白点 %.2f（%u 个锚点）", liveScan, w,
                                       (unsigned) anchors.size());
                }
            }

            // Paper white shows only when there is a point to set: before the first anchor, or when a
            // row is selected to edit. Once points exist and none is selected, the white point is fixed
            // by the anchors and only the trim adjusts the live picture -- so the trim takes the
            // slider's place, the same shape as the game-exposure source.
            const bool showPaperWhite = anchors.empty() || editingRow;

            if (showPaperWhite)
            {
                float pw = editingRow ? anchors[selectedAnchor].white
                                      : config->DlssNrWhitePointScale.value_or_default();

                char lbl[48];
                if (editingRow)
                    snprintf(lbl, sizeof(lbl), "手动白点（编辑锚点 %d）###Paper white edit", selectedAnchor + 1);
                else
                    snprintf(lbl, sizeof(lbl), "手动白点###Paper white");

                if (ImGui::SliderFloat(lbl, &pw, 0.25f, 2000.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
                {
                    if (editingRow)
                    {
                        DlssNr::ExposureScan::AnchorSetWhite(selectedAnchor, pw);
                        config->DlssNrScanAnchors = DlssNr::ExposureScan::SerializeAnchors();
                    }
                    else
                        config->DlssNrWhitePointScale = pw;
                }

                HelpMarker("The white point for the selected calibration point, or -- with no row"
                               "\nselected -- the value the next Anchor press captures."
                               "\n\nSet it until the picture looks right here, then Anchor. Move to very"
                               "\ndifferent light and do it again: two points fix the buffer's real"
                               "\nrelationship and the white point holds between them. Click a row below"
                               "\nto come back and adjust that point; click it again to let go.");
            }

            // The trim multiplies the interpolated result, and in the steady state it is the control
            // that stands in for paper white: adjust it until the picture looks right in the current
            // light, then Anchor bakes that trimmed value into a new point and resets the trim to 1.
            if (!anchors.empty())
            {
                float trim = config->DlssNrScanTrim.value_or_default();

                if (ImGui::SliderFloat("扫描结果微调###Trim (x the scan)", &trim, 0.25f, 4.0f, "%.2fx",
                                       ImGuiSliderFlags_Logarithmic))
                    config->DlssNrScanTrim = std::clamp(trim, 0.25f, 4.0f);

                HelpMarker("A multiplier on the scan's white point, and the control you adjust between"
                               "\nanchor points: dial it until the picture looks right in the current"
                               "\nlight, then press Anchor here -- it captures the trimmed value as a new"
                               "\npoint and resets the trim to 1.");
            }
        }
        else if (wpSource == 1)
        {
            const bool ofScan = false;

            float trim = ofScan ? config->DlssNrScanTrim.value_or_default()
                                : config->DlssNrWhitePointTrim.value_or_default();

            if (ImGui::SliderFloat(ofScan ? "扫描结果微调###Trim (x the scan)" : "游戏曝光微调###Trim (x the game's exposure)", &trim,
                                   0.25f, 4.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
            {
                if (ofScan)
                    config->DlssNrScanTrim = std::clamp(trim, 0.25f, 4.0f);
                else
                    config->DlssNrWhitePointTrim = std::clamp(trim, 0.25f, 4.0f);
            }

            HelpMarker("A multiplier on the exposure the game supplied. 1.00x takes its number"
                           "\nexactly, and that is the right answer here."
                           "\n\nThis is not a fudge factor. If a game needs the trim far from 1 to look"
                           "\nright, that is evidence the exposure being read is wrong for that game,"
                           "\nnot that the game wants trimming. Somewhere around 0.8 to 1.25 is honest"
                           "\ntuning; reaching for 4 means something upstream is broken and the trim is"
                           "\nhiding it."
                           "\n\nYour manual paper white is kept separately and comes back untouched if"
                           "\nyou switch the option above off.");
        }
        else
        {
            // Logarithmic, because the useful range is not linear. A quarter to 2000: the low end
            // because a frame the game already tone mapped wants roughly 1, the high end because
            // there is no principled ceiling -- this is a divisor on an open-ended linear buffer, and
            // how far up a given game needs to go is a property of that game's exposure rather than
            // of anything that can be bounded here. One tester was still improving at 100.
            float wpScale = config->DlssNrWhitePointScale.value_or_default();

            if (ImGui::SliderFloat("手动白点###Paper white", &wpScale, 0.25f, 2000.0f, "%.2fx",
                                   ImGuiSliderFlags_Logarithmic))
                config->DlssNrWhitePointScale = wpScale;

        HelpMarker("What the frame is divided by before the model sees it. There is no other white"
                       "\npoint; this is the whole of it."
                       "\n\nThe model was trained on finished frames where white sits at 1. The"
                       "\nupscaler's output is linear and open-ended, so something has to say where"
                       "\nwhite is -- and where the game's DLSS buffer is linear HDR, that number is"
                       "\nrarely anywhere near 1. Measured in Monster Hunter Wilds it takes 16 or more"
                       "\nbefore the model's detail reaches the frame at all, and the value that suits"
                       "\na shaded camp is still too small for the same game out in daylight."
                       "\n\nToo low and almost every pixel trips the soft knee: the model is shown a"
                       "\nflat near-white picture, its answer is scaled away, and only its hue"
                       "\nsurvives -- which reads as a colour cast rather than as lost detail. Too"
                       "\nhigh and it is shown an underexposed one, its answer degrades, and this same"
                       "\nnumber multiplies that error on the way out."
                       "\n\nRaise it until the picture stops improving. Past that point it does not"
                       "\nplateau, it gets worse in the other direction."
                       "\n\nThis was once a multiplier on a measured white point. The measurement is"
                       "\ngone: it read scene brightness rather than where white belongs, handed the"
                       "\nmodel a picture three times too dark, and left the highlight path nothing to"
                       "\ngive back."
                       "\n\nAt strength zero the frame is still bit-identical whatever this says.");
        }

        // Highlight guard, directly under the white point / trim -- it bounds the model's edit and
        // belongs with the exposure controls it works alongside.
        float maxRatio = config->DlssNrMaxRatio.value_or_default();
        if (ImGui::SliderFloat("亮暗变化保护###Highlight guard", &maxRatio, 1.0f, 8.0f, "%.1fx"))
            config->DlssNrMaxRatio = maxRatio;

        HelpMarker("The most the pass may move any pixel, as a multiple of what it already was, in"
                       "\nboth directions -- a pixel may not be brightened past this nor darkened past"
                       "\nits reciprocal. Lights are where the model has least to say and rescaling its"
                       "\nanswer does the most damage; 2x leaves detail intact while stopping a strip"
                       "\nlight turning into a string of coloured cells. Raise it only if bright areas"
                       "\nlook clipped.");
        AdjustmentPreviewToggle(config, "NRGuardPreview", "显示亮暗保护生效区域（红色）", 4u);

        // Directly under the white point, because that is the number it moves and the number the
        // anchor captures. It used to sit under Inspect, a whole section away from the slider it
        // reads, which left "在此添加锚点###Anchor here" looking like a control for something else entirely.
        {
            // No checkbox here any more.
            //
            // The dropdown above says whether the scan is the white point's source, and that is
            // the only reason anybody using this would want it running. A second control could
            // only agree with the dropdown or contradict it, and both were on offer: it began as
            // a redundant question and became a way to switch off the thing the chosen source
            // depended on.
            //
            // The ini key survives as a developer override for the one case a user has no reason
            // to want -- running the scan in a game that supplies a REAL exposure, so the log can
            // compare the two. That is validation, and validation does not need a widget.
            //
            // Worth keeping written down, since the panel no longer says it: the scan matches
            // buffers by SHAPE, and shape is a weak filter. In GTA V -- a game that supplies a
            // real exposure, so the right answer sat visible beside it -- the best candidate was
            // a 1x1 R32_FLOAT that climbed in a straight line for seventeen minutes while the
            // true exposure held still. Their ratio moved 14x. That is an accumulator, not an
            // eye adaptation.

                // Only where it means something. The lamp reads the scan, so offering it beside a
                // white point that comes from the game's own exposure is offering a control that
                // cannot light up.
                if (config->DlssNrWhitePointSource.value_or_default() == 2)
                {
                    bool meter = config->DlssNrScanMeter.value_or_default();
                    if (ImGui::Checkbox("屏幕显示曝光指示灯###Show the light meter on screen", &meter))
                        config->DlssNrScanMeter = meter;

                    HelpMarker("A lamp in the corner: red for dark, green for full light, and the"
                               "\nshades between, with the reading beside it."
                               "\n\nIt is how you see at a glance that the scan is TRACKING rather"
                               "\nthan merely running. Walk into shade and it should slide toward"
                               "\nred; step out and it should go green. If it moves the wrong way,"
                               "\nthat is what the setting above is for."
                               "\n\nPurely a readout. It changes nothing.");
                }

            // Shown when the scan is actually running, whichever way it got switched on.
            if (DlssNr::ExposureScan::Scanning())
            {
                // Anchoring: one press, then it never needs touching again.
                //
                // The absolute white point cannot come out of a buffer whose units are unknown.
                // Every value AFTER the first can: only the ratio against the anchor is used, so
                // whatever the number means, it cancels. That is why this is a button and not a
                // measurement -- the one thing a person can supply that no amount of cleverness
                // can is "this looks right to me".
                int which = 0;
                float low = 0.0f, high = 0.0f;
                const float live = DlssNr::ExposureScan::BestValue(&which, &low, &high);

                const bool isSource = config->DlssNrWhitePointSource.value_or_default() == 2;

                // Anchor captures (currentScan, currentPaperWhite) and ADDS a row -- it does not
                // replace. One row is the old single-anchor ratio law; add a second in different
                // light and the white point is interpolated between the points, so it holds across
                // the whole range instead of only near one anchor. Greyed unless the scan is the
                // chosen source and it currently has a value to capture.
                ImGui::BeginDisabled(live <= 0.0f || !isSource);

                if (ImGui::Button("在此添加锚点###Anchor here"))
                {
                    // What to capture. Before the first point, the paper white above (an absolute value
                    // with the wide range a fresh game needs). After that, the EFFECTIVE white point the
                    // picture is showing right now -- the interpolated value times the Trim the user just
                    // dialed in -- so a second point in different light captures the trimmed look, not a
                    // frozen paper white (which would make two equal whites and a flat, non-tracking
                    // curve). The trim is reset afterwards: the new point, which the picture now passes
                    // through exactly, must not be multiplied by it a second time.
                    const float captureWhite =
                        anchors.empty()
                            ? std::max(0.01f, config->DlssNrWhitePointScale.value_or_default())
                            : std::max(0.01f, DlssNr::ExposureScan::AnchoredWhitePoint(
                                                  live, config->DlssNrScanInverted.value_or_default(),
                                                  config->DlssNrScanTrim.value_or_default()));

                    if (DlssNr::ExposureScan::AnchorAdd(live, captureWhite))
                    {
                        config->DlssNrScanAnchors = DlssNr::ExposureScan::SerializeAnchors();
                        config->DlssNrScanTrim = 1.0f;
                        selectedAnchor = -1;
                    }
                }

                ImGui::EndDisabled();

                HelpMarker("Make the picture look right, then press this -- it captures the current look"
                               "\nas a point. For the first point use the Paper white slider above; for"
                               "\nevery point after, move to different light and use the Trim, which the"
                               "\nAnchor then bakes into a new point."
                               "\n\nThe first press calibrates one point -- the white point then"
                               "\nfollows the scan by ratio from there, as before. Walk into very"
                               "\ndifferent light, set paper white again, and press it again: the"
                               "\nsecond point pins down the buffer's real curve and everything"
                               "\nbetween the two is right, not just near one anchor. Up to eight."
                               "\n\nThe table is per game and shareable: one person calibrates a game"
                               "\nand the numbers are the same for everyone who takes the profile.");

                if (!isSource)
                    ImGui::TextDisabled("（扫描器当前只观察；上方白点来自其他来源）");

                if (!anchors.empty())
                {
                    // The row nearest the live scan value (in log space) is the one driving the
                    // picture right now; mark it so the user can see which calibration is in effect.
                    int active = 0;
                    float bestDist = 1e30f;
                    const float liveLog = std::log(std::max(live, 1e-6f));

                    for (size_t i = 0; i < anchors.size(); ++i)
                    {
                        const float d =
                            std::fabs(std::log(std::max(anchors[i].scan, 1e-6f)) - liveLog);
                        if (d < bestDist)
                        {
                            bestDist = d;
                            active = (int) i;
                        }
                    }

                    for (size_t i = 0; i < anchors.size(); ++i)
                    {
                        ImGui::PushID((int) i);

                        // Delete first, so its click is never swallowed by the row-wide Selectable.
                        if (ImGui::SmallButton("x"))
                        {
                            DlssNr::ExposureScan::AnchorRemove((int) i);
                            config->DlssNrScanAnchors = DlssNr::ExposureScan::SerializeAnchors();
                            if (selectedAnchor == (int) i)
                                selectedAnchor = -1;
                            else if (selectedAnchor > (int) i)
                                --selectedAnchor;
                            ImGui::PopID();
                            continue;
                        }

                        ImGui::SameLine();

                        const bool sel = (int) i == selectedAnchor;
                        char row[96];
                        snprintf(row, sizeof(row), "%s 扫描 %.4f  →  白点 %.2f%s",
                                 ((int) i == active && isSource) ? ">" : "  ", anchors[i].scan,
                                 anchors[i].white, sel ? "（编辑中）" : "");

                        // Click selects the row (slider edits it); click again deselects (slider
                        // returns to the live unanchored point).
                        if (ImGui::Selectable(row, sel))
                            selectedAnchor = sel ? -1 : (int) i;

                        ImGui::PopID();
                    }

                    ImGui::TextDisabled("单击一行可用上方滑块编辑，再次单击则取消选择；> 表示当前使用的点。");
                }

                // The direction flag only means anything with a single point; with two or more the
                // direction the white point moves is already fixed by the data.
                if (anchors.size() == 1)
                {
                    bool inverted = config->DlssNrScanInverted.value_or_default();
                    if (ImGui::Checkbox("数值方向相反###The number runs the other way", &inverted))
                        config->DlssNrScanInverted = inverted;

                    HelpMarker("Flip this if the picture gets worse in the direction it should be"
                                   "\ngetting better. Most engines store an exposure that falls as"
                                   "\nthe scene brightens; some store its reciprocal, and a buffer"
                                   "\nfound by shape does not say which. Add a second anchor point in"
                                   "\ndifferent light and this is decided for you, so it disappears.");
                }

                // The scan -> white point readout is shown above the sliders now, not here.

                // Everything below is read-out rather than control: what the scan is looking at and
                // how to tell whether it found the right thing. Folded away because the two decisions
                // that matter -- anchor, and which way the number runs -- are above it.
                if (ImGui::TreeNode("高级诊断###Advanced"))
                {

                    const auto found = DlssNr::ExposureScan::Report();
                    const char* why = DlssNr::ExposureScan::Status();

                    if (found.empty())
                    {
                        ImGui::TextDisabled("%s", why != nullptr && why[0] != 0
                                                      ? why
                                                      : "尚未匹配到候选。");
                    }
                    else
                    {
                        for (size_t i = 0; i < found.size(); ++i)
                        {
                            const auto& c = found[i];

                            if (c.reads == 0)
                            {
                                ImGui::TextDisabled("%zu. %s — 尚未读取", i + 1, c.shape.c_str());
                                continue;
                            }

                            // Moving is the whole signal, so it is the thing that is coloured.
                            ImGui::TextColored(c.moves ? ImVec4(0.45f, 0.8f, 0.45f, 1.0f)
                                                       : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                               "%zu. %s = %.5f  (seen %.5f..%.5f) %s", i + 1,
                                               c.shape.c_str(), c.latest, c.lowest, c.highest,
                                               c.moves ? "有变化" : "目前不变");
                        }

                        ImGui::TextDisabled("请从阴影走到亮处：真实曝光值应随光照变化。");
                        ImGui::TextDisabled("若数值只持续单向增加，它更可能是计数器而非曝光。");
                    }

                    ImGui::TreePop();
                }
            }
        }


        }

        ImGui::SeparatorText("对比方式###Compare");

        // Freeze the frame the model works on, so a setting change re-renders it in place -- the only
        // clean way to A/B our own settings (a moving scene confounds every other comparison). See
        // design/frame-hold.md.
        bool held = config->DlssNrHoldFrame.value_or_default();
        if (ImGui::Checkbox("冻结 NR 输入帧###Hold frame", &held))
            config->DlssNrHoldFrame = held;

        HelpMarker("Freezes the frame the model works on. While held, change paper white, the"
                       "\nstrengths, the reversible mode, the model preset -- anything below the"
                       "\nupscaler -- and only that setting moves; the scene does not."
                       "\n\nWhat it CANNOT show: DLSS/FSR/XeSS upscaler presets or anything upstream"
                       "\n(the upscaler is not re-run on a held frame), and the game's own HUD and"
                       "\npost-processing, which run after this pass and keep updating. The white"
                       "\npoint stops being measured and holds its value while frozen, so it cannot"
                       "\ndrift and confound the comparison."
                       "\n\nHide the menu and it stays held. Untoggle to resume.");

        static const char* compareNames[] = { "关闭", "左右并排", "滑动分割" };
        int compare = (int) config->DlssNrCompare.value_or_default();
        if (ImGui::Combo("对比方式###Compare", &compare, compareNames, IM_ARRAYSIZE(compareNames)))
            config->DlssNrCompare = (uint32_t) compare;

        HelpMarker("Shows the pass against itself, so the two can be seen at once rather than"
                       "\ntoggled and remembered."
                       "\n\nSide by side puts the whole frame in each half, untouched on the left and"
                       "\nedited on the right. Both halves are squeezed horizontally to fit, so it is"
                       "\nfor looking at rather than playing in."
                       "\n\nWipe cuts a single frame at the split and resamples nothing, so the picture"
                       "\nis the right shape and can be played normally. Drag the split below; it is a"
                       "\nstored setting and stays put once the menu is closed."
                       "\n\nNeither needs the menu open to keep working. A hairline marks the join.");

        if (compare != 0)
        {
            bool swap = config->DlssNrCompareSwap.value_or_default();
            if (ImGui::Checkbox("交换左右###Swap sides", &swap))
                config->DlssNrCompareSwap = swap;

            bool tags = config->DlssNrCompareTags.value_or_default();
            if (ImGui::Checkbox("显示画面标签###Label the sides", &tags))
                config->DlssNrCompareTags = tags;

            HelpMarker("Writes which side is which onto the frame itself, so a screenshot still"
                           "\nsays so after it has left this machine. Drawn into the picture's own"
                           "\nplane: in the wipe the split reveals and hides the label exactly as it"
                           "\ndoes the images, and there is nothing to drag. Swap sides moves the"
                           "\nlabels with their pictures.");

            if (tags)
            {
                float tagScale = config->DlssNrTagScale.value_or_default();
                if (ImGui::SliderFloat("标签大小###Label size", &tagScale, 0.5f, 5.0f, "%.1fx"))
                    config->DlssNrTagScale = std::clamp(tagScale, 0.5f, 5.0f);
            }

            HelpMarker("Puts the edited frame on the other side."
                           "\n\nWorth doing once you have decided which you prefer: the eye is not"
                           "\neven-handed about left and right, and a difference can read as an"
                           "\nimprovement purely from where it sits. If the same side still wins after"
                           "\nswapping, it is the pass you are seeing and not the placement.");
        }

        if (compare == 1)
        {
            float zoom = config->DlssNrCompareZoom.value_or_default();
            if (ImGui::SliderFloat("对比缩放###Zoom", &zoom, 1.0f, 2.0f, "%.2f"))
                config->DlssNrCompareZoom = std::clamp(zoom, 1.0f, 2.0f);

            HelpMarker("How much of the frame each half shows."
                           "\n\nA half is half as wide as the frame and just as tall, so the frame"
                           "\ncannot fill it and keep its shape."
                           "\n\nAt 1 the whole frame is there at its right proportions, with bars above"
                           "\nand below. At 2 the half is filled and the sides are cropped away"
                           "\ninstead. Anything between trades one for the other.");
        }

        if (compare == 2)
        {
            float split = config->DlssNrCompareSplit.value_or_default();
            if (ImGui::SliderFloat("分割位置###Split", &split, 0.0f, 1.0f, "%.2f"))
                config->DlssNrCompareSplit = std::clamp(split, 0.0f, 1.0f);

            HelpMarker("Where the wipe cuts. Left of it is the frame as the upscaler produced it,"
                           "\nright of it is the frame the model edited.");
        }

        static const char* debugNames[] = { "关闭", "模型输入代理", "模型原始输出",
                                            "差异（放大）" };
        int debugView = (int) config->DlssNrDebugView.value_or_default();
        if (ImGui::Combo("调试视图###Debug view", &debugView, debugNames, IM_ARRAYSIZE(debugNames)))
        {
            config->DlssNrDebugView = (uint32_t) debugView;
            if (debugView != 0)
            {
                config->DlssNrAdjustmentPreview = 0u;
                config->DlssNrEffectPreview = false;
            }
        }

        HelpMarker("Proxy is the picture handed to the model -- if that looks wrong, the white point"
                       "\nis wrong and nothing downstream can be judged."
                       "\n\nDifference shows what the model actually changed, amplified twenty times and"
                       "\ncentred on grey. A flat grey frame there means it is doing nothing.");

        ImGui::PopItemWidth();
    }
}

} // namespace DlssNr
