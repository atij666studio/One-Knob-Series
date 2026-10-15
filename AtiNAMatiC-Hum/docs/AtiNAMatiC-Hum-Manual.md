# AtiNAMatiC-Hum — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

Single-coil pickups, ground loops and nearby power supplies put a steady **hum** (50 or 60 Hz) and a harsher **buzz** (its upper harmonics) into your DI. An amp sim with lots of gain makes it much louder. A gate only hides it between notes; AtiNAMatiC-Hum removes it while you play.

- A cascade of true notches, each only about 2.5 Hz wide, at the mains frequency and its harmonics. Narrow enough that your notes pass.
- **Auto** works out whether your hum is 50 Hz or 60 Hz by itself.
- Measured: hum removed by about 15 dB (30 %), 22 dB (60 %) and 31–38 dB (100 %); a clean guitar with no hum loses only 0.2–0.3 dB.
- Zero latency.

## 2. Signal chain placement

```
Guitar -> [AtiNAMatiC-Hum] -> Gate -> ... -> Amp sim
```

Put Hum **first**, before the gate and before anything that adds gain.

## 3. Controls

### HUM REMOVAL (big knob, 0–100 %, default 50 %)
Sets both how deep the notches cut (0 → -40 dB) and how many harmonics are treated (2 → 12). The low end of the knob (HUM) takes out the 50/60 Hz hum; higher settings (BUZZ) also remove the upper harmonics that make single-coil buzz.

### MAINS (mode switch)

| Mains | Use |
|---|---|
| **Auto** (default) | detects 50 Hz or 60 Hz from your signal (needs a second or two of hum to decide) |
| **50 Hz** | Europe, Asia, Africa, Australia, most of South America |
| **60 Hz** | North America, parts of South America, Japan (west), Korea, Taiwan, Philippines |

### OUTPUT (-24 to +12 dB)
Final trim.

## 4. Meters

- **INPUT** — peak level.
- **HUM** — energy of the hum series in your input.
- **REMOVED** — how much of it is being taken out, in dB.
- Status: **CLEAN**, **50 HZ HUM** / **60 HZ HUM**, or **… REMOVED**.

## 5. Installation

Windows: run **AtiNAMatiC-Hum-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Hum.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Hum.clap` to `C:\Program Files\Common Files\CLAP\`.

## 6. Tips

- Start at 40–60 %; go higher only if you still hear buzz after the amp.
- If Auto picks the wrong mains on a near-silent signal, set 50 Hz or 60 Hz manually.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Hum v0.2.0 · Atij 666 Studio*
