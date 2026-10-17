<#
.SYNOPSIS
    Gives the frames of a live stream of the walks to the app through the
    ring file in the Android emulator, and compares the pictures of the
    renderer of the app with the pictures of the renderer of the mod on the
    PC (docs/android.md, "Test the APK", step 5).

.DESCRIPTION
    The feed and the pictures of the PC come from one desktop run with
    vk_stream_live=1 and the test reader of the PC (bwvr_stream_reader read,
    see "The live stream"): the feed is ring-capture.bin, the frames that the
    reader took from the ring of the mod, from the frame of the answer to its
    ask. The test:

      1. Starts the AVD bwvr_test with the GPU of the PC (-gpu host): the
         renderer needs Vulkan 1.3, and SwiftShader has 1.2.
      2. Installs bwvr/out/android/bwvr-quest.apk, starts it one time, and
         puts the feed and a bwvr.ini with stream_live=1 and
         stream_live_feed=1 into the game folder of the app.
      3. Starts the app. A child process of the app makes the ring
         bwvr-stream.ring in the game folder and writes the frames of the
         feed into it, as the mod does. The reader thread of the app maps the
         same file, asks for the whole state, and takes the frames. The loop
         of the app gives one frame to the renderer at each step.
      4. Waits for the line "no new frame for 1 s" of the log, reads back
         each picture bwvr-vk-*.bmp, and compares it with the picture of the
         same name of the PC.

    The feed, the ring and the pictures hold pictures of the game: they stay
    in the folders of this PC (the desktop run, out) and never go into the
    repo.

.PARAMETER Run
    The folder of the desktop run: ring-capture.bin and the pictures
    bwvr-vk-*.bmp of the PC.

.PARAMETER Reuse
    Takes an emulator that runs on the port already. Without it the script
    stops when an emulator runs there.

.PARAMETER MaxPercent
    The most pixels in percent that may differ by more than 24 in a channel.
    2 by default, as in emu_replay_test.ps1.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Run,
    [double]$MaxPercent = 2.0,
    [switch]$Keep,
    [switch]$Reuse
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$Out = Join-Path $Bwvr 'out\android\emu-live-test'
$Feed = Join-Path $Run 'ring-capture.bin'
$Avd = 'bwvr_test'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"
$Port = 5584
$Serial = "emulator-$Port"

foreach ($f in @($Apk, $Feed, $Run)) {
    if (-not (Test-Path -LiteralPath $f)) { Stop-Android "There is no file or folder:`n$f" }
}
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

# The share of pixels that differ by more than 24 in a channel, and the mean
# difference, of two BMP files of one size (as emu_replay_test.ps1).
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class LiveBmpDiff {
  public static double[] Compare(string a, string b) {
    byte[] x = File.ReadAllBytes(a), y = File.ReadAllBytes(b);
    int wx = BitConverter.ToInt32(x, 18), hx = BitConverter.ToInt32(x, 22);
    int wy = BitConverter.ToInt32(y, 18), hy = BitConverter.ToInt32(y, 22);
    int bx = BitConverter.ToInt16(x, 28) / 8, by = BitConverter.ToInt16(y, 28) / 8;
    if (wx != wy || Math.Abs(hx) != Math.Abs(hy)) return new double[] { -1, -1, wx, hx, wy, hy };
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
    return new double[] { 100.0 * differ / total, sum / (3.0 * total), wx, hx, wy, hy };
  }
}
'@

$references = @(Get-ChildItem -LiteralPath $Run -Filter 'bwvr-vk-*.bmp')
Expect ($references.Count -gt 0) "the PC made $($references.Count) pictures of the renderer"

# The frames of the feed: the frame of each kFrame record that starts a
# frame of the stream (the record after kStart, or the first record).
$bytes = [System.IO.File]::ReadAllBytes($Feed)
$feedFrames = New-Object System.Collections.Generic.List[long]
$at = 8; $open = $false
while ($at + 8 -le $bytes.Length) {
    $kind = [BitConverter]::ToUInt32($bytes, $at)
    $size = [BitConverter]::ToUInt32($bytes, $at + 4)
    if ($kind -eq 2 -and -not $open) { $feedFrames.Add([BitConverter]::ToInt64($bytes, $at + 8)); $open = $true }
    if ($kind -eq 17) { $open = $false }
    $at = ($at + 8 + $size + 3) -band (-bnot 3)
}
Expect ($feedFrames.Count -gt 0) "the feed has $($feedFrames.Count) frames ($($feedFrames[0]) to $($feedFrames[$feedFrames.Count - 1])), $($bytes.Length) bytes"

