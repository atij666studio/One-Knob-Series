#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <OneKnobDSP.h>
#include <atomic>
#include <cmath>

namespace ns
{
    // Power-supply sag — placed BEFORE the amp model.
    //
    // A tube rectifier can't hold its voltage when you dig in: the supply
    // droops, the amp loses a little gain and headroom, then recovers over a
    // few hundred milliseconds. That droop-and-recover is what players call
    // "sag" and "bloom": hard notes squash slightly and then swell.
    //
    // Per sample (one linked detector for all channels):
    //   1. Supply envelope: peak follower with a slow-ish attack (the
    //      supply can't collapse instantly) and slow recovery.
    //   2. Gain droop: 0 dB below -36 dBFS, rising to the full SAG depth at
    //      -6 dBFS. Smoothed again (5 ms) so it never zips.
    //   3. Lowered headroom: a unity-slope soft clipper whose drive and
    //      asymmetry grow with the droop (a starved supply clips earlier and
    //      less symmetrically), followed by a DC blocker.
    //   4. Static make-up (calibrated on DI guitar) so the knob changes feel,
    //      not level. Then MIX and OUTPUT.
    //
    // Modes:
    //   Rectifier : classic tube-rectifier sag. 20 ms droop / 180 ms recovery.
    //   Variac    : run at low voltage: softer, earlier clipping, slower.
    //   Bloom     : fast droop, long recovery: notes dip then swell back.
    //
    // Zero latency.
    class SagDSP
    {
    public:
        enum Mode { Rectifier = 0, Variac = 1, Bloom = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> sagDb    {   0.0f };   // current droop, positive dB

        void prepare (double sr, int /*blockSize*/, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            env = grDb = grSm = 0.0f;
            for (auto& d : dc) d.prepare (sr);
            grSmA = coef (0.005);
            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
            envIn = envOut = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float sag, int modeIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            sag = juce::jlimit (0.0f, 1.0f, sag);
            const Mode m = (Mode) juce::jlimit (0, 2, modeIn);

            float atkMs = 20.0f, relMs = 180.0f, maxDroopDb = 9.0f, headroom = 1.0f;
            switch (m)
            {
                case Variac: atkMs = 40.0f; relMs = 320.0f; maxDroopDb = 6.0f;  headroom = 1.8f; break;
                case Bloom:  atkMs = 6.0f;  relMs = 450.0f; maxDroopDb = 12.0f; headroom = 0.8f; break;
                default: break;
            }
            const float aAtk  = coef (atkMs * 0.001);
            const float aRel  = coef (relMs * 0.001);
            const float depth = sag * maxDroopDb;
            // The headroom curve fades in over the first third of the knob,
            // so SAG at 0 is a clean pass-through.
            const float shapeAmt = juce::jmin (1.0f, sag * 3.0f);

            // Static make-up: the droop a typical picked note (-12 dBFS
            // peaks) gets, given back -- calibrated so the sweep stays level.
            const float refNorm  = juce::jlimit (0.0f, 1.0f, (kMakeupRefDb - kKneeLoDb) / (kKneeHiDb - kKneeLoDb));
            const float makeupDb = depth * refNorm * kMakeupAmount + sag * headroom * 0.6f;
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (makeupDb + outDb));
            mixSm.setTargetValue (mix);

            float* ptr[8] = {};
            for (int c = 0; c < numCh; ++c) ptr[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f, grMax = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float det = 0.0f;
                for (int c = 0; c < numCh; ++c) det = juce::jmax (det, std::abs (ptr[c][i]));

                // 1) supply envelope
                env += (det - env) * (det > env ? aAtk : aRel);

                // 2) droop
                const float envDb = juce::Decibels::gainToDecibels (env, -120.0f);
                const float load  = juce::jlimit (0.0f, 1.0f, (envDb - kKneeLoDb) / (kKneeHiDb - kKneeLoDb));
                grDb = -depth * load;
                grSm += (grDb - grSm) * grSmA;
                const float g = juce::Decibels::decibelsToGain (grSm);

                // 3) lowered headroom: drive and asymmetry follow the droop
                const float starve = sag * headroom * (0.35f + 0.65f * load);
                const float drive  = 0.5f + starve * 2.0f;   // +6 dB headroom at rest
                const float bias   = starve * 0.06f;
                const float tb     = std::tanh (bias);
                const float norm   = 1.0f / (drive * (1.0f - tb * tb));

                const float og  = outGainSm.getNextValue();
                const float mx  = mixSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    const float x   = ptr[c][i];
                    const float xg  = x * g;
                    const float sat = xg + ((std::tanh (drive * xg + bias) - tb) * norm - xg) * shapeAmt;
                    const float wet = dc[(size_t) c].process (sat) * og;
                    const float y   = wet * mx + x * (1.0f - mx);
                    ptr[c][i] = y;
                    pkOut = juce::jmax (pkOut, std::abs (y));
                }
                pkIn  = juce::jmax (pkIn, det);
                grMax = juce::jmin (grMax, grSm);
            }

            const float a = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * a;
            envOut += (pkOut - envOut) * a;
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            sagDb.store    (-grMax, std::memory_order_relaxed);
        }

    private:
        static constexpr float kKneeLoDb     = -36.0f;
        static constexpr float kKneeHiDb     =  -6.0f;
        static constexpr float kMakeupRefDb  = -14.0f;
        static constexpr float kMakeupAmount =  1.0f;

        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        float  env { 0.0f }, grDb { 0.0f }, grSm { 0.0f }, grSmA { 0.0f };
        std::array<ok::DcBlocker, 8> dc;
        juce::SmoothedValue<float> outGainSm, mixSm;
        float envIn { 0.0f }, envOut { 0.0f };
    };
}
