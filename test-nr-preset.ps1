#requires -Version 7.0
param([Parameter(Mandatory)][string]$ModelPath, [string]$ToolchainRoot)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\build.ps1" -CheckDependencies -ToolchainRoot $ToolchainRoot
Set-Location -LiteralPath $PSScriptRoot
& cl.exe /nologo /std:c++20 /EHsc /O2 /MT /DNOMINMAX /utf-8 tools/ada_nr/bench_nvngx.dll.cpp /Fo:build/nr-preset-test.obj /Fe:build/nvngx.dll_preset-test.exe /link d3d12.lib dxgi.lib
if ($LASTEXITCODE -ne 0) { throw 'Preset test compilation failed' }
$core = Join-Path (Get-ItemProperty 'HKLM:\SOFTWARE\NVIDIA Corporation\Global\NGXCore').FullPath '_nvngx.dll'
& "$PSScriptRoot\build\nvngx.dll_preset-test.exe" $core (Resolve-Path -LiteralPath $ModelPath).Path 1280 720 99 5 20
if ($LASTEXITCODE -ne 0) { throw "Single-runtime preset test failed: $LASTEXITCODE" }
