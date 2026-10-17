<#
.SYNOPSIS
    Runs the four emulator tests of the APK one after the other
    (docs/android.md, "Test the APK").

.DESCRIPTION
    The order: emu_test.ps1, emu_xr_test.ps1, emu_xr_test.ps1 -Gpu host
    -Stream, emu_replay_test.ps1. Each test writes its whole output to
    out/android/emu-all/<name>.txt. The chain stops at the first test that
    does not end with a line "N checks, 0 failed.", also when the test gives
    no such line at all, and the exit code is then 1.

.PARAMETER Stream
    A stream of the walks (bwvr-stream.bin of a desk run with
    vk_stream_record). It holds pixels of the game, so it stays in local/.

.PARAMETER Reference
    The folder of the desk run that made the stream, with its pictures.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Stream,
    [Parameter(Mandatory = $true)][string]$Reference
)

$ErrorActionPreference = 'Continue'
$Bwvr = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Out = Join-Path $Bwvr 'out\android\emu-all'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Stream = (Resolve-Path -LiteralPath $Stream).Path
$Reference = (Resolve-Path -LiteralPath $Reference).Path

$tests = @(
    @{ Name = 'emu-test'; Script = 'emu_test.ps1'; Args = @() },
    @{ Name = 'emu-xr-test'; Script = 'emu_xr_test.ps1'; Args = @() },
    @{ Name = 'emu-xr-stream-test'; Script = 'emu_xr_test.ps1'; Args = @('-Gpu', 'host', '-Stream', $Stream) },
    @{ Name = 'emu-replay-test'; Script = 'emu_replay_test.ps1'; Args = @('-Stream', $Stream, '-Reference', $Reference) }
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
Write-Host '== All four emulator tests passed' -ForegroundColor Cyan
exit 0
