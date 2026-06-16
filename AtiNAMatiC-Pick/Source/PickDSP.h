#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace np
{
    // Pick Attack Designer.
    //
    //   Per channel, per sample:
    //     1) Two one-pole envelope followers
    //          fast : ~2 ms attack, ~30 ms release
    //          slow : ~30 ms attack, ~200 ms release
    //        Transient strength T = max (0, fast - slow), normalised so that
    //        a typical guitar attack peaks T near 1.0.
    //
    //     2) Per-sample gain shaping (the heart of the effect)
    //          A = attack knob mapped to -1..+1, neutral at 0
    //          g = 1 + A * kStrength * T
    //        Sharp (A>0) : transient peaks get amplified  (snap, click)
    //        Soft  (A<0) : transient peaks get attenuated (legato feel)
    //        Sustained material (T~0) is untouched.
    //
    //     3) Static tilt EQ that scales with A (very subtle: ±2 dB max)
    //          Sharp : +1.5 dB high-shelf @ 4 kHz, -1.0 dB low-shelf @ 120 Hz
    //          Soft  : -1.0 dB high-shelf @ 4 kHz, +1.0 dB low-shelf @ 200 Hz
    //        Implemented as one peak filter per channel, retuned per block.
    //
    //     4) Loudness make-up: tiny constant trim to keep perceived level
    //        roughly equal across the sweep.
    //
    //   Final blend: dry/wet with the user's MIX, then OUTPUT trim in dB.
    class PickDSP
    {
    public:
        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> transientDisp {  0.0f };  // 0..1 visualisation

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            // Envelope time constants (ms -> per-sample one-pole coeffs).
            fastAtt = oneOverTau (1.5,   sr);
            fastRel = oneOverTau (25.0,  sr);
            slowAtt = oneOverTau (40.0,  sr);
            slowRel = oneOverTau (220.0, sr);

            for (auto& e : fastEnv) e = 0.0f;
            for (auto& e : slowEnv) e = 0.0f;

            // Tilt EQ — start neutral. Coefficients shared, updated in
            // place; each per-channel filter holds a ref-counted pointer
            // and copies values from the shared object on prepare.
            highShelfCoefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                                 sr, 4000.0f, 0.7f, 1.0f);
            lowShelfCoefs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                                 sr, 160.0f,  0.7f, 1.0f);

            for (int c = 0; c < (int) highShelf.size(); ++c)
            {
                highShelf[c].reset();
                lowShelf[c].reset();
                highShelf[c].coefficients = highShelfCoefs;
                lowShelf[c].coefficients  = lowShelfCoefs;
            }

            envInDisp  = 0.0f;
            envOutDisp = 0.0f;
            transientShown = 0.0f;
            currentAttack = 1.0e9f; // force first update

            // Per-sample smoothing of the bipolar attack value feeding the
            // gain-shaping term -- a fast knob move (especially crossing
            // through neutral) would otherwise step/click every block.
            smA = 0.0f;
            smACoef = oneOverTau (15.0, sr);
        }

        // attack: 0..1, mapped to -1..+1
        // mix:    0..1
        // outDb:  -24..+12
        void process (juce::AudioBuffer<float>& buffer,
                      float attack01, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const float A = juce::jlimit (-1.0f, 1.0f, attack01 * 2.0f - 1.0f);

            // Update tilt EQ if attack has moved enough.
            if (std::abs (A - currentAttack) > 0.001f)
            {
                updateTiltEq (A);
                currentAttack = A;
            }

            // Strength: how much the transient envelope modulates gain.
            // ±0.7 means a transient with T=1 swings gain ~+/-0.7 (i.e. a
            // strong but not destructive shape). T is roughly bounded to
            // ~1 for realistic input.
            const float kStrength = 0.7f;

            // Tiny loudness make-up: the gain modulation is roughly
            // zero-mean over time, but the EQ tilt slightly changes
            // perceived level. Compensate gently.
            const float makeUpDb = -0.5f * std::abs (A);
            const float outGain  = juce::Decibels::decibelsToGain (makeUpDb + outDb);

            const float wet = mix;
            const float dry = 1.0f - mix;

            const int chClamp = juce::jmin (numCh, 8);
            float* wp[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c) wp[c] = buffer.getWritePointer (c);

            const float envDispA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));
            float pkIn = 0.0f, pkOut = 0.0f, transAcc = 0.0f;
            float transPk = 0.0f;

            // Envelope normaliser: T = clamp((fast - slow) / (slow + eps), 0..1)
            // Using ratio rather than raw diff keeps the response invariant
            // to input level.
            const float eps = 1.0e-4f;

            for (int i = 0; i < n; ++i)
            {
                // Use channel-0 envelope to drive the gain on all channels —
                // keeps stereo image stable, and is what most pick-attack
                // tools do.
                const float xMono = wp[0][i];
                const float aMono = std::abs (xMono);

                // Fast envelope (true peak follower)
                {
                    float& e = fastEnv[0];
                    const float coef = (aMono > e) ? fastAtt : fastRel;
                    e += (aMono - e) * coef;
                }
                // Slow envelope
                {
                    float& e = slowEnv[0];
                    const float coef = (aMono > e) ? slowAtt : slowRel;
                    e += (aMono - e) * coef;
                }

                const float fastV = fastEnv[0];
                const float slowV = slowEnv[0];
                float T = (fastV - slowV) / (slowV + eps);
                T = juce::jlimit (0.0f, 2.0f, T) * 0.5f; // map ~0..1 typical
                if (T > transPk) transPk = T;
                transAcc += T;

                smA += (A - smA) * smACoef;
                const float g = 1.0f + smA * kStrength * T;

                for (int c = 0; c < chClamp; ++c)
                {
                    const float xRaw = wp[c][i];

                    // Per-sample gain shaping.
                    float y = xRaw * g;

                    // Tilt EQ (per channel state).
                    y = highShelf[c].processSample (y);
                    y = lowShelf [c].processSample (y);

                    // Output trim + dry/wet.
                    const float yOut = y * outGain;
                    const float mixed = xRaw * dry + yOut * wet;
                    wp[c][i] = mixed;

                    if (c == 0)
                    {
                        const float ai = std::abs (xRaw);
                        const float ao = std::abs (mixed);
                        if (ai > pkIn)  pkIn  = ai;
                        if (ao > pkOut) pkOut = ao;
                    }
                }
            }

            envInDisp  += (pkIn  - envInDisp)  * envDispA;
            envOutDisp += (pkOut - envOutDisp) * envDispA;
            const float tAvg = transAcc / juce::jmax (1, n);
            // Peak hold: jump instantly to the block's peak transient,
            // then decay with a proper per-block coefficient (~120 ms).
            // This keeps the value elevated long enough for the 30 Hz UI
            // timer to read it reliably.
            const float peakTarget  = juce::jmin (1.0f, juce::jmax (transPk, tAvg * 4.0f));
            const float relPerBlock = (float) std::exp (-(double)n / (0.120 * sampleRate));
            if (peakTarget > transientShown)
                transientShown = peakTarget;        // instant grab
            else
                transientShown *= relPerBlock;      // ~120 ms per-block decay

            inputDb.store      (juce::Decibels::gainToDecibels (envInDisp,  -90.0f),
                                std::memory_order_relaxed);
            outputDb.store     (juce::Decibels::gainToDecibels (envOutDisp, -90.0f),
                                std::memory_order_relaxed);
            transientDisp.store (juce::jlimit (0.0f, 1.0f, transientShown),
                                std::memory_order_relaxed);
        }

    private:
        static float oneOverTau (double ms, double sr)
        {
            // One-pole coeff for time-constant tau (ms): 1 - exp(-1 / (tau*sr)).
            return (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr)));
        }

        void updateTiltEq (float A)
        {
            // Sharp (A>0): subtle bright tilt.
            // Soft  (A<0): subtle warm tilt.
            // Maxima are ±1.5 dB on the high shelf and ±1.0 dB on the low
            // shelf — meant to colour feel, not tone.
            const float hsDb = A * 1.5f;
            const float lsDb = -A * 1.0f;        // soft side adds low body

            *highShelfCoefs = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, 4000.0f, 0.7f, juce::Decibels::decibelsToGain (hsDb));
            *lowShelfCoefs  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                sampleRate, A > 0 ? 120.0f : 200.0f,
                0.7f, juce::Decibels::decibelsToGain (lsDb));
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };

        float fastAtt { 0.0f }, fastRel { 0.0f };
        float slowAtt { 0.0f }, slowRel { 0.0f };
        std::array<float, 8> fastEnv { };
        std::array<float, 8> slowEnv { };

        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> highShelfCoefs, lowShelfCoefs;
        std::array<juce::dsp::IIR::Filter<float>, 8> highShelf;
        std::array<juce::dsp::IIR::Filter<float>, 8> lowShelf;

        float currentAttack { 1.0e9f };

        // Per-sample smoothed bipolar attack value (drives gain shaping).
        float smA     { 0.0f };
        float smACoef { 0.0f };

        float envInDisp     { 0.0f };
        float envOutDisp    { 0.0f };
        float transientShown { 0.0f };
    };
}
