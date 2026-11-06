# Build every ONE KNOB plug-in (Windows x64) and stage the results:
#
#   dist\stage\AtiNAMatiC-<Name>\VST3\AtiNAMatiC-<Name>.vst3
#                               \CLAP\AtiNAMatiC-<Name>.clap
#                               \LV2\AtiNAMatiC-<Name>.lv2
#                               \AtiNAMatiC-<Name>-Manual.pdf
#
# Run from a Visual Studio x64 developer prompt (CI uses ilammy/msvc-dev-cmd).
param([string]$Generator = "Ninja")
$ErrorActionPreference = "Stop"

$Root  = Split-Path -Parent $PSScriptRoot
$Stage = Join-Path $Root "dist\stage"
$Deps  = Join-Path $Root "dist\deps"
New-Item -ItemType Directory -Force $Stage, $Deps | Out-Null

function Invoke-Native { param([string]$Exe, [string[]]$ArgList)
  & $Exe @ArgList
  if ($LASTEXITCODE -ne 0) { throw "$Exe failed (exit $LASTEXITCODE)" }
}

# JUCE and clap-juce-extensions are fetched once and shared by all builds.
$Juce = Join-Path $Deps "JUCE"
$Clap = Join-Path $Deps "clap-juce-extensions"
if (-not (Test-Path "$Juce\CMakeLists.txt")) {
  Invoke-Native git @("clone", "--depth", "1", "--branch", "8.0.4", "https://github.com/juce-framework/JUCE.git", $Juce)
}
if (-not (Test-Path "$Clap\CMakeLists.txt")) {
  Invoke-Native git @("clone", "https://github.com/free-audio/clap-juce-extensions.git", $Clap)
  Invoke-Native git @("-C", $Clap, "checkout", "7adee3a1bd4684d4caa5601100e364abccff4b4d")
  Invoke-Native git @("-C", $Clap, "submodule", "update", "--init", "--recursive")
}

$Plugins = Get-Content (Join-Path $Root "packaging\plugins.txt") |
  Where-Object { $_ -and -not $_.StartsWith("#") } | ForEach-Object { ($_ -split "\|")[0] }

foreach ($N in $Plugins) {
  $P = "AtiNAMatiC-$N"
  $Build = Join-Path $Root "$P\build-release"
  Write-Host "::group::Build $P"
  $cfg = @("-S", (Join-Path $Root $P), "-B", $Build, "-G", $Generator,
           "-DCMAKE_BUILD_TYPE=Release", "-DJUCE_PATH=$Juce",
           "-DFETCHCONTENT_SOURCE_DIR_CLAP_JUCE_EXTENSIONS=$Clap")
  if ($Generator -like "Visual Studio*") { $cfg += @("-A", "x64") }
  Invoke-Native cmake $cfg
  Invoke-Native cmake @("--build", $Build, "--config", "Release", "--parallel")
  Write-Host "::endgroup::"

  $A = Join-Path $Build "AtiNAMatiC${N}_artefacts\Release"
  $D = Join-Path $Stage $P
  if (Test-Path $D) { Remove-Item -Recurse -Force $D }
  New-Item -ItemType Directory -Force "$D\VST3", "$D\CLAP", "$D\LV2" | Out-Null
  Copy-Item -Recurse "$A\VST3\$P.vst3" "$D\VST3\"
  Copy-Item          "$A\CLAP\$P.clap" "$D\CLAP\"
  Copy-Item -Recurse "$A\LV2\$P.lv2"   "$D\LV2\"
  Copy-Item (Join-Path $Root "$P\docs\$P-Manual.pdf") "$D\"
}

Write-Host "Staged $($Plugins.Count) plug-ins in $Stage"
