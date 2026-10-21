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
#   frame with the pictures of -From. The writer waits up to 35 minutes for
#   the head of the mod, because desk_run.ps1 can first wait for an idle PC.
# -MaxPercent (0.25): the bound for each picture of the test frame against
#   the picture of -From, as the percentage of its pixels that differ by more
#   than 24. Two fake runs close in time differ by about 0.05.
#
# Both kinds also record the projection layer of each frame of the record
# (xr_view_record, docs/android.md, "The projection layer"): what the mod
# gives its session, or with xr_input=1 the back channel to the app. A
# channel run compares its record of the views with the record of -From
# (bwvr_stream_reader view-compare). With -More @{ fake_head_fov = 1 } in the
# fake run, the invented head has the angles of view of a headset, so the
# views have a projection layer, and the channel run gets the same angles
# from the record of the input.
#
# Both kinds also record the quad layers of each frame of the record
# (xr_layer_record, docs/android.md, "The quad layers"): the cards, the
# panel quads and the cut scene window that the mod gives its session, or
# with xr_input=1 the layers of the live stream. With -More @{
# fake_head_layers = 1 } in the fake run, the invented head routes the
# interface as a session does, as a channel run with xr_input_layers=1. A
# channel run compares its record of the layers with the record of -From
# (bwvr_stream_reader layer-compare). Give the same -More to both runs, for
# example ui_panel = 1 for the panel quads.
#
# -Live: the run also has the live stream of the walks (vk_stream_live=1, a
#   record of the frames -LiveRecord to -LiveRecord + 3), and
#   bwvr_stream_reader (bwvr\build\test\stream_reader) takes the frames
#   -LiveAttach to -LiveRecord + 3 from the ring, as live_stream_run.ps1
#   does. A channel run then checks that each record kView of the ring and of
#   the record of the stream is the view of the same frame in the record of
#   the views, and that the layer records of each frame of the ring and of
#   the record of the stream are the same bytes as in the record of the
#   layers. Its ring-capture.bin is a feed for emu_xr_test.ps1 -Live, with
#   the back channel.
#
# The output is in desk-<Name>: bwvr-input.rec, bwvr-input.chan,
# bwvr-view.rec, bwvr-layer.rec, with -Live the pictures of the cards of the
# capture (card-*.bmp), writer.txt, with -Live reader.txt, ring-capture.bin and
# bwvr-stream.bin, and the pictures. The pictures, the stream and the
# capture hold pixels of the game and stay in the folder of the run.
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
    [double]$MaxPercent = 0.25,
    [switch]$Live,
    [int]$LiveAttach = 2990,
    [int]$LiveRecord = 3000,
    [hashtable]$More = @{}
)
$ErrorActionPreference = 'Continue'
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$writer = Join-Path $sp '..\..\build\test\input_writer\Release\bwvr_input_writer.exe'
if (-not (Test-Path -LiteralPath $writer)) { "NOT RUN: no test writer at $writer. Build with .\dev.ps1 build"; exit 6 }
$reader = Join-Path $sp '..\..\build\test\stream_reader\Release\bwvr_stream_reader.exe'
if (-not (Test-Path -LiteralPath $reader)) { "NOT RUN: no test reader at $reader. Build with .\dev.ps1 build"; exit 6 }
$game = Get-BwvrGame
$chan = Join-Path $game 'bwvr-input.chan'
$rec = Join-Path $game 'bwvr-input.rec'
$views = Join-Path $game 'bwvr-view.rec'
$layerRec = Join-Path $game 'bwvr-layer.rec'
$ring = Join-Path $game 'bwvr-stream.ring'
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
foreach ($f in @($chan, $rec, $views, $layerRec)) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }
# With -Live the run writes bwvr-stream.bin and bwvr-stream.ring. A file of
# that name that is in the game folder before the run is not of this run: it
# waits in the folder of the run and goes back after the run.
$streamFiles = @((Join-Path $game 'bwvr-stream.bin'), $ring)
$aside = Join-Path $out 'game-folder-before'
if ($Live) {
    foreach ($f in $streamFiles) {
        if (Test-Path -LiteralPath $f) {
            New-Item -ItemType Directory -Force -Path $aside | Out-Null
            Move-Item -LiteralPath $f -Destination (Join-Path $aside (Split-Path -Leaf $f)) -Force
            "the file $f of an earlier run waits in $aside and goes back after the run"
        }
    }
}

