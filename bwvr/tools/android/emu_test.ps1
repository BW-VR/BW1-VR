<#
.SYNOPSIS
    Installs the APK in the Android emulator, starts it and drives the menu
    (docs/android.md, "Test the APK", step 2).

.DESCRIPTION
    The emulator has no OpenXR runtime, so the app shows the menu in its
    window (flat_view.cpp). The test:

      1. Starts the AVD bwvr_test with no window (it makes the AVD at the
         first run, from system-images;android-34;google_apis;x86_64).
      2. Installs bwvr/out/android/bwvr-quest.apk, the same file that goes
         to a headset.
      3. Puts a bwvr.ini with menu_capture=1 and a test game folder into the
         folder of the app. -Exe names a runblack.exe to put there. With no
         -Exe, a file of 4 KB with no meaning stands in for it, and the app
         must say that it is not the v1.42 file.
      4. Starts the app, and sends keys: to the group "Moving around", one
         step of "Turn in steps", Back, down to "Save Config", Enter.
      5. Reads back bwvr.ini, the log of the app, logcat, a screenshot and
         the picture of the menu, and checks them.

    The results go to bwvr/out/android/emu-test. The script stops the
    emulator at the end when it started it.

.PARAMETER Exe
    A runblack.exe for the test game folder, for example the one of your
    own game. The script copies only that file, into the emulator.

.PARAMETER Keep
    Leaves the emulator on at the end.

.PARAMETER Gpu
    The GPU of the emulator: swiftshader_indirect (the default, Vulkan 1.2 on
    the CPU) or host (the GPU of the PC).
