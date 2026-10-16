<#
.SYNOPSIS
    Runs the APK in the Android emulator with the OpenXR test runtime, so
    the session of the headset runs with no headset (docs/android.md, "Test
    the APK", step 3).

.DESCRIPTION
    The test:

      1. Builds the test runtime (bwvr/android/test_runtime) for x86_64.
      2. Starts the AVD bwvr_xr_test with a system that it can write (it
         makes the AVD at the first run), and as root puts the runtime into
         /data/local/tmp/bwvr-xr and its manifest into
         /system/etc/openxr/1/active_runtime.json. The Khronos loader of the
         app reads that file when no runtime broker is there. SELinux goes
         to permissive, so the app can load the library from that folder.
      3. Installs bwvr/out/android/bwvr-quest.apk and starts it one time, so
         the app makes its folder. Then it puts a bwvr.ini there.
      4. Starts the app. The runtime plays its script: the right controller
         points at "Moving around", pulls the trigger, points at "Turn in
         steps" and pulls the trigger, presses B, points at "Save Config"
         and pulls the trigger. Then the runtime ends the session.
      5. Reads back the report of the runtime, its pictures of the quads,
         the log of the app, bwvr.ini and logcat, and checks them.

    The results go to bwvr/out/android/emu-xr-test. The script stops the
    emulator at the end. The test runtime is only a test: it shows that the
    app makes the calls of OpenXR in the right order with the right structs,
    and that the menu works with a controller ray. Only a headset shows the
    runtime of Meta.

.PARAMETER Keep
    Leaves the emulator on at the end.

.PARAMETER Stream
    A stream of the walks (docs/android.md, "The stream of the walks"). The
    app plays it into the renderer of the mod on the device of the session,
    and the test checks the stereo screen of the renderer too. It needs
    -Gpu host: the renderer needs Vulkan 1.3.

.PARAMETER Gpu
    The GPU of the emulator: swiftshader_indirect (the default, Vulkan 1.2 on
    the CPU) or host (the GPU of the PC).
#>
[CmdletBinding()]
param(
    [switch]$Keep,
    [string]$Gpu = 'swiftshader_indirect',
    [string]$Stream
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
# The tools write notes to stderr. Each check here reads their output, so a
# line on stderr must not stop the script.
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Repo = Split-Path $Bwvr -Parent
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$Out = Join-Path $Bwvr 'out\android\emu-xr-test'
$Avd = 'bwvr_xr_test'
$Image = 'system-images;android-34;google_apis;x86_64'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"
$XrDir = '/data/local/tmp/bwvr-xr'
$Port = 5586
$Serial = "emulator-$Port"

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
function AdbOut([string[]]$Arguments) { return (& $Adb -s $Serial @Arguments 2>&1 | Out-String) }
function Adb([string[]]$Arguments) { & $Adb -s $Serial @Arguments 2>&1 | Out-Null }
function Wait-Boot([int]$Minutes) {
    $deadline = (Get-Date).AddMinutes($Minutes)
    do {
        Start-Sleep -Seconds 3
        $booted = (& $Adb -s $Serial shell getprop sys.boot_completed 2>$null | Out-String).Trim()
    } while ($booted -ne '1' -and (Get-Date) -lt $deadline)
    if ($booted -ne '1') { return $false }
    # The package manager answers some seconds after the end of the boot.
    do {
        Start-Sleep -Seconds 2
        $pm = (& $Adb -s $Serial shell pm path android 2>$null | Out-String)
    } while ($pm -notmatch 'package:' -and (Get-Date) -lt $deadline)
    return $true
}

# 1. The test runtime.
Say 'Build the OpenXR test runtime for x86_64'
$runtimeBuild = Join-Path $Bwvr 'build-android\test-runtime'
& $SdkCmake -Wno-deprecated -G Ninja -S (Join-Path $Bwvr 'android\test_runtime') -B $runtimeBuild `
    "-DCMAKE_MAKE_PROGRAM=$SdkNinja" "-DCMAKE_TOOLCHAIN_FILE=$AndroidNdk\build\cmake\android.toolchain.cmake" `
    '-DANDROID_ABI=x86_64' '-DANDROID_PLATFORM=android-29' '-DANDROID_STL=c++_static' `
    '-DCMAKE_BUILD_TYPE=Release' "-DBWVR_THIRD_PARTY=$Repo\third_party" | Out-Null
& $SdkCmake --build $runtimeBuild
if ($LASTEXITCODE -ne 0) { Stop-Android 'The test runtime did not build.' }
$runtimeSo = Join-Path $runtimeBuild 'libbwvr_test_runtime.so'

# 2. The emulator with a system that it can write.
$avds = & $Emulator -list-avds 2>$null
if (-not ($avds -contains $Avd)) {
    Say "Make the AVD $Avd"
    $avdManager = Join-Path $AndroidSdk 'cmdline-tools\latest\bin\avdmanager.bat'
    'no' | & $avdManager create avd -n $Avd -k $Image -d 'pixel_tablet' --force | Out-Null
    if ($LASTEXITCODE -ne 0) { Stop-Android "avdmanager could not make the AVD $Avd." }
}
& $Adb start-server 2>&1 | Out-Null
$started = $false
$running = & $Adb devices 2>$null | Select-String -Pattern "^$Serial\s+device"
if (-not $running) {
    Say 'Start the emulator (no window, a system that it can write)'
    Start-Process -FilePath $Emulator -ArgumentList @('-avd', $Avd, '-port', "$Port",
        '-no-window', '-no-audio', '-no-boot-anim', '-no-snapshot', '-writable-system',
        '-gpu', $Gpu) -WindowStyle Hidden | Out-Null
    $started = $true
}
if (-not (Wait-Boot 6)) { Stop-Android 'The emulator did not start in 6 minutes.' }

try {
    Say 'Put the test runtime into the emulator'
    Adb @('root')
    Start-Sleep -Seconds 3
    Adb @('wait-for-device')
    $remount = AdbOut @('remount')
    if ($remount -match 'reboot') {
        Write-Host '   the first remount needs a reboot'
        Adb @('reboot')
        Start-Sleep -Seconds 10
        if (-not (Wait-Boot 6)) { Stop-Android 'The emulator did not start again after the reboot.' }
        Adb @('root')
        Start-Sleep -Seconds 3
        Adb @('wait-for-device')
        $remount = AdbOut @('remount')
    }
    Expect ($remount -match 'remount succeeded|Remount succeeded') ('adb remount: ' + ($remount.Trim() -split "`n")[-1])
    Adb @('shell', 'setenforce', '0')
    Adb @('shell', 'rm', '-rf', $XrDir)
    Adb @('shell', 'mkdir', '-p', $XrDir)
    Adb @('push', $runtimeSo, "$XrDir/libbwvr_test_runtime.so")
    $manifest = Join-Path $Out 'active_runtime.json'
    Set-Content -LiteralPath $manifest -Encoding ascii -Value @(
        '{', '  "file_format_version": "1.0.0",',
        '  "runtime": {', '    "name": "bwvr test runtime",',
        "    `"library_path`": `"$XrDir/libbwvr_test_runtime.so`"", '  }', '}')
    Adb @('shell', 'mkdir', '-p', '/system/etc/openxr/1')
    Adb @('push', $manifest, '/system/etc/openxr/1/active_runtime.json')

    # The script of the controllers. The rows of the menu picture are 74
    # pixels high from y = 190 (menu_layout.h), so the middle of row r is at
    # 227 + 74 r. Layer $L is the menu quad.
    $script = Join-Path $Out 'script.txt'
    # With a stream the two quads of the stereo screen come first.
    $L = $(if ($Stream) { 2 } else { 0 })
    Set-Content -LiteralPath $script -Encoding ascii -Value @(
        '40 capture start',
        "50 point right $L 800 671",
        '60 capture point',
        '70 trigger right 1', '74 trigger right 0',
        "90 point right $L 400 227",
        '100 trigger right 1', '104 trigger right 0',
        '110 capture moving',
        '120 button right 1', '124 button right 0',
        "140 point right $L 800 893",
        '150 trigger right 1', '154 trigger right 0',
        '170 capture saved',
        '200 exit')
    Adb @('push', $script, "$XrDir/script.txt")
    Adb @('shell', 'chmod', '777', $XrDir)
    Adb @('shell', 'chmod', '755', "$XrDir/libbwvr_test_runtime.so")
    Adb @('shell', 'chmod', '644', "$XrDir/script.txt")

    # 3. The APK and its folder.
    Say 'Install the APK'
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('uninstall', $Package)
    # A folder of the app that adb made belongs to the shell, and an
    # uninstall leaves it. The shell can delete it.
    Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
    $install = AdbOut @('install', $Apk)
    Expect ($install -match 'Success') ('adb install: ' + ($install.Trim() -split "`n")[-1])
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 4
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('shell', 'rm', '-f', "$XrDir/report.txt", "$XrDir/*.bmp")
    $ini = Join-Path $Out 'bwvr.ini.pushed'
    $iniLines = @('[bwvr]', 'snap_turn=2', 'menu_capture=1')
    if ($Stream) { $iniLines += 'stream_replay=1' }
    Set-Content -LiteralPath $ini -Encoding ascii -Value $iniLines
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    if ($Stream) { Adb @('push', $Stream, "$AppDir/game/bwvr-stream.bin") }

    # 4. The run.
    Say 'Start the app with the test runtime'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    $deadline = (Get-Date).AddSeconds($(if ($Stream) { 400 } else { 120 }))
    do {
        Start-Sleep -Seconds 2
        $report = AdbOut @('shell', 'cat', "$XrDir/report.txt")
    } while ($report -notmatch 'call xrDestroyInstance' -and (Get-Date) -lt $deadline)
    Start-Sleep -Seconds 2

    # 5. The results.
    Say 'Read the results'
    Adb @('pull', "$XrDir/.", $Out)
    Adb @('pull', "$AppDir/bwvr-quest.log", (Join-Path $Out 'bwvr-quest.log'))
    Adb @('pull', "$AppDir/game/bwvr.ini", (Join-Path $Out 'bwvr.ini'))
    $logcat = AdbOut @('logcat', '-d', '-v', 'brief')
    Set-Content -LiteralPath (Join-Path $Out 'logcat.txt') -Value $logcat -Encoding utf8
    $report = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'report.txt')) { $report = Get-Content -LiteralPath (Join-Path $Out 'report.txt') -Raw }
    $log = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'bwvr-quest.log')) { $log = Get-Content -LiteralPath (Join-Path $Out 'bwvr-quest.log') -Raw }
    $saved = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'bwvr.ini')) { $saved = Get-Content -LiteralPath (Join-Path $Out 'bwvr.ini') -Raw }

    Expect ($report -match 'call xrNegotiateLoaderRuntimeInterface') 'the loader of the app found the runtime and negotiated'
    Expect ($report -match 'call xrCreateInstance .*XR_KHR_vulkan_enable2' -and $report -match 'XR_KHR_android_create_instance') 'the instance has Vulkan 2 and the extension for Android'
    Expect ($report -match 'call xrGetVulkanGraphicsRequirements2KHR' -and $report -match 'call xrCreateVulkanDeviceKHR 0') 'the runtime made the Vulkan device of the app'
    Expect ($report -match 'call xrCreateSession') 'the app made a session'
    Expect ($report -match 'call xrAttachSessionActionSets 1 touch 1') 'the bindings of the Touch controller have only paths of the profile'
    Expect ($report -match 'call xrCreateSwapchain 1600x1200') 'a swapchain of 1600 by 1200 for the menu'
    Expect ($report -match 'call xrCreateSwapchain 64x64') 'a swapchain of 64 by 64 for the cursor'
    Expect ($report -match 'call xrBeginSession') 'the app began the session at READY'
    Expect ($report -match 'layers \d .*quad 0\.000 -0\.150 -1\.400 size 1\.400 1\.050') 'the menu quad is 1.4 m in front, 0.15 m down, 1.4 by 1.05 m'
    Expect ($report -match 'quad 0\.000 -0\.150 -1\.400 size 1\.400 1\.050 eye 0 quad') 'the cursor quad shows when the ray hits the menu'
    $pulses = ([regex]::Matches($report, 'pulse /user/hand/right')).Count
    Expect ($pulses -ge 3) "the right hand had a pulse at each new row ($pulses pulses)"
    Expect ($report -match 'call xrEndSession') 'the app ended the session at STOPPING'
    Expect ($report -match 'call xrDestroySession') 'the app destroyed the session'
    Expect ($report -notmatch '(?m)^fault ') ('the runtime found no fault' + $(if ($report -match '(?m)^fault (.*)$') { ': ' + $Matches[1] } else { '' }))
    # With a stream the stereo screen is layer 0 (left eye) and 1 (right
    # eye), and the menu is layer 2.
    $menuLayer = $(if ($Stream) { 2 } else { 0 })
    foreach ($name in @('start', 'point', 'moving', 'saved')) {
        $m = [regex]::Match($report, "capture \S*/$name-$menuLayer\.bmp (\d+)x(\d+) lit (\d+)")
        Expect ($m.Success -and [int]$m.Groups[3].Value -gt 100000) "the picture $name of the menu quad has the menu ($($m.Groups[3].Value) lit pixels)"
    }
    if ($Stream) {
        Expect ($log -match 'xr           : the GPU of the runtime: .*The renderer of the mod: yes') 'the device of the session has Vulkan 1.3 and the features of the renderer'
        Expect ($log -match 'vulkan       : the Vulkan of the OpenXR session') 'the renderer drew on the device of the session'
        Expect ($log -match 'replay       : .*: \d+ records, \d+ frames, [1-9]\d* walks') 'the app played the stream'
        Expect ($log -match 'a stereo screen of two quads') 'the app made the stereo screen'
        Expect ($report -match 'quad 0\.000 0\.100 -2\.600 size 2\.600 [0-9.]+ eye 1 quad 0\.000 0\.100 -2\.600 size 2\.600 [0-9.]+ eye 2') 'the stereo screen is a quad for the left eye and a quad for the right eye, 2.6 m in front'
        foreach ($eye in 0, 1) {
            $m = [regex]::Match($report, "capture \S*/start-$eye\.bmp (\d+)x(\d+) lit (\d+)")
            Expect ($m.Success -and [int]$m.Groups[3].Value -gt 100000) "the picture of eye $eye of the stereo screen has the scene ($($m.Groups[3].Value) lit pixels)"
        }
    }

    Expect ($log -match 'xr           : runtime bwvr test runtime') 'the app logged the runtime'
    Expect ($log -match 'the headset asks for 2064 by 2208') 'the app read the size of the eyes'
    Expect ($log -match 'the session is made') 'the app made its session and its layers'
    Expect ($log -match 'menu         : saved') 'the app saved bwvr.ini after the trigger on Save Config'
    Expect ($saved -match '(?m)^snap_turn=3\r?$') 'the trigger on "Turn in steps" made snap_turn 3, and the save wrote it'
    Expect ($saved -match '(?m)^headset_width=2064\r?$' -and $saved -match '(?m)^headset_height=2208\r?$') 'the app wrote the size of the eyes to bwvr.ini'
    Expect ($logcat -notmatch 'JNI DETECTED ERROR' -and $logcat -notmatch 'Fatal signal' -and $logcat -notmatch 'FATAL EXCEPTION') 'logcat has no JNI error and no crash'
    # Android can keep the process after the end of the activity, so the
    # log of the app is the proof.
    Expect ($log -match 'the session state is 8' -and $log -match 'app          : the app ends') 'the app ended after the runtime ended the session (EXITING)'

    # The pictures as PNG files, for a look.
    Add-Type -AssemblyName System.Drawing
    foreach ($bmp in Get-ChildItem -LiteralPath $Out -Filter '*.bmp') {
        $image = [System.Drawing.Image]::FromFile($bmp.FullName)
        try { $image.Save([IO.Path]::ChangeExtension($bmp.FullName, '.png'), [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $image.Dispose() }
    }
} finally {
    if ($started -and -not $Keep) {
        Say 'Stop the emulator'
        & $Adb -s $Serial emu kill 2>&1 | Out-Null
    }
}

Write-Host ('== ' + $checks + ' checks, ' + $faults.Count + ' failed. The results are in ' + $Out) -ForegroundColor Cyan
if ($faults.Count -gt 0) { exit 1 }
