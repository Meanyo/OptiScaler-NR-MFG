# Ada 原生 FP8 NR 模型补丁

输入仅接受原始 `nvngx_dlssnr.dll 310.9.16.0`，SHA-256：
`2bc19bc09a7b89dd44f73e2ac0108fcc586e2154bdd0830f242f4f040e28789f`。
输出为独立副本，禁止原地覆盖输入；本仓库不提交原始或修改后的 NVIDIA 模型。

`patcher.py` 来自 [lmxxf/DLSS5-RTX-3080-laptop](https://github.com/lmxxf/DLSS5-RTX-3080-laptop)，
提交 `cb716c70edc6351a5ebcbf6d1ca8da0c297fad1f`，原 MIT 许可证见 `LICENSE-upstream.txt`。
本地修改：补充 310.9.16 的 acquire-only GPU fence 到 acq_rel 的转换。
`bench_nvngx.dll.cpp` 也来自同一提交，用于独立 D3D12 合成输入测试。

## 构建

准备 NVIDIA 官方 CUDA 13.3 的 ptxas、fatbinary、cuobjdump。可将官方下载并校验的
独立组件解压到同一个工具目录的不同子目录，无需更换驱动或安装完整 Toolkit。

```powershell
python tools/ada_nr/build_ada.py D:/models/nvngx_dlssnr.dll `
  --output outputs/ada/nvngx_dlssnr.dll `
  --cuda-root D:/tools/cuda-13.3-tools `
  --work-dir outputs/ada/build
```

工作目录需为空。仅构建 `sm_89` 原生内核，保留原始镜像；修改 NGX 最低架构要求、
架构元数据和内部架构分支，转换不兼容的 bulk copy、barrier、reduction、fence 和 min/relu。
不会调用 Ampere 的 FP8→FP16 软件转换；包装器逐模块检查原生 e4m3 PTX 行未变。
原槽位不足时，底层补丁器可能降低编译优化等级以容纳内核，具体命令记录在 `.build.json`。
修改后原 NVIDIA 数字签名失效，补丁器移除原证书表并更新 PE 校验和。

## 当前验证

RTX 4080 SUPER、617.42 驱动：原模型 Preset 2 创建返回 `0xBAD00001`；补丁模型创建和执行成功。
已测试 720p Preset 0/2、1080p Preset 2，输出无 NaN/Inf。
这是独立 D3D12 合成输入测试，不是游戏画面、时序稳定性或性能提升验证。

模型仍使用常规文件名。采用目录隔离版插件时，放入
`OptiScaler/NR-310.9.16/nvngx_dlssnr.dll`，桥接器保持 `nvngx.dll_dlssnr.dll`。
