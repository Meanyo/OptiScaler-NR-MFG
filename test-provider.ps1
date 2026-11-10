#requires -Version 7.0
param([Parameter(Mandatory = $true)][string[]]$Providers)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\build.ps1" -CheckDependencies
New-Item -ItemType Directory -Path "$PSScriptRoot\build" -Force | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /O2 /MD /utf-8 "/Fe:$PSScriptRoot\build\dlssg-preset-test.exe" "/Fo:$PSScriptRoot\build\dlssg-preset-test.obj" "$PSScriptRoot\tests\dlssg-preset-r11.cpp" /link "$PSScriptRoot\OptiScaler\library\detours\detours.lib"
if ($LASTEXITCODE -ne 0) { throw 'R11 provider test compilation failed' }
& "$PSScriptRoot\build\dlssg-preset-test.exe" @Providers | Tee-Object -FilePath "$PSScriptRoot\build\provider-test.log"
if ($LASTEXITCODE -ne 0) { throw 'R11 provider layout verification failed' }
