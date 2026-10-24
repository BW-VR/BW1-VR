<#
.SYNOPSIS
    Copies the profile of the player of Black & White on this PC to the game
    folder of the headset app (docs/android.md, "The profile of the PC"),
    over adb.

.DESCRIPTION
    A profile of the game is in two places:

      - the registry of the PC: the value Profile of
        HKCU\Software\Lionhead Studios Ltd\Black & White\LHMultiplayer (the
        name of the current player, REG_BINARY UTF-16) and the key
        LHMultiplayer\Profiles\<folder> with the values of that player (the
        name of the Creature, the file of the Creature, the options of the
        game, the key bindings, the alignment). <folder> is the name with a
        "_" before each character.
      - the files: Profiles\<folder> (the saved games, creature.lhp) and the
        Creature in Scripts\CreatureMind (the value "file" of the profile and
        Physique<file>).

    The script READS the values of the current profile from the registry and
    writes them into bwvr-profile.reg (only the value Profile and the key of
    the profile: no GameDir, no value of BWSetup). It pushes the files of the
    profile and then bwvr-profile.reg into the game folder of the app. At
    "Play" the app imports the values into the registry of the game in the
    Wine prefix (pc_profile=1, android/src/play_rules.h).

    The script never writes to the registry of the PC. It never changes,
    moves or deletes a file of the profile on the PC. It needs -Serial: with
    no serial it does nothing.

    The folders of the files: after its start the game reads and writes the
    files of the player in the folder of the value GameDir of the registry.
    When that folder is in Program Files and the game has no administrator
    rights, Windows puts each write of the game into the VirtualStore of the
    user, and the game reads a file of the VirtualStore before the file of
    GameDir. So by default the script takes each file from the VirtualStore
    of GameDir first, then from GameDir. -From gives other folders.

    The last line is COPIED, NOT COPIED, or DRY RUN.

.PARAMETER Serial
    The adb serial of the headset (adb devices). Necessary for a copy.

.PARAMETER DryRun
    Reads the registry and the folders, writes bwvr-profile.reg and the list
    of the files into -Out, and pushes nothing. Needs no device.

.PARAMETER Out
    With -DryRun: the folder for bwvr-profile.reg and files.txt.

.PARAMETER From
    The folders that hold Profiles and Scripts\CreatureMind, in the order in
    which the script looks for each file. By default the VirtualStore of
    GameDir and GameDir.

.PARAMETER WineUserReg
    A user.reg of a Wine prefix: the values of the profile come from it, not
    from the registry of the PC. For a test with a profile that the game made
    in the emulator (emu_game_test.ps1 -PullProfile). Needs -From.

.PARAMETER Replace
    Copies over a folder of the same profile that the game folder of the app
    has. With no -Replace the script stops when that folder is there: the
    game in the headset can have newer saves in it.
#>
[CmdletBinding()]
param(
    [string]$Serial,
    [switch]$DryRun,
    [string]$Out,
    [string[]]$From = @(),
    [string]$WineUserReg,
    [switch]$Replace
)
$ErrorActionPreference = 'Stop'
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path

function Stop-Profile([string]$Why, [int]$Code) {
    if ($DryRun) { "DRY RUN NOT DONE: $Why" } else { "NOT COPIED: $Why" }
    exit $Code
}

if ($DryRun) {
    if (-not $Out) { Stop-Profile 'give -Out, the folder for bwvr-profile.reg and the list of the files' 2 }
} elseif (-not $Serial) {
    Stop-Profile 'give -Serial, the adb serial of the headset (adb devices). The script does nothing with no serial' 2
}
if ($WineUserReg -and $From.Count -eq 0) { Stop-Profile 'with -WineUserReg give -From, the folder of the files of that profile' 2 }

$GameKey = 'Software\Lionhead Studios Ltd\Black & White'
$MultiKey = "$GameKey\LHMultiplayer"
$ProfilesKey = "$MultiKey\Profiles"

