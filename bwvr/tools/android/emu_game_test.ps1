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
         points at "Quit game" and pulls the trigger. It waits
         until the log says that the game ended.
      4. the same with "Play in VR" (no word on the command line: the game
         loads slot 20 of the profile, or starts a new game).
      5. ends the session.

    The checks: the files of the mod in the prefix, the registry, the keys
    of the run in bwvr.ini at the start of the game, the log of the mod (the
    build of the APK and not the ddraw.dll of the game folder, vk_device=0,
    no Vulkan device, the input channel, the ring), the frames that the app
    took from the ring, the projection layer, the eye pictures, the time
    from Play to the first frame, the frame rate of the game, the CPU load
    of the processes of Wine, the menu over the game, Quit game, no process
    of Wine after each game, and no crash. The test also collects the facts
    of the sound, of the null display driver and of the noexec storage (the
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
    Say "Start the emulator (no window, a system that it can write, the GPU of the PC, $Memory MB)"
    Start-Process -FilePath $Emulator -ArgumentList @('-avd', $Avd, '-port', "$Port",
        '-no-window', '-no-audio', '-no-boot-anim', '-no-snapshot', '-writable-system',
        '-gpu', 'host', '-memory', "$Memory") -WindowStyle Hidden | Out-Null
    $started = $true
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

    # THE SCRIPT OF THE CONTROLLERS. The rows of the menu are 74 pixels high
    # from y = 190 (menu_layout.h): row r has its middle at 227 + 74 r. With
    # no game, row 0 is "Play in VR" and row 1 "Play, new game". While the
    # game runs, row 0 is "Quit game".
    $script = Join-Path $Out 'script.txt'
    Set-Content -LiteralPath $script -Encoding ascii -Value @(
        '10 hold ready',
        '20 point right menu 800 301', '30 capture before',
        '40 trigger right 1', '44 trigger right 0',
        '50 hold measured1',
        '51 eyes new', '52 quads new',
        '60 menu left 1', '150 menu left 0',
        '170 capture over1',
        '180 point right menu 800 227', '190 trigger right 1', '194 trigger right 0',
        '200 hold ended1',
        '210 capture after1',
        '220 point right menu 800 227', '230 trigger right 1', '234 trigger right 0',
        '240 hold measured2',
        '241 eyes play', '242 quads play',
        '250 menu left 1', '340 menu left 0',
        '360 capture over2',
        '370 point right menu 800 227', '380 trigger right 1', '384 trigger right 0',
        '390 hold ended2',
        '400 capture after2',
        '420 exit')
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
        if ($item.Name -like 'bwvr.ini*' -or $item.Name -like 'bwvr*.log' -or $item.Name -eq 'bwvr-hint-counts.ini') { continue }
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
    Set-Content -LiteralPath $ini -Encoding ascii -Value @('[bwvr]', 'resolution_pct=50', "wine_debug=$WineDebug", 'menu_capture=1')
    Adb @('push', $ini, "$GameDir/bwvr.ini")

    # 4. The run.
    Say 'Start the app with the test runtime'
    Adb @('logcat', '-c')
    Adb @('shell', 'am', 'start', '-n', "$Package/android.app.NativeActivity")
    $log = WaitLog @('wine         : Wine is ready', 'FAULT') 900
    Expect ($log -match 'wine         : Wine is ready') 'Wine is ready'
    Expect ($log -match 'game         : play_game=1, play_menu_hold_ms=1000\. Play starts the game in Wine') 'the app can play: Wine is in the APK'
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
        $log = WaitLog @("game         : the first frame of the game \(frame \d+\) came", 'game         : the game ended with the exit code') ($StartMinutes * 60) $playAt
        $part = $log.Substring([math]::Max(0, $playAt))
        Expect ($part -match 'game         : ddraw\.dll of the APK \(\d+ bytes\) is in \S+/drive_c/windows/syswow64') "the ddraw.dll of the APK is in syswow64 of the prefix"
        $reg = [regex]::Match($part, 'game         : reg import of C:\\bwvr\\game\.reg ended with (-?\d+) after ([\d.]+) s: GameDir G:\\, ScreenW (\d+), ScreenH (\d+)')
        Expect ($reg.Success -and $reg.Groups[1].Value -eq '0') ('reg import of the registry of the game: ' + $reg.Value)
        Expect ($part -match "game         : start G:\\runblack\.exe$([regex]::Escape($r.Args)) in Wine") "the app started G:\runblack.exe$($r.Args)"
        $first = [regex]::Match($part, 'game         : the first frame of the game \(frame (\d+)\) came ([\d.]+) s after Play')
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
        Hold "measured$n"
        # The eye pictures (the read back takes a time), then the menu over
        # the game, and Quit game.
        $log = WaitLog @('game         : Quit game') 900 $playAt
        $log = WaitLog @("game         : the game ended with the exit code -?\d+ at [\d:]+ \(Quit game\)") 300 $playAt
        $part = $log.Substring([math]::Max(0, $playAt))
        $overAt = $part.IndexOf('game         : the menu shows over the game')
        $quitAt = $part.IndexOf('game         : Quit game')
        Expect ($overAt -ge 0 -and $quitAt -gt $overAt) "the hold of the menu button showed the menu over game $n before Quit game"
        Expect ($part -match 'game         : Quit game') "the trigger on Quit game in game $n"
        Expect ($part -match 'wine         : Quit game stops \S+/wine') 'Quit game stopped runblack.exe'
        $ended = [regex]::Match($part, 'game         : runblack\.exe ended with (-?\d+) after ([\d.]+) s \(Quit game\)')
        Expect $ended.Success ('runblack.exe ended: ' + $ended.Value)
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
        $results["first$n"] = $first.Groups[2].Value
        # The log of the mod of this game.
        Adb @('pull', "$GameDir/bwvr.log", (Join-Path $Out "bwvr-game$n.log"))
        Adb @('pull', "$AppDir/wine.log", (Join-Path $Out "wine-game$n.log"))
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

    Expect ($report -notmatch '(?m)^fault ') ('the runtime found no fault' + $(if ($report -match '(?m)^fault (.*)$') { ': ' + $Matches[1] } else { '' }))
    Expect ($log -match 'xr           : the GPU of the runtime: .*The renderer of the mod: yes') 'the device of the session has Vulkan 1.3 and the features of the renderer'
    Expect ($log -match 'live         : frames come') 'the reader of the app took frames from the ring'
    $summary = [regex]::Matches($log, 'live         : [^:]+: (\d+) frames played and (\d+) skipped \(frames (\d+) to (\d+)\), (\d+) pictures')
    foreach ($s in $summary) { Note $s.Value }
    Expect ($summary.Count -gt 0 -and [int64]$summary[0].Groups[5].Value -gt 0) 'the app played the frames of the ring and made pictures'
    Expect ($log -match 'xr           : the pictures of the renderer go out in a projection layer') 'the pictures of the game go out in a projection layer'
    $projectionLines = ([regex]::Matches($report, '(?m)^frame \d+ projection ')).Count
    Expect ($projectionLines -gt 0) "the runtime reports the projection layer ($projectionLines lines)"
    foreach ($name in @('new', 'play')) {
        foreach ($eye in 0, 1) {
            $m = [regex]::Match($report, "capture \S*/$name-p$eye\.bmp (\d+)x(\d+) lit (\d+)")
            Expect ($m.Success -and [int64]$m.Groups[3].Value -gt 1000) ("the picture of eye $eye of the projection layer in game $name has the game: " + $(if ($m.Success) { "$($m.Groups[1].Value) by $($m.Groups[2].Value), $($m.Groups[3].Value) lit pixels" } else { 'none' }))
        }
    }
    foreach ($name in @('before', 'over1', 'after1', 'over2', 'after2')) {
        $m = [regex]::Match($report, "capture \S*/$name-\d+\.bmp 1600x1200 lit (\d+)")
        Expect ($m.Success -and [int]$m.Groups[1].Value -gt 100000) "the picture $name of the menu quad has the menu ($($m.Groups[1].Value) lit pixels)"
    }
    # With no menu (the game runs) the runtime gets no quad of 1600 by 1200.
    $quadLines = @([regex]::Matches($report, '(?m)^frame (\d+) quads(.*?)\r?$'))
    $noMenu = @($quadLines | Where-Object { $_.Groups[2].Value -notmatch 'rect 0 0 1600x1200' })
    Expect ($noMenu.Count -gt 0) "the runtime had frames with no menu quad while the game ran ($($noMenu.Count) lines of the quads)"

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
        $lines = @($wineLog -split "`r?`n" | Where-Object { $_ -match $p } | Select-Object -First 6)
        foreach ($l in $lines) { $facts.Add("wine.log [$p]: $l") }
    }
    foreach ($p in @('audio out', 'window', 'focus', 'activ', 'front', 'pause', 'dialog', 'intro skip')) {
        $lines = @($modLog1 -split "`r?`n" | Where-Object { $_ -match $p } | Select-Object -First 6)
        foreach ($l in $lines) { $facts.Add("bwvr.log [$p]: $l") }
    }
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
