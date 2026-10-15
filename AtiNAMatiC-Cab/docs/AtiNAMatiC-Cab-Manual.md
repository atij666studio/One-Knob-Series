# AtiNAMatiC-Cab — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

Many NAM captures are "amp only" (direct out, no cabinet): played straight they sound fizzy and harsh. Normally you'd load an impulse response. AtiNAMatiC-Cab is the one-knob alternative: a speaker cabinet voiced with filters, where the one knob moves the microphone across the speaker.

- Resonant low cut (the "thump" of a closed box), low-mid body, a slight mid scoop, two cone-breakup presence peaks (~2.5 kHz and ~4.5 kHz) and a steep speaker roll-off.
- **MIC** moves the mic from the dust cap (bright, forward) to the cone edge (darker, warmer). Measured on a cab-less amp: the fizz above 7 kHz is cut by about 11 dB at the cap and 29 dB at the edge.
- Loudness is matched automatically from the filter response (within about ±1.5 dB across the knob and cabinets).
- Zero latency, minimum phase, very light on CPU.

It is not an impulse-response loader and does not try to copy a specific cabinet; it is a fast, musical "cab" that is always in tune with the knob.

## 2. Signal chain placement

```
Guitar -> ... -> Amp sim (no cab) -> [AtiNAMatiC-Cab] -> Cohere / Air / Room
```

Use it **after** an amp capture that has no cabinet. Do not stack it after a cab/IR (you'd filter twice).

## 3. Controls

### MIC POSITION (big knob, 0–100 %, default 35 %)

| Position | Sound |
|---|---|
| 0 % (CAP) | brightest, most forward, most presence |
| 50 % (CONE) | balanced |
| 100 % (EDGE) | darker, warmer, rounder low-mids |

### CABINET (mode switch)

| Cabinet | Low cut / thump | Body | Character |
|---|---|---|---|
| **1x12** | 105 Hz, soft | 200 Hz | open-back combo: lighter, looser low end |
| **2x12** | 85 Hz | 160 Hz | balanced |
| **4x12** (default) | 75 Hz, resonant | 140 Hz | closed-back stack: tight, deep thump |

### MIX (0–100 %) and OUTPUT (-24 to +12 dB)
Blend with the raw amp (for a little extra edge) and final trim.

## 4. Meters

- **INPUT / OUTPUT** — peak level.
- **BRIGHT** — where the mic is (100 % = on the cap).
- Status: **ON THE CAP**, **MID CONE**, **CONE EDGE**.

## 5. Installation

Windows: run **AtiNAMatiC-Cab-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Cab.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Cab.clap` to `C:\Program Files\Common Files\CLAP\`.

## 6. Tips

- High gain: 4x12, MIC 30–50 %. Then **Cohere** if anything still bites.
- Clean / edge of breakup: 1x12, MIC 20–40 %.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Cab v0.2.0 · Atij 666 Studio*
