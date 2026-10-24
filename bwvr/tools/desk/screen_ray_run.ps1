# One desktop run of the pointer on the screen quad (docs/controllers.md,
# "The screens with no scene"): the ray of a hand of the input channel points
# at a row of the start dialog of a new game on the screen quad of the mod,
# and the trigger clicks.
#
#   screen_ray_run.ps1 -Name r1 -Dll <build>\src\mod\Release\ddraw.dll
#
# The run starts a new game (no test save, -NEWGAME of desk_run.ps1) with the
# logos (intro_skip_test_frame=0), intro_vr=0 and intro_skip=0, so the
# Lionhead logo and the start dialog of a new game (SkipBox) over it are
# frames with no scene, and only a button of the mouse of the game ends the
# logo. It has backend=vulkan, one_walk=1, ui_panel=1 and xr_input=1:
# bwvr_input_writer.exe (bwvr\build\test\input_writer) runs a script into
# bwvr-input.chan of the game folder, as the headset app writes its input,
# 90 slots a second. The script waits for the line of the log that names the
# start dialog, aims the right hand at the row -Row of the dialog on the
# screen quad of the mod and pulls the trigger. The place of the row is the
# pixel of dialog.ps1 from the middle of the render, and the render size is
# the value of the game in the registry (a test only reads it).
#
# WHAT THE GAME DOES WITH THE CLICK. The loop of the Lionhead logo (DoLogo)
# ends on a button of the mouse (0x005FA09F). It does not give the buttons to
# the dialog box: only the loops of FrontEnd::JustDoABox, of the front end
# (0x0053CDC0) and of the scene copy the buttons of LHMouse (0x00E85304) to
# the buttons of the box (0x00C4CD0C). So the click ends the logo, and the
# box stays up over the first frames of the scene, as with a mouse.
#
# -Row normal ("Start the game normally"), select ("Skip straight to the
# Creature select"), skip (the default: "Skip all of the Creature
# tutorial") or keep ("Keep your old Creature"). -More @{...}: more ini keys, for example
# screen_pointer = 0 (the old way: the press does nothing, and the logo plays
# to its end).
#
# After the run: the lines of the log of the screen quad, of the buttons of
# the controller and of the dialog boxes, and the time of frame 450 of the
# scene after the start of the game process. RAY RUN DONE when the mod took
# a press on the quad, the left button of the game went down, and the frames
# of the scene came after it (the line "a dialog box is up now, frame 1": the
# start dialog over the first frame of the scene). The output is in
# desk-<Name>: bwvr.log, writer.txt and script.txt.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [int]$Seconds = 120,
    [ValidateSet('normal', 'select', 'skip', 'keep')][string]$Row = 'skip',
    [hashtable]$More = @{}
)
$ErrorActionPreference = 'Continue'
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'dialog.ps1')
$writer = Join-Path $sp '..\..\build\test\input_writer\Release\bwvr_input_writer.exe'
if (-not (Test-Path -LiteralPath $writer)) { "NOT RUN: no test writer at $writer. Build with .\dev.ps1 build"; exit 6 }
$size = Get-BwvrRenderSize
if (-not $size) { 'NOT RUN: the game has no render size in the registry. Start the launcher one time'; exit 8 }
$w = $size[0]; $h = $size[1]
$game = Get-BwvrGame
$chan = Join-Path $game 'bwvr-input.chan'
$gameLog = Join-Path $game 'bwvr.log'
$out = Join-Path $sp ("desk-" + $Name)
New-Item -ItemType Directory -Force -Path $out | Out-Null
Remove-Item -LiteralPath $chan -Force -ErrorAction SilentlyContinue

# The pixel of the row on the render (dialog.ps1).
$rowX = [int]($w / 2 + $script:DialogRowX)
$rowY = [int]($h / 2 + $script:DialogRowY[$Row])
$scriptFile = Join-Path $out 'script.txt'
@(
    '# A row of the start dialog of a new game by the ray of the right hand',
    'wait the dialog box SkipBox is up',
    'slots 90',
    "aim right $rowX $rowY",
    'slots 30',
    'trigger right 1',
    'slots 12',
    'trigger right 0',
    'slots 900'
) | Set-Content -LiteralPath $scriptFile -Encoding ascii
"the render is $w by $h. The row '$Row' is at the pixel $rowX $rowY"

