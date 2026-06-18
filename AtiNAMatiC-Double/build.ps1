# build.ps1 - Windows build script for AtiNAMatiC-Double
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

# --- Locate MSBuild via vswhere -------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Install Visual Studio 2022 with the C++ workload."
}
$vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath
if (-not $vsPath) {
    $vsPath = & $vswhere -latest -property installationPath
}
if (-not $vsPath) {
    Write-Error "No Visual Studio installation found."
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

# --- CMake build ------------------------------------------------------------
Write-Host "`nBuilding $Config..." -ForegroundColor Cyan
$cpuCount = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
cmake --build $BuildDir --config $Config --parallel $cpuCount

if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)" }

Write-Host "`nBuild OK." -ForegroundColor Green

$arts = "$BuildDir\AtiNAMatiCDouble_artefacts\$Config"
Write-Host "Standalone : $arts\Standalone\A-Double.exe"
Write-Host "VST3       : $arts\VST3\AtiNAMatiC-Double.vst3"
if (Test-Path "$arts\CLAP\AtiNAMatiC-Double.clap") {
    Write-Host "CLAP       : $arts\CLAP\AtiNAMatiC-Double.clap"
}

# --- Install VST3 system-wide -----------------------------------------------
if ($Config -eq "Release") {
    $vst3Src = "$arts\VST3\AtiNAMatiC-Double.vst3"
    $vst3Dst = "$env:CommonProgramFiles\VST3\AtiNAMatiC-Double.vst3"
    if (Test-Path $vst3Src) {
        if (Test-Path $vst3Dst) { Remove-Item -Recurse -Force $vst3Dst }
        Copy-Item -Recurse -Force $vst3Src "$env:CommonProgramFiles\VST3\"
        Write-Host "Installed VST3: $vst3Dst" -ForegroundColor Green
    }

    # --- Install CLAP system-wide -------------------------------------------
    $clapSrc = "$arts\CLAP\AtiNAMatiC-Double.clap"
    $clapDir = "$env:CommonProgramFiles\CLAP"
    if (Test-Path $clapSrc) {
        New-Item -ItemType Directory -Force -Path $clapDir | Out-Null
        Copy-Item -Force $clapSrc $clapDir
        Write-Host "Installed CLAP: $clapDir\AtiNAMatiC-Double.clap" -ForegroundColor Green
    }

    # --- Populate Installer\Bundle\ for Inno packaging ----------------------
    $bundleDir = Join-Path $ScriptDir "Installer\Bundle"
    New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null

    $exeSrc = "$arts\Standalone\A-Double.exe"
    if (Test-Path $exeSrc) {
        # Clean any stale exe (from before the A-Double rename) so it doesn't ship
        Get-ChildItem $bundleDir -Filter "*.exe" -ErrorAction SilentlyContinue | Remove-Item -Force
        Copy-Item -Force $exeSrc $bundleDir
        Write-Host "Bundle: copied A-Double.exe" -ForegroundColor DarkCyan
    }

    $vst3BundleDst = Join-Path $bundleDir "AtiNAMatiC-Double.vst3"
    if (Test-Path $vst3Src) {
        if (Test-Path $vst3BundleDst) { Remove-Item -Recurse -Force $vst3BundleDst }
        Copy-Item -Recurse -Force $vst3Src $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-Double.vst3/" -ForegroundColor DarkCyan
    }

    if (Test-Path $clapSrc) {
        Copy-Item -Force $clapSrc $bundleDir
        Write-Host "Bundle: copied AtiNAMatiC-Double.clap" -ForegroundColor DarkCyan
    } else {
        Write-Host "Bundle: CLAP not built (run cmake configure first)" -ForegroundColor Yellow
    }

    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $iscc)) { $iscc = "${env:ProgramFiles}\Inno Setup 6\ISCC.exe" }
    if (Test-Path $iscc) {
        Write-Host "Building installer..." -ForegroundColor Cyan
        & $iscc (Join-Path $ScriptDir "Installer\AtiNAMatiC-Double.iss")
        if ($LASTEXITCODE -eq 0) { Write-Host "  -> Installer\Output\" -ForegroundColor Green }
        else { Write-Warning "ISCC failed (exit $LASTEXITCODE)" }
    } else {
        Write-Host "Inno Setup not found - run: ISCC.exe Installer\AtiNAMatiC-Double.iss" -ForegroundColor DarkGray
    }
}
