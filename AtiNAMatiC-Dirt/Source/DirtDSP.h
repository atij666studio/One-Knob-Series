#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
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
    // sweeps from clean → grit, but the wave-shaper is normalised so output
    // peak stays roughly unity and loudness barely changes.
    //
    // Signal chain (per sample, per channel):
    //   1. DC blocker / sub tighten (HPF @ 40 Hz)
    //   2. Pre-emphasis tilt — boosts upper-mids INTO the shaper so the grit
    //      lands where guitars sing (depends on Type)
    //   3. Drive gain (0..+22 dB driven by DIRT) feeding shaper
    //   4. Asymmetric soft-clipper with controlled odd/even mix
    //         Edge   : tanh — even, smooth breakup
    //         Bite   : tanh + small hard-clip ceiling (sharper edge)
    //         Crunch : tanh of a tanh (more compressed, fizzier upper grit)
    //   5. De-emphasis (mirror of pre-emphasis)
    //   6. Post-tone tilt — gently darkens as DIRT increases so it doesn't
    //      get harsh (still keeps "alive" upper-mid bite)
    //   7. Auto loudness make-up + user OUTPUT trim
    //   8. Dry/wet MIX
    class DirtDSP
    {
    public:
        enum Type { Edge = 0, Bite = 1, Crunch = 2 };

        // Public meters (read by editor on a UI timer)
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> gritAmount  {   0.0f };  // 0..1 harmonic energy

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            const juce::dsp::ProcessSpec spec {
                sr, (juce::uint32) juce::jmax (blockSize, 32), (juce::uint32) channels
            };

            tightenHPF.reset();
            tightenHPF.prepare (spec);
            tightenHPF.setType (juce::dsp::StateVariableTPTFilterType::highpass);
            tightenHPF.setCutoffFrequency (40.0f);
            tightenHPF.setResonance (0.5f);

            preEmpCoefs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0f, 0.7f, 1.0f);
            postTiltCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0f, 0.7f, 1.0f);
            for (int c = 0; c < (int) preEmp.size(); ++c)
            {
                preEmp[c].reset();
                deEmp[c].reset();
                postTilt[c].reset();
                preEmp[c].coefficients   = preEmpCoefs;
                deEmp[c].coefficients    = preEmpCoefs;     // mirror update keeps them inverse-ish
                postTilt[c].coefficients = postTiltCoefs;
            }

            updatePreEmp (Edge);
            updatePostTilt (0.0f);

            envIn  = 0.0f;
            envOut = 0.0f;
            grit   = 0.0f;
            currentType = -1;
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float dirt, int type, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            // --- Macro mapping -------------------------------------------
            // dirt 0..1 → drive 0..+22 dB into the shaper.
            const float driveDb = dirt * 22.0f;
            const float drive   = juce::Decibels::decibelsToGain (driveDb);
            const float invTanh = 1.0f / std::tanh (juce::jmax (1.0e-3f, drive));

            // 2nd-harmonic asymmetry — keeps it "alive" rather than buzzy.
            const float bias    = dirt * 0.08f;

            // Soft hard-clip ceiling for Bite mode — sharpens the edge a touch.
            // Larger means softer (further away). Scales out as dirt rises.
            const float biteCeil = juce::jmax (0.6f, 1.4f - dirt * 0.8f);

            // Auto make-up: keep loudness ~steady across the sweep.
            const float autoMakeupDb = -driveDb * 0.85f;
            const float outGain = juce::Decibels::decibelsToGain (autoMakeupDb + outGainDb);

            if (type != currentType)
            {
                updatePreEmp ((Type) type);
                currentType = type;
            }
            updatePostTilt (dirt);

            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            const float dryMul = 1.0f - mix;
            const float wetMul = mix;

            const float envA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));

            float pkIn = 0.0f, pkOut = 0.0f, gritAcc = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < chClamp; ++c)
                {
                    const float xRaw = writePtrs[c][i];

                    // 1) tighten
                    const float xTight = tightenHPF.processSample (c, xRaw);

                    // 2) pre-emphasis (boost into shaper)
                    const float pre = preEmp[c].processSample (xTight);

                    // 3) drive
                    const float driven = pre * drive;

                    // 4) shaper per type — all normalised to ~unity peak.
                    float sat = 0.0f;
                    switch ((Type) type)
                    {
                        case Bite:
                        {
                            const float t = (std::tanh (driven + bias) - std::tanh (bias)) * invTanh;
                            // Soft hard-clip ceiling for sharper edge.
                            sat = juce::jlimit (-biteCeil, biteCeil, t) / biteCeil;
                            break;
                        }
                        case Crunch:
                        {
                            const float t1 = std::tanh (driven + bias);
                            const float t2 = std::tanh (t1 * 1.8f);
                            sat = (t2 - std::tanh (std::tanh (bias) * 1.8f)) * invTanh;
                            break;
                        }
                        case Edge:
                        default:
                            sat = (std::tanh (driven + bias) - std::tanh (bias)) * invTanh;
                            break;
                    }

                    // 5) de-emphasis (mirror pre with opposite gain — see updatePreEmp)
                    const float deemp = deEmp[c].processSample (sat);

                    // 6) post-tone tilt (slight darken as dirt rises)
                    const float toned = postTilt[c].processSample (deemp);

                    // 7) output trim + dry/wet mix
                    const float wet = toned * outGain;
                    const float y   = wet * wetMul + xRaw * dryMul;

                    writePtrs[c][i] = y;

                    if (c == 0)
                    {
                        const float aIn  = std::abs (xRaw);
                        const float aOut = std::abs (y);
                        if (aIn  > pkIn)  pkIn  = aIn;
                        if (aOut > pkOut) pkOut = aOut;

                        // "Grit" = how much the shaped sample diverges from
                        // the unity-gain reference (i.e. harmonic content).
                        gritAcc += std::abs (sat - xTight);
                    }
                }
            }

            envIn  += (pkIn  - envIn)  * envA;
            envOut += (pkOut - envOut) * envA;
            grit   += (juce::jmin (1.0f, gritAcc / juce::jmax (1, n) * (1.0f + dirt * 4.0f)) - grit) * envA;

            inputDb.store    (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store   (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            gritAmount.store (juce::jlimit (0.0f, 1.0f, grit),                 std::memory_order_relaxed);
        }

    private:
        // Pre-emphasis: focus the grit in the upper-mid region appropriate
        // to the chosen Type. The de-emphasis filter just mirrors the SAME
        // coefficients (which works as a soft inverse for moderate Q's and
        // is the cheapest way to stay clean tonally).
        void updatePreEmp (Type t)
        {
            float freq = 1500.0f, q = 0.7f, gainDb = 4.0f;
            switch (t)
            {
                case Bite:   freq = 2200.0f; gainDb = 5.0f; break;   // bites in upper mids
                case Crunch: freq = 1100.0f; gainDb = 3.0f; break;   // body grit
                case Edge:
                default:     freq = 1500.0f; gainDb = 4.0f; break;
            }
            *preEmpCoefs = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, freq, q, juce::Decibels::decibelsToGain (gainDb));
        }

        void updatePostTilt (float dirt)
        {
            // Gentle high-shelf cut as dirt rises — keeps the result musical.
            const float cutDb = -dirt * 2.0f;
            *postTiltCoefs = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, 6500.0f, 0.7f,
                juce::Decibels::decibelsToGain (cutDb));
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::dsp::StateVariableTPTFilter<float> tightenHPF;

        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> preEmpCoefs, postTiltCoefs;
        std::array<juce::dsp::IIR::Filter<float>, 8> preEmp;
        std::array<juce::dsp::IIR::Filter<float>, 8> deEmp;
        std::array<juce::dsp::IIR::Filter<float>, 8> postTilt;

        int   currentType { -1 };

        float envIn   { 0.0f };
        float envOut  { 0.0f };
        float grit    { 0.0f };
    };
}
