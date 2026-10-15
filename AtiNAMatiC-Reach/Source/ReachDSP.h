#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>

namespace nre
{
    // Reach — placed AFTER the amp / cab. Makes the guitar project and spread
    // without getting louder, muddier or less mono-safe.
    //
    //   Mid  : presence lift (2.2 kHz, up to +3 dB) and a mud dip (300 Hz,
    //          up to -2 dB) -- the guitar steps forward in a mix.
    //   Side : everything below ~150 Hz is folded to the centre (solid,
    //          mono low end), the existing side above 1 kHz is widened,
    //          and a "spread" side is generated from the mid's upper range
    //          (high-passed at 700 Hz, delayed ~1 ms) so even a mono amp sim
    //          gets width. The generated part goes into L and R with
    //          opposite signs, so L+R cancels it: the mono sum is the
    //          presence-shaped mid, untouched by the widening.
    //
    // Shapes: Focus (mostly projection), Wide (balanced), Huge (most spread).
    // Zero latency.
    class ReachDSP
    {
    public:
        enum Shape { Focus = 0, Wide = 1, Huge = 2 };

        std::atomic<float> leftDb   { -90.0f };
        std::atomic<float> rightDb  { -90.0f };
        std::atomic<float> widthAmt {   0.0f };   // 0..1 side/mid energy

