# AtiNAMatiC-Mono — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

Stereo amp sims, doublers and wideners are great until the low end gets wide and woolly, or the mix is played on a phone or a club PA in mono. AtiNAMatiC-Mono puts stereo where it helps and mono where it matters, with one knob.

- **Bass:** the low end becomes mono below a crossover you set; the rest of the image is untouched.
- **Full:** narrows the whole image from full stereo to mono.
- **Safe:** like Full, but the lows are always mono and the loudness is held as the image narrows.
- Watches left/right correlation and shows **PHASE ISSUE** when the input would partly cancel in mono.
- Zero latency.

## 2. Signal chain placement

Anywhere after something that makes stereo:

```
Amp sim -> Cab -> Double / Reach / Air -> [AtiNAMatiC-Mono] -> Squash / Level
```

## 3. Controls

### MONO (big knob, 0–100 %, default 50 %)
- **Bass:** the crossover frequency, 40 Hz (0 %) to 400 Hz (100 %), on a log scale. 50 % ≈ 126 Hz.
- **Full / Safe:** how narrow the image is; 0 % = untouched stereo, 100 % = mono.

### MODE (switch)

| Mode | What MONO does | Loudness |
|---|---|---|
| **Bass** | side removed below the crossover (LR4 high-pass on the side only) | unchanged |
| **Full** | whole image narrowed | drops a little as wide content is removed |
| **Safe** | lows below 120 Hz always mono + whole image narrowed | held (compensated) |

### OUTPUT (-24 to +12 dB)
Final trim.

## 4. Meters

- **LEFT / RIGHT** — peak level of each side.
- **WIDTH** — side-to-mid energy of the output (0 % = mono).
- Status: **STEREO**, **NARROW**, **MONO**, or **PHASE ISSUE** (input correlation below -0.1).

Measured on a wide doubled guitar: Bass at 50 % removes the side below ~60 Hz by 13 dB and at 100 % below ~200 Hz by 25 dB, leaving the upper image and the centre untouched; Full and Safe reach true mono at 100 %, Safe within 0.1 dB of the original loudness.

## 5. Installation

Windows: run **AtiNAMatiC-Mono-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Mono.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Mono.clap` to `C:\Program Files\Common Files\CLAP\`.

## 6. Tips

- After **Double** or **Reach**, Bass at 30–50 % keeps palm mutes centred and tight.
- Use Full at 100 % as a quick mono check of your whole guitar sound.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Mono v0.2.0 · Atij 666 Studio*
