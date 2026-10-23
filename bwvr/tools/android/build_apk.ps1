<#
.SYNOPSIS
    Builds the APK of the headset app (docs/android.md, "Build the APK").

.DESCRIPTION
    For each ABI the script configures bwvr/android with the toolchain of the
    NDK and builds libbwvr_quest.so. Then it links the manifest with aapt2,
    adds the libraries with no compression, aligns the APK for pages of 16 KB
    and signs it with the key of local/android/bwvr-debug.keystore. The
    script makes that key at its first run. The key is not in the repo.

    The APK has arm64-v8a for the headsets and x86_64 for the Android
    emulator, so one file is the one that the tests install and the one
    that goes to a headset.

    WINE. When bwvr/out/android/wine has the Wine tree of an ABI
    (<abi>.tar and <abi>.id of pack_wine.sh), the APK carries it as
    assets/wine/<abi>.tar (compressed) and assets/wine/<abi>.id, with the
    license texts of Wine, FreeType and Box64 in assets/licenses. A device
    unpacks only the tree of its own ABI. With a Wine tree the
    targetSdkVersion is 28: Android lets an app with 29 or later run no
    program of its own folder (docs/android.md, "Wine in the app"). With no
    Wine tree the APK has no Wine and targetSdkVersion 32, and the log says
    so.

.PARAMETER Abis
    The ABIs to build. arm64-v8a and x86_64 by default.

.PARAMETER Clean
    Builds each file again.
#>
[CmdletBinding()]
param(
    [string[]]$Abis = @('arm64-v8a', 'x86_64'),
    [switch]$Clean
)

. (Join-Path $PSScriptRoot 'android_env.ps1')

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Repo = Split-Path $Bwvr -Parent
$Source = Join-Path $Bwvr 'android'
$BuildRoot = Join-Path $Bwvr 'build-android'
$OutDir = Join-Path $Bwvr 'out\android'
$Apk = Join-Path $OutDir 'bwvr-quest.apk'
$KeyDir = Join-Path $Repo 'local\android'
$KeyStore = Join-Path $KeyDir 'bwvr-debug.keystore'
$ThirdParty = Join-Path $Repo 'third_party'

