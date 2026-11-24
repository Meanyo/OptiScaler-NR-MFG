#requires -Version 7.0
param(
    [switch]$Incremental,
    [switch]$RebuildNrShaders,
    [switch]$CheckDependencies,
    [string]$BuildDirectory = 'build',
    [string]$ToolchainRoot,
    [string]$WindowsKitsRoot = 'C:\Program Files (x86)\Windows Kits\10'
)
$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$projectDir = Join-Path $repo 'OptiScaler'
if (-not $ToolchainRoot) {
    $locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $locator) {
        $ToolchainRoot = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
    }
    if (-not $ToolchainRoot -and (Test-Path -LiteralPath 'C:\BuildTools\VC\Tools\MSVC')) {
        $ToolchainRoot = 'C:\BuildTools'
    }
}
if (-not $ToolchainRoot) { throw 'Install Visual Studio C++ Build Tools or specify -ToolchainRoot.' }
$toolchain = $ToolchainRoot
$windowsKits = $WindowsKitsRoot
$build = Join-Path $PSScriptRoot $BuildDirectory
New-Item -ItemType Directory -Path $build -Force | Out-Null
$vc = Get-ChildItem -LiteralPath "$toolchain\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$sdk = Get-ChildItem -LiteralPath "$windowsKits\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$vcbin = Join-Path $vc.FullName 'bin\Hostx64\x64'
$sdkbin = "$windowsKits\bin\$($sdk.Name)\x64"
$env:PATH = "$vcbin;$sdkbin;$env:PATH"
$env:INCLUDE = "$($vc.FullName)\include;$($sdk.FullName)\ucrt;$($sdk.FullName)\shared;$($sdk.FullName)\um;$($sdk.FullName)\winrt;$($sdk.FullName)\cppwinrt"
$env:LIB = "$($vc.FullName)\lib\x64;$windowsKits\Lib\$($sdk.Name)\ucrt\x64;$windowsKits\Lib\$($sdk.Name)\um\x64"
[xml]$xml = Get-Content -LiteralPath "$projectDir\OptiScaler.vcxproj" -Raw
$ns = [System.Xml.XmlNamespaceManager]::new($xml.NameTable)
$ns.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
$condition = "'`$(Configuration)|`$(Platform)'=='Release|x64'"
$props = $xml.SelectNodes('//m:PropertyGroup', $ns) | Where-Object { $_.Condition -eq $condition -and $_.IncludePath }
$defs = $xml.SelectNodes('//m:ItemDefinitionGroup', $ns) | Where-Object { $_.Condition -eq $condition }
function Expand-ProjectPath([string]$path) {
    $path.Replace('$(ProjectDir)', "$projectDir\").Replace('$(SolutionDir)', "$repo\")
}
$includes = @($props.IncludePath.Split(';') | Where-Object { $_ -notmatch '\$\(IncludePath\)' } | ForEach-Object { '/I"' + (Expand-ProjectPath $_).TrimEnd('\') + '"' })
$libdirs = @($props.LibraryPath.Split(';') | Where-Object { $_ -notmatch '\$\(LibraryPath\)' } | ForEach-Object { '/LIBPATH:"' + (Expand-ProjectPath $_).TrimEnd('\') + '"' })
if ($CheckDependencies) {
    $missing = @($xml.SelectNodes('//m:ClCompile[@Include]', $ns) | Where-Object { -not (Test-Path -LiteralPath (Join-Path $projectDir $_.Include)) } | ForEach-Object Include)
    $searchDirs = @($props.LibraryPath.Split(';') | Where-Object { $_ -notmatch '\$\(' -or $_ -match '\$\((ProjectDir|SolutionDir)\)' } | ForEach-Object { Expand-ProjectPath $_ }) + $env:LIB.Split(';')
    foreach ($lib in @($defs.Link.AdditionalDependencies.Split(';') | Where-Object { $_ -notmatch '%\(' })) {
        if (-not ($searchDirs | Where-Object { Test-Path -LiteralPath (Join-Path $_ $lib) })) { $missing += $lib }
    }
    if ($missing) { throw ('Missing build inputs: ' + ($missing -join ', ')) }
    Write-Output "Build dependencies OK: MSVC $($vc.Name), Windows SDK $($sdk.Name)."
    return
}
$common = @('/nologo','/c','/O2','/Oi','/Gy','/GL','/MD','/EHsc','/W3','/sdl','/permissive-','/std:c++latest','/fp:fast','/favor:blend','/DNDEBUG','/D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR','/D_WINDOWS','/D_USRDLL','/D_UNICODE','/DUNICODE','/w34996','/utf-8') + $includes
$pch = Join-Path $build 'OptiScaler.pch'
function Invoke-Rsp([string]$exe, [string]$name, [string[]]$arguments) {
    $rsp = Join-Path $build "$name.rsp"
    [IO.File]::WriteAllLines($rsp, $arguments, [Text.UTF8Encoding]::new($false))
    & $exe "@$rsp" 2>&1 | Tee-Object -FilePath (Join-Path $build "$name.log")
    if ($LASTEXITCODE -ne 0) { throw "$name failed: $LASTEXITCODE" }
}
Set-Location -LiteralPath $projectDir
if (-not $Incremental -or -not (Test-Path -LiteralPath $pch)) {
    Invoke-Rsp 'cl.exe' 'pch' ($common + @('/Yc"pch.h"', "/Fp`"$pch`"", "/Fo`"$build\pch.obj`"", 'pch.cpp'))
}
$allSources = @($xml.SelectNodes('//m:ClCompile[@Include]', $ns) | Where-Object { $_.Include -ne 'pch.cpp' })
foreach ($usesPch in @($true, $false)) {
    $group = @($allSources | Where-Object { (@($_.PrecompiledHeader | ForEach-Object { $_.InnerText }) -notcontains 'NotUsing') -eq $usesPch })
    if ($Incremental) { $group = @($group | Where-Object { $obj = Join-Path $build ([IO.Path]::GetFileNameWithoutExtension($_.Include) + '.obj'); ($RebuildNrShaders -and ($_.Include -in @('shaders\dlssnr\DlssNr_Dx12.cpp', 'shaders\dlssnr\DlssNr_Vk.cpp'))) -or (-not (Test-Path -LiteralPath $obj)) -or ((Get-Item -LiteralPath $_.Include).LastWriteTimeUtc -gt (Get-Item -LiteralPath $obj -ErrorAction SilentlyContinue).LastWriteTimeUtc) }) }
    if ($group.Count -eq 0) { continue }
    $opts = $common + @('/MP4', "/Fo`"$build\\`"")
    if ($usesPch) { $opts += @('/Yu"pch.h"', "/Fp`"$pch`"") }
    Invoke-Rsp 'cl.exe' "compile-$usesPch" ($opts + @($group | ForEach-Object { '"' + $_.Include + '"' }))
}
$res = Join-Path $build 'OptiScaler.res'
& rc.exe /nologo /d NDEBUG /d _UNICODE /d UNICODE /fo $res OptiScaler.rc
if ($LASTEXITCODE -ne 0) { throw "Resource compilation failed: $LASTEXITCODE" }
$libs = @($defs.Link.AdditionalDependencies.Split(';') | Where-Object { $_ -notmatch '%\(' })
$libs += @('kernel32.lib','user32.lib','gdi32.lib','winspool.lib','comdlg32.lib','advapi32.lib','shell32.lib','ole32.lib','oleaut32.lib','uuid.lib','odbc32.lib','odbccp32.lib','delayimp.lib')
$link = @('/nologo','/DLL','/MACHINE:X64','/SUBSYSTEM:WINDOWS','/OPT:REF','/OPT:ICF','/LTCG','/INCREMENTAL:NO','/MANIFEST:NO','/IGNORE:4099','/NODEFAULTLIB:LIBCMT','/DEF:Source.def', "/OUT:`"$build\OptiScaler.dll`"") + $libdirs + $libs
$link += @($defs.Link.DelayLoadDLLs.Split(';') | ForEach-Object { '/DELAYLOAD:' + $_ })
$link += @($xml.SelectNodes('//m:ClCompile[@Include]', $ns) | ForEach-Object { '"' + (Join-Path $build ([IO.Path]::GetFileNameWithoutExtension($_.Include) + '.obj')) + '"' })
$link += '"' + $res + '"'
Invoke-Rsp 'link.exe' 'link' $link
Get-Item -LiteralPath "$build\OptiScaler.dll" | Select-Object FullName,Length
Get-FileHash -LiteralPath "$build\OptiScaler.dll" -Algorithm SHA256

# Single NR runtime; model selection is handled through presets.
Invoke-Rsp 'cl.exe' 'nr-forwarder-compile' @('/nologo','/c','/O2','/MT','/EHsc','/std:c++17',
    "/Fo`"$build\nr-forwarder.obj`"", 'dlssnr\forwarder\dlssnr_forwarder.cpp')
Invoke-Rsp 'link.exe' 'nr-forwarder-link' @('/nologo','/DLL','/MACHINE:X64',
    "/OUT:`"$build\nvngx.dll_dlssnr.dll`"", "/IMPLIB:`"$build\nr-forwarder.lib`"",
    "`"$build\nr-forwarder.obj`"", 'kernel32.lib', 'd3d12.lib')
Copy-Item -LiteralPath "$build\OptiScaler.dll" -Destination "$build\OptiScaler-NR-MFG-R12.1.dll" -Force
