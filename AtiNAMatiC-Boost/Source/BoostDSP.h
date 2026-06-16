#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace nb
{
    // Pre-amp boost designed to feed a NAM amp model.
    //
    //  Signal chain (per sample, per channel):
    //    1. Optional very-gentle low tightening (HPF @ 35 Hz, always on)
    //    2. Clean input gain  (0..+18 dB driven by BOOST knob)
    //    3. Soft asymmetric saturation (tanh + small 2nd harmonic bias)
    //          - normalised so peak stays at 1.0 regardless of drive
    //          - this is the "harmonic life", NOT overdrive
    //    4. Tone shaping (per character mode)
    //          Balanced : tiny high shelf for clarity, scales with boost
    //          Bright   : Balanced + extra presence shelf (~6 kHz)
    //          Thick    : Balanced + low-mid body shelf  (~200 Hz)
    //    5. Output compensation
    //          - automatic loudness make-up so a 0..1 sweep of BOOST does
    //            not change perceived volume more than ~1 dB
    //          - then user OUTPUT trim
    //
    // The single BOOST macro (0..1) is mapped to *all* of:
    //   drive, harmonic content amount, clarity shelf gain, auto-makeup.
    //
    // Modes are static colour shapes; they do not change the drive amount.
    class BoostDSP
    {
    public:
        enum Mode { Balanced = 0, Bright = 1, Thick = 2 };

        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> sparkAmount {   0.0f };  // 0..1, harmonic energy

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            const juce::dsp::ProcessSpec spec {
                sr, (juce::uint32) juce::jmax (blockSize, 32), (juce::uint32) channels
            };

            // Gentle DC/sub blocker, always on. Very subtle "low tightening"
            // that prevents flubby low-end from being amplified into NAM.
            tightenHPF.reset();
            tightenHPF.prepare (spec);
            tightenHPF.setType (juce::dsp::StateVariableTPTFilterType::highpass);
            tightenHPF.setCutoffFrequency (35.0f);
            tightenHPF.setResonance (0.5f);

            // Per-channel tone shapers. Coefficients are shared (updated in
            // place from the audio thread) — both filter instances copy from
            // them on every block.
            clarityCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);
            modeCoefs    = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);
            for (int c = 0; c < (int) clarity.size(); ++c)
            {
                clarity[c].reset();
                modeF[c].reset();
                clarity[c].coefficients = clarityCoefs;
                modeF[c].coefficients   = modeCoefs;
            }

            smBoost = 0.0f;
            updateClarity (0.0f);

            // Snap the mode shelf to its true target on prepare (no need to
            // ramp in from silence), then every later switch crossfades.
            *modeCoefs = *targetModeCoeffs (Balanced);

            envIn  = 0.0f;
            envOut = 0.0f;
            spark  = 0.0f;

            // ~25 ms block-rate smoothing for the BOOST macro and for
            // crossfading the mode-shelf's raw biquad coefficients on switch.
            smCoef = std::exp (-float (juce::jmax (blockSize, 32)) / float (0.025 * sr));

            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float boost, int mode, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            // Smooth the BOOST macro at block rate so drive/shape/bias and the
            // clarity shelf don't snap on a fast knob move/automation.
            smBoost = smCoef * smBoost + (1.0f - smCoef) * boost;

            // --- Macro mapping -------------------------------------------
            // boost 0..1 controls everything together.
            const float driveDb = smBoost * 18.0f;                // pre-sat gain
            const float drive   = juce::Decibels::decibelsToGain (driveDb);
            // Effective shaping amount for the saturator (>= 1.0). We tanh on
            // (drive * x) and then renormalise so output peak stays ~1.0.
            // shapeNorm scales the curve into a more "alive" feel at low boost.
            const float shape   = juce::jmax (1.0e-3f, drive);
            const float invTanh = 1.0f / std::tanh (shape);
            // 2nd-harmonic bias — adds a tiny asymmetry so we get even-order
            // content (warm, "tube-ish"). Stays subtle.
            const float bias    = smBoost * 0.07f;

            // Auto make-up so loudness is preserved across the BOOST sweep.
            // Empirically the tanh stage adds roughly +driveDb of crest before
            // re-normalisation; we trim back almost all of it.
            const float autoMakeupDb = -driveDb * 0.85f;
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (autoMakeupDb + outGainDb));
            mixSm.setTargetValue (mix);

            // Crossfade the mode shelf's raw biquad coefficients toward the
            // selected mode every block, rather than snapping the instant the
            // mode changes -- avoids a hard filter-state jump on switch.
            updateModeShelf ((Mode) mode);
            updateClarity (smBoost);

            // --- Render --------------------------------------------------
            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            // Per-block envelope smoothing coeffs.
            const float envA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));

            float pkIn = 0.0f, pkOut = 0.0f, sparkAcc = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float outGainNow = outGainSm.getNextValue();
                const float mixNow     = mixSm.getNextValue();

                for (int c = 0; c < chClamp; ++c)
                {
                    const float xRaw = writePtrs[c][i];

                    // 1) tighten
                    const float xTight = tightenHPF.processSample (c, xRaw);

                    // 2) clean gain into saturator
                    const float driven = xTight * drive;

                    // 3) soft asymmetric saturation (normalised)
                    //    y = (tanh(driven + bias) - tanh(bias)) / tanh(drive)
                    //    keeps DC at 0 and peak ~1.0 regardless of drive.
                    const float sat = (std::tanh (driven + bias) - std::tanh (bias)) * invTanh;

                    // 4) tone shaping (per-channel filters)
                    const float shaped = modeF[c].processSample (clarity[c].processSample (sat));

                    // 5) output trim + dry/wet
                    const float wet = shaped * outGainNow;
                    const float y   = wet * mixNow + xRaw * (1.0f - mixNow);

                    writePtrs[c][i] = y;

                    if (c == 0)
                    {
                        const float aIn  = std::abs (xRaw);
                        const float aOut = std::abs (y);
                        if (aIn  > pkIn)  pkIn  = aIn;
                        if (aOut > pkOut) pkOut = aOut;

                        // "Spark" = how much harmonic content the saturator
                        // is producing — i.e. how much sat differs from a
                        // straight-through scaled signal. Normalised by drive.
                        const float linear = xTight; // unity-gain ref
                        sparkAcc += std::abs (sat - linear);
                    }
                }
            }

            envIn  += (pkIn  - envIn)  * envA;
            envOut += (pkOut - envOut) * envA;
            spark  += (juce::jmin (1.0f, sparkAcc / juce::jmax (1, n) * (1.0f + smBoost * 4.0f)) - spark) * envA;

            inputDb.store     (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store    (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            sparkAmount.store (juce::jlimit (0.0f, 1.0f, spark),                std::memory_order_relaxed);
        }

    private:
        void updateClarity (float boost)
        {
            // Up to ~+2.5 dB at 4 kHz. Subtle, scales with boost.
            const float gainDb = boost * 2.5f;
            *clarityCoefs = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, 4000.0f, 0.7f,
                juce::Decibels::decibelsToGain (gainDb));
        }

        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>
            targetModeCoeffs (Mode m)
        {
            switch (m)
            {
                case Bright:
                    return juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                            sampleRate, 6000.0f, 0.7f,
                            juce::Decibels::decibelsToGain (3.0f));
                case Thick:
                    return juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                            sampleRate, 220.0f, 0.7f,
                            juce::Decibels::decibelsToGain (3.0f));
                case Balanced:
                default:
                    return juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                            sampleRate, 1000.0f, 0.7f, 1.0f);
            }
        }

        // Bright/Thick/Balanced are qualitatively different filter shapes
        // (high-shelf / low-shelf / unity peak), so we can't interpolate their
        // freq/gain parameters like a same-shape sweep. Instead, crossfade the
        // raw biquad coefficients toward the new mode's target every block --
        // cheap, and avoids the instant filter-state jump on switch.
        void updateModeShelf (Mode m)
        {
            auto target = targetModeCoeffs (m);
            auto& cur = modeCoefs->coefficients;
            auto& tgt = target->coefficients;
            const int num = juce::jmin (cur.size(), tgt.size());
            for (int i = 0; i < num; ++i)
                cur.getReference (i) = smCoef * cur[i] + (1.0f - smCoef) * tgt[i];
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::dsp::StateVariableTPTFilter<float> tightenHPF;

        // Shared coefficient objects, updated in-place on the audio thread.
        // Each per-channel filter holds a ReferenceCountedObjectPtr to them.
        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> clarityCoefs, modeCoefs;
        std::array<juce::dsp::IIR::Filter<float>, 8> clarity;
        std::array<juce::dsp::IIR::Filter<float>, 8> modeF;

        // Block-rate smoothed BOOST macro + mode-shelf crossfade coefficient,
        // and per-sample output/mix ramps.
        float smBoost { 0.0f };
        float smCoef  { 0.0f };
        juce::SmoothedValue<float> outGainSm, mixSm;

        float envIn   { 0.0f };
        float envOut  { 0.0f };
        float spark   { 0.0f };
    };
}
