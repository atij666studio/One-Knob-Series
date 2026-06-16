# AtiNAMatiC-HitZone — User Manual

**Version 0.1.0** · Atij 666 Studio · Windows (Standalone, VST3 & CLAP)

---

## 1. What it is

AtiNAMatiC-HitZone is an intelligent input-level conditioner designed to sit **before** Neural Amp Modeler (NAM) plugins in your signal chain. Its job is simple and critical: make sure the signal hitting your NAM model is always at the exact level those models were trained at.

NAM models are typically captured with peaks around –18 dBFS and an RMS around **–16 dBFS**. Feed them too quietly and the amp sim sounds dull, thin, and lifeless. Feed them too loud and they compress, clip, or distort in unpleasant, unintended ways. HitZone watches your input level in real time, adjusts gain automatically, and tells you in plain language: **Too Cold**, **Perfect**, or **Too Hot**.

Beyond gain management, the **Sweet Spot** macro tunes the tonal character of the conditioned signal — from bright and tight (COLD side) to warm and driven (HOT side) — using gentle EQ shelves and a touch of asymmetric tube-style saturation.

In short: AtiNAMatiC-HitZone takes the guesswork out of driving NAM correctly.

---

## 2. Signal chain placement

```
Guitar  →  ★ AtiNAMatiC-HitZone ★  →  NAM Plugin  →  Cab / IR  →  Post-FX  →  Master
```

**Always place HitZone directly before your NAM model, never after it.**
The level meter and auto-gain are calibrated to NAM's training level. Placing it after NAM has no benefit and may introduce unwanted colouration.

If you use a noise gate, place it **before** HitZone so the gate's closed state does not confuse the auto-gain with near-silence readings.

---

## 3. The Controls

### SWEET SPOT (large central knob)

The master macro knob. Turning it from COLD to HOT simultaneously adjusts:

- **Tonal EQ** — COLD side lifts 4.5 kHz (+2 dB high shelf) and slightly cuts 180 Hz (–2 dB low shelf) for a bright, present character. HOT side does the opposite: +3 dB warmth at 180 Hz and –2.5 dB at 4.5 kHz for a thick, vintage feel.
- **Saturation** — below centre the signal is clean; above centre a gentle asymmetric tube-style soft-clip (2nd-harmonic bias) blends in progressively, reaching full drive at the HOT extreme.

Most players find their sweet spot somewhere between 35 % and 65 %. Highly distorted amp models benefit from the COLD side (keeps things tight); cleaner and crunch models often respond well to the HOT side.

**Default:** 50 % (centre — flat EQ, no saturation)  
**Double-click to reset to default.**

---

### AUTO (toggle, centre)

Enables real-time automatic gain adjustment.

| State | Behaviour |
|-------|-----------|
| **ON** (default) | HitZone continuously monitors RMS level and applies up to ±24 dB of gain to keep your signal in the sweet zone (–20 to –12 dBFS, ideal –16 dBFS). |
| **OFF** | Auto-gain is disabled. The **GAIN** knob (replaces RESPONSE in the bottom row) lets you dial in a fixed input gain manually. |

Auto mode uses an **asymmetric tracking algorithm**: it pulls gain down quickly when the signal is too hot (to protect the NAM model from clipping), but brings gain up slowly when too cold (to avoid level-pumping on transients and note attacks).

---

### RESPONSE (bottom row, centre — when AUTO is ON)

Controls how fast the automatic gain tracks changes in input level.

| Position | Up speed | Down speed |
|----------|----------|------------|
| **0 % (Slow)** | ~1.5 dB/sec | ~6 dB/sec |
| **50 % (default)** | ~6 dB/sec | ~16 dB/sec |
| **100 % (Fast)** | ~18 dB/sec | ~60 dB/sec |

Slow response is natural and transparent — good for playing through full songs. Fast response is useful when switching between quiet clean passages and high-gain riffs in the same session.

