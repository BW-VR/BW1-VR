<#
.SYNOPSIS
    Plays the game from the menu of the app in the Android emulator: "Play,
    new game", then "Play in VR", each time in Wine with the mod as its
    DirectDraw (docs/android.md, "Play in the app", and "Test the APK").

.DESCRIPTION
    The test needs the game. It copies the game folder of
    local/game-path.txt into the game folder of the app in the emulator,
    with no Profiles folder, no Creature of a profile (Scripts\CreatureMind\
    *.erc), no MemoryLeaks.txt and no bwvr.ini or log of the PC. The game
    folder on the PC is only read. The copy never goes into the repo or an
    APK, and the end of the test deletes the app and its folder in the
    emulator.

    The emulator is the AVD bwvr_xr_test of emu_xr_test.ps1 (port 5586) with
    the OpenXR test runtime, -gpu host (the renderer of the app needs Vulkan
    1.3) and -memory (4096 MB by default). The head stands still. The script
    of the runtime:

      1. waits (hold) until the test saw "Wine is ready" in the log of the
         app, then points at "Play, new game" and pulls the trigger.
      2. waits until the game gave frames for -PlaySeconds, while the test
         measures the CPU time of the processes of Wine. Then the runtime
         reads back the eye pictures of the projection layer and the quads.
      3. holds the menu button of the left controller for 90 frames (1.25
         s, the menu shows over the game after play_menu_hold_ms=1000),
         presses it for 6 frames (the menu goes), holds it again, points
         at "Quit game" and pulls the trigger. It waits until the log says
         that the game ended. The mod must get no press of the menu button.
      4. the same with "Play in VR" (no word on the command line: the game
         loads slot 20 of the profile, or starts a new game), with a short
         press of 6 frames first, which goes to the mod at its release, and
         no hide step.
      5. ends the session.

    The checks: the files of the mod in the prefix, the registry, the keys
    of the run in bwvr.ini at the start of the game, the log of the mod (the
    build of the APK and not the ddraw.dll of the game folder, vk_device=0,
    no Vulkan device, the input channel, the ring, the pace of the frames
    with vk_pace=1), the frames that the app
    took from the ring, the projection layer, the eye pictures, the time
    from Play to the first frame, the frame rate of the game, the CPU load
    of the processes of Wine, the menu over the game, Quit game, no process
    of Wine after each game, and no crash. The sound of each game: the audio
    driver aaudio in the registry, an output stream of wineaaudio in
    wine.log and no line "No driver ... could be initialized" of mmdevapi,
    and in the log of the mod the start of QMixer and its stream (no line
    "QMixer gave no block"). The test also collects the facts of the sound
    (the lines of wineaaudio, and the lines "audio out" and "audio mixer" of
    the mod), of the null display driver and of the noexec storage (the
    lines of wine.log with wine_debug=warn+virtual).

    The results go to bwvr/out/android/emu-game-test. They hold pictures of
    the game, so they stay on the PC.

.PARAMETER Game
    The game folder. The folder of local/game-path.txt by default.

.PARAMETER PlaySeconds
    The time of the measure of each game, after its first frame. 60 by
    default.

.PARAMETER StartMinutes
    The time that a game may take to its first frame. 15 by default.

.PARAMETER Memory
    The memory of the emulator in MB. 4096 by default.

.PARAMETER WineDebug
    The value of wine_debug in bwvr.ini. warn+virtual by default, for the
    lines of the noexec storage.

.PARAMETER SceneEyes
    After the measure of each game the runtime reads back the eye pictures of
    the projection layer (the word eyes), and the test checks the projection
    layer. Only for a game that comes to a frame of the scene: with no
    projection layer the read back is a fault of the runtime. With no
    -SceneEyes the runtime reads back the quads (the word quads), and the
    test checks the screen quad of the frames with no scene (docs/android.md,
    "The screens with no scene"): the log of the app, the quad that the
    runtime reports, its picture, and the line of the mod that names the
    dialog box.

.PARAMETER Audio
    The audio backend of the emulator (its option -audio). 'none' (the
    default): the emulator gives Android an output device and plays its
    sound nowhere, so no person hears the game. 'off': the option -no-audio,
    the emulator gives Android no audio device.

.PARAMETER WineAudio
    The value of wine_audio in bwvr.ini: 1 (the default, the audio driver
    aaudio) or 0 (no audio driver, no sound). With 0
    the test checks the log line of reg import, no Audio value in user.reg
    of the prefix after Play, no stream of wineaaudio and the line "No
    driver ... could be initialized" of mmdevapi.

.PARAMETER NewProfile
    Game 1 answers the dialog of a new profile (NewProfileBox) with the
    keyboard of the app (docs/android.md, "The keyboard of the app"): when
    the log of the mod names the dialog and says that it takes text, the
    runtime points at the keys of -ProfileName on the keyboard under the
    screen quad and pulls the trigger on each, then on Enter. The game makes
    that profile in the game folder of the app (a profile of the emulator,
    never a profile of the player). The test checks the keyboard, the keys
    in the logs of the app and of the mod, the dialog after Enter, the folder
    of the profile and the value Profile of the registry of the prefix after
    the game.

.PARAMETER ProfileOk
    With -NewProfile: x,y of the button OK of the dialog of a new profile on
    the render. After the name the runtime points at that pixel of the
    screen quad and pulls the trigger, in place of Enter: a control of the
    box takes the click of the ray. The test checks the press on the quad
    and that the dialog goes after it.

.PARAMETER SkipClick
    With -NewProfile: x,y of a row of the start dialog of a new game
    (SkipBox) on the render. When the log of the mod names that dialog over
    the Lionhead logo, the runtime points at that pixel of the screen quad
    and pulls the trigger. The click is a button of the mouse of the game,
    so the logo ends, and the frames of the scene come with the dialog over
    them (controllers.md, section 12.1).

.PARAMETER PanelClicks
    With -SkipClick: x,y pairs of pixels of the render, for example a row
    of the start dialog and then its OK. After the click on the screen quad
    the start dialog stays over the frames of the scene, on the menu panel
    quad of the mod (docs/controllers.md, section 13). When the log of the
    mod has the line of the menu panel quad, the runtime points at each
    pixel on that quad (the panel image has the size of the render) and
    pulls the trigger. The test checks each press of the mod on the panel at
    its pixel, the left button of the game, and that the start dialog goes
    after the last press.

.PARAMETER MenuClick
    x,y of a button of the Main Menu of the game over a saved game (for
    example "Continue Game") on the render. In game 2 ("Play in VR", the
    test save of -TestSave) the Main Menu is over the land, on the menu
    panel quad. When the log of the mod has the line of the menu panel quad,
    the runtime points at that pixel on the quad and pulls the trigger. The
    test checks the press on the panel and that the Main Menu goes after it.

.PARAMETER SaveName
    With -MenuClick: after the Main Menu of game 2 the ring of the left hand
    goes into the Temple and to its Save Game Room. The runtime clicks a
    picture of a slot two times, the box "Save Game Message" takes text in a
    frame of the scene, and the keyboard of the app at the menu panel quad
    types this name and Enter. The test checks the text flag of the mod, the
    keyboard of the app, the keys as WM_CHAR, and the new slot of the test
    profile in the emulator. The game has no Save in its Esc menu: it saves
    in the Temple.

.PARAMETER SavePicture
    With -SaveName: x,y of a picture of a slot in the view of the Save Game
    Room at its rest pose (710,445 by default, for the head of the runtime
    and a render of 1380 by 1104).

.PARAMETER SaveOk
    With -SaveName: x,y of OK of the box "Save Game Message" on the render
    (591,627 by default). The runtime clicks it on the menu panel quad after
    the name and Enter: Enter does not close the box.

.PARAMETER TestSave
    With -NewProfile: the name of a test save of local\test-saves of the
    main checkout (a saved game that a test run made). After game 1 the test
    puts its files into slot 20 of the profile of game 1 in the emulator, so
    game 2 ("Play in VR") continues it.

.PARAMETER Keep
    Leaves the emulator on at the end, with the app and the copy of the
    game in it.
#>
[CmdletBinding()]
param(
    [string]$Game,
    [int]$PlaySeconds = 60,
    [int]$StartMinutes = 15,
    [int]$Memory = 4096,
    [string]$WineDebug = 'warn+virtual',
    [switch]$SceneEyes,
    [string]$Audio = 'none',
    [ValidateSet(0, 1)][int]$WineAudio = 1,
    [switch]$NewProfile,
    [string]$ProfileName = 'bwvrtest',
    [int[]]$SkipClick = @(),
    [int[]]$ProfileOk = @(),
    [int[]]$PanelClicks = @(),
    [int[]]$MenuClick = @(),
    [string]$SaveName = '',
    [int[]]$SavePicture = @(710, 445),
    [int[]]$SaveOk = @(591, 627),
    [string]$TestSave = '',
    [switch]$Keep
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
Lock-AndroidEmulator
# The tools write notes to stderr. Each check here reads their output, so a
# line on stderr must not stop the script.
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Repo = Split-Path $Bwvr -Parent
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$Out = Join-Path $Bwvr 'out\android\emu-game-test'
$Avd = 'bwvr_xr_test'
$Image = 'system-images;android-34;google_apis;x86_64'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"
$GameDir = "$AppDir/game"
$XrDir = '/data/local/tmp/bwvr-xr'
$Port = 5586
$Serial = "emulator-$Port"

if (-not $Game) {
    $pathFile = Join-Path $Repo 'local\game-path.txt'
    if (-not (Test-Path -LiteralPath $pathFile)) { Stop-Android "There is no $pathFile. Give -Game." }
    $Game = (Get-Content -LiteralPath $pathFile -Raw).Trim()
}
if (-not (Test-Path -LiteralPath (Join-Path $Game 'runblack.exe'))) { Stop-Android "There is no runblack.exe in $Game." }
if (-not (Test-Path -LiteralPath $Apk)) { Stop-Android "There is no APK at:`n$Apk`nRun build_apk.ps1 first." }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# The build of the mod in the APK: its build id is a string in the DLL.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$modDll = Join-Path $Out 'apk-ddraw.dll'
$zip = [System.IO.Compression.ZipFile]::OpenRead($Apk)
try {
    $entry = $zip.Entries | Where-Object { $_.FullName -eq 'assets/mod/ddraw.dll' } | Select-Object -First 1
    if (-not $entry) { Stop-Android 'The APK has no assets/mod/ddraw.dll. Build it with a Wine tree.' }
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $modDll, $true)
} finally { $zip.Dispose() }
$modText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($modDll))
$gameDdraw = Join-Path $Game 'ddraw.dll'
$gameDdrawText = if (Test-Path -LiteralPath $gameDdraw) { [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($gameDdraw)) } else { '' }
Remove-Item -LiteralPath $modDll -Force

