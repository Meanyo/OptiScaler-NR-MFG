# OptiScaler-NR-MFG

基于 OptiScaler 的非官方增强分支：NR 神经渲染、NR↔SR 顺序切换、串行多层 NR、RTX 40 系多帧生成，以及中文设置界面。

当前源码版本：**R12.1**（`0.2.0-order-cn-r12.1` / `20261124_R12_1`）。本仓库以现有本地 R11 源码建立，未包含完整上游 Git 提交历史。详细来源见 [SOURCE_ORIGINS.md](SOURCE_ORIGINS.md)，原上游介绍保留在 [README.upstream.md](README.upstream.md)。

用户已确认 R12.1 在 FF14 和燕云中测试通过；详细测试范围见 [支持与验证记录](tools/ada_nr/VALIDATION.md)。

## 功能

- NR → SR / SR → NR 处理顺序选择。
- NR 单运行库模型切换：只加载一份 310.9.16，通过自动（0）／标准（1）／轻量（2）预设重建全部 NR 层，无需重启游戏；不再选择或切换 DLL。
- RTX 40 / Ada 的 310.9.16 原生 FP8 模型补丁与可复现构建、审计源码，见 [支持与验证记录](tools/ada_nr/VALIDATION.md)。
- 1–6 层真实串行 NR；每层复用第一层参数，各自维护模型实例。
- RTX 40（Ada）兼容 DLSS-G 运行库的 2X–6X 倍率解锁与时序中点修复。
- DLSS-G 默认 / A / B / C 模型选择，独立于超分模型和插帧倍率；修改后保存并重启游戏。
- C 预设适配已验证布局的 `nvngx_dlssg 310.9.16`；未识别的模型接口不进行猜测性修改。
- 中文 NR / 帧生成菜单、特效保护与调整预览。
- R10 引入的 D3D12 真实队列 Fence 延迟回收及模型参数合并重建。

## 使用与构建

- [R12.1 模型切换与安装说明](R12.1-使用说明.txt)
- [R11 使用说明](R11-使用说明.txt)：安装目录、模型选择、日志和已知限制。
- [构建说明](BUILDING.md)：Windows x64 Release 构建。
- [配置示例](OptiScaler.ini)：保留自己的配置，不要直接覆盖。
- [NR 特效保护](EFFECT_PROTECTION_CN.md)、[调整预览](ADJUSTMENT_PREVIEW_CN.md)。

Git 源码不包含 NVIDIA 模型或编译输出；Release 按主包、旧 NR、新版 Ada NR 三包发布。R12 主包包含非 NR 的 DLSS SR/RR/FG 等配套文件；NR 模型按需另装。运行库与模型沿用各自原厂许可；编译插件本身不会生成 NVIDIA 模型。

游戏原生 DLSSG 输入需要游戏实际提交帧生成数据；仅启用超分不等于启用了帧生成。模型选择与倍率解锁不保证某个游戏必然支持或稳定。

## 验证与已知问题

R11 已完成 x64 编译、946 个代理导出兼容性检查，以及 310.9.0 / 310.9.16 provider 的预设入口、Detours A/B/C 转发、倍率门槛和时序描述符离线验证。构建时没有 RTX 40 GPU，离线检查不能代替实际游戏测试。

目前尚未修复的已报告问题包括：

- 部分游戏运行中关闭补帧后，基础帧率未完全恢复；日志出现帧边界重复推进及 Reflex 帧号不一致，根因仍需进一步验证。
- 部分游戏照相模式 / 交换链切换后帧率恢复异常。
- 旧驱动对新运行库、动态 MFG 和模型 C 的支持情况需要分别验证。

R11 继承的是曝光回退后的固定白点处理；不要把保留的旧曝光配置键视为所有曝光功能都已启用。

## 来源与许可证

这是独立维护的衍生项目，不是 OptiScaler 或 NVIDIA 官方版本。OptiScaler 原有版权、GPL-3.0 许可证和第三方声明均保留，见 [LICENSE](LICENSE)、[Licenses](Licenses) 与各依赖目录的许可文件。

感谢 [OptiScaler](https://github.com/optiscaler/OptiScaler)、[OptiScaler_DLSSNR](https://github.com/Dagherbou/OptiScaler_DLSSNR)、[OptiScaler-Aurora](https://github.com/abc354402600/OptiScaler-Aurora)、[MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx) 和 [RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock) 的原始工作与研究。来源和适配范围以 [SOURCE_ORIGINS.md](SOURCE_ORIGINS.md) 为准。
