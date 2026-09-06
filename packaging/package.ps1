# VerseLink Windows release packaging
# Builds (optional) and bundles a distributable zip:
#   exe + default config + icon + README + KJV Bible (public domain)
#
# NOTE: Only the KJV is shipped - NASB/ESV and other modern translations are
# copyrighted and must be supplied by the end user into Bibles\.

param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$projDir = Join-Path $repoRoot "VerseLinkWindows"

# MSBuild's default output directory depends on the platform: x64 builds land in
# <project>\x64\<config>\ while Win32 builds land in <project>\<config>\ with no
# platform folder. Hardcoding the x64 path meant "-Platform Win32" either threw
# or, worse, packaged a stale x64 binary under a Win32 name.
function Resolve-BuiltExe {
    $candidates = @(
        (Join-Path $projDir "$Platform\$Configuration\VerseLinkWindows.exe"),
        (Join-Path $projDir "$Configuration\VerseLinkWindows.exe"),
        (Join-Path $repoRoot "$Platform\$Configuration\VerseLinkWindows.exe")
    )
    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

$buildStart = Get-Date

if (-not $SkipBuild) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install Visual Studio." }
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    $msbuild = Join-Path $vsRoot "MSBuild\Current\Bin\MSBuild.exe"

    Write-Host "== Building $Configuration|$Platform ==" -ForegroundColor Cyan
    & $msbuild (Join-Path $projDir "VerseLinkWindows.vcxproj") /p:Configuration=$Configuration /p:Platform=$Platform /m /v:m /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
}

$exePath = Resolve-BuiltExe
if (-not $exePath) {
    throw "Built exe not found for $Configuration|$Platform under $projDir. Build it first, or drop -SkipBuild."
}

# Guard against shipping a leftover binary from a different platform or an
# earlier run: if we just built, the exe must be newer than the build.
if (-not $SkipBuild -and (Get-Item $exePath).LastWriteTime -lt $buildStart) {
    throw "Found a stale exe at $exePath (older than this build). Clean the output directory and retry."
}

Write-Host "== Packaging $exePath ==" -ForegroundColor Cyan

# Named by version rather than by date so a zip pairs unambiguously with the
# installer and the release it belongs to.
$versionHeader = Join-Path $projDir "Version.h"
$match = Select-String -Path $versionHeader -Pattern '#define\s+VERSELINK_VERSION_STRING\s+"([^"]+)"'
if (-not $match) { throw "VERSELINK_VERSION_STRING not found in $versionHeader" }
$version = $match.Matches[0].Groups[1].Value

$distName = "VerseLink-$version-$Platform-portable"
$stage = Join-Path $PSScriptRoot "$distName"
$zip = Join-Path $repoRoot "dist\$distName.zip"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage\Bibles -Force | Out-Null

Copy-Item $exePath $stage
Copy-Item (Join-Path $projDir "VerseLinkIcon.ico") $stage -ErrorAction SilentlyContinue
Copy-Item (Join-Path $repoRoot "README.md") $stage
Copy-Item (Join-Path $repoRoot "Bibles\KJV.xml") $stage\Bibles
Copy-Item (Join-Path $PSScriptRoot "DISTRIBUTION-NOTES.txt") $stage

# config.json is deliberately not shipped. Settings live in
# %APPDATA%\VerseLink\config.json and are created with defaults on first run;
# a config.json next to the exe would be picked up by the legacy-config
# migration and could overwrite what an upgrading user actually had.

New-Item -ItemType Directory -Path (Split-Path $zip) -Force | Out-Null
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Remove-Item $stage -Recurse -Force

Write-Host "== Packaged: $zip ==" -ForegroundColor Green
Get-Item $zip | Select-Object Name, @{n="SizeMB";e={[Math]::Round($_.Length/1MB,2)}}
