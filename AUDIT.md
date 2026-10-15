# ONE KNOB Series — Audit & v0.2.0 changes

Audit of all 11 plug-ins: code review, offline measurement on real DI guitar
and a synthetic amp-sim signal, stress testing, and a UI refresh.

## How it was tested

Each plug-in's real sources were compiled into a test harness that can:

- **render** audio through the plug-in at any setting and measure level,
  crest factor, loudness change across the knob, spectral balance, stereo
  correlation and mono-sum comb filtering;
- measure **THD and aliasing** with sine tones (82 Hz, 440 Hz, 3.5 kHz);
- **stress** it: 44.1–192 kHz, mono/stereo, block sizes 1–4096, variable
  and zero-length blocks, silence/DC/impulse/+12 dBFS input, random
  automation, state save/restore (incl. garbage state), re-prepare, CPU;
- **screenshot** the editor while audio plays.

## Findings and fixes

Severity: **High** = the plug-in does not do what it says / can glitch a
session; **Med** = clearly audible flaw; **Low** = polish.

| Plug-in | Sev | Finding (before) | Fix (after) |
|---|---|---|---|
| HitZone | High | Auto gain was open-loop: it compared the *input* with the target and kept adding the same error, so it ran to the ±24 dB limit (measured +24 dB on a quiet DI, output -9 dBFS RMS). | Closed loop on post-gain level; holds in silence; instant peak ceiling + safety knee. Quiet and hot DI now both land at ≈ -21 dB RMS, peaks ≤ -0.8 dBFS. |
| HitZone | Med | Tone shelves advanced one smoothing step per *block* → took ~40 s to reach the Sweet Spot setting. | Smoother advanced by the full block. |
| Level | High | Moving the knob cleared the RMS window → leveler briefly "saw silence" and lurched. 1 dB deadband snapped the target to 0 dB → toggling / "wavy" pumping near target. Gaps were boosted up to +18 dB (noise). | Continuous two-stage detector, 1 dB *hold* band, gain frozen in gaps, smooth safety knee. -10 dB / +6 dB sections now come back to within ~1 dB, gain wobble < 0.4 dB. |
| Tight | High | Heap allocation on the audio thread every block (`AudioBuffer dryCopy` in `processBlock`). | Pre-allocated, chunked processing. |
| Tight | Med | Transient shaper: +9 dB on attacks → peaks +11…+14 dB into the amp; unlinked stereo; ripple at the note frequency. | Linked, shared-release detector, ≈+6 dB max. Peaks now +6…+9 dB. |
| Squash | High | Thresholds assumed a hot signal: on a -18 dB RMS bus Gentle/Standard barely compressed, while the auto make-up added up to +9.5 dB (it was mostly a volume knob). Adaptive make-up (first fix attempt) drifted for ~3 s after loud passages. | Thresholds re-mapped for real bus levels; static make-up calibrated at -18 dBFS RMS → loudness within ±0.5 dB (max -1.3 dB), no drift. |
| Boost | Med | Loudness fell 2.8 dB and peaks 10 dB across the knob (own spec: ±1 dB); +1.8 dB at 0 %; aliasing -23 dB at 3.5 kHz. | 4× oversampled unity-slope shaper, static curve-derived compensation, DC blocker. Now ±1 dB, transparent at 0 %, aliasing -34 dB worst case (-75 dB at mid settings). 38 samples latency, reported. |
| Dirt | Med | Crunch jumped +5.7 dB at 0 %, -5 dB at 100 %; aliasing up to -12 dB; Dirt/Mix/Output unsmoothed; Type switch clicked. | Same engine approach as Boost + Type crossfade + smoothing. Level -0.5…+2.3 dB across the range. |
| Air | High | On a mono amp sim (the normal use) the M/S widening produced **no** side signal (-222 dB): "depth/width" did nothing. Integer Haas delay zippered. Manual claimed it worked on mono. | Mono-safe early reflections written only to the side channel (L+R cancels them exactly). Side now -25…-8 dB, correlation down to 0.72; mono sum unchanged. |
| Feel | Med | Per-sample detectors rippled with the waveform → low-note distortion up to -22 dB THD. 3.5–18 ms attacks let every pick spike through at full make-up → peaks *rose* (the opposite of "tames spikes"). | Peak-hold detector, shared-release transient detector, near-instant attacks, calibrated static make-up. THD now < -60 dB, crest reduced in Tight/Even, level ±1 dB. |
| Pick | Med | Same detector ripple → audible fuzz on sustained low notes (-42 dB THD); Soft lost ~3 dB; Mix/Output unsmoothed. | Shared-release detector (THD -66 dB), calibrated make-up (±0.1 dB across the sweep), smoothing, linked stereo. |
| Double | Med | Pan/voice gains stepped per block (zipper); delay glided from 0 ms on load (pitch bend); 0 % ("MONO") still added a centred comb-filtered copy. | Per-sample smoothing, delays start at target, voices fade out below 15 % (0 % = exact passthrough), per-voice tone. |
| Gate | Med | Adaptive mode only learned the noise floor while closed → if noise held the gate open it never adapted. | Also learns from signal below -50 dBFS. Gaps now ≈ -78 dB even at 0 %. |
| All | Med | Mode/Character/Type switches and Level's Auto toggle never updated after the editor opened (automation, undo, preset recall left the UI stale). | `ParameterAttachment`-based switches. |
| Boost/Dirt/Air/Pick | Low | `*coefs = *makeX(...)` allocated a coefficient object on the audio thread every block. | `ArrayCoefficients` (no allocation). |
| All | Low | Five plug-ins reported themselves to hosts as "NAM Boost/Dirt/Gate/Level/Pick"; Squash/Tight/HitZone showed "0.5000"; Output had no unit. | `AtiNAMatiC-<Name>` everywhere; "72 %", "+1.5 dB" via `Shared/OneKnobParams.h`. |
| Build | Low | `clap-juce-extensions` fetched from moving `main` → builds could break without any change here. | Pinned to commit `7adee3a`. |