A **safety stomp** is always active regardless of Response: if the peak level approaches –3 dBFS (dangerously close to clipping the NAM model), the gain is pulled down at the fastest available rate immediately.

**Default:** 35 %  
**Double-click to reset to default.**

---

### GAIN (bottom row, centre — when AUTO is OFF)

Fixed input gain trim. Range: –24 dB to +24 dB, default 0 dB.

Use this when you want total manual control — for example when recording automation, when your DAW chain already includes level management, or when tracking the gain for a specific guitar/pickup combination.

The **Gain Readout** (small number to the right of the status label) shows the currently applied gain in dB — in Auto mode this reflects the live auto-gain value; in Manual mode it reflects the GAIN knob.

**Double-click to reset to 0 dB.**

---

### MIX (bottom row, left)

Wet/Dry blend. 0 % = fully dry (bypassed), 100 % = fully processed.

At values below 100 % the processed and original signals are summed. This is useful for parallel-processing workflows: run HitZone at 100 % for the NAM chain and blend in a small amount of unprocessed signal for transient clarity.

**Default:** 100 %  
**Double-click to reset to 100 %.**

---

### OUTPUT (bottom row, right)

Final output level trim, applied after all processing. Range: –24 dB to +24 dB, default 0 dB.

Use this to compensate for any net level change introduced by the EQ shelves or saturation, or to match levels between HitZone on/off.

**Double-click to reset to 0 dB.**

---

## 4. Status indicators

### Status label (centre of UI)

Displays the current input-level state in real time, updated at 20 Hz:

| Label | Meaning | What to do |
|-------|---------|------------|
| **Too Cold** (blue) | RMS below –21 dBFS — signal is too quiet for NAM | Increase gain at source, or turn up GAIN knob (manual mode) |
| **Perfect** (green) | RMS between –21 and –11 dBFS — NAM sweet zone | Nothing — you're in the zone |
| **Too Hot** (orange/red) | RMS above –11 dBFS — signal risks clipping the NAM model | Reduce gain at source, or use AUTO |

Status uses hysteresis to prevent rapid flickering at zone boundaries: the indicator does not switch back to Perfect until the signal has moved 0.5–1 dB clear of the threshold.

In **AUTO** mode, HitZone drives itself to maintain "Perfect" status. The label may briefly show Too Cold or Too Hot during fast transient changes.

---

### Gain Readout (small number, right of status)

Shows the currently applied input gain in dB (up to one decimal place). In AUTO mode this is the live auto-gain value; in MANUAL mode it mirrors the GAIN knob. Useful for setting a fixed trim: switch AUTO off, watch the readout until it stabilises in manual mode, then use that value as your GAIN knob reference.

---

### Level Meter

A horizontal RMS level meter spanning the COLD–HOT range, positioned between the Sweet Spot knob and the status row.

| Element | Meaning |
|---------|---------|
| **Shaded band** | Sweet zone (–20 to –12 dBFS) — target region |
| **Filled bar** | Live RMS level |
| **Peak indicator** | Short-term peak envelope |
| **Peak-hold dot** | Holds the highest peak seen, decays slowly (1.5 s) |

The COLD and HOT labels at the left and right of the knob area indicate the tonal character of the Sweet Spot position, **not** the current level status. Level status is indicated by the status label.

---

## 5. System requirements

| | Minimum |
|--|---------|
| **OS** | Windows 10 (64-bit) or later |
| **CPU** | Any modern x86-64 processor |
| **RAM** | 4 MB per instance |
| **DAW** | Any VST3-compatible host (64-bit), or CLAP-compatible host |
| **Sample rates** | 44.1, 48, 88.2, 96, 176.4, 192 kHz |
| **Buffer sizes** | Any (zero-latency processing) |

---

## 6. Installation

### Windows — Installer (recommended)

