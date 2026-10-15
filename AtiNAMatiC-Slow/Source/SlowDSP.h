#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace nsl
{
    // Slow — automatic volume swells (the "slow gear" / violin effect).
    //
    //   1. Linked onset detector: fast vs slow envelope with a shared
    //      release, rising-edge triggering (re-armed only once the attack
    //      has settled), a 60 ms hold, and a -50 dBFS floor for noise.
    //   2. On each new attack the gain dips to silence in 3 ms (so the last
    //      note's tail doesn't click) and then fades the new note in over the
    //      SWELL TIME (50 ms -> 2 s, log), along the chosen curve.
    //   3. MIX lets some of the pick attack through on top.
    //
    // Curves: Swell (smooth S-curve), Pad (slow linear-in-dB rise, softest
    // start), Reverse (holds back then rushes in at the end, like a
    // reversed note). Zero latency.
    class SlowDSP
    {
    public:
        enum Curve { Swell = 0, Pad = 1, Reverse = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> swellAmt {   1.0f };   // current swell gain 0..1
        std::atomic<float> swellMs  { 300.0f };

        void prepare (double sr, int /*blockSize*/, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            held = 0.0f; heldLeft = 0; heldLen = (int) (0.015 * sr);
            heldRel = (float) std::exp (-1.0 / (0.040 * sr));
            fast = slow = 0.0f; holdLeft = 0; armed = true;
            prog = 1.0f; dip = 1.0f; dipping = false;
            aFast = coef (0.001); aSlow = coef (0.030); aRel = coef (0.080);
            dipStep = (float) (1.0 / (0.003 * sr));
            mixSm.reset (sr, 0.02); mixSm.setCurrentAndTargetValue (1.0f);
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            envIn = envOut = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int curveIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            amount = juce::jlimit (0.0f, 1.0f, amount);
            const Curve cv = (Curve) juce::jlimit (0, 2, curveIn);
            const float ms   = 50.0f * std::pow (40.0f, amount);          // 50 ms .. 2 s
            const float step = 1.0f / (ms * 0.001f * (float) sampleRate);

            mixSm.setTargetValue (mix);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float* p[8] = {};
            for (int c = 0; c < numCh; ++c) p[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f, gLast = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                float det = 0.0f;
                for (int c = 0; c < numCh; ++c) det = juce::jmax (det, std::abs (p[c][i]));

                // Peak-hold the level first (~15 ms, a full low-E cycle) so the
                // envelopes below see a smooth level, not the waveform: during
                // sustain they converge and the detector re-arms cleanly.
                if (det >= held) { held = det; heldLeft = heldLen; }
                else if (heldLeft > 0) --heldLeft;
                else held = juce::jmax (det, held * heldRel);

                fast += (held - fast) * (held > fast ? aFast : aRel);
                slow += (held - slow) * (held > slow ? aSlow : aRel);
                if (holdLeft > 0) --holdLeft;
                // Rising edge only: re-arm once the attack has settled, so one
                // pick can never trigger twice (on low notes the slow
                // envelope only catches up a little each cycle).
                if (fast < slow * 1.3f) armed = true;
                if (armed && holdLeft <= 0 && fast > 0.003f && fast > slow * 1.8f)
                {
                    dipping = true; armed = false; holdLeft = (int) (0.06 * sampleRate);
                }

                if (dipping)
                {
                    dip -= dipStep;
                    if (dip <= 0.0f) { dip = 0.0f; dipping = false; prog = 0.0f; }
                }
                else
                {
                    dip = 1.0f;
                    prog = juce::jmin (1.0f, prog + step);
                }

                float shape;
                switch (cv)
                {
                    case Pad:     shape = prog >= 1.0f ? 1.0f : juce::Decibels::decibelsToGain (-60.0f * (1.0f - prog)); break;
                    case Reverse: shape = prog * prog * prog; break;
                    case Swell:
                    default:      shape = prog * prog * (3.0f - 2.0f * prog); break;
                }
                // during the 3 ms dip, fade down from wherever the curve was
                const float g = dipping ? juce::jmin (dip, shape) : shape;

                const float mx = mixSm.getNextValue();
                const float og = outSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    const float x = p[c][i];
                    const float y = (x * g * mx + x * (1.0f - mx)) * og;
                    p[c][i] = y;
                    pkOut = juce::jmax (pkOut, std::abs (y));
                }
                pkIn  = juce::jmax (pkIn, det);
                gLast = g;
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            swellAmt.store (gLast, std::memory_order_relaxed);
            swellMs.store  (ms, std::memory_order_relaxed);
        }

    private:
        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        float  held { 0 }, heldRel { 0.999f };
        int    heldLeft { 0 }, heldLen { 0 };
        float  fast { 0 }, slow { 0 };
        int    holdLeft { 0 };
        float  prog { 1.0f }, dip { 1.0f }, dipStep { 0.01f };
        bool   dipping { false }, armed { true };
        float  aFast { 0 }, aSlow { 0 }, aRel { 0 };
        juce::SmoothedValue<float> mixSm, outSm;
        float  envIn { 0 }, envOut { 0 };
    };
}
