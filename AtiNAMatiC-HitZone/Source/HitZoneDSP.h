#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace nhz
{
    // Target RMS window for "good" NAM input level.
    // NAM models are typically captured at peaks ~ -18 dBFS reamp.
    // RMS target sits a touch lower so transient peaks land near the model's sweet spot.
    constexpr float kTargetRmsLowDb  = -20.0f;
    constexpr float kTargetRmsHighDb = -12.0f;
    constexpr float kTargetRmsIdeal  = -16.0f;

    // Peak above this is considered actually clipping the model's input.
    constexpr float kPeakDangerDb = -3.0f;

    // Input RMS below this is treated as silence / noise between phrases.
    constexpr float kSilenceDb = -55.0f;

    enum class HitStatus { TooCold = 0, Perfect = 1, TooHot = 2 };

    inline float dbToGain (float db)  { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)   { return 20.0f * std::log10 (std::max (g, 1.0e-7f)); }

    class HitZoneDSP
    {
    public:
        // Parameters (set by processor each block)
        std::atomic<float> sweetSpot   { 0.5f };  // 0..1
        std::atomic<float> mix         { 1.0f };  // 0..1
        std::atomic<float> outputDb    { 0.0f };  // dB
        std::atomic<bool>  autoOn      { true };
        std::atomic<float> response    { 0.5f };  // 0 slow .. 1 fast
        std::atomic<float> manualGainDb{ 0.0f };  // used when auto off

        // Outputs (read by editor)
        std::atomic<float> currentRmsDb { -60.0f };
        std::atomic<float> peakDb       { -60.0f };
        std::atomic<float> peakHoldDb   { -60.0f };
        std::atomic<float> appliedGainDb{ 0.0f };
        std::atomic<int>   statusEnum   { (int) HitStatus::Perfect };

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, (juce::uint32) channels };

            tilt.prepare (spec);
            tilt.reset();
            *tilt.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (sr, 180.0, 0.7f, 1.0f);

            air.prepare (spec);
            air.reset();
            *air.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sr, 4500.0, 0.7f, 1.0f);

            // Pre-detector HPF removes subsonic so RMS isn't skewed and auto-gain doesn't pump.
            juce::dsp::ProcessSpec monoSpec { sr, (juce::uint32) blockSize, 1u };
            detectorHpf.prepare (monoSpec);
            detectorHpf.reset();
            *detectorHpf.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sr, 35.0, 0.707f);

            // RMS detector: 60 ms attack, 250 ms release (per-sample one-pole).
            rmsAttCoef = std::exp (-1.0f / (float) (0.060 * sr));
            rmsRelCoef = std::exp (-1.0f / (float) (0.250 * sr));
            rmsAcc = 1.0e-7f;

            // Peak envelope: instant attack, ~600 ms release.
            peakRelCoef = std::exp (-1.0f / (float) (0.600 * sr));
            peakEnv = 0.0f;

            // Peak-hold: holds, then decays slowly.
            peakHoldDecayCoef = std::exp (-1.0f / (float) (1.5 * sr));
            peakHoldVal = 0.0f;

            // Asymmetric per-sample gain glide (dB domain): drop fast so a
            // loud hit after a quiet passage is caught, rise gently.
            gainDownCoef = (float) (1.0 - std::exp (-1.0 / (0.0003 * sr)));
            gainUpCoef   = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
            gainDbNow    = 0.0f;
            outputGainSmoothed.reset      (sr, 0.020);
            mixSmoothed.reset             (sr, 0.020);
            satSmoothed.reset             (sr, 0.050);
            lowShelfGainSmoothed.reset    (sr, 0.080);
            highShelfGainSmoothed.reset   (sr, 0.080);

            outputGainSmoothed.setCurrentAndTargetValue (1.0f);
            mixSmoothed.setCurrentAndTargetValue (1.0f);
            satSmoothed.setCurrentAndTargetValue (0.0f);
            lowShelfGainSmoothed.setCurrentAndTargetValue (1.0f);
            highShelfGainSmoothed.setCurrentAndTargetValue (1.0f);

            autoGainDb = 0.0f;
            gainDbNow  = 0.0f;

            scratchDry.setSize (channels, blockSize, false, false, true);
            scratchDet.setSize (1,        blockSize, false, false, true);
        }

        void reset()
        {
            tilt.reset();
            air.reset();
            detectorHpf.reset();
            rmsAcc = 1.0e-7f;
            peakEnv = 0.0f;
            peakHoldVal = 0.0f;
            autoGainDb = 0.0f;
            gainDbNow  = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (buffer.getNumChannels(), channels);
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const float ss = juce::jlimit (0.0f, 1.0f, sweetSpot.load());

            // Tonal bias targets (smoothed).
            const float lowGainDb  = juce::jmap (ss, 0.0f, 1.0f, -2.0f, +3.0f);
            const float highGainDb = juce::jmap (ss, 0.0f, 1.0f, +2.0f, -2.5f);
            lowShelfGainSmoothed.setTargetValue  (dbToGain (lowGainDb));
            highShelfGainSmoothed.setTargetValue (dbToGain (highGainDb));

            // Coefficients are refreshed once per block, so advance the
            // smoothers by the whole block (one getNextValue per block would
            // stretch an 80 ms ramp to tens of seconds).
            lowShelfGainSmoothed.skip  (juce::jmax (0, n - 1));
            highShelfGainSmoothed.skip (juce::jmax (0, n - 1));
            *tilt.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf
                            (sampleRate, 180.0,  0.7f, lowShelfGainSmoothed.getNextValue());
            *air.state  = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf
                            (sampleRate, 4500.0, 0.7f, highShelfGainSmoothed.getNextValue());

            // Saturation only on the hot side.
            satSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, (ss - 0.5f) * 2.0f));

            // ---- Build a mono detector signal (HPF'd) ----
            if (scratchDet.getNumSamples() < n) scratchDet.setSize (1, n, false, false, true);
            scratchDet.clear (0, 0, n);
            const float invCh = 1.0f / (float) numCh;
            for (int ch = 0; ch < numCh; ++ch)
                scratchDet.addFrom (0, 0, buffer, ch, 0, n, invCh);

            {
                juce::dsp::AudioBlock<float> dblock (scratchDet.getArrayOfWritePointers(), 1, 0, (size_t) n);
                juce::dsp::ProcessContextReplacing<float> dctx (dblock);
                detectorHpf.process (dctx);
            }

            // ---- Per-sample envelope detection ----
            const float* det = scratchDet.getReadPointer (0);
            for (int i = 0; i < n; ++i)
            {
                const float x  = det[i];
                const float sq = x * x;

                const float coef = (sq > rmsAcc) ? rmsAttCoef : rmsRelCoef;
                rmsAcc = coef * rmsAcc + (1.0f - coef) * sq;

                const float a = std::abs (x);
                if (a > peakEnv) peakEnv = a;
                else             peakEnv *= peakRelCoef;

                if (a > peakHoldVal) peakHoldVal = a;
                else                 peakHoldVal *= peakHoldDecayCoef;
            }

            // Detector runs on the raw input; everything the user (and the
            // auto loop) cares about is the level *after* our gain, i.e. what
            // the NAM model actually receives.
            const float inRmsDb  = 10.0f * std::log10 (std::max (rmsAcc, 1.0e-7f));
            const float inPeakDb = 20.0f * std::log10 (std::max (peakEnv, 1.0e-7f));
            const float inHoldDb = 20.0f * std::log10 (std::max (peakHoldVal, 1.0e-7f));

            // ---- Decide gain ----
            float targetGainDb = 0.0f;
            const bool  isAuto  = autoOn.load();
            if (isAuto)
            {
                const float r = juce::jlimit (0.0f, 1.0f, response.load());

                // Asymmetric: pull-down faster than push-up. Default mid-Response is already snappy.
                const float upDbPerSec   = juce::jmap (r, 0.0f, 1.0f,  1.5f, 18.0f);
                const float downDbPerSec = juce::jmap (r, 0.0f, 1.0f,  6.0f, 60.0f);

                const float secs = (float) n / (float) sampleRate;

                // Closed loop: compare the target with input level + the gain
                // we are already applying (the old open-loop version kept
                // integrating the same error and ran to the +/-24 dB limit).
                const float err = kTargetRmsIdeal - (inRmsDb + autoGainDb);

                float step = 0.0f;
                if (inPeakDb + autoGainDb > kPeakDangerDb)
                {
                    // Safety stomp: pull down quickly when about to clip the model.
                    step = -downDbPerSec * secs;
                }
                else if (inRmsDb < kSilenceDb)
                {
                    // Between phrases / silence: hold the current gain instead
                    // of winding it up and blasting the next note.
                    step = 0.0f;
                }
                else if (err >= 0.0f)
                {
                    step = juce::jmin (err, upDbPerSec * secs);
                }
                else
                {
                    step = juce::jmax (err, -downDbPerSec * secs);
                }

                autoGainDb = juce::jlimit (-24.0f, 24.0f, autoGainDb + step);
                targetGainDb = autoGainDb;
            }
            else
            {
                targetGainDb = juce::jlimit (-24.0f, 24.0f, manualGainDb.load());
                autoGainDb   = targetGainDb;   // switching Auto back on starts from here
            }

            const float currentRmsDbVal = inRmsDb + targetGainDb;
            currentRmsDb.store (currentRmsDbVal);
            peakDb.store       (inPeakDb + targetGainDb);
            peakHoldDb.store   (inHoldDb + targetGainDb);

            // Hard ceiling from this block's input peak: whatever the loop
            // wants, never push a peak past the danger level. The detector
            // has already seen the whole block, so this acts like a short
            // lookahead and catches the attack that follows a quiet passage.
            if (isAuto)
            {
                float blockPeak = 0.0f;
                for (int i = 0; i < n; ++i) blockPeak = std::max (blockPeak, std::abs (det[i]));
                const float ceilingDb = kPeakDangerDb - gainToDb (blockPeak);
                if (targetGainDb > ceilingDb)
                {
                    targetGainDb = ceilingDb;
                    autoGainDb   = juce::jmin (autoGainDb, juce::jmax (-24.0f, ceilingDb));
                }
            }
            outputGainSmoothed.setTargetValue (dbToGain (outputDb.load()));
            mixSmoothed.setTargetValue        (juce::jlimit (0.0f, 1.0f, mix.load()));

            // ---- Status with hysteresis ----
            HitStatus s = (HitStatus) statusEnum.load();
            const float lowEnter  = kTargetRmsLowDb  - 1.0f;
            const float lowExit   = kTargetRmsLowDb  + 0.5f;
            const float highEnter = kTargetRmsHighDb + 1.0f;
            const float highExit  = kTargetRmsHighDb - 0.5f;
            if      (currentRmsDbVal < lowEnter)                              s = HitStatus::TooCold;
            else if (currentRmsDbVal > highEnter)                             s = HitStatus::TooHot;
            else if (s == HitStatus::TooCold && currentRmsDbVal > lowExit)    s = HitStatus::Perfect;
            else if (s == HitStatus::TooHot  && currentRmsDbVal < highExit)   s = HitStatus::Perfect;
            statusEnum.store ((int) s);

            // ---- Step 1: dry copy for parallel mix ----
            if (scratchDry.getNumChannels() != numCh || scratchDry.getNumSamples() < n)
                scratchDry.setSize (numCh, n, false, false, true);
            for (int ch = 0; ch < numCh; ++ch)
                scratchDry.copyFrom (ch, 0, buffer, ch, 0, n);

            // ---- Step 2: input gain + asymmetric soft saturation ----
            float lastAppliedGain = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                gainDbNow += (targetGainDb - gainDbNow) * (targetGainDb < gainDbNow ? gainDownCoef : gainUpCoef);
                const float g     = dbToGain (gainDbNow);
                const float satA  = satSmoothed.getNextValue();
                const float drive = 1.0f + satA * 1.8f; // 1 .. 2.8
                lastAppliedGain   = g;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    float x = buffer.getWritePointer (ch)[i] * g;

                    if (satA > 1.0e-4f)
                    {
                        // Asymmetric soft-clip — tube-ish 2nd-harmonic flavour.
                        const float bias = 0.07f * satA;
                        const float xb   = x * drive + bias;
                        const float y    = (std::tanh (xb) - std::tanh (bias)) / drive;
                        x = x + (y - x) * satA;
                    }

                    // Auto mode promises the model never gets slammed: a
                    // smooth knee above the danger level that tops out at
                    // 0 dBFS catches whatever the gain glide lets through.
                    if (isAuto)
                    {
                        constexpr float knee = 0.7079f;            // -3 dBFS
                        const float ax = std::abs (x);
                        if (ax > knee)
                            x = std::copysign (knee + (1.0f - knee) * std::tanh ((ax - knee) / (1.0f - knee)), x);
                    }

                    buffer.getWritePointer (ch)[i] = x;
                }
            }

            // Report what was actually applied to the audio (the smoothed,
            // ramped gain), not the raw unsmoothed target -- keeps the UI
            // readout in sync with what's audible during the ramp.
            appliedGainDb.store (gainToDb (lastAppliedGain));

            // ---- Step 3: tonal bias (low/high shelves) ----
            {
                juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                   (size_t) numCh, 0, (size_t) n);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                tilt.process (ctx);
                air.process  (ctx);
            }

            // ---- Step 4: parallel mix + output gain ----
            for (int i = 0; i < n; ++i)
            {
                const float mxv = mixSmoothed.getNextValue();
                const float og  = outputGainSmoothed.getNextValue();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float wet = buffer.getWritePointer (ch)[i];
                    const float dry = scratchDry.getReadPointer (ch)[i];
                    buffer.getWritePointer (ch)[i] = (dry * (1.0f - mxv) + wet * mxv) * og;
                }
            }
        }

    private:
        double sampleRate { 44100.0 };
        int    channels   { 2 };

        juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                       juce::dsp::IIR::Coefficients<float>> tilt, air, detectorHpf;

        float gainDbNow { 0.0f }, gainDownCoef { 0.1f }, gainUpCoef { 0.01f };

        juce::LinearSmoothedValue<float> outputGainSmoothed,
                                         mixSmoothed, satSmoothed,
                                         lowShelfGainSmoothed, highShelfGainSmoothed;

        juce::AudioBuffer<float> scratchDry, scratchDet;

        float rmsAttCoef { 0.99f }, rmsRelCoef { 0.999f };
        float peakRelCoef { 0.999f };
        float peakHoldDecayCoef { 0.9999f };
        float rmsAcc { 1.0e-7f };
        float peakEnv { 0.0f };
        float peakHoldVal { 0.0f };

        float autoGainDb { 0.0f };
    };
}
