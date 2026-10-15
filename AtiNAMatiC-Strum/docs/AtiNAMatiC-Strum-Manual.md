# AtiNAMatiC-Strum — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

A strum is six strings hit at once. A normal full-band transient shaper treats it as one event: lift the attack and the low "thump" of the pick across the strings comes up along with the sparkle, so chords get boomy before they get clear.

AtiNAMatiC-Strum splits the signal into three bands and shapes each one differently:

| Band | Range | What STRUM does |
|---|---|---|
| Low | below 250 Hz | pulls the attack **down**: tames boom and thump |
| Mid | 250 Hz – 2.5 kHz | a little attack and a little extra ring |
| High | above 2.5 kHz | pushes the attack **up**: string definition and sparkle |

The result is strummed chords that sound articulate and "produced" without getting louder or boomier.

- Each band has its own transient detector, linked across left and right so the stereo image stays still.
- The detectors share one release, so sustained notes are left alone; only real strikes are shaped.
- The three bands add back to exactly the input: at 0 % the signal passes untouched.
- Zero latency.

## 2. Signal chain placement

```
Guitar -> [AtiNAMatiC-Strum] -> Amp sim -> Cab / IR              (electric)
Acoustic / clean DI -> [AtiNAMatiC-Strum] -> EQ / Reverb         (acoustic)
```

Before the amp sim for electric guitar; anywhere on a clean or acoustic track.

## 3. Controls

### STRUM (big knob, 0–100 %, default 50 %)
How strongly the three bands are shaped. RAW (0 %) is a clean pass-through; POLISHED (100 %) is the full effect. Level is matched across the knob (within about ±0.2 dB).

### VOICING (mode switch)

| Voicing | Low attack | Mid attack / ring | High attack | For |
|---|---|---|---|---|
| **Clean** | -5 dB | +1.5 / +1 dB | +5 dB | clean electric strumming |
| **Crunch** | -6 dB | +2 / 0 dB | +3.5 dB | driven rhythm, keeps the amp from flubbing |
| **Acoustic** | -5 dB | +1 / +2 dB | +6 dB | acoustic and piezo: sparkle and body ring |

(Values at 100 %; they scale with the knob.)

### MIX (0–100 %) and OUTPUT (-24 to +12 dB)
Blend with the dry signal (never combs, the bands are phase-exact) and final trim.

## 4. Meters

- **INPUT** — peak level.
- **STRIKE** — how much high-band attack is being lifted right now.
- **THUMP** — how much low-band attack is being pulled down, in dB.
- Status: **RINGING** (sustain), **PICKING**, **STRUMMING**.

## 5. Signal chain (inside)

```
Input -> low = LR4 low-pass 250 Hz, rest = input - low
      -> mid = LR4 low-pass 2.5 kHz (of rest), high = rest - mid
      -> per-band linked transient shaper (1 ms / 25 ms attack, shared 100 ms release)
      -> sum -> make-up -> MIX -> OUTPUT
```

## 6. Latency and CPU

Zero latency. Light on CPU.

## 7. Installation

Windows: run **AtiNAMatiC-Strum-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Strum.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Strum.clap` to `C:\Program Files\Common Files\CLAP\`.

## 8. Tips

- For palm-muted metal rhythm use **Tight** instead; Strum is voiced for open chords.
- Acoustic voicing at 40–60 % is a quick "mix-ready" acoustic strum.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Strum v0.2.0 · Atij 666 Studio*