        void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
        {
            sampleRate = sr;
            using AC = juce::dsp::IIR::ArrayCoefficients<float>;
            *sideLowLP.coefficients = AC::makeLowPass  (sr, 150.0f);
            *sideHiHP.coefficients  = AC::makeHighPass (sr, 1000.0f);
            *genHP.coefficients     = AC::makeHighPass (sr, 700.0f);
            *genLP.coefficients     = AC::makeLowPass  (sr, juce::jmin (9000.0f, (float) sr * 0.4f));
            for (auto* f : { &sideLowLP, &sideHiHP, &genHP, &genLP, &pres, &mud }) f->reset();
            *pres.coefficients = AC::makePeakFilter (sr, 2200.0f, 0.9f, 1.0f);
            *mud.coefficients  = AC::makePeakFilter (sr, 300.0f, 0.8f, 1.0f);

            delay.assign ((size_t) juce::nextPowerOfTwo ((int) (sr * 0.004) + 4), 0.0f);
            dMask = (int) delay.size() - 1;
            dPos  = 0;

            amtSm.reset (sr, 0.03);   amtSm.setCurrentAndTargetValue (0.0f);
            mixSm.reset (sr, 0.02);   mixSm.setCurrentAndTargetValue (1.0f);
            outSm.reset (sr, 0.02);   outSm.setCurrentAndTargetValue (1.0f);
            msMid = msSide = 0.0f; envL = envR = 0.0f;
            lastAmt = -1.0f; lastShape = -1;
            delaySm = (float) (sr * 0.001);
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int shapeIn, float mix, float outDb)
        {
            const int n = buffer.getNumSamples();
            if (n <= 0) return;
            const bool stereo = buffer.getNumChannels() >= 2;
            float* L = buffer.getWritePointer (0);
            float* R = stereo ? buffer.getWritePointer (1) : nullptr;

            const Shape s = (Shape) juce::jlimit (0, 2, shapeIn);
            float presMax = 3.0f, widenMax = 0.6f, genMax = 0.45f, delayMs = 1.0f;
            switch (s)
            {
                case Focus: presMax = 3.5f; widenMax = 0.3f;  genMax = 0.22f; delayMs = 0.8f; break;
                case Huge:  presMax = 2.5f; widenMax = 0.9f;  genMax = 0.65f; delayMs = 1.4f; break;
                default: break;
            }

            amtSm.setTargetValue (juce::jlimit (0.0f, 1.0f, amount));
            mixSm.setTargetValue (mix);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            // EQ follows the (block-rate) amount; refresh only when it moves.
            const float aBlock = amtSm.getTargetValue();
            if (std::abs (aBlock - lastAmt) > 1.0e-3f || (int) s != lastShape)
            {
                // move gradually: step at most 0.02 per block
                const float aEq = lastAmt < 0.0f ? aBlock : lastAmt + juce::jlimit (-0.02f, 0.02f, aBlock - lastAmt);
                using AC = juce::dsp::IIR::ArrayCoefficients<float>;
                *pres.coefficients = AC::makePeakFilter (sampleRate, 2200.0f, 0.9f, juce::Decibels::decibelsToGain (presMax * aEq));
                *mud.coefficients  = AC::makePeakFilter (sampleRate, 300.0f, 0.8f, juce::Decibels::decibelsToGain (-2.0f * aEq));
                lastAmt = aEq; lastShape = (int) s;
            }
            const float targetDelay = (float) (sampleRate * 0.001 * delayMs);

            float pkL = 0.0f, pkR = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float a  = amtSm.getNextValue();
                const float mx = mixSm.getNextValue();
                const float og = outSm.getNextValue();
                delaySm += (targetDelay - delaySm) * 0.0005f;

                const float l = L[i], r = stereo ? R[i] : L[i];
                float mid  = 0.5f * (l + r);
                float side = 0.5f * (l - r);

                // centre: projection
                const float midOut = mud.processSample (pres.processSample (mid));

                // side: mono lows, wider highs, generated spread
                const float sideLow = sideLowLP.processSample (side);
                const float sideHi  = sideHiHP.processSample (side);
                float sideOut = side - a * sideLow + a * widenMax * sideHi;

                const float g = genLP.processSample (genHP.processSample (mid));
                delay[(size_t) dPos] = g;
                const float rd = (float) dPos - delaySm;
                const int   i0 = (int) std::floor (rd);
                const float fr = rd - (float) i0;
                const float d0 = delay[(size_t) (i0 & dMask)], d1 = delay[(size_t) ((i0 + 1) & dMask)];
                const float gd = d0 + (d1 - d0) * fr;
                dPos = (dPos + 1) & dMask;
                sideOut += a * genMax * gd;

                // gentle level match for the added presence / spread
                const float comp = 1.0f - 0.03f * a;
                const float yl = (midOut + sideOut) * comp * og;
                const float yr = (midOut - sideOut) * comp * og;

                const float outL = yl * mx + l * (1.0f - mx);
                const float outR = yr * mx + r * (1.0f - mx);
                L[i] = outL;
                if (stereo) R[i] = outR;

                const float mo = 0.5f * (outL + outR), so = 0.5f * (outL - outR);
                msMid  += (mo * mo - msMid)  * 0.0005f;
                msSide += (so * so - msSide) * 0.0005f;
                pkL = juce::jmax (pkL, std::abs (outL));
                pkR = juce::jmax (pkR, std::abs (outR));
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envL += (pkL - envL) * k;
            envR += (pkR - envR) * k;
            leftDb.store  (juce::Decibels::gainToDecibels (envL, -90.0f), std::memory_order_relaxed);
            rightDb.store (juce::Decibels::gainToDecibels (envR, -90.0f), std::memory_order_relaxed);
            const float sm = msMid > 1.0e-10f ? 10.0f * std::log10 ((msSide + 1.0e-12f) / msMid) : -60.0f;
            widthAmt.store (juce::jlimit (0.0f, 1.0f, (sm + 30.0f) / 27.0f), std::memory_order_relaxed);
        }

    private:
        double sampleRate { 48000.0 };
        juce::dsp::IIR::Filter<float> sideLowLP, sideHiHP, genHP, genLP, pres, mud;
        std::vector<float> delay;
        int dMask { 0 }, dPos { 0 };
        float delaySm { 48.0f };
        juce::SmoothedValue<float> amtSm, mixSm, outSm;
        float msMid { 0 }, msSide { 0 }, envL { 0 }, envR { 0 };
        float lastAmt { -1.0f };
        int   lastShape { -1 };
    };
}
