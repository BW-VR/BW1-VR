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

    When the feed has the records of the projection layer of a desk run
    with xr_input=1 (kView, docs/android.md, "The projection layer", for
    example a run of input_channel_run.ps1 -Kind channel -Live), the app
    shows the pictures in a projection layer (stream_projection=1). The test
    then checks the projection layer in place of the stereo screen: its two
    swapchains, the pose, the angles of view and the rect of each eye that
    the runtime reports against the records kView of the feed, the pictures
    of both eyes that the runtime read back, and a copy for each new
    picture.

.PARAMETER FeedMs
    With -Live: the time between two frames of the feed, 14 by default. 0:
    no wait, so the feed is faster than the session, and the test checks
    that the app skipped the walks of frames.

.PARAMETER WindowFrom
    With -Live: a test of the cut scene window of the app. The feed is a copy
    of ring-capture.bin in which each frame from this frame to -WindowTo is a
    frame of the window as a screen (bwvr_stream_reader window-feed: kViewFlat and a
    kWindow of the test, with a crop of 120 and an edge of 2 columns). The
    test checks the two quads of the eyes of the window too. The values of
    the window are of the test, not of a run of the mod.

.PARAMETER NoLayers
    The app has stream_layers=0: no quad layer of the mod. The test checks
    that the runtime has no quad but the menu and the cursor, also in the
    frames of the projection layer (no stereo screen over it).

.PARAMETER NoProjection
    The app has stream_projection=0: each picture shows on the stereo screen,
    a frame of the cut scene window too, and no quad layer of the mod goes
    out. The test checks that the app had no frame with the projection layer
    and no frame with the cut scene window, the stereo screen in each frame
    with a picture, and that each line of the quads of the runtime has the two
    quads of the stereo screen, the menu and the cursor, and no other quad.

.PARAMETER QuadFrames
    With a feed with cards: the runtime reads back the quads of the mod in
    each of its first 60 frames (the word quads), and the test checks that
    they have two different pictures of the cards or more, each with the
    bytes of a new picture that the app logs. For a feed over a fade.