$keys = @{
    backend = 'vulkan'; one_walk = 1; fake_controller = 2
    fake_controller_script = $Script; fake_controller_points = $Points
    vk_scene_test_frame = $TestFrame; xr_input_record = $Record
    xr_view_record = $Record; xr_layer_record = $Record; eye_separation_milli = 500 }
$q = $null
if ($Live) {
    $keys['vk_stream_live'] = 1
    $keys['vk_stream_record'] = $LiveRecord
    $keys['vk_stream_frames'] = 4
    $last = $LiveRecord + 3
    $q = Start-Process -FilePath $reader -NoNewWindow -PassThru `
        -ArgumentList @('read', "`"$ring`"", "`"$out`"", "$LiveAttach", "$last", "$(2100 + ($Seconds + 60) * 4)", '0', '0') `
        -RedirectStandardOutput (Join-Path $out 'reader.txt') -RedirectStandardError (Join-Path $out 'reader-err.txt')
    $null = $q.Handle  # keeps the exit code
    "the test reader runs (process $($q.Id)): it asks at frame $LiveAttach and leaves after frame $last"
}
$p = $null
if ($Kind -eq 'channel') {
    $keys['xr_input'] = 1
    $keys['xr_input_lockstep_ms'] = 2000
    $p = Start-Process -FilePath $writer -NoNewWindow -PassThru `
        -ArgumentList @('play', "`"$chan`"", "`"$fromRec`"", "$($Seconds + 60)", '2100') `
        -RedirectStandardOutput (Join-Path $out 'writer.txt') -RedirectStandardError (Join-Path $out 'writer-err.txt')
    $null = $p.Handle  # keeps the exit code
    "the test writer runs (process $($p.Id)): it plays $fromRec"
}
foreach ($k in $More.Keys) { $keys[$k] = $More[$k] }
& (Join-Path $sp 'desk_run.ps1') -Name $Name -Dll $Dll -Save pen -Seconds $Seconds -Extra $keys |
    Tee-Object -Variable runOut
