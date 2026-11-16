# Gives the game folder to the desktop test scripts. dev.ps1 setup writes it
# to local\game-path.txt at the root of the repo. The folder local is not
# tracked, so each contributor has his own.
function Get-BwvrGame {
    $repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
    $file = Join-Path $repo 'local\game-path.txt'
    if (-not (Test-Path -LiteralPath $file)) {
        throw 'No game folder is set. Run this first: .\dev.ps1 setup -Game "<your game folder>"'
    }
    $game = (Get-Content -LiteralPath $file -TotalCount 1).Trim()
    if (-not (Test-Path -LiteralPath (Join-Path $game 'runblack.exe'))) {
        throw "There is no runblack.exe in the game folder of local\game-path.txt: $game"
    }
    return $game
}

# Empty when the mod was started with its launcher in the game folder at
# least one time. Else the reason. The launcher writes bwvr.ini and gives the
# game its display size (a value of the game in the registry, which a test
# only reads). Before that the game renders 800 by 600 and has no settings
# file, and no recipe of the scripted controller fits. A game that is set to
# full screen is a reason too: its picture is over each window of the PC, and
# the launcher sets a window with "Play in VR".
function Test-BwvrFirstStart([string]$Game) {
    if (-not (Test-Path -LiteralPath (Join-Path $Game 'bwvr.ini'))) {
        return 'there is no bwvr.ini in the game folder'
    }
    $setup = Get-ItemProperty 'HKCU:\Software\Lionhead Studios Ltd\Black & White\BWSetup' -ErrorAction SilentlyContinue
    if (-not $setup -or -not $setup.ScreenW -or [int]$setup.ScreenW -lt 1024) {
        return 'the game has no display size from the launcher yet (it renders 800 by 600)'
    }
    if ($null -eq $setup.FullScreen -or [int]$setup.FullScreen -ne 0) {
        return 'the game is set to full screen in the registry'
    }
    return ''
}

# The folder that the desktop test scripts of each git worktree of this PC
# share. It holds the lock of a run (desk.lock) and the owner's "test now"
# flag (test-now-until.txt). It is in the temp folder of the user, so it is
# not in the repo and not in the game folder.
function Get-BwvrDeskShared {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) 'bwvr-desk'
    [void](New-Item -ItemType Directory -Force -Path $dir)
    return $dir
}

# True while the owner's "test now" holds: the time in test-now-until.txt is
# in the future. The file is in the shared folder. A file beside the scripts
# counts too, for the scripts of that worktree only.
function Test-BwvrTestNow {
    foreach ($dir in (Get-BwvrDeskShared), $PSScriptRoot) {
        $flag = Join-Path $dir 'test-now-until.txt'
        if (Test-Path -LiteralPath $flag) {
            try { if ((Get-Date) -lt [datetime]((Get-Content -LiteralPath $flag -Raw).Trim())) { return $true } } catch { }
        }
    }
    return $false
}

# True when a line of the output of a run script starts with the given
# verdict (NOT RUN, RUN DONE, GAVE UP). The test keeps the case and reads
# only the start of a line, so a line of a log that has the same words in it
# does not count.
function Test-BwvrVerdict($Output, [string]$Verdict) {
    return [bool](@($Output | ForEach-Object { "$_" -split "`r?`n" }) -cmatch ('^' + [regex]::Escape($Verdict)))
}

# The mod DLL of the build of this repo.
function Get-BwvrBuiltDll {
    $repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
    return (Join-Path $repo 'bwvr\build\src\mod\Release\ddraw.dll')
}