.PARAMETER NoScreen
    The app has stream_screen=0: no screen quad. With a feed of the screens
    with no scene (a desk run of screens_run.ps1), the test checks that the
    runtime has no quad but the menu and the cursor, and no projection layer.

    With such a feed and stream_screen=1 (the default) the test checks the
    screen quad (docs/android.md, "The screens with no scene"): the log of
    the app, each quad of the runtime against a record kScreen of the feed
    (bwvr_stream_reader screens), no stereo screen and no projection layer,
    the pictures of the screen frames of the feed that the app made
    (vk_stream_screen_test) against the pictures of the PC, and the picture
    of the quad that the runtime read back at the end against the picture of
    the PC of the last frame of the feed.

    A feed of screen frames and then frames of the scene with a projection
    layer (screens_run.ps1 over the end of the Lionhead logo): the test checks
    the projection layer of the scene, and the switch: the screen quad first,
    then the projection layer, the lines "the frames of the scene start" and
    "frames come" of the log, and no screen quad and no stereo screen in the
    lines of the quads of the runtime after its first projection layer. The
    runtime reads back the eye pictures at the end of the script, when the
    feed is at its frames of the scene.

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
    [int]$FeedMs = 14,
    [int]$WindowFrom = 0,
    [int]$WindowTo = 0,
    [switch]$NoLayers,
    [switch]$QuadFrames,
    [switch]$NoProjection,
    [switch]$NoScreen
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
$StreamReader = Join-Path $Bwvr 'build\test\stream_reader\Release\bwvr_stream_reader.exe'
# A run with a stream keeps its results in a folder of its own.
$Out = Join-Path $Bwvr $(if ($Live -and $WindowFrom -gt 0) { 'out\android\emu-xr-window-test' } elseif ($Live) { 'out\android\emu-xr-live-test' } elseif ($Stream) { 'out\android\emu-xr-stream-test' } else { 'out\android\emu-xr-test' })
$Feed = $null
if ($Live) {
    $Feed = Join-Path $Live 'ring-capture.bin'
    if (-not (Test-Path -LiteralPath $Feed)) { Stop-Android "There is no feed at:`n$Feed" }
    if ($WindowFrom -gt 0) {
        # The feed of the test of the cut scene window, beside the folder of
        # the results. It holds the pixels of the game.
        $windowFeed = Join-Path $Bwvr 'out\android\emu-xr-window-feed.bin'
        $made = & $StreamReader window-feed $Feed $windowFeed $WindowFrom $WindowTo 120 2 2>&1 | Out-String
        Write-Host ('   ' + $made.Trim())
        if ($LASTEXITCODE -ne 0) { Stop-Android "The feed of the cut scene window was not made:`n$made" }
        $Feed = $windowFeed
    }
}
# The renderer draws on the device of the session with a stream or a live
# stream, and then the stereo screen is there.
$Screen = [bool]$Stream -or [bool]$Live
# THE PROJECTION LAYER: a feed or a stream with records kView for a
# projection layer (bwvr_stream_reader views) shows in a projection layer,
# and not on the stereo screen.
$Projection = $false
$feedViews = @()
# The newest frame of the feed is a frame of the cut scene window. The app
# then shows its pictures on the quads of the window, and the runtime reads
# back no eye of a projection layer.
$lastFlat = $false
$viewSource = $(if ($Live) { $Feed } elseif ($Stream) { $Stream } else { $null })
if ($viewSource) {
    if (-not (Test-Path -LiteralPath $StreamReader)) { Stop-Android "There is no test reader of the live stream at:`n$StreamReader`nRun .\dev.ps1 build first." }
    $viewLines = @(& $StreamReader views $viewSource 2>&1 | ForEach-Object { "$_" })
    $summary = $viewLines | Where-Object { $_ -match 'records of the projection layer in .*, (\d+) for a projection layer' } | Select-Object -Last 1
    if ($summary -and [regex]::Match($summary, ', (\d+) for a projection layer').Groups[1].Value -ne '0') { $Projection = $true }
    foreach ($l in $viewLines) {
        $m = [regex]::Match($l, '^views        : frame (\d+) flags 0x(\S+) input \d+ time -?\d+ size \S+ half \S+ (eye 0 pose .* fov \S+ \S+ \S+ \S+) (eye 1 pose .* fov \S+ \S+ \S+ \S+)$')
        if ($m.Success) {
            $feedViews += , @($m.Groups[1].Value, $m.Groups[3].Value, $m.Groups[4].Value)
            # kViewFlat (8): a frame of the cut scene window.
            $lastFlat = ([Convert]::ToInt32($m.Groups[2].Value, 16) -band 8) -ne 0
        }
    }
}
# A feed of the cut scene window alone (each kView is of a frame of the
# window): no picture goes out in a projection layer, and the checks of the
# projection layer do not apply. The quad layers are checked.
$AllWindow = $false
if (-not $Projection -and $lastFlat -and $feedViews.Count -gt 0) { $Projection = $true; $AllWindow = $true }
# stream_projection=0 (-NoProjection): each picture on the stereo screen.
if ($NoProjection) { $Projection = $false; $AllWindow = $false }
# THE QUAD LAYERS: a feed or a stream with the records of the quad layers of
# the mod (kCard, kPanel, kWindow, bwvr_stream_reader layers) shows them over
# the projection layer. The reader gives the quads of each frame as the test
# runtime reports them, and the pictures of the cards (card-*.bmp).
$Layers = $false
$feedQuads = @{}
$feedCards = @{}
$feedNames = @{}
$cardFrames = @{}
$cardBmps = $null
$feedWindow = $false
if ($viewSource -and $Projection) {
    $cardBmps = Join-Path $Bwvr $(if ($Live) { 'out\android\emu-xr-live-cards' } else { 'out\android\emu-xr-stream-cards' })
    if (Test-Path -LiteralPath $cardBmps) { Remove-Item -LiteralPath $cardBmps -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $cardBmps | Out-Null
    $layerLines = @(& $StreamReader layers $viewSource - $cardBmps 2>&1 | ForEach-Object { "$_" })
    $layerSummary = $layerLines | Where-Object { $_ -match '^layers       : \d+ frames in .*, (\d+) with layers' } | Select-Object -Last 1
    if ($layerSummary -and [regex]::Match($layerSummary, ', (\d+) with layers').Groups[1].Value -ne '0') { $Layers = $true }
    # The feed has frames of the cut scene window as a screen.
    foreach ($l in $layerLines) {
        $m = [regex]::Match($l, '^layers       : frame (\d+) quads(.*)$')
        if ($m.Success) { $feedQuads[$m.Groups[1].Value] = $m.Groups[2].Value }
        $m = [regex]::Match($l, '^layers       : frame (\d+) cards (.*)$')
        if ($m.Success) { $feedCards[$m.Groups[1].Value] = $m.Groups[2].Value }
        $m = [regex]::Match($l, '^layers       : frame (\d+): (.*), \d+ pictures')
        if ($m.Success) { $feedNames[$m.Groups[1].Value] = $m.Groups[2].Value }
        $m = [regex]::Match($l, '^layers       : (.*) is in (\d+) frames$')
        if ($m.Success) { $cardFrames[$m.Groups[1].Value] = [int]$m.Groups[2].Value }
    }
    $feedWindow = @($feedNames.Values | Where-Object { $_ -match '^eye 0, eye 1' }).Count -gt 0
}
# THE SCREENS WITH NO SCENE: a feed or a stream with records kScreen
# (bwvr_stream_reader screens). With no kView for a projection layer the app
# shows its pictures on the screen quad and makes no stereo screen. The
# reader gives each quad as the test runtime reports it.
$Screens = $false
$feedScreens = @{}
$feedScreenFrames = @()
$lastScreenFrame = 0
if ($viewSource) {
    $screenLines = @(& $StreamReader screens $viewSource 2>&1 | ForEach-Object { "$_" })
    foreach ($l in $screenLines) {
        $m = [regex]::Match($l, '^screens      : frame (\d+), screen \d+, [^,]+, \d+x\d+, \d+ walks in its frame, (quad .* eye \d+ flags \d+)$')
        if ($m.Success) {
            $feedScreens[$m.Groups[1].Value] = ' ' + $m.Groups[2].Value
            $feedScreenFrames += [int64]$m.Groups[1].Value
        }
    }
    $screenSummary = $screenLines | Where-Object { $_ -match '^screens      : .* has \d+ screen frames and \d+ frames of the scene, (\d+) records kScreen' } | Select-Object -Last 1
    if ($screenSummary) {
        Write-Host ('   ' + $screenSummary)
        if ([regex]::Match($screenSummary, ', (\d+) records kScreen').Groups[1].Value -ne '0') { $Screens = $true }
    }
    if ($feedScreenFrames.Count -gt 0) { $lastScreenFrame = ($feedScreenFrames | Measure-Object -Maximum).Maximum }
}
# The feed has screens and no picture of the world in a projection layer.
$ScreensOnly = $Screens -and -not $Projection
# The feed has screens and then frames of the scene with a projection layer
# (the switch at the end of the Lionhead logo, screens_run.ps1).
$ScreensThenWorld = $Screens -and $Projection
# The quads of the stereo screen in the lines "layers" (3 decimals) and
# "quads" (4 decimals and the turn) of the report of the runtime.
$stereoAny = 'quad 0\.000 0\.100 -2\.600 size 2\.600|quad 0\.0000 0\.1000 -2\.6000 turn [-0-9. ]+ size 2\.6000'
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

    # The script of the controllers. The rows of the menu picture are 68
    # pixels high from y = 190 (menu_layout.h), so the middle of row r is at
    # 224 + 68 r. The layer "menu" of the runtime is the menu quad: the last
    # quad of 1600 by 1200 pixels, over the stereo screen and the quad layers
    # of the mod.
    $script = Join-Path $Out 'script.txt'
    $L = 'menu'
    # The live stream makes the stereo screen at its first picture, after the
    # first frames of the session: its script starts 200 frames later.
    $T = $(if ($Live) { 200 } else { 0 })
    Set-Content -LiteralPath $script -Encoding ascii -Value @(
        "$(30 + $T) recenter",
        "$(40 + $T) capture start",
        "$(50 + $T) point right $L 800 632",
        "$(60 + $T) capture point",
        "$(70 + $T) trigger right 1", "$(74 + $T) trigger right 0",
        "$(90 + $T) point right $L 400 227",
        "$(100 + $T) trigger right 1", "$(104 + $T) trigger right 0",
        "$(110 + $T) capture moving",
        "$(120 + $T) button right 1", "$(124 + $T) button right 0",
        "$(140 + $T) point right $L 800 836",
        "$(150 + $T) trigger right 1", "$(154 + $T) trigger right 0",
        "$(170 + $T) capture saved",
        "$(200 + $T) exit")
    # The pictures of the eyes of the projection layer, one time: an image of
    # the size of the render takes a long time to read back in the emulator.
    # A feed with screens first has the projection layer only at its end.
    if ($Projection -and -not $lastFlat) { Add-Content -LiteralPath $script -Encoding ascii -Value "$($(if ($ScreensThenWorld) { 190 } else { 40 }) + $T) eyes start" }
    # The quads of the mod in each of the first frames, while the pictures of
    # a fade come (-QuadFrames).
    if ($QuadFrames) { Add-Content -LiteralPath $script -Encoding ascii -Value @(1..60 | ForEach-Object { "$_ quads cards$_" }) }
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
    if ($NoLayers) { $iniLines += 'stream_layers=0' }
    if ($NoProjection) { $iniLines += 'stream_projection=0' }
    if ($NoScreen) { $iniLines += 'stream_screen=0' }
    Set-Content -LiteralPath $ini -Encoding ascii -Value $iniLines
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    if ($Stream) { Adb @('push', $Stream, "$AppDir/game/bwvr-stream.bin") }
    if ($Live) { Adb @('push', $Feed, "$AppDir/game/bwvr-stream-feed.bin") }

    # 4. The run.
    Say 'Start the app with the test runtime'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    # The read back of an eye of the projection layer (the size of the render)
    # takes minutes in the emulator.
    $deadline = (Get-Date).AddSeconds($(if ($Projection) { 1200 } elseif ($Live) { 600 } elseif ($Stream) { 400 } else { 120 }))
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
    # The menu quad is the quad of 1600 by 1200 pixels, over the stereo screen
    # and the quad layers of the mod.
    foreach ($name in @('start', 'point', 'moving', 'saved')) {
        $m = [regex]::Match($report, "capture \S*/$name-\d+\.bmp 1600x1200 lit (\d+)")
        Expect ($m.Success -and [int]$m.Groups[1].Value -gt 100000) "the picture $name of the menu quad has the menu ($($m.Groups[1].Value) lit pixels)"
    }
    if ($Screen) {
        Expect ($log -match 'xr           : the GPU of the runtime: .*The renderer of the mod: yes') 'the device of the session has Vulkan 1.3 and the features of the renderer'
        Expect ($log -match 'vulkan       : the Vulkan of the OpenXR session') 'the renderer drew on the device of the session'
        if ($Stream) {
            Expect ($log -match 'replay       : .*: \d+ records, \d+ frames, [1-9]\d* walks') 'the app played the stream'
        }
        if ($ScreensOnly) {
            # THE SCREEN QUAD of the frames with no scene: no stereo screen and
            # no projection layer.
            $menuQuadS = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 1600x1200 eye 0 flags 2'
            $cursorQuadS = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 64x64 eye 0 flags 2'
            $quadLinesS = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
            $modQuads = @($quadLinesS | ForEach-Object { [regex]::Replace($_.Groups[2].Value, "($menuQuadS)($cursorQuadS)?$", '') })
            $stereoLinesS = ([regex]::Matches($report, $stereoAny)).Count
            Expect ($log -notmatch 'a stereo screen of two quads' -and $stereoLinesS -eq 0) "the app made no stereo screen ($stereoLinesS lines of the runtime with its quads)"
            Expect ($report -notmatch '(?m)^frame \d+ projection ') 'the runtime had no projection layer'
            Expect ($feedScreens.Count -gt 0) "the feed has $($feedScreens.Count) records kScreen"
            if ($NoScreen) {
                Expect ($log -match 'xr           : stream_screen=0\. No screen quad') 'the app has stream_screen=0'
                $extraS = @($modQuads | Where-Object { $_ -ne '' })
                Expect ($quadLinesS.Count -gt 0 -and $extraS.Count -eq 0) "each line of the quads of the runtime has the menu and the cursor alone ($($quadLinesS.Count) lines, $($extraS.Count) with another quad)"
            } else {
                Expect ($log -match 'xr           : stream_screen=1\. A frame with no scene of the mod shows on a quad fixed in the room') 'the app has stream_screen=1'
                $chainS = [regex]::Match($log, 'xr           : a swapchain of (\d+) by (\d+) for the screen quad')
                Expect $chainS.Success ("the app made the swapchain of the screen quad" + $(if ($chainS.Success) { ", $($chainS.Groups[1].Value) by $($chainS.Groups[2].Value)" } else { '' }))
                $shownS = [regex]::Match($log, 'xr           : the screen quad shows screen (\d+) of the mod \(([^,]+), (\d+) by (\d+)\) as one quad layer, ([0-9.]+) by ([0-9.]+) m at ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) m')
                Expect $shownS.Success ("the log says that the screen quad shows a screen of the mod" + $(if ($shownS.Success) { ": screen $($shownS.Groups[1].Value), $($shownS.Groups[2].Value), $($shownS.Groups[5].Value) by $($shownS.Groups[6].Value) m at $($shownS.Groups[7].Value) $($shownS.Groups[8].Value) $($shownS.Groups[9].Value) m" } else { '' }))
                $withS = @($modQuads | Where-Object { $_ -ne '' })
                $feedQuadTexts = @($feedScreens.Values | Sort-Object -Unique)
                $knownS = @($withS | Where-Object { $feedQuadTexts -contains $_ })
                Expect ($withS.Count -gt 0 -and $knownS.Count -eq $withS.Count) "each quad of the mod that the runtime reports is the quad of a record kScreen of the feed, one opaque quad for both eyes ($($knownS.Count) of $($withS.Count) lines)"
                if ($withS.Count -gt 0) { Write-Host ('         the runtime: quads' + $withS[0]) }
                $countS = [regex]::Match($log, 'xr           : the screen quad \(stream_screen=1\): (\d+) screen pictures of the mod, (\d+) frames of the session with the screen quad, (\d+) copies of the picture of a walk, (\d+) pictures of the pixels of the CPU')
                Expect ($countS.Success -and [int]$countS.Groups[1].Value -gt 0 -and [int]$countS.Groups[2].Value -gt 0 -and ([int]$countS.Groups[3].Value + [int]$countS.Groups[4].Value) -gt 0) ("the app showed the screen pictures on the quad" + $(if ($countS.Success) { ": $($countS.Groups[1].Value) screen pictures, $($countS.Groups[2].Value) frames, $($countS.Groups[3].Value) copies of the picture of a walk, $($countS.Groups[4].Value) pictures of the pixels of the CPU" } else { ' (no line in the log)' }))
                # The picture of the quad at the end (the capture "saved") is the
                # picture of the last frame of the feed: against the picture of
                # the PC of that frame (vk_stream_screen_test).
                $lastRef = Join-Path $Live ("bwvr-vk-left-$lastScreenFrame.bmp")
                $savedS = Join-Path $Out 'saved-0.bmp'
                if ($Live -and (Test-Path -LiteralPath $lastRef)) {
                    if (Test-Path -LiteralPath $savedS) {
                        $d = [XrLiveBmpDiff]::Compare($lastRef, $savedS)
                        Expect ($d[0] -ge 0 -and $d[0] -le 2.0) ("the picture of the screen quad that the runtime read back at the end against the picture of the PC of frame {0}: {1:N2} percent of the pixels differ by more than 24, the mean difference is {2:N2}" -f $lastScreenFrame, $d[0], $d[1])
                    } else {
                        Expect $false 'the runtime read back the screen quad at the end (saved-0.bmp)'
                    }
                } else {
                    Write-Host "         the PC has no picture of the last frame $lastScreenFrame of the feed, so the picture of the quad is not compared"
                }
            }
        }
        if (-not $ScreensOnly) { Expect ($log -match 'a stereo screen of two quads') 'the app made the stereo screen' }
        if (-not $Projection -and -not $ScreensOnly) {
            # A stream with no kView: the fall back to the stereo screen, one
            # line in the log.
            if (-not $NoProjection) {
                $noView = ([regex]::Matches($log, 'xr           : stream_projection=1, but the mod gave no values of the projection layer for the picture of the renderer')).Count
                Expect ($noView -eq 1) "the log says one time that the pictures with no values of the mod show on the stereo screen ($noView lines)"
            } else {
                # stream_projection=0: no projection layer, no cut scene window,
                # no quad layer of the mod.
                Expect ($log -match 'xr           : stream_projection=0\. Each picture of the renderer shows on the stereo screen') 'the app has stream_projection=0'
                $counts = [regex]::Match($log, 'xr           : \d+ copies of the projection layer for eye 0 and \d+ for eye 1\. (\d+) frames with the projection layer, (\d+) frames with the stereo screen, (\d+) frames with the cut scene window')
                Expect ($counts.Success -and [int]$counts.Groups[1].Value -eq 0 -and [int]$counts.Groups[2].Value -gt 0 -and [int]$counts.Groups[3].Value -eq 0) ("no frame with the projection layer and no frame with the cut scene window, the stereo screen in the frames with a picture (" + $(if ($counts.Success) { "$($counts.Groups[1].Value), $($counts.Groups[2].Value) and $($counts.Groups[3].Value) frames" } else { 'no line in the log' }) + ')')
                $stereoQuads = ' quad 0\.0000 0\.1000 -2\.6000 turn 0\.0000 0\.0000 0\.0000 1\.0000 size 2\.6000 [0-9.]+ rect 0 0 \d+x\d+ eye 1 flags 0 quad 0\.0000 0\.1000 -2\.6000 turn 0\.0000 0\.0000 0\.0000 1\.0000 size 2\.6000 [0-9.]+ rect 0 0 \d+x\d+ eye 2 flags 0'
                $menuOnly = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 1600x1200 eye 0 flags 2( quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 64x64 eye 0 flags 2)?'
                $quadLinesNo = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
                $withScreen = @($quadLinesNo | Where-Object { $_.Groups[2].Value -match "^$stereoQuads$menuOnly$" })
                $otherLines = @($quadLinesNo | Where-Object { $_.Groups[2].Value -notmatch "^($stereoQuads)?$menuOnly$" })
                Expect ($withScreen.Count -gt 0 -and $otherLines.Count -eq 0) "each line of the quads of the runtime has the stereo screen, the menu and the cursor, and no quad of the mod ($($quadLinesNo.Count) lines, $($withScreen.Count) with the stereo screen, $($otherLines.Count) with another quad)"
            }
            Expect ($report -match 'quad 0\.000 0\.100 -2\.600 size 2\.600 [0-9.]+ eye 1 quad 0\.000 0\.100 -2\.600 size 2\.600 [0-9.]+ eye 2') 'the stereo screen is a quad for the left eye and a quad for the right eye, 2.6 m in front'
            foreach ($eye in 0, 1) {
                $m = [regex]::Match($report, "capture \S*/start-$eye\.bmp (\d+)x(\d+) lit (\d+)")
                Expect ($m.Success -and [int]$m.Groups[3].Value -gt 100000) "the picture of eye $eye of the stereo screen has the scene ($($m.Groups[3].Value) lit pixels)"
            }
        }
    }
    if ($Projection) {
        # THE PROJECTION LAYER of the pictures of the renderer, with the
        # values of the mod for the frame of each picture (kView).
        Expect ($feedViews.Count -gt 0) "the $(if ($Live) { 'feed' } else { 'stream' }) has $($feedViews.Count) records kView for a projection layer"
        $eyesLine = [regex]::Match($log, 'xr           : a projection layer of two swapchains of (\d+) by (\d+) for the pictures of the renderer')
        Expect $eyesLine.Success ("the app made the two swapchains of the projection layer" + $(if ($eyesLine.Success) { ", $($eyesLine.Groups[1].Value) by $($eyesLine.Groups[2].Value)" } else { '' }))
        if (-not $AllWindow) { Expect ($log -match 'xr           : the pictures of the renderer go out in a projection layer, with the values of frame \d+ of the mod') 'the pictures of the renderer go out in a projection layer' }
        $stereoLinesP = ([regex]::Matches($report, $stereoAny)).Count
        Expect ($stereoLinesP -eq 0) "no frame has the quads of the stereo screen ($stereoLinesP lines)"
        $layerLines = @([regex]::Matches($report, '(?m)^frame (\d+) projection (eye 0 pose .* fov \S+ \S+ \S+ \S+) rect (\d+) (\d+) (\d+)x(\d+) (eye 1 pose .* fov \S+ \S+ \S+ \S+) rect (\d+) (\d+) (\d+)x(\d+)\r?$'))
        $matched = 0
        $frames = @()
        foreach ($m in $layerLines) {
            foreach ($v in $feedViews) {
                if ($m.Groups[2].Value -eq $v[1] -and $m.Groups[7].Value -eq $v[2]) { $matched++; $frames += $v[0]; break }
            }
        }
        if (-not $AllWindow) { Expect ($layerLines.Count -gt 0 -and $matched -eq $layerLines.Count) ("the pose and the angles of view of each eye of each projection layer of the runtime are the values of a record kView of the feed ($matched of $($layerLines.Count) lines of the report)") }
        # THE FRAME OF EACH PICTURE. The app logs the frame of the mod of each
        # new picture of the layer. The values of the kView of those frames,
        # in their order, are the values of the layers that the runtime
        # reports, in their order (each with no repeat of the value before).
        $byFrame = @{}
        foreach ($v in $feedViews) { $byFrame[$v[0]] = $v[1] + ' ' + $v[2] }
        $shown = @([regex]::Matches($log, 'xr           : the projection layer shows the picture of frame (\d+)') | ForEach-Object { $_.Groups[1].Value })
        $want = New-Object System.Collections.Generic.List[string]
        $unknown = 0
        foreach ($f in $shown) {
            if (-not $byFrame.ContainsKey($f)) { $unknown++; continue }
            if ($want.Count -eq 0 -or $want[$want.Count - 1] -ne $byFrame[$f]) { $want.Add($byFrame[$f]) }
        }
        $got = New-Object System.Collections.Generic.List[string]
        foreach ($m in $layerLines) {
            # Not $t: PowerShell names are not case sensitive, and $T is the
            # start of the script of the controllers.
            $layerText = $m.Groups[2].Value + ' ' + $m.Groups[7].Value
            if ($got.Count -eq 0 -or $got[$got.Count - 1] -ne $layerText) { $got.Add($layerText) }
        }
        $distinct = @($got | Sort-Object -Unique).Count
        if (-not $AllWindow) { Expect ($shown.Count -gt 0 -and $unknown -eq 0 -and ($want -join ';') -eq ($got -join ';')) ("each projection layer of the runtime has the values of the kView of the frame of its picture: the app showed the pictures of $($shown.Count) frames ($($shown -join ' ')), $unknown not in the feed, the values of $($want.Count) and $($got.Count) steps in the same order") }
        # The app plays the first frame of the feed and the newest frame. A
        # feed with another pose in its last frames than in its first frames
        # gives two poses.
        # A feed with frames of the cut scene window from a frame on has fewer
        # pictures in the projection layer.
        # A feed with one pose (no step of the head) cannot tell the frames
        # apart by the pose.
        $feedPoses = @($feedViews | ForEach-Object { $_[1] + ' ' + $_[2] } | Sort-Object -Unique).Count
        if ($feedPoses -lt 2) { Write-Host "         the feed has $feedPoses pose of the head, so the poses do not tell its frames apart" }
        if ($WindowFrom -le 0 -and -not $feedWindow -and $feedPoses -ge 2) { Expect ($distinct -ge 2) "the layers of the runtime have $distinct poses, so the check above can tell the frames apart" }

        if ($layerLines.Count -gt 0) {
            $first = $layerLines[0]
            Write-Host ('         the runtime: ' + $first.Value.Trim())
        }
        $rectOk = $eyesLine.Success -and $layerLines.Count -gt 0
        foreach ($m in $layerLines) {
            if ($m.Groups[3].Value -ne '0' -or $m.Groups[4].Value -ne '0' -or $m.Groups[8].Value -ne '0' -or $m.Groups[9].Value -ne '0' -or
                $m.Groups[5].Value -ne $eyesLine.Groups[1].Value -or $m.Groups[6].Value -ne $eyesLine.Groups[2].Value -or
                $m.Groups[10].Value -ne $eyesLine.Groups[1].Value -or $m.Groups[11].Value -ne $eyesLine.Groups[2].Value) { $rectOk = $false }
        }
        if (-not $AllWindow) { Expect $rectOk 'the rect of each eye is its whole picture' }
        foreach ($eye in @($(if (-not $lastFlat) { 0, 1 }))) {
            $m = [regex]::Match($report, "capture \S*/start-p$eye\.bmp (\d+)x(\d+) lit (\d+)")
            Expect ($m.Success -and [int]$m.Groups[3].Value -gt 100000) "the picture of eye $eye of the projection layer has the scene ($($m.Groups[1].Value) by $($m.Groups[2].Value), $($m.Groups[3].Value) lit pixels)"
        }
    }
    if ($ScreensThenWorld -and -not $NoScreen) {
        # THE SWITCH FROM THE SCREENS WITH NO SCENE TO THE FRAMES OF THE SCENE
        # (a capture of screens_run.ps1 over the end of the Lionhead logo):
        # first the screen quad, then the projection layer with no screen quad
        # and no stereo screen.
        $screenAt = $log.IndexOf('xr           : the screen quad shows screen')
        $worldAt = $log.IndexOf('xr           : the pictures of the renderer go out in a projection layer')
        Expect ($screenAt -ge 0 -and $worldAt -gt $screenAt) "the app showed the screen quad first, then the projection layer (the lines at the characters $screenAt and $worldAt of the log)"
        $sceneStart = [regex]::Match($log, 'live         : the frames of the scene start at frame (\d+)')
        Expect $sceneStart.Success ("the log says that the frames of the scene start" + $(if ($sceneStart.Success) { ": frame $($sceneStart.Groups[1].Value)" } else { '' }))
        Expect ($log -match 'live         : frames come\. The first is frame 10\d{8}') 'the log says that frames come, the first a screen frame'
        $firstProjection = [regex]::Match($report, '(?m)^frame (\d+) projection ')
        $quadLinesW = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
        $screenQuadW = 'quad 0\.0000 0\.0000 -2\.0000 turn 0\.0000 0\.0000 0\.0000 1\.0000 size 2\.6000 [0-9.]+ rect 0 0 \d+x\d+ eye 0 flags 0'
        $before = @($quadLinesW | Where-Object { $firstProjection.Success -and [int]$_.Groups[1].Value -lt [int]$firstProjection.Groups[1].Value -and $_.Groups[2].Value -match $screenQuadW })
        $after = @($quadLinesW | Where-Object { $firstProjection.Success -and [int]$_.Groups[1].Value -ge [int]$firstProjection.Groups[1].Value })
        $afterScreen = @($after | Where-Object { $_.Groups[2].Value -match $screenQuadW })
        $afterStereo = @($after | Where-Object { $_.Groups[2].Value -match $stereoAny })
        Expect ($firstProjection.Success -and $before.Count -gt 0 -and $after.Count -gt 0 -and $afterScreen.Count -eq 0 -and $afterStereo.Count -eq 0) ("the runtime had the screen quad before its first projection layer (frame $($firstProjection.Groups[1].Value)), and no screen quad and no stereo screen in the lines of the quads after it ($($before.Count) lines before with the screen quad, $($after.Count) lines after, $($afterScreen.Count) of them with the screen quad, $($afterStereo.Count) with the stereo screen)")
        $lastProjection = @([regex]::Matches($report, '(?m)^frame (\d+) layers \d+ projection'))
        Expect ($lastProjection.Count -gt 0) "the runtime has frames with the projection layer after the switch ($($lastProjection.Count) lines of the layers)"
    }
    if ($Layers -and $NoLayers) {
        # stream_layers=0: the projection layer, the menu and the cursor, and
        # no other quad.
        Expect ($log -match 'xr           : stream_layers=0\. No quad layer of the mod') 'the app has stream_layers=0'
        $menuQuadNo = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 1600x1200 eye 0 flags 2'
        $cursorQuadNo = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 64x64 eye 0 flags 2'
        $allQuads = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
        $extra = @($allQuads | Where-Object { [regex]::Replace($_.Groups[2].Value, "($menuQuadNo)($cursorQuadNo)?$", '') -ne '' })
        Expect ($allQuads.Count -gt 0 -and $extra.Count -eq 0) "each line of the quads of the runtime has the menu and the cursor alone: no quad of the mod and no quad of the stereo screen over the projection layer ($($allQuads.Count) lines, $($extra.Count) with another quad)"
    }
    if ($Layers -and -not $NoLayers) {
        # THE QUAD LAYERS OF THE MOD over the projection layer. Each line
        # "quads" of the runtime, with no menu and no cursor at its end, is
        # the quads of the layers of the frame of a picture of the app, in
        # the order of the session of the mod, with the pose, the size, the
        # rect, the eye and the flags of the stream.
        Expect ($feedQuads.Count -gt 0) "the $(if ($Live) { 'feed' } else { 'stream' }) has the quad layers of $($feedQuads.Count) frames"
        $shownFrames = @([regex]::Matches($log, 'xr           : the (projection layer|cut scene window) shows the picture of frame (\d+)') | ForEach-Object { $_.Groups[2].Value })
        $quadLines = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
        $menuQuad = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 1600x1200 eye 0 flags 2'
        $cursorQuad = ' quad [-0-9.]+ [-0-9.]+ [-0-9.]+ turn [-0-9. ]+ size [0-9.]+ [0-9.]+ rect 0 0 64x64 eye 0 flags 2'
        $runtimeLayers = New-Object System.Collections.Generic.List[object]
        foreach ($m in $quadLines) {
            $text = [regex]::Replace($m.Groups[2].Value, "($menuQuad)($cursorQuad)?$", '')
            # A line of the screen quad of a screen frame of the feed is no
            # line of the quad layers of the mod (the checks of the switch).
            if ($ScreensThenWorld -and $feedScreens.Values -contains $text) { continue }
            $runtimeLayers.Add(@([int]$m.Groups[1].Value, $text))
        }
        $withLayers = @($runtimeLayers | Where-Object { $_[1] -ne '' })
        $matchedLayers = 0
        $layerFrames = @()
        foreach ($r in $withLayers) {
            # The shown frames with these quads (frames with the same quads are
            # one step).
            $hits = @($shownFrames | Where-Object { $feedQuads.ContainsKey($_) -and $feedQuads[$_] -eq $r[1] })
            if ($hits.Count -gt 0) { $matchedLayers++; $layerFrames += ($hits -join '/') }
        }
        Expect ($withLayers.Count -gt 0 -and $matchedLayers -eq $withLayers.Count) ("each line of the quads of the runtime has the quads of the layers of the frame of a picture of the app, in the order of the session of the mod ($matchedLayers of $($withLayers.Count) lines, the frames $($layerFrames -join ' '))")
        # The order of the pictures: the quads of the shown frames with
        # layers, with no repeat, are the quads of the runtime in their order.
        $wantQuads = New-Object System.Collections.Generic.List[string]
        foreach ($f in $shownFrames) {
            $q = $(if ($feedQuads.ContainsKey($f)) { $feedQuads[$f] } else { '' })
            if ($q -ne '' -and ($wantQuads.Count -eq 0 -or $wantQuads[$wantQuads.Count - 1] -ne $q)) { $wantQuads.Add($q) }
        }
        $gotQuads = New-Object System.Collections.Generic.List[string]
        foreach ($r in $withLayers) {
            if ($gotQuads.Count -eq 0 -or $gotQuads[$gotQuads.Count - 1] -ne $r[1]) { $gotQuads.Add($r[1]) }
        }
        Expect (($wantQuads -join ';') -eq ($gotQuads -join ';')) "the quads of the runtime come in the order of the pictures of the app ($($wantQuads.Count) and $($gotQuads.Count) steps)"
        if ($withLayers.Count -gt 0) {
            Write-Host ('         the runtime: frame ' + $withLayers[0][0] + ' quads' + $withLayers[0][1])
            $f0 = ($layerFrames[0] -split '/')[-1]
            if ($f0 -and $feedNames.ContainsKey($f0)) { Write-Host ("         the layers of frame $($f0): " + $feedNames[$f0]) }
        }
        foreach ($name in $cardFrames.Keys) {
            Expect ($log -match ('xr           : ' + [regex]::Escape($name) + ' goes out as one quad layer over the projection layer')) "the app shows $name as a quad layer (the feed has it in $($cardFrames[$name]) frames)"
        }
        if ($feedNames.Values | Where-Object { $_ -match 'panel \d' }) {
            Expect ($log -match 'xr           : the panel quads of the mod go out as \d+ quad layer') 'the app shows the panel quads of the mod with the panel image of the renderer'
        }
        # THE PICTURES OF THE CARDS that the runtime read back (the word
        # capture of the script): the rect of each quad of a card has the
        # pixels of the picture of that card in the stream, to the byte.
        Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class CardBmp {
  // The pixels of rect x, y, w, h of a BMP of 32 bits (bottom row first) of
  // a swapchain against a BMP of w by h of the stream. Gives the count of
  // the bytes that differ, or -1 for a wrong size.
  public static long Compare(string capture, int x, int y, int w, int h, string card) {
    byte[] a = File.ReadAllBytes(capture), b = File.ReadAllBytes(card);
    int wa = BitConverter.ToInt32(a, 18), ha = Math.Abs(BitConverter.ToInt32(a, 22));
    int wb = BitConverter.ToInt32(b, 18), hb = Math.Abs(BitConverter.ToInt32(b, 22));
    if (wb != w || hb != h || x + w > wa || y + h > ha) return -1;
    int oa = BitConverter.ToInt32(a, 10), ob = BitConverter.ToInt32(b, 10);
    long differ = 0;
    for (int row = 0; row < h; ++row) {
      // Row r from the top is row (height - 1 - r) of the file.
      int ra = oa + (ha - 1 - (y + row)) * wa * 4 + x * 4;
      int rb = ob + (hb - 1 - row) * wb * 4;
      for (int i = 0; i < w * 4; ++i) if (a[ra + i] != b[rb + i]) ++differ;
    }
    return differ;
  }
}
'@
        $captureFrames = @([regex]::Matches($report, '(?m)^frame (\d+) capture \S*/(start|point|moving|saved|cards\d+)-0\.bmp') | ForEach-Object { , @([int]$_.Groups[1].Value, $_.Groups[2].Value) })
        $cardChecks = 0
        $cardSame = 0
        $cardText = @()
        # THE NEW PICTURES OF THE CARDS that the app logs, with the hash of the
        # stream. Each one is a picture of that card in the feed. A readback
        # has the bytes of one of them: the captures of the runtime have no
        # frame of the mod, and a fade changes only the pixels of a card.
        $cardIndex = @{ 'the hint card of the hand that points' = '0'; 'the hint card of the other hand' = '1'; 'the subtitle card' = '2'; 'the ring' = '3' }
        $newPictures = @{}
        foreach ($m in [regex]::Matches($log, 'xr           : (.+?) gets a new picture ([0-9a-f]{16}), (\d+) by (\d+)')) {
            $ci = $cardIndex[$m.Groups[1].Value]
            if (-not $newPictures.ContainsKey($ci)) { $newPictures[$ci] = New-Object System.Collections.Generic.List[string] }
            $newPictures[$ci].Add($m.Groups[2].Value)
        }
        $logged = 0
        $inFeed = 0
        foreach ($ci in $newPictures.Keys) {
            foreach ($h in $newPictures[$ci]) { $logged++; if (Test-Path -LiteralPath (Join-Path $cardBmps "card-$ci-$h.bmp")) { $inFeed++ } }
        }
        if ($cardFrames.Count -gt 0) { Expect ($logged -gt 0 -and $inFeed -eq $logged) "each new picture of a card that the app logs is a picture of that card in the feed ($inFeed of $logged)" }
        $matched = @{}
        foreach ($cap in $captureFrames) {
            # The quads of the runtime at the frame of the capture.
            $at = $null
            foreach ($m in $quadLines) { if ([int]$m.Groups[1].Value -le $cap[0]) { $at = $m } }
            if (-not $at) { continue }
            $atText = [regex]::Replace($at.Groups[2].Value, "($menuQuad)($cursorQuad)?$", '')
            $frame = $null
            foreach ($f in $shownFrames) { if ($feedQuads.ContainsKey($f) -and $feedQuads[$f] -eq $atText) { $frame = $f } }
            if (-not $frame -or -not $feedNames.ContainsKey($frame) -or -not $feedCards.ContainsKey($frame)) { continue }
            $names = @($feedNames[$frame] -split ', ' | Where-Object { $_ -notmatch '^projection' })
            $rects = @([regex]::Matches($atText, 'rect (\d+) (\d+) (\d+)x(\d+)'))
            $hashes = @{}
            foreach ($p in ($feedCards[$frame].Trim() -split ' ')) { $kv = $p -split ':'; $hashes[$kv[0]] = $kv[1] }
            for ($i = 0; $i -lt $names.Count -and $i -lt $rects.Count; ++$i) {
                $nm = [regex]::Match($names[$i], '^card (\d)$')
                if (-not $nm.Success) { continue }
                $c = $nm.Groups[1].Value
                $bmp = Join-Path $Out "$($cap[1])-$i.bmp"
                $candidates = @($(if ($newPictures.ContainsKey($c)) { $newPictures[$c] } else { $hashes[$c] }))
                # The app logs the first 20 new pictures of a card. After them each
                # picture of that card in the feed counts.
                if ($newPictures.ContainsKey($c) -and $newPictures[$c].Count -ge 20) {
                    $candidates += @(Get-ChildItem -LiteralPath $cardBmps -Filter "card-$c-*.bmp" | ForEach-Object { $_.BaseName.Substring("card-$c-".Length) })
                }
                if (-not (Test-Path -LiteralPath $bmp)) { $cardChecks++; $cardText += "$($cap[1]) quad $i card $($c): no picture"; continue }
                $r = $rects[$i]
                $hit = $null
                foreach ($h in $candidates) {
                    $card = Join-Path $cardBmps "card-$c-$h.bmp"
                    if (-not (Test-Path -LiteralPath $card)) { continue }
                    $d = [CardBmp]::Compare($bmp, [int]$r.Groups[1].Value, [int]$r.Groups[2].Value, [int]$r.Groups[3].Value, [int]$r.Groups[4].Value, $card)
                    if ($d -eq 0) { $hit = $h; break }
                }
                $cardChecks++
                if ($hit) { $cardSame++; $matched["$c-$hit"] = $true }
                $cardText += "$($cap[1]) quad $i card $($c): $(if ($hit) { "the same bytes as the picture $hit" } else { "FAULT the bytes of none of the $($candidates.Count) pictures" })"
            }
        }
        foreach ($l in $cardText) { Write-Host "         $l" }
        if ($QuadFrames) { Expect ($matched.Count -ge 2) "the readbacks of the quads of the first frames have $($matched.Count) different pictures of the cards, each with the bytes of a new picture that the app logs" }
        if ($feedWindow -and $cardChecks -eq 0) { Write-Host '         no picture of a card: each capture of the runtime has a frame of the cut scene window' }
        elseif ($cardFrames.Count -eq 0) { Write-Host '         the feed has no card, only other quad layers of the mod, so no picture of a card is compared' }
        else { Expect ($cardChecks -gt 0 -and $cardSame -eq $cardChecks) "the pictures of the cards that the runtime read back have the pixels of the pictures of the mod in the stream ($cardSame of $cardChecks the same bytes, $($matched.Count) different pictures, of $logged new pictures in the log of the app)" }
        if ($feedWindow) {
            # THE CUT SCENE WINDOW: a quad of each eye of the swapchains of the
            # eyes at the pose of the window, each eye with its columns, and no
            # other layer of the mod but the subtitle card. The quads are the
            # quads of the feed (the check of the lines of the quads above).
            Expect ($log -match 'xr           : the cut scene window goes out as two quad layers') 'the app shows a frame of the cut scene window as two quad layers'
            $windowShown = @([regex]::Matches($log, 'xr           : the cut scene window shows the picture of frame (\d+)') | ForEach-Object { [int]$_.Groups[1].Value })
            $notFlat = @($windowShown | Where-Object { -not $feedNames.ContainsKey("$_") -or $feedNames["$_"] -notmatch '^eye 0, eye 1' })
            Expect ($windowShown.Count -gt 0 -and $notFlat.Count -eq 0) "the app showed the pictures of the frames $($windowShown -join ' ') in the cut scene window, each a frame of the window in the feed"
            if ($WindowFrom -gt 0) {
            $eyesW = [int]$eyesLine.Groups[1].Value
            $eyesH = [int]$eyesLine.Groups[2].Value
            $wantEyes = " quad 0.0000 0.1000 -2.0000 turn 0.0000 0.0000 0.0000 1.0000 size 2.6000 \S+ rect 122 0 $($eyesW - 124)x$eyesH eye 1 flags 0 quad 0.0000 0.1000 -2.0000 turn 0.0000 0.0000 0.0000 1.0000 size 2.6000 \S+ rect 2 0 $($eyesW - 124)x$eyesH eye 2 flags 0"
            $windowLines = @($runtimeLayers | Where-Object { $_[1] -match "^$wantEyes$" })
            Expect ($windowLines.Count -gt 0) "the runtime has the quads of the window: the left eye from column 122 and the right eye from column 2, $($eyesW - 124) columns each, 2.6 m wide, 2 m in front ($($windowLines.Count) lines)"
            }
            $wallLines = @($runtimeLayers | Where-Object { $_[1] -match 'rect 0 0 16x16 eye 0 flags 2' })
            if ($feedNames.Values -match 'wall 0') {
                Expect ($wallLines.Count -gt 0 -and $log -match 'xr           : the walls of the cut scene window go out as four quad layers') "the runtime has the four walls of the window over the projection layer ($($wallLines.Count) lines)"
            }
        }
        $newPixels = [regex]::Match($log, 'xr           : the cards got new pixels (\d+), (\d+), (\d+) and (\d+) times')
        Expect $newPixels.Success ("the app logs the new pixels of each card" + $(if ($newPixels.Success) { ": $($newPixels.Groups[1].Value), $($newPixels.Groups[2].Value), $($newPixels.Groups[3].Value) and $($newPixels.Groups[4].Value) times, for the pictures that the app showed (xr_copy_on_change=1)" } else { '' }))
    }
    if ($Live) {
        Expect ($log -match "live         : stream_live_feed=1\. The process \d+ writes the \d+ frames .* one each $FeedMs ms") "a child process of the app writes the frames of the feed into the ring, one each $FeedMs ms"
        Expect ($log -match 'live         : the writer answered the ask') 'the writer answered the ask for the whole state'
        Expect ($log -match 'live         : frames come\. The first is frame \d+, with the whole state') 'the first frame has the whole state'
        $liveLine = [regex]::Match($log, 'live         : (no new frame for 1 s|the app ends): (\d+) frames played and (\d+) skipped \(frames (\d+) to (\d+)\), (\d+) pictures\. In all (\d+) records, (\d+) walks, (\d+) draws, (\d+) surfaces, (\d+) of an unknown kind([^.]*)\.')
        Expect ($liveLine.Success -and [int]$liveLine.Groups[2].Value -gt 0 -and [int]$liveLine.Groups[8].Value -gt 0 -and [int]$liveLine.Groups[11].Value -eq 0 -and $liveLine.Groups[12].Value -eq '') ("the app played the live stream" + $(if ($liveLine.Success) { ": $($liveLine.Groups[2].Value) frames played and $($liveLine.Groups[3].Value) skipped, frames $($liveLine.Groups[4].Value) to $($liveLine.Groups[5].Value), $($liveLine.Groups[6].Value) pictures, $($liveLine.Groups[8].Value) walks, 0 of an unknown kind" } else { '' }))
        $screenLine = $(if ($Projection) { [regex]::Match($log, 'xr           : (\d+) copies of the projection layer for eye 0 and (\d+) for eye 1') } else { [regex]::Match($log, 'xr           : (\d+) copies of the stereo screen for eye 0 and (\d+) for eye 1') })
        $pictures = $(if ($liveLine.Success) { [int]$liveLine.Groups[6].Value } else { 0 })
        if ($FeedMs -eq 0 -and $liveLine.Success) {
            Expect ([int]$liveLine.Groups[3].Value -gt 0) "a feed faster than the session: the app skipped the walks of $($liveLine.Groups[3].Value) frames and played $($liveLine.Groups[2].Value) frames whole"
        }
        $what = $(if ($Projection) { 'the projection layer' } else { 'the stereo screen' })
        # A batch with a screen picture gives a copy to the screen quad, and
        # none to the eyes. A batch of a screen frame with no kScreen and no
        # walk (the screen frame that the first frame of the scene ends) gives
        # no new picture.
        if ($ScreensThenWorld) {
            $screenPics = [regex]::Match($log, 'xr           : the screen quad \(stream_screen=1\): (\d+) screen pictures')
            if ($screenPics.Success) { $pictures -= [int]$screenPics.Groups[1].Value }
            $silentFrames = [regex]::Match($screenSummary, ', (\d+) screen frames with no kScreen')
            if ($silentFrames.Success) { $pictures -= [int]$silentFrames.Groups[1].Value }
        }
        if (-not $ScreensOnly) { Expect ($screenLine.Success -and $pictures -gt 0 -and [int]$screenLine.Groups[1].Value -ge $pictures -and [int]$screenLine.Groups[2].Value -ge $pictures) ("each eye of $what got a copy for each new picture (" + $(if ($screenLine.Success) { "$($screenLine.Groups[1].Value) and $($screenLine.Groups[2].Value) copies, $pictures pictures" } else { 'no line in the log' }) + ')') }
        if ($Projection) {
            $frameLine = [regex]::Match($log, 'xr           : \d+ copies of the projection layer for eye 0 and \d+ for eye 1\. (\d+) frames with the projection layer, (\d+) frames with the stereo screen')
            if (-not $AllWindow) { Expect ($frameLine.Success -and [int]$frameLine.Groups[1].Value -gt 0 -and [int]$frameLine.Groups[2].Value -eq 0) ("each frame with a picture had the projection layer (" + $(if ($frameLine.Success) { "$($frameLine.Groups[1].Value) frames with the projection layer, $($frameLine.Groups[2].Value) with the stereo screen" } else { 'no line in the log' }) + ')') }
        }
        # The pictures of the test frame of the stream (vk_scene_test_frame),
        # against the pictures of the PC of the same desk run: at most 2
        # percent of the pixels with a difference of more than 24.
        $references = @(Get-ChildItem -LiteralPath $Live -Filter 'bwvr-vk-*.bmp')
        # A feed with kView names its frames. The app makes the pictures of the
        # test frame only when the feed has that frame.
        $feedFrames = @($feedViews | ForEach-Object { [int]$_[0] })
        if ($feedFrames.Count -gt 0 -and $references.Count -gt 0) {
            $testFrame = [int][regex]::Match($references[0].Name, '-(\d+)\.bmp$').Groups[1].Value
            if ($feedFrames -notcontains $testFrame) {
                Write-Host "         the feed has the frames $($feedFrames[0]) to $($feedFrames[-1]) and not the test frame $testFrame of the PC, so the app makes no picture of it"
                $references = @()
            }
        }
        # A feed of the screens with no scene: the pictures of the PC of the
        # screen frames of the feed (vk_stream_screen_test).
        if ($Screens -and $feedFrames.Count -eq 0) {
            $inFeed = @($references | Where-Object {
                $n = [regex]::Match($_.Name, '-(\d+)\.bmp$')
                $n.Success -and $feedScreenFrames -contains [int64]$n.Groups[1].Value })
            Write-Host "         the PC has $($references.Count) pictures, $($inFeed.Count) of them of the screen frames of the feed"
            $references = $inFeed
        }
        if ($references.Count -gt 0 -or $feedFrames.Count -eq 0) { Expect ($references.Count -gt 0) "the PC made $($references.Count) pictures of the test frame" }
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
    Expect ($slotLine.Success -and [int]$slotLine.Groups[2].Value -gt 100 -and ([int]$slotLine.Groups[1].Value % 2) -eq 0) ("the app wrote slots in the frames of the session (" + $(if ($slotLine.Success) { "$($slotLine.Groups[2].Value) slots, the sequence $($slotLine.Groups[1].Value) is even" } else { 'no slot' }) + ')')
    Expect ($slotLine.Success -and $slotLine.Groups[5].Value -eq '13888888' -and [int64]$slotLine.Groups[4].Value -gt 0) ("the slot has the display time of the runtime, period 13888888 ns (" + $(if ($slotLine.Success) { $slotLine.Groups[5].Value } else { '' }) + ')')
    Expect ($slotLine.Success -and $slotLine.Groups[6].Value -eq '1' -and $log -match 'the LOCAL space recentered \(1 recenters\)') 'the recenter of the runtime is in the slot (1 recenter)'
    Expect ($slotLine.Success -and $slotLine.Groups[7].Value -eq '2064' -and $slotLine.Groups[8].Value -eq '2208') 'the slot has the size of the eyes that the headset asks for'
    Expect ($dump -match 'eye 0 valid 1 position -0\.0320 0\.0000 0\.0000 orientation 0\.0000 0\.0000 0\.0000 1\.0000 fov -0\.9425 0\.8203 0\.8901 -0\.9599' -and
            $dump -match 'eye 1 valid 1 position 0\.0320 0\.0000 0\.0000 orientation 0\.0000 0\.0000 0\.0000 1\.0000 fov -0\.9425 0\.8203 0\.8901 -0\.9599') 'the views of the runtime are in the slot, as the runtime gave them'
    Expect ($slotLine.Success -and ([Convert]::ToInt32($slotLine.Groups[3].Value, 16) -band 1) -eq 0 -and $dump -match 'hand 1 flags 0x000 ') 'the last slot, at the end of the session, has no focus and no active hand'
    # The poses of the right hand of the script: each point of the report
    # gives the aim pose of the runtime, and the slots in the log have it.
    $poses = @()
    foreach ($m in [regex]::Matches($report, 'frame \d+ point 1 -?\d+ \d+ \d+ pose (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)')) {
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