if (Test-BwvrVerdict $runOut 'NOT RUN') {
    if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    if ($q) { Stop-Process -Id $q.Id -Force -ErrorAction SilentlyContinue }
    foreach ($f in $streamFiles) {
        $kept = Join-Path $aside (Split-Path -Leaf $f)
        if (Test-Path -LiteralPath $kept) { Move-Item -LiteralPath $kept -Destination $f -Force }
    }
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
if ($q) {
    if (-not $q.WaitForExit(60000)) {
        'the test reader did not end: it is stopped'
        Stop-Process -Id $q.Id -Force
    }
    "the test reader ended with the code $($q.ExitCode)"
    Get-Content -LiteralPath (Join-Path $out 'reader.txt')
}
$made = @($chan, $rec, $views, $layerRec)
if ($Live) { $made += $streamFiles }
foreach ($f in $made) {
    if (Test-Path -LiteralPath $f) {
        try { Move-Item -LiteralPath $f -Destination (Join-Path $out (Split-Path -Leaf $f)) -Force } catch { "WARNING: $f was not moved out of the game folder: $_" }
    }
}
foreach ($f in $streamFiles) {
    $kept = Join-Path $aside (Split-Path -Leaf $f)
    if ($Live -and (Test-Path -LiteralPath $kept)) {
        try { Move-Item -LiteralPath $kept -Destination $f -Force; "the file $f of the earlier run is back" } catch { "WARNING: $kept did not go back to the game folder: $_" }
    }
}
$log = Join-Path $out 'bwvr.log'
if (Test-Path -LiteralPath $log) {
    'the lines of the input channel and of the pulses in the log:'
    Select-String -LiteralPath $log -Pattern 'xr input     :|haptic pulse' | Select-Object -First 40 | ForEach-Object { '  ' + $_.Line }
}
$fails = 0
if ($Live) {
    "== the frames of the ring against the record of the stream"
    & $reader compare (Join-Path $out 'ring-capture.bin') (Join-Path $out 'bwvr-stream.bin')
    if ($LASTEXITCODE -ne 0) { $fails++ }
}
if ($Kind -ne 'channel') {
    if ($fails -gt 0) { "LIVE STREAM FAULT: see the lines compare"; exit 1 }
    exit 0
}

# THE RESULTS of a channel run.
"== the record of this run against the record of $From"
& $writer compare $fromRec (Join-Path $out 'bwvr-input.rec')
if ($LASTEXITCODE -ne 0) { $fails++ }
# THE VIEWS: the projection layer of each frame, from the back channel of
# this run and from the fake run of the same input.
"== the views of this run against the views of $From"
& $reader view-compare (Join-Path $From 'bwvr-view.rec') (Join-Path $out 'bwvr-view.rec')
if ($LASTEXITCODE -ne 0) { $fails++ }
if ($Live) {
    foreach ($s in @('ring-capture.bin', 'bwvr-stream.bin')) {
        "== the records kView of $s against the views of this run"
        & $reader views (Join-Path $out $s) (Join-Path $out 'bwvr-view.rec') |
            Where-Object { $_ -notmatch ': the same bytes as in the record$' }
        if ($LASTEXITCODE -ne 0) { $fails++ }
    }
}
# THE QUAD LAYERS: the layers of each frame, from the record of the layers of
# this run and from the fake run of the same input. A record with no frame
# (a run with no layers) has only its head of 8 bytes, or no byte.
$layersHere = Join-Path $out 'bwvr-layer.rec'
$layersFrom = Join-Path $From 'bwvr-layer.rec'
$haveLayers = (Test-Path -LiteralPath $layersHere) -and (Get-Item -LiteralPath $layersHere).Length -gt 8
if ($haveLayers -and (Test-Path -LiteralPath $layersFrom) -and (Get-Item -LiteralPath $layersFrom).Length -gt 8) {
    "== the layers of this run against the layers of $From"
    & $reader layer-compare $layersFrom $layersHere
    if ($LASTEXITCODE -ne 0) { $fails++ }
}
if ($Live -and $haveLayers) {
    foreach ($s in @('ring-capture.bin', 'bwvr-stream.bin')) {
        "== the layer records of $s against the layers of this run"
        # The pictures of the cards of the capture go to the folder of the run.
        $folder = @($(if ($s -eq 'ring-capture.bin') { $out }))
        & $reader layers (Join-Path $out $s) $layersHere @folder |
            Where-Object { $_ -notmatch ': the same bytes as in the record$' -and $_ -notmatch '^layers       : frame \d+ (quads|cards)' }
        if ($LASTEXITCODE -ne 0) { $fails++ }
    }
}
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
    $over = $d[0] -gt $MaxPercent
    if ($over) { $fails++ }
    "   {0}: {1:N3} percent of the pixels differ by more than 24, {2:N3} percent differ at all, the mean difference {3:N3}{4}" -f $ref.Name, $d[0], $d[1], $d[2], $(if ($over) { " (more than $MaxPercent percent: FAULT)" } else { '' })
}
if ($pictures.Count -eq 0) { $fails++ }
if ($fails -eq 0) { "INPUT CHANNEL VERIFIED: the record, the pulses, the views$(if ($haveLayers) { ', the layers' })$(if ($Live) { ', the views in the stream' })$(if ($Live -and $haveLayers) { ', the layers in the stream' }) and the pictures (each picture at most $MaxPercent percent of its pixels by more than 24)" }
else { "INPUT CHANNEL FAULT: $fails of the checks above" }
