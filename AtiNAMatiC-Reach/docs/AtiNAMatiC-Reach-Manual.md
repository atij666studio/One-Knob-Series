# AtiNAMatiC-Reach — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

AtiNAMatiC-Reach makes a guitar **step forward and spread out** in a mix without getting louder, muddier, or falling apart in mono.

- **Projection (centre):** a presence lift around 2.2 kHz (up to +3.5 dB) and a dip around 300 Hz (up to -2 dB). The guitar reads clearly without turning the fader up.
- **Solid low end:** stereo content below about 150 Hz is folded to the centre, so palm mutes stay tight on any system.
- **Spread (sides):** existing stereo content above 1 kHz is widened, and a spread is generated from the guitar's upper range (above 700 Hz, about 1 ms offset), so a **mono amp sim** gets width too.
- **Mono-safe:** the generated spread goes into left and right with opposite signs, so it cancels exactly when the mix is summed to mono. Only the centre's projection EQ remains.
- Accepts a mono input and outputs stereo. Zero latency.

## 2. Signal chain placement

```
Guitar -> ... -> Amp sim -> Cab / IR -> [AtiNAMatiC-Reach] -> Squash / Level
```

Place Reach **after** the amp and cab, on a stereo track (or a mono track whose insert outputs stereo).

## 3. Controls

### REACH (big knob, 0–100 %, default 40 %)
NEAR (0 %) is untouched. Turning it up raises projection and spread together. Level is matched within about ±0.2 dB.

### SHAPE (mode switch)

| Shape | Presence | Widening | Generated spread | Offset |
|---|---|---|---|---|
| **Focus** | +3.5 dB | light | light | 0.8 ms |
| **Wide** (default) | +3 dB | medium | medium | 1.0 ms |
| **Huge** | +2.5 dB | strong | strong | 1.4 ms |

(Values at 100 %.)

### MIX (0–100 %) and OUTPUT (-24 to +12 dB)
Blend with the dry signal and final trim.

## 4. Meters

- **LEFT / RIGHT** — peak level of each side.
- **WIDTH** — side-to-mid energy of the output (0 % = mono). Low notes stay centred by design, so width rises on brighter playing.
- Status: **MONO**, **FOCUSED**, **WIDE**, **HUGE**.

## 5. Signal chain (inside)

```
L/R -> mid / side
mid  -> presence peak 2.2 kHz -> mud dip 300 Hz                      (projection)
side -> minus low-passed side (<150 Hz) + widened side (>1 kHz)
     +  generated spread: mid -> HP 700 Hz -> LP 9 kHz -> ~1 ms delay
-> L = mid + side, R = mid - side -> MIX -> OUTPUT
```

## 6. Latency and CPU

Zero latency. Very light on CPU.

## 7. Installation

Windows: run **AtiNAMatiC-Reach-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Reach.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Reach.clap` to `C:\Program Files\Common Files\CLAP\`.

## 8. Tips

- Reach and **Air** combine well: Air adds room depth, Reach adds projection and spread.
- For double-tracked guitars already hard-panned, use Focus: projection without extra width.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Reach v0.2.0 · Atij 666 Studio*
