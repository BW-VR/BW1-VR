# One desktop run for the input channel from the headset app to the mod
# (docs/android.md, "The input channel").
#
#   input_channel_run.ps1 -Name a1 -Dll <build>\src\mod\Release\ddraw.dll -Kind fake
#   input_channel_run.ps1 -Name b1 -Dll <build>\src\mod\Release\ddraw.dll -Kind channel -From desk-a1
#
# Both kinds run in the test save "pen" with backend=vulkan, one_walk=1, the
# recipe of the scripted controller below, the pictures of the frame
# -TestFrame (vk_scene_test_frame) and a record of the input of the first
# -Record frames of the controllers (xr_input_record). The eyes are a fixed
# 0.5 world units apart (eye_separation_milli=500), because the invented
# head of the fake run has no eye separation of a headset to measure.
#
# -Kind fake: the head and the hands come from the fake sources (fake_head=1
#   of desk_test.ps1, fake_controller=2 with the recipe).
# -Kind channel: the run has xr_input=1. bwvr_input_writer.exe
#   (bwvr\build\test\input_writer) plays the record of the fake run of the
#   folder -From into bwvr-input.chan of the game folder, one frame of the
#   controllers at a time (xr_input_lockstep_ms), and writes each pulse that
#   the mod sends. After the run the script compares the record of this run
#   with the record of -From, the pulses that came to the writer with the
#   pulses in the log of this run and of -From, and the pictures of the test
#   frame with the pictures of -From.
#
# The output is in desk-<Name>: bwvr-input.rec, bwvr-input.chan,
# writer.txt and the pictures. The pictures hold pixels of the game and stay
# in the folder of the run.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [Parameter(Mandatory = $true)][ValidateSet('fake', 'channel')][string]$Kind,
    [string]$From = '',
    [int]$Record = 3100,
    [int]$TestFrame = 2996,
    [int]$Seconds = 60,
    [string]$Script = 'point@1500+1 trigger@1800+8 point@2300+1 trigger@2600+8',
    [string]$Points = '15,-25,40 -10,-30,50',
    [hashtable]$More = @{}
)
$ErrorActionPreference = 'Continue'
if (Test-Path -LiteralPath 'C:\Users\chris\AppData\Local\Temp\claude\D--Programmieren-BW1-VR\2fcc6cd4-74ed-4eae-9d1d-ce8b59c86a22\scratchpad\PAUSE') { 'NOT RUN: paused by the main session'; exit 7 }
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$writer = Join-Path $sp '..\..\build\test\input_writer\Release\bwvr_input_writer.exe'
if (-not (Test-Path -LiteralPath $writer)) { "NOT RUN: no test writer at $writer. Build with .\dev.ps1 build"; exit 6 }
$game = Get-BwvrGame
$chan = Join-Path $game 'bwvr-input.chan'
$rec = Join-Path $game 'bwvr-input.rec'
$out = Join-Path $sp ("desk-" + $Name)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$fromRec = $null
if ($Kind -eq 'channel') {
    if ($From -eq '') { 'NOT RUN: -Kind channel needs -From, the folder of a fake run'; exit 9 }
    if (-not [System.IO.Path]::IsPathRooted($From)) { $From = Join-Path $sp $From }
    $fromRec = Join-Path $From 'bwvr-input.rec'
    if (-not (Test-Path -LiteralPath $fromRec)) { "NOT RUN: no record at $fromRec"; exit 9 }
}
# A channel or a record of an earlier run must not be read.
foreach ($f in @($chan, $rec)) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }

$keys = @{
    backend = 'vulkan'; one_walk = 1; fake_controller = 2
    fake_controller_script = $Script; fake_controller_points = $Points
    vk_scene_test_frame = $TestFrame; xr_input_record = $Record
    eye_separation_milli = 500 }
