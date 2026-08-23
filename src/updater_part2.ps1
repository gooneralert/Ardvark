# updater_part2.ps1 - Part 2 of 2 (lives inside src/)
# Everything after the source install:
#   Step 3 - Downloads the two offset files, merges them, and writes
#            src\src\core\roblox\offsets\Offsets.h
#   Step 4 - Builds the external with MSBuild (Release x64)
#   Step 5 - Creates the Ardvark.lnk shortcut one folder above src
#
# Booted automatically by updater.ps1 (part 1) from the freshly downloaded
# src, or run it directly from src. Offset URLs and version.txt belong to the
# repo root (one folder up from here).
#
# Theos (offsets.imtheo.lol) is the primary source.
# Jonah (dumper.jonah.cool) fills in missing offsets.
# Version is auto-extracted from theos to build the matching Jonah URL.
# Duplicates: if an offset name already exists in the same namespace from theos,
# the Jonah version is skipped (primary wins).
#
# Parameters (optional):
#   -OutputFile : path to output file (default: src\core\roblox\offsets\Offsets.h
#                 relative to this script, i.e. inside this src folder)
#   -TheosUrl   : URL for theos offsets (default: https://offsets.imtheo.lol/offsets.hpp)
#   -JonahUrl   : explicit Jonah URL (default: auto-built from version)
#                 a local file path is also accepted (e.g. C:\offsets\jonah.h)
#   -Version    : explicit version string (default: extracted from theos)

[CmdletBinding()]
param(
    [string]$OutputFile   = 'src\core\roblox\offsets\Offsets.h',
    [string]$TheosUrl     = 'https://offsets.imtheo.lol/version-ce0bcd0fbd484804/offsets.hpp',
    [string]$JonahUrl     = '',
    [string]$Version      = ''
)

try {
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Resolve output path relative to this script if not rooted
if (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $OutputFile = Join-Path $scriptDir $OutputFile
}

# ---- Helper: extract Roblox version from theos file content ----
function Get-VersionFromTheosContent {
    param([string]$Content)
    if ($Content -match 'Roblox Version\s*:\s*([\w-]+)') {
        return $matches[1]
    }
    return $null
}

# ---- Determine version for Jonah URL ----
$version = $Version
if (-not $version -and -not $JonahUrl) {
    # Try environment variable
    $envVer = [Environment]::GetEnvironmentVariable('THEOS_VERSION')
    if ($envVer) {
        $version = $envVer
        Write-Host "Using version from environment variable THEOS_VERSION: $version"
    }
    else {
        # Try version.txt in script directory, then one folder up (repo root)
        $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
        $versionFile = Join-Path $scriptDir 'version.txt'
        if (-not (Test-Path $versionFile)) {
            $versionFile = Join-Path (Split-Path -Parent $scriptDir) 'version.txt'
        }
        if (Test-Path $versionFile) {
            $version = (Get-Content $versionFile -Raw).Trim()
            Write-Host "Using version from $versionFile : $version"
        }
    }
}

# Download theos first to extract version if needed
Write-Host "Downloading theos offsets from $TheosUrl ..."
$tmpTheos = Join-Path $env:TEMP 'offsets_theos.hpp'
curl.exe -L --fail --silent --show-error -A "Mozilla/5.0" -o $tmpTheos $TheosUrl
if ($LASTEXITCODE -ne 0) { throw "Failed to download theos offsets (curl exit code $LASTEXITCODE)." }
$theosContent = Get-Content -Path $tmpTheos -Raw

# If we still don't have a version and JonahUrl is not explicitly set, extract from theos
if (-not $JonahUrl -and -not $version) {
    $version = Get-VersionFromTheosContent -Content $theosContent
    if ($version) {
        Write-Host "Extracted version from theos file: $version"
    } else {
        Write-Host "Could not extract version from theos file, will use default Jonah URL."
    }
}

# Build Jonah URL
if ($JonahUrl) {
    Write-Host "Using explicit Jonah URL: $JonahUrl"
    $primaryUrl = $JonahUrl
} elseif ($version) {
    $primaryUrl = "https://dumper.jonah.cool/$version/offsets.h"
    Write-Host "Using version-based Jonah URL: $primaryUrl"
} else {
    $primaryUrl = 'https://dumper.jonah.cool/offsets.h'
    Write-Host "No version found, using default Jonah URL: $primaryUrl"
}

# ---- Acquire Jonah (secondary source) - local file path or download ----
# $primaryUrl may be a URL or a local file path. If it points to an existing
# file, read it directly instead of downloading ("exact same, just a file").
if ($primaryUrl -match '^[a-zA-Z]+://') {
    Write-Host "Downloading Jonah offsets (secondary source) from $primaryUrl ..."
    $tmpJonah = Join-Path $env:TEMP 'offsets_jonah.h'
    curl.exe -L --fail --silent --show-error -A "Mozilla/5.0" -o $tmpJonah $primaryUrl
    if ($LASTEXITCODE -ne 0) { throw "Failed to download Jonah offsets (curl exit code $LASTEXITCODE)." }
    $jonahContent = Get-Content -Path $tmpJonah -Raw
} else {
    # Local file: try as-is, then relative to this script's directory.
    $jonahFile = $primaryUrl
    if (-not (Test-Path -LiteralPath $jonahFile -PathType Leaf)) {
        $candidate = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) $primaryUrl
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $jonahFile = $candidate
        }
    }
    if (-not (Test-Path -LiteralPath $jonahFile -PathType Leaf)) {
        throw "Jonah file not found: $primaryUrl"
    }
    Write-Host "Using local Jonah file: $jonahFile"
    $jonahContent = Get-Content -Path $jonahFile -Raw
}

