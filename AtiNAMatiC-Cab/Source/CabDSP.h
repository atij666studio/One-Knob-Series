#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace ncb
{
    // Cab — a one-knob speaker cabinet for amp captures that have no cab.
    // Placed AFTER the amp model.
    //
    // A mic'd guitar speaker is a band-limited, lumpy filter. Cab models its
    // main features with seven minimum-phase biquads per channel:
    //
    //   resonant low cut (box/port thump) -> low-mid body -> mid scoop
    //   -> cone-breakup presence peaks (~2.5 kHz, ~4.5 kHz)
    //   -> 4th-order speaker roll-off (two low-pass stages)
    //
    // MIC moves the microphone from the dust cap (bright, forward) to the
    // cone edge (darker, warmer, rounder). CABINET sets the low end:
    // 1x12 open-back (lighter, looser), 2x12, 4x12 (tight, deep).
    //
    // Level is matched from the filter's own response to a pink-ish guitar
    // spectrum, so moving MIC or switching cabinets doesn't jump in volume.
    // Coefficients glide (block-rate) and are allocation-free. Zero latency.
    class CabDSP
    {
    public:
        enum Cabinet { OneByTwelve = 0, TwoByTwelve = 1, FourByTwelve = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> brightAmt {  0.0f };   // 0..1 (cap = 1, edge = 0)

        void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
        {
            sampleRate = sr;
            for (auto& ch : stages) for (auto& f : ch) f.reset();
            micSm = -1.0f; cabCur = -1;
            mixSm.reset (sr, 0.02); mixSm.setCurrentAndTargetValue (1.0f);
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            compSm = 1.0f;
            envIn = envOut = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float mic, int cabIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            mic = juce::jlimit (0.0f, 1.0f, mic);
            const int cab = juce::jlimit (0, 2, cabIn);

            // Glide MIC at block rate; recompute coefficients when it or the
            // cabinet moves.
            const float prevMic = micSm;
            micSm = micSm < 0.0f ? mic : micSm + (mic - micSm) * 0.15f;
            if (cab != cabCur || std::abs (micSm - prevMic) > 1.0e-4f)
            {
                design (micSm, cab);
                cabCur = cab;
            }

            mixSm.setTargetValue (mix);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float pkIn = 0.0f, pkOut = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                compSm += (compTarget - compSm) * 0.002f;
                const float mx = mixSm.getNextValue();
                const float og = outSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    float* d = buffer.getWritePointer (c);
                    const float x = d[i];
                    float y = x;
                    for (auto& f : stages[(size_t) c]) y = f.processSample (y);
                    const float out = (y * compSm * og) * mx + x * (1.0f - mx);
                    d[i] = out;
                    pkIn  = juce::jmax (pkIn,  std::abs (x));
                    pkOut = juce::jmax (pkOut, std::abs (out));
                }
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            inputDb.store   (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store  (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            brightAmt.store (1.0f - micSm, std::memory_order_relaxed);
        }

    private:
        static constexpr int kStages = 7;
        using AC = juce::dsp::IIR::ArrayCoefficients<float>;

        void design (float m, int cab)
        {
            // Cabinet: low cut / thump / body
            float lowHz = 85.0f, lowQ = 1.3f, bodyHz = 160.0f, bodyDb = 2.0f;
            switch (cab)
            {
                case OneByTwelve:  lowHz = 105.0f; lowQ = 0.9f; bodyHz = 200.0f; bodyDb = 1.0f; break;
                case FourByTwelve: lowHz = 75.0f;  lowQ = 1.6f; bodyHz = 140.0f; bodyDb = 2.5f; break;
                default: break;
            }
            // Mic: cap (m=0) -> edge (m=1)
            const float presDb  = juce::jmap (m, 6.0f, 1.0f);
            const float pres2Db = juce::jmap (m, 4.0f, -2.0f);
            const float lpHz    = juce::jmap (m, 6200.0f, 3800.0f);
            const float scoopDb = juce::jmap (m, -3.5f, -1.5f);
            const float warmDb  = bodyDb + 1.5f * m;
            const float nyq     = (float) sampleRate * 0.45f;

            std::array<std::array<float, 6>, kStages> c {{
                AC::makeHighPass  (sampleRate, lowHz, lowQ),
                AC::makePeakFilter (sampleRate, bodyHz, 1.0f, juce::Decibels::decibelsToGain (warmDb)),
                AC::makePeakFilter (sampleRate, 550.0f, 0.9f, juce::Decibels::decibelsToGain (scoopDb)),
                AC::makePeakFilter (sampleRate, juce::jmin (2500.0f, nyq), 1.2f, juce::Decibels::decibelsToGain (presDb)),
                AC::makePeakFilter (sampleRate, juce::jmin (4500.0f, nyq), 2.0f, juce::Decibels::decibelsToGain (pres2Db)),
                AC::makeLowPass   (sampleRate, juce::jmin (lpHz, nyq), 0.8f),
                AC::makeLowPass   (sampleRate, juce::jmin (lpHz * 1.15f, nyq), 0.6f) }};

            for (auto& ch : stages)
                for (int s = 0; s < kStages; ++s)
                    *ch[(size_t) s].coefficients = c[(size_t) s];

            // Level match: RMS gain over a pink-weighted 80 Hz-8 kHz grid.
            double sum = 0.0, wsum = 0.0;
            for (int k = 0; k < 48; ++k)
            {
                const double f = 80.0 * std::pow (100.0, k / 47.0);
                if (f >= sampleRate * 0.5) break;
                double mag2 = 1.0;
                for (int s = 0; s < kStages; ++s)
                {
                    const double h = stages[0][(size_t) s].coefficients->getMagnitudeForFrequency (f, sampleRate);
                    mag2 *= h * h;
                }
                const double w = 1.0;   // log-spaced grid == pink weighting
                sum += mag2 * w; wsum += w;
            }
            compTarget = (float) juce::jlimit (0.25, 4.0, std::sqrt (wsum / juce::jmax (1.0e-9, sum)) * kTrim);
        }

        static constexpr double kTrim = 1.10;   // calibrated on amp-sim guitar

        double sampleRate { 48000.0 };
        std::array<std::array<juce::dsp::IIR::Filter<float>, kStages>, 2> stages;
        float micSm { -1.0f };
        int   cabCur { -1 };
        float compTarget { 1.0f }, compSm { 1.0f };
        juce::SmoothedValue<float> mixSm, outSm;
        float envIn { 0 }, envOut { 0 };
    };
}
