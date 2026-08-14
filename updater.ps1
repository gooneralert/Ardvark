# updater.ps1
# Downloads two offset files, preserves namespaces + enums, merges them, and writes offsets.h

[CmdletBinding()]
param(
    [string]$OutputFile   = 'offsets.h',
    [string]$PrimaryUrl   = 'https://dumper.jonah.cool/offsets.h',
    [string]$SecondaryUrl = 'https://offsets.imtheo.lol/offsets.hpp'
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Resolve output path relative to this script if not rooted
if (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $OutputFile = Join-Path $scriptDir $OutputFile
}

$headers = @{ 'User-Agent' = 'Mozilla/5.0' }

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

Write-Host 'Downloading primary offsets...'
$tmpPrimary = Join-Path $env:TEMP 'offsets_primary.h'
Invoke-WebRequest -Uri $PrimaryUrl -OutFile $tmpPrimary -UseBasicParsing -Headers $headers

Write-Host 'Downloading secondary offsets...'
$tmpSecondary = Join-Path $env:TEMP 'offsets_secondary.hpp'
Invoke-WebRequest -Uri $SecondaryUrl -OutFile $tmpSecondary -UseBasicParsing -Headers $headers

$primaryContent   = Get-Content -Path $tmpPrimary -Raw
$secondaryContent = Get-Content -Path $tmpSecondary -Raw

$primary   = Parse-OffsetsFile -Content $primaryContent
$secondary = Parse-OffsetsFile -Content $secondaryContent

# ----- Merge namespaces -----
$mergedNs       = [ordered]@{}
$mergedEnums    = [ordered]@{}
$mergedOrder    = @()

# Primary first
foreach ($ns in $primary.NamespaceOrder) {
    if (-not $mergedNs.Contains($ns)) {
        $mergedNs[$ns]      = [ordered]@{}
        $mergedEnums[$ns]   = [ordered]@{}
        $mergedOrder += $ns
    }
    foreach ($off in $primary.Namespaces[$ns].GetEnumerator()) {
        $mergedNs[$ns][$off.Key] = $off.Value
    }
    foreach ($enum in $primary.NamespaceEnums[$ns].GetEnumerator()) {
        $mergedEnums[$ns][$enum.Key] = $enum.Value
    }
}

# Secondary second (only add if not already present)
foreach ($ns in $secondary.NamespaceOrder) {
    if (-not $mergedNs.Contains($ns)) {
        $mergedNs[$ns]      = [ordered]@{}
        $mergedEnums[$ns]   = [ordered]@{}
        $mergedOrder += $ns
    }
    foreach ($off in $secondary.Namespaces[$ns].GetEnumerator()) {
        if (-not $mergedNs[$ns].Contains($off.Key)) {
            $mergedNs[$ns][$off.Key] = $off.Value
        }
    }
    foreach ($enum in $secondary.NamespaceEnums[$ns].GetEnumerator()) {
        if (-not $mergedEnums[$ns].Contains($enum.Key)) {
            $mergedEnums[$ns][$enum.Key] = $enum.Value
        }
    }
}

# ----- Merge global offsets & enums -----
$mergedGlobal      = [ordered]@{}
$mergedGlobalEnums = [ordered]@{}
foreach ($off in $primary.Global.GetEnumerator())   { $mergedGlobal[$off.Key] = $off.Value }
foreach ($off in $secondary.Global.GetEnumerator()) { if (-not $mergedGlobal.Contains($off.Key)) { $mergedGlobal[$off.Key] = $off.Value } }

foreach ($enum in $primary.GlobalEnums.GetEnumerator())   { $mergedGlobalEnums[$enum.Key] = $enum.Value }
foreach ($enum in $secondary.GlobalEnums.GetEnumerator()) { if (-not $mergedGlobalEnums.Contains($enum.Key)) { $mergedGlobalEnums[$enum.Key] = $enum.Value } }

Write-Host "Merged offsets from $($mergedOrder.Count) namespaces."

# ----- Generate output -----
$lines = @(
    '#pragma once'
    '#include <cstdint>'
    ''
    '// geeg offsets'
    '// Auto-generated by updater.ps1'
    "// Sources: $PrimaryUrl"
    "//         $SecondaryUrl"
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

Remove-Item $tmpPrimary, $tmpSecondary -ErrorAction SilentlyContinue

Write-Host "Wrote $OutputFile"