# ---- Parsing function (works on any content) ----
function Parse-OffsetsFile {
    param([string]$Content)

    $namespaceOffsets = [ordered]@{}
    $namespaceEnums   = [ordered]@{}
    $namespaceOrder   = @()
    $globalOffsets    = [ordered]@{}
    $globalEnums      = [ordered]@{}

    # Capture top-level namespace blocks: namespace Name { ... }
    $nsRegex = [regex]'(?ms)^[ \t]*namespace\s+([A-Za-z_]\w*)\s*\{[^\r\n]*\r?\n(?<body>.*?)^[ \t]*\}[^\S\r\n]*(?://[^\r\n]*)?[ \t]*(?:\r?\n|$)'
    $matches = $nsRegex.Matches($Content)

    foreach ($m in $matches) {
        $nsName = $m.Groups[1].Value
        $body   = $m.Groups['body'].Value

        if (-not $namespaceOffsets.Contains($nsName)) {
            $namespaceOffsets[$nsName] = [ordered]@{}
            $namespaceEnums[$nsName]   = [ordered]@{}
            $namespaceOrder += $nsName
        }

        # --- Offset definitions (constexpr) ---
        $offsetRegex = [regex]'(?m)^[ \t]*(?:(?:inline|static)\s+)?constexpr\s+(?<type>(?:std::|::std::)?(?:ptrdiff_t|int64_t|uint64_t|uintptr_t|DWORD64|int32_t|uint32_t|auto))\s+(?<name>\w+)\s*=\s*(?<value>0x[0-9a-fA-F]+|\d+)\s*;'
        foreach ($om in $offsetRegex.Matches($body)) {
            $name  = $om.Groups['name'].Value
            $value = $om.Groups['value'].Value
            $type  = $om.Groups['type'].Value
            if (-not $namespaceOffsets[$nsName].Contains($name)) {
                $namespaceOffsets[$nsName][$name] = @{ Value = $value; Type = $type }
            }
        }

        # --- Enum definitions (enum class / enum) ---
        $enumRegex = [regex]'(?ms)^[ \t]*enum\s+(?:class\s+)?(?<name>\w+)\s*(?::\s*(?<type>\w+)\s*)?\{(?<body>.*?)\};'
        foreach ($em in $enumRegex.Matches($body)) {
            $enumName = $em.Groups['name'].Value
            $enumBlock = $em.Value
            if (-not $namespaceEnums[$nsName].Contains($enumName)) {
                $namespaceEnums[$nsName][$enumName] = $enumBlock
            }
        }
    }

    # Process remaining content (outside namespaces) for global offsets and enums
    $remaining = $nsRegex.Replace($Content, '')

    # Global #define offsets
    $defineRegex = [regex]'(?m)^[ \t]*#define\s+(?<name>\w+)\s+(?<value>0x[0-9a-fA-F]+|\d+)\b'
    foreach ($dm in $defineRegex.Matches($remaining)) {
        $name  = $dm.Groups['name'].Value
        $value = $dm.Groups['value'].Value
        if (-not $globalOffsets.Contains($name)) {
            $globalOffsets[$name] = @{ Value = $value; Type = '#define' }
        }
    }

    # Global constexpr offsets
    $globalConstRegex = [regex]'(?m)^[ \t]*(?:(?:inline|static)\s+)?constexpr\s+(?<type>(?:std::|::std::)?(?:ptrdiff_t|int64_t|uint64_t|uintptr_t|DWORD64|int32_t|uint32_t|auto))\s+(?<name>\w+)\s*=\s*(?<value>0x[0-9a-fA-F]+|\d+)\s*;'
    foreach ($om in $globalConstRegex.Matches($remaining)) {
        $name  = $om.Groups['name'].Value
        $value = $om.Groups['value'].Value
        $type  = $om.Groups['type'].Value
        if (-not $globalOffsets.Contains($name)) {
            $globalOffsets[$name] = @{ Value = $value; Type = $type }
        }
    }

    # Global enums
    $globalEnumRegex = [regex]'(?ms)^[ \t]*enum\s+(?:class\s+)?(?<name>\w+)\s*(?::\s*(?<type>\w+)\s*)?\{(?<body>.*?)\};'
    foreach ($em in $globalEnumRegex.Matches($remaining)) {
        $enumName = $em.Groups['name'].Value
        $enumBlock = $em.Value
        if (-not $globalEnums.Contains($enumName)) {
            $globalEnums[$enumName] = $enumBlock
        }
    }

    return @{
        Namespaces      = $namespaceOffsets
        NamespaceEnums  = $namespaceEnums
        NamespaceOrder  = $namespaceOrder
        Global          = $globalOffsets
        GlobalEnums     = $globalEnums
    }
}

