<#
.SYNOPSIS
    Checks the APK of the headset app with no device (docs/android.md, "Test
    the APK", step 1).

.DESCRIPTION
    The checks:
      - The signature (apksigner verify, schemes v2 and v3).
      - The manifest: the package, the SDK levels, the categories of an
        immersive app for Meta Horizon OS and for OpenXR, the permissions
        and the queries of the OpenXR loader, the headsets of Meta.
      - The native code: arm64-v8a for the headsets, x86_64 for the
        emulator. Each library is stored with no compression, on a page of
        16 KB, and its LOAD segments have an alignment of 16 KB.
      - The files: only the manifest, the resource table, the libraries,
        the license texts, the Wine trees, the files of the mod and the
        signature. No file of the game can be in the APK.
      - The files of the mod (assets/mod, with Wine only): ddraw.dll and
        openxr_loader.dll are the files of bwvr/build/src/mod/Release (the
        SHA-256), each a PE file for 32 bit x86.
      - Each Wine tree (assets/wine/<abi>.tar): a build id, the loader,
        wineserver, ntdll.dll of i386, the CPU DLL of its ABI in wine.inf
        and in the tree, libfreetype.so, the fonts, and none of the DLLs
        that stay out (kLeftOutNames and kLeftOutPrefix of
        android/src/wine_rules.h). With a tree the targetSdkVersion is 28
        and the license texts of Wine and FreeType (and Box64 for
        arm64-v8a) are in the APK. With none it is 32.
    It ends with exit code 1 and the list of the faults when a check fails.

.PARAMETER Apk
    The APK. bwvr/out/android/bwvr-quest.apk by default.
#>
[CmdletBinding()]
param(
    [string]$Apk
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
# The tools write notes to stderr. Each check here reads their output, so a
# line on stderr must not stop the script.
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
if (-not $Apk) { $Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk' }
if (-not (Test-Path -LiteralPath $Apk)) { Stop-Android "There is no APK at:`n$Apk`nRun build_apk.ps1 first." }

$faults = New-Object System.Collections.Generic.List[string]
$checks = 0
function Expect([bool]$Ok, [string]$What) {
    $script:checks++
    if ($Ok) {
        Write-Host ('   ok    ' + $What)
    } else {
        Write-Host ('   FAULT ' + $What) -ForegroundColor Red
        $script:faults.Add($What)
    }
}

Write-Host "== Check $Apk" -ForegroundColor Cyan

# The signature.
# With the minSdkVersion of the APK (29) apksigner looks at v3 only. A level
# of 24 makes it check the v2 block too.
$verify = & (Join-Path $BuildTools 'apksigner.bat') verify --verbose --min-sdk-version 24 $Apk 2>&1 | Out-String
Expect ($LASTEXITCODE -eq 0 -and $verify -match 'Verifies') 'apksigner verifies the APK'
Expect ($verify -match 'v2 scheme \(APK Signature Scheme v2\): true') 'the APK has a signature of scheme v2'

# The manifest.
$aapt2 = Join-Path $BuildTools 'aapt2.exe'
$badging = & $aapt2 dump badging $Apk 2>&1 | Out-String
$tree = & $aapt2 dump xmltree --file AndroidManifest.xml $Apk 2>&1 | Out-String
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($Apk)
try {
    $wineAbis = @($zip.Entries | ForEach-Object {
        if ($_.FullName -match '^assets/wine/(.+)\.tar$') { $Matches[1] } })
} finally {
    $zip.Dispose()
}
Expect ($badging -match "package: name='org.bwvr.quest'") 'the package is org.bwvr.quest'
Expect ($badging -match "minSdkVersion:'29'") 'minSdkVersion is 29 (Android 10, the floor of Quest)'
if ($wineAbis.Count -gt 0) {
    Expect ($badging -match "targetSdkVersion:'28'") 'targetSdkVersion is 28: the app runs the programs of Wine from its folder'
} else {
    Expect ($badging -match "targetSdkVersion:'32'") 'targetSdkVersion is 32 (no Wine tree)'
}
Expect ($badging -match "launchable-activity: name='android.app.NativeActivity'") 'the launch activity is the native activity'
Expect ($tree -match 'com\.oculus\.intent\.category\.VR') 'the category of an immersive app of Meta Horizon OS'
Expect ($tree -match 'org\.khronos\.openxr\.intent\.category\.IMMERSIVE_HMD') 'the category of an immersive app of OpenXR'
Expect ($tree -match 'org\.khronos\.openxr\.permission\.OPENXR"') 'the permission OPENXR'
Expect ($tree -match 'org\.khronos\.openxr\.permission\.OPENXR_SYSTEM') 'the permission OPENXR_SYSTEM'
Expect ($tree -match 'org\.khronos\.openxr\.runtime_broker') 'the query of the runtime broker'
Expect ($tree -match 'com\.oculus\.supportedDevices' -and $tree -match 'quest3') 'the headsets of Meta, with the Quest 3'
Expect ($tree -match 'android\.app\.lib_name' -and $tree -match '"bwvr_quest"') 'the native activity loads libbwvr_quest.so'
Expect ($tree -match 'android:hasCode\(0x0101000c\)=false') 'the APK has no Java code'

# The files of the APK.
$zip = [System.IO.Compression.ZipFile]::OpenRead($Apk)
$wineDir = Join-Path $env:TEMP ('bwvr-check-wine-' + [guid]::NewGuid().ToString('N'))
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName })
    $stored = @{}
    foreach ($e in $zip.Entries) { $stored[$e.FullName] = ($e.CompressedLength -eq $e.Length) }
    if ($wineAbis.Count -gt 0) {
        New-Item -ItemType Directory -Force -Path $wineDir | Out-Null
        foreach ($e in $zip.Entries) {
            if ($e.FullName -match '^assets/wine/([^/]+)$') {
                [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $wineDir $Matches[1]), $true)
            }
            if ($e.FullName -match '^assets/mod/([^/]+)$') {
                [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $wineDir ('mod-' + $Matches[1])), $true)
            }
        }
    }
} finally {
    $zip.Dispose()
}
$allowed = '^(AndroidManifest\.xml|resources\.arsc|META-INF/[^/]+|lib/(arm64-v8a|x86_64)/lib(bwvr_quest|openxr_loader)\.so|assets/licenses/[a-z0-9_]+\.txt|assets/wine/(arm64-v8a|x86_64)\.(tar|id)|assets/mod/(ddraw|openxr_loader)\.dll)$'
$other = @($names | Where-Object { $_ -notmatch $allowed })
Expect ($other.Count -eq 0) ('only the manifest, the libraries, the license texts, the Wine trees, the files of the mod and the signature are in the APK' + $(if ($other.Count) { ': ' + ($other -join ', ') } else { '' }))
Expect ($names -contains 'assets/licenses/openxr_loader.txt') 'the license text of the OpenXR loader is in the APK'

