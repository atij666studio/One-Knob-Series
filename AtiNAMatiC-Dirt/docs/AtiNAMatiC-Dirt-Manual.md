# AtiNAMatiC-Dirt -- User Manual

**Version 0.1.0** · Atij 666 Studio · Windows (Standalone, VST3 & CLAP)

---

## 1. What it is

AtiNAMatiC-Dirt is a pre-amp **character drive** stage designed to sit **before** your Neural Amp Modeler (NAM) amp model in the signal chain. It adds grit, edge, and harmonic texture without crossing into full distortion territory. Think of it as giving the amp model a "dirtier guitar" to work with -- attitude without destruction.

It is intentionally subtle by design. The wave-shaper is auto-normalised so loudness barely changes as DIRT increases, making it safe to dial in and out without level jumps.

Three flavours of breakup let you match the character to the style:

| Type   | Character                                                     |
|--------|---------------------------------------------------------------|
| EDGE   | Smooth tanh saturation -- warm, even-harmonic sheen           |
| BITE   | tanh + soft hard-clip ceiling -- sharper transient attack     |
| CRUNCH | Double-tanh compression -- more squashed, fizzy upper grit    |

---

## 2. Signal chain placement

```
Guitar  ->  [AtiNAMatiC-Dirt]  ->  NAM Amp Model  ->  Cab / IR  ->  AtiNAMatiC-Air  ->  Master
```

**Always place AtiNAMatiC-Dirt before the amp model / NAM.** It voices itself for clean-to-crunch source material. Placing it after a fully-distorted amp sim will compound artefacts rather than enhance character.

---

## 3. The Controls

### DIRT (large knob, centre)
The master drive macro. Sweeps from 0% (clean pass-through) to 100% (full harmonic grit). Internally this maps to 0 to +22 dB of drive gain into the wave-shaper, with automatic make-up applied so the output level stays roughly steady across the entire sweep.

Most players live between **20% and 55%**. Above 70% the grit becomes deliberate and pronounced -- useful for high-gain styles feeding a clean NAM model.

### TYPE (EDGE / BITE / CRUNCH)
Selects the saturation character (see table above). Each type also shifts the pre-emphasis frequency slightly to focus the grit where guitars project:

- **EDGE** focuses around 1.5 kHz -- the classic "woman tone" nasal body.
- **BITE** focuses a little higher -- attack and pick clarity.
- **CRUNCH** focuses upper-mids -- rasp and compression.

### MIX (Wet/Dry)
0% = bypass, 100% = fully processed. Default 100%. Blend back the dry signal to add just a hint of grit without committing fully -- useful for studio tracking when the engineer wants flexibility.

### OUTPUT (-24 to +12 dB)
Final trim after all processing. Even with auto make-up, different guitar pickups and amp models may need slight level matching here. Use it to match the bypassed level before committing to a setting.

### Activity Meter
Real-time dual-bar meter showing input and output levels (-60 to 0 dBFS). As DIRT increases, the output bar transitions from blue-grey to the rust/ember accent colour, giving a visual reference for how much harmonic content is being added.

### Grit Pill (top-right of meter area)
A LED-style indicator that names the current saturation character:

| Label | Meaning                                      |
|-------|----------------------------------------------|
| CLEAN | Negligible drive -- near-transparent output  |
| GRIT  | Subtle edge -- just kissing the shaper       |
| BITE  | Noticeable character -- harmonic texture     |
| FANG  | Heavy grit -- amp model will respond hard    |

---

## 4. System Requirements

| | Minimum |
|---|---|
| OS | Windows 10 64-bit (or later) |
| CPU | Any x64 processor (SSE2) |
| RAM | 32 MB |
| Formats | Standalone, VST3, CLAP |
| DAW | Any VST3 or CLAP host (Reaper, Ableton, FL Studio, Bitwig, etc.) |

