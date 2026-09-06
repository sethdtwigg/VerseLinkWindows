# Builds VerseLink and compiles the Inno Setup installer into dist\.
#
# The version comes from VerseLinkWindows\Version.h and is passed to the .iss,
# so the installer, the exe's VERSIONINFO and the About dialog can never
# disagree about which build this is.

param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    # Fail unless Version.h matches this (e.g. "1.1.0"). The release workflow
    # passes the tag so a mistagged release cannot ship.
    [string]$ExpectVersion
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$projDir = Join-Path $repoRoot "VerseLinkWindows"

function Get-VerseLinkVersion {
    $versionHeader = Join-Path $projDir "Version.h"
    if (-not (Test-Path $versionHeader)) { throw "Version.h not found at $versionHeader" }

    $match = Select-String -Path $versionHeader -Pattern '#define\s+VERSELINK_VERSION_STRING\s+"([^"]+)"'
    if (-not $match) { throw "VERSELINK_VERSION_STRING not found in $versionHeader" }
    return $match.Matches[0].Groups[1].Value
}

function Find-InnoCompiler {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )
    $found = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($found) { return $found }

    $onPath = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    throw "Inno Setup 6 not found. Install it (winget install JRSoftware.InnoSetup) or add ISCC.exe to PATH."
}

$version = Get-VerseLinkVersion
Write-Host "== VerseLink $version ==" -ForegroundColor Cyan

if ($ExpectVersion) {
    $expected = $ExpectVersion.TrimStart('v')
    if ($version -ne $expected) {
        throw "Version mismatch: Version.h says $version but $expected was expected. Bump Version.h, or tag the version that is actually in the tree."
    }
    Write-Host "   version matches the expected $expected" -ForegroundColor DarkGray
}

if (-not $SkipBuild) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install Visual Studio." }
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    $msbuild = Join-Path $vsRoot "MSBuild\Current\Bin\MSBuild.exe"

    Write-Host "== Building $Configuration|x64 ==" -ForegroundColor Cyan
    & $msbuild (Join-Path $projDir "VerseLinkWindows.vcxproj") /p:Configuration=$Configuration /p:Platform=x64 /m /v:m /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
}

$exePath = Join-Path $projDir "x64\$Configuration\VerseLinkWindows.exe"
if (-not (Test-Path $exePath)) { throw "Built exe not found: $exePath" }

# The exe's own resource must agree with Version.h, or the installer would
# advertise a version the installed file does not report.
$exeVersion = (Get-Item $exePath).VersionInfo.FileVersion
if ($exeVersion -and ($exeVersion.Trim() -ne $version)) {
    throw "Built exe reports version '$exeVersion' but Version.h says '$version'. Rebuild so the resource is current."
}

# Refuse to package a build that cannot pass its own checks.
Write-Host "== Self test ==" -ForegroundColor Cyan
Push-Location $repoRoot
try {
    & $exePath --selftest
    if ($LASTEXITCODE -ne 0) { throw "Self test failed; refusing to package this build." }
} finally {
    Pop-Location
}

$iscc = Find-InnoCompiler
Write-Host "== Compiling installer with $iscc ==" -ForegroundColor Cyan

$distDir = Join-Path $repoRoot "dist"
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

& $iscc "/DAppVersion=$version" "/DSourceDir=$repoRoot" (Join-Path $PSScriptRoot "VerseLink.iss")
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }

$installer = Join-Path $distDir "VerseLink-$version-Setup.exe"
if (-not (Test-Path $installer)) { throw "Installer not produced at $installer" }

Write-Host "== Installer ready ==" -ForegroundColor Green
Get-Item $installer | Select-Object Name, @{n = "SizeMB"; e = { [Math]::Round($_.Length / 1MB, 2) } }
