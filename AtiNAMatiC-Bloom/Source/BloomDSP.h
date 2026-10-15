#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <OneKnobDSP.h>
#include <atomic>
#include <array>
#include <cmath>
#include <vector>

namespace nbl
{
    // Bloom — placed BEFORE the amp model.
    //
    // A loud amp "blooms": hold a note or chord and it seems to grow, getting
    // fuller and richer instead of just decaying. Bloom recreates that:
    //
    //   1. A linked onset detector (fast vs slow envelope, shared release)
    //      finds each pick attack and remembers its level. It also watches
    //      the band above 4 kHz (the pick click), so a new pick over a chord
    //      that is still ringing is caught too.
    //   2. While the note sustains, a gain rises over the BLOOM time toward
    //      "attack level / current level" -- it brings the decaying sustain
    //      back up toward where the note started, never above it.
    //      The knob sets how much of that gap is closed (and the cap, +9 dB).
    //   3. As the note blooms, a little unity-slope saturation is blended in
    //      (richer, not louder); Singing adds an upper-mid resonance that
    //      grows with the bloom, like an amp about to feed back.
    //   4. A new attack resets the bloom (1 ms glide down). The audio runs
    //      6 ms behind the detector (reported as latency), so the reset is
    //      finished before the pick reaches the output: the next pick never
    //      passes boosted.
    //      Below about -50 dBFS the bloom is released: decays into noise are
    //      never lifted.
    //
    // Modes (bloom time): Swell 0.35 s, Sustain 0.9 s, Singing 1.4 s.
    class BloomDSP
    {
    public:
        enum Mode { Swell = 0, Sustain = 1, Singing = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> bloomDb  {   0.0f };   // current bloom gain, dB

        int getLookaheadSamples() const { return lookahead; }

