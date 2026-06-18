# build.ps1 - AtiNAMatiC-Boost Windows build script
# Usage:  .\build.ps1             (Release)
#         .\build.ps1 -Config Debug

param(
    [string]$Config = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root     = $PSScriptRoot
$buildDir = Join-Path $root "build"

# ------------------------------------------------------------------
# Wipe stale Mac/Unix build directory if it exists (wrong generator)
# ------------------------------------------------------------------
if (Test-Path (Join-Path $buildDir "Makefile"))
{
    Write-Host "Detected Unix Makefile build -- removing stale build/ directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# ------------------------------------------------------------------
# Configure
# ------------------------------------------------------------------
Write-Host "Configuring AtiNAMatiC-Boost ($Config)..." -ForegroundColor Cyan

cmake -S $root -B $buildDir `
      -G "Visual Studio 17 2022" -A x64

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

# ------------------------------------------------------------------
# Build
# ------------------------------------------------------------------
Write-Host "Building..." -ForegroundColor Cyan

cmake --build $buildDir --config $Config

if ($LASTEXITCODE -ne 0) { throw "Build failed." }

# ------------------------------------------------------------------
# Install VST3 to system folder
# ------------------------------------------------------------------
$artefacts = Join-Path $buildDir "AtiNAMatiCBoost_artefacts\$Config"
$vst3Src   = Join-Path $artefacts "VST3\AtiNAMatiC-Boost.vst3"
$vst3Dst   = "C:\Program Files\Common Files\VST3\AtiNAMatiC-Boost.vst3"

if (Test-Path $vst3Src)
{
    Write-Host "Installing VST3..." -ForegroundColor Cyan
    Remove-Item -Recurse -Force $vst3Dst -ErrorAction SilentlyContinue
    Copy-Item -Recurse -Force $vst3Src "C:\Program Files\Common Files\VST3\"
    Write-Host "VST3 installed to $vst3Dst" -ForegroundColor Green
}

# ------------------------------------------------------------------
# Install CLAP to system folder
# ------------------------------------------------------------------
$clapSrc = Join-Path $artefacts "CLAP\AtiNAMatiC-Boost.clap"
$clapDst = "C:\Program Files\Common Files\CLAP\AtiNAMatiC-Boost.clap"

if (Test-Path $clapSrc)
{
    Write-Host "Installing CLAP..." -ForegroundColor Cyan
    $clapDir = "C:\Program Files\Common Files\CLAP"
    if (-not (Test-Path $clapDir)) { New-Item -ItemType Directory -Force $clapDir | Out-Null }
    Copy-Item -Force $clapSrc $clapDst
    Write-Host "CLAP installed to $clapDst" -ForegroundColor Green
}

# ------------------------------------------------------------------
# Populate Installer\Bundle\ for Inno Setup packaging
# ------------------------------------------------------------------
if ($Config -eq "Release") {
    $bundleDir = Join-Path $root "Installer\Bundle"
    New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null

    $vst3BundleDst = Join-Path $bundleDir "AtiNAMatiC-Boost.vst3"
    if (Test-Path $vst3Src) {
        if (Test-Path $vst3BundleDst) { Remove-Item -Recurse -Force $vst3BundleDst }
        Copy-Item -Recurse -Force $vst3Src $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-Boost.vst3/" -ForegroundColor DarkCyan
    }

    if (Test-Path $clapSrc) {
        Copy-Item -Force $clapSrc $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-Boost.clap" -ForegroundColor DarkCyan
    }

    # Build installer if Inno Setup is present
    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $iscc)) { $iscc = "${env:ProgramFiles}\Inno Setup 6\ISCC.exe" }
    if (Test-Path $iscc) {
        Write-Host "Building installer..." -ForegroundColor Cyan
        & $iscc (Join-Path $root "Installer\AtiNAMatiC-Boost.iss")
        if ($LASTEXITCODE -ne 0) { Write-Warning "ISCC failed (exit $LASTEXITCODE)" }
        else { Write-Host "  -> Installer\Output\AtiNAMatiC-Boost-Setup-$Config.exe" -ForegroundColor Green }
    } else {
        Write-Host "Inno Setup not found - run manually: ISCC.exe Installer\AtiNAMatiC-Boost.iss" -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "Build OK." -ForegroundColor Green
Write-Host "VST3       : $vst3Dst"
Write-Host "CLAP       : $clapDst"
