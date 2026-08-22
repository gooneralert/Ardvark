# updater.ps1 - Part 1 of 2
# Downloads the latest Ardvark source from GitHub and installs it into src,
# then boots Part 2 (src\updater_part2.ps1) from inside the freshly installed src.
#
# Part 2 does everything else: offset merge into Offsets.h, MSBuild (Release x64),
# and the Ardvark.lnk shortcut. It ships inside the repo's src/ folder.
#
# Parameters (optional, passed through to Part 2):
#   -OutputFile : path to output file (default: src\src\core\roblox\offsets\Offsets.h)
#   -TheosUrl   : URL for theos offsets (default: https://offsets.imtheo.lol/offsets.hpp)
#   -JonahUrl   : explicit Jonah URL (default: auto-built from version)
#   -Version    : explicit version string (default: extracted from theos)

[CmdletBinding()]
param(
    [string]$OutputFile = 'src\src\core\roblox\offsets\Offsets.h',
    [string]$TheosUrl     = 'https://offsets.imtheo.lol/offsets.hpp',
    [string]$JonahUrl     = '',
    [string]$Version      = ''
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$headers   = @{ 'User-Agent' = 'Mozilla/5.0' }

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

# ---- Step 2: download + extract + install the latest src ----
Write-Host '[*] Downloading latest repo from GitHub...'
if (Test-Path $TempZip) { Remove-Item -Path $TempZip -Force }
Invoke-WebRequest -Uri $RepoZipUrl -OutFile $TempZip -UseBasicParsing -Headers $headers

Write-Host '[*] Extracting archive...'
if (Test-Path $TempExtract) { Remove-Item -Path $TempExtract -Recurse -Force }
New-Item -ItemType Directory -Path $TempExtract -Force | Out-Null

# Try tar first (built-in on Win10/11), then fall back to .NET ZipFile
$tarOk = $false
try {
    tar -xf $TempZip -C $TempExtract 2>$null
    if ($LASTEXITCODE -eq 0) { $tarOk = $true }
} catch { $tarOk = $false }
if (-not $tarOk) {
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
$part2 = Join-Path $SrcDir 'updater_part2.ps1'
if (-not (Test-Path $part2)) {
    throw 'updater_part2.ps1 not found inside the installed src.'
}

Write-Host ''
Write-Host '[+] Part 1 complete - src installed.'
Write-Host "[*] Booting Part 2 from installed src: $part2"
Write-Host ''

& $part2 -OutputFile $OutputFile -TheosUrl $TheosUrl -JonahUrl $JonahUrl -Version $Version
