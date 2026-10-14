#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <OneKnobDSP.h>
#include <atomic>
#include <cmath>

namespace nd
{
    // "Character dirt" stage — sits BEFORE a NAM amp model.
    //
    //   - light saturation
    //   - edge breakup
    //   - harmonic grit
    //
    // It is intentionally NOT a full distortion box. The DIRT macro (0..1)
    // sweeps from clean → grit while the loudness stays put.
    //
    // Signal chain (per channel):
    //   1. DC blocker / sub tighten (HPF @ 40 Hz)
    //   2. Pre-emphasis — boosts upper-mids INTO the shaper so the grit
    //      lands where guitars sing (depends on Type)
    //   3. 4x oversampled shaper, unity small-signal gain (so DIRT changes
    //      the amount of grit, not the volume). Drive -4..+24 dB.
    //         Edge   : tanh — smooth breakup
    //         Bite   : tanh into a sharper knee (more edge, still smooth)
    //         Crunch : tanh of a tanh (more compressed, fizzier upper grit)
    //      Switching Type crossfades the two shapers over ~25 ms.
    //   4. DC removal (the asymmetric bias creates an envelope-following
    //      offset), then de-emphasis (true inverse of the pre-emphasis)
    //   5. Post-tone tilt — gently darkens as DIRT increases
    //   6. Static loudness compensation (computed from the curve at a typical
    //      DI level -- pedal-style, never breathes), then user OUTPUT trim
    //   7. Dry/wet MIX against a latency-aligned dry signal
    class DirtDSP
    {
    public:
        enum Type { Edge = 0, Bite = 1, Crunch = 2 };

        // Public meters (read by editor on a UI timer)
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> gritAmount  {   0.0f };  // 0..1 harmonic energy

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
            tightenHPF.setCutoffFrequency (40.0f);
            tightenHPF.setResonance (0.5f);

            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false, true);
            oversampler->initProcessing ((size_t) maxBlock);
            latency = (int) std::round (oversampler->getLatencyInSamples());
            dryDelay.prepare (channels, latency + 1);
            work.setSize (channels, maxBlock, false, true, false);

