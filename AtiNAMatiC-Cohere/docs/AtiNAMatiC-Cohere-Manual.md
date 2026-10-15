# AtiNAMatiC-Cohere — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

Amp sims, NAM captures especially, often carry a harsh **bite** around 3–4 kHz and a **fizz** around 6–9 kHz that jump out on some notes and chords and not others. A static EQ cut dulls every note to fix the few.

AtiNAMatiC-Cohere cuts those two bands **only while they stick out** compared with the guitar's own normal balance, then glues the result with a gentle compressor and a touch of tape-style saturation. The tone you dialled in stays; the moments that hurt get caught.

- Bite and fizz are detected in narrow bands and cut with smooth, wider filters: up to 9 dB (bite) and 12 dB (fizz) on the worst moments, almost nothing on normal playing.
- Measured on a test signal with fizz spikes: spikes cut by about 5 dB, the rest of the playing by under 1 dB.
- Glue compression and make-up are calibrated so the level stays put (within about ±0.7 dB).
- Zero latency.

## 2. Signal chain placement

```
Guitar -> ... -> Amp sim -> Cab / IR -> [AtiNAMatiC-Cohere] -> Air / Double / Squash / Level
```

Place Cohere **after** the amp and cab. It works on single tracks and on a guitar bus.

## 3. Controls

### COHERE (big knob, 0–100 %, default 40 %)
One knob for all three stages. At 0 % (RAW) the signal is untouched. Turning it up lowers the margin a band must stick out by before it is cut (6 dB → 1 dB), deepens the glue compression and blends in the saturation.

### CHARACTER (mode switch)

| Character | Taming | Glue | Saturation | Top end |
|---|---|---|---|---|
| **Smooth** | strongest | light | very light | flat |
| **Glue** (default) | balanced | balanced | light | flat |
| **Tape** | lighter | strongest | most | -1.5 dB shelf above 9 kHz |

### MIX (0–100 %) and OUTPUT (-24 to +12 dB)
Parallel blend (phase-exact, no combing) and final trim.

## 4. Meters

- **OUTPUT** — peak level.
- **TAME** — how much bite/fizz is being cut right now (the larger of the two), in dB.
- **GLUE** — compressor gain reduction, in dB.
- Status: **SMOOTH** (nothing to tame), **TAMING**, **TAMING HARD**.

## 5. Signal chain (inside)

```
Input -> linked detectors: broadband, bite (3.5 kHz, Q 2.5), fizz (7.5 kHz, Q 3)
      -> each band's share vs its own 1.5 s average -> dynamic cuts (Q 1.4 / Q 1.2)
      -> glue: soft-knee RMS compressor (20 ms / 250 ms)
      -> tape-style saturation (blended) -> Tape shelf -> make-up -> MIX -> OUTPUT
```

## 6. Latency and CPU

Zero latency. Light on CPU.

## 7. Installation

Windows: run **AtiNAMatiC-Cohere-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Cohere.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Cohere.clap` to `C:\Program Files\Common Files\CLAP\`.

## 8. Tips

- On a fizzy high-gain capture, start in Smooth at 40–60 %.
- On a guitar bus, Glue at 30–50 % before **Squash** or **Level** gives a more finished sound.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Cohere v0.2.0 · Atij 666 Studio*
