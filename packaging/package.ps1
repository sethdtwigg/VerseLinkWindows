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
$exePath = Join-Path $projDir "x64\$Configuration\VerseLinkWindows.exe"

if (-not $SkipBuild) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install Visual Studio." }
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    $msbuild = Join-Path $vsRoot "MSBuild\Current\Bin\MSBuild.exe"

    Write-Host "== Building $Configuration|$Platform ==" -ForegroundColor Cyan
    & $msbuild (Join-Path $projDir "VerseLinkWindows.vcxproj") /p:Configuration=$Configuration /p:Platform=$Platform /m /v:m /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
}

if (-not (Test-Path $exePath)) { throw "Built exe not found: $exePath" }

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
