#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace na
{
    // ------------------------------------------------------------------
    // NAM Level — auto output leveler (END OF CHAIN).
    //
    // Concept: keep perceived loudness consistent regardless of what
    // happens upstream (riff change, palm mute, big chord, lead boost…).
    //
    // Single LEVEL macro (0..1):
    //   Soft   (0.0) : slow, gentle. Long time constants, small max
    //                  correction range. Preserves dynamics.
    //   Locked (1.0) : fast, strict. Short time constants, wide
    //                  correction range. Keeps level rock-steady.
    //
    // Auto toggle: when OFF the gain stage is bypassed (passthrough)
    // but the meter still shows output level — useful for A/B.
    //
    // Algorithm:
    //   1. Track short-term RMS of the input (window scaled by macro).
    //   2. Compare to a fixed internal target (-18 dBFS RMS).
    //   3. Compute correction gain in dB, clamp to allowed range,
    //      smooth it with attack/release time constants (also macro
    //      dependent — asymmetric, attack always faster than release
    //      so we duck loud parts quickly but lift quiet parts gently).
    //   4. Apply gain. Final soft-clipper / brickwall to catch any
    //      remaining peaks above 0 dBFS.
    //
    // The macro shapes time constants and max correction together so
    // the knob always feels coherent: more LEVEL = tighter leash.
    // ------------------------------------------------------------------
    class LevelDSP
    {
    public:
        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> gainDb      {   0.0f };  // current applied correction
        std::atomic<float> grAmount    {   0.0f };  // 0..1 — abs gain change vs max

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);
            juce::ignoreUnused (blockSize);

            // Loudness detector: two cascaded one-pole mean-square stages
            // (behaves like a ~window-length boxcar but never needs a reset
            // when the window changes -- the old sliding window was cleared
            // on every knob move, which made the leveler lurch).
            ms1 = ms2 = 0.0f;
            heldDb = 0.0f;
            envIn         = 0.0f;
            envOut        = 0.0f;
            smoothedGain  = 1.0f;

            // Auto on/off is a hard boolean checked once per block; ramp the
            // manual<->auto-corrected gain blend over ~30 ms instead of
            // switching instantly, or toggling Auto pops at the block boundary.
            autoBlend     = 1.0f;
            autoBlendCoef = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float macro, bool autoOn, float trimDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const float macroClamped = juce::jlimit (0.0f, 1.0f, macro);

            // ---- Macro mapping ------------------------------------------
            // Detector time: 600 ms (Soft) -> 250 ms (Locked). Faster
            // detector = more aggressive. Floor is 250 ms so the leveler
            // tracks sustained level drift, not individual note envelopes.
            const double winSec = 0.600 - 0.350 * (double) macroClamped;
            const float  msA    = (float) (1.0 - std::exp (-1.0 / (0.5 * winSec * sampleRate)));

            // Attack/release time constants. Attack faster than release.
            // Soft   : att 250 ms, rel 1500 ms
            // Locked : att  20 ms, rel  400 ms
            // Release floor raised to 400 ms (was 150 ms) so gain restores
            // gradually after ducking — prevents audible pump/breathe on guitar.
            const double attMs = 250.0 - 230.0 * (double) macroClamped;
            const double relMs = 1500.0 - 1100.0 * (double) macroClamped;
            const float  attA  = (float) (1.0 - std::exp (-1.0 / (0.001 * attMs * sampleRate)));
            const float  relA  = (float) (1.0 - std::exp (-1.0 / (0.001 * relMs * sampleRate)));

            // Max correction range (dB).
            // Soft   :  ±4 dB
            // Locked : ±18 dB
            const float maxCutDb   = 4.0f + 14.0f * macroClamped;  // how much we can lower
            const float maxBoostDb = 4.0f + 14.0f * macroClamped;  // how much we can lift

            // Internal loudness target: -18 dBFS RMS. Output trim is
            // applied AFTER correction so the user can offset the final
            // output level without changing the leveling action.
            const float targetDb    = -18.0f;
            const float trimGain    = juce::Decibels::decibelsToGain (trimDb);
            const float envSmooth   = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));

            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f;
            float lastAppliedDb = juce::Decibels::gainToDecibels (smoothedGain, -60.0f);
            const float autoTarget = autoOn ? 1.0f : 0.0f;
            const float invCh = 1.0f / (float) juce::jmax (1, chClamp);

            // Output safety knee: transparent below -2 dBFS, tops out at 0 dBFS.
            constexpr float knee = 0.7943f;

            for (int i = 0; i < n; ++i)
            {
                autoBlend += (autoTarget - autoBlend) * autoBlendCoef;

                // --- Mean square across channels (phase-proof) ----------
                float sq = 0.0f, aIn = 0.0f;
                for (int c = 0; c < chClamp; ++c)
                {
                    const float x = writePtrs[c][i];
                    sq += x * x;
                    aIn = juce::jmax (aIn, std::abs (x));
                }
                sq *= invCh;

                ms1 += (sq  - ms1) * msA;
                ms2 += (ms1 - ms2) * msA;
                const float rms = std::sqrt (juce::jmax (1.0e-12f, ms2));
                const float rmsDb = juce::Decibels::gainToDecibels (rms, -100.0f);

                // --- Desired correction (dB) ----------------------------
                float corrDb = juce::jlimit (-maxCutDb, maxBoostDb, targetDb - rmsDb);

                // Gaps between phrases / silence: hold the gain we have
                // instead of lifting the noise floor (or releasing to unity
                // and pumping back on the next note).
                if (rmsDb < targetDb - 30.0f)
                    corrDb = heldDb;

                // Hold band: only move the target once the needed correction
                // has drifted more than 1 dB from what we're holding. Unlike
                // the old deadband (which snapped back to 0 dB) this never
                // makes the target jump, so there's no wavy toggling.
                if (std::abs (corrDb - heldDb) > 1.0f)
                    heldDb = corrDb - std::copysign (1.0f, corrDb - heldDb);
                heldDb = juce::jlimit (-maxCutDb, maxBoostDb, heldDb);
                corrDb = heldDb;

                const float targetGain = juce::Decibels::decibelsToGain (corrDb);

                // Asymmetric smoothing: attack when we need to REDUCE gain
                // (signal got loud), release when we need to ADD gain.
                const float a = (targetGain < smoothedGain) ? attA : relA;
                smoothedGain += (targetGain - smoothedGain) * a;

                // --- Apply ----------------------------------------------
                // Blend smoothly between unity (manual/trim-only) and the
                // full auto-corrected gain so toggling Auto doesn't pop.
                const float autoGainNow = 1.0f + autoBlend * (smoothedGain - 1.0f);
                const float applyGain   = autoGainNow * trimGain;

                for (int c = 0; c < chClamp; ++c)
                {
                    float y = writePtrs[c][i] * applyGain;

                    const float ay = std::abs (y);
                    if (ay > knee)
                        y = std::copysign (knee + (1.0f - knee) * std::tanh ((ay - knee) / (1.0f - knee)), y);

                    writePtrs[c][i] = y;

                    const float aOut = std::abs (y);
                    if (aOut > pkOut) pkOut = aOut;
                }

                if (aIn > pkIn) pkIn = aIn;

                lastAppliedDb = juce::Decibels::gainToDecibels (
                                    juce::jmax (1.0e-6f, smoothedGain), -60.0f);
            }

            envIn  += (pkIn  - envIn)  * envSmooth;
            envOut += (pkOut - envOut) * envSmooth;

            const float maxRange = juce::jmax (maxCutDb, maxBoostDb);
            const float grNorm   = juce::jlimit (0.0f, 1.0f,
                                                 std::abs (lastAppliedDb) / juce::jmax (0.1f, maxRange));

            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            gainDb.store   (lastAppliedDb * autoBlend, std::memory_order_relaxed);
            grAmount.store (grNorm        * autoBlend, std::memory_order_relaxed);
        }

    private:
        double sampleRate { 48000.0 };
        int    channels   { 2 };

        float ms1    { 0.0f };
        float ms2    { 0.0f };
        float heldDb { 0.0f };

        float envIn        { 0.0f };
        float envOut       { 0.0f };
        float smoothedGain { 1.0f };

        // Smoothed 0..1 blend toward the Auto on/off target (see process()).
        float autoBlend     { 1.0f };
        float autoBlendCoef { 0.0f };
    };
}