$keys = @{
    backend = 'vulkan'; one_walk = 1; ui_panel = 1; intro_skip_test_frame = 0; intro_vr = 0
    intro_skip = 0; xr_input = 1 }
foreach ($k in $More.Keys) { $keys[$k] = $More[$k] }

$p = Start-Process -FilePath $writer -NoNewWindow -PassThru `
    -ArgumentList @('script', "`"$chan`"", "`"$scriptFile`"", "$($Seconds + 60)", '2100', '90', "`"$gameLog`"", "$w", "$h") `
    -RedirectStandardOutput (Join-Path $out 'writer.txt') -RedirectStandardError (Join-Path $out 'writer-err.txt')
$null = $p.Handle  # keeps the exit code
"the test writer runs (process $($p.Id)): it runs $scriptFile"

& (Join-Path $sp 'desk_run.ps1') -Name $Name -Dll $Dll -Seconds $Seconds -Extra $keys |
    Tee-Object -Variable runOut
if (Test-BwvrVerdict $runOut 'NOT RUN') {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    'NOT RUN: the desktop run did not start'
    exit 7
}
if (-not $p.WaitForExit(($Seconds + 120) * 1000)) {
    'the test writer did not end: it is stopped'
    Stop-Process -Id $p.Id -Force
}
"the test writer ended with the code $($p.ExitCode)"
Get-Content -LiteralPath (Join-Path $out 'writer.txt') | Select-Object -Last 12
if (Test-Path -LiteralPath $chan) {
    try { Move-Item -LiteralPath $chan -Destination (Join-Path $out 'bwvr-input.chan') -Force } catch { "WARNING: $chan was not moved out of the game folder: $_" }
}

$log = Join-Path $out 'bwvr.log'
if (-not (Test-Path -LiteralPath $log)) { "RAY RUN FAULT: no log at $log"; exit 1 }
'== the lines of the screen quad, of the buttons, of the logos and of the dialog boxes in the log'
Select-String -LiteralPath $log -Pattern '^(controllers  : (screen|the left button|the controller lets go|the key|a dialog box)|setup box    :|intro skip   :|xr input     : the first slot)' |
    Select-Object -First 40 | ForEach-Object { '   ' + $_.Line }
$lines = @(Get-Content -LiteralPath $log)
$press = -1; $down = -1; $scene = -1; $at450 = ''
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($press -lt 0 -and $lines[$i] -match 'screen quad: the right trigger presses') { $press = $i }
    if ($press -ge 0 -and $down -lt 0 -and $lines[$i] -match '^controllers  : the left button is down') { $down = $i }
    if ($scene -lt 0 -and $lines[$i] -match '^controllers  : a dialog box is up now, frame 1$') { $scene = $i }
    if ($at450 -eq '' -and $lines[$i] -match '^stereo       : .*frame 450, ([0-9.]+) s after the start of the game process') { $at450 = $Matches[1] }
}
"   the press on the screen quad: $(if ($press -ge 0) { 'line ' + ($press + 1) } else { 'none' }), the left button down for the game: $(if ($down -ge 0) { 'line ' + ($down + 1) } else { 'none' }), the first frame of the scene with the start dialog: $(if ($scene -ge 0) { 'line ' + ($scene + 1) } else { 'none' }), frame 450 of the scene $(if ($at450) { "$at450 s after the start of the game process" } else { 'not in the log' })"
if ($press -ge 0 -and $down -gt $press -and $scene -gt $down) { 'RAY RUN DONE: the game took the click of the ray on the screen quad: the Lionhead logo ended on it' }
else { 'RAY RUN FAULT: the mod took no press on the quad, or the frames of the scene did not come after the press' }
