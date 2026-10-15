# AtiNAMatiC-Sag — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

A real tube amp with a tube rectifier can't hold its supply voltage when you dig in. The voltage droops, the amp loses a little gain and headroom, then recovers over a few hundred milliseconds. Players call it **sag**: hard notes squash slightly, then **bloom** back. It is a big part of why an old amp feels "spongy" and forgiving.

Amp captures are usually taken with steady test signals, so much of that touch response is missing. AtiNAMatiC-Sag puts the droop-and-recover in front of your amp model.

- Below about -36 dBFS nothing happens; the droop grows with how hard you play.
- The supply can't collapse instantly, so your pick attack passes first and the note sags just after, then swells back.
- As the supply droops the headroom drops too: a gentle, slightly asymmetric soft-clip that only appears while the "supply" is loaded.
- Zero latency.

## 2. Signal chain placement

```
Guitar -> Gate -> [AtiNAMatiC-Sag] -> Boost / Dirt -> Amp sim -> Cab / IR
```

Place Sag **before** the amp sim. It works on clean, crunch and high-gain models; the effect is most obvious on edge-of-breakup tones.

## 3. Controls

### SAG (big knob, 0–100 %, default 40 %)
How far the supply droops when you dig in.

| Position | Feel |
|---|---|
| 0 % (STIFF) | Clean pass-through. Nothing changes. |
| 30–50 % (WARM) | Notes give a little under hard picking; chords feel rounder. |
| 70–100 % (SPONGY) | Obvious squash and bloom; soft playing stays open, hard playing compresses. |

Level is calibrated so turning SAG does not make the signal louder or quieter (within about ±1 dB on typical guitar).

### SUPPLY (mode switch)

| Mode | Droop / recovery | Max droop | Character |
|---|---|---|---|
| **Rectifier** | 20 ms / 180 ms | 9 dB | Classic tube-rectifier sag. |
| **Variac** | 40 ms / 320 ms | 6 dB | "Running at low voltage": softer, earlier clipping, slower. |
| **Bloom** | 6 ms / 450 ms | 12 dB | Fast dip, long recovery: notes dip then swell back up. |

### MIX (0–100 %)
Blend with the dry signal for a subtler effect.

### OUTPUT (-24 to +12 dB)
Final level trim.

## 4. Meters

- **INPUT / OUTPUT** — peak level.
- **DROOP** — how far the supply is currently sagging, in dB.
- Status: **STIFF** (no droop), **BREATHING** (light), **SAGGING** (more than 3 dB).

## 5. Signal chain (inside)

```
Input -> supply envelope (linked L/R) -> gain droop (smoothed 5 ms)
      -> headroom stage: soft clip whose drive and asymmetry follow the droop
      -> DC blocker -> static make-up -> MIX -> OUTPUT
```

## 6. Latency and CPU

Zero latency. Very light on CPU.

## 7. Installation

Windows: run **AtiNAMatiC-Sag-Setup-0.2.0.exe**, then rescan plug-ins in your DAW. Manual install: copy `AtiNAMatiC-Sag.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Sag.clap` to `C:\Program Files\Common Files\CLAP\`.

## 8. Tips

- Pair with **Feel** for a very forgiving practice tone: Feel evens out the picking, Sag adds the bloom.
- Bloom mode at 40–60 % on a clean amp gives a nice "swell" on sustained chords.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Sag v0.2.0 · Atij 666 Studio*