# The folder of the test saves: local\test-saves of the MAIN checkout of the
# repo, so each git worktree of the repo uses the same saves. A test save is
# a saved game that the game wrote in a test run (make_test_save.ps1). git
# does not track the folder local, and a test save must not go into the repo.
function Get-BwvrTestSaves {
    $repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
    $main = $repo
    try {
        $common = & git -C $repo rev-parse --path-format=absolute --git-common-dir 2>$null
        if ($LASTEXITCODE -eq 0 -and $common) {
            $parent = Split-Path -Parent (("$common").Trim() -replace '/', '\')
            if (Test-Path -LiteralPath (Join-Path $parent 'bwvr\tools\desk')) { $main = $parent }
        }
    } catch { }
    return (Join-Path $main 'local\test-saves')
}

# The folder of one test save, or null when it is not there. The files of
# the save are in its folder 20, and save.txt says what the save is.
function Get-BwvrTestSave([string]$Name) {
    if ($Name -notmatch '^[A-Za-z0-9_-]+$') { return $null }
    $dir = Join-Path (Get-BwvrTestSaves) $Name
    if (Test-Path -LiteralPath (Join-Path $dir '20\20.sav')) { return $dir }
    return $null
}

# The test save that a run script starts in: the name when the save is there.
# When it is not there, one run of make_test_save.ps1 makes it (the saves
# "pen" and "temple" have a recipe there). Empty when the save was not made:
# the run script then starts in a new game. The lines go to the host, not to
# the result.
function Confirm-BwvrTestSave([string]$Name, [string]$Dll, [int]$Minutes = 115) {
    if (Get-BwvrTestSave $Name) { return $Name }
    Write-Host "the test save '$Name' is not there: one run makes it"
    & (Join-Path $PSScriptRoot 'make_test_save.ps1') -Name $Name -Dll $Dll -Minutes $Minutes |
        Select-Object -Last 1 | ForEach-Object { Write-Host $_ }
    if (Get-BwvrTestSave $Name) { return $Name }
    Write-Host "the test save '$Name' was not made, so the run starts in a new game"
    return ''
}

# The length in seconds of a run in a test save, from the steps of its recipe.
# The controller places the Hand about 12 s after the start of the game
# process, and that frame is the play frame of the recipe. The last step
# comes (its frame - the play frame) frames later. The number is for a game
# that runs at 40 frames a second or more, with 4 s after the last step. A
# word "look" holds a state to the end of the run, so only its start counts.
# -More gives frames of test keys that come after the last step. -Most is the
# length that the same recipe has in a new game, less the seconds that the
# start in the test save is shorter: the result is never more than that.
function Get-BwvrSaveSeconds([string]$Steps, [int]$PlayFrame, [int]$Most = 0, [int[]]$More = @()) {
    $last = $PlayFrame
    foreach ($m in [regex]::Matches($Steps, '([a-z_0-9]+)@(\d+)\+(\d+)')) {
        $end = [int]$m.Groups[2].Value
        if ($m.Groups[1].Value -ne 'look') { $end += [int]$m.Groups[3].Value }
        if ($end -gt $last) { $last = $end }
    }
    foreach ($f in $More) { if ($f -gt $last) { $last = $f } }
    $seconds = 12 + [int][Math]::Ceiling(($last - $PlayFrame) / 40.0) + 4
    if ($Most -gt 0 -and $seconds -gt $Most) { $seconds = $Most }
    return $seconds
}

# The renderer of the player's bwvr.ini in the game folder: the value of the
# key backend, or empty when the key is not there.
function Get-BwvrBackend {
    $ini = Join-Path (Get-BwvrGame) 'bwvr.ini'
    if (-not (Test-Path -LiteralPath $ini)) { return '' }
    $m = Select-String -LiteralPath $ini -Pattern '^\s*backend\s*=\s*(\S+)' | Select-Object -First 1
    if ($m) { return $m.Matches[0].Groups[1].Value }
    return ''
}

# The flag LARGE_ADDRESS_AWARE of runblack.exe, as the launcher sets it
# (src/common/large_address.h). Get gives 'on', 'off' or 'other' (not the
# v1.42 file). Set makes the file have the flag or not: it keeps a copy of the
# plain file as runblack.exe.before-bwvr, writes the two bytes of the field,
# and compares the SHA-1 after. It gives the state after the call.
$script:BwvrExeSha1Plain = '9c9aea96f046911ab1b7e496fae511ed5ebbfefe'
$script:BwvrExeSha1Large = '2315844467e45e3d09497c3e91cb364077dfd028'
function Get-BwvrLargeAddress([string]$Game = (Get-BwvrGame)) {
    $sha = (Get-FileHash -Algorithm SHA1 -LiteralPath (Join-Path $Game 'runblack.exe')).Hash.ToLower()
    if ($sha -eq $script:BwvrExeSha1Large) { return 'on' }
    if ($sha -eq $script:BwvrExeSha1Plain) { return 'off' }
    return 'other'
}
function Set-BwvrLargeAddress([bool]$On, [string]$Game = (Get-BwvrGame)) {
    $now = Get-BwvrLargeAddress $Game
    $want = if ($On) { 'on' } else { 'off' }
    if ($now -eq 'other' -or $now -eq $want) { return $now }
    $exe = Join-Path $Game 'runblack.exe'
    $backup = Join-Path $Game 'runblack.exe.before-bwvr'
    $backupOk = (Test-Path -LiteralPath $backup) -and
        ((Get-FileHash -Algorithm SHA1 -LiteralPath $backup).Hash.ToLower() -eq $script:BwvrExeSha1Plain)
    if (-not $backupOk -and $now -eq 'off') { Copy-Item -LiteralPath $exe -Destination $backup -Force }
    $stream = [IO.File]::Open($exe, 'Open', 'ReadWrite', 'Read')
    try {
        $head = New-Object byte[] 4096
        [void]$stream.Read($head, 0, $head.Length)
        $at = [BitConverter]::ToInt32($head, 0x3C) + 22
        $value = [BitConverter]::ToUInt16($head, $at)
        $value = if ($On) { $value -bor 0x20 } else { $value -band (-bnot 0x20) }
        [void]$stream.Seek($at, 'Begin')
        $stream.WriteByte([byte]($value -band 0xFF))
        $stream.WriteByte([byte](($value -shr 8) -band 0xFF))
        $stream.Flush()
    } finally {
        $stream.Close()
    }
    $after = Get-BwvrLargeAddress $Game
    if ($after -eq 'other' -and (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $backup -Destination $exe -Force
        $after = Get-BwvrLargeAddress $Game
    }
    return $after
}

# True when this process has administrator rights (the mandatory label of
# its token is high or more). A game that a test starts gets the same rights.
function Test-BwvrElevated {
    $text = (& (Join-Path $env:windir 'System32\whoami.exe') /groups /fo csv 2>$null) -join "`n"
    if ($text -match '"S-1-16-(\d+)"') { return [int]$Matches[1] -ge 12288 }
    return $false
}

# The folders of the files of the player: the Profiles folder,
# Scripts\CreatureMind and MemoryLeaks.txt. The game names these files with a
# path from its current folder (.\Profiles\...). In its start that folder is
# the game folder: the game reads slot 20 there, and a new game writes its
# first save there. Later the game changes its current folder to the folder of
# the value GameDir of its key in the registry, which a test only reads, and
# the writes of the exit path go there. On most PCs GameDir is the game
# folder. A PC with a second install of the game can have the other install
# there.
# THE VIRTUALSTORE. The game is a 32 bit program with no manifest, so Windows
# virtualizes its file access when it has no administrator rights. For a
# folder in Program Files, in ProgramData or in Windows, each write that the
# folder refuses goes to the same path in the VirtualStore of the user
# (Get-BwvrVirtualStore). After that, the game uses the VirtualStore copy of
# each file that is there, also for a read and also when the folder lets the
# user write now. So the files of the player can be in the folder, in its
# VirtualStore, or in both. Windows does not virtualize a file with the
# extension .inf, so the game needs save_virtual_store=1 of the mod for
# SaveGame.inf there (docs/backend.md, "A save in the VirtualStore"). The
# result:
#   Folder     the folder that gets the writes of the game after its start
#   Roots      each folder in which a test run must keep the files of the
#              player: Folder, the game folder, the GameDir folder and the
#              VirtualStore of each of the two
#   Game       the game folder
#   GameStore  the VirtualStore of the game folder that the game uses, or null
#   Why        one line that says where the writes go
function Get-BwvrPlayerFolders([string]$Game) {
    $gameFull = [System.IO.Path]::GetFullPath($Game).TrimEnd('\')
    $dir = $gameFull
    $key = Get-ItemProperty 'HKCU:\Software\Lionhead Studios Ltd\Black & White' -ErrorAction SilentlyContinue
    if ($key -and $key.GameDir) {
        $named = [System.IO.Path]::GetFullPath("$($key.GameDir)".Trim()).TrimEnd('\')
        if (Test-Path -LiteralPath $named) { $dir = $named }
    }
    $elevated = Test-BwvrElevated
    $gameStore = if ($elevated) { $null } else { Get-BwvrVirtualStore $gameFull }
    $dirStore = if ($elevated) { $null } else { Get-BwvrVirtualStore $dir }
    $folder = $dir
    $place = if ($dir -eq $gameFull) { "the game folder $dir" } else { "$dir, the folder of the value GameDir in the registry, and not in the game folder $gameFull" }
    $why = "the game writes the files of the player in $place"
    if ($dirStore) {
        # The game uses the VirtualStore when the profiles are there, or when
        # the folder refuses a write of the user.
        $why = $null
        if (Test-Path -LiteralPath (Join-Path $dirStore 'Profiles')) {
            $why = "the game writes the files of the player in $dirStore, the VirtualStore of $place. The Profiles folder is there, and the game uses the VirtualStore copy of each file that is there"
        } elseif (-not (Test-BwvrCanWrite $dir)) {
            $why = "the game writes the files of the player in $dirStore, the VirtualStore of $place. The user cannot write in that folder"
        }
        if ($why) { $folder = $dirStore } else { $why = "the game writes the files of the player in $place. The user can write there, and its VirtualStore has no Profiles folder" }
    }
    $roots = @(@($folder, $gameFull, $dir, $gameStore, $dirStore) | Where-Object { $_ } | Select-Object -Unique)
    return @{ Folder = $folder; Roots = $roots; Game = $gameFull; GameStore = $gameStore; Why = $why }
}

# The VirtualStore of a folder, by the rule of Windows: the path of the folder
# with no drive, in %LOCALAPPDATA%\VirtualStore. Only a folder in Program
# Files, in ProgramData or in Windows has one. Null for each other folder.
function Get-BwvrVirtualStore([string]$Dir) {
    $full = [System.IO.Path]::GetFullPath($Dir).TrimEnd('\')
    foreach ($root in $env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramData, $env:windir) {
        if ($root -and $full.StartsWith($root.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            return (Join-Path (Join-Path $env:LOCALAPPDATA 'VirtualStore') $full.Substring(3))
        }
    }
    return $null
}

# True when this process can make a file in the folder. The test makes an
# empty file and deletes it at once.
function Test-BwvrCanWrite([string]$Dir) {
    $probe = Join-Path $Dir ("bwvr-write-test-$PID.tmp")
    try {
        $fs = [System.IO.File]::Open($probe, 'CreateNew', 'Write', 'None')
        $fs.Close()
        [System.IO.File]::Delete($probe)
        return $true
    } catch {
        return $false
    }
}

# The folder of the profile that the game uses: Profiles\<folder> of -Root.
# The game reads slot 20 in the profile of the game folder in its start, and
# it writes slot 20 on its exit path in the profile of the folder that
# Get-BwvrPlayerFolders gives. The game keeps the name of the profile in the
# registry, which a test only reads. The folder has the name with a "_"
# before each character. Null when the name is not there, and when the
# profile is not in -Root (with -Make, not in -Root and not in GameDir or its
# VirtualStore: the VirtualStore can get the profile with the first write).
function Get-BwvrProfileFolder([string]$Root, [switch]$Make) {
    $key = Get-ItemProperty 'HKCU:\Software\Lionhead Studios Ltd\Black & White\LHMultiplayer' -ErrorAction SilentlyContinue
    if (-not $key -or -not $key.Profile) { return $null }
    $name = [System.Text.Encoding]::Unicode.GetString([byte[]]$key.Profile).TrimEnd([char]0)
    if ($name -eq '') { return $null }
    $folder = -join ($name.ToCharArray() | ForEach-Object { "_$_" })
    $path = Join-Path (Join-Path $Root 'Profiles') $folder
    if (Test-Path -LiteralPath $path) { return $path }
    if ($Make) {
        $key = Get-ItemProperty 'HKCU:\Software\Lionhead Studios Ltd\Black & White' -ErrorAction SilentlyContinue
        if ($key -and $key.GameDir) {
            $named = "$($key.GameDir)".Trim()
            foreach ($dir in @($named, (Get-BwvrVirtualStore $named))) {
                if ($dir -and (Test-Path -LiteralPath (Join-Path (Join-Path $dir 'Profiles') $folder))) { return $path }
            }
        }
    }
    return $null
}

# The profile folders of a run with a test save (desk_run.ps1). The result:
#   Read   the profile whose slot 20 the game reads in its start: the profile
#          in the VirtualStore of the game folder when it is there (the game
#          then uses the VirtualStore copy of each file), else the profile in
#          the game folder. Null when neither is there.
#   Later  the profile that gets the writes of the exit path, in Folder of
#          Get-BwvrPlayerFolders (Get-BwvrProfileFolder -Make). Can be null.
#   All    each profile folder of the files of the player: Read, Later, and
#          the profile in each other root. Each one gets the test save, so
#          the game finds the same slot 20 in each place that it can read.
function Get-BwvrSaveProfiles($Folders) {
    $read = $null
    if ($Folders.GameStore) { $read = Get-BwvrProfileFolder $Folders.GameStore }
    if (-not $read) { $read = Get-BwvrProfileFolder $Folders.Game }
    $later = Get-BwvrProfileFolder $Folders.Folder -Make
    $all = @($read, $later)
    foreach ($root in $Folders.Roots) { $all += Get-BwvrProfileFolder $root }
    return @{ Read = $read; Later = $later; All = @($all | Where-Object { $_ } | Select-Object -Unique) }
}

# Removes a folder of the test saves with each file in it. It removes each
# file and then each empty folder, and it does nothing with a path that is
# not in the folder of the test saves.
function Remove-BwvrTestSaveFolder([string]$Dir) {
    $root = (Get-BwvrTestSaves) + '\'
    if (-not $Dir.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) { throw "not a folder of the test saves: $Dir" }
    if (-not (Test-Path -LiteralPath $Dir)) { return }
    foreach ($f in (Get-ChildItem -LiteralPath $Dir -Recurse -File -Force)) { [System.IO.File]::Delete($f.FullName) }
    foreach ($d in (Get-ChildItem -LiteralPath $Dir -Recurse -Directory -Force | Sort-Object { $_.FullName.Length } -Descending)) {
        [System.IO.Directory]::Delete($d.FullName, $false)
    }
    [System.IO.Directory]::Delete($Dir, $false)
}

# The files of a folder with the hash of each one, as one text. Two folders
# hold the same files when their texts are the same.
function Get-BwvrFolderText([string]$Dir) {
    if (-not (Test-Path -LiteralPath $Dir)) { return '' }
    return (@(Get-ChildItem -LiteralPath $Dir -Recurse -File -Force | ForEach-Object {
        $_.FullName.Substring($Dir.Length) + ' ' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    } | Sort-Object) -join "`n")
}

# The seconds from the start of the game process to a frame of the scripted
# controller, from the clock of a run log: the line "game start" and the
# lines with the frame rate have a frame and its time. The time of a frame
# between two of them is on the line between the two. Null when the log has
# no clock.
function Get-BwvrFrameSeconds([string]$Log, [int]$Frame) {
    if (-not (Test-Path -LiteralPath $Log)) { return $null }
    $clock = @(Select-String -LiteralPath $Log -Pattern '^(?:game start   : .*?frame (\d+)\b.*?|stereo       : .* fps .*\), frame (\d+)), ([0-9.]+) s after the start of the game process' | ForEach-Object {
        $g = $_.Matches[0].Groups
        $f = if ($g[1].Success) { $g[1].Value } else { $g[2].Value }
        [pscustomobject]@{ Frame = [int]$f; Seconds = [double]::Parse($g[3].Value, [cultureinfo]::InvariantCulture) }
    } | Sort-Object Frame)
    if ($clock.Count -eq 0) { return $null }
    if ($clock.Count -eq 1) { return [Math]::Round($clock[0].Seconds + ($Frame - $clock[0].Frame) / 60.0, 1) }
    $a = $clock[0]; $b = $clock[1]
    for ($i = 1; $i -lt $clock.Count; $i++) {
        $a = $clock[$i - 1]; $b = $clock[$i]
        if ($Frame -le $b.Frame) { break }
    }
    if ($b.Frame -eq $a.Frame) { return [Math]::Round($a.Seconds, 1) }
    return [Math]::Round($a.Seconds + ($Frame - $a.Frame) * ($b.Seconds - $a.Seconds) / ($b.Frame - $a.Frame), 1)
}

# One choice of the ring with its button (docs/controllers.md, "The ring"):
# the words of the scripted controller, and the two places of the left hand
# for fake_controller_left_points. The button goes down at -Frame. The left
# hand steps 10 cm toward the slot 6 frames later. The button goes up 30
# frames after -Frame: that is the choice. The hand steps back 6 frames after
# it. -Slot counts from the top and clockwise, 0 to 7. The ring is on the
# plane through the hand that faces the middle of the head, with no roll, so
# the step is on that plane. -Button is the face button of the layout: y
# with button_layout=1, and x with 0. -Hand is the place of the left hand in
# cm from the head: right, up, forward. The places are in whole cm, as the
# key takes them. Give the places of all choices of a run in the order of
# their frames.
function Get-BwvrRingChoice([int]$Slot, [int]$Frame, [string]$Button = 'y', [int[]]$Hand = @(-20, -30, 35)) {
    # The hand in the space of the runtime: x to the right, y up, z to the back.
    $h = @([double]$Hand[0], [double]$Hand[1], -[double]$Hand[2])
    $len = [Math]::Sqrt($h[0] * $h[0] + $h[1] * $h[1] + $h[2] * $h[2])
    # From the hand to the head, then the right and the up of the plane.
    $n = @((-$h[0] / $len), (-$h[1] / $len), (-$h[2] / $len))
    $flat = [Math]::Sqrt($n[0] * $n[0] + $n[2] * $n[2])
    $right = @(($n[2] / $flat), 0.0, (-$n[0] / $flat))
    $up = @(($n[1] * $right[2] - $n[2] * $right[1]), ($n[2] * $right[0] - $n[0] * $right[2]), ($n[0] * $right[1] - $n[1] * $right[0]))
    $turn = $Slot * [Math]::PI / 4
    $p = @(0..2 | ForEach-Object { $h[$_] + 10.0 * ([Math]::Sin($turn) * $right[$_] + [Math]::Cos($turn) * $up[$_]) })
    $to = '{0},{1},{2}' -f [int][Math]::Round($p[0]), [int][Math]::Round($p[1]), [int][Math]::Round(-$p[2])
    return @{
        Steps  = "$Button@$Frame+30 lpoint@$($Frame + 6)+1 lpoint@$($Frame + 36)+1"
        Points = "$to $($Hand[0]),$($Hand[1]),$($Hand[2])"
    }
}
