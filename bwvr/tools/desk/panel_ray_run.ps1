# One desktop run of the pointer on the panel quads (docs/controllers.md,
# section 13): the ray of a hand of the input channel points at a row of the
# start dialog of a new game on the menu panel quad of the mod, in a frame of
# the scene, and the trigger clicks the row and then OK.
#
#   panel_ray_run.ps1 -Name p1 -Dll <build>\src\mod\Release\ddraw.dll
#
# The run starts a new game (no test save, -NEWGAME of desk_run.ps1) with the
# logos (intro_skip_test_frame=0), intro_vr=0 and intro_skip=0, so the
# Lionhead logo and the start dialog of a new game (SkipBox) over it are
# frames with no scene. It has backend=vulkan, one_walk=1, ui_panel=1 and
# xr_input=1: bwvr_input_writer.exe (bwvr\build\test\input_writer) runs a
# script into bwvr-input.chan of the game folder, as the headset app writes
# its input, 90 slots a second. The script:
#
#   1. waits for the line of the log that names the start dialog, aims the
#      right hand at the place of the row skip on the screen quad (as
#      screen_ray_run.ps1) and pulls the trigger.
#      The loop of the Lionhead logo takes the click as the end of the logo
#      (screen_ray_run.ps1), and the frames of the scene come with the start
#      dialog over them, on the menu panel quad.
#   2. waits for the line of the mod with the menu panel quad (panel), aims
#      at the row on the panel and pulls the trigger, then aims at OK and
#      pulls the trigger.
#
# The places of the row and of OK are the pixels of dialog.ps1 from the
# middle of the render. The panel image has the size of the render, so they
# are pixels of the panel image too. The render size is the value of the
# game in the registry (a test only reads it).
#
# -Row normal ("Start the game normally": no click on a row, only OK),
# select (the default: "Skip straight to the Creature select"), skip ("Skip
# all of the Creature tutorial") or keep ("Keep your old Creature"), the
# names of the rows of dialog.ps1 from the top. -More @{...}: more ini
# keys, for example panel_pointer = 0 (the old way: the pointer comes from
# the eye view).
#
# -PanelQuad "<text>": the menu panel quad as the line of the mod gives it
# after "The menu panel quad at " (from a run with panel_pointer=1). The
# writer then aims at that quad after the first frame of the scene with the
# dialog, with no wait for the line of the mod. For a run with
# panel_pointer = 0, in which the mod writes no such line: the ray aims at
# the same places, and the run says whether the start dialog went.
#
# After the run: the lines of the log of the panel quads, of the buttons of
# the controller and of the dialog boxes. PANEL RUN DONE when the mod took
# the presses on the panel at the pixels of the row and of OK (within 4
# pixels), the left button of the game went down after each one, and the
# start dialog went after the press on OK ("setup box : no dialog box is up
# now"). The output is in desk-<Name>: bwvr.log, writer.txt and script.txt.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [int]$Seconds = 150,
    [ValidateSet('normal', 'select', 'skip', 'keep')][string]$Row = 'select',
    [hashtable]$More = @{},
    [string]$PanelQuad = ''
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

# The pixels of the row and of OK on the render (dialog.ps1).
$skipX = [int]($w / 2 + $script:DialogRowX)
$skipY = [int]($h / 2 + $script:DialogRowY['skip'])
$rowX = [int]($w / 2 + $script:DialogRowX)
$rowY = [int]($h / 2 + $script:DialogRowY[$Row])
$okX = [int]($w / 2 + $script:DialogOkX)
$okY = [int]($h / 2 + $script:DialogOkY)
$steps = @(
    '# The end of the Lionhead logo by a click of the ray on the screen quad',
    'wait the dialog box SkipBox is up',
    'slots 90',
    "aim right $skipX $skipY",
    'slots 30',
    'trigger right 1',
    'slots 12',
    'trigger right 0',
    '# The start dialog on the menu panel quad in the frames of the scene')
