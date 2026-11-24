# 构建说明

## 环境

- Windows x64；PowerShell 7。
- Visual Studio 2022 / Build Tools，安装“使用 C++ 的桌面开发”。本地 R11 使用 MSVC 14.44。
- Windows 10/11 SDK。本地 R11 使用已安装的 Windows SDK。

本快照的依赖头文件和构建用静态库按现有目录收录，不使用 Git submodule。不要在此快照运行上游的 submodule 更新命令来覆盖本分支的依赖头文件。

## 编译

```powershell
git clone https://github.com/Meanyo/OptiScaler-NR-MFG.git
cd OptiScaler-NR-MFG
pwsh -File ./build.ps1 -CheckDependencies
pwsh -File ./build.ps1
```

脚本会查找 Visual Studio / Build Tools 和 Windows SDK；也可指定安装根目录：

```powershell
pwsh -File ./build.ps1 -ToolchainRoot 'C:\BuildTools'
```

输出为 `build/OptiScaler.dll`。这是仅编译、链接 DLL 的脚本，不执行旧 vcxproj 中的游戏运行库复制和发布后处理。

修改头文件后请执行完整构建。`-Incremental` 只按源文件时间判断，不追踪全部头文件依赖，不能用于验证头文件修改后的完整构建。

源码已包含使用中的预编译着色器头文件。重编译着色器需要另行准备 DXC；此仓库不携带 DXC 可执行程序或 DLL。

## Provider 离线验证

自行取得要检查的原始 NVIDIA `nvngx_dlssg.dll` 后执行：

```powershell
pwsh -File ./test-provider.ps1 -Providers 'D:\runtime\nvngx_dlssg.dll'
```

该检查映射 DLL，但不初始化 CUDA / NGX / 游戏图形设备；验证预设入口、Detours 调用约定、旧多帧门槛以及时序描述符修复。它不能证明驱动兼容、实际插帧画面正确或某个游戏不会崩溃。

## 运行依赖

NR 使用的 NVIDIA 模型 DLL 和 forwarder、DLSS SR / RR / FG、Streamline、XeSS / FSR 运行库需要按所用功能另外准备。NR forwarder 工程位于 `OptiScaler/dlssnr/forwarder`，不由根目录 `build.ps1` 自动编译。不要把个人游戏目录或 NVIDIA 缓存目录提交到仓库。

原有 `package_release.ps1` 为上游历史打包脚本，默认路径和版本不适合作为本分支 R11 发布流程；发布前请按本分支说明单独整理输出。

## 源码精简范围

本快照移除了 magic_enum、simpleini、spdlog、unordered_dense 的第三方示例/测试，以及未参与主工程编译的 imgui_demo.cpp（共 177 个文件）。依赖头文件、实现、静态库、许可证及本项目 tests/ 均保留。依赖的 CMake/Meson 默认配置已相应关闭被移除的示例/测试；如需运行第三方库自身测试，请从其原始项目取得完整源码。此次精简不改变 OptiScaler 的编译输入或游戏功能，也不改动已有 DLL、NVIDIA 模型及个人配置。