$p = $null
if ($Kind -eq 'channel') {
    $keys['xr_input'] = 1
    $keys['xr_input_lockstep_ms'] = 2000
    $p = Start-Process -FilePath $writer -NoNewWindow -PassThru `
        -ArgumentList @('play', "`"$chan`"", "`"$fromRec`"", "$($Seconds + 60)") `
        -RedirectStandardOutput (Join-Path $out 'writer.txt') -RedirectStandardError (Join-Path $out 'writer-err.txt')
    $null = $p.Handle  # keeps the exit code
    "the test writer runs (process $($p.Id)): it plays $fromRec"
}
foreach ($k in $More.Keys) { $keys[$k] = $More[$k] }
& (Join-Path $sp 'desk_run.ps1') -Name $Name -Dll $Dll -Save pen -Seconds $Seconds -Extra $keys |
    Tee-Object -Variable runOut
if (Test-BwvrVerdict $runOut 'NOT RUN') {
    if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    'NOT RUN: the desktop run did not start'
    exit 7
}
if ($p) {
    # The writer ends by itself after its time. A writer that is not done
    # 120 s later is stopped.
    if (-not $p.WaitForExit(($Seconds + 120) * 1000)) {
        'the test writer did not end: it is stopped'
        Stop-Process -Id $p.Id -Force
    }
    "the test writer ended with the code $($p.ExitCode)"
}
foreach ($f in @($chan, $rec)) {
    if (Test-Path -LiteralPath $f) {
        try { Move-Item -LiteralPath $f -Destination (Join-Path $out (Split-Path -Leaf $f)) -Force } catch { "WARNING: $f was not moved out of the game folder: $_" }
    }
}
$log = Join-Path $out 'bwvr.log'
if (Test-Path -LiteralPath $log) {
    'the lines of the input channel and of the pulses in the log:'
    Select-String -LiteralPath $log -Pattern 'xr input     :|haptic pulse' | Select-Object -First 40 | ForEach-Object { '  ' + $_.Line }
}
if ($Kind -ne 'channel') { exit 0 }

# THE RESULTS of a channel run.
$fails = 0
"== the record of this run against the record of $From"
& $writer compare $fromRec (Join-Path $out 'bwvr-input.rec')
if ($LASTEXITCODE -ne 0) { $fails++ }
function Get-LogPulses([string]$File) {
    if (-not (Test-Path -LiteralPath $File)) { return @() }
    return @(Select-String -LiteralPath $File -Pattern 'haptic pulse, (\d+) ms at ([0-9.]+), frame (\d+)' |
        ForEach-Object { "frame $($_.Matches[0].Groups[3].Value), $($_.Matches[0].Groups[1].Value) ms at $($_.Matches[0].Groups[2].Value)" })
}
$writerPulses = @()
if (Test-Path -LiteralPath (Join-Path $out 'writer.txt')) {
    Get-Content -LiteralPath (Join-Path $out 'writer.txt') | Where-Object { $_ -notmatch '^pulse ' }
    $writerPulses = @(Select-String -LiteralPath (Join-Path $out 'writer.txt') -Pattern '^pulse frame (\d+) hand \d+, (\d+) ms at ([0-9.]+)' |
        ForEach-Object { "frame $($_.Matches[0].Groups[1].Value), $($_.Matches[0].Groups[2].Value) ms at $($_.Matches[0].Groups[3].Value)" })
}
$here = Get-LogPulses $log
$there = Get-LogPulses (Join-Path $From 'bwvr.log')
"== the pulses: $($writerPulses.Count) came to the writer, $($here.Count) in the log of this run, $($there.Count) in the log of $From"
$writerSame = ($writerPulses.Count -gt 0) -and (($writerPulses -join ';') -eq ($here -join ';'))
$fakeSame = ($here.Count -gt 0) -and (($here -join ';') -eq ($there -join ';'))
"   the pulses of the writer are the pulses of the log of this run: $writerSame"
"   the pulses of this run are the pulses of the fake run (frame, length, amplitude): $fakeSame"
foreach ($l in $writerPulses) { "   writer: $l" }
foreach ($l in $there) { "   fake run: $l" }
if (-not $writerSame) { $fails++ }
if (-not $fakeSame) { $fails++ }

# The pictures of the test frame against the pictures of -From.
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class InputBmpDiff {
  public static double[] Compare(string a, string b) {
    byte[] x = File.ReadAllBytes(a), y = File.ReadAllBytes(b);
    int wx = BitConverter.ToInt32(x, 18), hx = BitConverter.ToInt32(x, 22);
    int wy = BitConverter.ToInt32(y, 18), hy = BitConverter.ToInt32(y, 22);
    int bx = BitConverter.ToInt16(x, 28) / 8, by = BitConverter.ToInt16(y, 28) / 8;
    if (wx != wy || Math.Abs(hx) != Math.Abs(hy)) return new double[] { -1, -1, -1 };
    int ox = BitConverter.ToInt32(x, 10), oy = BitConverter.ToInt32(y, 10);
    int rx = (wx * bx + 3) & ~3, ry = (wy * by + 3) & ~3;
    long differ = 0, any = 0, total = 0; double sum = 0;
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
        if (worst > 0) ++any;
        ++total;
      }
    }
    return new double[] { 100.0 * differ / total, 100.0 * any / total, sum / (3.0 * total) };
  }
}
'@
$pictures = @(Get-ChildItem -LiteralPath $From -Filter 'bwvr-vk-*.bmp' -ErrorAction SilentlyContinue)
"== the pictures of the test frame against $From ($($pictures.Count) pictures)"
foreach ($ref in $pictures) {
    $mine = Join-Path $out $ref.Name
    if (-not (Test-Path -LiteralPath $mine)) { "   $($ref.Name): not in this run"; $fails++; continue }
    $d = [InputBmpDiff]::Compare($ref.FullName, $mine)
    "   {0}: {1:N3} percent of the pixels differ by more than 24, {2:N3} percent differ at all, the mean difference {3:N3}" -f $ref.Name, $d[0], $d[1], $d[2]
}
if ($pictures.Count -eq 0) { $fails++ }
if ($fails -eq 0) { 'INPUT CHANNEL VERIFIED: the record, the pulses and the pictures (see the numbers above, against the noise of two fake runs)' }
else { "INPUT CHANNEL FAULT: $fails of the checks above" }