function Say([string]$Text) { Write-Host ('== ' + $Text) -ForegroundColor Cyan }
function Note([string]$Text) { Write-Host ('   ' + $Text) }
function Run([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { Stop-Android "$([IO.Path]::GetFileName($Exe)) failed with exit code $LASTEXITCODE." }
}

# The version from version.h, and the build id as the PC build makes it.
$versionHeader = Get-Content -LiteralPath (Join-Path $Bwvr 'src\common\version.h') -Raw
$major = [int]([regex]::Match($versionHeader, '#define BWVR_VERSION_MAJOR (\d+)').Groups[1].Value)
$minor = [int]([regex]::Match($versionHeader, '#define BWVR_VERSION_MINOR (\d+)').Groups[1].Value)
$patch = [int]([regex]::Match($versionHeader, '#define BWVR_VERSION_PATCH (\d+)').Groups[1].Value)
$versionName = "$major.$minor.$patch"
$versionCode = $major * 10000 + $minor * 100 + $patch
$buildId = (& git -C $Repo rev-parse --short=7 HEAD 2>$null)
if (-not $buildId) { $buildId = 'unknown' }
$dirty = (& git -C $Repo status --porcelain --untracked-files=no 2>$null)
if ($dirty) { $buildId += '+' }

Say "Build the headset app $versionName (build $buildId)"
Note "NDK:         $AndroidNdk"
Note "Build tools: $BuildTools"
Note "JDK:         $JavaHome"

$stage = Join-Path $BuildRoot 'apk'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$strip = Join-Path $AndroidNdk 'toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-strip.exe'

foreach ($abi in $Abis) {
    Say "Native code for $abi"
    $dir = Join-Path $BuildRoot $abi
    if ($Clean -and (Test-Path -LiteralPath $dir)) { Remove-Item -LiteralPath $dir -Recurse -Force }
    # -Wno-deprecated: the toolchain file of the NDK asks for an old CMake.
    Run $SdkCmake @('-Wno-deprecated', '-G', 'Ninja', '-S', $Source, '-B', $dir,
        "-DCMAKE_MAKE_PROGRAM=$SdkNinja",
        "-DCMAKE_TOOLCHAIN_FILE=$AndroidNdk\build\cmake\android.toolchain.cmake",
        "-DANDROID_ABI=$abi", '-DANDROID_PLATFORM=android-29',
        '-DANDROID_STL=c++_static', '-DCMAKE_BUILD_TYPE=Release',
        "-DBWVR_THIRD_PARTY=$ThirdParty", "-DBWVR_BUILD_ID_NOW=$buildId")
    Run $SdkCmake @('--build', $dir)
    $lib = Join-Path $stage "lib\$abi"
    New-Item -ItemType Directory -Force -Path $lib | Out-Null
    Run $strip @('--strip-unneeded', '-o', (Join-Path $lib 'libbwvr_quest.so'), (Join-Path $dir 'libbwvr_quest.so'))
    Copy-Item -LiteralPath (Join-Path $ThirdParty "openxr-android\$abi\libopenxr_loader.so") -Destination $lib
}

Say 'The APK'
# The license texts of the third party code in the APK.
$assets = Join-Path $stage 'assets'
New-Item -ItemType Directory -Force -Path (Join-Path $assets 'licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $ThirdParty 'openxr-android\LICENSE') `
    -Destination (Join-Path $assets 'licenses\openxr_loader.txt')

# THE WINE TREES (pack_wine.sh). Each ABI that has one gets it.
$wineDir = Join-Path $OutDir 'wine'
$wineAbis = @()
foreach ($abi in $Abis) {
    $tar = Join-Path $wineDir "$abi.tar"
    $id = Join-Path $wineDir "$abi.id"
    if ((Test-Path -LiteralPath $tar) -and (Test-Path -LiteralPath $id)) {
        New-Item -ItemType Directory -Force -Path (Join-Path $assets 'wine') | Out-Null
        Copy-Item -LiteralPath $tar, $id -Destination (Join-Path $assets 'wine')
        $wineAbis += $abi
        Note ("Wine tree of ${abi}: " + (Get-Content -LiteralPath $id -Raw).Trim() + ', ' +
            [math]::Round((Get-Item -LiteralPath $tar).Length / 1MB, 1) + ' MB')
    } else {
        Note "Wine tree of ${abi}: none in $wineDir. The APK has no Wine for $abi"
    }
}
$targetSdk = '32'
if ($wineAbis.Count -gt 0) {
    $targetSdk = '28'
    foreach ($name in @('wine', 'freetype') + $(if ($wineAbis -contains 'arm64-v8a') { @('box64') } else { @() })) {
        $text = Join-Path $wineDir "licenses\$name.txt"
        if (-not (Test-Path -LiteralPath $text)) { Stop-Android "There is no license text $text. Run pack_wine.sh again." }
        Copy-Item -LiteralPath $text -Destination (Join-Path $assets "licenses\$name.txt")
    }
}
Note "targetSdkVersion $targetSdk"

$base = Join-Path $stage 'base.apk'
Run (Join-Path $BuildTools 'aapt2.exe') @('link', '-o', $base,
    '--manifest', (Join-Path $Source 'AndroidManifest.xml'),
    '-I', $AndroidJar, '--min-sdk-version', '29', '--target-sdk-version', $targetSdk,
    '--version-code', "$versionCode", '--version-name', $versionName)

# The libraries go in with no compression (the method "stored"): with
# extractNativeLibs="false" Android maps them from the APK, and zipalign puts
# them on a page. The zip classes of .NET Framework give a deflate entry for
# NoCompression, so the jar tool of the JDK adds them (0: store, M: no
# manifest of a jar). Each file is named, so the APK gets no entry for a
# folder.
# The license texts go in the same way: aapt2 of Windows gives an asset a
# name with a backslash, which Android does not find.
$libs = @(Get-ChildItem -LiteralPath (Join-Path $stage 'lib') -Recurse -File |
    ForEach-Object { 'lib/' + $_.Directory.Name + '/' + $_.Name }) +
    @(Get-ChildItem -LiteralPath (Join-Path $assets 'licenses') -File |
    ForEach-Object { 'assets/licenses/' + $_.Name })
Push-Location $stage
try {
    Run (Join-Path $JavaHome 'bin\jar.exe') (@('uf0M', $base) + $libs)
    # The Wine trees go in compressed (the method "deflate"). The asset
    # manager of Android inflates them as a stream when the app reads them.
    $wineFiles = @($wineAbis | ForEach-Object { "assets/wine/$_.tar"; "assets/wine/$_.id" })
    if ($wineFiles.Count -gt 0) { Run (Join-Path $JavaHome 'bin\jar.exe') (@('ufM', $base) + $wineFiles) }
} finally {
    Pop-Location
}

$aligned = Join-Path $stage 'aligned.apk'
Run (Join-Path $BuildTools 'zipalign.exe') @('-P', '16', '-f', '4', $base, $aligned)

if (-not (Test-Path -LiteralPath $KeyStore)) {
    Say 'Make the debug key (local/android, not in the repo)'
    New-Item -ItemType Directory -Force -Path $KeyDir | Out-Null
    Run (Join-Path $JavaHome 'bin\keytool.exe') @('-genkeypair', '-keystore', $KeyStore,
        '-alias', 'bwvr', '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000',
        '-storepass', 'android', '-keypass', 'android',
        '-dname', 'CN=Black and White VR Mod debug key')
}
# The schemes v2 and v3. Android 10 and later read both, and a tool that
# reads only v2 finds it too. No v1: no Android before 7 runs the app.
Run (Join-Path $BuildTools 'apksigner.bat') @('sign', '--ks', $KeyStore,
    '--ks-key-alias', 'bwvr', '--ks-pass', 'pass:android', '--key-pass', 'pass:android',
    '--v1-signing-enabled', 'false', '--v2-signing-enabled', 'true',
    '--v3-signing-enabled', 'true', '--out', $Apk, $aligned)

$size = (Get-Item -LiteralPath $Apk).Length
Say ('APK: ' + $Apk + ' (' + [math]::Round($size / 1MB, 2) + ' MB)')
