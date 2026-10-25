<#
.SYNOPSIS
    Builds libwow64fex.dll of FEX for the headsets (docs/android.md, "FEX as
    the CPU of the 32 bit code").

.DESCRIPTION
    libwow64fex.dll is the WoW64 CPU DLL of FEX: a DLL for Windows on ARM64
    with the BTCpu functions of the CPU interface of the WoW64 of Wine, as
    wowbox64.dll of Box64. It goes into the Wine tree of arm64-v8a beside
    wowbox64.dll, and the key wow64_cpu of the app chooses one of them.

    The source of FEX is not in the repo. Give the folder of a clone of a
    release tag with its submodules (for example local/third-party-src/
    FEX-2610). The script configures it with the toolchain file of FEX for
    mingw (Data/CMake/toolchain_mingw.cmake, the triple aarch64-w64-mingw32
    of llvm-mingw), with the options of the Wine DLL build of FEX (no LTO,
    no assertions, no tests, TUNE_CPU=none), and builds the target wow64fex
    with the Ninja of the Android SDK. The build folder is
    bwvr/build-android/fex-wow64.

    The build of FEX runs python3 (the generator of the config options) and
    dd (the Wine builtin mark in the PE header). The script puts the python3
    of llvm-mingw and the usr/bin of Git for Windows into PATH for them. It
    downloads nothing.

    The script writes a copy with no debug data to
    bwvr/out/android/fex/libwow64fex.dll, and the license texts of FEX and
    of each part of its source that goes into the DLL to
    bwvr/out/android/fex/licenses. pack_wine.sh puts both into the Wine tree
    of arm64-v8a for the APK.

.PARAMETER Source
    The folder of the FEX source, with its CMakeLists.txt and its
    submodules.

.PARAMETER Mingw
    The bin folder of llvm-mingw.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Mingw
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Source = (Resolve-Path -LiteralPath $Source).Path
$Mingw = (Resolve-Path -LiteralPath $Mingw).Path
foreach ($f in @('CMakeLists.txt', 'Data\CMake\toolchain_mingw.cmake', 'Source\Windows\WOW64\libwow64fex.def')) {
    if (-not (Test-Path -LiteralPath (Join-Path $Source $f))) {
        Stop-Android "There is no $f in $Source. Give the folder of the FEX source."
    }
}

# THE PARTS OF THE SOURCE THAT GO INTO THE DLL, with their license file.
# FEXCore and the code of FEX are under the license of FEX. The submodules
# and folders of External that the target wow64fex compiles or includes:
# fmt (the text format), xxhash (the hash of the code cache), rpmalloc (the
# allocator of FEXCore), range-v3 and unordered_dense (header libraries),
# tiny-json (the reader of the config files), SoftFloat-3e and cephes (the
# x87 math with 80 bit precision) and cpp-optparse of Source/Common.
# SoftFloat-3e has no license file: its license is the comment at the head
# of each source file, and the script copies that comment. The runtime of
# llvm-mingw that the DLL links statically (libc++, libc++abi, libunwind,
# the builtins of compiler-rt) has the LLVM exception, which asks for no
# notice with a compiled program. The script stops when one of the parts is
# not in the source, so a clone with no submodules gives no DLL.
$Parts = [ordered]@{
    'fex'                 = 'LICENSE'
    'fex_fmt'             = 'External\fmt\LICENSE'
    'fex_xxhash'          = 'External\xxhash\LICENSE'
    'fex_rpmalloc'        = 'External\rpmalloc\LICENSE'
    'fex_range_v3'        = 'External\range-v3\LICENSE.txt'
    'fex_unordered_dense' = 'External\unordered_dense\LICENSE'
    'fex_tiny_json'       = 'External\tiny-json\LICENSE'
    'fex_softfloat_3e'    = 'External\SoftFloat-3e\src\extF80_add.c'
    'fex_cephes'          = 'External\cephes\LICENSE'
    'fex_cpp_optparse'    = 'Source\Common\cpp-optparse\LICENSE'
}
foreach ($p in $Parts.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath (Join-Path $Source $p.Value))) {
        Stop-Android ("The FEX source has no " + $p.Value + " (" + $p.Key + "). A submodule is not there: " +
            "git submodule update --init in the FEX source.")
    }
}

foreach ($tool in @('aarch64-w64-mingw32-clang.exe', 'aarch64-w64-mingw32-clang++.exe', 'aarch64-w64-mingw32-dlltool.exe',
                    'aarch64-w64-mingw32-windres.exe', 'llvm-strip.exe', 'llvm-objdump.exe', 'llvm-readobj.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $Mingw $tool))) { Stop-Android "There is no $tool in $Mingw." }
}
# The build calls python3 by name. The python3 of PATH on Windows can be the
# stub of the Microsoft Store, so the python3 of llvm-mingw goes first.
$python = Join-Path (Split-Path $Mingw -Parent) 'python\bin\python3.exe'
if (-not (Test-Path -LiteralPath $python)) { Stop-Android "There is no python3.exe in the python\bin folder of llvm-mingw ($python)." }
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) { Stop-Android 'Git for Windows is not in PATH. The build of FEX needs its dd.exe.' }
$gitRoot = Split-Path (Split-Path $git.Source -Parent) -Parent
$dd = Join-Path $gitRoot 'usr\bin\dd.exe'
if (-not (Test-Path -LiteralPath $dd)) { Stop-Android "There is no dd.exe in $gitRoot\usr\bin." }

