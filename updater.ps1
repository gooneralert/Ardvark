# updater.ps1
# Downloads two offset files, preserves namespaces + enums, merges them, and writes the
# offsets file the external actually uses: src\src\core\roblox\offsets\Offsets.h
#
# Theos (secondary) is the primary source; Jonah's dumper is used only for missing offsets.
# Version is auto-extracted from theos to build the Jonah URL.

[CmdletBinding()]
param(
    [string]$OutputFile   = 'src\src\core\roblox\offsets\Offsets.h',
    [string]$TheosUrl     = 'https://offsets.imtheo.lol/offsets.hpp',   # primary source
    [string]$JonahUrl     = '',   # auto-built from version, unless explicitly set
    [string]$Version      = ''    # optional explicit version
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Resolve output path relative to this script if not rooted
if (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $OutputFile = Join-Path $scriptDir $OutputFile
}

$headers = @{ 'User-Agent' = 'Mozilla/5.0' }

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
        # Try version.txt in script directory
        $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
        $versionFile = Join-Path $scriptDir 'version.txt'
        if (Test-Path $versionFile) {
            $version = (Get-Content $versionFile -Raw).Trim()
            Write-Host "Using version from $versionFile : $version"
        }
    }
}

# Download theos (secondary) first to extract version if needed
Write-Host "Downloading theos offsets (primary source) from $TheosUrl ..."
$tmpTheos = Join-Path $env:TEMP 'offsets_theos.hpp'
Invoke-WebRequest -Uri $TheosUrl -OutFile $tmpTheos -UseBasicParsing -Headers $headers
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

# ---- Download Jonah (secondary source) ----
Write-Host "Downloading Jonah offsets (secondary source) from $primaryUrl ..."
$tmpJonah = Join-Path $env:TEMP 'offsets_jonah.h'
Invoke-WebRequest -Uri $primaryUrl -OutFile $tmpJonah -UseBasicParsing -Headers $headers
$jonahContent = Get-Content -Path $tmpJonah -Raw

# ---- Parsing functions (unchanged) ----
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

# ----- Merge namespaces: theos first, then jonah (only add missing) -----
$mergedNs       = [ordered]@{}
$mergedEnums    = [ordered]@{}
$mergedOrder    = @()

# Theos first (primary)
foreach ($ns in $theos.NamespaceOrder) {
    if (-not $mergedNs.Contains($ns)) {
        $mergedNs[$ns]      = [ordered]@{}
        $mergedEnums[$ns]   = [ordered]@{}
        $mergedOrder += $ns
    }
    foreach ($off in $theos.Namespaces[$ns].GetEnumerator()) {
        $mergedNs[$ns][$off.Key] = $off.Value
    }
    foreach ($enum in $theos.NamespaceEnums[$ns].GetEnumerator()) {
        $mergedEnums[$ns][$enum.Key] = $enum.Value
    }
}

# Jonah second (only add if not already present)
foreach ($ns in $jonah.NamespaceOrder) {
    if (-not $mergedNs.Contains($ns)) {
        $mergedNs[$ns]      = [ordered]@{}
        $mergedEnums[$ns]   = [ordered]@{}
        $mergedOrder += $ns
    }
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

# ----- Merge global offsets & enums: theos first -----
$mergedGlobal      = [ordered]@{}
$mergedGlobalEnums = [ordered]@{}
foreach ($off in $theos.Global.GetEnumerator())   { $mergedGlobal[$off.Key] = $off.Value }
foreach ($off in $jonah.Global.GetEnumerator())   { if (-not $mergedGlobal.Contains($off.Key)) { $mergedGlobal[$off.Key] = $off.Value } }

foreach ($enum in $theos.GlobalEnums.GetEnumerator())   { $mergedGlobalEnums[$enum.Key] = $enum.Value }
foreach ($enum in $jonah.GlobalEnums.GetEnumerator())   { if (-not $mergedGlobalEnums.Contains($enum.Key)) { $mergedGlobalEnums[$enum.Key] = $enum.Value } }

Write-Host "Merged offsets from $($mergedOrder.Count) namespaces (theos primary)."

# ----- Generate output -----
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