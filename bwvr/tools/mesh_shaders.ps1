<#
.SYNOPSIS
    Makes src\mod\vk_mesh_vert.h from src\mod\vk_mesh.vert with the glslc of
    the Android NDK.

.DESCRIPTION
    The header has one SPIR-V array for each variant of the shader:
    kVkMeshVert (no define), kVkMeshVertOffsets (-DOFFSETS) and
    kVkMeshVertGroups (-DGROUPS), and the same three with -DLEGACY (the linear depth of
    gpu_mesh_depth=0). The first
    line of the header names the FNV-1a hash of the source (64 bit, over the
    text with LF line ends), and a check of the math tests reads it, so a
    change of the source with no new header fails the build.

      .\bwvr\tools\mesh_shaders.ps1
#>
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'android\android_env.ps1')
$glslc = Join-Path $AndroidNdk 'shader-tools\windows-x86_64\glslc.exe'
if (-not (Test-Path -LiteralPath $glslc)) { throw "There is no glslc at $glslc" }
$mod = Join-Path (Split-Path -Parent $PSScriptRoot) 'src\mod'
$source = Join-Path $mod 'vk_mesh.vert'
$header = Join-Path $mod 'vk_mesh_vert.h'

# The hash is over the text with LF line ends, so a checkout with CRLF has
# the same value.
Add-Type -TypeDefinition @'
public static class MeshFnv {
  public static string Of(byte[] data) {
    ulong h = 0xcbf29ce484222325UL;
    foreach (byte b in data) { h ^= b; unchecked { h *= 0x100000001b3UL; } }
    return h.ToString("x16");
  }
}
'@
$text = [IO.File]::ReadAllText($source).Replace("`r`n", "`n")
$sha = [MeshFnv]::Of([Text.Encoding]::UTF8.GetBytes($text))

function Compile([string[]]$Defines) {
    $out = & $glslc -mfmt=c --target-env=vulkan1.3 -fshader-stage=vertex @Defines $source -o - 2>&1
    if ($LASTEXITCODE -ne 0) { throw "glslc failed: $out" }
    return ($out -join "`n")
}

$plain = Compile @()
$offsets = Compile @('-DOFFSETS')
$groups = Compile @('-DGROUPS')
$legacy = Compile @('-DLEGACY')
$legacyOffsets = Compile @('-DLEGACY', '-DOFFSETS')
$legacyGroups = Compile @('-DLEGACY', '-DGROUPS')
$sb = New-Object Text.StringBuilder
[void]$sb.Append("// glslc (shaderc ndk-r27c): vk_mesh.vert and vk_mesh.vert with -DOFFSETS and -DGROUPS. source-fnv64 $sha`n")
[void]$sb.Append("#pragma once`n")
[void]$sb.Append("const uint32_t kVkMeshVert[] = $plain`n;`n")
[void]$sb.Append("const uint32_t kVkMeshVertOffsets[] = $offsets`n;`n")
[void]$sb.Append("const uint32_t kVkMeshVertGroups[] = $groups`n;`n")
[void]$sb.Append("const uint32_t kVkMeshVertLegacy[] = $legacy`n;`n")
[void]$sb.Append("const uint32_t kVkMeshVertLegacyOffsets[] = $legacyOffsets`n;`n")
[void]$sb.Append("const uint32_t kVkMeshVertLegacyGroups[] = $legacyGroups`n;`n")
[IO.File]::WriteAllText($header, $sb.ToString().Replace("`n", "`r`n"), (New-Object Text.UTF8Encoding($false)))
"vk_mesh_vert.h made from vk_mesh.vert (fnv64 $sha)"
