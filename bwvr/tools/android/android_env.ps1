<#
.SYNOPSIS
    Finds the tools of the Android SDK, the NDK and the JDK for the scripts
    of the headset app (docs/android.md, "Build the APK").

.DESCRIPTION
    Dot source this file. It sets $AndroidSdk, $AndroidNdk, $BuildTools,
    $AndroidJar, $SdkCmake, $SdkNinja, $Adb, $Emulator and $JavaHome, and
    puts the bin folder of the JDK first in PATH. It stops with a message
    that names the missing part.
#>

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# The versions that the build is made with. A newer NDK or newer build tools
# can work, but the scripts take these first.
$NdkVersion = '27.3.13750724'
$BuildToolsVersion = '35.0.1'
$PlatformLevel = 'android-34'
$SdkCmakeVersion = '3.31.6'

function Stop-Android([string]$Text) {
    Write-Host ''
    foreach ($line in ($Text -split "`n")) { Write-Host ('   ' + $line) -ForegroundColor Red }
    Write-Host ''
    exit 1
}

# THE LOCK OF THE EMULATOR TESTS. The emulator tests of each checkout and
# each worktree use the same AVDs and ports. A test calls Lock-AndroidEmulator
# before it starts or uses an emulator. When a test of another process holds
# the lock, the test gives "NOT RUN" and the exit code 2. Windows frees the
# lock when the process ends, also after a crash.
$script:EmulatorLock = $null
function Lock-AndroidEmulator {
    $script:EmulatorLock = New-Object System.Threading.Mutex($false, 'Local\bwvr-android-emulator')
    $held = $false
    try { $held = $script:EmulatorLock.WaitOne(0) } catch [System.Threading.AbandonedMutexException] { $held = $true }
    if (-not $held) {
        Write-Host ''
        Write-Host '   NOT RUN: an emulator test of another process runs. Start this test when it ended.' -ForegroundColor Yellow
        Write-Host ''
        exit 2
    }
}

function Find-Newest([string]$Dir, [string]$Want) {
    if (-not (Test-Path -LiteralPath $Dir)) { return $null }
    $exact = Join-Path $Dir $Want
    if (Test-Path -LiteralPath $exact) { return $exact }
    $all = Get-ChildItem -LiteralPath $Dir -Directory | Sort-Object Name -Descending
    if ($all) { return $all[0].FullName }
    return $null
}

$AndroidSdk = $env:ANDROID_HOME
if (-not $AndroidSdk) { $AndroidSdk = $env:ANDROID_SDK_ROOT }
if (-not $AndroidSdk) { $AndroidSdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
# A program from the Microsoft Store (for example the Claude app) writes to
# LOCALAPPDATA in a folder of its package. An SDK that such a program
# installed is there, and a plain terminal finds it there too.
if (-not (Test-Path -LiteralPath $AndroidSdk)) {
    $packaged = Get-ChildItem -Path (Join-Path $env:LOCALAPPDATA 'Packages\*\LocalCache\Local\Android\Sdk') -Directory -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($packaged) { $AndroidSdk = $packaged.FullName }
}
if (-not (Test-Path -LiteralPath $AndroidSdk)) {
    Stop-Android "There is no Android SDK at:`n$AndroidSdk`nSet ANDROID_HOME, or see docs/android.md, `"Build the APK`"."
}

$AndroidNdk = Find-Newest (Join-Path $AndroidSdk 'ndk') $NdkVersion
if (-not $AndroidNdk) { Stop-Android "The Android SDK has no NDK. Install `"ndk;$NdkVersion`" with sdkmanager." }
$BuildTools = Find-Newest (Join-Path $AndroidSdk 'build-tools') $BuildToolsVersion
if (-not $BuildTools) { Stop-Android "The Android SDK has no build tools. Install `"build-tools;$BuildToolsVersion`" with sdkmanager." }
$AndroidJar = Join-Path $AndroidSdk "platforms\$PlatformLevel\android.jar"
if (-not (Test-Path -LiteralPath $AndroidJar)) { Stop-Android "The Android SDK has no $PlatformLevel. Install `"platforms;$PlatformLevel`" with sdkmanager." }
$cmakeDir = Find-Newest (Join-Path $AndroidSdk 'cmake') $SdkCmakeVersion
if (-not $cmakeDir) { Stop-Android "The Android SDK has no CMake. Install `"cmake;$SdkCmakeVersion`" with sdkmanager." }
$SdkCmake = Join-Path $cmakeDir 'bin\cmake.exe'
$SdkNinja = Join-Path $cmakeDir 'bin\ninja.exe'
$Adb = Join-Path $AndroidSdk 'platform-tools\adb.exe'
$Emulator = Join-Path $AndroidSdk 'emulator\emulator.exe'

$JavaHome = $env:JAVA_HOME
if (-not $JavaHome -or -not (Test-Path -LiteralPath (Join-Path $JavaHome 'bin\java.exe'))) {
    $JavaHome = $null
    foreach ($root in @('C:\Program Files\Microsoft', 'C:\Program Files\Eclipse Adoptium', 'C:\Program Files\Java')) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $jdk = Get-ChildItem -LiteralPath $root -Directory -Filter 'jdk*' | Sort-Object Name -Descending | Select-Object -First 1
        if ($jdk -and (Test-Path -LiteralPath (Join-Path $jdk.FullName 'bin\java.exe'))) { $JavaHome = $jdk.FullName; break }
    }
}
if (-not $JavaHome) { Stop-Android "There is no JDK 17 or later. apksigner needs Java. Install Microsoft OpenJDK 17, or set JAVA_HOME." }
$env:JAVA_HOME = $JavaHome
$env:Path = (Join-Path $JavaHome 'bin') + ';' + $env:Path
