<#
.SYNOPSIS
    Plays a stream of the walks of the game in the Vulkan renderer of the app
    in the Android emulator, and compares its pictures with the pictures of
    the renderer of the mod on the PC (docs/android.md, "Test the APK", step
    4).

.DESCRIPTION
    The stream and the pictures of the PC come from one desktop run with
    vk_stream_record, vk_stream_frames and vk_scene_test_frame (see "The
    stream of the walks"). The test:

      1. Starts the AVD bwvr_test with the GPU of the PC (-gpu host): the
         renderer needs Vulkan 1.3, and SwiftShader has 1.2.
      2. Installs bwvr/out/android/bwvr-quest.apk, starts it one time, and
         puts the stream and a bwvr.ini with stream_replay=1 into the game
         folder of the app.
      3. Starts the app, waits for the line "replay" of its log, and reads
         back each picture bwvr-vk-*.bmp.
      4. Compares each picture with the picture of the same name of the PC.

    The stream and the pictures hold pictures of the game: they stay in the
    folders of this PC (local, out) and never go into the repo.

.PARAMETER Stream
    The stream file. local\android-stream\bwvr-stream.bin by default.

.PARAMETER Reference
    The folder with the pictures bwvr-vk-*.bmp of the PC.

.PARAMETER MaxPercent
    The most pixels in percent that may differ by more than 24 in a channel.
    2 by default: the GPU of the emulator is the GPU of the PC, but the
    driver of the emulator is another driver.
#>
[CmdletBinding()]
param(
    [string]$Stream,
    [Parameter(Mandatory = $true)][string]$Reference,
    [double]$MaxPercent = 2.0,
    [switch]$Keep
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
Lock-AndroidEmulator
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Repo = Split-Path $Bwvr -Parent
if (-not $Stream) { $Stream = Join-Path $Repo 'local\android-stream\bwvr-stream.bin' }
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$Out = Join-Path $Bwvr 'out\android\emu-replay-test'
$Avd = 'bwvr_test'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"
$Port = 5584
$Serial = "emulator-$Port"

foreach ($f in @($Apk, $Stream, $Reference)) {
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
# difference, of two BMP files of one size.
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class BmpDiff {
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

$references = @(Get-ChildItem -LiteralPath $Reference -Filter 'bwvr-vk-*.bmp')
Expect ($references.Count -gt 0) "the PC made $($references.Count) pictures of the renderer"

# 1. The emulator with the GPU of the PC.
& $Adb start-server 2>&1 | Out-Null
$started = $false
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
    # 2. The APK, its folder, the stream.
    Say 'Install the APK and put the stream into the game folder of the app'
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('uninstall', $Package)
    Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
    $install = AdbOut @('install', $Apk)
    Expect ($install -match 'Success') ('adb install: ' + ($install.Trim() -split "`n")[-1])
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 3
    Adb @('shell', 'am', 'force-stop', $Package)
    $ini = Join-Path $Out 'bwvr.ini.pushed'
    Set-Content -LiteralPath $ini -Encoding ascii -Value @('[bwvr]', 'stream_replay=1')
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    Adb @('push', $Stream, "$AppDir/game/bwvr-stream.bin")

    # 3. The replay.
    Say 'Start the app with stream_replay=1'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    $deadline = (Get-Date).AddSeconds(180)
    do {
        Start-Sleep -Seconds 2
        $log = AdbOut @('shell', 'cat', "$AppDir/bwvr-quest.log")
    } while ($log -notmatch 'replay       : .*records' -and (Get-Date) -lt $deadline)
    Start-Sleep -Seconds 2
    Adb @('pull', "$AppDir/bwvr-quest.log", (Join-Path $Out 'bwvr-quest.log'))
    $logcat = AdbOut @('logcat', '-d', '-v', 'brief')
    Set-Content -LiteralPath (Join-Path $Out 'logcat.txt') -Value $logcat -Encoding utf8
    $listing = AdbOut @('shell', 'ls', "$AppDir/game")
    foreach ($name in ([regex]::Matches($listing, 'bwvr-vk-[a-z]+-\d+\.bmp') | ForEach-Object { $_.Value })) {
        Adb @('pull', "$AppDir/game/$name", (Join-Path $Out $name))
    }
    $log = Get-Content -LiteralPath (Join-Path $Out 'bwvr-quest.log') -Raw

    Say 'Check'
    $m = [regex]::Match($log, 'replay       : .*: (\d+) records, (\d+) frames, (\d+) walks, (\d+) draws, (\d+) surfaces, (\d+) of an unknown kind')
    Expect $m.Success 'the app played the stream'
    if ($m.Success) {
        Expect ([int]$m.Groups[3].Value -gt 0 -and [int]$m.Groups[4].Value -gt 0) "the stream had $($m.Groups[3].Value) walks and $($m.Groups[4].Value) draws"
        Expect ([int]$m.Groups[6].Value -eq 0) 'each record had a kind that the player knows'
    }
    Expect ($log -notmatch 'the file stops in a record') 'the stream did not stop in a record'
    Expect ($log -match 'vulkan       : Vulkan 1\.3') 'the renderer of the app started Vulkan 1.3'
    Expect ($log -notmatch 'vk scene     : .* failed') 'no step of the renderer failed'
    Expect ($logcat -notmatch 'Fatal signal' -and $logcat -notmatch 'FATAL EXCEPTION') 'logcat has no crash'
    foreach ($ref in $references) {
        $mine = Join-Path $Out $ref.Name
        if (-not (Test-Path -LiteralPath $mine)) {
            Expect $false "the app made the picture $($ref.Name)"
            continue
        }
        $d = [BmpDiff]::Compare($ref.FullName, $mine)
        if ($d[0] -lt 0) {
            Expect $false "$($ref.Name) has the size $($d[2]) x $($d[3]) on the PC and $($d[4]) x $($d[5]) in the app"
            continue
        }
        Expect ($d[0] -le $MaxPercent) ("{0}: {1:N2} percent of the pixels differ by more than 24, the mean difference is {2:N2}" -f $ref.Name, $d[0], $d[1])
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