Linux builds (VST3 + CLAP) available via `build-linux.sh`.

---

## 5. Installation

### Windows -- Installer (recommended)
1. Run `AtiNAMatiC-Dirt-Setup-0.1.0.exe` (requires administrator rights).
2. Choose which formats to install:
   - **Standalone** -- runs as a normal application, no DAW needed.
   - **VST3** -- installs to `C:\Program Files\Common Files\VST3\`.
   - **CLAP** -- installs to `C:\Program Files\Common Files\CLAP\`.
3. Click Install, then rescan plug-ins in your DAW (see DAW Rescan section below).

### Windows -- Manual install
Copy the files from `Installer\Bundle\` to the correct folders:

| Format     | Source file                    | Destination folder                                    |
|------------|-------------------------------|-------------------------------------------------------|
| Standalone | `AtiNAMatiC-Dirt.exe`          | Anywhere you like (e.g. `C:\Program Files\Atij666Studio\`) |
| VST3       | `AtiNAMatiC-Dirt.vst3\` (folder) | `C:\Program Files\Common Files\VST3\`              |
| CLAP       | `AtiNAMatiC-Dirt.clap`         | `C:\Program Files\Common Files\CLAP\`               |

> **Note:** Copying VST3 requires administrator rights. Right-click the terminal / Explorer and choose **Run as administrator** if prompted.

PowerShell one-liners (run as Administrator):
```powershell
# VST3
Copy-Item -Recurse -Force ".\Installer\Bundle\AtiNAMatiC-Dirt.vst3" "$env:CommonProgramFiles\VST3\"

# CLAP
Copy-Item -Force ".\Installer\Bundle\AtiNAMatiC-Dirt.clap" "$env:CommonProgramFiles\CLAP\"
```

### macOS -- Manual install
> macOS builds require compiling from source. Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` then `cmake --build build --config Release`.

| Format     | Destination                                           |
|------------|-------------------------------------------------------|
| Standalone | `/Applications/` or anywhere in your Applications     |
| VST3       | `/Library/Audio/Plug-Ins/VST3/` (system) or `~/Library/Audio/Plug-Ins/VST3/` (user) |
| AU         | `/Library/Audio/Plug-Ins/Components/` (system) or `~/Library/Audio/Plug-Ins/Components/` (user) |
| CLAP       | `/Library/Audio/Plug-Ins/CLAP/` (system) or `~/Library/Audio/Plug-Ins/CLAP/` (user) |

Terminal commands (user install, no sudo required):
```bash
cp -r build/AtiNAMatiCDirt_artefacts/Release/VST3/AtiNAMatiC-Dirt.vst3 \
    ~/Library/Audio/Plug-Ins/VST3/
cp -r build/AtiNAMatiCDirt_artefacts/Release/AU/AtiNAMatiC-Dirt.component \
    ~/Library/Audio/Plug-Ins/Components/
cp build/AtiNAMatiCDirt_artefacts/Release/CLAP/AtiNAMatiC-Dirt.clap \
    ~/Library/Audio/Plug-Ins/CLAP/
```

### Linux -- Manual install
Build from source:
```bash
./build-linux.sh
```
Then install:
```bash
# Per-user (no sudo required)
mkdir -p ~/.vst3 ~/.clap
cp -r build-linux/AtiNAMatiCDirt_artefacts/Release/VST3/AtiNAMatiC-Dirt.vst3 ~/.vst3/
cp  build-linux/AtiNAMatiCDirt_artefacts/Release/CLAP/AtiNAMatiC-Dirt.clap  ~/.clap/

# System-wide (requires sudo)
sudo cp -r build-linux/AtiNAMatiCDirt_artefacts/Release/VST3/AtiNAMatiC-Dirt.vst3 /usr/lib/vst3/
sudo cp  build-linux/AtiNAMatiCDirt_artefacts/Release/CLAP/AtiNAMatiC-Dirt.clap  /usr/lib/clap/
```