            preEmpCoefs   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0f, 0.7f, 1.0f);
            deEmpCoefs    = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0f, 0.7f, 1.0f);
            postTiltCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0f, 0.7f, 1.0f);
            for (int c = 0; c < (int) preEmp.size(); ++c)
            {
                preEmp[c].reset();
                deEmp[c].reset();
                postTilt[c].reset();
                dc[c].prepare (sr);
                preEmp[c].coefficients   = preEmpCoefs;
                deEmp[c].coefficients    = deEmpCoefs;
                postTilt[c].coefficients = postTiltCoefs;
            }

            targetPreEmp (Edge, smFreq, smGainDb);
            updatePreEmpCoeffs (smFreq, smGainDb);
            updatePostTilt (0.0f);
            // ~30 ms block-rate smoothing on the pre/de-emphasis freq+gain and
            // the DIRT macro so nothing snaps mid-stream.
            smTypeCoef = std::exp (-float (maxBlock) / float (0.030 * sr));
            smDirt     = -1.0f;

            curType  = prevType = Edge;
            xfade    = 1.0f;
            xfadeInc = 1.0f / float (0.025 * sr * 4.0);   // per oversampled sample

            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);

            envIn = envOut = grit = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float dirt, int type, float mix, float outGainDb)
        {
            const int total = buffer.getNumSamples();
            for (int pos = 0; pos < total; pos += maxBlock)
            {
                juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                               pos, juce::jmin (maxBlock, total - pos));
                processChunk (view, dirt, type, mix, outGainDb);
            }
        }

    private:
        struct Shape { float drive, bias, tb, c1, norm, biteCeil; };

        // All three curves pass through 0 with slope 1.
        static inline float shape (Type t, float x, const Shape& s)
        {
            const float u = std::tanh (s.drive * x + s.bias) - s.tb;
            switch (t)
            {
                case Bite:
                {
                    const float v = u * s.norm;                         // unity-slope tanh
                    const float r = std::abs (v) / s.biteCeil;          // sharper knee at biteCeil
                    return v / std::sqrt (std::sqrt (1.0f + r * r * r * r));
                }
                case Crunch:
                    return (std::tanh (1.8f * (u + s.tb)) - s.c1) * s.norm;
                case Edge:
                default:
                    return u * s.norm;
            }
        }

        static Shape makeShape (Type t, float drive, float bias, float dirt)
        {
            Shape s;
            s.drive    = drive;
            s.bias     = bias;
            s.tb       = std::tanh (bias);
            s.c1       = std::tanh (1.8f * s.tb);
            s.biteCeil = juce::jmax (0.35f, 1.0f - dirt * 0.5f) / drive;
            const float sech2b = 1.0f - s.tb * s.tb;
            if (t == Crunch)
                s.norm = 1.0f / (drive * sech2b * 1.8f * (1.0f - s.c1 * s.c1));
            else
                s.norm = 1.0f / (drive * sech2b);
            return s;
        }

        void processChunk (juce::AudioBuffer<float>& buffer,
                           float dirt, int typeIn, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            smDirt = smDirt < 0.0f ? dirt : smTypeCoef * smDirt + (1.0f - smTypeCoef) * dirt;

            // --- Macro mapping -------------------------------------------
            const float drive = juce::Decibels::decibelsToGain (-4.0f + smDirt * 28.0f);
            const float bias  = smDirt * 0.08f;   // 2nd-harmonic asymmetry

            const Type type = (Type) juce::jlimit (0, 2, typeIn);
            if (type != curType)
            {
                prevType = curType;
                curType  = type;
                xfade    = 0.0f;
            }
            const Shape sCur  = makeShape (curType,  drive, bias, smDirt);
            const Shape sPrev = makeShape (prevType, drive, bias, smDirt);

            float targFreq, targGainDb;
            targetPreEmp (type, targFreq, targGainDb);
            smFreq   = smTypeCoef * smFreq   + (1.0f - smTypeCoef) * targFreq;
            smGainDb = smTypeCoef * smGainDb + (1.0f - smTypeCoef) * targGainDb;
            updatePreEmpCoeffs (smFreq, smGainDb);
            updatePostTilt (smDirt);

            // Static loudness compensation from the selected curve at a
            // typical DI level: a pedal-style level match that never breathes.
            const float comp = ok::referenceCompensation ([&] (float x) { return shape (curType, x, sCur); });
            outGainSm.setTargetValue (comp * juce::Decibels::decibelsToGain (outGainDb));
            mixSm.setTargetValue (mix);

            // 1-2) tighten + pre-emphasis (keep the raw input for dry/ref)
            for (int c = 0; c < numCh; ++c)
            {
                work.copyFrom (c, 0, buffer, c, 0, n);
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = preEmp[c].processSample (tightenHPF.processSample (c, d[i]));
            }

            // 3) oversampled shaper (with Type crossfade)
            float gritNum = 0.0f, gritDen = 1.0e-9f;
            {
                juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
                auto up = oversampler->processSamplesUp (block);
                const float xf0 = xfade;
                float xfEnd = xf0;
                for (size_t c = 0; c < up.getNumChannels(); ++c)
                {
                    auto* d = up.getChannelPointer (c);
                    float xf = xf0;
                    for (size_t i = 0; i < up.getNumSamples(); ++i)
                    {
                        const float x = d[i];
                        float y = shape (curType, x, sCur);
                        if (xf < 1.0f)
                        {
                            y = xf * y + (1.0f - xf) * shape (prevType, x, sPrev);
                            xf = juce::jmin (1.0f, xf + xfadeInc);
                        }
                        if (c == 0)
                        {
                            gritNum += std::abs (y - x);
                            gritDen += std::abs (x);
                        }
                        d[i] = y;
                    }
                    xfEnd = xf;
                }
                xfade = xfEnd;
                oversampler->processSamplesDown (block);
            }

            // 4..7) DC, de-emphasis, tilt, loudness, trim, mix
            float pkIn = 0.0f, pkOut = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float outGainNow = outGainSm.getNextValue();
                const float mixNow     = mixSm.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    float* d = buffer.getWritePointer (c);
                    const float toned = postTilt[c].processSample (deEmp[c].processSample (dc[c].process (d[i])));
                    const float xIn = work.getSample (c, i);
                    const float dry = dryDelay.process (c, xIn, latency);
                    const float y   = toned * outGainNow * mixNow + dry * (1.0f - mixNow);
                    d[i] = y;
                    pkIn  = juce::jmax (pkIn,  std::abs (xIn));
                    pkOut = juce::jmax (pkOut, std::abs (y));
                }
                dryDelay.advance();
            }

            const float envA = (float) (1.0 - std::exp (-(double) n / (0.060 * sampleRate)));
            envIn  += (pkIn  - envIn)  * envA;
            envOut += (pkOut - envOut) * envA;
            grit   += (juce::jlimit (0.0f, 1.0f, std::sqrt (gritNum / gritDen) * 2.2f) - grit) * envA;

            inputDb.store    (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store   (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            gritAmount.store (juce::jlimit (0.0f, 1.0f, grit),                 std::memory_order_relaxed);
        }

        // Pre-emphasis target: focuses the grit in the upper-mid region
        // appropriate to the chosen Type.
        static void targetPreEmp (Type t, float& freq, float& gainDb)
        {
            switch (t)
            {
                case Bite:   freq = 2200.0f; gainDb = 5.0f; break;   // bites in upper mids
                case Crunch: freq = 1100.0f; gainDb = 3.0f; break;   // body grit
                case Edge:
                default:     freq = 1500.0f; gainDb = 4.0f; break;
            }
        }

        // Applies the pre-emphasis boost and its genuine inverse (same freq/Q,
        // reciprocal gain) for the de-emphasis stage. ArrayCoefficients keep
        // this allocation-free on the audio thread.
        void updatePreEmpCoeffs (float freq, float gainDb)
        {
            constexpr float q = 0.7f;
            *preEmpCoefs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
                sampleRate, freq, q, juce::Decibels::decibelsToGain (gainDb));
            *deEmpCoefs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
                sampleRate, freq, q, juce::Decibels::decibelsToGain (-gainDb));
        }

        void updatePostTilt (float dirt)
        {
            // Gentle high-shelf cut as dirt rises — keeps the result musical.
            *postTiltCoefs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
                sampleRate, 6500.0f, 0.7f, juce::Decibels::decibelsToGain (-dirt * 2.0f));
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

        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> preEmpCoefs, deEmpCoefs, postTiltCoefs;
        std::array<juce::dsp::IIR::Filter<float>, 8> preEmp;
        std::array<juce::dsp::IIR::Filter<float>, 8> deEmp;
        std::array<juce::dsp::IIR::Filter<float>, 8> postTilt;

        float smFreq     { 1500.0f };
        float smGainDb   {    4.0f };
        float smTypeCoef { 0.0f };
        float smDirt     { -1.0f };

        Type  curType  { Edge }, prevType { Edge };
        float xfade    { 1.0f }, xfadeInc { 0.001f };

        juce::SmoothedValue<float> outGainSm, mixSm;

        float envIn   { 0.0f };
        float envOut  { 0.0f };
        float grit    { 0.0f };
    };
}