# 1. The emulator with the GPU of the PC.
& $Adb start-server 2>&1 | Out-Null
$started = $false
# An emulator on the port that this script did not start can be the test of
# a different session, and it can have no GPU of the PC. Only -Reuse takes it.
if ((& $Adb devices 2>$null | Select-String -Pattern "^$Serial\s") -and -not $Reuse) {
    Stop-Android "NOT RUN: an emulator runs on the port $Port already. It can be the test of a different session. Wait for its end, or give -Reuse."
}
if (-not (& $Adb devices 2>$null | Select-String -Pattern "^$Serial\s+device")) {
    Say 'Start the emulator (no window, the GPU of the PC)'
    Start-Process -FilePath $Emulator -ArgumentList @('-avd', $Avd, '-port', "$Port",
        '-no-window', '-no-audio', '-no-boot-anim', '-no-snapshot', '-gpu', 'host') -WindowStyle Hidden | Out-Null
    $started = $true
}
$deadline = (Get-Date).AddMinutes(6)
do {
    Start-Sleep -Seconds 3
    $booted = (& $Adb -s $Serial shell getprop sys.boot_completed 2>$null | Out-String).Trim()
} while ($booted -ne '1' -and (Get-Date) -lt $deadline)
do {
    Start-Sleep -Seconds 2
    $pm = (& $Adb -s $Serial shell pm path android 2>$null | Out-String)
} while ($pm -notmatch 'package:' -and (Get-Date) -lt $deadline)
if ($booted -ne '1') { Stop-Android 'The emulator did not start in 6 minutes.' }

