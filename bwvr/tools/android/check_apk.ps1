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
      - The files: only the manifest, the resource table, the libraries and
        the signature. No file of the game can be in the APK.
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
Expect ($badging -match "package: name='org.bwvr.quest'") 'the package is org.bwvr.quest'
Expect ($badging -match "minSdkVersion:'29'") 'minSdkVersion is 29 (Android 10, the floor of Quest)'
Expect ($badging -match "targetSdkVersion:'32'") 'targetSdkVersion is 32'
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
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($Apk)
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName })
    $stored = @{}
    foreach ($e in $zip.Entries) { $stored[$e.FullName] = ($e.CompressedLength -eq $e.Length) }
} finally {
    $zip.Dispose()
}
$allowed = '^(AndroidManifest\.xml|resources\.arsc|META-INF/[^/]+|lib/(arm64-v8a|x86_64)/lib(bwvr_quest|openxr_loader)\.so)$'
$other = @($names | Where-Object { $_ -notmatch $allowed })
Expect ($other.Count -eq 0) ('only the manifest, the libraries and the signature are in the APK' + $(if ($other.Count) { ': ' + ($other -join ', ') } else { '' }))
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