**pluginval** (Tracktion, strictness 8, all 11 VST3 builds): 11/11 pass.
**Release builds** (Windows, VST3 + CLAP + Standalone): 11/11 build; v0.2.0
installers built with Inno Setup.

Things that were checked and are fine: state save/restore (all parameters,
including garbage input), NaN/Inf at every sample rate / block size, silence
stays silent, denormals (CPU flat on decaying tails), Gate's lookahead and
reported latency.

## UI

Shared kit in `Shared/OneKnobUI.h`, used by all 11 editors (each editor is now
~40 lines of description instead of ~1,200 lines of duplicated look-and-feel).

- Machined macro knob with printed 0–10 scale, hairline value ring, knurled
  skirt, engraved pointer; each plug-in keeps its background art, accent and
  segmented VU bars.
- Embedded fonts (Barlow Condensed, IBM Plex Mono — both SIL OFL, licences in
  `Shared/Fonts/`) so text looks the same on every OS.
- LED mode switches, meter window with threshold markers / target zones /
  status LED, catalogue number = position in the recommended chain:
  `01 Gate → 02 Tight → 03 Pick → 04 Feel → 05 HitZone → 06 Boost → 07 Dirt → [amp] → 08 Air → 09 Double → 10 Squash → 11 Level`.
- Resizable 75–150 % (remembered in the session), double-click to reset,
  Shift-drag for fine control.

## Not done / recommendations

- **PDF manuals** were regenerated from the updated `.md` files (pandoc + Edge).

- **Listening test.** Everything above is measured, not auditioned. Please
  A/B especially Boost, Dirt, Air (Room) and Squash (Smash) on your own rig.
- **Latency:** Boost and Dirt now report 38 samples (≈0.8 ms @ 48 kHz) for
  oversampling. Gate reports 3 ms lookahead; Slow and Bloom 6 ms. Everything
  else is 0.
- **Squash Smash** still lets the first attack after silence overshoot by up
  to ~+7 dB at high settings (normal for a fast compressor with make-up; it was
  ~+10 dB). A soft output ceiling could be added if you want it bullet-proof.
- **Feel** lifts a quiet DI by up to +7 dB at maximum Assist (that's its job as
  a compressor with make-up, but worth knowing for gain staging).