$Build = Join-Path $Bwvr 'build-android\fex-wow64'
$OutDir = Join-Path $Bwvr 'out\android\fex'
# The license texts of an earlier build go, so the folder has only the texts
# of this build.
if (Test-Path -LiteralPath (Join-Path $OutDir 'licenses')) { Remove-Item -LiteralPath (Join-Path $OutDir 'licenses') -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Build, $OutDir, (Join-Path $OutDir 'licenses') | Out-Null
$env:PATH = $Mingw + ';' + (Split-Path $python -Parent) + ';' + (Split-Path $dd -Parent) + ';' +
    (Split-Path $SdkNinja -Parent) + ';' + $env:PATH

Write-Host '== Configure FEX for WoW64 on ARM64 (aarch64-w64-mingw32)' -ForegroundColor Cyan
& $SdkCmake -Wno-deprecated -G Ninja -S $Source -B $Build "-DCMAKE_MAKE_PROGRAM=$SdkNinja" `
    "-DCMAKE_TOOLCHAIN_FILE=$Source\Data\CMake\toolchain_mingw.cmake" '-DMINGW_TRIPLE=aarch64-w64-mingw32' `
    '-DCMAKE_BUILD_TYPE=Release' '-DENABLE_LTO=False' '-DENABLE_ASSERTIONS=False' '-DENABLE_JEMALLOC_GLIBC_ALLOC=False' `
    '-DBUILD_TESTING=False' '-DBUILD_FEXCONFIG=False' '-DENABLE_CCACHE=False' '-DTUNE_ARCH=generic' '-DTUNE_CPU=none' `
    '-DRANGES_NATIVE=OFF' "-DPython_EXECUTABLE=$python" 2>&1 |
    Out-File -Encoding utf8 (Join-Path $Build 'configure.txt')
if ($LASTEXITCODE -ne 0) { Stop-Android "The configure of FEX failed. See $Build\configure.txt." }
$version = (Select-String -LiteralPath (Join-Path $Build 'configure.txt') -Pattern 'FEX version: (\S+)' |
    Select-Object -First 1).Matches[0].Groups[1].Value

Write-Host '== Build libwow64fex.dll' -ForegroundColor Cyan
& $SdkCmake --build $Build --target wow64fex 2>&1 | Out-File -Encoding utf8 (Join-Path $Build 'build.txt')
if ($LASTEXITCODE -ne 0) { Stop-Android "The build of libwow64fex.dll failed. See $Build\build.txt." }
$built = Join-Path $Build 'Bin\libwow64fex.dll'
if (-not (Test-Path -LiteralPath $built)) { Stop-Android "The build made no $built." }

$dll = Join-Path $OutDir 'libwow64fex.dll'
& (Join-Path $Mingw 'llvm-strip.exe') '--strip-debug' '-o' $dll $built
if ($LASTEXITCODE -ne 0) { Stop-Android 'llvm-strip failed.' }
foreach ($p in $Parts.GetEnumerator()) {
    $to = Join-Path $OutDir ('licenses\' + $p.Key + '.txt')
    if ($p.Value.EndsWith('.c')) {
        $text = (Get-Content -LiteralPath (Join-Path $Source $p.Value) -Raw).TrimStart()
        $end = $text.IndexOf('*/')
        if (-not $text.StartsWith('/*') -or $end -lt 0) { Stop-Android ('There is no license comment at the head of ' + $p.Value + '.') }
        Set-Content -LiteralPath $to -Value $text.Substring(0, $end + 2) -Encoding ascii
    } else {
        Copy-Item -LiteralPath (Join-Path $Source $p.Value) -Destination $to
    }
}
# The CRT of FEX has parts of musl (Source/Windows/Common/CRT/Musl, MIT).
# Their copyright lines go with the MIT text of fex.txt.
$musl = Join-Path $Source 'Source\Windows\Common\CRT\Musl'
$owners = @(Get-ChildItem -LiteralPath $musl -File | Select-String -Pattern 'SPDX-FileCopyrightText: (.*)' |
    ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() } | Sort-Object -Unique)
if ($owners.Count -eq 0) { Stop-Android "There are no copyright lines in $musl." }
[IO.File]::WriteAllLines((Join-Path $OutDir 'licenses\fex_musl.txt'), [string[]](
    @('The files of Source/Windows/Common/CRT/Musl of FEX come from musl. They have the MIT license of fex.txt',
      '(SPDX-License-Identifier: MIT) and these copyright lines:', '') + $owners))
Set-Content -LiteralPath (Join-Path $OutDir 'version.txt') -Value $version -Encoding ascii

$info = (& (Join-Path $Mingw 'llvm-objdump.exe') -p $dll | Out-String)
$exports = @([regex]::Matches($info, '\bBTCpu\w+') | ForEach-Object { $_.Value } | Sort-Object -Unique)
$imports = @([regex]::Matches($info, 'DLL Name: (\S+)') | ForEach-Object { $_.Groups[1].Value })
$header = (& (Join-Path $Mingw 'llvm-readobj.exe') --file-headers $dll | Out-String)
$machine = [regex]::Match($header, 'Machine: (\S+)').Groups[1].Value
Write-Host ('== ' + $dll + ' (FEX ' + $version + ', ' + (Get-Item -LiteralPath $dll).Length + ' bytes, ' + $machine + ', ' +
    $exports.Count + ' exports BTCpu, imports ' + ($imports -join ', ') + ')') -ForegroundColor Cyan
Write-Host ('   the exports BTCpu: ' + ($exports -join ' '))
Write-Host ('   the licenses: ' + ((@($Parts.Keys) + 'fex_musl' | ForEach-Object { $_ + '.txt' }) -join ', '))
if ($machine -ne 'IMAGE_FILE_MACHINE_ARM64') { Stop-Android "libwow64fex.dll is for $machine, not for ARM64." }