# THE FILES OF THE MOD (assets/mod, android/src/play_rules.h): with Wine,
# the ddraw.dll and the OpenXR loader of the build of .\dev.ps1 build, byte
# for byte, as 32 bit DLLs for x86. With no Wine, none.
$modBuild = Join-Path $Bwvr 'build\src\mod\Release'
foreach ($name in @('ddraw.dll', 'openxr_loader.dll')) {
    $entry = "assets/mod/$name"
    if ($wineAbis.Count -eq 0) {
        Expect (-not ($names -contains $entry)) "an APK with no Wine has no $entry"
        continue
    }
    $inApk = Join-Path $wineDir "mod-$name"
    $built = Join-Path $modBuild $name
    $apkHash = if (Test-Path -LiteralPath $inApk) { (Get-FileHash -LiteralPath $inApk -Algorithm SHA256).Hash.ToLowerInvariant() } else { 'none' }
    $buildHash = if (Test-Path -LiteralPath $built) { (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash.ToLowerInvariant() } else { 'no build' }
    Expect ($apkHash -eq $buildHash) "$entry is the $name of the build ($modBuild): SHA-256 $apkHash"
    $machine = ''
    if (Test-Path -LiteralPath $inApk) {
        $pe = [IO.File]::ReadAllBytes($inApk)
        $at = [BitConverter]::ToInt32($pe, 0x3C)
        if ($pe.Length -gt $at + 6 -and [BitConverter]::ToUInt32($pe, $at) -eq 0x4550) { $machine = '0x{0:x}' -f [BitConverter]::ToUInt16($pe, $at + 4) }
    }
    Expect ($machine -eq '0x14c') "$entry is a PE file for 32 bit x86 (machine $machine)"
}

# THE WINE TREES. The tar of Windows (bsdtar) lists them and reads wine.inf.
$rules = Get-Content -LiteralPath (Join-Path $Bwvr 'android\src\wine_rules.h') -Raw
$leftNames = @([regex]::Matches([regex]::Match($rules, '(?s)kLeftOutNames\[\] = \{(.*?)\};').Groups[1].Value, '"([^"]+)"') |
    ForEach-Object { $_.Groups[1].Value })
$leftPrefix = [regex]::Match($rules, 'kLeftOutPrefix\[\] = "([^"]+)"').Groups[1].Value
$bsdtar = Join-Path $env:SystemRoot 'System32\tar.exe'
foreach ($abi in $wineAbis) {
    $arch = @{ 'arm64-v8a' = 'aarch64'; 'x86_64' = 'x86_64' }[$abi]
    $cpu = @{ 'arm64-v8a' = 'wowbox64.dll'; 'x86_64' = 'wow64cpu.dll' }[$abi]
    $tarFile = Join-Path $wineDir "$abi.tar"
    $idFile = Join-Path $wineDir "$abi.id"
    $id = if (Test-Path -LiteralPath $idFile) { (Get-Content -LiteralPath $idFile -Raw).Trim() } else { '' }
    Expect ($id -match '^[0-9a-f]{16}$') "the Wine tree of $abi has a build id ($id)"
    Expect (-not $stored["assets/wine/$abi.tar"]) "the Wine tree of $abi is compressed in the APK"
    $list = @(& $bsdtar -tf $tarFile 2>&1 | ForEach-Object { "$_".TrimEnd('/') })
    Expect ($LASTEXITCODE -eq 0 -and $list.Count -gt 100) "the tar of $abi reads ($($list.Count) entries)"
    foreach ($need in @("$abi/lib/wine/$arch-unix/wine", "$abi/bin/wineserver",
                        "$abi/lib/wine/$arch-unix/ntdll.so", "$abi/lib/wine/i386-windows/ntdll.dll",
                        "$abi/lib/wine/$arch-windows/$cpu", "$abi/lib/libfreetype.so",
                        'share/wine/wine.inf', 'share/wine/fonts/tahoma.ttf')) {
        Expect ($list -contains $need) "the Wine tree of $abi has $need"
    }
    $bad = @($list | Where-Object {
        $stem = ([IO.Path]::GetFileName($_) -split '\.')[0].ToLowerInvariant()
        ($leftNames -contains $stem) -or $stem.StartsWith($leftPrefix)
    })
    Expect ($leftNames.Count -ge 6 -and $leftPrefix -eq 'd3d' -and $bad.Count -eq 0) ("the Wine tree of $abi has none of the DLLs that stay out (" +
        ($leftNames -join ', ') + ", $leftPrefix*)" + $(if ($bad.Count) { ': ' + ($bad -join ', ') } else { '' }))
    $inf = (& $bsdtar -xOf $tarFile 'share/wine/wine.inf' 2>$null | Out-String)
    Expect ($inf -match [regex]::Escape("HKLM,Software\Microsoft\Wow64\x86,,,`"$cpu`"")) "wine.inf of $abi makes $cpu the CPU of the 32 bit code"
    Expect ($names -contains 'assets/licenses/wine.txt' -and $names -contains 'assets/licenses/freetype.txt') "the license texts of Wine and FreeType are in the APK"
    if ($abi -eq 'arm64-v8a') { Expect ($names -contains 'assets/licenses/box64.txt') 'the license text of Box64 is in the APK' }
}
if (Test-Path -LiteralPath $wineDir) { Remove-Item -LiteralPath $wineDir -Recurse -Force }
if ($wineAbis.Count -eq 0) { Write-Host '   note  the APK has no Wine tree' }
foreach ($abi in @('arm64-v8a', 'x86_64')) {
    foreach ($lib in @('libbwvr_quest.so', 'libopenxr_loader.so')) {
        $name = "lib/$abi/$lib"
        Expect ($names -contains $name) "$name is in the APK"
        if ($names -contains $name) { Expect $stored[$name] "$name has no compression" }
    }
}
$align = & (Join-Path $BuildTools 'zipalign.exe') -c -P 16 4 $Apk 2>&1 | Out-String
Expect ($LASTEXITCODE -eq 0) 'zipalign: the libraries are on pages of 16 KB'

# The LOAD segments of the libraries of the mod.
$readelf = Join-Path $AndroidNdk 'toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-readelf.exe'
foreach ($abi in @('arm64-v8a', 'x86_64')) {
    $so = Join-Path $Bwvr "build-android\apk\lib\$abi\libbwvr_quest.so"
    if (-not (Test-Path -LiteralPath $so)) { continue }
    $segments = & $readelf -lW $so 2>&1 | Out-String
    $loads = @([regex]::Matches($segments, '(?m)^\s*LOAD\s.*\s(0x[0-9a-f]+)\s*$') | ForEach-Object { $_.Groups[1].Value })
    Expect ($loads.Count -gt 0 -and @($loads | Where-Object { [Convert]::ToInt64($_, 16) -lt 16384 }).Count -eq 0) "the LOAD segments of $abi have an alignment of 16 KB"
    $needed = & $readelf -d $so 2>&1 | Out-String
    Expect ($needed -notmatch 'libc\+\+_shared') "libbwvr_quest.so of $abi needs no libc++_shared.so"
    Expect ($needed -match 'libopenxr_loader\.so') "libbwvr_quest.so of $abi loads the OpenXR loader"
}

$size = (Get-Item -LiteralPath $Apk).Length
Write-Host ('== ' + $checks + ' checks, ' + $faults.Count + ' failed. The APK is ' + [math]::Round($size / 1MB, 2) + ' MB') -ForegroundColor Cyan
if ($faults.Count -gt 0) { exit 1 }
