#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace nf
{
    // "Feel Enhancer" — placed BEFORE a NAM amp model.
    //
    // Goal:
    //   - slight compression (tames spikes, lifts soft notes)
    //   - transient smoothing (rounds harsh pick attacks)
    //   - forgiving dynamics for practice (and comfort even for pros)
    //
    // One macro knob (ASSIST 0..1) opens up:
    //   compression amount + transient softening + auto make-up.
    //
    // Mode picker shapes the character:
    //   Tight  : fast attack, short release, articulate. Minimal transient cut.
    //   Even   : balanced — feedforward-style smooth comp.
    //   Smooth : slow-ish attack + heavy transient softening, very forgiving.
    //
    // Per-sample chain (per channel):
    //   1. DC blocker (HPF @ 25 Hz)
    //   2. Transient softener — light envelope-difference based attack ducker
    //      that only attenuates the *peak* part of pick attacks. Bypassed
    //      when assist == 0.
    //   3. Feedforward soft-knee compressor (log-domain envelope) with auto
    //      threshold and ratio driven by ASSIST. Smooth attack/release per
    //      mode.
    //   4. Auto make-up (compensates static gain reduction at avg input).
    //   5. Output trim.
    //
    // No oversampling, no FFT, no lookahead — keeps latency at zero so it
    // feels native in front of a NAM amp model.
    class FeelDSP
    {
    public:
        enum Mode { Tight = 0, Even = 1, Smooth = 2 };

        // Public meters (read by editor on a UI timer)
        std::atomic<float> inputDb       { -90.0f };
        std::atomic<float> outputDb      { -90.0f };
        std::atomic<float> grDb          {   0.0f };  // gain reduction (>= 0, dB)
        std::atomic<float> smoothAmount  {   0.0f };  // 0..1 transient softening activity

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            const juce::dsp::ProcessSpec spec {
                sr, (juce::uint32) juce::jmax (blockSize, 32), (juce::uint32) channels
            };

            dcBlock.reset();
            dcBlock.prepare (spec);
            dcBlock.setType (juce::dsp::StateVariableTPTFilterType::highpass);
            dcBlock.setCutoffFrequency (25.0f);
            dcBlock.setResonance (0.5f);

            for (int c = 0; c < (int) envFast.size(); ++c)
            {
                envFast[c]  = 0.0f;
                envSlow[c]  = 0.0f;
                envComp[c]  = 0.0f;  // log-domain envelope (in dB, negative below thresh)
            }

            envIn = envOut = 0.0f;
            grEnv = 0.0f;
            smoothEnv = 0.0f;

            outGainSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float assist, int mode, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            assist = juce::jlimit (0.0f, 1.0f, assist);

            // Mode-dependent timings & character ----------------------------
            float compAttackMs = 8.0f, compReleaseMs = 120.0f;
            float ratioMax     = 3.0f;     // max compression ratio at assist=1
            float threshFloor  = -34.0f;   // dB below which threshold can travel
            float transientAmt = 0.6f;     // base transient softening scale
            float kneeDb       = 6.0f;

            switch ((Mode) mode)
            {
                case Tight:
                    compAttackMs = 3.5f;  compReleaseMs = 80.0f;
                    ratioMax = 2.5f;      threshFloor = -28.0f;
                    transientAmt = 0.25f; kneeDb = 4.0f;
                    break;
                case Smooth:
                    compAttackMs = 18.0f; compReleaseMs = 220.0f;
                    ratioMax = 4.0f;      threshFloor = -38.0f;
                    transientAmt = 1.0f;  kneeDb = 9.0f;
                    break;
                case Even:
                default:
                    compAttackMs = 8.0f;  compReleaseMs = 130.0f;
                    ratioMax = 3.0f;      threshFloor = -32.0f;
                    transientAmt = 0.6f;  kneeDb = 6.0f;
                    break;
            }

            // Macro mapping --------------------------------------------------
            // ratio: 1.0 (no comp) → ratioMax. Threshold travels DOWN as
            // assist rises so more of the signal gets gently squeezed.
            const float ratio       = 1.0f + assist * (ratioMax - 1.0f);
            const float threshDb    = juce::jmap (assist, 0.0f, 1.0f, -10.0f, threshFloor);
            const float invRatio    = 1.0f / ratio;
            const float halfKnee    = kneeDb * 0.5f;

            // Per-sample envelope coefficients (log-domain envelope of |x|).
            const float aAtk = (float) (1.0 - std::exp (-1.0 / (compAttackMs  * 0.001 * sampleRate)));
            const float aRel = (float) (1.0 - std::exp (-1.0 / (compReleaseMs * 0.001 * sampleRate)));

            // Transient detector (fast vs slow). Difference (fast - slow)
            // marks pick attacks; we duck the *peak* portion proportional to
            // assist * mode-scale.
            const float aTrFast = (float) (1.0 - std::exp (-1.0 / (0.0015 * sampleRate)));   // 1.5 ms
            const float aTrSlow = (float) (1.0 - std::exp (-1.0 / (0.030  * sampleRate)));   //  30 ms
            const float trGain  = assist * transientAmt;   // 0..~1

            // Auto make-up: estimate static GR at threshold of -12 dBFS-ish
            // signal and add half of it back. Keeps loudness perceptually
            // similar across the sweep.
            const float refDb       = -12.0f;
            const float overRef     = juce::jmax (0.0f, refDb - threshDb);
            const float staticGRdB  = overRef * (1.0f - invRatio);
            const float makeupDb    = staticGRdB * 0.7f;
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (makeupDb + outGainDb));

            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            const float meterA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));

            float pkIn = 0.0f, pkOut = 0.0f, grAcc = 0.0f, smAcc = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                // Use ch0 as the sidechain detector for stable stereo image.
                const float xDet0 = dcBlock.processSample (0, writePtrs[0][i]);
                writePtrs[0][i]   = xDet0;

                const float ax    = std::abs (xDet0) + 1.0e-12f;

                // --- Transient softener ---------------------------------
                envFast[0] += (ax - envFast[0]) * aTrFast;
                envSlow[0] += (ax - envSlow[0]) * aTrSlow;
                const float over = envFast[0] - envSlow[0];   // > 0 during attack
                // Map "attack overshoot" relative to slow envelope to a
                // 0..1 amount of softening.
                const float relOver = juce::jmax (0.0f, over) / (envSlow[0] + 1.0e-6f);
                const float softT   = juce::jlimit (0.0f, 1.0f, relOver * 1.5f);
                // Maximum attenuation during a pick spike: up to 4 dB at trGain=1.
                const float trDuckDb = -softT * trGain * 4.0f;
                const float trGainLin = juce::Decibels::decibelsToGain (trDuckDb);

                // --- Compressor envelope (log-domain) -------------------
                const float xDb = juce::Decibels::gainToDecibels (ax, -100.0f);
                // Target gain reduction in dB given soft knee.
                float over0 = xDb - threshDb;
                float gr;   // negative dB
                if (over0 < -halfKnee)            gr = 0.0f;
                else if (over0 >  halfKnee)       gr = -(over0 * (1.0f - invRatio));
                else
                {
                    const float k = over0 + halfKnee;
                    const float soft = (k * k) / (2.0f * kneeDb);
                    gr = -(soft * (1.0f - invRatio));
                }

                // Smooth GR: attack when more reduction needed, release when less.
                const float a = (gr < envComp[0]) ? aAtk : aRel;
                envComp[0]   += (gr - envComp[0]) * a;

                const float compGainLin = juce::Decibels::decibelsToGain (envComp[0]);
                const float outGainNow  = outGainSm.getNextValue();

                // Apply per-channel (same envelope for stereo coherency).
                for (int c = 0; c < chClamp; ++c)
                {
                    float x = (c == 0) ? xDet0 : dcBlock.processSample (c, writePtrs[c][i]);
                    x *= trGainLin;
                    x *= compGainLin;
                    x *= outGainNow;
                    writePtrs[c][i] = x;

                    if (c == 0)
                    {
                        const float aIn  = ax;
                        const float aOut = std::abs (x);
                        if (aIn  > pkIn)  pkIn  = aIn;
                        if (aOut > pkOut) pkOut = aOut;
                        grAcc += -envComp[0];   // positive dB
                        smAcc += softT;
                    }
                }
            }

            envIn  += (pkIn  - envIn)  * meterA;
            envOut += (pkOut - envOut) * meterA;

            const float grAvg = grAcc / juce::jmax (1, n);
            const float smAvg = smAcc / juce::jmax (1, n) * trGain;
            grEnv     += (grAvg - grEnv)         * meterA;
            smoothEnv += (smAvg - smoothEnv)     * meterA;

            inputDb.store      (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store     (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            grDb.store         (juce::jmax (0.0f, grEnv),                       std::memory_order_relaxed);
            smoothAmount.store (juce::jlimit (0.0f, 1.0f, smoothEnv),           std::memory_order_relaxed);
        }

    private:
        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::dsp::StateVariableTPTFilter<float> dcBlock;

        std::array<float, 8> envFast { {} };
        std::array<float, 8> envSlow { {} };
        std::array<float, 8> envComp { {} };  // dB (negative = reduction)

        juce::SmoothedValue<float> outGainSm;

        float envIn     { 0.0f };
        float envOut    { 0.0f };
        float grEnv     { 0.0f };
        float smoothEnv { 0.0f };
    };
}
