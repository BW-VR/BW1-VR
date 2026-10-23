<#
.SYNOPSIS
    Runs the six emulator tests of the APK one after the other
    (docs/android.md, "Test the APK").

.DESCRIPTION
    The order: emu_test.ps1, emu_wine_test.ps1, emu_xr_test.ps1, emu_xr_test.ps1 -Gpu host
    -Stream, emu_replay_test.ps1, emu_live_test.ps1. Each test writes its whole output to
    out/android/emu-all/<name>.txt. The chain stops at the first test that
    does not end with a line "N checks, 0 failed.", also when the test gives
    no such line at all, and the exit code is then 1.

.PARAMETER Stream
    A stream of the walks (bwvr-stream.bin of a desk run with
    vk_stream_record). It holds pixels of the game, so it stays in local/.

.PARAMETER Reference
    The folder of the desk run that made the stream, with its pictures.

.PARAMETER Live
    The folder of a desk run of live_stream_run.ps1: ring-capture.bin and the
    pictures of the PC (docs/android.md, "The live stream"). It holds pixels
    of the game, so it stays in the folder of the desk run.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Stream,
    [Parameter(Mandatory = $true)][string]$Reference,
    [Parameter(Mandatory = $true)][string]$Live
)

. (Join-Path $PSScriptRoot 'android_env.ps1')
$ErrorActionPreference = 'Continue'
# adb starts its server at its first call, and the server keeps the handles
# that it gets from the process that starts it. A server that a test of the
# chain starts keeps the output pipe of the chain open, and the chain then
# waits for that pipe with no end. So the chain starts the server first,
# through the shell of Windows, and the server gets no handle of the chain.
# The chain waits for the client of adb only: -Wait of Start-Process would
# also wait for the server, which runs on.
$client = Start-Process -FilePath $Adb -ArgumentList 'start-server' -WindowStyle Hidden -PassThru
$client.WaitForExit()
$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Out = Join-Path $Bwvr 'out\android\emu-all'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Stream = (Resolve-Path -LiteralPath $Stream).Path
$Reference = (Resolve-Path -LiteralPath $Reference).Path
$Live = (Resolve-Path -LiteralPath $Live).Path

$tests = @(
    @{ Name = 'emu-test'; Script = 'emu_test.ps1'; Args = @() },
    @{ Name = 'emu-wine-test'; Script = 'emu_wine_test.ps1'; Args = @() },
    @{ Name = 'emu-xr-test'; Script = 'emu_xr_test.ps1'; Args = @() },
    @{ Name = 'emu-xr-stream-test'; Script = 'emu_xr_test.ps1'; Args = @('-Gpu', 'host', '-Stream', $Stream) },
    @{ Name = 'emu-replay-test'; Script = 'emu_replay_test.ps1'; Args = @('-Stream', $Stream, '-Reference', $Reference) },
    @{ Name = 'emu-live-test'; Script = 'emu_live_test.ps1'; Args = @('-Run', $Live) }
)

foreach ($t in $tests) {
    Write-Host ('== ' + $t.Name) -ForegroundColor Cyan
    $file = Join-Path $Out ($t.Name + '.txt')
    $script = Join-Path $PSScriptRoot $t.Script
    $lines = @(& powershell -NoProfile -ExecutionPolicy Bypass -File $script @($t.Args) 2>&1 |
        ForEach-Object { "$_" })
    $lines | Set-Content -LiteralPath $file -Encoding utf8
    $verdict = @($lines | Where-Object { $_ -match '== \d+ checks, \d+ failed\.' }) | Select-Object -Last 1
    if (-not $verdict) {
        Write-Host "   NO VERDICT: the test gave no line of checks. The output is in $file"
        exit 1
    }
    Write-Host ('   ' + $verdict.Trim())
    if ($verdict -notmatch '== \d+ checks, 0 failed\.') {
        Write-Host "   The chain stops. The output is in $file"
        exit 1
    }
}
Write-Host '== All six emulator tests passed' -ForegroundColor Cyan
exit 0
