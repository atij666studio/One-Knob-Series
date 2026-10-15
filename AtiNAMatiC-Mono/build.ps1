# build.ps1 - Windows build script for AtiNAMatiC-Mono
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

# --- Locate MSBuild via vswhere ---------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Install Visual Studio 2022 with the C++ workload."
}
$vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath
if (-not $vsPath) {
    # Fallback: any VS installation
    $vsPath = & $vswhere -latest -property installationPath
}
if (-not $vsPath) {
    Write-Error "No Visual Studio installation found. Install VS 2022 with the 'Desktop development with C++' workload."
}
$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Error "MSBuild.exe not found inside VS at: $vsPath"
}
Write-Host "Using VS at: $vsPath" -ForegroundColor Cyan

# --- Detect and wipe stale macOS cmake cache --------------------------------
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $cacheContent = Get-Content $cacheFile -Raw
    # Mac cache contains Unix Makefile generator or Darwin platform references
    if ($cacheContent -match "CMAKE_MAKE_PROGRAM.*make|CMAKE_SYSTEM_NAME.*Darwin|Unix Makefiles") {
        Write-Host "Detected stale macOS CMake cache - clearing build/ folder..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }
}

# --- CMake configure --------------------------------------------------------
Write-Host "`nConfiguring with Visual Studio 17 2022 (x64)..." -ForegroundColor Cyan
cmake -S . -B $BuildDir `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_BUILD_TYPE=$Config

if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed (exit $LASTEXITCODE)" }

#  CMake build
Write-Host "`nBuilding $Config..." -ForegroundColor Cyan
$cpuCount = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
cmake --build $BuildDir --config $Config --parallel $cpuCount

if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)" }

# --- Output locations + copy to Installer\Bundle\ ----------------------------
Write-Host "`nBuild OK." -ForegroundColor Green

$arts = "$BuildDir\AtiNAMatiCMono_artefacts\$Config"
Write-Host "Standalone : $arts\Standalone\A-Mono.exe"
Write-Host "VST3       : $arts\VST3\AtiNAMatiC-Mono.vst3"
if (Test-Path "$arts\CLAP\AtiNAMatiC-Mono.clap") {
    Write-Host "CLAP       : $arts\CLAP\AtiNAMatiC-Mono.clap"
}
Write-Host ""
Write-Host "Installed VST3 : $env:CommonProgramFiles\VST3\AtiNAMatiC-Mono.vst3" -ForegroundColor Gray

# --- Copy artefacts to Installer\Bundle\ for Inno packaging -----------------
if ($Config -eq "Release") {
    $bundleDir = Join-Path $ScriptDir "Installer\Bundle"
    New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null

    # Standalone (renamed to A-Mono.exe so desktop shortcut is visually distinct)
    $exeSrc = "$arts\Standalone\A-Mono.exe"
    if (Test-Path $exeSrc) {
        # Clean up any old AtiNAMatiC-Mono.exe left over from previous builds
        $oldExe = Join-Path $bundleDir "AtiNAMatiC-Mono.exe"
        if (Test-Path $oldExe) { Remove-Item -Force $oldExe }
        Copy-Item -Force $exeSrc $bundleDir
        Write-Host "Bundle: copied A-Mono.exe" -ForegroundColor DarkCyan
    }

    # VST3 (whole folder - copy with Replace semantics)
    $vst3Src = "$arts\VST3\AtiNAMatiC-Mono.vst3"
    $vst3Dst = Join-Path $bundleDir "AtiNAMatiC-Mono.vst3"
    if (Test-Path $vst3Src) {
        if (Test-Path $vst3Dst) { Remove-Item -Recurse -Force $vst3Dst }
        Copy-Item -Recurse -Force $vst3Src $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-Mono.vst3/" -ForegroundColor DarkCyan
    }

    # CLAP
    $clapSrc = "$arts\CLAP\AtiNAMatiC-Mono.clap"
    if (Test-Path $clapSrc) {
        Copy-Item -Force $clapSrc $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-Mono.clap" -ForegroundColor DarkCyan
    } else {
        Write-Host "Bundle: CLAP not built yet (run cmake configure first)" -ForegroundColor Yellow
    }

    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $iscc)) { $iscc = "${env:ProgramFiles}\Inno Setup 6\ISCC.exe" }
    if (Test-Path $iscc) {
        Write-Host "Building installer..." -ForegroundColor Cyan
        & $iscc (Join-Path $ScriptDir "Installer\AtiNAMatiC-Mono.iss")
        if ($LASTEXITCODE -eq 0) { Write-Host "  -> Installer\Output\" -ForegroundColor Green }
        else { Write-Warning "ISCC failed (exit $LASTEXITCODE)" }
    } else {
        Write-Host "Inno Setup not found - run: ISCC.exe Installer\AtiNAMatiC-Mono.iss" -ForegroundColor DarkGray
    }
}
