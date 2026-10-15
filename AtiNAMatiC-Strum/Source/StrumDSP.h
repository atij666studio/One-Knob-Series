#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace nst
{
    // Strum shaper — for strummed chords (clean, crunch, acoustic).
    //
    // A full-band transient shaper treats a strum as one event: lift the
    // attack and the low "thump" of the pick hitting six strings comes up
    // with the sparkle. Strum splits the signal into three bands and shapes
    // each one differently:
    //
    //   LOW  (< 250 Hz)   : attack pulled DOWN -- tames boom / thump
    //   MID  (250-2.5 k)  : a little attack and a little extra ring
    //   HIGH (> 2.5 kHz)  : attack pushed UP -- string definition, sparkle
    //
    // Subtractive crossovers: low = LP(x), rest = x - low, mid = LP(rest),
    // high = rest - mid. The three bands add back to exactly the input, so
    // at 0 % (or with MIX at 0) the signal passes untouched -- no phase
    // rotation in front of the amp, and blending never combs.
    //
    // Each band has a linked (all channels) transient detector: fast vs slow
    // attack with a SHARED release, so sustained notes leave it at rest and
    // only real attacks are shaped (no gain ripple / fuzz on low notes).
    //
    // Zero latency.
    class StrumDSP
    {
    public:
        enum Mode { Clean = 0, Crunch = 1, Acoustic = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> strike   {   0.0f };   // 0..1 high-band attack activity
        std::atomic<float> thumpDb  {   0.0f };   // low-band attack cut, positive dB

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            const juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, blockSize), (juce::uint32) channels };

            auto setup = [&] (juce::dsp::LinkwitzRileyFilter<float>& f, juce::dsp::LinkwitzRileyFilterType t, float hz)
            {
                f.prepare (spec); f.setType (t); f.setCutoffFrequency (hz); f.reset();
            };
            setup (xLow,  juce::dsp::LinkwitzRileyFilterType::lowpass, 250.0f);
            setup (xHigh, juce::dsp::LinkwitzRileyFilterType::lowpass, 2500.0f);

            const float aF = coef (0.001), aS = coef (0.025), aR = coef (0.100), aG = coef (0.0007);
            for (auto& b : bands) { b = {}; b.aFast = aF; b.aSlow = aS; b.aRel = aR; b.aGain = aG; }

            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            amtSm.reset (sr, 0.03);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
            amtSm.setCurrentAndTargetValue (0.0f);
            envIn = envOut = strikeEnv = thumpEnv = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int modeIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const Mode m = (Mode) juce::jlimit (0, 2, modeIn);
            // Per-band targets at 100 %: { attack dB, sustain dB }
            Shape low { -5.0f, 0.0f }, mid { 1.5f, 1.0f }, high { 5.0f, 0.0f };
            switch (m)
            {
                case Crunch:   low = { -6.0f, -0.5f }; mid = { 2.0f, 0.0f }; high = { 3.5f, 0.0f }; break;
                case Acoustic: low = { -5.0f,  0.0f }; mid = { 1.0f, 2.0f }; high = { 6.0f, 0.5f }; break;
                default: break;
            }

            amtSm.setTargetValue (juce::jlimit (0.0f, 1.0f, amount));
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));
            mixSm.setTargetValue (mix);

            float* ptr[8] = {};
            for (int c = 0; c < numCh; ++c) ptr[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f, strikeMax = 0.0f, thumpMax = 0.0f;
            float bl[8], bm[8], bh[8];

            for (int i = 0; i < n; ++i)
            {
                float dL = 0.0f, dM = 0.0f, dH = 0.0f, dIn = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    const float x = ptr[c][i];
                    bl[c] = xLow.processSample (c, x);
                    const float rest = x - bl[c];
                    bm[c] = xHigh.processSample (c, rest);
                    bh[c] = rest - bm[c];
                    dL = juce::jmax (dL, std::abs (bl[c]));
                    dM = juce::jmax (dM, std::abs (bm[c]));
                    dH = juce::jmax (dH, std::abs (bh[c]));
                    dIn = juce::jmax (dIn, std::abs (x));
                }

                const float a  = amtSm.getNextValue();
                const float gL = bands[0].process (dL, low,  a);
                const float gM = bands[1].process (dM, mid,  a);
                const float gH = bands[2].process (dH, high, a);
                const float og = outGainSm.getNextValue();
                // Make-up: the shaping adds ~2 dB at 100 % on DI guitar.
                const float makeup = 1.0f - 0.205f * a;
                const float mx = mixSm.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    const float dry = ptr[c][i];   // == bl + bm + bh exactly
                    const float wet = (bl[c] * gL + bm[c] * gM + bh[c] * gH) * og * makeup;
                    const float y   = wet * mx + dry * (1.0f - mx);
                    ptr[c][i] = y;
                    pkOut = juce::jmax (pkOut, std::abs (y));
                }
                pkIn      = juce::jmax (pkIn, dIn);
                strikeMax = juce::jmax (strikeMax, bands[2].attack);
                thumpMax  = juce::jmax (thumpMax, -bands[0].gainDb);
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            strikeEnv = juce::jmax (strikeMax, strikeEnv * (1.0f - k));
            thumpEnv  = juce::jmax (thumpMax,  thumpEnv  * (1.0f - k));
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            strike.store   (juce::jlimit (0.0f, 1.0f, strikeEnv), std::memory_order_relaxed);
            thumpDb.store  (juce::jmax (0.0f, thumpEnv), std::memory_order_relaxed);
        }

    private:
        struct Shape { float attackDb, sustainDb; };

        struct Band
        {
            float fast { 0.0f }, slow { 0.0f }, gainDb { 0.0f }, attack { 0.0f };
            float aFast { 0.0f }, aSlow { 0.0f }, aRel { 0.0f }, aGain { 0.0f };

            float process (float det, const Shape& s, float amount)
            {
                fast += (det - fast) * (det > fast ? aFast : aRel);
                slow += (det - slow) * (det > slow ? aSlow : aRel);
                const float norm   = juce::jmax (0.0f, fast - slow) / juce::jmax (slow, 1.0e-5f);
                attack             = norm / (1.0f + norm);
                const float steady = 1.0f - juce::jmin (1.0f, norm * 2.0f);
                const float target = amount * (s.attackDb * attack + s.sustainDb * steady);
                gainDb += (target - gainDb) * aGain;
                return juce::Decibels::decibelsToGain (gainDb);
            }
        };

        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::dsp::LinkwitzRileyFilter<float> xLow, xHigh;
        std::array<Band, 3> bands;
        juce::SmoothedValue<float> outGainSm, mixSm, amtSm;
        float envIn { 0.0f }, envOut { 0.0f }, strikeEnv { 0.0f }, thumpEnv { 0.0f };
    };
}