# A text of a .reg file: a backslash and a quote get a backslash. Only the
# printable characters of ASCII: the app takes no other byte.
function RegQuote([string]$Text, [string]$What) {
    foreach ($c in $Text.ToCharArray()) {
        if ([int]$c -lt 0x20 -or [int]$c -gt 0x7E) { Stop-Profile "$What has a character that is not printable ASCII. The app takes only such a text" 3 }
    }
    return '"' + $Text.Replace('\', '\\').Replace('"', '\"') + '"'
}
function HexList([byte[]]$Bytes) {
    return (@($Bytes | ForEach-Object { '{0:x2}' -f $_ }) -join ',')
}
function ProfileFolder([string]$Name) { return -join ($Name.ToCharArray() | ForEach-Object { "_$_" }) }

# THE VALUES. Each one: Name (the text between the quotes, with its escapes)
# and Data (the text after "=").
$values = New-Object System.Collections.Generic.List[object]
$profileData = ''
$profileName = ''
$folder = ''
$creatureFile = ''

if ($WineUserReg) {
    # The values of a Wine prefix: user.reg has the keys of HKCU with "\\"
    # for each backslash and a time after the key, and a long value goes on
    # over more lines with a "\" at the end.
    if (-not (Test-Path -LiteralPath $WineUserReg)) { Stop-Profile "there is no $WineUserReg" 3 }
    $lines = [IO.File]::ReadAllLines($WineUserReg)
    $sections = @{}
    $current = $null
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $m = [regex]::Match($line, '^\[(.*)\](?: \d+)?$')
        if ($m.Success) {
            $current = $m.Groups[1].Value.Replace('\\', '\')
            if (-not $sections.ContainsKey($current.ToLowerInvariant())) { $sections[$current.ToLowerInvariant()] = New-Object System.Collections.Generic.List[object] }
            continue
        }
        if ($null -eq $current -or $line -notmatch '^"') { continue }
        while ($line.EndsWith('\') -and $i + 1 -lt $lines.Count) { $i++; $line = $line.Substring(0, $line.Length - 1) + $lines[$i].TrimStart() }
        $v = [regex]::Match($line, '^"((?:[^"\\]|\\.)*)"=(.*)$')
        if (-not $v.Success) { continue }
        $sections[$current.ToLowerInvariant()].Add(@{ Name = $v.Groups[1].Value; Data = $v.Groups[2].Value })
    }
    $multi = $sections[$MultiKey.ToLowerInvariant()]
    $p = @()
    if ($multi) { $p = @($multi | Where-Object { $_.Name -eq 'Profile' }) }
    if ($p.Count -ne 1) { Stop-Profile "$WineUserReg has no value Profile of $MultiKey" 3 }
    $profileData = $p[0].Data
    $hex = [regex]::Match($profileData, '^hex:([0-9a-fA-F,]*)$')
    if (-not $hex.Success) { Stop-Profile "the value Profile of $WineUserReg is not REG_BINARY" 3 }
    [byte[]]$bytes = @($hex.Groups[1].Value.Split(',') | Where-Object { $_ -ne '' } | ForEach-Object { [Convert]::ToByte($_, 16) })
    $profileName = [Text.Encoding]::Unicode.GetString($bytes).TrimEnd([char]0)
    RegQuote $profileName 'The name of the profile' | Out-Null
    if ($profileName -eq '' -or $profileName.Contains('\')) { Stop-Profile "the value Profile has no name that can be a key ($profileName)" 3 }
    $folder = ProfileFolder $profileName
    $own =$sections["$ProfilesKey\$folder".ToLowerInvariant()]
    if (-not $own -or $own.Count -eq 0) { Stop-Profile "$WineUserReg has no key $ProfilesKey\$folder" 3 }
    foreach ($v in $own) {
        # The forms of data that the app takes (play_rules.h, ValueDataRight).
        if ($v.Data -notmatch '^(dword:[0-9a-fA-F]{8}|hex(\([0-9a-fA-F]{1,8}\))?:([0-9a-fA-F]{2}(,[0-9a-fA-F]{2})*)?|"(?:[^"\\]|\\["\\])*")$') {
            Stop-Profile "the value $($v.Name) of the profile in $WineUserReg has a form that the app does not take: $($v.Data)" 3
        }
        RegQuote $v.Name "The name of a value" | Out-Null
        $values.Add($v)
        if ($v.Name -eq 'file') { $creatureFile = $v.Data.Trim('"') }
    }
    "The values of the profile $profileName come from $WineUserReg (not from the registry of the PC)."
} else {
    # The values of the PC, read only: OpenSubKey with no write access.
    $hkcu = [Microsoft.Win32.Registry]::CurrentUser
    $multi = $hkcu.OpenSubKey($MultiKey, $false)
    if ($null -eq $multi) { Stop-Profile "the registry of the PC has no key HKCU\$MultiKey. The game has no profile on this PC" 3 }
    try {
        if (@($multi.GetValueNames()) -notcontains 'Profile') { Stop-Profile "HKCU\$MultiKey has no value Profile. The game has no current profile on this PC" 3 }
        if ($multi.GetValueKind('Profile') -ne [Microsoft.Win32.RegistryValueKind]::Binary) { Stop-Profile "the value Profile is not REG_BINARY" 3 }
        [byte[]]$bytes = $multi.GetValue('Profile')
    } finally { $multi.Close() }
    $profileName = [Text.Encoding]::Unicode.GetString($bytes).TrimEnd([char]0)
    RegQuote $profileName 'The name of the profile' | Out-Null
    if ($profileName -eq '' -or $profileName.Contains('\')) { Stop-Profile "the value Profile has no name that can be a key ($profileName)" 3 }
    $profileData = 'hex:' + (HexList $bytes)
    $folder = ProfileFolder $profileName
    $key = $hkcu.OpenSubKey("$ProfilesKey\$folder", $false)
    if ($null -eq $key) { Stop-Profile "the registry of the PC has no key HKCU\$ProfilesKey\$folder" 3 }
    try {
        foreach ($name in $key.GetValueNames()) {
            if ($name -eq '') { continue }
            $kind = $key.GetValueKind($name)
            $raw = $key.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            switch ($kind) {
                'DWord' { $data = 'dword:{0:x8}' -f [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$raw), 0) }
                'Binary' { $data = 'hex:' + (HexList ([byte[]]$raw)) }
                'String' { $data = RegQuote ([string]$raw) "The value $name" }
                default { Stop-Profile "the value $name of the profile has the type $kind. The script takes REG_DWORD, REG_BINARY and REG_SZ" 3 }
            }
            $q = RegQuote $name 'The name of a value'
            $values.Add(@{ Name = $q.Substring(1, $q.Length - 2); Data = $data })
            if ($name -eq 'file' -and $kind -eq 'String') { $creatureFile = [string]$raw }
        }
    } finally { $key.Close() }
    "The values of the profile $profileName come from the registry of the PC (read only): HKCU\$ProfilesKey\$folder, $($values.Count) values."
}

# The text between the quotes of RegQuote, with its escapes.
foreach ($v in $values) {
    if ($v.Name -notmatch '^(?:[^"\\]|\\["\\])*$') { Stop-Profile "the name $($v.Name) has a form that the app does not take" 3 }
}

# bwvr-profile.reg: ASCII with CRLF lines and no BOM (play_rules.h,
# ParseProfileFile).
$reg = New-Object System.Text.StringBuilder
[void]$reg.Append("Windows Registry Editor Version 5.00`r`n`r`n")
[void]$reg.Append("[HKEY_CURRENT_USER\$MultiKey]`r`n")
[void]$reg.Append("`"Profile`"=$profileData`r`n`r`n")
[void]$reg.Append("[HKEY_CURRENT_USER\$ProfilesKey\$folder]`r`n")
foreach ($v in $values) { [void]$reg.Append("`"$($v.Name)`"=$($v.Data)`r`n") }
$regText = $reg.ToString()

# THE FOLDERS of the files.
$roots = @()
if ($From.Count -gt 0) {
    foreach ($f in $From) {
        if (-not (Test-Path -LiteralPath $f)) { Stop-Profile "there is no folder $f" 3 }
        $roots += (Resolve-Path -LiteralPath $f).ProviderPath.TrimEnd('\')
    }
} else {
    $game = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($GameKey, $false)
    $gameDir = ''
    if ($null -ne $game) { try { $gameDir = [string]$game.GetValue('GameDir', '') } finally { $game.Close() } }
    if (-not $gameDir -or -not (Test-Path -LiteralPath $gameDir)) { Stop-Profile "the value GameDir of the registry names no folder ($gameDir). Give -From" 3 }
    $gameDir = [IO.Path]::GetFullPath($gameDir.Trim()).TrimEnd('\')
    foreach ($root in $env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramData, $env:windir) {
        if ($root -and $gameDir.StartsWith($root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
            $virtual = Join-Path (Join-Path $env:LOCALAPPDATA 'VirtualStore') $gameDir.Substring(3)
            if (Test-Path -LiteralPath $virtual) {
                $roots += $virtual
                "GameDir is in $root. A game with no administrator rights reads the VirtualStore first: $virtual"
            }
        }
    }
    $roots += $gameDir
}
"The folders of the files, first to last: $($roots -join ' | ')"

# Each file of Profiles\<folder> and of the Creature, from the first folder
# that has it.
$wanted = @{}
foreach ($r in $roots) {
    $dir = Join-Path (Join-Path $r 'Profiles') $folder
    if (Test-Path -LiteralPath $dir) {
        foreach ($f in Get-ChildItem -LiteralPath $dir -Recurse -File -Force) {
            $rel = $f.FullName.Substring($r.Length + 1)
            if (-not $wanted.ContainsKey($rel.ToLowerInvariant())) { $wanted[$rel.ToLowerInvariant()] = [pscustomobject]@{ Rel = $rel; Path = $f.FullName; Root = $r; Size = $f.Length; Time = $f.LastWriteTime } }
        }
    }
    if ($creatureFile) {
        foreach ($name in @($creatureFile, "Physique$creatureFile")) {
            $p = Join-Path (Join-Path $r 'Scripts\CreatureMind') $name
            $rel = "Scripts\CreatureMind\$name"
            if ((Test-Path -LiteralPath $p) -and -not $wanted.ContainsKey($rel.ToLowerInvariant())) {
                $f = Get-Item -LiteralPath $p -Force
                $wanted[$rel.ToLowerInvariant()] = [pscustomobject]@{ Rel = $rel; Path = $f.FullName; Root = $r; Size = $f.Length; Time = $f.LastWriteTime }
            }
        }
    }
}
$files = @($wanted.Values | Sort-Object { $_.Rel })
if (@($files | Where-Object { $_.Rel -like 'Profiles\*' }).Count -eq 0) { Stop-Profile "no folder of the list has Profiles\$folder" 3 }
if ($creatureFile -and @($files | Where-Object { $_.Rel -eq "Scripts\CreatureMind\$creatureFile" }).Count -eq 0) {
    "NOTE: no folder of the list has the Creature Scripts\CreatureMind\$creatureFile of the profile"
}
$total = ($files | Measure-Object -Property Size -Sum).Sum
"The profile $profileName ($folder): $($values.Count) values, $($files.Count) files, {0:N1} MB" -f ($total / 1MB)
# The saved games: the files of each slot, so the player sees which slot
# has a whole save (a .sav, Script_<n>.sav and SaveGame.inf).
$slots = @($files | Where-Object { $_.Rel -match '^Profiles\\[^\\]+\\Saved Games\\(\d+(\\\d+)?)\\' } |
    Group-Object { [regex]::Match($_.Rel, '^Profiles\\[^\\]+\\Saved Games\\(\d+(\\\d+)?)\\').Groups[1].Value })
foreach ($s in ($slots | Sort-Object { [int]($_.Name.Split('\')[0]) }, Name)) {
    $names = @($s.Group | ForEach-Object { Split-Path -Leaf $_.Rel })
    "  slot $($s.Name): " + ($names -join ', ')
}
$list = @($files | ForEach-Object { "{0}`t{1}`t{2:yyyy-MM-dd HH:mm:ss}`t{3}" -f $_.Rel, $_.Size, $_.Time, $_.Root })

if ($DryRun) {
    New-Item -ItemType Directory -Force -Path $Out | Out-Null
    [IO.File]::WriteAllText((Join-Path $Out 'bwvr-profile.reg'), $regText, [Text.Encoding]::ASCII)
    Set-Content -LiteralPath (Join-Path $Out 'files.txt') -Encoding utf8 -Value (@("path`tbytes`ttime`tfolder") + $list)
    "DRY RUN: bwvr-profile.reg and files.txt are in $Out. Nothing went to a device"
    exit 0
}

# THE COPY.
. (Join-Path $sp 'android_env.ps1')
$ErrorActionPreference = 'Continue'
$dev = @('-s', $Serial)
$dest = '/sdcard/Android/data/org.bwvr.quest/files/game'
$state = (& $Adb @dev get-state 2>&1 | Out-String).Trim()
if ($state -ne 'device') { Stop-Profile "adb has no device $Serial ($state)" 4 }
$there = (& $Adb @dev shell "ls $dest/runblack.exe" 2>&1 | Out-String).Trim()
if ($there -match 'No such file' -or $there -notmatch 'runblack\.exe') {
    Stop-Profile "the game folder of the app on $Serial has no runblack.exe. Copy the game first (push_game.ps1) ($there)" 4
}
# A name in single quotes of the shell: each ' becomes '\''.
function Sh([string]$Path) { return "'" + $Path.Replace("'", "'\''") + "'" }
$have = (& $Adb @dev shell "ls -d $(Sh "$dest/Profiles/$folder")" 2>&1 | Out-String).Trim()
if ($have -notmatch 'No such file') {
    if (-not $Replace) { Stop-Profile "the game folder of the app has the profile folder Profiles/$folder. The game in the headset can have newer saves there. Give -Replace to copy over it" 5 }
    "-Replace: the copy goes over Profiles/$folder of the app. Files of the app that the PC does not have stay"
}
# The app imports nothing while the copy is not whole: the values file goes
# last.
& $Adb @dev shell "rm -f $(Sh "$dest/bwvr-profile.reg")" 2>&1 | Out-Null
$t0 = Get-Date
$dirs = @($files | ForEach-Object { Split-Path -Parent $_.Rel } | Sort-Object -Unique)
$batch = @()
foreach ($d in $dirs) {
    $batch += "mkdir -p $(Sh "$dest/$($d.Replace('\', '/'))")"
    if ($batch.Count -ge 40) { & $Adb @dev shell ($batch -join '; ') | Out-Null; $batch = @() }
}
if ($batch.Count) { & $Adb @dev shell ($batch -join '; ') | Out-Null }
$bad = 0
foreach ($f in $files) {
    $rel = $f.Rel.Replace('\', '/')
    $o = & $Adb @dev push $f.Path "$dest/$rel" 2>&1 | Out-String
    if ($o -notmatch '1 file pushed') { $bad++; "FAULT $rel : $($o.Trim())" }
}
if ($bad -gt 0) { Stop-Profile "$bad files did not go to the device. The app has no bwvr-profile.reg, so it imports nothing" 6 }
# The same as emu_game_test.ps1 does for a test save: each one may read and
# write. A device that does not let the shell do it keeps its own rights.
$mode = (& $Adb @dev shell "chmod -R a+rwX $(Sh "$dest/Profiles/$folder") 2>&1; echo chmod `$?" 2>&1 | Out-String).Trim()
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("bwvr-profile-" + [Guid]::NewGuid().ToString('N') + '.reg')
try {
    [IO.File]::WriteAllText($tmp, $regText, [Text.Encoding]::ASCII)
    $o = & $Adb @dev push $tmp "$dest/bwvr-profile.reg" 2>&1 | Out-String
} finally { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
if ($o -notmatch '1 file pushed') { Stop-Profile "bwvr-profile.reg did not go to the device: $($o.Trim())" 6 }
"{0:N0} s. chmod of the profile folder: $mode" -f ((Get-Date) - $t0).TotalSeconds
"COPIED. The profile $profileName ($folder, $($files.Count) files and bwvr-profile.reg) is in the game folder of the app on $Serial. Play imports its values"
