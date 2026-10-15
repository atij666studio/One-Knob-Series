# AtiNAMatiC-Slow — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

Violin-style volume swells without touching a volume knob or pedal. AtiNAMatiC-Slow hides the pick attack of every note and fades the note in, like a classic "slow gear" pedal.

- Every new pick is detected automatically, including picks over strings that are still ringing: besides the overall level, the detector watches the pick click above 4 kHz. It re-arms only once the previous attack has settled, so a note never triggers twice.
- At each new attack the level dips to silence in 4 ms (the last note's tail fades instead of clicking), then the new note swells in over the swell time.
- Noise below about -50 dBFS never triggers a swell.
- 6 ms latency, reported to your DAW (which compensates). Slow uses it to look ahead: the volume is already closed when the pick arrives, so none of the pick click gets through. The delay is inaudible because the attack is hidden anyway.

Measured on picked notes at the default 263 ms swell: 13 % level at 60 ms, 60 % at 150 ms, full level by 270 ms, the same on every note.

## 2. Signal chain placement

```
Guitar -> [AtiNAMatiC-Slow] -> Amp sim -> Cab / IR -> Room / delay / reverb
```

Before the amp for the classic swell into distortion; after the amp for smoother, more "pad"-like swells. A reverb or delay after it makes the effect bloom.

## 3. Controls

### SWELL TIME (big knob, 50 ms → 2 s)
How long each note takes to fade in. The readout shows the time. Short settings soften the attack; long settings give ambient pads.

### CURVE (mode switch)

| Curve | Shape | Feel |
|---|---|---|
| **Swell** (default) | smooth S-curve | the classic slow-gear swell |
| **Pad** | slow rise in dB, softest start | string-pad, the attack completely hidden |
| **Reverse** | holds back, then rushes in at the end | "reversed note" feel |

### MIX (0–100 %)
100 % = full swell; lower settings let some of the pick attack through.

### OUTPUT (-24 to +12 dB)
Final trim.

## 4. Meters

- **INPUT / OUTPUT** — peak level.
- **SWELL** — the current swell level (0 % at a new attack, 100 % once open).
- Status: **WAITING**, **SWELLING**, **OPEN**.

## 5. Installation

Windows: run **AtiNAMatiC-Slow-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Slow.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Slow.clap` to `C:\Program Files\Common Files\CLAP\`.

## 6. Tips

- Swell at 300–600 ms with **Room** (Hall) after the amp is an instant ambient guitar.
- Play legato for continuous pads; a new pick restarts the swell.
- For faster playing, shorten the swell time. A note picked again before its swell finishes is faded out while it is still rising.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Slow v0.2.0 · Atij 666 Studio*