        void prepare (double sr, int /*blockSize*/, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            lookahead  = juce::jmax (1, (int) std::round (0.006 * sr));
            ring.assign ((size_t) (channels * lookahead), 0.0f);
            ringPos = 0;
            hHeld = hFast = hSlow = 0.0f; hHeldLeft = 0; hArmed = true;
            hHeldLen = juce::jmax (1, (int) (0.003 * sr));
            hHeldRel = (float) std::exp (-1.0 / (0.010 * sr));
            {   // 2nd-order Butterworth high-pass at 4 kHz (RBJ)
                const double w0 = juce::MathConstants<double>::twoPi * juce::jmin (4000.0, 0.45 * sr) / sr;
                const double al = std::sin (w0) / (2.0 * 0.70710678), cw = std::cos (w0), a0 = 1.0 + al;
                hb0 = (float) ((1.0 + cw) * 0.5 / a0); hb1 = (float) (-(1.0 + cw) / a0); hb2 = hb0;
                ha1 = (float) (-2.0 * cw / a0);        ha2 = (float) ((1.0 - al) / a0);
                hz1 = hz2 = 0.0f;
            }
            held = 0.0f; heldLeft = 0; heldLen = (int) (0.015 * sr);
            heldRel = (float) std::exp (-1.0 / (0.040 * sr));
            fast = slow = attackPk = 0.0f; age = 1.0e9f; holdLeft = 0; armed = true;
            gDb = 0.0f;
            aFast = coef (0.001); aSlow = coef (0.030); aRel = coef (0.060);
            aDown = coef (0.020);
            aDrop = coef (0.001);
            hSlowA = coef (0.020); hRelA = coef (0.030);
            for (auto& f : reso) f.reset();
            resoAmt = -1.0f;
            mixSm.reset (sr, 0.02); mixSm.setCurrentAndTargetValue (1.0f);
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            envIn = envOut = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int modeIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            amount = juce::jlimit (0.0f, 1.0f, amount);
            const Mode m = (Mode) juce::jlimit (0, 2, modeIn);
            const float bloomSec = m == Swell ? 0.35f : m == Sustain ? 0.9f : 1.4f;
            const float singing  = m == Singing ? 1.0f : 0.0f;
            const float maxDb    = 9.0f * amount;
            const float closeGap = 0.35f + 0.65f * amount;   // fraction of the attack-to-sustain gap restored
            const float invT     = 1.0f / (bloomSec * (float) sampleRate);

            mixSm.setTargetValue (mix);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float* p[8] = {};
            for (int c = 0; c < numCh; ++c) p[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f, gMax = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float det = 0.0f, sum = 0.0f;
                for (int c = 0; c < numCh; ++c) { det = juce::jmax (det, std::abs (p[c][i])); sum += p[c][i]; }

                // high band (pick click), transposed direct form II
                const float xm = sum / (float) numCh;
                const float hp = hb0 * xm + hz1;
                hz1 = hb1 * xm - ha1 * hp + hz2;
                hz2 = hb2 * xm - ha2 * hp;
                const float hd = std::abs (hp);
                if (hd >= hHeld) { hHeld = hd; hHeldLeft = hHeldLen; }
                else if (hHeldLeft > 0) --hHeldLeft;
                else hHeld = juce::jmax (hd, hHeld * hHeldRel);
                hFast += (hHeld - hFast) * (hHeld > hFast ? aFast  : hRelA);
                hSlow += (hHeld - hSlow) * (hHeld > hSlow ? hSlowA : hRelA);
                if (hFast < hSlow * 1.3f) hArmed = true;

                // --- onset detection (shared release => sustain stays quiet)
                // Peak-hold the level first (~15 ms, a full low-E cycle) so the
                // envelopes below see a smooth level, not the waveform: during
                // sustain they converge and the detector re-arms cleanly.
                if (det >= held) { held = det; heldLeft = heldLen; }
                else if (heldLeft > 0) --heldLeft;
                else held = juce::jmax (det, held * heldRel);

                fast += (held - fast) * (held > fast ? aFast : aRel);
                slow += (held - slow) * (held > slow ? aSlow : aRel);
                // Rising edge only (re-armed once the attack settles), so one
                // pick can never retrigger the bloom.
                if (fast < slow * 1.3f) armed = true;
                const bool fullHit = armed  && fast  > slow  * 1.7f;
                const bool highHit = hArmed && hFast > hSlow * 2.5f && hFast > fast * 0.01f;
                const bool onset = holdLeft <= 0 && fast > 0.003f && (fullHit || highHit);
                if (onset)
                {
                    age = 0.0f; attackPk = fast; holdLeft = (int) (0.05 * sampleRate); armed = hArmed = false;
                }
                if (holdLeft > 0) { --holdLeft; attackPk = juce::jmax (attackPk, fast); }
                age += 1.0f;

                // --- bloom target ----------------------------------------
                float target = 0.0f;
                if (fast > 0.003f && attackPk > 0.0f && age > 0.03f * (float) sampleRate)
                {
                    const float gapDb  = juce::Decibels::gainToDecibels (attackPk / juce::jmax (fast, 1.0e-6f));
                    const float ramp   = 1.0f - std::exp (-(age - 0.03f * (float) sampleRate) * invT);
                    target = juce::jlimit (0.0f, maxDb, gapDb * closeGap) * ramp;
                }
                // up slowly (the ramp already shapes it), down on new attacks / gate
                // A new attack drops the bloom almost at once (1 ms, inside the
                // 6 ms lookahead) so the next pick never passes boosted;
                // otherwise release gently.
                const float down = holdLeft > 0 ? aDrop : aDown;
                gDb += (target - gDb) * (target > gDb ? 0.01f : down);
                gDb = juce::jmax (0.0f, gDb);
                const float g = juce::Decibels::decibelsToGain (gDb);
                const float bloomNorm = juce::jlimit (0.0f, 1.0f, gDb / 9.0f);

                // Singing resonance follows the bloom (coefficients at a slow rate)
                const float rTarget = singing * bloomNorm * 3.5f * amount;
                if (std::abs (rTarget - resoAmt) > 0.05f && (i & 31) == 0)
                {
                    resoAmt = rTarget;
                    const auto rc = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
                        sampleRate, 1600.0f, 1.4f, juce::Decibels::decibelsToGain (resoAmt));
                    for (int c = 0; c < numCh; ++c) *reso[(size_t) c].coefficients = rc;
                }

                const float satMix = bloomNorm * 0.6f * amount;
                const float mx = mixSm.getNextValue();
                const float og = outSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    float& slot = ring[(size_t) (c * lookahead + ringPos)];
                    const float x = slot;          // the input from 6 ms ago
                    slot = p[c][i];
                    float y = x * g;
                    const float s = std::tanh (1.8f * y) / 1.8f;
                    y = y + (s - y) * satMix;
                    if (singing > 0.0f) y = reso[(size_t) c].processSample (y);
                    const float out = (y * og) * mx + x * (1.0f - mx);
                    p[c][i] = out;
                    pkOut = juce::jmax (pkOut, std::abs (out));
                }
                if (++ringPos >= lookahead) ringPos = 0;
                pkIn = juce::jmax (pkIn, det);
                gMax = juce::jmax (gMax, gDb);
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            bloomDb.store  (gMax, std::memory_order_relaxed);
        }

    private:
        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        float  held { 0 }, heldRel { 0.999f };
        int    heldLeft { 0 }, heldLen { 0 };
        float  fast { 0 }, slow { 0 }, attackPk { 0 }, age { 1.0e9f }, gDb { 0 };
        int    holdLeft { 0 };
        bool   armed { true };
        float  aFast { 0 }, aSlow { 0 }, aRel { 0 }, aDown { 0 }, aDrop { 0 };
        float  resoAmt { -1.0f };
        int    lookahead { 288 }, ringPos { 0 };
        std::vector<float> ring;
        float  hHeld { 0 }, hHeldRel { 0.99f }, hFast { 0 }, hSlow { 0 }, hSlowA { 0 }, hRelA { 0 };
        int    hHeldLeft { 0 }, hHeldLen { 1 };
        bool   hArmed { true };
        float  hb0 { 0 }, hb1 { 0 }, hb2 { 0 }, ha1 { 0 }, ha2 { 0 }, hz1 { 0 }, hz2 { 0 };
        std::array<juce::dsp::IIR::Filter<float>, 8> reso;
        juce::SmoothedValue<float> mixSm, outSm;
        float  envIn { 0 }, envOut { 0 };
    };
}
