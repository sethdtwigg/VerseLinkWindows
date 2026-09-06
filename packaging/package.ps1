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

$stamp = Get-Date -Format "yyyyMMdd"
$distName = "VerseLinkWindows-$stamp-$Platform"
$stage = Join-Path $PSScriptRoot "$distName"
$zip = Join-Path $repoRoot "dist\$distName.zip"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage\Bibles -Force | Out-Null

Copy-Item $exePath $stage
Copy-Item (Join-Path $repoRoot "config.json") $stage
Copy-Item (Join-Path $projDir "VerseLinkIcon.ico") $stage -ErrorAction SilentlyContinue
Copy-Item (Join-Path $repoRoot "README.md") $stage
Copy-Item (Join-Path $repoRoot "Bibles\KJV.xml") $stage\Bibles

@'
VerseLink Windows - distribution notes
======================================

This package includes the King James Version (public domain).

Other translations such as the NASB or ESV are copyrighted. To use them,
obtain the text legally and place the XML file(s) into the Bibles folder
next to VerseLinkWindows.exe, then pick the version in the tray icon's
Settings dialog.
'@ | Set-Content (Join-Path $stage "DISTRIBUTION-NOTES.txt")

New-Item -ItemType Directory -Path (Split-Path $zip) -Force | Out-Null
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Remove-Item $stage -Recurse -Force

Write-Host "== Packaged: $zip ==" -ForegroundColor Green
Get-Item $zip | Select-Object Name, @{n="SizeMB";e={[Math]::Round($_.Length/1MB,2)}}