# ---- Parse both files ----
$theos   = Parse-OffsetsFile -Content $theosContent
$jonah   = Parse-OffsetsFile -Content $jonahContent

# ----- Merge: theos (primary) then jonah (secondary, missing only) -----
$mergedNs       = [ordered]@{}
$mergedEnums    = [ordered]@{}
$mergedOrder    = @()

# 1. Copy all theos namespaces (offsets + enums)
foreach ($ns in $theos.NamespaceOrder) {
    $mergedNs[$ns]    = [ordered]@{}
    $mergedEnums[$ns] = [ordered]@{}
    foreach ($off in $theos.Namespaces[$ns].GetEnumerator()) {
        $mergedNs[$ns][$off.Key] = $off.Value
    }
    foreach ($enum in $theos.NamespaceEnums[$ns].GetEnumerator()) {
        $mergedEnums[$ns][$enum.Key] = $enum.Value
    }
    $mergedOrder += $ns
}

# 2. Copy all theos globals
$mergedGlobal      = [ordered]@{}
$mergedGlobalEnums = [ordered]@{}
foreach ($off in $theos.Global.GetEnumerator())   { $mergedGlobal[$off.Key] = $off.Value }
foreach ($enum in $theos.GlobalEnums.GetEnumerator()) { $mergedGlobalEnums[$enum.Key] = $enum.Value }

# 3. Add jonah namespaces/offsets/enums only if missing
foreach ($ns in $jonah.NamespaceOrder) {
    if (-not $mergedNs.Contains($ns)) {
        # New namespace: add everything
        $mergedNs[$ns]    = [ordered]@{}
        $mergedEnums[$ns] = [ordered]@{}
        $mergedOrder += $ns
        foreach ($off in $jonah.Namespaces[$ns].GetEnumerator()) {
            $mergedNs[$ns][$off.Key] = $off.Value
        }
        foreach ($enum in $jonah.NamespaceEnums[$ns].GetEnumerator()) {
            $mergedEnums[$ns][$enum.Key] = $enum.Value
        }
    } else {
        # Existing namespace: add only offsets/enums whose name is not already taken
        foreach ($off in $jonah.Namespaces[$ns].GetEnumerator()) {
            if (-not $mergedNs[$ns].Contains($off.Key)) {
                $mergedNs[$ns][$off.Key] = $off.Value
            }
        }
        foreach ($enum in $jonah.NamespaceEnums[$ns].GetEnumerator()) {
            if (-not $mergedEnums[$ns].Contains($enum.Key)) {
                $mergedEnums[$ns][$enum.Key] = $enum.Value
            }
        }
    }
}

# 4. Add jonah globals only if missing by name
foreach ($off in $jonah.Global.GetEnumerator()) {
    if (-not $mergedGlobal.Contains($off.Key)) {
        $mergedGlobal[$off.Key] = $off.Value
    }
}
foreach ($enum in $jonah.GlobalEnums.GetEnumerator()) {
    if (-not $mergedGlobalEnums.Contains($enum.Key)) {
        $mergedGlobalEnums[$enum.Key] = $enum.Value
    }
}