- **macOS/Linux** were not built locally; CI should be run on the branch.
- Consider code signing (Windows SmartScreen / macOS Gatekeeper) for releases.

## New plug-ins (Tier 1, built from the scaffolds)

The five Tier 1 scaffolds were placeholder DSP (a few lines each, with
`static` state shared across channels and instances). Each was designed and
built properly on the series kit, moved into this workspace, and verified the
same way as the original 11.

| No. | Plug-in | What it does | Verified |
|---|---|---|---|
| 12 | Sag | Tube-rectifier supply sag: droop under hard picking, bloom on recovery (Rectifier / Variac / Bloom) | clean at 0 % (THD -127 dB), level within ±1 dB, zero latency |
| 13 | Strum | Three-band strum shaper: low thump down, mid ring, high strike up (Clean / Crunch / Acoustic) | bands sum to the exact input (0 % = pass-through), level within ±0.2 dB |
| 14 | Cohere | Dynamic bite/fizz tamer relative to the signal's own balance + glue + tape saturation | fizz spikes -5.3 dB vs -0.9 dB on normal playing, level within ±0.7 dB |
| 15 | Reach | Presence + mud dip in the centre, mono lows, widened highs, mono-safe generated spread | mono amp → side up to -7.7 dB (corr 0.71), mono sum unchanged, level ±0.2 dB |
| 16 | Room | 8-line FDN room reverb with ducking (Booth / Studio / Hall) | RT60 0.32 / 0.61 / 1.46 s, L/R decorrelated, duck -8 dB under playing |

All five: harness stress suite pass, pluginval strictness 8 pass, Release
VST3 + CLAP + Standalone build, installers, manuals (md + pdf). 

## New plug-ins (Tier 2 and Tier 3)

These folders were empty (names only), so each plug-in was designed from its
name to fill a gap in the series, then built and verified the same way.

| No. | Plug-in | What it does | Verified |
|---|---|---|---|
| 17 | Bloom | Held notes swell back toward their attack level (Swell / Sustain / Singing) | held notes: sustain +4.3 / +8.6 dB at 50 / 100 %, attack and peaks unchanged |
| 18 | Cab | Filter-based speaker cabinet, MIC cap -> edge, 1x12 / 2x12 / 4x12 | fizz >7 kHz -11 dB (cap) to -29 dB (edge), level within ±1.5 dB |
| 19 | Choke | Note-relative decay shortener (Tight / Staccato / Djent) | late tail -17 / -37 / -40 dB at 100 %, attacks unchanged, THD -127 dB |
| 20 | Mono | Mono bass crossover, full narrowing, loudness-safe mono, phase check | side lows -13 / -25 dB, upper image untouched; Safe within 0.1 dB |
| 21 | Hum | Constant-bandwidth notches at mains harmonics, auto 50/60 Hz | hum -15 / -22 / -31..-38 dB; clean guitar -0.2..-0.3 dB |
| 22 | Pitch | Pitch-synchronous granular shifter: Drop / Detune / Octave | single notes: by-products 35-41 dB down; 0 st exact pass-through |
| 23 | Slow | Auto volume swell, 50 ms - 2 s (Swell / Pad / Reverse) | identical swell on every note (one pick = one swell); pick click hidden by 45-53 dB (6 ms lookahead) |

Found while building these and fixed: a constant-Q notch design that ate
1.3 dB of clean guitar (Hum), grain phase cancellation that turned the octave
into sidebands (Pitch), and an onset detector that could double-trigger and
then stop re-arming on low notes (Slow, Bloom).

After listening tests: Slow sounded like a reverse delay. Its detector fired
only after a pick had started, so the first few ms of every attack passed at
full level and each note ended "swell, then click". It also missed most picks
over strings that were still ringing (full-band level rose only 1.2-2x).
Slow and Bloom now run 6 ms behind their detector (reported latency) and also
detect the pick click above 4 kHz. Synthetic plucks at 120-800 ms spacing over
ringing notes: Slow hides 25 of 26 attacks by 45-53 dB (the old detector caught 10, and
let the click through on all of them); Bloom
boosted new attacks by up to +6.8 dB and now leaves every one at 0.0 dB.
