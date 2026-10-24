# One desktop run of the screens with no scene through the live stream
# (docs/android.md, "The screens with no scene").
#
#   screens_run.ps1 -Name s1 -Dll <build>\src\mod\Release\ddraw.dll -From desk-a1
#
# The run starts a new game (no test save, -NEWGAME of desk_run.ps1) with
# the logos (intro_skip_test_frame=0), the video before the intro and the
# dialog box after it. It has backend=vulkan, one_walk=1, ui_panel=1 and
# xr_input=1: bwvr_input_writer.exe (bwvr\build\test\input_writer) plays the
# record of the input of the fake run of the folder -From
# (input_channel_run.ps1 -Kind fake) into bwvr-input.chan of the game folder,
# as the headset app writes its input. xr_input_stale_ms=0: the frame of the
# controllers does not move in these screens, so the writer writes one slot.
#
# The live stream (vk_stream_live=1): bwvr_stream_reader
# (bwvr\build\test\stream_reader) asks for the whole state at the screen frame
# -Attach and takes each frame to the screen frame -Last. The record of the
# stream (vk_stream_record) has the screen frames -Record to -Record + 3. Each
# -Every-th screen frame saves the pictures of its walk (vk_stream_screen_test,
# bwvr-vk-left-<frame>.bmp): the pictures of the PC for emu_replay_test.ps1
# and emu_xr_test.ps1 -Live. A screen frame has the number 1000000000 + its
# count (src/mod/screen_quad_math.h), and -Attach, -Last and -Record are
# counts.
#
# After the run: the lines of the log of the screens and of the dialog boxes,
# bwvr_stream_reader screens of the capture and of the record, and
# bwvr_stream_reader compare of the record with the capture.
#
# -Most N: the reader leaves after N frames too. The numbers of the frames go
# down when the frames of the scene start, so a capture over that switch
# needs it. -SkipAt N (intro_skip_test_frame): at frame N of the intro a
# press that no hand made skips what is left of the intro (with intro_vr=0
# the Lionhead logo too), so the frames of the scene start at about the same
# screen frame in each run. 0 (the default): no skip. -TestFrame N
# (vk_scene_test_frame): the pictures of the frame N of the scene, the
# pictures of the PC for emu_xr_test.ps1 -Live. Example, the switch at the
# end of the Lionhead logo: -SkipAt 8000 -Attach 8800 -Last 999999 -Most 350
# -TestFrame 60 -More @{ intro_vr = 0 }. -Last must be above -Attach: the
# reader leaves at the screen frame -Last.
#
# -More @{...}: more ini keys, for example vk_device = 0 or
# vk_stream_screens = 0. -WriterHz <hz>: the test writer also writes the
# last slot again <hz> times a second, as the app writes a slot at each
# frame of its session (input_channel_run.ps1 -WriterHz), for vk_pace = 1.
#
# The output is in desk-<Name>: bwvr.log, reader.txt, writer.txt,
# ring-capture.bin, bwvr-stream.bin and the pictures. The stream, the capture
# and the pictures hold pixels of the game and stay in the folder of the run.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [Parameter(Mandatory = $true)][string]$From,
    [int]$Seconds = 200,
    [int]$Attach = 2000,
    [int]$Last = 2040,
    [int]$Record = 0,
    [int]$Every = 0,
    [int]$Most = 0,
    [int]$SkipAt = 0,
    [int]$TestFrame = 0,
    [int]$WriterHz = 0,
    [hashtable]$More = @{}
)
$ErrorActionPreference = 'Continue'
if ($Last -le $Attach) {
    "SCREENS RUN NOT RUN: -Last $Last is not above -Attach $Attach, so the reader leaves at its first frame. Give a -Last above -Attach (with -Most, for example -Last 999999)"
    exit 2
}
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$writer = Join-Path $sp '..\..\build\test\input_writer\Release\bwvr_input_writer.exe'
if (-not (Test-Path -LiteralPath $writer)) { "NOT RUN: no test writer at $writer. Build with .\dev.ps1 build"; exit 6 }
$reader = Join-Path $sp '..\..\build\test\stream_reader\Release\bwvr_stream_reader.exe'
if (-not (Test-Path -LiteralPath $reader)) { "NOT RUN: no test reader at $reader. Build with .\dev.ps1 build"; exit 6 }
if (-not [System.IO.Path]::IsPathRooted($From)) { $From = Join-Path $sp $From }
$fromRec = Join-Path $From 'bwvr-input.rec'
if (-not (Test-Path -LiteralPath $fromRec)) { "NOT RUN: no record of the input at $fromRec"; exit 9 }
$base = 1000000000
$game = Get-BwvrGame
$chan = Join-Path $game 'bwvr-input.chan'
$ring = Join-Path $game 'bwvr-stream.ring'
$bin = Join-Path $game 'bwvr-stream.bin'
$out = Join-Path $sp ("desk-" + $Name)
New-Item -ItemType Directory -Force -Path $out | Out-Null
# A file of a run before must not be read. A stream of the player that is in
# the game folder waits in the folder of the run and goes back after it.
Remove-Item -LiteralPath $chan -Force -ErrorAction SilentlyContinue
$aside = Join-Path $out 'game-folder-before'
foreach ($f in @($bin, $ring)) {
    if (Test-Path -LiteralPath $f) {
        New-Item -ItemType Directory -Force -Path $aside | Out-Null
        Move-Item -LiteralPath $f -Destination (Join-Path $aside (Split-Path -Leaf $f)) -Force
        "the file $f of an earlier run waits in $aside and goes back after the run"
    }
}

