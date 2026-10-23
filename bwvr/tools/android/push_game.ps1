# Copies a game folder of Black & White into the folder `game` of the
# headset app (docs/android.md, "The game folder"), over adb.
#
#   push_game.ps1                       the folder of local\game-path.txt
#   push_game.ps1 -Game "C:\Black & White"
#   push_game.ps1 -Serial <adb serial>  when more than one device is there
#   push_game.ps1 -NoProfile            no Profiles folder and no Creature of
#                                       a profile (Scripts\CreatureMind\*.erc),
#                                       for a test
#
# A push of the whole folder (adb push <folder> ...) does not work in the
# folder of an app on a Quest 3: adb makes the first folder, and then the
# make of each folder in it fails ("remote secure_mkdirs failed: Operation
# not permitted"). So this script makes each folder with mkdir -p first and
# then pushes each file. The app must have started one time before, so its
# folder belongs to the app. The ini and the logs of the mod of the PC and
# MemoryLeaks.txt stay on the PC.
#
# The last line is COPIED or NOT COPIED.
param(
    [string]$Game,
    [string]$Serial,
    [switch]$NoProfile
)
$ErrorActionPreference = 'Stop'
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'android_env.ps1')
$adb = $Adb
if (-not $Game) {
    $pathFile = Join-Path $sp '..\..\..\local\game-path.txt'
    if (-not (Test-Path -LiteralPath $pathFile)) { 'NOT COPIED: give -Game, or write the game folder into local\game-path.txt'; exit 2 }
    $Game = (Get-Content -LiteralPath $pathFile -TotalCount 1).Trim()
}
if (-not (Test-Path -LiteralPath (Join-Path $Game 'runblack.exe'))) { "NOT COPIED: $Game has no runblack.exe"; exit 3 }
$dev = @('-s', $Serial)
if (-not $Serial) { $dev = @() }
$dest = '/sdcard/Android/data/org.bwvr.quest/files/game'

$ErrorActionPreference = 'Continue'
$there = (& $adb @dev shell "ls -d $dest" 2>&1 | Out-String).Trim()
if ($there -notmatch [regex]::Escape($dest) -or $there -match 'No such file') {
    "NOT COPIED: there is no $dest. Start the app one time first (adb: $there)"; exit 4
}

function Skip([string]$rel) {
    $top = $rel.Split('\')[0]
    if ($top -eq 'MemoryLeaks.txt') { return $true }
    if ($rel -notmatch '\\' -and ($top -like 'bwvr.ini*' -or $top -like 'bwvr*.log' -or $top -eq 'bwvr-hint-counts.ini')) { return $true }
    if ($NoProfile -and $top -eq 'Profiles') { return $true }
    if ($NoProfile -and $rel -like 'Scripts\CreatureMind\*' -and $rel.EndsWith('.erc')) { return $true }
    return $false
}

$t0 = Get-Date
$dirs = @(Get-ChildItem -LiteralPath $Game -Recurse -Directory -Force |
    ForEach-Object { $_.FullName.Substring($Game.Length).TrimStart('\') } | Where-Object { -not (Skip $_) })
$batch = @()
foreach ($d in $dirs) {
    $batch += "mkdir -p '$dest/$($d.Replace('\', '/'))'"
    if ($batch.Count -ge 40) { & $adb @dev shell ($batch -join '; ') | Out-Null; $batch = @() }
}
if ($batch.Count) { & $adb @dev shell ($batch -join '; ') | Out-Null }
$files = @(Get-ChildItem -LiteralPath $Game -Recurse -File -Force |
    Where-Object { -not (Skip $_.FullName.Substring($Game.Length).TrimStart('\')) })
$bytes = ($files | Measure-Object -Property Length -Sum).Sum
"$($dirs.Count) folders, $($files.Count) files, {0:N1} MB from $Game" -f ($bytes / 1MB)
$bad = 0
foreach ($f in $files) {
    $rel = $f.FullName.Substring($Game.Length).TrimStart('\').Replace('\', '/')
    $out = & $adb @dev push $f.FullName "$dest/$rel" 2>&1 | Out-String
    if ($out -notmatch '1 file pushed') { $bad++; "FAULT $rel : $($out.Trim())" }
}
$kb = (& $adb @dev shell "du -s -k $dest" 2>&1 | Out-String).Trim()
"{0:N0} s, {1} faults. The game folder of the app: $kb" -f ((Get-Date) - $t0).TotalSeconds, $bad
if ($bad -eq 0) { 'COPIED. Start the app again, so it checks runblack.exe' } else { 'NOT COPIED: see the faults above'; exit 5 }
