# AtiNAMatiC-Boost — User Manual
**Version 0.1.0**
Atij 666 Studio — Designed & developed by Atij

---

## Overview

**AtiNAMatiC-Boost** adds life, punch, and harmonic clarity to any signal — designed to sit after your NAM amp sim or anywhere in your chain where you want musical saturation, presence, and dynamic energy.

Unlike a simple booster, it uses a three-stage soft-saturation engine with character modes and parallel mix control, so you can push the tone without destroying your dynamics.

---

## System Requirements

| | Minimum |
|---|---|
| **OS** | Windows 10 (64-bit) · macOS 10.15+ · Linux (Ubuntu 20.04+) |
| **CPU** | Any modern 64-bit processor |
| **RAM** | 256 MB |
| **Formats** | Standalone · VST3 · CLAP · AU (macOS) |
| **DAW** | Any VST3 or CLAP host |

---

## Installation

### Windows — Using the Installer (Recommended)
Run **AtiNAMatiC-Boost-v0.1.0-Setup.exe** and follow the on-screen steps.

The installer places files at:

| Format | Location |
|--------|----------|
| Standalone | `C:\Program Files\Atij666Studio\AtiNAMatiC-Boost\` |
| VST3 | `C:\Program Files\Common Files\VST3\` |
| CLAP | `C:\Program Files\Common Files\CLAP\` |

After installation, rescan plugins in your DAW.

> **Antivirus notice:** Windows may show a SmartScreen or antivirus popup on first run because the binary is not yet widely known. The software is safe — click *More info → Run anyway* (SmartScreen) or *Allow* / *Trust* in your AV. This warning disappears as more users install the plugin and its reputation builds.

---

### Windows — Manual Installation (No Installer)
If you prefer not to use the installer, copy the files yourself — no extra files or dependencies are needed.

**Step 1 — Copy your desired formats:**

| File | Copy to |
|------|---------|
| `AtiNAMatiC-Boost.exe` | Anywhere (e.g. `C:\Program Files\Atij666Studio\AtiNAMatiC-Boost\`) |
| `AtiNAMatiC-Boost.vst3` *(folder)* | `C:\Program Files\Common Files\VST3\` |
| `AtiNAMatiC-Boost.clap` | `C:\Program Files\Common Files\CLAP\` |

> **Important:** The `.vst3` is a **folder**, not a single file. Copy the entire folder (including its `Contents\` subdirectory) into the VST3 directory.

**Step 2 — Rescan in your DAW**
After copying, open your DAW and trigger a plugin rescan. The exact option varies by DAW:
- **Reaper:** Options → Preferences → Plug-ins → VST → Re-scan
- **Bitwig:** Settings → Plug-ins → Rescan
- **Ableton Live:** Preferences → Plug-Ins → Rescan
- **FL Studio:** Options → Manage plugins → Refresh

**To uninstall manually:** delete the copied files from the paths above.

---

### macOS
Mount the **.dmg**, drag the components to the indicated folders, then rescan in your DAW.

| Format | Location |
|--------|----------|
| Standalone | `/Applications/` |
| VST3 | `/Library/Audio/Plug-Ins/VST3/` |
| AU | `/Library/Audio/Plug-Ins/Components/` |
| CLAP | `/Library/Audio/Plug-Ins/CLAP/` |

---

### Linux
Run `./build-linux.sh` from the project root. Plugins are installed to `~/.vst3/` and `~/.clap/`.

---

## Controls Reference

```
┌─────────────────────────────────────────────┐
│  AtiNAMatiC-Boost          Life, punch...   │
│ ─────────────────────────────────────────── │
│                  BOOST                      │
│               ╭───────╮                     │
│               │   ◈   │   ← main knob       │
│               ╰───────╯                     │
│        CLEAN    SWEET    PUSH               │
│ ─────────────────────────────────────────── │
│             CHARACTER                       │
│  [BALANCED]   [BRIGHT]   [THICK]            │
│ ─────────────────────────────────────────── │
│             ACTIVITY     ◉ CLEAN            │
│  INPUT  ▓▓▓▓▓▓░░░░░░░░░         -90 dB     │
│  OUTPUT ▓▓▓▓▓▓░░░░░░░░░         -90 dB     │
│         -60  -48  -36  -24  -12  0 dB       │
│ ─────────────────────────────────────────── │
│     MIX                  OUTPUT             │
│    [knob]                [knob]             │
│    100 %                 0.00 dB            │
└─────────────────────────────────────────────┘
```

### BOOST
Controls the drive into the saturation engine. Higher values push more harmonic content and add musical compression. The Spark Pill indicator shows the current harmonic activity level.

| Range | Character |
|-------|-----------|
| 0 – 20 % | Transparent gain, subtle warmth |
| 20 – 50 % | Natural harmonic enhancement — sweet spot for most signals |
| 50 – 80 % | Noticeable saturation, aggressive presence |
| 80 – 100 % | Heavy push, leads and solos |

### CHARACTER

Selects the tonal focus of the saturation engine:

| Mode | Description |
|------|-------------|
| **BALANCED** | Flat character — enhances presence evenly across the spectrum |
| **BRIGHT** | High-frequency clarity shelf — adds edge and definition, ideal for single-coil pickups and clean tones |
| **THICK** | Low-mid weight shelf — fattens humbuckers and palm-muted riffs |

### MIX
Parallel blend between the dry signal (0 %) and fully processed signal (100 %). Use lower values (20–50 %) for New York–style parallel saturation — adds harmonic density while preserving the attack transients of the dry signal.

### OUTPUT
Final output trim gain, range **−24 to +12 dB**. Use it to match perceived loudness after boosting, or to drive the next stage harder.

### Activity Meter
Shows real-time input (top) and output (bottom) levels from **−60 to 0 dB**. The output band colour shifts toward the accent colour as harmonic content increases — giving a visual read of how hard the saturation is working.

### Spark Pill
A harmonic activity indicator in the top-right of the meter section:

| Label | Harmonic Amount |
|-------|-----------------|
| **CLEAN** | 0 – 10 % — minimal saturation |
| **LIFT** | 10 – 45 % — light harmonic enhancement |
| **ALIVE** | 45 – 75 % — moderate saturation |
| **PUSH** | 75 – 100 % — heavy drive |

---

## Signal Chain (Under the Hood)

```
Input
  → High-pass filter (35 Hz)     removes sub-bass mud before saturation
  → Clean gain stage             scales signal into saturation range
  → Tanh soft-clipping           adds even-order harmonics
  → 2nd-harmonic bias            warms the saturation character
  → Clarity / Mode shelf EQ      shapes tonal character (per MODE)
  → Auto make-up gain            compensates loudness
  → Output trim                  user-controlled OUTPUT knob
  → Dry/Wet blend                MIX knob
