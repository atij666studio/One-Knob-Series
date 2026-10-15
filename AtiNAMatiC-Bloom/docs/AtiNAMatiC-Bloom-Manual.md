# AtiNAMatiC-Bloom — User Manual

**Version 0.2.0** · Atij 666 Studio · Windows · macOS · Linux (VST3, CLAP, AU, LV2, Standalone)

---

## 1. What it is

A loud amp **blooms**: hold a note or a chord and it seems to grow, fuller and richer, instead of just dying away. AtiNAMatiC-Bloom puts that in front of your amp model.

- On every pick attack Bloom remembers how loud the note started.
- While the note sustains, it gradually brings the decaying sustain back up toward that starting level — never above it — over the bloom time.
- As the note blooms, a little harmonic richness is blended in; in **Singing** mode an upper-mid resonance grows too, like an amp just before it feeds back.
- A new attack resets the bloom instantly, so the next pick is never boosted. Signals below about -50 dBFS are left alone, so decays into noise are never lifted.
- Zero latency.

Measured on held notes: the attack is unchanged, the sustain comes up by about +3 dB (50 %) to +6 dB (100 %), and peaks stay where they were.

## 2. Signal chain placement

```
Guitar -> Gate -> [AtiNAMatiC-Bloom] -> Boost / Dirt -> Amp sim -> Cab / IR
```

Place Bloom **before** the amp. It is most obvious on held chords, leads and clean or crunch tones; fast riffing barely triggers it (notes end before they bloom), which keeps rhythm parts tight.

## 3. Controls

### BLOOM (big knob, 0–100 %, default 40 %)
How much of the drop between the attack and the sustain is brought back, up to +9 dB. 0 % (DECAY) is a clean pass-through.

### TIME (mode switch)

| Mode | Bloom time | Character |
|---|---|---|
| **Swell** | 0.35 s | quick bloom, notes hold up |
| **Sustain** | 0.9 s | slower, long sustaining notes |
| **Singing** | 1.4 s | slow swell plus an upper-mid "about to feed back" resonance |

### MIX (0–100 %) and OUTPUT (-24 to +12 dB)
Blend with the dry signal and final trim.

## 4. Meters

- **INPUT / OUTPUT** — peak level.
- **BLOOM** — current bloom gain in dB.
- Status: **ATTACK**, **BLOOMING**, **SINGING**.

## 5. Latency and CPU

Zero latency. Very light on CPU.

## 6. Installation

Windows: run **AtiNAMatiC-Bloom-Setup-0.2.0.exe**, then rescan plug-ins. Manual install: copy `AtiNAMatiC-Bloom.vst3` to `C:\Program Files\Common Files\VST3\` and `AtiNAMatiC-Bloom.clap` to `C:\Program Files\Common Files\CLAP\`.

## 7. Tips

- Leads: Singing at 50–80 % into a crunch amp gives that held-note swell.
- Pair with **Sag**: Sag squashes the attack, Bloom brings the sustain up — a very "alive" amp feel.
- Double-click any knob to reset it; hold Shift while dragging for fine control.

---

*AtiNAMatiC-Bloom v0.2.0 · Atij 666 Studio*
