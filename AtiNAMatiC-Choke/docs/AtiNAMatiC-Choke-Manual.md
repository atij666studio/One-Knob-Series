# AtiNAMatiC-Choke — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

Tight riffs live and die by how notes **stop**. A gate only removes the noise between notes; it can't make a ringing chord stop sooner. AtiNAMatiC-Choke works inside the note's own decay: once a note has fallen a little below its peak, Choke makes the rest of the decay fall faster. Stops get tighter, the riff gets more staccato, and the pick attack is never touched.

- Choke follows each note's own peak, so it works the same on quiet and loud passages (it is relative, not a fixed threshold).
- Every new pick attack restores full level instantly.
- The level detector holds for a full cycle of a low E, so it never rides the waveform: no distortion on low notes.
- Zero latency.

Measured on held notes (100 %): Tight cuts the late tail by about 17 dB, Staccato by about 37 dB, Djent chokes notes within ~0.3 s. Attacks: unchanged.

## 2. Signal chain placement

```
Guitar -> Gate -> [AtiNAMatiC-Choke] -> ... -> Amp sim          (tighter playing into the amp)
Amp sim -> Cab / IR -> [AtiNAMatiC-Choke]                        (shorter amp ring-out)
```

Both work. Before the amp it shapes how you play into it; after the amp it also shortens the amp's own sustain.

## 3. Controls

### CHOKE (big knob, 0–100 %, default 40 %)
How fast the decay is pulled down once it starts. 0 % (RING) is a clean pass-through.

### STYLE (mode switch)

| Style | Starts cutting | Max ratio (100 %) | For |
|---|---|---|---|
| **Tight** | 9 dB below the note peak | 3.5 : 1 | rhythm that should just sound "tight" |
| **Staccato** | 6 dB below | 5 : 1 | short, stabbing riffs |
| **Djent** | 3 dB below | 8 : 1 | machine-gun stops, chugs, metal breaks |

### MIX (0–100 %) and OUTPUT (-24 to +12 dB)
Blend with the dry signal (a little ring back in) and final trim.

## 4. Meters

- **INPUT / OUTPUT** — peak level.
- **CHOKE** — how much the decay is currently being cut, in dB.
- Status: **RINGING**, **TIGHTENING**, **CHOKED**.

## 5. Installation

Windows: run **AtiNAMatiC-Choke-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Choke.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Choke.clap` to `C:\Program Files\Common Files\CLAP\`.

## 6. Tips

- Use **Gate** for the noise, Choke for the ring: Gate → Choke → amp is a very tight front end.
- On chord stabs, Staccato at 40–60 % plus Mix 80 % keeps a little natural ring.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Choke v0.2.0 · Atij 666 Studio*
