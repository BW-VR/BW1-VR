<#
.SYNOPSIS
    Tests Wine in the headset app in the Android emulator (docs/android.md,
    "Test the APK", and "Wine in the app").

.DESCRIPTION
    The emulator is x86_64, so the app unpacks the x86_64 Wine tree of the
    APK, and the 32 bit code runs natively through wow64cpu.dll of Wine. So
    the test covers the whole path of the app except the CPU emulator of the
    headsets (wowbox64.dll). The test:

      1. Starts the AVD bwvr_test with no window (port 5584, as
         emu_test.ps1), and installs bwvr/out/android/bwvr-quest.apk on a
         clean app folder. The APK must have a Wine tree for x86_64.
      2. The first start: the app makes its folders, unpacks the tree into
         its internal folder and starts wineboot for the prefix. The test
         checks the targetSdkVersion, the SELinux domain of the app
         (untrusted_app_27: Android lets it run the programs of its own
         folder) and the unpack, and ends the app with the back key while
         wineboot runs: the end of the app stops wineboot and runs
         wineserver -k, and no process of Wine stays. As root the test
         checks that the stopped prefix has no marker and has a system.reg
         (it puts an empty one there when wineboot made none).
      3. The start after the stop: no unpack, and a new prefix, because the
         stopped prefix has no marker. The test waits for "Wine is ready"
         in the log, and checks the steps, the time, and that the menu came
         up before the setup ended.
      4. Puts bwvr.ini with wine_probe=1 and wine_debug=warn+virtual,
         bwvr_wine_probe.exe of the PC build (.\dev.ps1 build) and a stand
         in for runblack.exe into the game folder on the shared storage.
      5. The next start: no unpack and no new prefix. The test program
         runs from the drive G: (the game folder on the shared storage, a
         file system with noexec), and the test checks each value of its
         result file: the Windows version, the version of Wine, a 32 bit
         process on x86_64, the CPU DLL of the registry, the null display
         driver, the loop time, the read and the write of the drive, no
         DirectDraw, Direct3D, OpenGL or Vulkan of Wine, and a TrueType font
         (Tahoma) with glyphs.
      6. Checks that no process of Wine is left after the test program,
         and ends the app with the back key (wineserver -k).
      7. A start with wine_setup=0 in bwvr.ini: the log line, no setup, no
         wineserver -k at the end, no wine.log and no process of Wine. Then
         the test checks logcat (no crash).

    The results go to bwvr/out/android/emu-wine-test.

.PARAMETER Keep
    Leaves the emulator on at the end.

.PARAMETER SetupMinutes
    The time that the first start may take for the setup. 15 by default.
