# build.ps1 -- AtiNAMatiC-Gate Windows build script
# Usage: .\build.ps1

$ErrorActionPreference = "Stop"
$Root    = $PSScriptRoot
$Build   = Join-Path $Root "build"
$Target  = "AtiNAMatiCGate"
$Config  = "Release"

# -- Find vswhere -----------------------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere not found. Install Visual Studio 2022." }

$vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsPath) { throw "No Visual Studio installation found." }

# -- CMake configure --------------------------------------------------------
Write-Host "[1/3] Configuring..." -ForegroundColor Cyan
cmake -S $Root -B $Build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

# -- CMake build ------------------------------------------------------------
Write-Host "[2/3] Building $Config..." -ForegroundColor Cyan
cmake --build $Build --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

# -- Install VST3 system-wide -----------------------------------------------
Write-Host "[3/3] Installing VST3..." -ForegroundColor Cyan
$vst3Src = Join-Path $Build "${Target}_artefacts\$Config\VST3\AtiNAMatiC-Gate.vst3"
$vst3Dst = "$env:CommonProgramFiles\VST3\AtiNAMatiC-Gate.vst3"
if (Test-Path $vst3Dst) { Remove-Item -Recurse -Force $vst3Dst }
Copy-Item -Recurse -Force $vst3Src "$env:CommonProgramFiles\VST3\"
Write-Host "  -> Installed: $vst3Dst" -ForegroundColor Green

# -- Populate Installer\Bundle\ ---------------------------------------------
$bundle = Join-Path $Root "Installer\Bundle"
if (-not (Test-Path $bundle)) { New-Item -ItemType Directory -Force $bundle | Out-Null }

# Remove any stale AtiNAMatiC-Gate.exe from a previous build naming
$staleExe = Join-Path $bundle "AtiNAMatiC-Gate.exe"
if (Test-Path $staleExe) { Remove-Item -Force $staleExe }

$exeSrc = Join-Path $Build "${Target}_artefacts\$Config\Standalone\A-Gate.exe"
if (Test-Path $exeSrc) {
    Copy-Item -Force $exeSrc $bundle
    Write-Host "  -> Standalone (A-Gate.exe) copied to Bundle" -ForegroundColor Green
}

# Inno script expects Bundle\AtiNAMatiC-Gate.vst3\* (not Bundle\VST3\)
$vst3BundleDst = Join-Path $bundle "AtiNAMatiC-Gate.vst3"
if (Test-Path $vst3BundleDst) { Remove-Item -Recurse -Force $vst3BundleDst }
Copy-Item -Recurse -Force $vst3Src $bundle
Write-Host "  -> VST3 copied to Bundle\AtiNAMatiC-Gate.vst3" -ForegroundColor Green

# Also clean up legacy Bundle\VST3 folder if present
$legacyVst3 = Join-Path $bundle "VST3"
if (Test-Path $legacyVst3) { Remove-Item -Recurse -Force $legacyVst3 }

$clapSrc = Join-Path $Build "${Target}_artefacts\$Config\CLAP\AtiNAMatiC-Gate.clap"
if (Test-Path $clapSrc) {
    # Also install CLAP system-wide
    $clapDir = "$env:CommonProgramFiles\CLAP"
    New-Item -ItemType Directory -Force -Path $clapDir | Out-Null
    Copy-Item -Force $clapSrc $clapDir
    Write-Host "  -> Installed CLAP: $clapDir\AtiNAMatiC-Gate.clap" -ForegroundColor Green

    Copy-Item -Force $clapSrc $bundle
    Write-Host "  -> CLAP copied to Bundle" -ForegroundColor Green
}

Write-Host ""
Write-Host "Build complete. Rescan your DAW to pick up the new VST3." -ForegroundColor Yellow