if ($PanelQuad -ne '') {
    $steps += @('wait controllers  : a dialog box is up now, frame 1', "panelquad The menu panel quad at $PanelQuad")
} else {
    $steps += @('panel')
}
$steps += @('slots 180')
$wantPress = @()
if ($Row -ne 'normal') {
    $steps += @("aim right $rowX $rowY", 'slots 30', 'trigger right 1', 'slots 12', 'trigger right 0', 'slots 60')
    $wantPress += , @($rowX, $rowY)
}
$steps += @("aim right $okX $okY", 'slots 30', 'trigger right 1', 'slots 12', 'trigger right 0', 'slots 900')
$wantPress += , @($okX, $okY)
$scriptFile = Join-Path $out 'script.txt'
$steps | Set-Content -LiteralPath $scriptFile -Encoding ascii
"the render is $w by $h. The row '$Row' is at the pixel $rowX $rowY, OK at $okX $okY"

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
Get-Content -LiteralPath (Join-Path $out 'writer.txt') | Select-Object -Last 16
if (Test-Path -LiteralPath $chan) {
    try { Move-Item -LiteralPath $chan -Destination (Join-Path $out 'bwvr-input.chan') -Force } catch { "WARNING: $chan was not moved out of the game folder: $_" }
}

$log = Join-Path $out 'bwvr.log'
if (-not (Test-Path -LiteralPath $log)) { "PANEL RUN FAULT: no log at $log"; exit 1 }
'== the lines of the panel quads, of the buttons and of the dialog boxes in the log'
Select-String -LiteralPath $log -Pattern '^(controllers  : (panel|screen quad|the left button|the right button|the controller lets go|a dialog box)|setup box    :|xr layers    : (a dialog|no dialog)|pointer      : the)' |
    Select-Object -First 60 | ForEach-Object { '   ' + $_.Line }
$lines = @(Get-Content -LiteralPath $log)
$presses = @()
$closed = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    $m = [regex]::Match($lines[$i], '^controllers  : panel quads: the right hand presses the left button at the pixel (\d+) (\d+)')
    if ($m.Success) {
        $down = -1
        for ($j = $i + 1; $j -lt [math]::Min($lines.Count, $i + 40); $j++) { if ($lines[$j] -match '^controllers  : the left button is down') { $down = $j; break } }
        if ($down -lt 0) { for ($j = [math]::Max(0, $i - 5); $j -lt $i; $j++) { if ($lines[$j] -match '^controllers  : the left button is down') { $down = $j } } }
        $presses += , @([int]$m.Groups[1].Value, [int]$m.Groups[2].Value, $i, $down)
    }
}
$ok = $presses.Count -eq $wantPress.Count
for ($k = 0; $k -lt $presses.Count; $k++) {
    $p0 = $presses[$k]
    $want = if ($k -lt $wantPress.Count) { $wantPress[$k] } else { @(-99, -99) }
    $near = [math]::Abs($p0[0] - $want[0]) -le 4 -and [math]::Abs($p0[1] - $want[1]) -le 4
    "   press $($k + 1) on the panel at the pixel $($p0[0]) $($p0[1]) (want $($want[0]) $($want[1])), log line $($p0[2] + 1), the left button down: $(if ($p0[3] -ge 0) { 'line ' + ($p0[3] + 1) } else { 'none' })"
    if (-not $near -or $p0[3] -lt 0) { $ok = $false }
}
if ($presses.Count -gt 0) {
    $last = $presses[$presses.Count - 1][2]
    for ($i = $last; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^setup box    : no dialog box is up now') { $closed = $i; break } }
}
"   presses on the panel: $($presses.Count) (want $($wantPress.Count)), the start dialog went after the last press: $(if ($closed -ge 0) { 'line ' + ($closed + 1) } else { 'no' })"
if ($PanelQuad -ne '') {
    # The run with the quad of the script: the presses of the left button in
    # the frames of the scene, and whether the start dialog went at all.
    $scene = -1; $downs = 0; $gone = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($scene -lt 0 -and $lines[$i] -match '^controllers  : a dialog box is up now, frame 1$') { $scene = $i }
        if ($scene -ge 0 -and $lines[$i] -match '^controllers  : the left button is down') { $downs++ }
        if ($scene -ge 0 -and $gone -lt 0 -and $lines[$i] -match '^setup box    : no dialog box is up now') { $gone = $i }
    }
    "   in the frames of the scene: the left button went down $downs times, and the start dialog $(if ($gone -ge 0) { 'went at line ' + ($gone + 1) } else { 'stayed up to the end of the run' })"
}
'== the lines of the log after the start dialog went'
if ($closed -ge 0) { $lines[$closed..([math]::Min($lines.Count - 1, $closed + 25))] | ForEach-Object { '   ' + $_ } }
if ($ok -and $closed -ge 0) { 'PANEL RUN DONE: the game took the clicks of the ray on the menu panel quad, and the start dialog went' }
else { 'PANEL RUN FAULT: the presses on the panel are not at the row and OK, the left button did not go down, or the start dialog stayed' }
