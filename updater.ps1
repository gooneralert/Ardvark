# updater.ps1 - Part 1 of 2
# Downloads the latest Ardvark source from GitHub and installs it into src,
# then boots Part 2 (src\updater_part2.ps1) from inside the freshly installed src.
#
# Part 2 does everything else: offset merge into Offsets.h, MSBuild (Release x64),
# and the Ardvark.lnk shortcut. It ships inside the repo's src/ folder.
# Downloads in both parts use curl.exe.
#
# Parameters (optional, passed through to Part 2):
#   -OutputFile : path to output file (default: src\src\core\roblox\offsets\Offsets.h)
#   -TheosUrl   : URL for theos offsets (default: https://offsets.imtheo.lol/offsets.hpp)
#   -JonahUrl   : explicit Jonah URL (default: auto-built from version)
#                        a local file path is also accepted (e.g. "C:\offsets\jonah.h")
#   -UseLocalJonah : switch - read the local jonah_offsets.h inside src instead of
#                        downloading Jonah (forwarded to Part 2)
#   -Version    : explicit version string (default: extracted from theos)

[CmdletBinding()]
param(
    [string]$OutputFile = 'src\src\core\roblox\offsets\Offsets.h',
    [string]$TheosUrl     = 'https://offsets.imtheo.lol/offsets.hpp',
    [string]$JonahUrl     = '',
    [switch]$UseLocalJonah,
    [string]$Version      = ''
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$RepoZipUrl  = 'https://github.com/gooneralert/Ardvark/archive/refs/heads/main.zip'
$TempZip     = Join-Path $env:TEMP 'ardvark_src_update.zip'
$TempExtract = Join-Path $env:TEMP 'ardvark_src_update'
$SrcDir      = Join-Path $scriptDir 'src'

# ---- Step 1: delete the existing src folder ----
Write-Host '[*] Deleting existing src folder...'
if (Test-Path $SrcDir) {
    Remove-Item -Path $SrcDir -Recurse -Force
    if (Test-Path $SrcDir) { throw 'Failed to delete the src folder.' }
}
Write-Host '[+] src deleted.'

# ---- Step 2: download + extract + install the latest src (via curl.exe) ----
Write-Host '[*] Downloading latest repo from GitHub (using curl.exe)...'
if (Test-Path $TempZip) { Remove-Item -Path $TempZip -Force }
curl.exe -L --fail --silent --show-error -A "Mozilla/5.0" -o $TempZip $RepoZipUrl
if ($LASTEXITCODE -ne 0) { throw "Failed to download the repo (curl exit code $LASTEXITCODE)." }

Write-Host '[*] Extracting archive...'
if (Test-Path $TempExtract) { Remove-Item -Path $TempExtract -Recurse -Force }
New-Item -ItemType Directory -Path $TempExtract -Force | Out-Null

# Try tar first (built-in on Win10/11), then fall back to .NET ZipFile
try {
    tar -xf $TempZip -C $TempExtract 2>$null
    if ($LASTEXITCODE -ne 0) { throw "tar failed exit $LASTEXITCODE" }
} catch {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($TempZip, $TempExtract)
}
if (-not (Test-Path $TempExtract)) { throw 'Extraction failed.' }

# The zip extracts under a top-level "<Repo>-<branch>" folder; find its src.
$extractedSrc = Join-Path $TempExtract 'Ardvark-main\src'
if (-not (Test-Path $extractedSrc)) { $extractedSrc = Join-Path $TempExtract 'Ardvark-master\src' }
if (-not (Test-Path $extractedSrc)) {
    throw 'Could not find the src folder inside the downloaded archive.'
}

Write-Host '[*] Copying src folder into place...'
Copy-Item -Path $extractedSrc -Destination $SrcDir -Recurse -Force

Write-Host '[*] Cleaning up temporary files...'
Remove-Item -Path $TempExtract -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path $TempZip -Force -ErrorAction SilentlyContinue
Write-Host '[+] src updated.'

# ---- Step 3: boot Part 2 from inside the freshly installed src ----
# Resolve the output path to an absolute path first, so Part 2 (which lives
# inside src) does not double-prefix its own script directory.
if (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    $OutputFile = Join-Path $scriptDir $OutputFile
}

$part2 = Join-Path $SrcDir 'updater_part2.ps1'
if (-not (Test-Path $part2)) {
    throw 'updater_part2.ps1 not found inside the installed src.'
}

Write-Host ''
Write-Host '[+] Part 1 complete - src installed.'
Write-Host "[*] Booting Part 2 from installed src: $part2"
Write-Host ''

& $part2 -OutputFile $OutputFile -TheosUrl $TheosUrl -JonahUrl $JonahUrl -UseLocalJonah:$UseLocalJonah -Version $Version