### DAW rescan (after manual install)
After copying files manually, tell your DAW to find the new plug-in:

| DAW        | Where to rescan                                                          |
|------------|--------------------------------------------------------------------------|
| Reaper     | Options > Preferences > Plug-ins > VST > **Re-scan** (or Clear cache and re-scan) |
| Ableton    | Options > Plug-in Source Preferences > **Rescan**                        |
| FL Studio  | Options > Manage Plug-ins > **Start scan**                               |
| Bitwig     | Settings > Plug-ins > **Rescan plug-ins**                                |
| Cubase     | Studio > Studio Setup > VST Plug-in Manager > **Update**                 |
| Logic Pro  | Plug-in Manager > **Reset & Rescan** (AU only)                           |
| CLAP hosts | Most hosts auto-detect on next launch; check host preferences if not     |

---

## 6. Controls Reference (Quick Summary)

| Control   | Range          | Default | Description                      |
|-----------|----------------|---------|----------------------------------|
| DIRT      | 0 -- 100%      | 30%     | Drive macro (pre-shaper gain)    |
| TYPE      | EDGE/BITE/CRUNCH | EDGE  | Saturation character             |
| MIX       | 0 -- 100%      | 100%    | Dry/wet blend                    |
| OUTPUT    | -24 to +12 dB  | 0 dB    | Final output trim                |

---

## 7. Signal Chain (Internal)

```
Input
  --> HPF 40 Hz (sub tighten)
  --> Pre-emphasis peak (type-dependent, ~1.5 kHz)
  --> Drive gain (0 to +22 dB, driven by DIRT)
  --> Wave-shaper (EDGE / BITE / CRUNCH)
  --> De-emphasis (mirror of pre-emphasis)
  --> Post-tone tilt (gentle darkening as DIRT rises, prevents harshness)
  --> Auto make-up + OUTPUT trim
  --> Dry/wet MIX
Output
```

---

## 8. Typical Use Cases

### Clean-ish NAM model, wants more life
- TYPE: EDGE, DIRT: 25-40%, MIX: 100%
- Adds warmth and even harmonics without obvious distortion.

### High-gain rhythm -- wants attack and clarity
- TYPE: BITE, DIRT: 35-55%, MIX: 80-100%
- Sharpens pick attack, focuses the midrange before the amp model compresses it.

### Lead tone -- compressed, singing grit
- TYPE: CRUNCH, DIRT: 45-65%, MIX: 70-100%
- Double-tanh compression smooths peaks and adds fizzy sustain harmonic content.

### Subtle parallel blend
- Any type, DIRT: 50-80%, MIX: 20-40%
- Blends harmonic texture behind the dry signal -- adds "hair" without obvious effect.

---

## 9. Tips

- **Less is more.** At 30% DIRT even CRUNCH mode is subtle. Increase gradually and listen.
- **Stack with NAM.** AtiNAMatiC-Dirt is designed to excite the NAM model's input stage. Set NAM input gain lower when using high DIRT settings.
- **Use OUTPUT to null-test.** Set MIX to 0% and match levels with OUTPUT. Then raise MIX to hear exactly what Dirt is adding.
- **EDGE for chords, BITE for solos.** Type switching mid-song is valid -- automate it in your DAW.

---

## 10. About

**AtiNAMatiC-Dirt** is part of the **AtiNAMatiC Series** -- a suite of focused utility plug-ins built around the Neural Amp Modeler workflow.

- Designed & developed by **Atij** (Atij 666 Studio)
- Built with JUCE Framework (Raw Material Software Ltd.)
- VST3 SDK (Steinberg Media Technologies GmbH)
- Neural Amp Modeler -- Steven Atkinson & contributors
- CMake, MSVC, clap-juce-extensions, and the open-source audio community

Click the **AtiNAMatiC-Dirt** title badge in the plugin UI to open the About panel.
