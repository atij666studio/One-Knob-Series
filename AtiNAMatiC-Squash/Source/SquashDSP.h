#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace sq
{
    enum class Mode { Gentle = 0, Standard = 1, Smash = 2 };

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    // ---- Feedforward RMS compressor, stereo-linked ----
    class SquashDSP
    {
    public:
        // Live params set each block by the processor
        std::atomic<float> squash { 0.5f };
        std::atomic<int>   modeIx { 1 };

        // Output — gain reduction in dB (≤ 0), read by the GR meter
        std::atomic<float> grDb { 0.0f };

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            for (auto& cs : chState) { cs.meanSq = 0.0f; cs.gainSmooth = 1.0f; }

            // Block-rate param smoother (~40 ms time constant)
            smCoef = std::exp (-float (blockSize) / float (0.040 * sr));
            smThresh  = -18.0f; smRatio = 4.0f;
            smAttMs   = 10.0f;  smRelMs = 100.0f;
            smKneeDb  =  4.0f;  smMakeup = 6.0f;

            // GR display ballistic coefficients computed per-block in process()
            grDisplay = 0.0f;

            // 10 ms RMS window
            rmsCoef = std::exp (-1.0f / float (0.010 * sr));
        }

        void reset()
        {
            for (auto& cs : chState) { cs.meanSq = 0.0f; cs.gainSmooth = 1.0f; }
            grDisplay = 0.0f;
            grDb.store (0.0f);
        }

        // In-place compression.
        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (buffer.getNumChannels(), channels);
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const float s = juce::jlimit (0.0f, 1.0f, squash.load());
            const Mode  m = (Mode) juce::jlimit (0, 2, modeIx.load());

            float threshDb, ratio, attackMs, releaseMs, kneeDb, makeupDb;
            mapKnob (s, m, threshDb, ratio, attackMs, releaseMs, kneeDb, makeupDb);

            // Smooth param targets at block rate (no coefficient-jump clicks)
            smThresh  = smCoef * smThresh  + (1.0f - smCoef) * threshDb;
            smRatio   = smCoef * smRatio   + (1.0f - smCoef) * ratio;
            smAttMs   = smCoef * smAttMs   + (1.0f - smCoef) * attackMs;
            smRelMs   = smCoef * smRelMs   + (1.0f - smCoef) * releaseMs;
            smKneeDb  = smCoef * smKneeDb  + (1.0f - smCoef) * kneeDb;
            smMakeup  = smCoef * smMakeup  + (1.0f - smCoef) * makeupDb;

            const float attCoef  = std::exp (-1.0f / float (smAttMs  * 0.001 * sampleRate));
            const float relCoef  = std::exp (-1.0f / float (smRelMs  * 0.001 * sampleRate));
            const float makeup   = dbToGain (smMakeup);
            const float halfKnee = smKneeDb * 0.5f;

            float peakGrDb = 0.0f;   // worst GR this block

            for (int i = 0; i < n; ++i)
            {
                // ---- RMS detection: stereo-linked (use max across channels) ----
                float det = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float x = buffer.getReadPointer (ch)[i];
                    auto& cs = chState[(size_t) juce::jmin (ch, (int) chState.size() - 1)];
                    cs.meanSq = rmsCoef * cs.meanSq + (1.0f - rmsCoef) * x * x;
                    det = juce::jmax (det, cs.meanSq);
                }

                // Input level in dBFS (10*log10 of mean-square = 20*log10 of RMS)
                const float inputDb = 10.0f * std::log10 (juce::jmax (det, 1.0e-10f));

                // ---- Gain computer with soft knee ----
                float gainReductionDb = 0.0f;
                const float overDb = inputDb - smThresh;
                if (overDb >= halfKnee)
                {
                    // Above knee — hard gain reduction
                    gainReductionDb = overDb * (1.0f / smRatio - 1.0f);
                }
                else if (overDb > -halfKnee && smKneeDb > 0.001f)
                {
                    // Inside knee — quadratic blend
                    const float knee = overDb + halfKnee;
                    gainReductionDb  = knee * knee * (1.0f / smRatio - 1.0f)
                                       / (2.0f * smKneeDb);
                }
                // gainReductionDb is always ≤ 0

                // ---- Per-sample attack / release gain smoothing ----
                // Makeup applied AFTER the smoother so mode switches don't
                // route the makeup delta through the attack/release envelope.
                const float targetGr = dbToGain (gainReductionDb);

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto& cs = chState[(size_t) juce::jmin (ch, (int) chState.size() - 1)];
                    // attack when we need more compression (gain drops), release otherwise
                    const float coef = (targetGr < cs.gainSmooth) ? attCoef : relCoef;
                    cs.gainSmooth = coef * cs.gainSmooth + (1.0f - coef) * targetGr;
                    buffer.getWritePointer (ch)[i] *= cs.gainSmooth * makeup;
                }

                peakGrDb = juce::jmin (peakGrDb, gainReductionDb);
            }

            // ---- GR display ballistic: instant attack, 100 ms release ----
            // Coef computed from actual block size so it's correct at any buffer size.
            const float dispRelCoef = std::exp (-float (n) / float (0.100 * sampleRate));
            if (peakGrDb < grDisplay)
                grDisplay = peakGrDb;   // instant attack
            else
                grDisplay = dispRelCoef * grDisplay + (1.0f - dispRelCoef) * 0.0f; // release to 0

            grDb.store (grDisplay);
        }

    private:
        // Maps the SQUASH knob (0..1) to per-stage compressor parameters.
        // smoothstep applied so the low half of the knob is more gentle.
        static void mapKnob (float s, Mode m,
                             float& threshDb, float& ratio,
                             float& attackMs, float& releaseMs,
                             float& kneeDb,   float& makeupDb)
        {
            const float se = s * s * (3.0f - 2.0f * s); // smoothstep

            switch (m)
            {
                case Mode::Gentle:
                    // Transparent bus glue — slow, wide knee, mild ratio
                    threshDb  = juce::jmap (se, 0.0f, 1.0f,  -3.0f, -14.0f);
                    ratio     = juce::jmap (se, 0.0f, 1.0f,   1.5f,   4.0f);
                    attackMs  = juce::jmap (se, 0.0f, 1.0f,  50.0f,  12.0f);
                    releaseMs = juce::jmap (se, 0.0f, 1.0f, 300.0f,  80.0f);
                    kneeDb    = juce::jmap (se, 0.0f, 1.0f,   8.0f,   4.0f);
                    break;

                case Mode::Standard:
                default:
                    // All-purpose: snappy but musical
                    threshDb  = juce::jmap (se, 0.0f, 1.0f,  -6.0f, -22.0f);
                    ratio     = juce::jmap (se, 0.0f, 1.0f,   2.0f,   8.0f);
                    attackMs  = juce::jmap (se, 0.0f, 1.0f,  25.0f,   3.0f);
                    releaseMs = juce::jmap (se, 0.0f, 1.0f, 200.0f,  40.0f);
                    kneeDb    = juce::jmap (se, 0.0f, 1.0f,   6.0f,   1.5f);
                    break;

                case Mode::Smash:
                    // Aggressive parallel-style — fast, tight, hard knee
                    threshDb  = juce::jmap (se, 0.0f, 1.0f,  -9.0f, -30.0f);
                    ratio     = juce::jmap (se, 0.0f, 1.0f,   4.0f,  20.0f);
                    attackMs  = juce::jmap (se, 0.0f, 1.0f,   8.0f,   0.5f);
                    releaseMs = juce::jmap (se, 0.0f, 1.0f, 100.0f,  15.0f);
                    kneeDb    = juce::jmap (se, 0.0f, 1.0f,   4.0f,   0.0f);
                    break;
            }

            // Auto makeup: scale with se so the knob at 0 is truly transparent.
            // Approximates 70% of the expected static GR at full compression.
            const float rawMakeup = -threshDb * (1.0f - 1.0f / ratio) * 0.70f;
            makeupDb = se * juce::jlimit (0.0f, 20.0f, rawMakeup);
        }

        double sampleRate { 44100.0 };
        int    channels   { 2 };

        float rmsCoef { 0.99f };
        float smCoef  { 0.0f  };

        // Block-rate smoothed param targets
        float smThresh { -18.0f };
        float smRatio  {   4.0f };
        float smAttMs  {  10.0f };
        float smRelMs  { 100.0f };
        float smKneeDb {   4.0f };
        float smMakeup {   6.0f };

        // Per-channel state
        struct ChannelState { float meanSq { 0.0f }; float gainSmooth { 1.0f }; };
        std::array<ChannelState, 8> chState;

        float grDisplay     { 0.0f };

    };
}