$faults = New-Object System.Collections.Generic.List[string]
$checks = 0
function Expect([bool]$Ok, [string]$What) {
    $script:checks++
    if ($Ok) { Write-Host ('   ok    ' + $What) }
    else { Write-Host ('   FAULT ' + $What) -ForegroundColor Red; $script:faults.Add($What) }
}
function Say([string]$Text) { Write-Host ('== ' + $Text) -ForegroundColor Cyan }
function Note([string]$Text) { Write-Host ('         ' + $Text) }
function AdbOut([string[]]$Arguments) { return (& $Adb -s $Serial @Arguments 2>&1 | Out-String) }
function Adb([string[]]$Arguments) { & $Adb -s $Serial @Arguments 2>&1 | Out-Null }
function Wait-Boot([int]$Minutes) {
    $deadline = (Get-Date).AddMinutes($Minutes)
    do {
        Start-Sleep -Seconds 3
        $booted = (& $Adb -s $Serial shell getprop sys.boot_completed 2>$null | Out-String).Trim()
    } while ($booted -ne '1' -and (Get-Date) -lt $deadline)
    if ($booted -ne '1') { return $false }
    do {
        Start-Sleep -Seconds 2
        $pm = (& $Adb -s $Serial shell pm path android 2>$null | Out-String)
    } while ($pm -notmatch 'package:' -and (Get-Date) -lt $deadline)
    return $true
}
function AppLog { return (AdbOut @('shell', 'cat', "$AppDir/bwvr-quest.log")) }
# Waits until the log of the app matches one of the patterns, or the app
# ends, or the time is over. After: only the log from this index counts.
# Gives the log.
function WaitLog([string[]]$Patterns, [int]$Seconds, [int]$After = 0) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        Start-Sleep -Seconds 3
        $log = AppLog
        $tail = if ($log.Length -gt $After) { $log.Substring($After) } else { '' }
        foreach ($p in $Patterns) { if ($tail -match $p) { return $log } }
        $alive = (AdbOut @('shell', 'pidof', $Package)).Trim()
    } while ((Get-Date) -lt $deadline -and $alive -match '^\d+$')
    return $log
}
function Hold([string]$Name) { Adb @('shell', 'touch', "$XrDir/$Name"); Adb @('shell', 'chmod', '666', "$XrDir/$Name") }
# The processes of the user of the app, with no process of the app itself.
function WineProcesses([string]$User) {
    $ps = AdbOut @('shell', 'ps', '-A', '-o', 'USER,PID,NAME')
    return @($ps -split "`r?`n" | Where-Object { $_ -match "^$User\s" -and $_ -notmatch [regex]::Escape($Package) })
}
# The CPU time of each process of the user (utime and stime of /proc/<pid>/
# stat, in ticks of 1/100 s) and the uptime. As root.
function CpuSample([string]$User) {
    $text = AdbOut @('shell', 'sh', "$XrDir/cpu.sh", $User)
    $procs = @{}
    $uptime = 0.0
    foreach ($line in ($text -split "`r?`n")) {
        $m = [regex]::Match($line, '^(\d+) \((.*)\) \S+ (.*)$')
        if ($m.Success) {
            $f = $m.Groups[3].Value -split ' '
            $procs[$m.Groups[1].Value] = @{ Name = $m.Groups[2].Value; Ticks = [int64]$f[10] + [int64]$f[11] }
            continue
        }
        $u = [regex]::Match($line, '^([\d.]+) [\d.]+$')
        if ($u.Success) { $uptime = [double]::Parse($u.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) }
    }
    return @{ Procs = $procs; Uptime = $uptime }
}
function CpuLoad($A, $B) {
    $seconds = $B.Uptime - $A.Uptime
    $lines = @()
    $wine = 0.0
    $app = 0.0
    $rows = @()
    foreach ($id in $B.Procs.Keys) {
        $p = $B.Procs[$id]
        $before = if ($A.Procs.ContainsKey($id)) { $A.Procs[$id].Ticks } else { 0 }
        $pct = if ($seconds -gt 0) { ($p.Ticks - $before) / $seconds } else { 0 }
        # The process of the app has the name of the package (its end).
        if ($Package.EndsWith($p.Name)) { $app += $pct } else { $wine += $pct }
        $rows += , @($pct, ('{0} {1}: {2} %' -f $id, $p.Name, [math]::Round($pct, 1)))
    }
    $lines = @($rows | Sort-Object { $_[0] } -Descending | ForEach-Object { $_[1] })
    return @{ Seconds = $seconds; Wine = $wine; App = $app; Lines = $lines }
}

function ModLog { return (AdbOut @('shell', 'cat', "$GameDir/bwvr.log")) }
# Waits until the log of the mod has each pattern, or the time is over.
# Gives the log.
function WaitModLog([string[]]$Patterns, [int]$Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $log = ModLog
        $all = $true
        foreach ($p in $Patterns) { if ($log -notmatch $p) { $all = $false } }
        if ($all) { return $log }
        Start-Sleep -Seconds 3
    } while ((Get-Date) -lt $deadline)
    return $log
}
# THE DIALOG OF A NEW PROFILE by the keyboard of the app (-NewProfile).
function ProfileDialog([int]$PlayAt) {
    Say "The dialog of a new profile: the keyboard of the app types $ProfileName"
    $mod = WaitModLog @('setup box    : the dialog box NewProfileBox is up now',
        'vk stream    : screen frame \d+: a dialog box takes text') 900
    Expect ($mod -match 'setup box    : the dialog box NewProfileBox is up now') 'the mod names the dialog of a new profile'
    Expect ($mod -match 'vk stream    : screen frame \d+: a dialog box takes text, so the kScreen asks the app for its keyboard') 'the kScreen of the mod asks the app for its keyboard'
    Expect ($mod -match 'controllers  : screen_pointer=1\.' -and $mod -match 'controllers  : screen_keys=1\.') 'the mod has screen_pointer=1 and screen_keys=1'
    $log = WaitLog @('keyboard     : a dialog box of the game takes text\. The keyboard shows') 60 $PlayAt
    $kb = [regex]::Match($log, 'keyboard     : a dialog box of the game takes text\. The keyboard shows under the screen quad, [^\r\n]*')
    Expect $kb.Success ('the app shows its keyboard: ' + $kb.Value)
    Hold 'profile'
    $ok = $ProfileOk.Count -eq 2
    $keys = $ProfileName.Length + $(if ($ok) { 0 } else { 1 })
    # The end of the dialog: Enter of the keyboard, or the press of the ray
    # on OK.
    $endLine = if ($ok) { 'controllers  : screen quad: the right trigger presses at the pixel' } else { 'controllers  : the key Enter of the app goes to the game' }
    $mod = WaitModLog @([regex]::Escape($endLine)) 300
    Start-Sleep -Seconds 10
    $mod = ModLog
    $log = AppLog
    $appKeys = @([regex]::Matches($log.Substring([math]::Min($PlayAt, $log.Length)), 'keyboard     : the key (\S+) goes to the mod') | ForEach-Object { $_.Groups[1].Value })
    Note ('the keys of the app: ' + ($appKeys -join ' '))
    $wantKeys = @($ProfileName.ToCharArray() | ForEach-Object { "$_" }) + $(if ($ok) { @() } else { @('Enter') })
    Expect ($appKeys.Count -eq $keys -and ($appKeys -join ' ') -eq ($wantKeys -join ' ')) "the keyboard of the app gave $($wantKeys -join ' ') to the mod ($($appKeys.Count) keys)"
    $modKeys = @($mod -split "`r?`n" | Where-Object { $_ -match '^controllers  : the key .* of the app goes to the game \(WM_CHAR\)' })
    $modKeys | Select-Object -First 3 | ForEach-Object { Note ('bwvr.log: ' + $_) }
    Expect ($modKeys.Count -eq $keys) "the mod gave each key to the game as WM_CHAR ($($modKeys.Count) of $keys)"
    $boxLines = @($mod -split "`r?`n" | Where-Object { $_ -match '^(setup box    :|controllers  : (screen quad|the left button)|vk stream    : screen frame \d+: (a dialog box takes|no dialog box takes))' })
    $boxLines | ForEach-Object { Note ('bwvr.log: ' + $_) }
    $endAt = $mod.IndexOf($endLine)
    if ($ok) {
        $press = [regex]::Match($mod, 'controllers  : screen quad: the right trigger presses at the pixel [^\r\n]*')
        Expect $press.Success ('the ray pressed on OK of the dialog on the screen quad: ' + $press.Value)
        $down = $mod.IndexOf('controllers  : the left button is down', [math]::Max(0, $endAt))
        Expect ($endAt -ge 0 -and $down -gt $endAt) 'the trigger on OK went to the game as the left button'
    }
    $afterAt = if ($endAt -ge 0) { $mod.IndexOf('setup box    : no dialog box is up now', $endAt) } else { -1 }
    $after = if ($afterAt -ge 0) { [regex]::Match($mod.Substring($afterAt), 'setup box    : no dialog box is up now[^\r\n]*').Value } else { '' }
    Expect ($afterAt -gt $endAt) ("the dialog of a new profile went after " + $(if ($ok) { 'the click of the ray on OK' } else { 'Enter' }) + ": $after")
    Hold 'typed'
    $profiles = (AdbOut @('shell', 'ls', "$GameDir/Profiles")).Trim()
    Note "the folder Profiles of the game folder of the app: $profiles"
    $want = (($ProfileName.ToCharArray() | ForEach-Object { "_$_" }) -join '')
    Expect ($profiles -match [regex]::Escape($want)) "the game made the profile $ProfileName in the game folder of the app (the folder $want)"
    if ($SkipClick.Count -eq 2) {
        # The start dialog of a new game over the Lionhead logo, by the ray.
        $mod = WaitModLog @('setup box    : the dialog box SkipBox is up now') 300
        Expect ($mod -match 'setup box    : the dialog box SkipBox is up now') 'the mod names the start dialog of a new game (SkipBox)'
        Hold 'skipbox'
        $log = WaitLog @('game         : the frames of the scene start') 180 $PlayAt
        $mod = ModLog
        # The press after the line that names SkipBox (the press on OK of the
        # dialog of a new profile comes before it).
        $skipAt = [math]::Max(0, $mod.IndexOf('setup box    : the dialog box SkipBox is up now'))
        $press = [regex]::Match($mod.Substring($skipAt), 'controllers  : screen quad: the right trigger presses at the pixel [^\r\n]*')
        Expect $press.Success ('the ray pressed on the start dialog on the screen quad: ' + $press.Value)
        Expect ($mod.IndexOf('controllers  : the left button is down', $skipAt) -gt $skipAt) 'the trigger went to the game as the left button'
        $scene = [regex]::Match($log.Substring([math]::Min($PlayAt, $log.Length)), 'game         : the frames of the scene start at frame \d+, ([\d.]+) s after Play')
        Expect $scene.Success ('the click ended the Lionhead logo, and the frames of the scene came: ' + $scene.Value)
        @($mod -split "`r?`n" | Where-Object { $_ -match '^(setup box    :|controllers  : (screen quad|the left button|a dialog box))' }) | Select-Object -Last 8 | ForEach-Object { Note ('bwvr.log: ' + $_) }
        Hold 'clicked'
        if ($PanelClicks.Count -ge 2) { PanelClicks 'SkipBox' $PanelClicks 'panel1' 'start dialog' }
    }
}