1. Run **AtiNAMatiC-HitZone-Setup-0.1.0.exe**
2. Choose the formats you want: **Standalone**, **VST3**, and/or **CLAP**
3. Click Install
4. After installation a dialog will remind you to rescan plug-ins in your DAW
5. Rescan your DAW (see table below) and load HitZone from the plug-in browser

The installer places files at:

| Format | Location |
|--------|----------|
| Standalone | `C:\Program Files\Atij 666 Studio\AtiNAMatiC-HitZone\AtiNAMatiC-HitZone.exe` |
| VST3 | `C:\Program Files\Common Files\VST3\AtiNAMatiC-HitZone.vst3\` |
| CLAP | `C:\Program Files\Common Files\CLAP\AtiNAMatiC-HitZone.clap` |

---

### Windows — Manual installation

Copy the files from `Installer\Bundle\` to the locations above by hand, then rescan your DAW.

---

### macOS — Manual installation

Build from source (`build.sh` or CMake with Xcode), then install:

| Format | Location |
|--------|----------|
| Standalone | `~/Applications/AtiNAMatiC-HitZone.app` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/AtiNAMatiC-HitZone.vst3` |
| AU | `~/Library/Audio/Plug-Ins/Components/AtiNAMatiC-HitZone.component` |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP/AtiNAMatiC-HitZone.clap` |

---

### Linux — Manual installation

Build with `build-linux.sh` (requires CMake + Ninja + a C++17 compiler), then install:

| Format | Location |
|--------|----------|
| Standalone | `/usr/local/bin/AtiNAMatiC-HitZone` |
| VST3 | `~/.vst3/AtiNAMatiC-HitZone.vst3/` |
| CLAP | `~/.clap/AtiNAMatiC-HitZone.clap` |

---

### DAW rescan after install

| DAW | Where to rescan |
|-----|----------------|
| **Reaper** | Options → Preferences → Plug-ins → VST → Re-scan |
| **Cubase / Nuendo** | Studio → VST Plug-in Manager → Update |
| **Studio One** | Options → Locations → VST Plug-ins → Reset Blacklist → Rescan |
| **Ableton Live** | Preferences → Plug-Ins → Rescan |
| **Bitwig Studio** | Settings → Plug-ins → Rescan plug-in locations |
| **FL Studio** | Options → Manage plug-ins → Start scan |
| **Pro Tools (CLAP)** | n/a — CLAP not supported; use VST3 via a wrapper if available |

In your DAW the plug-in appears under manufacturer **Atij666Studio**, category **Fx / Dynamics / Guitar**.

---

## 7. Controls reference

| Control | Range | Default | Description |
|---------|-------|---------|-------------|
| Sweet Spot | 0–100 % | 50 % | Tonal character + saturation macro (COLD = bright/clean, HOT = warm/driven) |
| AUTO | On/Off | On | Enables RMS-based automatic input gain |
| Response | 0–100 % | 35 % | Auto-gain tracking speed (visible when AUTO is ON) |
| Gain | –24 to +24 dB | 0 dB | Fixed manual input gain (visible when AUTO is OFF) |
| Mix | 0–100 % | 100 % | Wet/Dry blend |
| Output | –24 to +24 dB | 0 dB | Final output level trim |

All knobs support **double-click to reset to default**.

---

## 8. Internal signal chain

```
Input
  │
  ├──► [Dry copy for parallel Mix]
  │
  ▼
[Pre-detector HPF @ 35 Hz]  ← prevents subsonic bias in RMS reading
  │
  ▼
[RMS detector]  (60 ms attack / 250 ms release)
[Peak detector] (instant attack / 600 ms release)
[Peak-hold]     (holds, then 1.5 s decay)
  │
  ▼
[Auto-gain / Manual-gain decision]
  → status → UI: Too Cold / Perfect / Too Hot
  │
  ▼
[Input gain (smoothed 30 ms)]
  │
  ▼