#>
[CmdletBinding()]
param(
    [switch]$Keep,
    [int]$SetupMinutes = 15
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
Lock-AndroidEmulator
# The tools write notes to stderr. Each check here reads their output, so a
# line on stderr must not stop the script.
$ErrorActionPreference = 'Continue'

$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Apk = Join-Path $Bwvr 'out\android\bwvr-quest.apk'
$Probe = Join-Path $Bwvr 'build\test\wine_probe\Release\bwvr_wine_probe.exe'
$Out = Join-Path $Bwvr 'out\android\emu-wine-test'
$Avd = 'bwvr_test'
$Image = 'system-images;android-34;google_apis;x86_64'
$Package = 'org.bwvr.quest'
$AppDir = "/sdcard/Android/data/$Package/files"

if (-not (Test-Path -LiteralPath $Apk)) { Stop-Android "There is no APK at:`n$Apk`nRun build_apk.ps1 first." }
if (-not (Test-Path -LiteralPath $Probe)) { Stop-Android "There is no test program at:`n$Probe`nRun .\dev.ps1 build first." }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($Apk)
try { $hasWine = @($zip.Entries | Where-Object { $_.FullName -eq 'assets/wine/x86_64.tar' }).Count -eq 1 } finally { $zip.Dispose() }
if (-not $hasWine) { Stop-Android "The APK has no Wine tree for x86_64. Run pack_wine.sh x86_64, then build_apk.ps1." }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$faults = New-Object System.Collections.Generic.List[string]
$checks = 0
function Expect([bool]$Ok, [string]$What) {
    $script:checks++
    if ($Ok) { Write-Host ('   ok    ' + $What) }
    else { Write-Host ('   FAULT ' + $What) -ForegroundColor Red; $script:faults.Add($What) }
}
function Say([string]$Text) { Write-Host ('== ' + $Text) -ForegroundColor Cyan }
function AdbOut([string[]]$Arguments) { return (& $Adb -s $script:Serial @Arguments 2>&1 | Out-String) }
function Adb([string[]]$Arguments) { & $Adb -s $script:Serial @Arguments 2>&1 | Out-Null }
function Shot([string]$Name) { cmd /c "`"$Adb`" -s $script:Serial exec-out screencap -p > `"$Out\$Name`"" }
# Waits until the log of the app has one of the patterns. Gives the log.
function WaitLog([string[]]$Patterns, [int]$Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        Start-Sleep -Seconds 3
        $log = LastStart (AdbOut @('shell', 'cat', "$AppDir/bwvr-quest.log"))
        foreach ($p in $Patterns) { if ($log -match $p) { return $log } }
        $alive = (AdbOut @('shell', 'pidof', $Package)).Trim()
    } while ((Get-Date) -lt $deadline -and $alive -match '^\d+$')
    return $log
}
# The part of the log of the last start. After the back key Android keeps
# the process, android_main starts again in it, and the log file stays
# open, so the log holds each start of that process.
function LastStart([string]$Log) {
    $at = $Log.LastIndexOf(', the headset app')
    if ($at -lt 0) { return $Log }
    $line = $Log.LastIndexOf("`n", $at)
    return $Log.Substring($line + 1)
}
# Ends the app with the back key on the main page and waits for its last
# log line. When the app does not end in 30 s, the test says so and stops
# it with force-stop. Gives the log and if the app ended itself.
function EndWithBack {
    Adb @('shell', 'input', 'keyevent', 'KEYCODE_BACK')
    $log = WaitLog @('app          : the app ends') 30
    $ended = $log -match 'app          : the app ends'
    if (-not $ended) {
        Write-Host '   NOTE: the back key did not end the app in 30 s. The test stops it with force-stop' -ForegroundColor Yellow
        Adb @('shell', 'am', 'force-stop', $Package)
        $log = LastStart (AdbOut @('shell', 'cat', "$AppDir/bwvr-quest.log"))
    }
    $script:BackEnded += @($ended)
    return @{ Log = $log; Ended = $ended }
}
$BackEnded = @()
# The processes of the user of the app, with no process of the app itself.
function WineProcesses([string]$User) {
    $ps = AdbOut @('shell', 'ps', '-A', '-o', 'USER,PID,NAME')
    return @($ps -split "`r?`n" | Where-Object { $_ -match "^$User\s" -and $_ -notmatch [regex]::Escape($Package) })
}
function ProbeValue([string]$Text, [string]$Key) {
    $m = [regex]::Match($Text, "(?m)^$Key=(.*?)\r?$")
    if ($m.Success) { return $m.Groups[1].Value }
    return $null
}

# 1. The emulator.
$avdManager = Join-Path $AndroidSdk 'cmdline-tools\latest\bin\avdmanager.bat'
$avds = & $Emulator -list-avds 2>$null
if (-not ($avds -contains $Avd)) {
    Say "Make the AVD $Avd"
    'no' | & $avdManager create avd -n $Avd -k $Image -d 'pixel_tablet' --force | Out-Null
    if ($LASTEXITCODE -ne 0) { Stop-Android "avdmanager could not make the AVD $Avd. Install `"$Image`" with sdkmanager." }
}
& $Adb start-server 2>&1 | Out-Null
$port = 5584
$Serial = "emulator-$port"
$started = $false
$running = & $Adb devices 2>$null | Select-String -Pattern "^$Serial\s+device"
if ($running) {
    Say "The emulator $Serial runs already"
} else {
    Say 'Start the emulator (no window)'
    Start-Process -FilePath $Emulator -ArgumentList @('-avd', $Avd, '-port', "$port",
        '-no-window', '-no-audio', '-no-boot-anim', '-no-snapshot',
        '-gpu', 'swiftshader_indirect') -WindowStyle Hidden | Out-Null
    $started = $true
}
$deadline = (Get-Date).AddMinutes(6)
do {
    Start-Sleep -Seconds 3
    $booted = (& $Adb -s $Serial shell getprop sys.boot_completed 2>$null | Out-String).Trim()
} while ($booted -ne '1' -and (Get-Date) -lt $deadline)
if ($booted -ne '1') { Stop-Android 'The emulator did not start in 6 minutes.' }
do {
    Start-Sleep -Seconds 2
    $pm = (& $Adb -s $Serial shell pm path android 2>$null | Out-String)
} while ($pm -notmatch 'package:' -and (Get-Date) -lt $deadline)
Write-Host ('   ' + (AdbOut @('shell', 'getprop', 'ro.build.fingerprint')).Trim())

try {
    Say 'Install the APK on a clean app folder'
    Adb @('shell', 'am', 'force-stop', $Package)
    Adb @('uninstall', $Package)
    Adb @('shell', 'rm', '-rf', "/sdcard/Android/data/$Package")
    $install = AdbOut @('install', $Apk)
    Expect ($install -match 'Success') ('adb install: ' + ($install.Trim() -split "`n")[-1])
    $dump = AdbOut @('shell', 'dumpsys', 'package', $Package)
    Expect ($dump -match 'targetSdk=28') 'the installed app has targetSdk=28'
    $uid = [regex]::Match($dump, '(?:userId|appId)=(\d+)').Groups[1].Value
    $user = if ($uid) { 'u0_a' + ([int]$uid - 10000) } else { 'none' }
    $minTarget = (AdbOut @('shell', 'getprop', 'ro.build.version.min_supported_target_sdk')).Trim()
    Write-Host "   the user of the app is $user. ro.build.version.min_supported_target_sdk is '$minTarget'"
    $mounts = AdbOut @('shell', 'mount')
    $sdcard = @($mounts -split "`r?`n" | Where-Object { $_ -match ' /storage/emulated ' }) | Select-Object -First 1
    Write-Host "   $sdcard"
    Expect ($sdcard -match 'noexec') 'the shared storage of the game folder has noexec'

    # 2. The first start: the setup.
    Say 'The first start: unpack and make the prefix'
    Adb @('logcat', '-c')
    $t0 = Get-Date
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    Start-Sleep -Seconds 4
    Shot 'start1-setup.png'
    $domain = AdbOut @('shell', 'ps', '-A', '-Z', '-o', 'LABEL,NAME')
    $label = @($domain -split "`r?`n" | Where-Object { $_ -match [regex]::Escape($Package) }) | Select-Object -First 1
    Write-Host "   $label"
    Expect ($label -match 'untrusted_app_27') 'the app runs in the domain untrusted_app_27 (targetSdkVersion 28)'
    $focus = AdbOut @('shell', 'dumpsys', 'window', 'displays')
    $current = [regex]::Match($focus, 'mCurrentFocus=([^\r\n]*)').Groups[1].Value
    Write-Host "   mCurrentFocus=$current"
    Expect ($current -match 'NativeActivity') 'the app has the focus: no dialog of the system over it'
    # The app ends while wineboot runs: the back key on the main page gives
    # the key back to the system, which finishes the activity, and the end
    # of android_main calls WineRuntime::Stop.
    $log0 = WaitLog @('wine         : make the prefix', 'FAULT') ($SetupMinutes * 60)
    Start-Sleep -Seconds 4
    $end0 = EndWithBack
    $log0 = $end0.Log
    Set-Content -LiteralPath (Join-Path $Out 'bwvr-quest-start1.log') -Value $log0 -Encoding utf8
    Expect ($log0 -match 'wine         : the APK has the Wine tree [0-9a-f]{16} for x86_64') 'the app found the Wine tree of x86_64 in the APK'
    Expect ($log0 -match 'is not there\. Unpack the tree of the APK') 'the first start unpacks the tree'
    $unpack = [regex]::Match($log0, 'unpacked (\d+) files \(([\d.]+) MB\) and (\d+) folders in ([\d.]+) s')
    Expect $unpack.Success ('the unpack ended: ' + $unpack.Value)
    Expect ($log0 -match 'wine         : make the prefix' -and $log0 -notmatch 'the prefix took' -and $log0 -notmatch 'Wine is ready') 'the app stopped while wineboot ran'
    if ($end0.Ended) {
        Expect ($log0 -match 'wine         : the app ends, so it stops \S+/wine') 'the end of the app stopped the wineboot that the thread waited for'
        Expect ($log0 -match 'wineserver -k ended with 0 ') ('the end of the app ran wineserver -k and stopped the server of wineboot: ' + [regex]::Match($log0, 'wineserver -k ended with \S+').Value)
        Expect ($log0 -match 'app          : the app ends') 'the app ended (android_main returned)'
        Start-Sleep -Seconds 5
        $left = @(WineProcesses $user)
        Expect ($left.Count -eq 0) ('no process of Wine is left after the end of the app' + $(if ($left.Count) { ': ' + ($left -join '; ') } else { '' }))
    }

    # The prefix after the stop, as root: it has no marker. When wineboot
    # made no system.reg yet, the test puts an empty one there, so the next
    # start meets a prefix with system.reg and no marker.
    Adb @('root')
    Start-Sleep -Seconds 3
    Adb @('wait-for-device')
    $prefixDir = "/data/data/$Package/files/prefix"
    $prefixList = AdbOut @('shell', 'ls', '-la', $prefixDir)
    Set-Content -LiteralPath (Join-Path $Out 'prefix-after-stop.txt') -Value $prefixList -Encoding utf8
    $hadReg = $prefixList -match '\ssystem\.reg(\r?\n|$)'
    Write-Host ("   the stopped prefix " + $(if ($hadReg) { 'has' } else { 'has no' }) + ' system.reg of wineboot')
    Expect ($prefixList -match 'user\.reg' -and $prefixList -notmatch 'bwvr-prefix-id\.txt') 'the stopped prefix has no marker'
    if (-not $hadReg) {
        Adb @('shell', "touch $prefixDir/system.reg && chown ${user}:${user} $prefixDir/system.reg")
    }
    $prefixList = AdbOut @('shell', 'ls', '-la', $prefixDir)
    Expect ($prefixList -match '\ssystem\.reg(\r?\n|$)') 'the prefix with no marker has a system.reg before the next start'
    Adb @('unroot')
    Start-Sleep -Seconds 3
    Adb @('wait-for-device')
    do { Start-Sleep -Seconds 2; $pm = (AdbOut @('shell', 'pm', 'path', 'android')) } while ($pm -notmatch 'package:')

    # The start after the stop: the tree is whole, and the prefix has no
    # marker, so the app makes it again.
    Say 'The start after the stop: a new prefix'
    $t0 = Get-Date
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    $log1 = WaitLog @('wine         : Wine is ready', 'FAULT') ($SetupMinutes * 60)
    $setupSeconds = [math]::Round(((Get-Date) - $t0).TotalSeconds)
    Shot 'start1-ready.png'
    Set-Content -LiteralPath (Join-Path $Out 'bwvr-quest-start1b.log') -Value $log1 -Encoding utf8
    Write-Host "   the prefix of this start took about $setupSeconds s (the test reads the log each 3 s)"
    Expect ($log1 -match 'is the tree of the APK\. No unpack') 'the start after the stop does not unpack: the tree is whole'
    Expect ($log1 -match 'wine         : make the prefix') 'the start after the stop makes a new prefix: the stopped prefix has no marker'
    $boot = [regex]::Match($log1, 'wineboot --init ended with (-?\d+) after ([\d.]+) s')
    Expect ($boot.Success -and $boot.Groups[1].Value -eq '0') ('wineboot --init: ' + $boot.Value)
    $wait = [regex]::Match($log1, 'wineserver -w ended with (-?\d+) after ([\d.]+) s')
    Expect ($wait.Success -and $wait.Groups[1].Value -eq '0') ('wineserver -w: ' + $wait.Value)
    $took = [regex]::Match($log1, 'the prefix took ([\d.]+) s')
    Expect $took.Success ('the log has the time of the prefix: ' + $took.Value)
    Expect ($log1 -match 'the drive G: is the game folder /storage/emulated/0/Android/data/org\.bwvr\.quest/files/game') 'the drive G: is the game folder'
    Expect ($log1 -match 'wine         : Wine is ready' -and $log1 -notmatch 'FAULT') 'Wine is ready, with no fault'
    $menuAt = $log1.IndexOf('canvas       : a picture of 1600 by 1200')
    $readyAt = $log1.IndexOf('wine         : Wine is ready')
    Expect ($menuAt -ge 0 -and $readyAt -gt $menuAt) 'the menu started before the setup ended: the setup does not hold the app'
    Start-Sleep -Seconds 8
    $left = @(WineProcesses $user)
    Expect ($left.Count -eq 0) ('no process of Wine is left after the setup' + $(if ($left.Count) { ': ' + ($left -join '; ') } else { '' }))

    # 3. The test program and the ini.
    Say 'Put the test program and bwvr.ini into the game folder'
    Adb @('shell', 'am', 'force-stop', $Package)
    $ini = Join-Path $Out 'bwvr.ini.pushed'
    Set-Content -LiteralPath $ini -Encoding ascii -Value @('[bwvr]', 'wine_probe=1', 'wine_debug=warn+virtual', 'menu_capture=1')
    Adb @('push', $ini, "$AppDir/game/bwvr.ini")
    Adb @('push', $Probe, "$AppDir/game/bwvr_wine_probe.exe")
    $standIn = Join-Path $Out 'stand-in.exe'
    [IO.File]::WriteAllBytes($standIn, (New-Object byte[] 4096))
    Adb @('push', $standIn, "$AppDir/game/runblack.exe")
    Adb @('shell', 'rm', '-f', "$AppDir/wine.log")

    # 4. The second start: no new setup, the test program.
    Say 'The second start: the test program'
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    $log2 = WaitLog @('wine         : wine_probe=1\. bwvr_wine_probe\.exe ended', 'FAULT', 'the game folder has no bwvr_wine_probe') 600
    Start-Sleep -Seconds 2
    Shot 'start2-probe.png'
    Set-Content -LiteralPath (Join-Path $Out 'bwvr-quest-start2.log') -Value $log2 -Encoding utf8
    Expect ($log2 -match 'is the tree of the APK\. No unpack') 'the second start does not unpack'
    Expect ($log2 -match 'wine         : the prefix .* is there' -and $log2 -notmatch 'make the prefix') 'the second start makes no new prefix'
    $ran = [regex]::Match($log2, 'bwvr_wine_probe\.exe ended with (-?\d+) after ([\d.]+) s')
    Expect ($ran.Success -and $ran.Groups[1].Value -eq '0') ('the test program: ' + $ran.Value)
    Adb @('pull', "$AppDir/game/bwvr-wine-probe.txt", (Join-Path $Out 'bwvr-wine-probe.txt'))
    Adb @('pull', "$AppDir/wine.log", (Join-Path $Out 'wine.log'))
    Adb @('pull', "$AppDir/game/bwvr-menu.bmp", (Join-Path $Out 'bwvr-menu.bmp'))
    $r = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'bwvr-wine-probe.txt')) { $r = Get-Content -LiteralPath (Join-Path $Out 'bwvr-wine-probe.txt') -Raw }
    Write-Host ($r -replace "`r", '' -split "`n" | ForEach-Object { '         ' + $_ } | Out-String).TrimEnd()
    Expect ((ProbeValue $r 'program') -eq 'G:\bwvr_wine_probe.exe') 'the test program ran from the drive G:'
    Expect ((ProbeValue $r 'windows') -match '^10\.0\.\d+$') ('the Windows version is 10.0: ' + (ProbeValue $r 'windows'))
    Expect ((ProbeValue $r 'wine') -eq '11.0') 'the version of Wine is 11.0'
    Expect ((ProbeValue $r 'process') -eq 'i386' -and (ProbeValue $r 'native') -eq 'x86_64') 'IsWow64Process2: a process for i386 on x86_64'
    Expect ((ProbeValue $r 'cpu_dll') -eq 'wow64cpu.dll') 'HKLM\Software\Microsoft\Wow64\x86 is wow64cpu.dll (the line of pack_wine.sh in wine.inf)'
    Expect ((ProbeValue $r 'graphics') -eq 'null') 'HKCU\Software\Wine\Drivers, Graphics is null'
    $loop = 0.0
    Expect ([double]::TryParse((ProbeValue $r 'loop_ms'), [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$loop) -and $loop -gt 0) "the loop took $loop ms"
    Expect ((ProbeValue $r 'drive') -eq 'G:' -and (ProbeValue $r 'drive_write') -eq '1' -and (ProbeValue $r 'drive_read') -eq '1' -and (ProbeValue $r 'drive_delete') -eq '1') 'the program wrote, read back and deleted a file of 64 KB on G:'
    foreach ($dll in @('ddraw', 'd3d9', 'wined3d', 'dxgi', 'opengl32', 'vulkan-1')) {
        Expect ((ProbeValue $r "system_$dll") -match '^none ') ("Wine has no $dll.dll in the system folder: " + (ProbeValue $r "system_$dll"))
    }
    Expect ((ProbeValue $r 'font_face') -eq 'Tahoma' -and (ProbeValue $r 'font_truetype') -eq '1') 'the font Tahoma is a TrueType font'
    $extent = [regex]::Match([string](ProbeValue $r 'font_extent'), '^(\d+)x(\d+)$')
    Expect ($extent.Success -and [int]$extent.Groups[1].Value -gt 0 -and [int]$extent.Groups[2].Value -gt 0) ('GetTextExtentPoint32 of Tahoma: ' + (ProbeValue $r 'font_extent'))
    $glyph = 0
    Expect ([int]::TryParse([string](ProbeValue $r 'font_glyph_bytes'), [ref]$glyph) -and $glyph -gt 0) "GetGlyphOutline of 'A' in Tahoma: $glyph bytes"
    Expect ((ProbeValue $r 'result') -eq 'ok') 'the result file is whole'
    $wineLog = ''
    if (Test-Path -LiteralPath (Join-Path $Out 'wine.log')) { $wineLog = Get-Content -LiteralPath (Join-Path $Out 'wine.log') -Raw }
    # The texts of dlls/ntdll/unix/virtual.c: with warn+virtual map_pe_header
    # says "noexec file system, falling back to read" or "file system doesn't
    # support mmap, falling back to read" when the mmap of a PE file fails,
    # and map_image_into_view says "failed to set ... protection on ...
    # section ..., noexec filesystem?" (an err line) when a section gets no
    # protection.
    $noexec = ([regex]::Matches($wineLog, 'falling back to read')).Count
    $protect = ([regex]::Matches($wineLog, 'protection on .* section .*, noexec filesystem\?')).Count
    Write-Host "   wine.log: $noexec lines 'falling back to read', $protect lines 'failed to set ... protection ... noexec filesystem?'"
    Expect ($protect -eq 0) 'each section of the PE file of the shared storage got its protection'
    Start-Sleep -Seconds 8
    $left = @(WineProcesses $user)
    Expect ($left.Count -eq 0) ('no process of Wine is left after the test program' + $(if ($left.Count) { ': ' + ($left -join '; ') } else { '' }))

    # The end of the app after the test program: wineserver -k with no
    # program of Wine that runs.
    Say 'The end of the app with the back key'
    $end2 = EndWithBack
    Set-Content -LiteralPath (Join-Path $Out 'bwvr-quest-start2-end.log') -Value $end2.Log -Encoding utf8
    if ($end2.Ended) {
        # wineserver -k ends with 0 when it stopped a server, and with 1 when
        # no server ran (server/main.c, kill_lock_owner). The server of the
        # test program ends some seconds after the program.
        $kill = [regex]::Match($end2.Log, 'wineserver -k ended with (-?\d+) ')
        Expect ($kill.Success -and ($kill.Groups[1].Value -eq '0' -or $kill.Groups[1].Value -eq '1')) ('the end of the app ran wineserver -k: ' + $kill.Value + $(if ($kill.Groups[1].Value -eq '1') { '(no server ran)' } else { '' }))
        Expect ($end2.Log -match 'app          : the app ends') 'the app ended (android_main returned)'
        Start-Sleep -Seconds 5
        $left = @(WineProcesses $user)
        Expect ($left.Count -eq 0) ('no process of Wine is left after the end of the app' + $(if ($left.Count) { ': ' + ($left -join '; ') } else { '' }))
    }
    # 5. wine_setup=0: the app touches no part of Wine. No thread, no
    # wineserver -k at the end, and no wine.log.
    Say 'A start with wine_setup=0'
    Adb @('shell', 'am', 'force-stop', $Package)
    $iniOff = Join-Path $Out 'bwvr.ini.off'
    Set-Content -LiteralPath $iniOff -Encoding ascii -Value @('[bwvr]', 'wine_setup=0', 'menu_capture=1')
    Adb @('push', $iniOff, "$AppDir/game/bwvr.ini")
    Adb @('shell', 'rm', '-f', "$AppDir/wine.log")
    Adb @('shell', 'am', 'start', '-W', '-n', "$Package/android.app.NativeActivity")
    $log3 = WaitLog @('canvas       : a picture of 1600 by 1200') 120
    Start-Sleep -Seconds 4
    $end3 = EndWithBack
    $log3 = $end3.Log
    Set-Content -LiteralPath (Join-Path $Out 'bwvr-quest-start3.log') -Value $log3 -Encoding utf8
    Expect ($log3 -match 'wine         : wine_setup=0\. The app does not set up Wine') 'wine_setup=0: the log says that the app does not set up Wine'
    Expect ($log3 -notmatch 'wine         : the APK has the Wine tree' -and $log3 -notmatch 'wine         : Wine is ready') 'wine_setup=0: no setup thread'
    if ($end3.Ended) {
        Expect ($log3 -match 'app          : the app ends' -and $log3 -notmatch 'wineserver -k') 'wine_setup=0: the end of the app runs no wineserver -k'
    }
    $wineLogThere = (AdbOut @('shell', 'ls', "$AppDir/wine.log")).Trim()
    Expect ($wineLogThere -match 'No such file') ("wine_setup=0: no wine.log in the folder of the app ($wineLogThere)")
    Start-Sleep -Seconds 3
    $left = @(WineProcesses $user)
    Expect ($left.Count -eq 0) ('wine_setup=0: no process of Wine' + $(if ($left.Count) { ': ' + ($left -join '; ') } else { '' }))

    Write-Host ('   the back key ended the app ' + @($BackEnded | Where-Object { $_ }).Count + ' of ' + $BackEnded.Count + ' times')

    $logcat = AdbOut @('logcat', '-d', '-v', 'brief')
    Set-Content -LiteralPath (Join-Path $Out 'logcat.txt') -Value $logcat -Encoding utf8
    Expect ($logcat -notmatch 'FATAL EXCEPTION' -and $logcat -notmatch 'Fatal signal') 'logcat has no crash'
} catch {
    Expect $false ('the script ran to its end: ' + $_.Exception.Message +
        ' (line ' + $_.InvocationInfo.ScriptLineNumber + ')')
} finally {
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
