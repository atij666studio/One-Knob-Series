# build.ps1 - Windows build script for AtiNAMatiC-HitZone
# Usage:  .\build.ps1              (Release)
#         .\build.ps1 -Config Debug
param(
    [string]$Config = "Release"
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$BuildDir = "build"

# --- Locate MSBuild via vswhere ----------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Install Visual Studio 2022 with the C++ workload."
}
$vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath
if (-not $vsPath) {
    $vsPath = & $vswhere -latest -property installationPath
}
if (-not $vsPath) {
    Write-Error "No Visual Studio installation found. Install VS 2022 with the Desktop development with C++ workload."
}
$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Error "MSBuild.exe not found inside VS at: $vsPath"
}
Write-Host "Using VS at: $vsPath" -ForegroundColor Cyan

# --- Detect and wipe stale macOS cmake cache ---------------------------
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $cacheContent = Get-Content $cacheFile -Raw
    if ($cacheContent -match "CMAKE_MAKE_PROGRAM.*make|CMAKE_SYSTEM_NAME.*Darwin|Unix Makefiles") {
        Write-Host "Detected stale macOS CMake cache - clearing build/ folder..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }
}

# --- CMake configure ---------------------------------------------------
Write-Host "`nConfiguring with Visual Studio 17 2022 (x64)..." -ForegroundColor Cyan
cmake -S . -B $BuildDir `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_BUILD_TYPE=$Config

if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed (exit $LASTEXITCODE)" }

# --- CMake build -------------------------------------------------------
Write-Host "`nBuilding $Config..." -ForegroundColor Cyan
$cpuCount = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
cmake --build $BuildDir --config $Config --parallel $cpuCount

if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)" }

Write-Host "`nBuild OK." -ForegroundColor Green

$arts = "$BuildDir\AtiNAMatiCHitZone_artefacts\$Config"
Write-Host "Standalone : $arts\Standalone\A-HitZone.exe"
Write-Host "VST3       : $arts\VST3\AtiNAMatiC-HitZone.vst3"
if (Test-Path "$arts\CLAP\AtiNAMatiC-HitZone.clap") {
    Write-Host "CLAP       : $arts\CLAP\AtiNAMatiC-HitZone.clap"
}

# --- Install VST3 system-wide ------------------------------------------
if ($Config -eq "Release") {
    $vst3Src = "$arts\VST3\AtiNAMatiC-HitZone.vst3"
    $vst3Sys = "$env:CommonProgramFiles\VST3\AtiNAMatiC-HitZone.vst3"
    if (Test-Path $vst3Src) {
        if (Test-Path $vst3Sys) { Remove-Item -Recurse -Force $vst3Sys }
        Copy-Item -Recurse -Force $vst3Src "$env:CommonProgramFiles\VST3\"
        Write-Host "Installed VST3: $vst3Sys" -ForegroundColor Green
    }

    # --- Copy artefacts to Installer\Bundle\ ---------------------------
    $bundleDir = Join-Path $ScriptDir "Installer\Bundle"
    New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null

    # Remove any stale AtiNAMatiC-HitZone.exe from a previous build naming
    $staleExe = Join-Path $bundleDir "AtiNAMatiC-HitZone.exe"
    if (Test-Path $staleExe) { Remove-Item -Force $staleExe }

    $exeSrc = "$arts\Standalone\A-HitZone.exe"
    if (Test-Path $exeSrc) {
        Copy-Item -Force $exeSrc $bundleDir
        Write-Host "Bundle: copied A-HitZone.exe" -ForegroundColor DarkCyan
    }

    $vst3Dst = Join-Path $bundleDir "AtiNAMatiC-HitZone.vst3"
    if (Test-Path $vst3Src) {
        if (Test-Path $vst3Dst) { Remove-Item -Recurse -Force $vst3Dst }
        Copy-Item -Recurse -Force $vst3Src $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-HitZone.vst3/" -ForegroundColor DarkCyan
    }

    $clapSrc = "$arts\CLAP\AtiNAMatiC-HitZone.clap"
    if (Test-Path $clapSrc) {
        Copy-Item -Force $clapSrc $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-HitZone.clap" -ForegroundColor DarkCyan
    } else {
        Write-Host "Bundle: CLAP not found (run cmake configure to enable)" -ForegroundColor Yellow
    }

    Write-Host ""
    Write-Host "Installer\Bundle\ is ready - run ISCC.exe on Installer\AtiNAMatiC-HitZone.iss to build the installer." -ForegroundColor Green
}
