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
    //   Tight  : short release, articulate. Minimal transient cut.
    //   Even   : balanced — smooth comp.
    //   Smooth : long release + heavy transient softening, very forgiving.
    //
    // Attacks are near-instant on purpose: with zero lookahead, anything
    // slower lets the first half-millisecond of every pick attack through
    // at full make-up gain, so the spikes would come out *louder* than they
    // went in. The detector holds its peak for ~15 ms, so a fast attack only
    // shapes the leading edge and never rides the waveform.
    //
    // Per-sample chain (per channel):
    //   1. DC blocker (HPF @ 25 Hz)
    //   2. Transient softener — light envelope-difference based attack ducker
    //      that only attenuates the *peak* part of pick attacks. Bypassed
    //      when assist == 0.
    //   3. Feedforward soft-knee compressor (peak-hold detector) with auto
    //      threshold and ratio driven by ASSIST. Smooth attack/release per
    //      mode.
    //   4. Static auto make-up (gain reduction of a typical picked note).
    //   5. Output trim.
    //
    // No oversampling, no FFT, no lookahead — keeps latency at zero so it
    // feels native in front of a NAM amp model.
    // Make-up calibration: reference note peak level and how much of its
    // gain reduction to give back (tuned on DI guitar for a ~flat sweep).
    constexpr float kMakeupRefDb  = -4.5f;
    constexpr float kMakeupAmount = 1.0f;

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
            pkHeld = 0.0f;
            holdCount = 0;
            trDuckDb = 0.0f;

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
                    compAttackMs = 0.02f; compReleaseMs = 90.0f;
                    ratioMax = 2.5f;      threshFloor = -28.0f;
                    transientAmt = 0.25f; kneeDb = 4.0f;
                    break;
                case Smooth:
                    compAttackMs = 0.1f;  compReleaseMs = 220.0f;
                    ratioMax = 4.0f;      threshFloor = -38.0f;
                    transientAmt = 1.0f;  kneeDb = 9.0f;
                    break;
                case Even:
                default:
                    compAttackMs = 0.05f; compReleaseMs = 130.0f;
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

            // Per-sample gain-reduction smoothing coefficients.
            const float aAtk = (float) (1.0 - std::exp (-1.0 / (compAttackMs  * 0.001 * sampleRate)));
            const float aRel = (float) (1.0 - std::exp (-1.0 / (compReleaseMs * 0.001 * sampleRate)));

            // Transient detector: fast vs slow attack, SHARED release. On a
            // sustained note both followers decay together between waveform
            // peaks, so only a real pick attack opens a gap between them
            // (the old symmetric 1.5 ms follower rippled at the note's own
            // frequency and modulated the gain = audible distortion).
            const float aTrFast = (float) (1.0 - std::exp (-1.0 / (0.0015 * sampleRate)));   // 1.5 ms
            const float aTrSlow = (float) (1.0 - std::exp (-1.0 / (0.020  * sampleRate)));   //  20 ms
            const float aTrRel  = (float) (1.0 - std::exp (-1.0 / (0.120  * sampleRate)));   // 120 ms
            const float trGain  = assist * transientAmt;   // 0..~1

            // Level detector: peak with ~15 ms hold, then 60 ms release. The
            // hold bridges a full cycle of a low E, so the level the gain
            // computer sees doesn't ripple with the waveform.
            const int   holdLen = (int) (0.015 * sampleRate);
            const float pkRel   = (float) std::exp (-1.0 / (0.060 * sampleRate));

            // Static, pedal-style make-up: the gain reduction a typical
            // picked note (peaks around REF dBFS) gets, given back. Static on
            // purpose -- an adaptive make-up "remembers" hard playing and
            // jumps the next soft note.
            const float overRef    = juce::jmax (0.0f, kMakeupRefDb - threshDb);
            const float makeupDb   = overRef * (1.0f - invRatio) * kMakeupAmount + trGain * 1.5f;
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (makeupDb + outGainDb));

            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            const float meterA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));

            float pkIn = 0.0f, pkOut = 0.0f, grAcc = 0.0f, smAcc = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                // DC-block every channel, then detect on the loudest one
                // (linked: one gain for all channels keeps the image steady).
                float ax = 1.0e-12f;
                for (int c = 0; c < chClamp; ++c)
                {
                    const float xc = dcBlock.processSample (c, writePtrs[c][i]);
                    writePtrs[c][i] = xc;
                    ax = juce::jmax (ax, std::abs (xc));
                }

                // --- Transient softener ---------------------------------
                envFast[0] += (ax - envFast[0]) * (ax > envFast[0] ? aTrFast : aTrRel);
                envSlow[0] += (ax - envSlow[0]) * (ax > envSlow[0] ? aTrSlow : aTrRel);
                const float relOver = juce::jmax (0.0f, envFast[0] - envSlow[0]) / (envSlow[0] + 1.0e-6f);
                const float softT   = juce::jlimit (0.0f, 1.0f, relOver * 1.5f);
                // Up to ~5 dB off the pick spike at trGain = 1, smoothed so
                // the ducking itself has no edges.
                trDuckDb += (-softT * trGain * 5.0f - trDuckDb) * aTrFast;

                // --- Compressor (log-domain, peak-hold detector) --------
                if (ax >= pkHeld) { pkHeld = ax; holdCount = holdLen; }
                else if (holdCount > 0) --holdCount;
                else pkHeld = juce::jmax (ax, pkHeld * pkRel);

                const float xDb = juce::Decibels::gainToDecibels (pkHeld, -100.0f);
                const float over0 = xDb - threshDb;
                float gr;   // negative dB
                if (over0 < -halfKnee)            gr = 0.0f;
                else if (over0 >  halfKnee)       gr = -(over0 * (1.0f - invRatio));
                else
                {
                    const float k = over0 + halfKnee;
                    gr = -((k * k) / (2.0f * kneeDb) * (1.0f - invRatio));
                }

                // Smooth GR: attack when more reduction needed, release when less.
                envComp[0] += (gr - envComp[0]) * ((gr < envComp[0]) ? aAtk : aRel);

                const float g = juce::Decibels::decibelsToGain (envComp[0] + trDuckDb) * outGainSm.getNextValue();

                for (int c = 0; c < chClamp; ++c)
                    writePtrs[c][i] *= g;

                pkIn  = juce::jmax (pkIn, ax);
                pkOut = juce::jmax (pkOut, std::abs (writePtrs[0][i]));
                grAcc += -envComp[0];   // positive dB
                smAcc += softT;
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

        float pkHeld    { 0.0f };
        int   holdCount { 0 };
        float trDuckDb  { 0.0f };

        float envIn     { 0.0f };
        float envOut    { 0.0f };
        float grEnv     { 0.0f };
        float smoothEnv { 0.0f };
    };
}
