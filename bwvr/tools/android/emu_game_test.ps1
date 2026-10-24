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
    no Vulkan device, the input channel, the ring), the frames that the app
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
    $script = Join-Path $Out 'script.txt'
    Set-Content -LiteralPath $script -Encoding ascii -Value @(
        '10 hold ready',
        '20 point right menu 800 292', '30 capture before',
        '40 trigger right 1', '44 trigger right 0',
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
        '380 point right menu 800 227', '390 trigger right 1', '394 trigger right 0',
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
        '640 exit' | Where-Object { $_ -ne '' })
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
        "$XrDir/measured2", "$XrDir/ended1", "$XrDir/ended2")

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
    Set-Content -LiteralPath $ini -Encoding ascii -Value @('[bwvr]', 'resolution_pct=50', "wine_debug=$WineDebug", 'menu_capture=1', "wine_audio=$WineAudio")
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
        foreach ($kv in @('vk_stream_live=1', 'xr_input=1', 'vk_device=0', 'ui_panel=1', 'stream_live=1', 'input_live=1', 'vulkan_strict=1', 'backend=vulkan')) {
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
                foreach ($h in @('measured1', 'ended1', 'measured2', 'ended2')) { Hold $h }
                break
            }
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
        Expect ($stereoLines -eq 0 -and $log -notmatch 'a stereo screen of two quads') "no frame has the quads of the stereo screen, and the app made no stereo screen ($stereoLines lines of the runtime)"
        foreach ($name in @('new', 'play')) {
            $m = [regex]::Match($report, "capture \S*/$name-0\.bmp (\d+)x(\d+) lit (\d+)")
            Expect ($m.Success -and [int64]$m.Groups[3].Value -gt 1000) ("the picture of the screen quad after the measure of game $name has the game: " + $(if ($m.Success) { "$($m.Groups[1].Value) by $($m.Groups[2].Value), $($m.Groups[3].Value) lit pixels (the files $name-0.bmp and $name-0.png)" } else { 'none' }))
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
