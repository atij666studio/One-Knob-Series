#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <OneKnobDSP.h>
#include <atomic>
#include <cmath>

namespace nb
{
    // Pre-amp boost designed to feed a NAM amp model.
    //
    //  Signal chain (per channel):
    //    1. Gentle low tightening (HPF @ 35 Hz, always on)
    //    2. 4x oversampled soft asymmetric saturation
    //          - unity small-signal gain, so the knob changes how hard the
    //            curve is driven, not how loud the signal is
    //          - CLEAN end starts well above full scale (practically
    //            transparent), PUSH end starts bending around -18 dBFS
    //          - small bias adds even-order "tube" content; the DC it
    //            creates is removed right after
    //    3. Tone shaping (per character mode)
    //          Balanced : tiny high shelf for clarity, scales with boost
    //          Bright   : Balanced + extra presence shelf (~6 kHz)
    //          Thick    : Balanced + low-mid body shelf  (~220 Hz)
    //    4. Static loudness compensation computed from the actual curve at a
    //       typical DI level, so a 0..1 sweep of BOOST stays within ~1 dB
    //       (no adaptive auto-gain, so nothing breathes), then OUTPUT trim
    //    5. Dry/wet MIX against a latency-aligned dry signal
    //
    // The single BOOST macro (0..1) drives: drive, harmonic bias and the
    // clarity shelf. Modes are static colour shapes.
    class BoostDSP
    {
    public:
        enum Mode { Balanced = 0, Bright = 1, Thick = 2 };

        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> sparkAmount {   0.0f };  // 0..1, harmonic energy

        int getLatencySamples() const { return latency; }

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            maxBlock   = juce::jmax (blockSize, 32);

            const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, (juce::uint32) channels };

            tightenHPF.reset();
            tightenHPF.prepare (spec);
            tightenHPF.setType (juce::dsp::StateVariableTPTFilterType::highpass);
            tightenHPF.setCutoffFrequency (35.0f);
            tightenHPF.setResonance (0.5f);