$keys = @{
    backend = 'vulkan'; one_walk = 1; ui_panel = 1; intro_skip_test_frame = $SkipAt
    xr_input = 1; xr_input_stale_ms = 0; vk_stream_live = 1 }
if ($TestFrame -gt 0) { $keys['vk_scene_test_frame'] = $TestFrame }
if ($Record -gt 0) { $keys['vk_stream_record'] = $base + $Record; $keys['vk_stream_frames'] = 4 }
if ($Every -gt 0) { $keys['vk_stream_screen_test'] = $Every }
foreach ($k in $More.Keys) { $keys[$k] = $More[$k] }

$q = Start-Process -FilePath $reader -NoNewWindow -PassThru `
    -ArgumentList @('read', "`"$ring`"", "`"$out`"", "$($base + $Attach)", "$($base + $Last)", "$(2100 + ($Seconds + 60) * 4)", '0', '0', "$Most") `
    -RedirectStandardOutput (Join-Path $out 'reader.txt') -RedirectStandardError (Join-Path $out 'reader-err.txt')
$null = $q.Handle  # keeps the exit code
"the test reader runs (process $($q.Id)): it asks at screen frame $Attach ($($base + $Attach)) and leaves after screen frame $Last$(if ($Most -gt 0) { " or after $Most frames" })"
$p = Start-Process -FilePath $writer -NoNewWindow -PassThru `
    -ArgumentList @('play', "`"$chan`"", "`"$fromRec`"", "$($Seconds + 60)", '2100', "$WriterHz") `
    -RedirectStandardOutput (Join-Path $out 'writer.txt') -RedirectStandardError (Join-Path $out 'writer-err.txt')
$null = $p.Handle
"the test writer runs (process $($p.Id)): it plays $fromRec"

& (Join-Path $sp 'desk_run.ps1') -Name $Name -Dll $Dll -Seconds $Seconds -Extra $keys |
    Tee-Object -Variable runOut
if (Test-BwvrVerdict $runOut 'NOT RUN') {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $q.Id -Force -ErrorAction SilentlyContinue
    foreach ($f in @($bin, $ring)) {
        $kept = Join-Path $aside (Split-Path -Leaf $f)
        if (Test-Path -LiteralPath $kept) { Move-Item -LiteralPath $kept -Destination $f -Force }
    }
    'NOT RUN: the desktop run did not start'
    exit 7
}
if (-not $p.WaitForExit(($Seconds + 120) * 1000)) {
    'the test writer did not end: it is stopped'
    Stop-Process -Id $p.Id -Force
}
"the test writer ended with the code $($p.ExitCode)"
if (-not $q.WaitForExit(60000)) {
    'the test reader did not end: it is stopped'
    Stop-Process -Id $q.Id -Force
}
"the test reader ended with the code $($q.ExitCode)"
Get-Content -LiteralPath (Join-Path $out 'reader.txt') | Select-Object -Last 12
foreach ($f in @($chan, $bin, $ring)) {
    if (Test-Path -LiteralPath $f) {
        try { Move-Item -LiteralPath $f -Destination (Join-Path $out (Split-Path -Leaf $f)) -Force } catch { "WARNING: $f was not moved out of the game folder: $_" }
    }
}
foreach ($f in @($bin, $ring)) {
    $kept = Join-Path $aside (Split-Path -Leaf $f)
    if (Test-Path -LiteralPath $kept) {
        try { Move-Item -LiteralPath $kept -Destination $f -Force; "the file $f of the earlier run is back" } catch { "WARNING: $kept did not go back to the game folder: $_" }
    }
}
$fails = 0
$log = Join-Path $out 'bwvr.log'
if (Test-Path -LiteralPath $log) {
    '== the lines of the screens, the dialog boxes and the logos in the log'
    Select-String -LiteralPath $log -Pattern '^(vk stream    : .*screen|setup box    :|intro skip   :|controllers  : a dialog box)' |
        Select-Object -First 60 | ForEach-Object { '   ' + $_.Line }
    $boxes = @(Select-String -LiteralPath $log -Pattern '^setup box    : the dialog box \S+ is up now').Count
    "   lines that name a dialog box: $boxes"
} else {
    "   no log at $log"
    $fails++
}
foreach ($s in @('ring-capture.bin', 'bwvr-stream.bin')) {
    $file = Join-Path $out $s
    if (-not (Test-Path -LiteralPath $file)) {
        if ($s -eq 'ring-capture.bin' -or $Record -gt 0) { "== no $s"; $fails++ }
        continue
    }
    "== the screens of $s"
    & $reader screens $file | Select-Object -Last 8
    if ($LASTEXITCODE -ne 0) { $fails++ }
}
# The record and the capture have the same bytes for a frame when the record
# is in the frames of the capture.
if ($Record -gt 0 -and ($Record -le $Attach -or $Record + 3 -gt $Last)) {
    "== the record (screen frames $Record to $($Record + 3)) is not in the capture (screen frames $($Attach + 1) to $Last), so the two are not compared"
} elseif ($Record -gt 0 -and (Test-Path -LiteralPath (Join-Path $out 'bwvr-stream.bin'))) {
    '== the frames of the ring against the record of the stream'
    & $reader compare (Join-Path $out 'ring-capture.bin') (Join-Path $out 'bwvr-stream.bin')
    if ($LASTEXITCODE -ne 0) { $fails++ }
}
$pictures = @(Get-ChildItem -LiteralPath $out -Filter 'bwvr-vk-left-1*.bmp' -ErrorAction SilentlyContinue)
"== $($pictures.Count) pictures of the left eye of screen frames in the folder of the run"
if ($fails -eq 0) { 'SCREENS RUN DONE: the screen frames are in the stream' }
else { "SCREENS RUN FAULT: $fails of the checks above" }
