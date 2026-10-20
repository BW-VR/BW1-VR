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

    Each run has input_live=1 (docs/android.md, "The input channel"): the
    app writes the input of the mod into bwvr-input.chan of the game folder
    at each frame of the session. The runtime recenters its LOCAL space one
    time, and the app puts one pulse into the ring of the events itself
    (input_live_test_pulse). The test reads the file back with
    bwvr_input_writer.exe (bwvr\build\test\input_writer) and checks the views,
    the display period, the recenter, the size of the eyes and the end of the
    session in the slot, the poses of the right hand of the script in the
    slots of the log, and the pulse at the runtime.

    The results go to bwvr/out/android/emu-xr-test, with -Stream to
    bwvr/out/android/emu-xr-stream-test, and with -Live to
    bwvr/out/android/emu-xr-live-test. The script stops the
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

.PARAMETER Live
    The folder of a desk run of live_stream_run.ps1 (docs/android.md, "The
    live stream"). Its ring-capture.bin goes into the game folder of the app
    as the feed, with stream_live=1, stream_live_feed=1 and
    stream_live_feed_ms (-FeedMs): a child process of the app writes a frame
    into the ring each FeedMs, and the app plays the live stream on the
    device of the session. The test checks the stereo screen, that the
    screen got a copy for each new picture, and the pictures of the test
    frame of the stream against the pictures of the PC in the desk folder.
    It needs -Gpu host.

.PARAMETER FeedMs
    With -Live: the time between two frames of the feed, 14 by default. 0:
    no wait, so the feed is faster than the session, and the test checks
    that the app skipped the walks of frames.

.PARAMETER Gpu
    The GPU of the emulator: swiftshader_indirect (the default, Vulkan 1.2 on
    the CPU) or host (the GPU of the PC).
#>
[CmdletBinding()]
param(
    [switch]$Keep,
    [string]$Gpu = 'swiftshader_indirect',
    [string]$Stream,
    [string]$Live,
    [int]$FeedMs = 14
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
Lock-AndroidEmulator
# The tools write notes to stderr. Each check here reads their output, so a
# line on stderr must not stop the script.
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Repo = Split-Path $Bwvr -Parent
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$InputWriter = Join-Path $Bwvr 'build\test\input_writer\Release\bwvr_input_writer.exe'
# A run with a stream keeps its results in a folder of its own.
$Out = Join-Path $Bwvr $(if ($Live) { 'out\android\emu-xr-live-test' } elseif ($Stream) { 'out\android\emu-xr-stream-test' } else { 'out\android\emu-xr-test' })
$Feed = $null
if ($Live) {
    $Feed = Join-Path $Live 'ring-capture.bin'
    if (-not (Test-Path -LiteralPath $Feed)) { Stop-Android "There is no feed at:`n$Feed" }
}
# The renderer draws on the device of the session with a stream or a live
# stream, and then the stereo screen is there.
$Screen = [bool]$Stream -or [bool]$Live
$Avd = 'bwvr_xr_test'
$Image = 'system-images;android-34;google_apis;x86_64'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"
$XrDir = '/data/local/tmp/bwvr-xr'
$Port = 5586
$Serial = "emulator-$Port"

if (-not (Test-Path -LiteralPath $Apk)) { Stop-Android "There is no APK at:`n$Apk`nRun build_apk.ps1 first." }
if (-not (Test-Path -LiteralPath $InputWriter)) { Stop-Android "There is no test writer of the input channel at:`n$InputWriter`nRun .\dev.ps1 build first." }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# The share of pixels that differ by more than 24 in a channel, and the mean
# difference, of two BMP files of one size (as emu_replay_test.ps1). -1 for
# two sizes.
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class XrLiveBmpDiff {
  public static double[] Compare(string a, string b) {
    byte[] x = File.ReadAllBytes(a), y = File.ReadAllBytes(b);
    int wx = BitConverter.ToInt32(x, 18), hx = BitConverter.ToInt32(x, 22);
    int wy = BitConverter.ToInt32(y, 18), hy = BitConverter.ToInt32(y, 22);
    int bx = BitConverter.ToInt16(x, 28) / 8, by = BitConverter.ToInt16(y, 28) / 8;
    if (wx != wy || Math.Abs(hx) != Math.Abs(hy)) return new double[] { -1, -1 };
    int ox = BitConverter.ToInt32(x, 10), oy = BitConverter.ToInt32(y, 10);
    int rx = (wx * bx + 3) & ~3, ry = (wy * by + 3) & ~3;
    long differ = 0, total = 0; double sum = 0;
    for (int row = 0; row < Math.Abs(hx); ++row) {
      for (int col = 0; col < wx; ++col) {
        int px = ox + row * rx + col * bx, py = oy + row * ry + col * by;
        int worst = 0;
        for (int c = 0; c < 3; ++c) {
          int d = Math.Abs(x[px + c] - y[py + c]);
          sum += d;
          if (d > worst) worst = d;
        }
        if (worst > 24) ++differ;
        ++total;
      }
    }
    return new double[] { 100.0 * differ / total, sum / (3.0 * total) };
  }
}
'@

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
    $L = $(if ($Screen) { 2 } else { 0 })
    # The live stream makes the stereo screen at its first picture, after the
    # first frames of the session: its script starts 200 frames later.
    $T = $(if ($Live) { 200 } else { 0 })
    Set-Content -LiteralPath $script -Encoding ascii -Value @(
        "$(30 + $T) recenter",
        "$(40 + $T) capture start",
        "$(50 + $T) point right $L 800 671",
        "$(60 + $T) capture point",
        "$(70 + $T) trigger right 1", "$(74 + $T) trigger right 0",
        "$(90 + $T) point right $L 400 227",
        "$(100 + $T) trigger right 1", "$(104 + $T) trigger right 0",
        "$(110 + $T) capture moving",
        "$(120 + $T) button right 1", "$(124 + $T) button right 0",
        "$(140 + $T) point right $L 800 893",
        "$(150 + $T) trigger right 1", "$(154 + $T) trigger right 0",
        "$(170 + $T) capture saved",
        "$(200 + $T) exit")
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
    # The pulse of the test goes into the ring after the script points at
    # the menu, so the hand has a pulse of each kind.
    $iniLines = @('[bwvr]', 'snap_turn=2', 'menu_capture=1', 'input_live=1', "input_live_test_pulse=$(100 + $T)")
    if ($Stream) { $iniLines += 'stream_replay=1' }
    if ($Live) { $iniLines += @('stream_live=1', 'stream_live_feed=1', "stream_live_feed_ms=$FeedMs") }
    Set-Content -LiteralPath $ini -Encoding ascii -Value $iniLines
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    if ($Stream) { Adb @('push', $Stream, "$AppDir/game/bwvr-stream.bin") }
    if ($Live) { Adb @('push', $Feed, "$AppDir/game/bwvr-stream-feed.bin") }

    # 4. The run.
    Say 'Start the app with the test runtime'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    $deadline = (Get-Date).AddSeconds($(if ($Live) { 600 } elseif ($Stream) { 400 } else { 120 }))
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
    Adb @('pull', "$AppDir/game/bwvr-input.chan", (Join-Path $Out 'bwvr-input.chan'))
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
    $menuLayer = $(if ($Screen) { 2 } else { 0 })
    foreach ($name in @('start', 'point', 'moving', 'saved')) {
        $m = [regex]::Match($report, "capture \S*/$name-$menuLayer\.bmp (\d+)x(\d+) lit (\d+)")
        Expect ($m.Success -and [int]$m.Groups[3].Value -gt 100000) "the picture $name of the menu quad has the menu ($($m.Groups[3].Value) lit pixels)"
    }
    if ($Screen) {
        Expect ($log -match 'xr           : the GPU of the runtime: .*The renderer of the mod: yes') 'the device of the session has Vulkan 1.3 and the features of the renderer'
        Expect ($log -match 'vulkan       : the Vulkan of the OpenXR session') 'the renderer drew on the device of the session'
        if ($Stream) {
            Expect ($log -match 'replay       : .*: \d+ records, \d+ frames, [1-9]\d* walks') 'the app played the stream'
        }
        Expect ($log -match 'a stereo screen of two quads') 'the app made the stereo screen'
        Expect ($report -match 'quad 0\.000 0\.100 -2\.600 size 2\.600 [0-9.]+ eye 1 quad 0\.000 0\.100 -2\.600 size 2\.600 [0-9.]+ eye 2') 'the stereo screen is a quad for the left eye and a quad for the right eye, 2.6 m in front'
        foreach ($eye in 0, 1) {
            $m = [regex]::Match($report, "capture \S*/start-$eye\.bmp (\d+)x(\d+) lit (\d+)")
            Expect ($m.Success -and [int]$m.Groups[3].Value -gt 100000) "the picture of eye $eye of the stereo screen has the scene ($($m.Groups[3].Value) lit pixels)"
        }
    }
    if ($Live) {
        Expect ($log -match "live         : stream_live_feed=1\. The process \d+ writes the \d+ frames .* one each $FeedMs ms") "a child process of the app writes the frames of the feed into the ring, one each $FeedMs ms"
        Expect ($log -match 'live         : the writer answered the ask') 'the writer answered the ask for the whole state'
        Expect ($log -match 'live         : frames come\. The first is frame \d+, with the whole state') 'the first frame has the whole state'
        $liveLine = [regex]::Match($log, 'live         : (no new frame for 1 s|the app ends): (\d+) frames played and (\d+) skipped \(frames (\d+) to (\d+)\), (\d+) pictures\. In all (\d+) records, (\d+) walks, (\d+) draws, (\d+) surfaces, (\d+) of an unknown kind([^.]*)\.')
        Expect ($liveLine.Success -and [int]$liveLine.Groups[2].Value -gt 0 -and [int]$liveLine.Groups[8].Value -gt 0 -and [int]$liveLine.Groups[11].Value -eq 0 -and $liveLine.Groups[12].Value -eq '') ("the app played the live stream" + $(if ($liveLine.Success) { ": $($liveLine.Groups[2].Value) frames played and $($liveLine.Groups[3].Value) skipped, frames $($liveLine.Groups[4].Value) to $($liveLine.Groups[5].Value), $($liveLine.Groups[6].Value) pictures, $($liveLine.Groups[8].Value) walks, 0 of an unknown kind" } else { '' }))
        $screenLine = [regex]::Match($log, 'xr           : (\d+) copies of the stereo screen for eye 0 and (\d+) for eye 1')
        $pictures = $(if ($liveLine.Success) { [int]$liveLine.Groups[6].Value } else { 0 })
        if ($FeedMs -eq 0 -and $liveLine.Success) {
            Expect ([int]$liveLine.Groups[3].Value -gt 0) "a feed faster than the session: the app skipped the walks of $($liveLine.Groups[3].Value) frames and played $($liveLine.Groups[2].Value) frames whole"
        }
        Expect ($screenLine.Success -and $pictures -gt 0 -and [int]$screenLine.Groups[1].Value -ge $pictures -and [int]$screenLine.Groups[2].Value -ge $pictures) ("each eye of the stereo screen got a copy for each new picture (" + $(if ($screenLine.Success) { "$($screenLine.Groups[1].Value) and $($screenLine.Groups[2].Value) copies, $pictures pictures" } else { 'no line in the log' }) + ')')
        # The pictures of the test frame of the stream (vk_scene_test_frame),
        # against the pictures of the PC of the same desk run: at most 2
        # percent of the pixels with a difference of more than 24.
        $references = @(Get-ChildItem -LiteralPath $Live -Filter 'bwvr-vk-*.bmp')
        Expect ($references.Count -gt 0) "the PC made $($references.Count) pictures of the test frame"
        $listing = AdbOut @('shell', 'ls', "$AppDir/game")
        foreach ($name in ([regex]::Matches($listing, 'bwvr-vk-[a-z]+-\d+\.bmp') | ForEach-Object { $_.Value })) {
            Adb @('pull', "$AppDir/game/$name", (Join-Path $Out $name))
        }
        foreach ($ref in $references) {
            $mine = Join-Path $Out $ref.Name
            if (-not (Test-Path -LiteralPath $mine)) {
                Expect $false "the app made the picture $($ref.Name) of the test frame"
                continue
            }
            $d = [XrLiveBmpDiff]::Compare($ref.FullName, $mine)
            Expect ($d[0] -ge 0 -and $d[0] -le 2.0) ("{0}: {1:N2} percent of the pixels differ by more than 24 from the PC, the mean difference is {2:N2}" -f $ref.Name, $d[0], $d[1])
        }
    }

    # THE INPUT CHANNEL (input_live=1).
    Expect ($log -match 'input        : input_live=1\. Each frame of the session writes the input of the mod') 'the app maps the input channel (input_live=1)'
    Expect ($log -match 'the session has the actions of the mod') 'the session has the actions of the mod'
    Expect ($report -match 'call xrLocateViews eyes -0\.0320 0\.0320 fov -0\.9425 0\.8203 0\.8901 -0\.9599') 'the app locates the views of the eyes (xrLocateViews)'
    $dump = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'bwvr-input.chan')) {
        $dump = (& $InputWriter dump (Join-Path $Out 'bwvr-input.chan') | Out-String)
    }
    Set-Content -LiteralPath (Join-Path $Out 'input-dump.txt') -Value $dump -Encoding utf8
    Expect ($dump -match 'head whole, 4096 bytes, version 1') 'the channel file has a whole head'
    $slotLine = [regex]::Match($dump, 'slot seq (\d+) frame (\d+) flags 0x([0-9A-F]+) time (\d+) period (\d+) recenters (\d+) size (\d+) (\d+)')
    Expect ($slotLine.Success -and [int]$slotLine.Groups[2].Value -gt 100 -and ([int]$slotLine.Groups[1].Value % 2) -eq 0) ("the app wrote the slot at each frame (" + $(if ($slotLine.Success) { "$($slotLine.Groups[2].Value) slots, the sequence $($slotLine.Groups[1].Value) is even" } else { 'no slot' }) + ')')
    Expect ($slotLine.Success -and $slotLine.Groups[5].Value -eq '13888888' -and [int64]$slotLine.Groups[4].Value -gt 0) ("the slot has the display time of the runtime, period 13888888 ns (" + $(if ($slotLine.Success) { $slotLine.Groups[5].Value } else { '' }) + ')')
    Expect ($slotLine.Success -and $slotLine.Groups[6].Value -eq '1' -and $log -match 'the LOCAL space recentered \(1 recenters\)') 'the recenter of the runtime is in the slot (1 recenter)'
    Expect ($slotLine.Success -and $slotLine.Groups[7].Value -eq '2064' -and $slotLine.Groups[8].Value -eq '2208') 'the slot has the size of the eyes that the headset asks for'
    Expect ($dump -match 'eye 0 valid 1 position -0\.0320 0\.0000 0\.0000 orientation 0\.0000 0\.0000 0\.0000 1\.0000 fov -0\.9425 0\.8203 0\.8901 -0\.9599' -and
            $dump -match 'eye 1 valid 1 position 0\.0320 0\.0000 0\.0000 orientation 0\.0000 0\.0000 0\.0000 1\.0000 fov -0\.9425 0\.8203 0\.8901 -0\.9599') 'the views of the runtime are in the slot, as the runtime gave them'
    Expect ($slotLine.Success -and ([Convert]::ToInt32($slotLine.Groups[3].Value, 16) -band 1) -eq 0 -and $dump -match 'hand 1 flags 0x000 ') 'the last slot, at the end of the session, has no focus and no active hand'
    # The poses of the right hand of the script: each point of the report
    # gives the aim pose of the runtime, and the slots in the log have it.
    $poses = @()
    foreach ($m in [regex]::Matches($report, 'frame \d+ point 1 \d+ \d+ \d+ pose (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)')) {
        $poses += , @(1..7 | ForEach-Object { [double]$m.Groups[$_].Value })
    }
    $slots = @([regex]::Matches($log, 'input        : slot (\d+), focus 1, .* the right hand flags 0x([0-9A-F]+) aim (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) trigger ([0-9.]+), profile "([^"]*)"'))
    $matched = 0
    foreach ($s in $slots) {
        $v = @(3..9 | ForEach-Object { [double]$s.Groups[$_].Value })
        foreach ($p in $poses) {
            $near = $true
            for ($i = 0; $i -lt 7; ++$i) { if ([math]::Abs($v[$i] - $p[$i]) -gt 0.0011) { $near = $false } }
            if ($near) { $matched++; break }
        }
    }
    Expect ($poses.Count -ge 3 -and $matched -ge 1) "the aim poses of the right hand of the script are in the slots ($($poses.Count) points of the script, $matched of $($slots.Count) slots in the log with one of them)"
    Expect (@($slots | Where-Object { $_.Groups[11].Value -eq 'oculus/touch_controller' -and ([Convert]::ToInt32($_.Groups[2].Value, 16) -band 0x1F) -eq 0x1F }).Count -ge 1) 'a slot has the right hand active with the aim and the grip valid and tracked, and the profile oculus/touch_controller'
    Expect ($log -match "input        : input_live_test_pulse: a pulse of 80 ms at 0\.50 for the right hand goes into the ring at slot $(100 + $T)" -and
            $log -match 'input        : a pulse of the mod, 80 ms at 0\.50 for the right hand' -and
            $report -match 'pulse /user/hand/right amplitude 0\.50 duration 80000000') 'a pulse of the ring of the events reaches the runtime, 80 ms at 0.50'

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
    $copies = [regex]::Match($log, 'xr           : (\d+) copies into the layers, (\d+) skipped because the picture did not change \(xr_copy_on_change=1\)')
    Expect ($copies.Success -and [int64]$copies.Groups[1].Value -gt 0 -and [int64]$copies.Groups[2].Value -gt 0) ("the app copied a layer only when its picture changed (" + $(if ($copies.Success) { $copies.Groups[1].Value + ' copies, ' + $copies.Groups[2].Value + ' skipped' } else { 'no line in the log' }) + ')')

    # The pictures as PNG files, for a look.
    Add-Type -AssemblyName System.Drawing
    foreach ($bmp in Get-ChildItem -LiteralPath $Out -Filter '*.bmp') {
        $image = [System.Drawing.Image]::FromFile($bmp.FullName)
        try { $image.Save([IO.Path]::ChangeExtension($bmp.FullName, '.png'), [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $image.Dispose() }
    }
} catch {
    # A fault of the script is a failed check, so that a test that stops
    # early does not end with "0 failed".
    Expect $false ('the script ran to its end: ' + $_.Exception.Message +
        ' (line ' + $_.InvocationInfo.ScriptLineNumber + ')')
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
    Unlock-AndroidEmulator
}

Write-Host ('== ' + $checks + ' checks, ' + $faults.Count + ' failed. The results are in ' + $Out) -ForegroundColor Cyan
if ($faults.Count -gt 0) { exit 1 }