            // 4x, linear-phase half-band FIR stages: clean aliasing
            // rejection and an integer latency we can report and match.
            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false, true);
            oversampler->initProcessing ((size_t) maxBlock);
            latency = (int) std::round (oversampler->getLatencyInSamples());

            dryDelay.prepare (channels, latency + 1);
            work.setSize (channels, maxBlock, false, true, false);

            clarityCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);
            modeCoefs    = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);
            for (int c = 0; c < (int) clarity.size(); ++c)
            {
                clarity[c].reset();
                modeF[c].reset();
                dc[c].prepare (sr);
                clarity[c].coefficients = clarityCoefs;
                modeF[c].coefficients   = modeCoefs;
            }

            smBoost = -1.0f;   // snap to the first value we see
            updateClarity (0.0f);
            for (int m = 0; m < 3; ++m)
                modeTargets[(size_t) m] = targetModeCoeffs ((Mode) m);
            *modeCoefs = modeTargets[Balanced];

            envIn = envOut = spark = 0.0f;

            smCoef = std::exp (-float (maxBlock) / float (0.025 * sr));

            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float boost, int mode, float mix, float outGainDb)
        {
            // Hosts may (rarely) exceed the negotiated block size: work in
            // chunks of what we prepared for, never allocate.
            const int total = buffer.getNumSamples();
            for (int pos = 0; pos < total; pos += maxBlock)
            {
                juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                               pos, juce::jmin (maxBlock, total - pos));
                processChunk (view, boost, mode, mix, outGainDb);
            }
        }

    private:
        void processChunk (juce::AudioBuffer<float>& buffer,
                           float boost, int mode, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            // Block-rate smoothing of the BOOST macro.
            smBoost = smBoost < 0.0f ? boost : smCoef * smBoost + (1.0f - smCoef) * boost;

            // --- Macro mapping -------------------------------------------
            // Drive -6..+18 dB: the curve starts bending around 1/drive, so
            // CLEAN (~+6 dBFS onset) is effectively linear for any sane
            // input while PUSH saturates from about -18 dBFS.
            const float drive = juce::Decibels::decibelsToGain (-6.0f + smBoost * 24.0f);
            const float bias  = smBoost * 0.07f;
            const float tb    = std::tanh (bias);
            const float norm  = 1.0f / (drive * (1.0f - tb * tb));   // unity slope at 0

            // Static loudness compensation at a typical DI level, so the knob
            // changes character rather than volume (and never breathes).
            const float comp = ok::referenceCompensation ([&] (float x) { return (std::tanh (drive * x + bias) - tb) * norm; });
            outGainSm.setTargetValue (comp * juce::Decibels::decibelsToGain (outGainDb));
            mixSm.setTargetValue (mix);

            updateModeShelf ((Mode) mode);
            updateClarity (smBoost);

            // --- 1) tighten, keep the input for dry / loudness reference --
            for (int c = 0; c < numCh; ++c)
            {
                work.copyFrom (c, 0, buffer, c, 0, n);
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = tightenHPF.processSample (c, d[i]);
            }

            // --- 2) oversampled saturation -------------------------------
            float sparkNum = 0.0f, sparkDen = 1.0e-9f;
            {
                juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
                auto up = oversampler->processSamplesUp (block);
                for (size_t c = 0; c < up.getNumChannels(); ++c)
                {
                    auto* d = up.getChannelPointer (c);
                    for (size_t i = 0; i < up.getNumSamples(); ++i)
                    {
                        const float x   = d[i];
                        const float sat = (std::tanh (drive * x + bias) - tb) * norm;
                        if (c == 0)
                        {
                            sparkNum += std::abs (sat - x);
                            sparkDen += std::abs (x);
                        }
                        d[i] = sat;
                    }
                }
                oversampler->processSamplesDown (block);
            }

            // --- 3..5) DC, tone, loudness, trim, mix ---------------------
            float pkIn = 0.0f, pkOut = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float outGainNow = outGainSm.getNextValue();
                const float mixNow     = mixSm.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    float* d = buffer.getWritePointer (c);
                    const float shaped = modeF[c].processSample (clarity[c].processSample (dc[c].process (d[i])));
                    const float xIn = work.getSample (c, i);
                    const float dry = dryDelay.process (c, xIn, latency);
                    const float y   = shaped * outGainNow * mixNow + dry * (1.0f - mixNow);
                    d[i] = y;

                    pkIn  = juce::jmax (pkIn,  std::abs (xIn));
                    pkOut = juce::jmax (pkOut, std::abs (y));
                }
                dryDelay.advance();
            }

            const float envA = (float) (1.0 - std::exp (-(double) n / (0.060 * sampleRate)));
            envIn  += (pkIn  - envIn)  * envA;
            envOut += (pkOut - envOut) * envA;
            const float sparkNow = juce::jlimit (0.0f, 1.0f, std::sqrt (sparkNum / sparkDen) * 2.5f);
            spark  += (sparkNow - spark) * envA;

            inputDb.store     (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store    (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            sparkAmount.store (juce::jlimit (0.0f, 1.0f, spark),                std::memory_order_relaxed);
        }

        using Biquad = std::array<float, 6>;

        void updateClarity (float boost)
        {
            // Up to ~+2.5 dB at 4 kHz. Subtle, scales with boost.
            // ArrayCoefficients: no heap allocation on the audio thread.
            *clarityCoefs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
                sampleRate, 4000.0f, 0.7f, juce::Decibels::decibelsToGain (boost * 2.5f));
        }

        Biquad targetModeCoeffs (Mode m) const
        {
            switch (m)
            {
                case Bright:
                    return juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
                            sampleRate, 6000.0f, 0.7f, juce::Decibels::decibelsToGain (3.0f));
                case Thick:
                    return juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (
                            sampleRate, 220.0f, 0.7f, juce::Decibels::decibelsToGain (3.0f));
                case Balanced:
                default:
                    return juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
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
            const auto& tgt = modeTargets[(size_t) juce::jlimit (0, 2, (int) m)];
            auto* cur = modeCoefs->getRawCoefficients();
            // Coefficients stores b0 b1 b2 a1 a2 (a0 normalised away);
            // ArrayCoefficients gives b0 b1 b2 a0 a1 a2.
            const float inv = 1.0f / tgt[3];
            const float t[5] { tgt[0] * inv, tgt[1] * inv, tgt[2] * inv, tgt[4] * inv, tgt[5] * inv };
            for (int i = 0; i < 5; ++i)
                cur[i] = smCoef * cur[i] + (1.0f - smCoef) * t[i];
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        int    maxBlock   { 512 };
        int    latency    { 0 };

        juce::dsp::StateVariableTPTFilter<float> tightenHPF;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        juce::AudioBuffer<float> work;
        ok::DelayLine dryDelay;
        std::array<ok::DcBlocker, 8> dc;

        // Shared coefficient objects, updated in-place on the audio thread.
        // Each per-channel filter holds a ReferenceCountedObjectPtr to them.
        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> clarityCoefs, modeCoefs;
        std::array<Biquad, 3> modeTargets {};
        std::array<juce::dsp::IIR::Filter<float>, 8> clarity;
        std::array<juce::dsp::IIR::Filter<float>, 8> modeF;

        float smBoost { -1.0f };
        float smCoef  { 0.0f };
        juce::SmoothedValue<float> outGainSm, mixSm;

        float envIn   { 0.0f };
        float envOut  { 0.0f };
        float spark   { 0.0f };
    };
}
