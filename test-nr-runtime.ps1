#requires -Version 7.0
param([string]$ToolchainRoot)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\build.ps1" -CheckDependencies -ToolchainRoot $ToolchainRoot
$out = Join-Path $PSScriptRoot 'build'
$source = Join-Path $PSScriptRoot 'tests\nr-runtime-r12.cpp'
foreach ($id in 1, 2) {
    & cl.exe /nologo /EHsc /MD /LD /DMOCK_NR_MODEL "/DMODEL_ID=$id" $source "/Fo:$out\mock-nr-$id.obj" /link "/OUT:$out\mock-nr-$id.dll" "/IMPLIB:$out\mock-nr-$id.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Mock model compilation failed' }
}
& cl.exe /nologo /EHsc /MD $source "/Fo:$out\nr-runtime-test.obj" "/Fe:$out\nr-runtime-test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Runtime test compilation failed' }
& "$out\nr-runtime-test.exe" "$out\nvngx.dll_dlssnr.dll" "$out\nvngx.dll_dlssnr_310_9_16.dll" "$out\mock-nr-1.dll" "$out\mock-nr-2.dll"
if ($LASTEXITCODE -ne 0) { throw 'Runtime isolation test failed' }