Write-Host "Merged offsets from $($mergedOrder.Count) namespaces (theos primary)."

# ----- Generate output file -----
$lines = @(
    '#pragma once'
    '#include <cstdint>'
    ''
    '// geeg offsets'
    '// Auto-generated by updater.ps1'
    "// Primary source (theos): $TheosUrl"
    "// Secondary source (Jonah): $primaryUrl"
    ''
)

# Global offsets first
if ($mergedGlobal.Count -gt 0) {
    foreach ($off in $mergedGlobal.GetEnumerator()) {
        $type  = $off.Value.Type
        $value = $off.Value.Value
        if ($type -eq '#define') {
            $lines += "#define $($off.Key) $value"
        } else {
            $lines += "constexpr $type $($off.Key) = $value;"
        }
    }
    $lines += ''
}

# Global enums
if ($mergedGlobalEnums.Count -gt 0) {
    foreach ($enum in $mergedGlobalEnums.Values) {
        $lines += ($enum -split "`r?`n")
        $lines += ''
    }
}

# Namespace blocks
foreach ($ns in $mergedOrder) {
    $lines += "namespace $ns {"

    # Offsets inside namespace
    foreach ($off in $mergedNs[$ns].GetEnumerator()) {
        $type  = $off.Value.Type
        $value = $off.Value.Value
        $lines += "    constexpr $type $($off.Key) = $value;"
    }

    # Enums inside namespace
    if ($mergedEnums[$ns].Count -gt 0) {
        if ($mergedNs[$ns].Count -gt 0) {
            $lines += ''   # blank line between offsets and enums
        }
        foreach ($enum in $mergedEnums[$ns].Values) {
            $lines += ($enum -split "`r?`n")
            $lines += ''   # blank line after each enum (optional)
        }
    }

    $lines += "}"
    $lines += ''
}

Set-Content -Path $OutputFile -Value $lines -Encoding Ascii

Remove-Item $tmpTheos, $tmpJonah -ErrorAction SilentlyContinue

Write-Host "Wrote $OutputFile"
Write-Host ''

# ==============================================================
#  Step 4 - Build the external with MSBuild (Release x64)
# ==============================================================
Write-Host '[*] Locating Visual Studio / MSBuild...'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = $null
if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vsInstall) {
    throw 'Could not find Visual Studio with the C++ workload. Install "Desktop development with C++" and try again.'
}

Write-Host '[*] Setting up the MSBuild / VC environment...'
$vsDevCmd = Join-Path $vsInstall 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path $vsDevCmd)) {
    throw "VsDevCmd.bat not found: $vsDevCmd"
}

# Run VsDevCmd in a cmd session and import its environment variables.
$envOutput = & cmd.exe /d /c "`"$vsDevCmd`" -arch=x64 >nul 2>&1 && set"
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to set up the Visual Studio build environment.'
}
foreach ($line in $envOutput) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}

Write-Host '[*] Building external (MSBuild Release x64)...'
& msbuild (Join-Path $scriptDir 'jewsploit.sln') /m /p:Configuration=Release /p:Platform=x64 /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Build failed (exit code $LASTEXITCODE)."
}
Write-Host '[+] Build complete.'
Write-Host ''

# ==============================================================
#  Step 5 - Create a shortcut one folder above src
# ==============================================================
$targetExe = Join-Path $scriptDir 'x64\Release\jewsploit.exe'
if (-not (Test-Path $targetExe)) {
    throw "Built executable not found: $targetExe"
}

Write-Host '[*] Creating shortcut one folder above src...'
$shortcutPath = Join-Path (Split-Path -Parent $scriptDir) 'Ardvark.lnk'
$wsShell = New-Object -ComObject WScript.Shell
$sc = $wsShell.CreateShortcut($shortcutPath)
$sc.TargetPath = $targetExe
$sc.WorkingDirectory = Join-Path $scriptDir 'x64\Release'
$sc.Save()

Write-Host ''
Write-Host '[+] Done.'
Write-Host "    Shortcut : $shortcutPath"
Write-Host "    Target   : $targetExe"
Write-Host ''

exit 0

} catch {
    Write-Host ''
    Write-Host "[!] Error: $($_.Exception.Message)"
    Read-Host 'Press Enter to exit...'
    exit 1
}
