# One desktop run with the live stream of the walks and the test reader of
# the PC (docs/android.md, "The live stream").
#
#   live_stream_run.ps1 -Name live -Dll <build>\src\mod\Release\ddraw.dll
#
# 1. Starts bwvr_stream_reader (bwvr\build\test\stream_reader) with no window
#    of its own. It waits for a new head in bwvr-stream.ring of the game
#    folder, asks for the whole state at the frame -Attach of the game (a
#    reader that comes late), takes each frame to the frame -Last, and leaves.
# 2. Runs desk_run.ps1 in the test save "pen" with backend=vulkan,
#    one_walk=1, fake_controller=1, vk_stream_live=1, a record of the frames
#    -Record to -Record + 3 (vk_stream_record, vk_stream_frames=4) and the
#    pictures of the frame -TestFrame (vk_scene_test_frame).
# 3. Moves bwvr-stream.bin and bwvr-stream.ring from the game folder into the
#    folder of the run, and compares each frame of the record with the frame
#    of the same number that the reader took (bwvr_stream_reader compare).
#
# -SlowMs N: the test reader takes 1 MB and then sleeps N ms, so the ring is
# full (a test of the drop). -More @{...}: more ini keys, for example
# vk_stream_live_drop_us.
#
# The output is in desk-<Name>: reader.txt, ring-frames.txt (the bytes and
# the wait of the writer for each frame), ring-capture.bin, the record and
# the pictures. These files hold pixels of the game and stay in the folder of
# the run. ring-capture.bin is the feed of emu_live_test.ps1.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [int]$Attach = 2990,
    [int]$Record = 3000,
    [int]$TestFrame = 2996,
    [int]$Seconds = 60,
    [int]$SlowMs = 0,
    [hashtable]$More = @{}
)
$ErrorActionPreference = 'Continue'
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$reader = Join-Path $sp '..\..\build\test\stream_reader\Release\bwvr_stream_reader.exe'
if (-not (Test-Path -LiteralPath $reader)) { "NOT RUN: no test reader at $reader. Build with .\dev.ps1 build"; exit 6 }
$game = Get-BwvrGame
$ring = Join-Path $game 'bwvr-stream.ring'
$out = Join-Path $sp ("desk-" + $Name)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$last = $Record + 3
$p = Start-Process -FilePath $reader -NoNewWindow -PassThru `
    -ArgumentList @('read', "`"$ring`"", "`"$out`"", "$Attach", "$last", "$($Seconds * 4)", "$SlowMs") `
    -RedirectStandardOutput (Join-Path $out 'reader.txt') -RedirectStandardError (Join-Path $out 'reader-err.txt')
$null = $p.Handle  # keeps the exit code
"the test reader runs (process $($p.Id)): it asks at frame $Attach and leaves after frame $last"
$keys = @{
    backend = 'vulkan'; one_walk = 1; fake_controller = 1; vk_stream_live = 1
    vk_stream_record = $Record; vk_stream_frames = 4; vk_scene_test_frame = $TestFrame }
foreach ($k in $More.Keys) { $keys[$k] = $More[$k] }
& (Join-Path $sp 'desk_run.ps1') -Name $Name -Dll $Dll -Save pen -Seconds $Seconds -Extra $keys |
    Tee-Object -Variable runOut
if (Test-BwvrVerdict $runOut 'NOT RUN') {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    'NOT RUN: the desktop run did not start, so the test reader is stopped'
    exit 7
}
if (-not $p.WaitForExit(60000)) {
    "the test reader did not end: it is stopped"
    Stop-Process -Id $p.Id -Force
}
"the test reader ended with the code $($p.ExitCode)"
Get-Content -LiteralPath (Join-Path $out 'reader.txt')
foreach ($f in @('bwvr-stream.bin', 'bwvr-stream.ring')) {
    $from = Join-Path $game $f
    if (Test-Path -LiteralPath $from) {
        try { Move-Item -LiteralPath $from -Destination (Join-Path $out $f) -Force } catch { "WARNING: $f was not moved out of the game folder: $_" }
    }
}
& $reader compare (Join-Path $out 'ring-capture.bin') (Join-Path $out 'bwvr-stream.bin')
if ($LASTEXITCODE -eq 0) { 'LIVE STREAM VERIFIED: each frame of the record has the same bytes in the ring' }
else { 'LIVE STREAM FAULT: see the lines compare' }
