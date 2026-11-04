<#
.SYNOPSIS
    Makes src\mod\vk_compose_vert.h and src\mod\vk_compose_frag.h from
    src\mod\vk_compose.vert and src\mod\vk_compose.frag with the glslc of the
    Android NDK.

.DESCRIPTION
    The first line of each header names the FNV-1a hash of the source (64 bit,
    over the text with LF line ends), and a check of the math tests reads it,
    so a change of a source with no new header fails the build.

      .\bwvr\tools\compose_shaders.ps1
#>
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'android\android_env.ps1')
$glslc = Join-Path $AndroidNdk 'shader-tools\windows-x86_64\glslc.exe'
if (-not (Test-Path -LiteralPath $glslc)) { throw "There is no glslc at $glslc" }
$mod = Join-Path (Split-Path -Parent $PSScriptRoot) 'src\mod'

Add-Type -TypeDefinition @'
public static class ComposeFnv {
  public static string Of(byte[] data) {
    ulong h = 0xcbf29ce484222325UL;
    foreach (byte b in data) { h ^= b; unchecked { h *= 0x100000001b3UL; } }
    return h.ToString("x16");
  }
}
'@

function Make([string]$Name, [string]$Stage, [string]$Array) {
    $source = Join-Path $mod ($Name + '.' + $Stage)
    $header = Join-Path $mod ($Name + '_' + $Stage.Substring(0, 4) + '.h')
    $text = [IO.File]::ReadAllText($source).Replace("`r`n", "`n")
    $sha = [ComposeFnv]::Of([Text.Encoding]::UTF8.GetBytes($text))
    $stageName = if ($Stage -eq 'vert') { 'vertex' } else { 'fragment' }
    $out = & $glslc -mfmt=c --target-env=vulkan1.3 "-fshader-stage=$stageName" $source -o - 2>&1
    if ($LASTEXITCODE -ne 0) { throw "glslc failed: $out" }
    $sb = New-Object Text.StringBuilder
    [void]$sb.Append("// glslc (shaderc ndk-r27c): $Name.$Stage. source-fnv64 $sha`n")
    [void]$sb.Append("#pragma once`n")
    [void]$sb.Append("const uint32_t $Array[] = $(($out -join "`n"))`n;`n")
    [IO.File]::WriteAllText($header, $sb.ToString().Replace("`n", "`r`n"), (New-Object Text.UTF8Encoding($false)))
    "${Name}_$Stage.h made from $Name.$Stage (fnv64 $sha)"
}

Make 'vk_compose' 'vert' 'kVkComposeVert'
Make 'vk_compose' 'frag' 'kVkComposeFrag'