try {
    # 2. The APK, its folder, the feed.
    Say 'Install the APK and put the feed into the game folder of the app'
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('uninstall', $Package)
    Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
    $install = AdbOut @('install', $Apk)
    Expect ($install -match 'Success') ('adb install: ' + ($install.Trim() -split "`n")[-1])
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 3
    Adb @('shell', 'am', 'force-stop', $Package)
    $ini = Join-Path $Out 'bwvr.ini.pushed'
    Set-Content -LiteralPath $ini -Encoding ascii -Value @('[bwvr]', 'stream_live=1', 'stream_live_feed=1')
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    Adb @('push', $Feed, "$AppDir/game/bwvr-stream-feed.bin")

    # 3. The live stream.
    Say 'Start the app with stream_live=1 and stream_live_feed=1'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    $deadline = (Get-Date).AddSeconds(300)
    do {
        Start-Sleep -Seconds 2
        $log = AdbOut @('shell', 'cat', "$AppDir/bwvr-quest.log")
    } while ($log -notmatch 'live         : no new frame for 1 s' -and (Get-Date) -lt $deadline)
    Start-Sleep -Seconds 2
    $ring = AdbOut @('shell', 'ls', '-l', "$AppDir/game/bwvr-stream.ring")
    Adb @('pull', "$AppDir/bwvr-quest.log", (Join-Path $Out 'bwvr-quest.log'))
    $logcat = AdbOut @('logcat', '-d', '-v', 'brief')
    Set-Content -LiteralPath (Join-Path $Out 'logcat.txt') -Value $logcat -Encoding utf8
    $listing = AdbOut @('shell', 'ls', "$AppDir/game")
    foreach ($name in ([regex]::Matches($listing, 'bwvr-vk-[a-z]+-\d+\.bmp') | ForEach-Object { $_.Value })) {
        Adb @('pull', "$AppDir/game/$name", (Join-Path $Out $name))
    }
    $log = ''
    $logFile = Join-Path $Out 'bwvr-quest.log'
    if (Test-Path -LiteralPath $logFile) { $log = [string](Get-Content -LiteralPath $logFile -Raw) }

    Say 'Check'
    Expect ($log.Length -gt 0) 'the app wrote its log'
    Expect ($log -match "live         : stream_live_feed=1\. The process \d+ writes the $($feedFrames.Count) frames") 'a child process of the app writes each frame of the feed into the ring'
    Expect ($ring -match '33558528') ('the ring file in the game folder has 32 MB and the head: ' + $ring.Trim())
    Expect ($log -match 'live         : the ring is there: 32 MB') 'the reader thread mapped the ring with a whole head'
    $m = [regex]::Match($log, 'live         : the writer answered the ask after (\d+) ms')
    Expect $m.Success 'the writer answered the ask for the whole state'
    $m = [regex]::Match($log, 'live         : frames come\. The first is frame (\d+)(, with the whole state)?')
    Expect ($m.Success -and [long]$m.Groups[1].Value -eq $feedFrames[0] -and $m.Groups[2].Success) "the first frame is the first frame of the feed ($($feedFrames[0])), with the whole state"
    $m = [regex]::Match($log, 'live         : no new frame for 1 s: (\d+) frames played \(frames (\d+) to (\d+)\)\. In all (\d+) records, (\d+) walks, (\d+) draws, (\d+) surfaces, (\d+) of an unknown kind([^.]*)\. The play of a frame took ([\d.]+) ms as a mean and ([\d.]+) ms at most')
    Expect $m.Success 'the app played the frames and then waited for more'
    if ($m.Success) {
        Expect ([int]$m.Groups[1].Value -eq $feedFrames.Count -and [long]$m.Groups[3].Value -eq $feedFrames[$feedFrames.Count - 1]) "the app played each frame of the feed: $($m.Groups[1].Value) frames, $($m.Groups[2].Value) to $($m.Groups[3].Value)"
        Expect ([int]$m.Groups[5].Value -gt 0 -and [int]$m.Groups[6].Value -gt 0) "the frames had $($m.Groups[5].Value) walks, $($m.Groups[6].Value) draws and $($m.Groups[7].Value) surfaces"
        Expect ([int]$m.Groups[8].Value -eq 0 -and $m.Groups[9].Value -eq '') 'each record had a kind that the player knows, and no frame stops in a record'
        Write-Host ("   info  the play of a frame took {0} ms as a mean and {1} ms at most" -f $m.Groups[10].Value, $m.Groups[11].Value)
    }
    Expect ($log -match 'live         : the feed process ended with the status 0') 'the feed process ended with the status 0: the reader took each byte'
    Expect ($log -notmatch 'live         : the writer gave up|live         : a record in the ring is not|live         : the indices of the ring are not valid') 'the reader did not lose its place'
    Expect ($log -match 'vulkan       : Vulkan 1\.3') 'the renderer of the app started Vulkan 1.3'
    Expect ($log -notmatch 'vk scene     : .* failed') 'no step of the renderer failed'
    Expect ($logcat -notmatch 'Fatal signal' -and $logcat -notmatch 'FATAL EXCEPTION') 'logcat has no crash'
    foreach ($ref in $references) {
        $mine = Join-Path $Out $ref.Name
        if (-not (Test-Path -LiteralPath $mine)) {
            Expect $false "the app made the picture $($ref.Name)"
            continue
        }
        $d = [LiveBmpDiff]::Compare($ref.FullName, $mine)
        if ($d[0] -lt 0) {
            Expect $false "$($ref.Name) has the size $($d[2]) x $($d[3]) on the PC and $($d[4]) x $($d[5]) in the app"
            continue
        }
        Expect ($d[0] -le $MaxPercent) ("{0}: {1:N2} percent of the pixels differ by more than 24, the mean difference is {2:N2}" -f $ref.Name, $d[0], $d[1])
    }
} finally {
    # The ring and the feed hold pixels of the game: they go with the folder
    # of the app.
    Adb @('shell', 'am', 'force-stop', $Package)
    if ($started -and -not $Keep) {
        Say 'Stop the emulator'
        & $Adb -s $Serial emu kill 2>&1 | Out-Null
    }
}

Write-Host ('== ' + $checks + ' checks, ' + $faults.Count + ' failed. The results are in ' + $Out) -ForegroundColor Cyan
if ($faults.Count -gt 0) { exit 1 }