# A place for the word pointfile of the runtime, during the run.
function PushPlace([string]$Name, [string]$Text) {
    $local = Join-Path $Out $Name
    Set-Content -LiteralPath $local -Value $Text -Encoding ascii
    Adb @('push', $local, "$XrDir/$Name")
    Adb @('shell', 'chmod', '666', "$XrDir/$Name")
    Note "the place $Name for the runtime: $Text"
}

# A BOX THAT TAKES TEXT IN A FRAME OF THE SCENE (-SaveName, game 2): the box
# "Save Game Message" of the Save Game Room of the Temple, with the keyboard
# of the app at the menu panel quad (docs/controllers.md, 13.3). The game has
# no Save in its Esc menu (the Esc menu over a game is the Main Menu). It
# saves in the Temple: a second click on a picture of a slot with no saved
# game opens that box. The ring of the left hand goes into the Temple and to
# the room. -SavePicture is the pixel of a picture of the view of that room
# at its rest pose, and the runtime aims along the ray of the eye through it. The first click turns the camera to the
# picture, and the second click, in the middle of the view, opens the box.
# The keys of the name and Enter come from the keyboard of the app.
function SaveFlow {
    Say "The box of a save in a frame of the scene: the keyboard of the app types $SaveName"
    $holds = @('ring2', 'temple2', 'saveroom', 'aim2', 'savebox', 'saved2')
    try {
        Start-Sleep -Seconds 8
        Hold 'ring2'
        # The first visit of the hall plays a guide, and the ring does not open
        # while it plays: the ray of the hall after the last line of a guide.
        # A guide starts some frames after the player comes in, so the test
        # looks 10 s after that at the earliest.
        $deadline = (Get-Date).AddSeconds(400)
        $inSince = $null
        do {
            Start-Sleep -Seconds 3
            $mod = ModLog
            if ($null -eq $inSince -and $mod -match 'ring         : the game: the player is in the Temple') { $inSince = Get-Date }
            $guideAt = $mod.LastIndexOf('temple       : in the hall a guide of the game plays')
            $rayAt = $mod.LastIndexOf('temple       : in the hall the pointing hand points with a ray')
        } while (-not ($null -ne $inSince -and ((Get-Date) - $inSince).TotalSeconds -ge 10 -and $rayAt -gt $guideAt) -and (Get-Date) -lt $deadline)
        Start-Sleep -Seconds 3
        $in = [regex]::Match($mod, 'ring         : the game: the player is in the Temple[^\r\n]*')
        Expect $in.Success ('the ring of the left hand took the player into the Temple: ' + $in.Value)
        Hold 'temple2'
        $mod = WaitModLog @('ring         : the game: the player is in the Temple, in the Save Game Room') 400
        $room = [regex]::Match($mod, 'ring         : the game: the player is in the Temple, in the Save Game Room[^\r\n]*')
        Expect $room.Success ('the ring took the player to the Save Game Room: ' + $room.Value)
        # The ray of the room after its guide.
        $deadline = (Get-Date).AddSeconds(400)
        do {
            Start-Sleep -Seconds 3
            $mod = ModLog
            $guideAt = $mod.LastIndexOf('temple       : in the Save Game Room a guide of the game plays')
            $rayAt = $mod.LastIndexOf('temple       : in the Save Game Room the pointing hand points with a ray')
        } while (-not ($guideAt -ge 0 -and $rayAt -gt $guideAt) -and (Get-Date) -lt $deadline)
        Start-Sleep -Seconds 3
        PushPlace 'aim1.txt' "view $($SavePicture[0]) $($SavePicture[1])"
        Hold 'saveroom'
        Start-Sleep -Seconds 12
        PushPlace 'aim2.txt' 'view 690 552'
        Hold 'aim2'
        $text = 'xr layers    : a dialog box on the menu panel quad takes text, so the kPanel asks the app for its keyboard'
        $mod = WaitModLog @([regex]::Escape($text)) 120
        $boxes = @([regex]::Matches($mod, 'setup box    : the dialog box (\S+) is up now') | ForEach-Object { $_.Groups[1].Value })
        Note ('the dialog boxes of game 2: ' + ($boxes -join ' '))
        Expect ($mod -match [regex]::Escape($text)) 'the mod has the line that a dialog box of a frame of the scene takes text (kPanelQuadText)'
        $log = WaitLog @('keyboard     : a dialog box of the game takes text\. The keyboard shows at the menu panel quad') 60
        $kb = [regex]::Match($log, 'keyboard     : a dialog box of the game takes text\. The keyboard shows at the menu panel quad[^\r\n]*')
        Expect $kb.Success ('the app shows its keyboard at the menu panel quad: ' + $kb.Value)
        $appAt = $log.Length
        Hold 'savebox'
        $mod = WaitModLog @('controllers  : the key Enter of the app') 300
        Start-Sleep -Seconds 15
        $mod = ModLog
        $log = AppLog
        $textAt = $mod.IndexOf($text)
        $after = if ($textAt -ge 0) { $mod.Substring($textAt) } else { '' }
        $modKeys = @([regex]::Matches($after, 'controllers  : the key (\S+) of the app goes to the game \(WM_CHAR\)') | ForEach-Object { $_.Groups[1].Value.Trim('"') })
        $want = @($SaveName.ToCharArray() | ForEach-Object { "$_" }) + @('Enter')
        Note ('the keys of the app to the game: ' + ($modKeys -join ' '))
        Expect (($modKeys -join ' ') -eq ($want -join ' ')) "the mod gave $($want -join ' ') to the game as WM_CHAR ($($modKeys.Count) keys)"
        # The box: the callback of its controls is DialogBoxSaveMessage::ControlCallback.
        $box = [regex]::Match($mod, 'setup box    : a dialog box is up now: [^\r\n]*the callback of its controls 00515CB0[^\r\n]*')
        Expect $box.Success ('the box "Save Game Message" (DialogBoxSaveMessage) came up in a frame of the scene: ' + $box.Value)
        # OK by the ray after Enter: one press on the panel, at OK.
        $pressPattern = 'controllers  : panel quads: the right hand presses the left button at the pixel (\d+) (\d+)'
        $deadline = (Get-Date).AddSeconds(120)
        while (-not ($after -match $pressPattern) -and (Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 3
            $mod = ModLog
            $after = if ($textAt -ge 0) { $mod.Substring($textAt) } else { '' }
        }
        Start-Sleep -Seconds 15
        $mod = ModLog
        $after = if ($textAt -ge 0) { $mod.Substring($textAt) } else { '' }
        $enterAt = $after.IndexOf('controllers  : the key Enter of the app')
        $presses = @([regex]::Matches($after, $pressPattern))
        $beforeEnter = @($presses | Where-Object { $_.Index -lt $enterAt }).Count
        Expect ($enterAt -ge 0 -and $beforeEnter -eq 0) "no pull of the trigger on the keyboard was a press on the panel ($beforeEnter)"
        $ok = @($presses | Where-Object { $_.Index -gt $enterAt })
        $okPress = if ($ok.Count -gt 0) { $ok[0] } else { $null }
        $okX = if ($okPress) { [int]$okPress.Groups[1].Value } else { -1 }
        $okY = if ($okPress) { [int]$okPress.Groups[2].Value } else { -1 }
        Expect ($okPress -and [math]::Abs($okX - $SaveOk[0]) -le 4 -and [math]::Abs($okY - $SaveOk[1]) -le 4) "the ray pressed OK of the box on the panel at the pixel $okX $okY (want $($SaveOk[0]) $($SaveOk[1])), not under the keyboard"
        $goneAt = if ($okPress) { $after.IndexOf('setup box    : no dialog box is up now', $okPress.Index) } else { -1 }
        $gone = if ($goneAt -ge 0) { [regex]::Match($after.Substring($goneAt), '[^\r\n]*').Value } else { '' }
        Expect ($goneAt -gt 0) "the box went after the press on OK: $gone"        @($after -split "`r?`n" | Where-Object { $_ -match '^(setup box    :|controllers  : (panel quads|the key|the left button)|xr layers    :|temple       : the Save Game Room)' }) | Select-Object -First 30 | ForEach-Object { Note ('bwvr.log: ' + $_) }
        $want = (($ProfileName.ToCharArray() | ForEach-Object { "_$_" }) -join '')
        # The slots of the test profile: a new slot of 0 to 13 with a saved
        # game is the save of the box (slot 20 holds the test save).
        $games = "$GameDir/Profiles/$want/Saved Games"
        $listing = (AdbOut @('shell', "ls -lR '$games'")).Trim()
        Set-Content -LiteralPath (Join-Path $Out 'saved-games.txt') -Value $listing -Encoding utf8
        $slots = @((AdbOut @('shell', "ls '$games'")).Trim() -split '\s+' | Where-Object { $_ -match '^\d+$' -and [int]$_ -le 13 })
        $saved = @($slots | Where-Object { (AdbOut @('shell', "ls '$games/$_'")) -match '\.sav' })
        Note ("the slots 0 to 13 of the test profile in the emulator with a saved game: " + ($saved -join ' '))
        $name = if ($saved.Count -gt 0) { (AdbOut @('shell', "cat '$games/$($saved[0])/SaveGame.inf'")) -replace '[^\x20-\x7E]', '' } else { '' }
        Note "SaveGame.inf of that slot, the printable bytes: $name"
        Expect ($saved.Count -eq 1) "the save went into a slot of the test profile $ProfileName in the emulator ($($saved -join ' '))"
    } finally {
        foreach ($h in $holds) { Hold $h }
    }
}

# THE CLICKS OF THE RAY ON THE MENU PANEL QUAD in a frame of the scene
# (-PanelClicks, -MenuClick, docs/controllers.md, section 13). Box: the class
# of the dialog box. Points: x,y pairs of the render. Name: the hold that lets
# the runtime point, and Name + "done" after the check.
function PanelClicks([string]$Box, [int[]]$Points, [string]$Name, [string]$What) {
    $count = [int][math]::Floor($Points.Count / 2)
    Say "The $What ($Box) on the menu panel quad by the ray"
    $line = 'controllers  : panel quads: a dialog box is up in a frame of the scene'
    $mod = WaitModLog @("setup box    : the dialog box $Box is up now", [regex]::Escape($line)) 600
    $boxAt = $mod.LastIndexOf("setup box    : the dialog box $Box is up now")
    $panelAt = if ($boxAt -ge 0) { $mod.IndexOf($line, $boxAt) } else { -1 }
    $panelLine = if ($panelAt -ge 0) { [regex]::Match($mod.Substring($panelAt), '[^\r\n]*').Value } else { '' }
    Expect ($panelAt -gt $boxAt -and $boxAt -ge 0) ("the mod has the menu panel quad of $Box in a frame of the scene: " + $panelLine)
    Hold $Name
    $pattern = 'controllers  : panel quads: the right hand presses the left button at the pixel (\d+) (\d+)'
    $from = [math]::Max(0, $panelAt)
    $deadline = (Get-Date).AddSeconds(300)
    do {
        Start-Sleep -Seconds 3
        $mod = ModLog
        $presses = @([regex]::Matches($mod.Substring([math]::Min($from, $mod.Length)), $pattern))
    } while ($presses.Count -lt $count -and (Get-Date) -lt $deadline)
    Start-Sleep -Seconds 10
    $mod = ModLog
    $tail = $mod.Substring([math]::Min($from, $mod.Length))
    $presses = @([regex]::Matches($tail, $pattern))
    Expect ($presses.Count -eq $count) "the mod took $count presses of the ray on the panel ($($presses.Count))"
    for ($k = 0; $k -lt [math]::Min($count, $presses.Count); $k++) {
        $p = $presses[$k]
        $x = [int]$p.Groups[1].Value; $y = [int]$p.Groups[2].Value
        $wx = $Points[2 * $k]; $wy = $Points[2 * $k + 1]
        $down = $tail.IndexOf('controllers  : the left button is down', $p.Index)
        $next = if ($k + 1 -lt $presses.Count) { $presses[$k + 1].Index } else { $tail.Length }
        Expect ([math]::Abs($x - $wx) -le 4 -and [math]::Abs($y - $wy) -le 4 -and $down -gt $p.Index -and $down -lt $next) "press $($k + 1) on the panel at the pixel $x $y (want $wx $wy), and the left button of the game went down"
    }
    if ($presses.Count -gt 0) {
        $lastAt = $presses[$presses.Count - 1].Index
        $goneAt = $tail.IndexOf('setup box    : no dialog box is up now', $lastAt)
        $gone = if ($goneAt -ge 0) { [regex]::Match($tail.Substring($goneAt), '[^\r\n]*').Value } else { '' }
        Expect ($goneAt -gt $lastAt) "$Box went after the last press of the ray on the panel: $gone"
    }
    @($tail -split "`r?`n" | Where-Object { $_ -match '^(setup box    :|controllers  : (panel quads|the left button)|xr layers    : (a dialog|no dialog))' }) | Select-Object -First 14 | ForEach-Object { Note ('bwvr.log: ' + $_) }
    Hold ($Name + 'done')
}

# 1. The test runtime.
Say 'Build the OpenXR test runtime for x86_64'
$runtimeBuild = Join-Path $Bwvr 'build-android\test-runtime'
& $SdkCmake -Wno-deprecated -G Ninja -S (Join-Path $Bwvr 'android\test_runtime') -B $runtimeBuild `
    "-DCMAKE_MAKE_PROGRAM=$SdkNinja" "-DCMAKE_TOOLCHAIN_FILE=$AndroidNdk\build\cmake\android.toolchain.cmake" `
    '-DANDROID_ABI=x86_64' '-DANDROID_PLATFORM=android-29' '-DANDROID_STL=c++_static' `
    '-DCMAKE_BUILD_TYPE=Release' "-DBWVR_THIRD_PARTY=$Repo\third_party" | Out-Null
& $SdkCmake --build $runtimeBuild
if ($LASTEXITCODE -ne 0) { Stop-Android 'The test runtime did not build.' }
$runtimeSo = Join-Path $runtimeBuild 'libbwvr_test_runtime.so'

# 2. The emulator.
$avds = & $Emulator -list-avds 2>$null
if (-not ($avds -contains $Avd)) {
    Say "Make the AVD $Avd"
    $avdManager = Join-Path $AndroidSdk 'cmdline-tools\latest\bin\avdmanager.bat'
    'no' | & $avdManager create avd -n $Avd -k $Image -d 'pixel_tablet' --force | Out-Null
    if ($LASTEXITCODE -ne 0) { Stop-Android "avdmanager could not make the AVD $Avd." }
}
& $Adb start-server 2>&1 | Out-Null
$started = $false
$running = & $Adb devices 2>$null | Select-String -Pattern "^$Serial\s+device"
if (-not $running) {
    Say "Start the emulator (no window, a system that it can write, the GPU of the PC, $Memory MB, audio $Audio)"
    $audioArgs = if ($Audio -eq 'off') { @('-no-audio') } else { @('-audio', $Audio) }
    Start-Process -FilePath $Emulator -ArgumentList (@('-avd', $Avd, '-port', "$Port",
        '-no-window') + $audioArgs + @('-no-boot-anim', '-no-snapshot', '-writable-system',
        '-gpu', 'host', '-memory', "$Memory")) -WindowStyle Hidden | Out-Null
    $started = $true
} else {
    Say "The emulator $Serial runs already. Its audio is the one of its start, not -Audio $Audio"
}
if (-not (Wait-Boot 6)) { Stop-Android 'The emulator did not start in 6 minutes.' }

try {
    Say 'Put the test runtime into the emulator'
    Adb @('root')
    Start-Sleep -Seconds 3
    Adb @('wait-for-device')
    $remount = AdbOut @('remount')
    if ($remount -match 'reboot') {
        Write-Host '   the first remount needs a reboot'
        Adb @('reboot')
        Start-Sleep -Seconds 10
        if (-not (Wait-Boot 6)) { Stop-Android 'The emulator did not start again after the reboot.' }
        Adb @('root')
        Start-Sleep -Seconds 3
        Adb @('wait-for-device')
        $remount = AdbOut @('remount')
    }
    Expect ($remount -match 'remount succeeded|Remount succeeded') ('adb remount: ' + ($remount.Trim() -split "`n")[-1])
    Write-Host ('   ' + (AdbOut @('shell', 'getprop', 'ro.build.fingerprint')).Trim())
    $mem = [regex]::Match((AdbOut @('shell', 'cat', '/proc/meminfo')), 'MemTotal:\s+(\d+) kB')
    Note "the emulator has $([math]::Round([int64]$mem.Groups[1].Value / 1024)) MB of memory and $((AdbOut @('shell', 'nproc')).Trim()) CPUs"
    Adb @('shell', 'setenforce', '0')
    Adb @('shell', 'rm', '-rf', $XrDir)
    Adb @('shell', 'mkdir', '-p', $XrDir)
    Adb @('push', $runtimeSo, "$XrDir/libbwvr_test_runtime.so")
    $manifest = Join-Path $Out 'active_runtime.json'
    Set-Content -LiteralPath $manifest -Encoding ascii -Value @(
        '{', '  "file_format_version": "1.0.0",',
        '  "runtime": {', '    "name": "bwvr test runtime",',
        "    `"library_path`": `"$XrDir/libbwvr_test_runtime.so`"", '  }', '}')
    Adb @('shell', 'mkdir', '-p', '/system/etc/openxr/1')
    Adb @('push', $manifest, '/system/etc/openxr/1/active_runtime.json')
    # The CPU time of the processes of a user, for the CPU load of Wine.
    $cpuScript = Join-Path $Out 'cpu.sh'
    [IO.File]::WriteAllText($cpuScript, "u=`$1`nfor d in /proc/[0-9]*; do`n  [ `"`$(stat -c %U `$d 2>/dev/null)`" = `"`$u`" ] || continue`n  cat `$d/stat 2>/dev/null`n  echo`ndone`ncat /proc/uptime`n")
    Adb @('push', $cpuScript, "$XrDir/cpu.sh")

    # THE SCRIPT OF THE CONTROLLERS. The rows of the menu are 68 pixels high
    # from y = 190 (menu_layout.h): row r has its middle at 224 + 68 r. With
    # no game, row 0 is "Play in VR" and row 1 "Play, new game". While the
    # game runs, row 8 is "Quit game" (the 7 groups, Save Config, Quit game,
    # Exit).
    # -NewProfile: the steps of the dialog of a new profile come after the
    # trigger on "Play, new game", and the steps after them move by $shift.
    $profileSteps = @()
    $shift = 0
    if ($NewProfile) {
        $f = 45
        $profileSteps += "$f hold profile"; $f += 2
        $profileSteps += "$f quads profilebox"; $f += 4
        $keys = @($ProfileName.ToCharArray() | ForEach-Object { "$_" })
        if ($ProfileOk.Count -ne 2) { $keys += @('enter') }
        foreach ($k in $keys) {
            $profileSteps += "$f point right key $k"
            $profileSteps += "$($f + 8) trigger right 1"
            $profileSteps += "$($f + 12) trigger right 0"
            $f += 20
        }
        if ($ProfileOk.Count -eq 2) {
            # The name is in the field: a picture, then OK by the ray.
            $profileSteps += "$f quads named"; $f += 4
            $profileSteps += "$f point right screen $($ProfileOk[0]) $($ProfileOk[1])"
            $profileSteps += "$($f + 8) trigger right 1"
            $profileSteps += "$($f + 12) trigger right 0"
            $f += 20
        }
        $profileSteps += "$f hold typed"; $f += 2
        $profileSteps += "$f quads typed"; $f += 4
        if ($SkipClick.Count -eq 2) {
            $profileSteps += "$f hold skipbox"; $f += 2
            $profileSteps += "$f point right screen $($SkipClick[0]) $($SkipClick[1])"
            $profileSteps += "$($f + 8) trigger right 1"
            $profileSteps += "$($f + 12) trigger right 0"
            $f += 20
            $profileSteps += "$f hold clicked"; $f += 2
            $profileSteps += "$f quads clicked"; $f += 4
            if ($PanelClicks.Count -ge 2) {
                # The start dialog over the frames of the scene, on the menu
                # panel quad.
                $profileSteps += "$f hold panel1"; $f += 2
                $profileSteps += "$f quads panel1"; $f += 4
                for ($i = 0; $i + 1 -lt $PanelClicks.Count; $i += 2) {
                    $profileSteps += "$f point right panel $($PanelClicks[$i]) $($PanelClicks[$i + 1])"
                    $profileSteps += "$($f + 8) trigger right 1"
                    $profileSteps += "$($f + 12) trigger right 0"
                    $f += 30
                }
                $profileSteps += "$f hold panel1done"; $f += 2
                $profileSteps += "$f quads panel1done"; $f += 4
            }
        }
        $shift = $f - 49
    }
    # -MenuClick: the Main Menu of game 2 over the land, on the menu panel
    # quad, after the trigger on "Play in VR". The steps after it move by
    # $shift2.
    $menuSteps = @()
    $shift2 = 0
    if ($MenuClick.Count -eq 2) {
        $menuSteps = @('396 hold menu2', '398 quads menu2',
            "402 point right panel $($MenuClick[0]) $($MenuClick[1])",
            '410 trigger right 1', '414 trigger right 0',
            '420 hold menu2done', '422 quads menu2done')
        $shift2 = 26
        if ($SaveName -ne '') {
            # The Save Game Room of the Temple and the box "Save Game
            # Message" (SaveFlow): the ring of the left hand (Y, the hand 8.5
            # cm toward the item, Y up) goes into the Temple ("enter", the
            # slot up left of page 1), then to the Save Game Room
            # ("room_save", the slot down right of the ring of the Temple).
            # The test writes the places of the clicks into aim1.txt and
            # aim2.txt during the run.
            $f = 426
            $s = @("$f hold ring2", "$($f + 2) button left 1", "$($f + 12) move left -0.06 0.06 0",
                "$($f + 44) button left 0", "$($f + 54) move left 0.06 -0.06 0", "$($f + 64) hold temple2",
                "$($f + 66) button left 1", "$($f + 76) move left 0.06 -0.06 0", "$($f + 108) button left 0",
                "$($f + 118) move left -0.06 0.06 0", "$($f + 128) hold saveroom", "$($f + 130) eyes saveroom",
                "$($f + 134) pointfile right aim1.txt", "$($f + 142) trigger right 1", "$($f + 152) trigger right 0",
                "$($f + 260) eyes turned", "$($f + 264) hold aim2", "$($f + 266) pointfile right aim2.txt",
                "$($f + 274) trigger right 1", "$($f + 284) trigger right 0", "$($f + 300) hold savebox",
                "$($f + 302) quads savebox")
            $f += 306
            foreach ($k in @($SaveName.ToCharArray() | ForEach-Object { "$_" }) + @('enter')) {
                $s += "$f point right key $k"
                $s += "$($f + 8) trigger right 1"
                $s += "$($f + 18) trigger right 0"
                if ($k -eq 'enter') { $s += "$($f + 6) quads typed2" }
                $f += 26
            }
            # The game keeps the box up after Enter: OK by the ray.
            $s += "$f point right panel $($SaveOk[0]) $($SaveOk[1])"
            $s += "$($f + 8) trigger right 1"
            $s += "$($f + 18) trigger right 0"
            $f += 26
            $s += "$f hold saved2"; $s += "$($f + 2) quads saved2"; $s += "$($f + 4) eyes saved2"
            $f += 8
            $menuSteps += $s
            $shift2 = $f - 400
        }
    }
    $script = Join-Path $Out 'script.txt'
    Set-Content -LiteralPath $script -Encoding ascii -Value (@(
        '10 hold ready',
        '20 point right menu 800 292', '30 capture before',
        '40 trigger right 1', '44 trigger right 0') + $profileSteps + (@(
        '50 hold measured1',
        $(if ($SceneEyes) { '51 eyes new' } else { '' }), '52 quads new',
        # Game 1: show (a hold of 90 frames), hide (a press of 6 frames),
        # show again, Quit game. No press reaches the mod.
        '60 menu left 1', '150 menu left 0',
        '170 capture over1',
        '180 menu left 1', '186 menu left 0',
        '200 quads hidden1', '205 capture notice1',
        '220 menu left 1', '310 menu left 0',
        '330 capture again1',
        '340 point right menu 800 768', '350 trigger right 1', '354 trigger right 0',
        '360 hold ended1',
        '370 capture after1',
        '380 point right menu 800 227', '390 trigger right 1', '394 trigger right 0') + $menuSteps + @(@(
        '400 hold measured2',
        $(if ($SceneEyes) { '401 eyes play' } else { '' }), '402 quads play',
        # Game 2: a short press (6 frames) goes to the mod at its release,
        # then show and Quit game.
        '410 menu left 1', '416 menu left 0',
        '470 menu left 1', '560 menu left 0',
        '580 capture over2',
        '590 point right menu 800 768', '600 trigger right 1', '604 trigger right 0',
        '610 hold ended2',
        '620 capture after2',
        '640 exit') | ForEach-Object {
            # The steps of game 2 move by $shift2 (-MenuClick).
            $m = [regex]::Match($_, '^(\d+) (.*)$')
            if ($m.Success) { "$([int]$m.Groups[1].Value + $shift2) $($m.Groups[2].Value)" } else { $_ }
        }) | Where-Object { $_ -ne '' } | ForEach-Object {
            # The steps after the dialog of a new profile move by $shift.
            $m = [regex]::Match($_, '^(\d+) (.*)$')
            "$([int]$m.Groups[1].Value + $shift) $($m.Groups[2].Value)"
        }))
    Adb @('push', $script, "$XrDir/script.txt")
    Adb @('shell', 'chmod', '777', $XrDir)
    Adb @('shell', 'chmod', '755', "$XrDir/libbwvr_test_runtime.so")
    Adb @('shell', 'chmod', '644', "$XrDir/script.txt")
    Adb @('shell', 'chmod', '755', "$XrDir/cpu.sh")

    # 3. The APK, its folder and the copy of the game.
    Say 'Install the APK on a clean app folder'
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('uninstall', $Package)
    Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
    $install = AdbOut @('install', $Apk)
    Expect ($install -match 'Success') ('adb install: ' + ($install.Trim() -split "`n")[-1])
    $dump = AdbOut @('shell', 'dumpsys', 'package', $Package)
    $uid = [regex]::Match($dump, '(?:userId|appId)=(\d+)').Groups[1].Value
    $user = if ($uid) { 'u0_a' + ([int]$uid - 10000) } else { 'none' }
    Note "the user of the app is $user"
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 4
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('shell', 'rm', '-f', "$XrDir/report.txt", "$XrDir/*.bmp", "$XrDir/ready", "$XrDir/measured1",
        "$XrDir/measured2", "$XrDir/ended1", "$XrDir/ended2", "$XrDir/profile", "$XrDir/typed", "$XrDir/skipbox",
        "$XrDir/clicked", "$XrDir/panel1", "$XrDir/panel1done", "$XrDir/menu2", "$XrDir/menu2done", "$XrDir/ring2", "$XrDir/temple2", "$XrDir/saveroom",
        "$XrDir/aim2", "$XrDir/savebox", "$XrDir/saved2", "$XrDir/aim1.txt", "$XrDir/aim2.txt")

    Say "Copy the game folder $Game into the emulator (no Profiles, no files of the player)"
    $skipTop = @('Profiles', 'MemoryLeaks.txt', 'Scripts')
    $copied = 0
    $bytes = 0
    $t0 = Get-Date
    foreach ($item in Get-ChildItem -LiteralPath $Game -Force) {
        if ($skipTop -contains $item.Name) { continue }
        # The files of the mod of a desk run that runs now (the input
        # channel, the ring) are not files of the game. With a copy of them
        # the open of bwvr-input.chan in the app gives errno 13.
        if ($item.Name -like 'bwvr.ini*' -or $item.Name -like 'bwvr*.log' -or $item.Name -eq 'bwvr-hint-counts.ini' -or
            $item.Name -like 'bwvr-*.chan' -or $item.Name -like 'bwvr-*.ring') { continue }
        Adb @('push', $item.FullName, "$GameDir/")
        if ($item.PSIsContainer) {
            $files = @(Get-ChildItem -LiteralPath $item.FullName -Recurse -File -Force)
            $copied += $files.Count
            $bytes += ($files | Measure-Object -Property Length -Sum).Sum
        } else {
            $copied++
            $bytes += $item.Length
        }
    }
    # Scripts with no Creature of a profile (*.erc in Scripts\CreatureMind).
    $scripts = Join-Path $Game 'Scripts'
    foreach ($f in Get-ChildItem -LiteralPath $scripts -Recurse -File -Force) {
        if ($f.Extension -eq '.erc') { continue }
        $rel = $f.FullName.Substring($scripts.Length + 1).Replace('\', '/')
        $dir = [IO.Path]::GetDirectoryName($rel).Replace('\', '/')
        Adb @('shell', 'mkdir', '-p', "`"$GameDir/Scripts/$dir`"")
        Adb @('push', $f.FullName, "$GameDir/Scripts/$rel")
        $copied++
        $bytes += $f.Length
    }
    Note ("copied {0} files ({1:N1} MB) in {2:N0} s" -f $copied, ($bytes / 1MB), ((Get-Date) - $t0).TotalSeconds)
    $remote = AdbOut @('shell', 'du', '-s', '-k', $GameDir)
    $remoteKb = [int64]([regex]::Match($remote, '^(\d+)').Groups[1].Value)
    Expect ($remoteKb * 1024 -ge $bytes * 0.98) ("the game folder of the app has the copy: $remoteKb KB")
    $profiles = (AdbOut @('shell', 'ls', "$GameDir/Profiles")).Trim()
    Expect ($profiles -match 'No such file') 'the copy has no Profiles folder'
    $ini = Join-Path $Out 'bwvr.ini.pushed'
    # A render of 50 percent of the eye of the test runtime: 1380 by 1104.
    Set-Content -LiteralPath $ini -Encoding ascii -Value (@('[bwvr]', 'resolution_pct=50', "wine_debug=$WineDebug", 'menu_capture=1', "wine_audio=$WineAudio"))
    Adb @('push', $ini, "$GameDir/bwvr.ini")

    # 4. The run.
    Say 'Start the app with the test runtime'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    $log = WaitLog @('wine         : Wine is ready', 'FAULT') 900
    Expect ($log -match 'wine         : Wine is ready') 'Wine is ready'
    Expect ($log -match 'game         : play_game=1, play_menu_hold_ms=1000, play_menu_press=1\. Play starts the game in Wine') 'the app can play: Wine is in the APK'
    Expect ($log -match 'xr           : input_live=1\. The session has the actions of the mod') 'the session has the actions of the mod from its start'
    Hold 'ready'

    $runs = @(
        @{ N = 1; Name = 'Play, new game'; Args = ' -NEWGAME'; Eyes = 'new' },
        @{ N = 2; Name = 'Play in VR'; Args = ''; Eyes = 'play' })
    $results = @{}
    foreach ($r in $runs) {
        $n = $r.N
        Say "Game $n`: $($r.Name)"
        $log = WaitLog @("game         : $([regex]::Escape($r.Name)) at ", 'menu         : .*\. Saved\.') 120
        $playAt = $log.IndexOf("game         : $($r.Name) at ")
        Expect ($playAt -ge 0) "the trigger on `"$($r.Name)`" started the game"
        # bwvr.ini at the start of the game.
        Adb @('pull', "$GameDir/bwvr.ini", (Join-Path $Out "bwvr-game$n.ini"))
        $iniText = if (Test-Path -LiteralPath (Join-Path $Out "bwvr-game$n.ini")) { Get-Content -LiteralPath (Join-Path $Out "bwvr-game$n.ini") -Raw } else { '' }
        foreach ($kv in @('vk_stream_live=1', 'xr_input=1', 'vk_device=0', 'ui_panel=1', 'vk_pace=1', 'stream_live=1', 'input_live=1', 'vulkan_strict=1', 'backend=vulkan')) {
            Expect ($iniText -match "(?m)^$([regex]::Escape($kv))\r?$") "bwvr.ini at the start of game $n has $kv"
        }
        $log = WaitLog @("game         : the first frame of the game \(frame \d+(, a screen with no scene)?\) came", 'game         : the game ended with the exit code') ($StartMinutes * 60) $playAt
        $part = $log.Substring([math]::Max(0, $playAt))
        Expect ($part -match 'game         : ddraw\.dll of the APK \(\d+ bytes\) is in \S+/drive_c/windows/syswow64') "the ddraw.dll of the APK is in syswow64 of the prefix"
        $reg = [regex]::Match($part, 'game         : reg import of C:\\bwvr\\game\.reg ended with (-?\d+) after ([\d.]+) s: GameDir G:\\, ScreenW (\d+), ScreenH (\d+)')
        Expect ($reg.Success -and $reg.Groups[1].Value -eq '0') ('reg import of the registry of the game: ' + $reg.Value)
        if ($WineAudio -eq 1) {
            Expect ($part -match 'game         : reg import of .* and the audio driver aaudio \(wine_audio=1\)') "the registry of game $n has the audio driver aaudio"
        } else {
            Expect ($part -match 'game         : reg import of .* and no audio driver \(wine_audio=0\)') "the registry of game $n has no audio driver (wine_audio=0)"
        }
        Expect ($part -match "game         : start G:\\runblack\.exe$([regex]::Escape($r.Args)) in Wine") "the app started G:\runblack.exe$($r.Args)"
        $first = [regex]::Match($part, 'game         : the first frame of the game \(frame (\d+)(, a screen with no scene)?\) came ([\d.]+) s after Play')
        Expect $first.Success ("the first frame of game $n came: " + $first.Value)
        if (-not $first.Success) {
            # The test goes on: the CPU load, the menu over the game and Quit
            # game run with no frame too.
            $alive = $part -notmatch 'game         : the game ended with the exit code'
            Note ("the game gave no frame in $StartMinutes minutes" + $(if ($alive) { ', and it runs' } else { ', and it ended' }) + '. The end of the log of the app:')
            ($part -split "`n" | Select-Object -Last 8) | ForEach-Object { Note $_ }
            if (-not $alive) {
                foreach ($h in @('measured1', 'ended1', 'menu2', 'menu2done', 'measured2', 'ended2')) { Hold $h }
                break
            }
        }
        # THE DIALOG OF A NEW PROFILE (-NewProfile, game 1): the keyboard of
        # the app types the name, and Enter.
        if ($NewProfile -and $n -eq 1 -and $first.Success) {
            ProfileDialog $playAt
        }
        # THE MAIN MENU over the land of the test save (-MenuClick, game 2).
        if ($MenuClick.Count -eq 2 -and $n -eq 2 -and $first.Success) {
            PanelClicks 'MainMenu' $MenuClick 'menu2' 'Main Menu'
            if ($SaveName -ne '') { SaveFlow }
        }
        # The measure: the CPU time of the processes of Wine over PlaySeconds.
        $a = CpuSample $user
        Start-Sleep -Seconds $PlaySeconds
        $b = CpuSample $user
        $load = CpuLoad $a $b
        Set-Content -LiteralPath (Join-Path $Out "cpu-game$n.txt") -Value $load.Lines -Encoding utf8
        Note ("the CPU load in {0:N1} s: the processes of Wine {1:N0} % (100 % is one CPU), the app {2:N0} %" -f $load.Seconds, $load.Wine, $load.App)
        $load.Lines | Select-Object -First 8 | ForEach-Object { Note $_ }
        Expect ($load.Wine -gt 0) "the processes of Wine ran ($([math]::Round($load.Wine)) % of one CPU)"
        $results["cpu$n"] = $load
        # The tree of the processes of the user: which process the app
        # started, and the parent of runblack.exe.
        $tree = @((AdbOut @('shell', 'ps', '-A', '-o', 'PID,PPID,USER,NAME')) -split "`r?`n" | Where-Object { $_ -match "^\s*\d+\s+\d+\s+$User\s" })
        Set-Content -LiteralPath (Join-Path $Out "tree-game$n.txt") -Value $tree -Encoding utf8
        Note 'the processes of the user of the app (PID PPID USER NAME):'
        $tree | ForEach-Object { Note $_.Trim() }
        $started = [regex]::Match($log.Substring([math]::Max(0, $playAt)), 'wine         : the process (\d+) of the app runs G:\\runblack\.exe').Groups[1].Value
        $byPid = @{}
        foreach ($t in $tree) { $f = $t.Trim() -split '\s+', 4; if ($f.Count -eq 4) { $byPid[$f[0]] = @{ Parent = $f[1]; Name = $f[3] } } }
        $game = @($byPid.Keys | Where-Object { $byPid[$_].Name -match 'runblack\.exe' }) | Select-Object -First 1
        $gameParent = if ($game) { $byPid[$game].Parent } else { '' }
        $parentName = if ($byPid.ContainsKey($gameParent)) { $byPid[$gameParent].Name } else { 'not of the user' }
        $startedName = if ($started -and $byPid.ContainsKey($started)) { $byPid[$started].Name } else { 'not there' }
        Note "the app started the process $started ($startedName). runblack.exe is $game, its parent is $gameParent ($parentName)"
        $results["tree$n"] = "the app started $started ($startedName), runblack.exe $game has the parent $gameParent ($parentName)"
        Expect ($started -ne '' -and $byPid.ContainsKey($started)) "the log names the process that the app started, and it runs ($started, $startedName)"
        Hold "measured$n"
        # The eye pictures (the read back takes a time), then the menu over
        # the game, and Quit game.
        $log = WaitLog @('game         : Quit game') 900 $playAt
        $log = WaitLog @("game         : the game ended with the exit code -?\d+ at [\d:]+ \(Quit game\)") 300 $playAt
        $part = $log.Substring([math]::Max(0, $playAt))
        $show = 'game         : the menu shows over the game'
        $hide = 'game         : the menu goes. The game has the hands again'
        $quitAt = $part.IndexOf('game         : Quit game')
        $overAt = $part.IndexOf($show)
        if ($n -eq 1) {
            # Show, hide with a press, show again, then Quit game.
            $hideAt = if ($overAt -ge 0) { $part.IndexOf($hide, $overAt) } else { -1 }
            $againAt = if ($hideAt -ge 0) { $part.IndexOf($show, $hideAt) } else { -1 }
            Expect ($overAt -ge 0 -and $hideAt -gt $overAt -and $againAt -gt $hideAt -and $quitAt -gt $againAt) 'game 1: the hold showed the menu over the game, a press hid it, a hold showed it again, then Quit game'
            Expect ($part.Substring(0, [math]::Max(0, $quitAt)) -notmatch 'game         : a press of the menu button goes to the mod') 'game 1: the app gave no press of the menu button to the mod (the holds and the press that hid the menu)'
        } else {
            Expect ($overAt -ge 0 -and $quitAt -gt $overAt) "the hold of the menu button showed the menu over game $n before Quit game"
            Expect ($part -match 'game         : a press of the menu button goes to the mod at its release \(play_menu_press=1\)') 'game 2: the short press went to the mod at its release'
        }
        Expect ($part -match 'game         : Quit game') "the trigger on Quit game in game $n"
        $sig = [regex]::Match($part, 'wine         : Quit game sends SIGKILL to the process (\d+)')
        Expect ($sig.Success -and $sig.Groups[1].Value -eq $started) ("Quit game sent SIGKILL to the process that the app started: " + $sig.Value)
        $ended = [regex]::Match($part, 'game         : the process of the game \(start\.exe of Wine, for runblack\.exe\) ended with (-?\d+) after ([\d.]+) s \(Quit game\)')
        Expect $ended.Success ('the process that the app started ended: ' + $ended.Value)
        $kill = [regex]::Match($part, 'game         : wineserver -k ended with (-?\d+) after')
        Expect ($kill.Success -and $kill.Groups[1].Value -eq '0') ('wineserver -k after Quit game stopped the server: ' + $kill.Value)
        Expect ($part -match 'game         : the game ended with the exit code -?\d+ at [\d:]+ \(Quit game\)') 'the menu came back with the exit code'
        Start-Sleep -Seconds 5
        $left = @(WineProcesses $user)
        Expect ($left.Count -eq 0) ("no process of Wine is left after game $n" + $(if ($left.Count) { ': ' + ($left -join '; ') } else { '' }))
        if ($NewProfile -and $n -eq 1) {
            # The server of Wine wrote the registry at its end.
            $prefixReg = AdbOut @('shell', 'cat', "/data/data/$Package/files/prefix/user.reg")
            $prof = @($prefixReg -split "`r?`n" | Where-Object { $_ -match '^"Profile"=' })
            Note ('user.reg of the prefix after game 1: ' + ($prof -join ' '))
            Expect ($prof.Count -gt 0) 'the registry of the prefix has the value Profile of the game after game 1'
            if ($TestSave -ne '') {
                # The test save into slot 20 of the profile of game 1.
                $common = (& git -C $Repo rev-parse --path-format=absolute --git-common-dir 2>$null | Out-String).Trim()
                $main = if ($common) { Split-Path -Parent ($common -replace '/', '\') } else { $Repo }
                $save = Join-Path $main "local\test-saves\$TestSave\20"
                $want = (($ProfileName.ToCharArray() | ForEach-Object { "_$_" }) -join '')
                $slot = "$GameDir/Profiles/$want/Saved Games/20"
                if (Test-Path -LiteralPath (Join-Path $save '20.sav')) {
                    # The path has a space: one command line for the shell.
                    Adb @('shell', "mkdir -p '$slot' && chmod 777 '$slot'")
                    $files = @(Get-ChildItem -LiteralPath $save -File)
                    foreach ($f in $files) { Adb @('push', $f.FullName, "$slot/$($f.Name)") }
                    Adb @('shell', "chmod 666 '$slot'/*")
                    $there = (AdbOut @('shell', "ls '$slot'")).Trim() -split '\s+'
                    Expect (@($files | Where-Object { $there -contains $_.Name }).Count -eq $files.Count) "the test save $TestSave is in slot 20 of the profile $ProfileName in the emulator ($($files.Count) files)"
                } else {
                    Expect $false "the test save $TestSave is in $save"
                }
            }
        }
        # The frame rate of the game.
        $rates = @([regex]::Matches($part, 'game         : frame (\d+), ([\d.]+) frames a second in the last ([\d.]+) s') | ForEach-Object { [double]::Parse($_.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture) })
        if ($rates.Count -gt 0) {
            Note ("the frame rate of game {0}: {1} (frames a second each 10 s), mean {2:N1}" -f $n, ($rates -join ' '), (($rates | Measure-Object -Average).Average))
        }
        Expect ($rates.Count -gt 0) "the app logged the frame rate of game $n ($($rates.Count) lines)"
        $results["rates$n"] = $rates
        $results["first$n"] = $first.Groups[3].Value
        $results["firstScreen$n"] = $first.Groups[2].Success
        # The log of the mod of this game: the Esc of the menu button. In
        # game 1 no press reached the mod, in game 2 the short press did.
        Adb @('pull', "$GameDir/bwvr.log", (Join-Path $Out "bwvr-game$n.log"))
        $modLogNow = if (Test-Path -LiteralPath (Join-Path $Out "bwvr-game$n.log")) { Get-Content -LiteralPath (Join-Path $Out "bwvr-game$n.log") -Raw } else { '' }
        $escLines = @($modLogNow -split "`r?`n" | Where-Object { $_ -match 'controllers  : the menu button (sends Esc|is dropped)' })
        $escLines | ForEach-Object { Note ('bwvr.log: ' + $_) }
        $sceneHere = $first.Success -and (-not $first.Groups[2].Success -or $part -match 'game         : the frames of the scene start')
        if (-not $sceneHere) {
            # The mod reads the buttons in a frame of the scene
            # (ControllersFrame of stereo.cpp). With no such frame its log
            # cannot show a press: the log of the app above is the proof.
            Note "no frame of the scene came, so the mod read no button in game $n ($($escLines.Count) lines of the Esc of the mod)"
        } elseif ($n -eq 1) {
            Expect ($escLines.Count -eq 0) "game 1: the mod got no press of the menu button ($($escLines.Count) lines of the mod)"
        } else {
            Expect ($escLines.Count -ge 1) "game 2: the mod got the short press of the menu button ($($escLines.Count) lines of the mod)"
        }
        Adb @('pull', "$AppDir/wine.log", (Join-Path $Out "wine-game$n.log"))
        # The sound of this game: the stream of wineaaudio in wine.log, and
        # QMixer in the log of the mod.
        $wineNow = if (Test-Path -LiteralPath (Join-Path $Out "wine-game$n.log")) { Get-Content -LiteralPath (Join-Path $Out "wine-game$n.log") -Raw } else { '' }
        $streams = @($wineNow -split "`r?`n" | Where-Object { $_ -match '^wineaaudio: (opened|opened again) the output stream' })
        $streams | Select-Object -First 4 | ForEach-Object { Note ('wine.log: ' + $_) }
        $qmixer = @($modLogNow -split "`r?`n" | Where-Object { $_ -match '^audio out    : (QMixer starts|the stream of QMixer is|QMixer gave no block)' })
        $qmixer | ForEach-Object { Note ('bwvr.log: ' + $_) }
        if ($WineAudio -eq 1) {
            Expect ($streams.Count -ge 1) "game $n`: wineaaudio opened an output stream ($($streams.Count) in wine.log)"
            Expect ($wineNow -notmatch 'No driver from .* could be initialized') "game $n`: mmdevapi found an audio driver (no line `"No driver ... could be initialized`")"
            Expect ($modLogNow -match 'audio out    : QMixer starts' -and $modLogNow -notmatch 'audio out    : QMixer gave no block') "game $n`: QMixer started and gave blocks to its output"
            Expect ($modLogNow -match 'audio out    : the stream of QMixer is \d+ Hz, 16 bit stereo PCM') "game $n`: the mod knows the stream of QMixer (16 bit stereo PCM, the mixer of the mod can work)"
        } else {
            # wine_audio=0: no audio driver, no sound.
            Expect ($streams.Count -eq 0) "game $n with wine_audio=0: wineaaudio opened no output stream"
            $noDriver = [regex]::Match($wineNow, 'No driver from .* could be initialized')
            Expect $noDriver.Success ("game $n with wine_audio=0: mmdevapi found no audio driver: " + $noDriver.Value)
            Expect ($modLogNow -match 'audio out    : QMixer starts') "game $n with wine_audio=0: QMixer starts"
        }
        Hold "ended$n"
    }

    # 5. The end of the session.
    $report = ''
    $deadline = (Get-Date).AddSeconds(300)
    do {
        Start-Sleep -Seconds 2
        $report = AdbOut @('shell', 'cat', "$XrDir/report.txt")
    } while ($report -notmatch 'call xrDestroyInstance' -and (Get-Date) -lt $deadline)
    Start-Sleep -Seconds 2

    Say 'Read the results'
    Adb @('pull', "$XrDir/.", $Out)
    Adb @('pull', "$AppDir/bwvr-quest.log", (Join-Path $Out 'bwvr-quest.log'))
    Adb @('pull', "$AppDir/wine.log", (Join-Path $Out 'wine.log'))
    Adb @('pull', "$GameDir/bwvr.ini", (Join-Path $Out 'bwvr.ini'))
    Adb @('pull', "$GameDir/bwvr.log", (Join-Path $Out 'bwvr.log'))
    $prefixReg = AdbOut @('shell', 'cat', "/data/data/$Package/files/prefix/user.reg")
    $logcat = AdbOut @('logcat', '-d', '-v', 'brief')
    Set-Content -LiteralPath (Join-Path $Out 'logcat.txt') -Value $logcat -Encoding utf8
    if (Test-Path -LiteralPath (Join-Path $Out 'report.txt')) { $report = Get-Content -LiteralPath (Join-Path $Out 'report.txt') -Raw }
    $log = if (Test-Path -LiteralPath (Join-Path $Out 'bwvr-quest.log')) { Get-Content -LiteralPath (Join-Path $Out 'bwvr-quest.log') -Raw } else { '' }

    # The audio driver in user.reg of the prefix after the Play of game 2
    # (wine_audio).
    $audioReg = @($prefixReg -split "`r?`n" | Where-Object { $_ -match '^"Audio"=' })
    if ($WineAudio -eq 1) {
        Expect ($audioReg.Count -eq 1 -and $audioReg[0] -match '^"Audio"="aaudio"') ('user.reg of the prefix after Play has "Audio"="aaudio": ' + ($audioReg -join ' '))
    } else {
        Expect ($audioReg.Count -eq 0) ('user.reg of the prefix after Play has no Audio value (wine_audio=0)' + $(if ($audioReg.Count) { ': ' + ($audioReg -join ' ') } else { '' }))
    }
    Expect ($log -match "wine         : wine_audio=$WineAudio, wine_audio_low_latency=1\.") "the log of the app has wine_audio=$WineAudio"
    Expect ($report -notmatch '(?m)^fault ') ('the runtime found no fault' + $(if ($report -match '(?m)^fault (.*)$') { ': ' + $Matches[1] } else { '' }))
    Expect ($log -match 'xr           : the GPU of the runtime: .*The renderer of the mod: yes') 'the device of the session has Vulkan 1.3 and the features of the renderer'
    Expect ($log -match 'live         : frames come') 'the reader of the app took frames from the ring'
    $summary = [regex]::Matches($log, 'live         : [^:]+: (\d+) frames played and (\d+) skipped \(frames (\d+) to (\d+)\), (\d+) pictures')
    foreach ($s in $summary) { Note $s.Value }
    Expect ($summary.Count -gt 0 -and [int64]$summary[0].Groups[5].Value -gt 0) 'the app played the frames of the ring and made pictures'
    # THE FRAMES OF THE SCENE: the projection layer. A game that waits in a
    # dialog box before its first frame of the scene has none.
    $sceneFrames = $log -match 'game         : the frames of the scene start' -or
        ($log -match 'game         : the first frame of the game \(frame \d+\) came')
    if ($sceneFrames -or $SceneEyes) {
        Expect ($log -match 'xr           : the pictures of the renderer go out in a projection layer') 'the pictures of the game go out in a projection layer'
        $projectionLines = ([regex]::Matches($report, '(?m)^frame \d+ projection ')).Count
        Expect ($projectionLines -gt 0) "the runtime reports the projection layer ($projectionLines lines)"
    } else {
        Note 'no frame of the scene came in a game, so there is no projection layer to check'
    }
    if ($SceneEyes) {
        foreach ($name in @('new', 'play')) {
            foreach ($eye in 0, 1) {
                $m = [regex]::Match($report, "capture \S*/$name-p$eye\.bmp (\d+)x(\d+) lit (\d+)")
                Expect ($m.Success -and [int64]$m.Groups[3].Value -gt 1000) ("the picture of eye $eye of the projection layer in game $name has the game: " + $(if ($m.Success) { "$($m.Groups[1].Value) by $($m.Groups[2].Value), $($m.Groups[3].Value) lit pixels" } else { 'none' }))
            }
        }
    }
    # THE SCREEN QUAD of the frames with no scene (stream_screen=1): the
    # logos, the video before the intro and the dialog box. The quad of the
    # screen of the PC: 2.6 m wide (screen_width_cm), 2 m in front
    # (screen_distance_cm), opaque, for both eyes, with the whole render.
    $screenShown = @([regex]::Matches($log, 'xr           : the screen quad shows screen (\d+) of the mod \(([^,]+), (\d+) by (\d+)\) as one quad layer, ([0-9.]+) by ([0-9.]+) m at ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) m'))
    foreach ($s in $screenShown | Select-Object -First 4) { Note $s.Value }
    if (@($results.Keys | Where-Object { $_ -like 'firstScreen*' -and $results[$_] }).Count -gt 0) {
        Expect ($screenShown.Count -gt 0) "the app showed the frames with no scene on the screen quad ($($screenShown.Count) lines)"
        $screenQuad = ' quad 0\.0000 0\.0000 -2\.0000 turn 0\.0000 0\.0000 0\.0000 1\.0000 size 2\.6000 ([0-9.]+) rect 0 0 (\d+)x(\d+) eye 0 flags 0'
        $screenQuadLines = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
        $withScreen = @($screenQuadLines | Where-Object { $_.Groups[2].Value -match "^$screenQuad" })
        $screenSize = if ($withScreen.Count -gt 0) { [regex]::Match($withScreen[0].Groups[2].Value, "^$screenQuad") } else { $null }
        Expect ($withScreen.Count -gt 0) ("the runtime has the screen quad, 2.6 m wide at 0 0 -2 m, opaque, for both eyes" + $(if ($screenSize) { ": $($screenSize.Groups[2].Value) by $($screenSize.Groups[3].Value) pixels, $($screenSize.Groups[1].Value) m high, $($withScreen.Count) lines" } else { '' }))
        # The stereo screen in the lines "layers" (3 decimals) and "quads" (4
        # decimals and the turn) of the runtime, and in the log of the app.
        $stereoAny = 'quad 0\.000 0\.100 -2\.600 size 2\.600|quad 0\.0000 0\.1000 -2\.6000 turn [-0-9. ]+ size 2\.6000'
        $stereoLines = ([regex]::Matches($report, $stereoAny)).Count
        # The app makes the swapchains of the stereo screen with the first
        # picture of the world (ShowEyes), so only the log before the first
        # frame of the scene counts.
        $sceneAt = $log.IndexOf('live         : the frames of the scene start')
        $beforeScene = if ($sceneAt -ge 0) { $log.Substring(0, $sceneAt) } else { $log }
        Expect ($stereoLines -eq 0 -and $beforeScene -notmatch 'a stereo screen of two quads') "no frame has the quads of the stereo screen, and the app made no stereo screen for a screen with no scene ($stereoLines lines of the runtime)"
        # A game whose dialog box went by a click of the ray on the panel
        # (-PanelClicks, -MenuClick) may have no quad at the measure: its
        # first quad is in the picture of the step before the click.
        foreach ($name in @($(if ($PanelClicks.Count -ge 2) { 'panel1' } else { 'new' }), $(if ($MenuClick.Count -eq 2) { 'menu2' } else { 'play' }))) {
            $m = [regex]::Match($report, "capture \S*/$name-0\.bmp (\d+)x(\d+) lit (\d+)")
            Expect ($m.Success -and [int64]$m.Groups[3].Value -gt 1000) ("the picture $name-0 of the first quad has the game (" + $(if ($name -eq 'panel1') { 'game 1, the start dialog on the menu panel quad before the clicks' } elseif ($name -eq 'menu2') { 'game 2, the Main Menu on the menu panel quad before the click' } else { "the screen quad after the measure of game $name" }) + "): " + $(if ($m.Success) { "$($m.Groups[1].Value) by $($m.Groups[2].Value), $($m.Groups[3].Value) lit pixels (the files $name-0.bmp and $name-0.png)" } else { 'none' }))
        }
    }
    # The notice (play_notice=1): with the menu hidden before the first frame
    # of the game the quad shows the notice, a picture that is not the main
    # page of the menu.
    if ($log -notmatch 'game         : the first frame of the game') {
        $n = [regex]::Match($report, 'capture \S*/notice1-\d+\.bmp 1600x1200 lit (\d+)')
        $b = [regex]::Match($report, 'capture \S*/before-\d+\.bmp 1600x1200 lit (\d+)')
        $nLit = if ($n.Success) { [int64]$n.Groups[1].Value } else { 0 }
        $bLit = if ($b.Success) { [int64]$b.Groups[1].Value } else { 0 }
        Expect ($nLit -gt 100000 -and $bLit -gt 0 -and [math]::Abs($nLit - $bLit) -gt $bLit * 0.02) "the quad shows the notice with the menu hidden, not the main page ($nLit lit pixels, the main page $bLit)"
    }

    foreach ($name in @('before', 'over1', 'again1', 'after1', 'over2', 'after2')) {
        $m = [regex]::Match($report, "capture \S*/$name-\d+\.bmp 1600x1200 lit (\d+)")
        Expect ($m.Success -and [int]$m.Groups[1].Value -gt 100000) "the picture $name of the menu quad has the menu ($($m.Groups[1].Value) lit pixels)"
    }
    # With no menu (the game runs) the runtime gets no quad of 1600 by 1200
    # after the first frame of the game. Before it the quad shows the notice
    # (play_notice=1).
    $quadLines = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
    $noMenu = @($quadLines | Where-Object { $_.Groups[2].Value -notmatch 'rect 0 0 1600x1200' })
    if ($log -match 'game         : the first frame of the game') {
        Expect ($noMenu.Count -gt 0) "the runtime had frames with no menu quad while the game ran ($($noMenu.Count) lines of the quads)"
    } else {
        Expect ($log -match 'game         : play_notice=1\. The quad of the menu shows a notice') 'no first frame of the game: the quad of the menu shows the notice (play_notice=1)'
        Expect ($noMenu.Count -eq 0) "no first frame of the game: each frame of the runtime has the quad of the menu ($($quadLines.Count) lines of the quads, $($noMenu.Count) with no menu)"
    }

    # THE LOG OF THE MOD of the first game (the second game writes it again).
    foreach ($n in 1, 2) {
        $modLog = if (Test-Path -LiteralPath (Join-Path $Out "bwvr-game$n.log")) { Get-Content -LiteralPath (Join-Path $Out "bwvr-game$n.log") -Raw } else { '' }
        $head = ($modLog -split "`r?`n")[0]
        $build = [regex]::Match($head, '^Black & White VR Mod (\S+) \(build ([^)]+)\)')
        Note "the mod of game $n`: $head"
        Expect ($build.Success -and $modText.Contains($build.Groups[2].Value)) "the mod of game $n is the ddraw.dll of the APK (build $($build.Groups[2].Value))"
        if ($gameDdrawText -ne '' -and $build.Success) {
            Expect (-not $gameDdrawText.Contains($build.Groups[2].Value)) "the ddraw.dll of the game folder of the copy is another build, and Wine loaded the known DLL of the prefix"
        }
        Expect ($modLog -match 'vk device    : vk_device=0\. The mod makes no Vulkan instance and no device') "the mod of game $n has vk_device=0"
        Expect ($modLog -notmatch 'vulkan       : the device|vkCreateDevice|vulkan-1\.dll is loaded') "the mod of game $n made no Vulkan device"
        Expect ($modLog -match 'xr input     : xr_input=1\.') "the mod of game $n reads the input channel"
        Expect ($modLog -match 'xr input     : the first slot of the app') "the mod of game $n read a slot of the app"
        Expect ($modLog -match 'vk stream    : vk_stream_live=1\. The ring') "the mod of game $n writes the ring"
        Expect ($modLog -match 'vulkan strict: vulkan_strict=1') "the mod of game $n has vulkan_strict=1"
        $firstScreen = [regex]::Match($modLog, 'vk stream    : screen frame (\d+) is the first frame with no scene in the stream: ([^,]+), (\d+) by (\d+), on a quad ([0-9.]+) by ([0-9.]+) m')
        Expect $firstScreen.Success ("the mod of game $n gave the frames with no scene to the stream" + $(if ($firstScreen.Success) { ": the first is screen frame $($firstScreen.Groups[1].Value), $($firstScreen.Groups[2].Value), $($firstScreen.Groups[3].Value) by $($firstScreen.Groups[4].Value), on a quad $($firstScreen.Groups[5].Value) by $($firstScreen.Groups[6].Value) m" } else { '' }))
        $boxLines = @($modLog -split "`r?`n" | Where-Object { $_ -match '^setup box    : ' })
        $boxLines | Select-Object -First 6 | ForEach-Object { Note ('bwvr.log: ' + $_) }
        Expect (@($boxLines | Where-Object { $_ -match '^setup box    : the dialog box \S+ is up now' }).Count -gt 0) "the log of the mod of game $n names the class of the dialog box that is up ($($boxLines.Count) lines)"
        $rateLines = @($modLog -split "`r?`n" | Where-Object { $_ -match '^vk stream    : screen frame \d+, [0-9.]+ screen frames a second' })
        $rateLines | Select-Object -First 4 | ForEach-Object { Note ('bwvr.log: ' + $_) }
        # THE PACE (vk_pace=1 of "Play", docs/android.md, "The pace of the
        # frames"): each frame of the game waits for the next slot of the app,
        # so the game makes no more frames than the session (72 a second with
        # the test runtime).
        Expect ($modLog -match 'xr input     : vk_pace=1 with vk_device=0\. Each frame of the game waits') "the mod of game $n paces its frames by the slots of the app (vk_pace=1)"
        $paceLines = @($modLog -split "`r?`n" | Where-Object { $_ -match '^xr input     : the pace: ' })
        $paceLines | Select-Object -First 6 | ForEach-Object { Note ('bwvr.log: ' + $_) }
        $paceRates = @($paceLines | ForEach-Object { [double]::Parse([regex]::Match($_, 'frames in [0-9.]+ s \(([0-9.]+) a second\)').Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) })
        $paceWrites = @($paceLines | Where-Object { $_ -match 'a second\), [1-9]\d* started at a new write' })
        Expect ($paceLines.Count -gt 0 -and $paceWrites.Count -gt 0) "the log of the mod of game $n has the lines of the pace, and frames started at a new write of the slot ($($paceLines.Count) lines, $($paceWrites.Count) with a new write)"
        $paceMost = if ($paceRates.Count) { ($paceRates | Measure-Object -Maximum).Maximum } else { 0 }
        Expect ($paceRates.Count -gt 0 -and $paceMost -le 80) ("the paced frames of game $n come at most at the rate of the session: " + ($paceRates -join ' ') + ' a second')
    }

    # THE FACTS for the doc: the sound, the window of the null driver, the
    # noexec storage. The test does not judge them.
    Say 'Facts'
    $wineLog = ''
    foreach ($n in 1, 2) {
        $f = Join-Path $Out "wine-game$n.log"
        if (Test-Path -LiteralPath $f) { $wineLog += (Get-Content -LiteralPath $f -Raw) }
    }
    $modLog1 = if (Test-Path -LiteralPath (Join-Path $Out 'bwvr-game1.log')) { Get-Content -LiteralPath (Join-Path $Out 'bwvr-game1.log') -Raw } else { '' }
    $facts = New-Object System.Collections.Generic.List[string]
    $facts.Add("wine.log: $(([regex]::Matches($wineLog, 'falling back to read')).Count) lines 'falling back to read', $(([regex]::Matches($wineLog, 'noexec filesystem\?')).Count) lines 'noexec filesystem?'")
    foreach ($p in @('falling back to read', 'noexec', 'dsound', 'winmm', 'mmdevapi', 'audio', 'sound', 'explorer', 'desktop', 'nulldrv', 'display', 'socket')) {
        $lines = @($wineLog -split "`r?`n" | Where-Object { $_ -match $p -and $_ -notmatch '^wineaaudio: ' } | Select-Object -First 6)
        foreach ($l in $lines) { $facts.Add("wine.log [$p]: $l") }
    }
    foreach ($l in @($wineLog -split "`r?`n" | Where-Object { $_ -match '^wineaaudio: ' } | Select-Object -First 30)) { $facts.Add("wine.log [wineaaudio]: $l") }
    foreach ($p in @('audio out', 'window', 'focus', 'activ', 'front', 'pause', 'dialog', 'intro skip')) {
        $lines = @($modLog1 -split "`r?`n" | Where-Object { $_ -match $p } | Select-Object -First 6)
        foreach ($l in $lines) { $facts.Add("bwvr.log [$p]: $l") }
    }
    foreach ($l in @($modLog1 -split "`r?`n" | Where-Object { $_ -match '^audio (out|mixer) ' } | Select-Object -First 30)) { $facts.Add("bwvr.log [audio]: $l") }
    $facts.Add("user.reg of the prefix: " + (@($prefixReg -split "`r?`n" | Where-Object { $_ -match 'Lionhead|GameDir|Screen|FullScreen' }) -join ' | '))
    Set-Content -LiteralPath (Join-Path $Out 'facts.txt') -Value $facts -Encoding utf8
    $facts | ForEach-Object { Note $_ }

    Expect ($logcat -notmatch 'JNI DETECTED ERROR' -and $logcat -notmatch 'Fatal signal' -and $logcat -notmatch 'FATAL EXCEPTION') 'logcat has no JNI error and no crash'
    Expect ($log -match 'app          : the app ends') 'the app ended after the runtime ended the session'
    foreach ($n in 1, 2) {
        if ($results.ContainsKey("first$n")) { Note ("game {0}: the first frame {1} s after Play, the frame rate {2}, the CPU load of Wine {3:N0} %" -f $n, $results["first$n"], ($results["rates$n"] -join ' '), $results["cpu$n"].Wine) }
    }

    Add-Type -AssemblyName System.Drawing
    foreach ($bmp in Get-ChildItem -LiteralPath $Out -Filter '*.bmp') {
        $image = [System.Drawing.Image]::FromFile($bmp.FullName)
        try { $image.Save([IO.Path]::ChangeExtension($bmp.FullName, '.png'), [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $image.Dispose() }
    }
} catch {
    Expect $false ('the script ran to its end: ' + $_.Exception.Message +
        ' (line ' + $_.InvocationInfo.ScriptLineNumber + ')')
} finally {
    if (-not $Keep) {
        # The copy of the game stays in the emulator only for the test.
        Say 'Delete the app and the copy of the game in the emulator'
        Adb @('shell', 'am', 'force-stop', $Package)
        Adb @('uninstall', $Package)
        Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
        $gone = (AdbOut @('shell', 'ls', "/sdcard/Android/data/$Package")).Trim()
        Write-Host ('   ' + $(if ($gone -match 'No such file') { 'the copy of the game is gone' } else { "NOTE: /sdcard/Android/data/$Package is still there" }))
    }
    if ($started -and -not $Keep) {
        Say 'Stop the emulator'
        & $Adb -s $Serial emu kill 2>&1 | Out-Null
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