[Tilt EQ]
  ├─ Low shelf @ 180 Hz: –2 to +3 dB (Sweet Spot left → right)
  └─ High shelf @ 4.5 kHz: +2 to –2.5 dB (Sweet Spot left → right)
  │
  ▼
[Asymmetric soft saturation]  ← only above 50 % Sweet Spot
  (2nd-harmonic bias, tube-style, blends in progressively)
  │
  ▼
[Output gain (smoothed 20 ms)]
  │
  ▼
[Parallel Mix with dry copy]  (smoothed 20 ms)
  │
  ▼
Output  →  NAM Plugin
```

---

## 9. Typical use cases

### Standard NAM rig (recommended starting point)

- AUTO: **On** · Sweet Spot: **50 %** · Response: **35 %** · Mix: **100 %** · Output: **0 dB**

Set it and forget it. HitZone centres your signal in NAM's sweet zone and Status will read "Perfect" within a few seconds of playing.

---

### High-gain / tight riff work

- AUTO: **On** · Sweet Spot: **25 %** (COLD side) · Response: **70 %** · Mix: **100 %** · Output: **0 dB**

The COLD tonal character (slight high-shelf lift) keeps chug tight and articulate. Fast Response prevents any level softening between palm mutes.

---

### Clean / crunch with vintage warmth

- AUTO: **On** · Sweet Spot: **70 %** (HOT side) · Response: **30 %** · Mix: **100 %** · Output: **–1 dB**

The HOT tonal character adds low-mid weight and a hint of tube saturation that sits well under cleaner NAM models. Slow Response avoids pumping on sustained notes.

---

### Fixed gain (recording / automation)

- AUTO: **Off** · Gain: set by ear until Status reads "Perfect" · Mix: **100 %** · Output: **0 dB**

Useful when you need a locked gain value — for instance, when recording automation lanes or when your interface's preamp level is already dialled for NAM.

---

### Parallel blend

- AUTO: **On** · Mix: **40–60 %** · Output: **0 dB**

Blends HitZone's conditioned signal with the raw dry signal. Adds level intelligence and tonal shaping while preserving pick attack from the unprocessed chain.

---

## 10. Tips

- **Start with AUTO ON at 50 % Sweet Spot.** HitZone will normalise most guitars to Perfect within 2–3 seconds of playing. Adjust Sweet Spot from there to taste.
- **Watch the Gain Readout.** If AUTO consistently applies more than ±10 dB, your source level is significantly off NAM's ideal. Consider adjusting at the source (DI level, track fader) so HitZone has less work to do.
- **The safety stomp is always on.** Even in Manual mode, the auto system's peak-danger check does not apply — but the status indicator still tells you when you're Too Hot. Trust it.
- **Use COLD for metal, HOT for blues.** The tonal presets are subtle but stack up across a full mix. High-gain drop-tuned riffs sound tighter on the COLD side; single-coil and P-90 players often prefer the HOT side's extra body.
- **Response at 100 % can pump.** Very fast tracking on signals with wide dynamic range (fingerpicking, clean arpeggios) can audibly breathe. Keep Response below 60 % for expressive playing.
- **Zero latency.** HitZone adds no latency — suitable for monitoring through your DAW without buffer compensation.

---

## 11. About

**AtiNAMatiC-HitZone** — Always Hit the Perfect Level

Designed and developed by **Atij** · Atij 666 Studio

Built with:
- [JUCE 8](https://juce.com) — Raw Material Software Ltd.
- VST3 SDK — Steinberg Media Technologies GmbH
- [Neural Amp Modeler (NAM)](https://github.com/sdatkinson/neural-amp-modeler) — Steven Atkinson & contributors
- The open-source audio & guitar-plugin community

Personal and commercial use permitted on machines you own.  
Bug reports and feature requests: click the **AtiNAMatiC** title badge in the plug-in window to open the About panel.

---

*AtiNAMatiC-HitZone is part of the AtiNAMatiC Series — a family of focused, hardware-flavoured guitar tools for the modern post-amp signal chain.*
