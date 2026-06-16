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
# Done
# ------------------------------------------------------------------
Write-Host ""
Write-Host "Build OK." -ForegroundColor Green
Write-Host "Standalone : $artefacts\Standalone\A-Boost.exe"
Write-Host "VST3       : $vst3Dst"
Write-Host "CLAP       : $clapDst"
