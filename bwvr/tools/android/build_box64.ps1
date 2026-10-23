<#
.SYNOPSIS
    Builds Box64 for the headsets (arm64-v8a, Android) with the NDK
    (docs/android.md, "What the app cannot do yet").

.DESCRIPTION
    The source of Box64 is not in the repo. Give the folder of an unpacked
    source archive of a release (for example local/third-party-src/
    box64-0.4.4-src/box64-0.4.4). The script configures it with the
    toolchain of the NDK for Android 10 (API 29), with the options ARM64 and
    ANDROID (the dynamic recompiler for ARM64), builds it with Ninja, and
    writes a stripped copy to bwvr/out/android/box64/box64.

    The build of Box64 calls sh for one header (git_head.h), so the folder
    of sh.exe of Git for Windows goes first in PATH. GIT_DIR names a folder
    that is not there, so the header gets no revision of this repo.

    With -Mingw the script builds wowbox64.dll (the option WOW64 of Box64)
    and not the Linux program box64: the CPU for x86 of an ARM64 Wine in
    the WoW64 mode, a DLL for Windows on ARM64. Its build needs llvm-mingw
    (aarch64-w64-mingw32-clang), because the NDK has no headers of
    Windows. The script writes a copy with no debug data to
    bwvr/out/android/box64/wowbox64.dll, and the license text of Box64 to
    bwvr/out/android/box64/LICENSE. pack_wine.sh puts both into the Wine
    tree of arm64-v8a for the APK. The Linux program box64 is not in the
    APK.

.PARAMETER Source
    The folder of the Box64 source, with its CMakeLists.txt.

.PARAMETER Mingw
    The bin folder of llvm-mingw.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [string]$Mingw
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Source = (Resolve-Path -LiteralPath $Source).Path
if (-not (Test-Path -LiteralPath (Join-Path $Source 'CMakeLists.txt'))) {
    Stop-Android "There is no CMakeLists.txt in $Source."
}
$Build = Join-Path $Bwvr $(if ($Mingw) { 'build-android\box64-wow64' } else { 'build-android\box64-arm64' })
$OutDir = Join-Path $Bwvr 'out\android\box64'
New-Item -ItemType Directory -Force -Path $Build, $OutDir | Out-Null

$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) { Stop-Android 'Git for Windows is not in PATH. The build of Box64 needs its sh.exe.' }
$gitRoot = Split-Path (Split-Path $git.Source -Parent) -Parent
$sh = @(Get-ChildItem -Path (Join-Path $gitRoot 'usr\bin\sh.exe'), (Join-Path $gitRoot 'bin\sh.exe') -ErrorAction SilentlyContinue)
if ($sh.Count -eq 0) { Stop-Android "There is no sh.exe in $gitRoot." }
$env:PATH = $sh[0].DirectoryName + ';' + $env:PATH
$env:GIT_DIR = Join-Path $Build 'no-git'
$wow64 = 'OFF'
if ($Mingw) {
    $Mingw = (Resolve-Path -LiteralPath $Mingw).Path
    if (-not (Test-Path -LiteralPath (Join-Path $Mingw 'aarch64-w64-mingw32-clang.exe'))) {
        Stop-Android "There is no aarch64-w64-mingw32-clang.exe in $Mingw."
    }
    # wowbox64 is an external project of CMake. It finds its compiler and
    # Ninja through PATH.
    $env:PATH = $Mingw + ';' + (Split-Path $SdkNinja -Parent) + ';' + $env:PATH
    $wow64 = 'ON'
}

Write-Host '== Configure Box64 for arm64-v8a' -ForegroundColor Cyan
& $SdkCmake -Wno-deprecated -G Ninja -S $Source -B $Build "-DCMAKE_MAKE_PROGRAM=$SdkNinja" `
    "-DCMAKE_TOOLCHAIN_FILE=$AndroidNdk\build\cmake\android.toolchain.cmake" `
    '-DANDROID_ABI=arm64-v8a' '-DANDROID_PLATFORM=android-29' `
    '-DARM64=1' '-DANDROID=1' '-DNOGIT=1' "-DWOW64=$wow64" '-DCMAKE_BUILD_TYPE=RelWithDebInfo' |
    Out-File -Encoding utf8 (Join-Path $Build 'configure.txt')
if ($LASTEXITCODE -ne 0) { Stop-Android "The configure of Box64 failed. See $Build\configure.txt." }

if ($Mingw) {
    Write-Host '== Build wowbox64.dll' -ForegroundColor Cyan
    & $SdkCmake --build $Build --target wowbox64 2>&1 | Out-File -Encoding utf8 (Join-Path $Build 'build.txt')
    if ($LASTEXITCODE -ne 0) { Stop-Android "The build of wowbox64.dll failed. See $Build\build.txt." }
    $dll = Join-Path $OutDir 'wowbox64.dll'
    & (Join-Path $Mingw 'llvm-strip.exe') '--strip-debug' '-o' $dll `
        (Join-Path $Build 'wowbox64-prefix\src\wowbox64-build\wowbox64.dll')
    if ($LASTEXITCODE -ne 0) { Stop-Android 'llvm-strip failed.' }
    Copy-Item -LiteralPath (Join-Path $Source 'LICENSE') -Destination (Join-Path $OutDir 'LICENSE')
    $info = (& (Join-Path $Mingw 'llvm-objdump.exe') -p $dll | Out-String)
    $exports = ([regex]::Matches($info, 'BTCpu\w+')).Count
    $imports = @([regex]::Matches($info, 'DLL Name: (\S+)') | ForEach-Object { $_.Groups[1].Value })
    $header = (& (Join-Path $Mingw 'llvm-readobj.exe') --file-headers $dll | Out-String)
    $machine = [regex]::Match($header, 'Machine: (\S+)').Groups[1].Value
    Write-Host ('== ' + $dll + ' (' + [math]::Round((Get-Item -LiteralPath $dll).Length / 1MB, 1) +
        ' MB, ' + $machine + ', ' + $exports + ' exports BTCpu, imports ' + ($imports -join ', ') + ')') -ForegroundColor Cyan
    exit 0
}

Write-Host '== Build Box64' -ForegroundColor Cyan
& $SdkCmake --build $Build 2>&1 | Out-File -Encoding utf8 (Join-Path $Build 'build.txt')
if ($LASTEXITCODE -ne 0) { Stop-Android "The build of Box64 failed. See $Build\build.txt." }

$tools = Join-Path $AndroidNdk 'toolchains\llvm\prebuilt\windows-x86_64\bin'
$out = Join-Path $OutDir 'box64'
& (Join-Path $tools 'llvm-strip.exe') '--strip-unneeded' '-o' $out (Join-Path $Build 'box64')
if ($LASTEXITCODE -ne 0) { Stop-Android 'llvm-strip failed.' }
$header = (& (Join-Path $tools 'llvm-readelf.exe') -h $out | Out-String)
$needed = @(& (Join-Path $tools 'llvm-readelf.exe') -d $out | Select-String 'NEEDED' |
    ForEach-Object { [regex]::Match($_.Line, '\[(.*)\]').Groups[1].Value })
$machine = [regex]::Match($header, 'Machine:\s+(\S+)').Groups[1].Value
Write-Host ('== ' + $out + ' (' + [math]::Round((Get-Item -LiteralPath $out).Length / 1MB, 1) +
    ' MB, ' + $machine + ', needs ' + ($needed -join ', ') + ')') -ForegroundColor Cyan