Output
```

---

## Typical Use Cases

### After a NAM amp sim
Place AtiNAMatiC-Boost on the channel after your Neural Amp Modeler. Use **BALANCED** mode at **40–60 % BOOST**, **MIX 100 %**, and trim OUTPUT to match the unprocessed loudness. This adds the "pushed amp" feel that neural captures sometimes lack.

### Parallel saturation bus
Send multiple tracks to an auxiliary. Insert AtiNAMatiC-Boost with **MIX 20–40 %** and **BOOST 70 %** in **THICK** mode. Blend back to taste — adds glue and warmth without thickening transients.

### Lead guitar presence boost
Use **BRIGHT** mode with **BOOST 60–80 %** and **MIX 60 %**. Keeps the dry attack while adding harmonic cut-through. Reduce OUTPUT by 2–4 dB to compensate loudness before automation.

### Drum bus punch
**BALANCED** mode, **BOOST 30 %**, **MIX 30–50 %**. Adds body and snap without obvious saturation. Especially effective on room mics or overheads.

---

## Tips

- The **35 Hz high-pass filter** is always active — this is intentional. Sub-bass energy in the saturation stage creates intermodulation distortion; removing it keeps the low end clean.
- Use **BOOST + low MIX** rather than **low BOOST + full MIX** when you want subtle enhancement — pushing the saturation stage harder and blending less gives richer harmonics at equal perceived loudness.
- For A/B comparison, set **MIX to 0 %** and trim **OUTPUT** until loudness matches, then gradually raise MIX.

---

## About

**AtiNAMatiC-Boost v0.1.0**
Atij 666 Studio
Designed & developed by Atij

Built with:
- JUCE Framework — Raw Material Software Ltd.
- VST3 SDK — Steinberg Media Technologies GmbH
- CLAP SDK — Free Audio / clap-juce-extensions
- Neural Amp Modeler (NAM) — Steven Atkinson & contributors
- The open-source audio & guitar-plugin community

*Thank you to everyone who makes open-source audio tools possible.*

---

*© 2026 Atij 666 Studio. All rights reserved.*
