# 来源与适配范围

本仓库是本地 R11 的首次源码快照，并非从 GitHub 官方 fork 功能创建；未伪造上游历史，也未声称所有代码由本仓库作者原创。

| 项目 | 本快照中的用途 |
| --- | --- |
| [OptiScaler](https://github.com/optiscaler/OptiScaler) | 注入代理、超分后端、帧生成框架及原有界面；保留 GPL-3.0 和原作者声明 |
| [Dagherbou/OptiScaler_DLSSNR](https://github.com/Dagherbou/OptiScaler_DLSSNR) | DLSSNR 集成基础，先后基于 0.1.x / 0.2.0 源码适配 |
| [abc354402600/OptiScaler-Aurora](https://github.com/abc354402600/OptiScaler-Aurora) | RTX 40 MFG 倍率门槛与运行库接入参考 |
| [MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx) | R10 合入用户提供的 1.0 版精确指纹时序中点修复；见 `Licenses/RenoDX_ATTRIBUTION.txt` 和源文件声明 |
| [dashdogy/RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock) | R11 provider 预设读取入口、版本支持与 A/B/C 值语义参考；MIT 许可见 `OptiScaler/framegen/dlssg/LICENSE-dashdogy.txt` |
| [NVIDIA Streamline](https://github.com/NVIDIA-RTX/Streamline) | 编译 API / 头文件及接口定义；NVIDIA 原有许可保留在依赖目录 |
| [NVIDIA Profile Inspector](https://github.com/Orbmu2k/nvidiaProfileInspector) | 核对帧生成预设设置 ID / 字母数值；不在本项目中修改驱动配置数据库 |

R11 模型读取适配参考提交：`dashdogy/RTX40MFG-Unlock@2b52823e3769c31cf52d3b368c6963cff8fc5344`。未整库合并该项目或 OptiScaler 当前 master。

分支增加和调整了中文界面、NR↔SR 顺序、串行多层 NR、D3D12 队列 Fence 回收、部分模型与补帧状态显示，以及 R11 的帧生成模型选择。相关行为存在硬件和游戏限制，详见 README。

`external/` 与 `OptiScaler/library/` 保留现有构建依赖的布局、头文件和必要静态库；每项依赖仍使用其自己的许可证。当前快照无法提供所有依赖的精确历史提交号，不应把目录名或本仓库版本当作依赖来源的完整证明。

NVIDIA DLL、游戏日志、个人配置、编译缓存、测试机上的 NGX 模型缓存均不上传。历史本地测试使用过社区分发的原始 NVIDIA 运行库，验证过文件签名，但这些运行库不属于本项目源码，也未收入本仓库。
