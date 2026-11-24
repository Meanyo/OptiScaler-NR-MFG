# R12 / R12.1 的 RTX 40 / Ada 支持记录

## 实现与来源

- R12 原为双目录 DLL 切换。R12.1 移除该实现，使用一份新版模型，通过 Preset 1/2 重建全部 NR 层；新版模型应放到插件或游戏主程序目录。
- 新版模型使用 `build_ada.py` 与 `patcher.py` 构建，底层源码来自 lmxxf/DLSS5-RTX-3080-laptop 的 `cb716c70edc6351a5ebcbf6d1ca8da0c297fad1f`；MIT 许可证随源码保留。
- 适配 NGX 最低架构要求、架构元数据和内部架构分支，为 15 个 Fatbin 加入 `sm_89` 内核，并在原槽位完成回填。
- 处理较新 bulk copy、barrier、reduction、fence、min/relu 指令；补充 acquire-only GPU fence 的 acq_rel 转换。
- 未替换权重，未强制改变模型预设，未采用 Ampere 的 FP8→FP16 软件降级。
- `audit_model.py` 检查 49,208 行原生 e4m3 PTX 保持不变，反汇编确认 19,952 条原生 FP8 QMMA 指令。
- CUDA 13.3 官方工具构建；构建回执无 O1 紧凑回退。两次独立构建输出 SHA-256 一致。

## 模型身份

| 文件 | SHA-256 |
|---|---|
| 输入 310.9.16 原模型 | `2BC19BC09A7B89DD44F73E2AC0108FCC586E2154BDD0830F242F4F040E28789F` |
| Ada 补丁输出 | `ED4B4164BDFDE6268A3C896AAD5E340CB66646D5E93E0AA4FF02583E7D684274` |
| 独立发布的旧版 310.8 模型 | `214FFC2BA081B66CE7F3EB50AEE9EAAE4D01C55B6A9B3CCA4BB66BBEAABAC6CA` |

旧模型为此前本地使用的 310.8 文件，当前 Authenticode 检查为 HashMismatch，不标作未修改的原厂文件。

模型二进制和 CUDA 工具不进入 Git 源码。修改后的新版 NR 不再保留 NVIDIA 原始数字签名。

## GPU 测试：2026-11-24

设备：RTX 4080 SUPER，驱动 617.42。通过独立 D3D12 合成输入程序 `bench_nvngx.dll.cpp` 测试。

原模型 Preset 2 初始化成功，但创建返回 `0xBAD00001 / FAIL_FeatureNotSupported`。
Ada 补丁的下列测试均成功创建、执行，输出无 NaN/Inf：

| 分辨率 | NR 预设 | 预热/计时帧 | 模型 GPU 中位耗时 |
|---|---|---|---|
| 1280×720 | 2 | 2 / 5 | 1.815 ms |
| 1280×720 | 0 | 5 / 20 | 1.821 ms |
| 1920×1080 | 2 | 10 / 30 | 2.714 ms |

这不是游戏帧率测试，也不是相同画质下与旧模型的性能比较。没有完成 FF14 内的画面、长时间稳定性、多层 NR 或帧生成组合验证；其他 RTX 40 型号亦未实测。

## R12 历史 Release 分包

- `OptiScaler-NR-MFG-R12.zip`：目录隔离版插件、常规名桥接器、运行依赖，以及非 NR 的 NVIDIA DLSS SR/RR/FG、DeepDVC、Latewarp 文件；不含 NR 模型。
- `NR-Legacy-310.8.zip`：旧模型，解压到游戏主程序目录。
- `NR-310.9.16-Ada-FP8.zip`：Ada 补丁模型，解压后位于 `OptiScaler/NR-310.9.16/nvngx_dlssnr.dll`，附独立测试与审计记录。

默认配置不自动启用 NR。保留个人 INI；按所需模型安装分包后在菜单选择。

## R12.1 单运行库切换验证（2026-11-24）

RTX 4080 SUPER / 617.42，1280×720，同一进程、模型模块、设备和能力参数块，Preset 1→2→1→0 四次创建和执行均成功，全部输出无 NaN/Inf。两次标准输出逐浮点完全相同，标准与轻量输出不同，自动与轻量输出完全相同。测试源码为 bench_nvngx.dll.cpp（preset=99），入口 test-nr-preset.ps1。D3D12/Vulkan 插件编译通过；用户已确认 R12.1 在 FF14 和燕云测试通过。未收到逐项多层/FG 组合测试记录；Vulkan 未做实卡验证。

R12.1 发布主包继续不含 NR 模型；新版 Ada NR 包使用根目录 `nvngx_dlssnr.dll` 布局，模型字节及 SHA-256 与 R12 相同。旧310.8不具备本次验证的新轻量模型路径，仅为兼容备选。