#>
[CmdletBinding()]
param(
    [string]$Exe,
    [switch]$Keep,
    [string]$Gpu = 'swiftshader_indirect'
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
# The tools write notes to stderr. Each check here reads their output, so a
# line on stderr must not stop the script.
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$Out = Join-Path $Bwvr 'out\android\emu-test'
$Avd = 'bwvr_test'
$Image = 'system-images;android-34;google_apis;x86_64'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"

if (-not (Test-Path -LiteralPath $Apk)) { Stop-Android "There is no APK at:`n$Apk`nRun build_apk.ps1 first." }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$faults = New-Object System.Collections.Generic.List[string]
$checks = 0
function Expect([bool]$Ok, [string]$What) {
    $script:checks++
    if ($Ok) { Write-Host ('   ok    ' + $What) }
    else { Write-Host ('   FAULT ' + $What) -ForegroundColor Red; $script:faults.Add($What) }
}
function Say([string]$Text) { Write-Host ('== ' + $Text) -ForegroundColor Cyan }
function AdbOut([string[]]$Arguments) { return (& $Adb -s $script:Serial @Arguments 2>&1 | Out-String) }
function Adb([string[]]$Arguments) { & $Adb -s $script:Serial @Arguments 2>&1 | Out-Null }
function Key([string]$Name, [int]$Times = 1) {
    for ($i = 0; $i -lt $Times; ++$i) {
        Adb @('shell', 'input', 'keyevent', $Name)
        Start-Sleep -Milliseconds 350
    }
}

# 1. The emulator.
$avdManager = Join-Path $AndroidSdk 'cmdline-tools\latest\bin\avdmanager.bat'
$avds = & $Emulator -list-avds 2>$null
if (-not ($avds -contains $Avd)) {
    Say "Make the AVD $Avd"
    'no' | & $avdManager create avd -n $Avd -k $Image -d 'pixel_tablet' --force | Out-Null
    if ($LASTEXITCODE -ne 0) { Stop-Android "avdmanager could not make the AVD $Avd. Install `"$Image`" with sdkmanager." }
}

& $Adb start-server 2>&1 | Out-Null
# The test has its own port, so it never takes the emulator of the test with
# the OpenXR test runtime (emu_xr_test.ps1, port 5586).
$port = 5584
$Serial = "emulator-$port"
$started = $false
$running = & $Adb devices 2>$null | Select-String -Pattern "^$Serial\s+device"
if ($running) {
    Say "The emulator $Serial runs already"
} else {
    Say "Start the emulator (no window)"
    Start-Process -FilePath $Emulator -ArgumentList @('-avd', $Avd, '-port', "$port",
        '-no-window', '-no-audio', '-no-boot-anim', '-no-snapshot',
        '-gpu', $Gpu) -WindowStyle Hidden | Out-Null
    $started = $true
}
$deadline = (Get-Date).AddMinutes(6)
do {
    Start-Sleep -Seconds 3
    $booted = (& $Adb -s $Serial shell getprop sys.boot_completed 2>$null | Out-String).Trim()
} while ($booted -ne '1' -and (Get-Date) -lt $deadline)
if ($booted -ne '1') { Stop-Android "The emulator did not start in 6 minutes." }
# The package manager answers some seconds after the end of the boot.
do {
    Start-Sleep -Seconds 2
    $pm = (& $Adb -s $Serial shell pm path android 2>$null | Out-String)
} while ($pm -notmatch 'package:' -and (Get-Date) -lt $deadline)
Write-Host ('   ' + (AdbOut @('shell', 'getprop', 'ro.build.fingerprint')).Trim())

try {
    # 2. The APK.
    Say 'Install the APK'
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('uninstall', $Package)
    # A folder of the app that adb made belongs to the shell, and an
    # uninstall leaves it. The shell can delete it.
    Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
    $install = AdbOut @('install', $Apk)
    Expect ($install -match 'Success') ('adb install: ' + ($install.Trim() -split "`n")[-1])

    # 3. The test game folder. The app starts one time first: Android gives
    # the folder of the app to the app when the app asks for it. A folder
    # that adb makes first belongs to the shell, and the app cannot write
    # to it.
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 3
    Adb @('shell', 'am', 'force-stop', $Package)
    $first = AdbOut @('shell', 'ls', "$AppDir")
    Expect ($first -match 'game' -and $first -match 'bwvr-quest.log') 'the first start made the folder game and the log'
    $ini = Join-Path $Out 'bwvr.ini.pushed'
    Set-Content -LiteralPath $ini -Encoding ascii -Value @('[bwvr]', 'menu_capture=1', 'snap_turn=2', 'gpu_bridge=1', 'fake_head=1', 'keep_me=7')
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    $exeFile = $Exe
    if (-not $exeFile) {
        $exeFile = Join-Path $Out 'stand-in.exe'
        [IO.File]::WriteAllBytes($exeFile, (New-Object byte[] 4096))
    }
    Adb @('push', $exeFile, "$AppDir/game/runblack.exe")

    # 4. The app and the keys.
    Say 'Start the app and drive the menu'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 4
    cmd /c "`"$Adb`" -s $Serial exec-out screencap -p > `"$Out\menu-main.png`""
    # The main page: Play, Play new game, Picture, Performance, Foveated
    # rendering, View, Moving around (row 6), Controllers, Testing, Save.
    Key 'KEYCODE_DPAD_DOWN' 6
    Key 'KEYCODE_ENTER'
    Start-Sleep -Seconds 1
    cmd /c "`"$Adb`" -s $Serial exec-out screencap -p > `"$Out\menu-moving.png`""
    Key 'KEYCODE_DPAD_RIGHT'
    Key 'KEYCODE_BACK'
    Key 'KEYCODE_DPAD_DOWN' 3
    Key 'KEYCODE_ENTER'
    Start-Sleep -Seconds 2
    cmd /c "`"$Adb`" -s $Serial exec-out screencap -p > `"$Out\menu-saved.png`""

    # 5. The results.
    Say 'Read the results'
    $appPid = (AdbOut @('shell', 'pidof', $Package)).Trim()
    Expect ($appPid -match '^\d+$') "the app still runs (pid $appPid)"
    Adb @('pull', "$AppDir/game/bwvr.ini", (Join-Path $Out 'bwvr.ini'))
    Adb @('pull', "$AppDir/bwvr-quest.log", (Join-Path $Out 'bwvr-quest.log'))
    Adb @('pull', "$AppDir/game/bwvr-menu.bmp", (Join-Path $Out 'bwvr-menu.bmp'))
    $logcat = AdbOut @('logcat', '-d', '-v', 'brief')
    Set-Content -LiteralPath (Join-Path $Out 'logcat.txt') -Value $logcat -Encoding utf8

    $saved = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'bwvr.ini')) { $saved = Get-Content -LiteralPath (Join-Path $Out 'bwvr.ini') -Raw }
    Expect ($saved -match '(?m)^snap_turn=3\r?$') 'the key step made snap_turn 3 (90 degrees), and the save wrote it'
    Expect ($saved -match '(?m)^backend=vulkan\r?$') 'the save wrote backend=vulkan'
    Expect ($saved -match '(?m)^vulkan_strict=1\r?$') 'the save wrote vulkan_strict=1: no fallback of the mod'
    Expect ($saved -match '(?m)^vr=1\r?$' -and $saved -match '(?m)^stereo=1\r?$' -and $saved -match '(?m)^head_camera=1\r?$') 'the save wrote vr, stereo and head_camera'
    Expect ($saved -notmatch 'gpu_bridge' -and $saved -notmatch 'fake_head') 'the save removed gpu_bridge and a retired key'
    Expect ($saved -match '(?m)^keep_me=7\r?$' -and $saved -match '(?m)^menu_capture=1\r?$') 'the save kept the keys that are not in the table'

    $log = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'bwvr-quest.log')) { $log = Get-Content -LiteralPath (Join-Path $Out 'bwvr-quest.log') -Raw }
    $buildId = (& git -C (Split-Path $Bwvr -Parent) rev-parse --short=7 HEAD 2>$null)
    Expect ($log -match "the headset app" -and $log -match [regex]::Escape("(build $buildId")) "the app of this build wrote its log (build $buildId)"
    Expect ($log -match 'no OpenXR session') 'with no runtime the app went to the window'
    Expect ($log -match 'flat view    : a swapchain') 'the window has a swapchain of Vulkan'
    Expect ($log -match 'canvas       : a picture of 1600 by 1200') 'the canvas of Android started'
    if ($Exe) {
        Expect ($log -match '\(v1\.42\)') 'the app found the v1.42 runblack.exe'
    } else {
        Expect ($log -match '\(not v1\.42\)') 'the app found that the stand in is not the v1.42 file'
    }
    Expect ($log -match 'menu         : saved') 'the app logged the save'
    Expect ($logcat -notmatch 'JNI DETECTED ERROR' -and $logcat -notmatch 'FATAL EXCEPTION' -and $logcat -notmatch 'Fatal signal') 'logcat has no JNI error and no crash'

    Add-Type -AssemblyName System.Drawing
    foreach ($shot in @('menu-main.png', 'menu-moving.png', 'menu-saved.png')) {
        $path = Join-Path $Out $shot
        $lit = 0
        if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).Length -gt 1000) {
            $bitmap = [System.Drawing.Bitmap]::FromFile($path)
            try {
                for ($y = 0; $y -lt $bitmap.Height; $y += 16) {
                    for ($x = 0; $x -lt $bitmap.Width; $x += 16) {
                        $c = $bitmap.GetPixel($x, $y)
                        if ($c.R + $c.G + $c.B -gt 120) { $lit++ }
                    }
                }
            } finally {
                $bitmap.Dispose()
            }
        }
        Expect ($lit -gt 50) "the screenshot $shot shows the menu ($lit lit samples)"
    }
} finally {
    if ($started -and -not $Keep) {
        Say 'Stop the emulator'
        & $Adb -s $Serial emu kill 2>&1 | Out-Null
        # adb lists the emulator for some seconds after the kill. A test that
        # starts in that time on the same port would take it for a running
        # emulator, so the script waits until it is gone.
        $gone = (Get-Date).AddSeconds(60)
        while ((Get-Date) -lt $gone -and
               (& $Adb devices 2>$null | Select-String -Pattern "^$Serial\s")) {
            Start-Sleep -Seconds 2
        }
    }
}

Write-Host ('== ' + $checks + ' checks, ' + $faults.Count + ' failed. The results are in ' + $Out) -ForegroundColor Cyan
if ($faults.Count -gt 0) { exit 1 }
