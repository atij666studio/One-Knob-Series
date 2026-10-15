#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace nhu
{
    // Hum — removes mains hum and buzz (single coils, ground loops).
    // Placed BEFORE the amp (an amp sim amplifies hum a lot).
    //
    //   * A cascade of true notches (each ~2.5 Hz wide) at the mains frequency and
    //     its harmonics. Narrow enough that notes pass: the cuts are a few
    //     Hz wide, while the nearest guitar note to 100 / 120 Hz is several
    //     Hz away and has its own harmonics far from the mains series.
    //   * HUM sets both the depth (0 -> -40 dB) and how many harmonics are
    //     treated (2 -> 12): low settings take out the 50/60 Hz "hum", higher
    //     settings also the upper-harmonic "buzz".
    //   * Mains: Auto detects 50 or 60 Hz (Goertzel energy of both harmonic
    //     families, compared over ~1 s with hysteresis); or fix 50 / 60 Hz.
    //
    // Zero latency.
    class HumDSP
    {
    public:
        enum Mains { Auto = 0, Fifty = 1, Sixty = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> humDb    { -120.0f };  // hum-family energy at the input
        std::atomic<float> removedDb{   0.0f };   // reduction of that energy
        std::atomic<int>   mainsHz  {  50 };

        void prepare (double sr, int /*blockSize*/, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            for (auto& ch : notch) for (auto& f : ch) f.reset();
            curHz = 0.0f; curDepth = -1.0f; curCount = -1;
            autoHz = 50.0f; votes = 0;
            for (auto& g : gIn)  g = {};
            for (auto& g : gOut) g = {};
            goertzelN = 0; goertzelLen = (int) (sr * 0.5);
            setupGoertzel();
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            envIn = 0.0f; humEnv = -120.0f; remEnv = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int mainsIn, float /*unused*/, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            amount = juce::jlimit (0.0f, 1.0f, amount);
            const Mains m = (Mains) juce::jlimit (0, 2, mainsIn);
            const float hz = m == Fifty ? 50.0f : m == Sixty ? 60.0f : autoHz;
            const float depth = -40.0f * std::pow (amount, 0.7f);
            // notch blend for that depth: residual at the centre = 1 - blend
            const float blend = 1.0f - juce::Decibels::decibelsToGain (depth);
            const int   count = 2 + (int) std::round (10.0f * amount);
            if (hz != curHz || std::abs (depth - curDepth) > 0.25f || count != curCount)
                design (hz, depth, count);

            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float* p[8] = {};
            for (int c = 0; c < numCh; ++c) p[c] = buffer.getWritePointer (c);
            float pkIn = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float og = outSm.getNextValue();
                float monoIn = 0.0f, monoOut = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    const float x = p[c][i];
                    float y = x;
                    if (amount > 0.0f)
                        for (int k = 0; k < curCount; ++k)
                        {
                            const float nz = notch[(size_t) c][(size_t) k].processSample (y);
                            y += blend * (nz - y);
                        }
                    p[c][i] = y * og;
                    monoIn += x; monoOut += y;
                    pkIn = juce::jmax (pkIn, std::abs (x));
                }
                feedGoertzel (monoIn / (float) numCh, monoOut / (float) numCh);
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn += (pkIn - envIn) * k;
            inputDb.store   (juce::Decibels::gainToDecibels (envIn, -90.0f), std::memory_order_relaxed);
            humDb.store     (humEnv, std::memory_order_relaxed);
            removedDb.store (remEnv, std::memory_order_relaxed);
            mainsHz.store   ((int) std::round (hz), std::memory_order_relaxed);
        }

    private:
        static constexpr int   kMaxNotches = 12;
        static constexpr float kNotchBwHz  = 2.5f;
        static constexpr int kBins = 6;   // per family: 1st..6th harmonic

        void design (float hz, float depthDb, int count)
        {
            const float nyq = (float) sampleRate * 0.45f;
            for (int k = 0; k < kMaxNotches; ++k)
            {
                const float f = hz * (float) (k + 1);
                // True notch (-inf at the centre, ~1-2 Hz wide at -3 dB);
                // its depth is set by blending, see process().
                // Constant bandwidth (not constant Q): every notch is the same
                // ~2.5 Hz wide, so upper harmonics don't take chunks of music.
                const float fc = juce::jmin (f, nyq);
                const auto c = juce::dsp::IIR::ArrayCoefficients<float>::makeNotch (
                    sampleRate, fc, juce::jmax (8.0f, fc / kNotchBwHz));
                for (auto& ch : notch) *ch[(size_t) k].coefficients = c;
            }
            curHz = hz; curDepth = depthDb; curCount = count;
        }

        // --- Goertzel energy at the 50- and 60-Hz harmonic families -------
        struct G { float s1 { 0 }, s2 { 0 }; };
        void setupGoertzel()
        {
            for (int k = 0; k < kBins; ++k)
            {
                coefF[(size_t) k] = 2.0f * (float) std::cos (juce::MathConstants<double>::twoPi * 50.0 * (k + 1) / sampleRate);
                coefS[(size_t) k] = 2.0f * (float) std::cos (juce::MathConstants<double>::twoPi * 60.0 * (k + 1) / sampleRate);
            }
        }
        void feedGoertzel (float in, float out)
        {
            for (int k = 0; k < kBins; ++k)
            {
                step (gIn[(size_t) k],          in,  coefF[(size_t) k]);
                step (gIn[(size_t) (k + kBins)], in,  coefS[(size_t) k]);
                step (gOut[(size_t) k],          out, coefF[(size_t) k]);
                step (gOut[(size_t) (k + kBins)], out, coefS[(size_t) k]);
            }
            if (++goertzelN < goertzelLen) return;

            double eF = 0, eS = 0, oF = 0, oS = 0;
            for (int k = 0; k < kBins; ++k)
            {
                eF += power (gIn[(size_t) k],           coefF[(size_t) k]);
                eS += power (gIn[(size_t) (k + kBins)],  coefS[(size_t) k]);
                oF += power (gOut[(size_t) k],          coefF[(size_t) k]);
                oS += power (gOut[(size_t) (k + kBins)], coefS[(size_t) k]);
            }
            for (auto& g : gIn)  g = {};
            for (auto& g : gOut) g = {};
            const double norm = 2.0 / ((double) goertzelLen * goertzelLen);

            // Auto: three windows in a row agreeing (by 6 dB) switch the mains.
            if (eF > eS * 4.0)      votes = juce::jmin (votes + 1, 3);
            else if (eS > eF * 4.0) votes = juce::jmax (votes - 1, -3);
            if (votes >= 3)  autoHz = 50.0f;
            if (votes <= -3) autoHz = 60.0f;

            const bool fifty = curHz < 55.0f;
            const double e = (fifty ? eF : eS) * norm, o = (fifty ? oF : oS) * norm;
            humEnv = (float) (10.0 * std::log10 (e + 1.0e-14));
            remEnv = (float) juce::jlimit (0.0, 60.0, 10.0 * std::log10 ((e + 1.0e-14) / (o + 1.0e-14)));
            goertzelN = 0;
        }
        static void  step (G& g, float x, float coef) { const float s = x + coef * g.s1 - g.s2; g.s2 = g.s1; g.s1 = s; }
        static double power (const G& g, float coef) { return (double) g.s1 * g.s1 + (double) g.s2 * g.s2 - (double) coef * g.s1 * g.s2; }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        std::array<std::array<juce::dsp::IIR::Filter<float>, kMaxNotches>, 8> notch;
        float curHz { 0 }, curDepth { -1 };
        int   curCount { -1 };
        float autoHz { 50.0f };
        int   votes { 0 };
        std::array<float, kBins> coefF {}, coefS {};
        std::array<G, kBins * 2> gIn {}, gOut {};
        int   goertzelN { 0 }, goertzelLen { 1 };
        juce::SmoothedValue<float> outSm;
        float envIn { 0 }, humEnv { -120.0f }, remEnv { 0 };
    };
}
