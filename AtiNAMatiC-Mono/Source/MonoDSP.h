#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace nmo
{
    // Mono — fold stereo to mono where it should be, safely.
    //
    //   Bass : MONO sets a crossover (40 -> 400 Hz, log) below which the
    //          side signal is removed (LR4 high-pass on the side only): the
    //          low end becomes mono, the centre is untouched.
    //   Full : MONO narrows the whole image, 100 % stereo -> mono.
    //   Safe : like Full, but everything below 120 Hz is always mono and
    //          the loudness is held as the image narrows (removing wide
    //          content would otherwise make it quieter).
    //
    // It also measures L/R correlation and shows "PHASE ISSUE" when the
    // input would partly cancel in mono. Zero latency.
    class MonoDSP
    {
    public:
        enum Mode { Bass = 0, Full = 1, Safe = 2 };

        std::atomic<float> leftDb      { -90.0f };
        std::atomic<float> rightDb     { -90.0f };
        std::atomic<float> widthAmt    {   0.0f };   // output side/mid, 0..1
        std::atomic<float> correlation {   1.0f };   // input L/R, -1..1

        void prepare (double sr, int blockSize, int /*numChannels*/)
        {
            sampleRate = sr;
            const juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, blockSize), 1 };
            sideLP.prepare (spec); sideLP.setType (juce::dsp::LinkwitzRileyFilterType::highpass); sideLP.reset();
            fixedLP.prepare (spec); fixedLP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            fixedLP.setCutoffFrequency (120.0f); fixedLP.reset();
            xoverSm = -1.0f;
            amtSm.reset (sr, 0.03); amtSm.setCurrentAndTargetValue (0.0f);
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            sLL = sRR = sLR = 0.0f; msMid = msSide = 0.0f; msInMid = msInSide = 0.0f;
            envL = envR = 0.0f; comp = 1.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int modeIn, float /*unused*/, float outDb)
        {
            const int n = buffer.getNumSamples();
            if (n <= 0) return;
            if (buffer.getNumChannels() < 2)
            {
                buffer.applyGain (juce::Decibels::decibelsToGain (outDb));
                return;   // already mono
            }
            float* L = buffer.getWritePointer (0);
            float* R = buffer.getWritePointer (1);
            amount = juce::jlimit (0.0f, 1.0f, amount);
            const Mode m = (Mode) juce::jlimit (0, 2, modeIn);

            // Bass-mode crossover glides at block rate.
            const float xTarget = 40.0f * std::pow (10.0f, amount);   // 40 Hz .. 400 Hz
            xoverSm = xoverSm < 0.0f ? xTarget : xoverSm + (xTarget - xoverSm) * 0.2f;
            sideLP.setCutoffFrequency (juce::jmin (xoverSm, (float) sampleRate * 0.45f));

            amtSm.setTargetValue (m == Bass ? (amount > 0.0f ? 1.0f : 0.0f) : amount);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float pkL = 0.0f, pkR = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = R[i];
                sLL += (l * l - sLL) * 0.0005f;
                sRR += (r * r - sRR) * 0.0005f;
                sLR += (l * r - sLR) * 0.0005f;

                const float mid  = 0.5f * (l + r);
                float       side = 0.5f * (l - r);
                msInMid  += (mid * mid - msInMid) * 0.0002f;
                msInSide += (side * side - msInSide) * 0.0002f;

                const float a = amtSm.getNextValue();
                switch (m)
                {
                    case Bass:
                    {
                        const float hiSide = sideLP.processSample (0, side);
                        side += a * (hiSide - side);            // side above the crossover only
                        break;
                    }
                    case Safe:
                    {
                        side = fixedLP.processSample (0, side) * (1.0f - a);   // lows always mono, rest narrowed
                        break;
                    }
                    case Full:
                    default:
                        side *= (1.0f - a);
                        break;
                }

                // Safe holds loudness: as side energy is removed, lift the
                // whole signal by the energy that went missing (slowly).
                float g = outSm.getNextValue();
                if (m == Safe)
                {
                    const float before = msInMid + msInSide;
                    const float after  = msInMid + msInSide * (1.0f - a) * (1.0f - a);
                    const float target = after > 1.0e-9f ? juce::jlimit (1.0f, 2.0f, std::sqrt (before / after)) : 1.0f;
                    comp += (target - comp) * 0.0005f;
                    g *= comp;
                }
                else comp += (1.0f - comp) * 0.0005f;

                const float yl = (mid + side) * g, yr = (mid - side) * g;
                L[i] = yl; R[i] = yr;
                const float mo = 0.5f * (yl + yr), so = 0.5f * (yl - yr);
                msMid  += (mo * mo - msMid)  * 0.0005f;
                msSide += (so * so - msSide) * 0.0005f;
                pkL = juce::jmax (pkL, std::abs (yl));
                pkR = juce::jmax (pkR, std::abs (yr));
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envL += (pkL - envL) * k;
            envR += (pkR - envR) * k;
            leftDb.store  (juce::Decibels::gainToDecibels (envL, -90.0f), std::memory_order_relaxed);
            rightDb.store (juce::Decibels::gainToDecibels (envR, -90.0f), std::memory_order_relaxed);
            const float sm = msMid > 1.0e-10f ? 10.0f * std::log10 ((msSide + 1.0e-12f) / msMid) : -60.0f;
            widthAmt.store (juce::jlimit (0.0f, 1.0f, (sm + 30.0f) / 30.0f), std::memory_order_relaxed);
            const float den = std::sqrt (sLL * sRR);
            correlation.store (den > 1.0e-10f ? juce::jlimit (-1.0f, 1.0f, sLR / den) : 1.0f, std::memory_order_relaxed);
        }

    private:
        double sampleRate { 48000.0 };
        juce::dsp::LinkwitzRileyFilter<float> sideLP, fixedLP;
        float xoverSm { -1.0f };
        juce::SmoothedValue<float> amtSm, outSm;
        float sLL { 0 }, sRR { 0 }, sLR { 0 }, msMid { 0 }, msSide { 0 }, msInMid { 0 }, msInSide { 0 };
        float envL { 0 }, envR { 0 }, comp { 1.0f };
    };
}
