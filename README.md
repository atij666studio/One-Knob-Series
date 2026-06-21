# ONE KNOB Series

11 free, open-source audio plug-ins for guitar amp-sim rigs (NAM · ToneX · Neural DSP · Kemper etc.).  
Each plug-in does one thing exceptionally well, controlled by a single knob.

**Formats:** VST3 · CLAP · AU (macOS) · LV2 (Linux)  
**Platforms:** Windows 10+ x64 · macOS 10.15+ Universal · Linux x86\_64 · Linux ARM64

---

## Plug-ins

| Plug-in | What it does |
|---------|-------------|
| **AtiNAMatiC-Air** | Post-cab "amp-in-the-room" feel — adds air, presence and stereo depth after your IR |
| **AtiNAMatiC-Boost** | Front-end clean boost — life, punch and clarity before the amp sim |
| **AtiNAMatiC-Dirt** | Harmonic saturation — adds grit and bite without muddying the low end |
| **AtiNAMatiC-Double** | Natural double-tracker — thickens a mono signal without chorus artefacts |
| **AtiNAMatiC-Feel** | Touch-sensitivity shaper — controls how the amp sim responds to pick attack |
| **AtiNAMatiC-Gate** | Intelligent noise gate — silences hum and buzz between notes |
| **AtiNAMatiC-HitZone** | Sweet-spot compressor — finds and locks in the most musical dynamic range |
| **AtiNAMatiC-Level** | Auto output leveler — keeps volume consistent across different amp sims |
| **AtiNAMatiC-Pick** | Pick-attack shaper — dials in the transient character from soft to percussive |
| **AtiNAMatiC-Squash** | Bus and mix compressor — glue and punch for the final mix |
| **AtiNAMatiC-Tight** | Low-end tightener — controls flub and mud in downtuned or high-gain rigs |

---

## Download

Pre-built installers and plug-in bundles are on the [Releases](../../releases) page.

### Windows
Download `ONE-KNOB-Series-vX.X.X-Windows-x64-Installers.zip`, extract, and run each `*-Setup.exe`.  
Each installer copies the plug-in to the standard VST3/CLAP system folders.

### macOS
Download `ONE-KNOB-Series-vX.X.X-macOS-Universal.zip` and copy the bundles to:

| Format | Path |
|--------|------|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |
| AU | `~/Library/Audio/Plug-Ins/Components/` |

> **Note:** Builds are unsigned. On first launch right-click → Open, or run:  
> `sudo xattr -r -d com.apple.quarantine /path/to/plugin.vst3`

### Linux
```bash
tar -xzf ONE-KNOB-Series-vX.X.X-Linux-x86_64.tar.gz
bash install.sh             # user install: ~/.vst3  ~/.clap  ~/.lv2
sudo bash install.sh --system   # system install: /usr/lib/...
```

---

## Building from Source

### Prerequisites

| Platform | Requirements |
|----------|-------------|
| All | CMake 3.22+ (JUCE 8.0.4 is auto-downloaded if not found locally) |
| Windows | Visual Studio 2022 with "Desktop development with C++" workload |
| macOS | Xcode 14+ command-line tools |
| Linux | See apt deps below |

Linux dependencies:
```bash
sudo apt-get install -y cmake ninja-build build-essential pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype6-dev libfontconfig1-dev \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxcomposite-dev libxext-dev libgtk-3-dev libwebkit2gtk-4.1-dev
```

### Windows (one command)
```powershell
cd AtiNAMatiC-Air
.\build.ps1
```
Builds VST3 + CLAP in Release, then runs Inno Setup automatically if installed, producing `Installer\Output\AtiNAMatiC-Air-Setup-0.1.0.exe`.

### macOS (Universal Binary)
```bash
cd AtiNAMatiC-Air
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --parallel
```

### Linux
```bash
cd AtiNAMatiC-Air
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

---

## CI / Releases

GitHub Actions builds all platforms automatically on every push.  
To cut a release, push a version tag:

```bash
git tag v0.1.0
git push origin v0.1.0
```

This triggers the Release workflow, builds all 4 platform targets, and publishes a GitHub Release with signed artifacts for every format.

---

## License

Copyright © 2024–2026 Atij 666 Studio  
Licensed under the [GNU General Public License v3.0](LICENSE).
